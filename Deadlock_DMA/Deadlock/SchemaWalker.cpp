#include "pch.h"

#include "SchemaWalker.h"

#include <cctype>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace
{
	// ===================================================================
	// Source 2 structure layouts
	// ===================================================================
	// Documented in dezlock-dump/src/schema-manager.cpp. These are engine
	// level: they describe the schema system itself, not the game classes it
	// publishes, and they move on engine refactors rather than game patches.

	// SchemaClassFieldData_t — one entry per declared field.
	constexpr std::ptrdiff_t kFieldName   = 0x00; // const char*
	constexpr std::ptrdiff_t kFieldOffset = 0x10; // int32, offset within the class
	constexpr size_t         kFieldStride = 0x20;

	// SchemaClassInfoData_t. The Sep-2026 Deadlock build inserted a second
	// copy of the class-name pointer at +0x18, shifting everything after it.
	// Both layouts are still in the wild, so detect per class: in the current
	// layout +0x18 dereferences to the class's own name, in the legacy layout
	// it holds m_nSizeOf/m_nFieldSize and never reads as a matching string.
	struct ClassLayout { std::ptrdiff_t FieldCount, Fields; };
	constexpr ClassLayout kLegacyLayout { 0x1C, 0x28 };
	constexpr ClassLayout kCurrentLayout{ 0x24, 0x30 };
	constexpr std::ptrdiff_t kClassName    = 0x08; // const char*
	constexpr std::ptrdiff_t kClassAltName = 0x18; // const char*, current layout only
	constexpr size_t         kClassHeaderSize = 0x60;

	// CUtlTSHash< CSchemaClassBinding* >: a memory pool followed by a fixed
	// 256-bucket array, each bucket heading a linked list of nodes.
	constexpr size_t kBucketCount  = 256;
	constexpr size_t kBucketStride = 0x18;
	// +0x00 is m_AddLock (lock state, not a pointer) and is never read here.
	constexpr std::ptrdiff_t kBucketFirst            = 0x08; // HashFixedData_t*
	constexpr std::ptrdiff_t kBucketFirstUncommitted = 0x10; // HashFixedData_t*
	constexpr std::ptrdiff_t kNodeNext = 0x08; // HashFixedData_t*
	constexpr std::ptrdiff_t kNodeData = 0x10; // CSchemaClassBinding*
	constexpr size_t         kNodeSize = 0x18;

	// The pool also keeps a chain of blocks that are allocated but not linked
	// into any bucket, and real class bindings live there — walking only the
	// buckets found 3018 of the dump's 3880 classes. Note the different node
	// shape: the free chain's next pointer is at +0x00, not +0x08.
	constexpr std::ptrdiff_t kFreeListHeads[] = { 0x18, 0x20, 0x28 };
	constexpr std::ptrdiff_t kFreeNodeNext = 0x00;

	// CSchemaSystemTypeScope, from its constructor in schemasystem.dll:
	//   V_strncpy(this + 8, name, 256)                      -> m_szName
	//   CUtlMemoryPoolBase(this + 1376, 24, 256, ...)       -> binding pool
	//   256-entry bucket array built at this + 1472         -> buckets
	// So the pool is at +0x560 and the buckets 0x60 past it at +0x5C0. The
	// probe further down is kept for the day those move.
	constexpr std::ptrdiff_t kScopeName         = 0x08;
	constexpr std::ptrdiff_t kScopeBindingsHint = 0x560;
	constexpr std::ptrdiff_t kPoolSizeHint      = 0x60;

	// Probe bounds. The scope buffer has to cover a candidate hash plus its
	// whole bucket array (0x1800), hence the headroom.
	constexpr size_t kScopeProbeBytes  = 0x4000;
	constexpr std::ptrdiff_t kScopeProbeFirst = 0x100;
	constexpr std::ptrdiff_t kScopeProbeLast  = 0x4000 - 0x1800 - kBucketStride;
	constexpr std::ptrdiff_t kPoolSizes[] = {
		0x60, 0x80, 0x40, 0x48, 0x50, 0x58, 0x68, 0x70, 0x78, 0x88, 0x90, 0xA0,
	};

	// CSchemaSystem::m_TypeScopes, read straight off the disassembly of
	// FindTypeScopeForModule / FindOrCreateTypeScopeForModule rather than
	// probed for:
	//   +0x190  int32                      m_Size
	//   +0x198  CSchemaSystemTypeScope**   m_pMemory
	//   +0x1A0  int32                      m_nAllocationCount
	//   +0x1A8  CUtlSymbolTable            module name -> index
	constexpr std::ptrdiff_t kSystemScopeCount = 0x190;
	constexpr std::ptrdiff_t kSystemScopeArray = 0x198;
	constexpr size_t         kMaxTypeScopes    = 512;

	// Vtable matches that turn out not to be the singleton. Each costs a few
	// scatter rounds to reject, so stop rather than grind through a bad scan.
	constexpr size_t kMaxInstanceCandidates = 16;

	// Walk guards. client.dll publishes a few thousand classes; anything far
	// past that means we are following garbage.
	// Ranges per VMMDLL_Scatter_ExecuteRead. Past a few thousand the tail of
	// the batch comes back empty with no error, so every batch here is split.
	constexpr size_t kMaxScatterRanges = 512;

	constexpr size_t kMaxBindings    = 200000;
	constexpr size_t kMaxChainRounds = 4096;
	constexpr size_t kMaxFieldCount  = 8192;
	constexpr size_t kNameBytes      = 96;   // longest schema name we care about
	constexpr std::ptrdiff_t kMaxPlausibleFieldOffset = 0x100000;

	bool PlausiblePtr(uint64_t v)
	{
		return v >= 0x10000 && v < 0x00007FFFFFFFFFFFull;
	}

	// ===================================================================
	// Cache
	// ===================================================================

	// s_Classes is written by Resolve on the DMA thread during init and read by
	// Find on that same thread, so it needs no lock. The three reported values
	// below are different: the Offsets tab reads them from the GUI thread while
	// Resolve may still be running, so they are atomic / lock-guarded.
	std::unordered_map<std::string, std::unordered_map<std::string, std::ptrdiff_t>> s_Classes;

	std::atomic<size_t> s_FieldCount{ 0 };
	std::atomic<size_t> s_ClassCount{ 0 };
	std::atomic<bool>   s_Ready{ false };

	std::mutex  s_StatusMutex;
	std::string s_Status = "not run";

	void SetStatus(std::string text)
	{
		std::scoped_lock lock(s_StatusMutex);
		s_Status = std::move(text);
	}

	// ===================================================================
	// Read helpers
	// ===================================================================

	struct Ctx
	{
		DMA_Connection* Conn;
		DWORD           Pid;

		// Bulk read. ZEROPAD so a module image with unmapped pages still
		// yields a usable buffer instead of failing outright.
		bool ReadBlock(uintptr_t Addr, void* Out, size_t Size, DWORD* Got = nullptr) const
		{
			if (!Size) { if (Got) *Got = 0; return true; }
			DWORD read = 0;
			const bool ok = VMMDLL_MemReadEx(Conn->GetHandle(), Pid, Addr,
				static_cast<PBYTE>(Out), static_cast<DWORD>(Size), &read,
				VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL) != 0;
			if (Got) *Got = read;
			return ok;
		}

		// Scatter-read one fixed-size record per address, in batches.
		//
		// Batching is not an optimisation: a single Execute with a few
		// thousand queued ranges silently drops the tail, which showed up as
		// whole classes decoding zero fields while the ones early in
		// iteration order came back fine. Order came from an unordered_set,
		// so which classes survived changed between runs.
		//
		// Clear() runs only between batches. MemProcFS documents the flow as
		// Initialize -> Prepare -> Execute -> Clear(to reuse); clearing a
		// handle that has not executed yet left it returning nothing.
		template<size_t N>
		std::vector<std::array<uint8_t, N>> ReadEach(const std::vector<uintptr_t>& Addrs) const
		{
			std::vector<std::array<uint8_t, N>> Out(Addrs.size());
			if (Addrs.empty()) return Out;

			ScatterRead sr(Conn->GetHandle(), Pid);
			for (size_t base = 0; base < Addrs.size(); base += kMaxScatterRanges)
			{
				const size_t end = std::min(Addrs.size(), base + kMaxScatterRanges);
				if (base) sr.Clear();
				for (size_t i = base; i < end; ++i)
					sr.AddRaw(Addrs[i], static_cast<DWORD>(N), Out[i].data());
				sr.Execute();
			}
			return Out;
		}

		// Same as ReadStrings but one plain MemReadEx per address. Stage 2
		// uses this: it is only a few hundred reads, and keeping the probe
		// that bootstraps everything else off the scatter path means a
		// scatter problem cannot present as "no type scopes found".
		std::vector<std::string> ReadStringsDirect(const std::vector<uintptr_t>& Addrs) const
		{
			std::vector<std::string> Out(Addrs.size());
			for (size_t i = 0; i < Addrs.size(); ++i)
			{
				if (!PlausiblePtr(Addrs[i])) continue;

				std::array<uint8_t, kNameBytes> b{};
				if (!ReadBlock(Addrs[i], b.data(), b.size())) continue;

				size_t n = 0;
				while (n < kNameBytes && b[n] != 0 && b[n] >= 0x20 && b[n] <= 0x7E)
					++n;
				if (n) Out[i].assign(reinterpret_cast<const char*>(b.data()), n);
			}
			return Out;
		}

		// Scatter-read a NUL-terminated string per address. Entries whose
		// address is 0, or whose bytes are not printable ASCII, come back
		// empty.
		std::vector<std::string> ReadStrings(const std::vector<uintptr_t>& Addrs) const
		{
			std::vector<std::array<uint8_t, kNameBytes>> Raw(Addrs.size());
			if (!Addrs.empty())
			{
				ScatterRead sr(Conn->GetHandle(), Pid);
				for (size_t base = 0; base < Addrs.size(); base += kMaxScatterRanges)
				{
					const size_t end = std::min(Addrs.size(), base + kMaxScatterRanges);
					if (base) sr.Clear();
					for (size_t i = base; i < end; ++i)
						if (PlausiblePtr(Addrs[i]))
							sr.AddRaw(Addrs[i], kNameBytes, Raw[i].data());
					sr.Execute();
				}
			}

			std::vector<std::string> Out(Addrs.size());
			for (size_t i = 0; i < Addrs.size(); ++i)
			{
				const auto& b = Raw[i];
				size_t n = 0;
				while (n < kNameBytes && b[n] != 0
				       && b[n] >= 0x20 && b[n] <= 0x7E)
					++n;
				if (n) Out[i].assign(reinterpret_cast<const char*>(b.data()), n);
			}
			return Out;
		}
	};

	uint64_t Qword(const std::vector<uint8_t>& Buf, size_t Off)
	{
		uint64_t v = 0;
		if (Off + 8 <= Buf.size()) std::memcpy(&v, Buf.data() + Off, 8);
		return v;
	}

	template<size_t N>
	uint64_t Qword(const std::array<uint8_t, N>& Buf, size_t Off)
	{
		uint64_t v = 0;
		if (Off + 8 <= N) std::memcpy(&v, Buf.data() + Off, 8);
		return v;
	}

	template<size_t N>
	int16_t Word(const std::array<uint8_t, N>& Buf, size_t Off)
	{
		int16_t v = 0;
		if (Off + 2 <= N) std::memcpy(&v, Buf.data() + Off, 2);
		return v;
	}

	// All offsets in Haystack where Needle occurs.
	std::vector<size_t> FindAll(const std::vector<uint8_t>& Haystack,
		const void* Needle, size_t NeedleSize, size_t Stride)
	{
		std::vector<size_t> Hits;
		if (Haystack.size() < NeedleSize) return Hits;
		const auto* n = static_cast<const uint8_t*>(Needle);
		for (size_t i = 0; i + NeedleSize <= Haystack.size(); i += Stride)
			if (std::memcmp(Haystack.data() + i, n, NeedleSize) == 0)
				Hits.push_back(i);
		return Hits;
	}

	// ===================================================================
	// Stage 1 — locate the CSchemaSystem singleton by RTTI
	// ===================================================================
	//
	// schemasystem.dll keeps the instance in a static, so the chain is:
	//   ".?AVCSchemaSystem@@" (type descriptor name)
	//     -> RTTITypeDescriptor  (name sits at +0x10)
	//     -> RTTICompleteObjectLocator (references the descriptor by RVA)
	//     -> vtable              (the COL pointer sits one slot before it)
	//     -> instance            (any qword in the image equal to the vtable)
	//
	// No signature anywhere in it, so it survives recompiles of the module.

	std::vector<uintptr_t> FindSchemaSystemCandidates(const Ctx& C, uintptr_t Base, size_t Size)
	{
		std::vector<uintptr_t> Out;

		std::vector<uint8_t> Image(Size);
		DWORD Got = 0;
		if (!C.ReadBlock(Base, Image.data(), Size, &Got))
		{
			Log::Warn("[Schema] could not read schemasystem.dll image");
			return Out;
		}
		// ZEROPAD_ON_FAIL means a partly-unreadable image still reports
		// success, and the instance scan is a byte search over exactly these
		// bytes — a short read silently changes which candidates exist.
		if (Got < Size)
			Log::Warn("[Schema] schemasystem.dll image short: {} of {} bytes", Got, Size);

		static constexpr char kTypeName[] = ".?AVCSchemaSystem@@";
		auto NameHits = FindAll(Image, kTypeName, sizeof(kTypeName), 1); // includes the NUL
		if (NameHits.empty())
		{
			Log::Warn("[Schema] CSchemaSystem type descriptor not found");
			return Out;
		}

		// The descriptor starts 0x10 before its name (pVFTable, spare, name[]).
		for (size_t NameOff : NameHits)
		{
			if (NameOff < 0x10) continue;
			const uint32_t DescRva = static_cast<uint32_t>(NameOff - 0x10);

			// RTTICompleteObjectLocator: signature, offset, cdOffset,
			// pTypeDescriptor, pClassDescriptor, pSelf — all RVAs. pSelf
			// pointing back at the locator is what makes this unambiguous.
			for (size_t Col : FindAll(Image, &DescRva, sizeof(DescRva), 4))
			{
				if (Col < 0x0C) continue;
				const size_t ColOff = Col - 0x0C;
				if (ColOff + 0x18 > Image.size()) continue;

				uint32_t Sig = 0, Self = 0;
				std::memcpy(&Sig,  Image.data() + ColOff + 0x00, 4);
				std::memcpy(&Self, Image.data() + ColOff + 0x14, 4);
				if (Sig != 1 || Self != ColOff) continue;

				// The locator pointer is stored at vtable[-1].
				const uint64_t ColAddr = Base + ColOff;
				for (size_t Slot : FindAll(Image, &ColAddr, sizeof(ColAddr), 8))
				{
					const uint64_t VTable = Base + Slot + 8;
					for (size_t Inst : FindAll(Image, &VTable, sizeof(VTable), 8))
						Out.push_back(Base + Inst);
				}
			}
		}

		Log::Info("[Schema] schemasystem.dll 0x{:X}: {} descriptor hit(s), {} instance candidate(s)",
			Base, NameHits.size(), Out.size());
		return Out;
	}

	// ===================================================================
	// Stage 2 — find the type-scope vector inside CSchemaSystem
	// ===================================================================
	//
	// Rather than hardcode m_TypeScopes' offset, probe every pointer in the
	// instance header for one that leads to an array of objects which name
	// themselves "<module>.dll". That test is specific enough that a false
	// positive would have to be another array of type scopes.

	struct ScopeHit { uintptr_t Scope = 0; std::string Name; };

	bool IsModuleName(const std::string& s)
	{
		return s.size() > 4 && s.size() < 64 && s.ends_with(".dll");
	}

	// Reads CSchemaSystem::m_TypeScopes.
	//
	// Offsets come from the schemasystem.dll disassembly rather than from
	// probing, which is what the earlier version did and got wrong:
	// FindTypeScopeForModule (vtable[13]) ends in
	//   return *(void **)(this[51] + 8 * symbol);
	// and FindOrCreateTypeScopeForModule (vtable[12]) grows the same vector,
	// so the layout at +0x190 is a plain CUtlVector< CSchemaSystemTypeScope* >.
	//
	// The important part is that the vector is indexed by CUtlSymbol id, and
	// the grow path leaves skipped slots untouched — null entries are normal.
	// Probing element 0 and giving up, as this used to, finds nothing.
	std::vector<ScopeHit> ReadTypeScopes(const Ctx& C, uintptr_t Instance)
	{
		int32_t  Count = 0;
		uint64_t Array = 0;
		C.ReadBlock(Instance + kSystemScopeCount, &Count, sizeof(Count));
		C.ReadBlock(Instance + kSystemScopeArray, &Array, sizeof(Array));

		if (Count <= 0 || static_cast<size_t>(Count) > kMaxTypeScopes || !PlausiblePtr(Array))
		{
			Log::Warn("[Schema] instance 0x{:X}: scope vector rejected (count={} array=0x{:X})",
				Instance, Count, Array);
			return {};
		}

		std::vector<uint64_t> Slots(static_cast<size_t>(Count));
		if (!C.ReadBlock(static_cast<uintptr_t>(Array), Slots.data(), Slots.size() * sizeof(uint64_t)))
			return {};

		std::vector<uintptr_t> NameAddrs;
		NameAddrs.reserve(Slots.size());
		for (uint64_t s : Slots)
			NameAddrs.push_back(PlausiblePtr(s) ? static_cast<uintptr_t>(s) + kScopeName : 0);

		const auto Names = C.ReadStringsDirect(NameAddrs);

		std::vector<ScopeHit> Hits;
		for (size_t i = 0; i < Slots.size(); ++i)
			if (PlausiblePtr(Slots[i]) && IsModuleName(Names[i]))
				Hits.push_back({ static_cast<uintptr_t>(Slots[i]), Names[i] });

		Log::Info("[Schema] instance 0x{:X}: {} scope slots, {} named modules",
			Instance, Count, Hits.size());
		return Hits;
	}


	// ===================================================================
	// Stage 3 — find the class-binding hash inside a type scope
	// ===================================================================

	struct HashHit { uintptr_t Buckets = 0; };

	// Scores a candidate bucket array on shape alone, using only the already
	// fetched scope buffer: 256 records whose two head slots are each either
	// null or a plausible pointer, with enough non-null heads to be a
	// populated table. m_AddLock at +0x00 is deliberately not checked — it is
	// lock state, not a pointer, and reads as a small integer when contended.
	// A handful of malformed records is tolerated so that one odd value does
	// not reject the real table.
	constexpr int kMaxMalformedBuckets = 8;

	int ScoreBuckets(const std::vector<uint8_t>& Scope, size_t Off)
	{
		if (Off + kBucketCount * kBucketStride > Scope.size()) return -1;

		int Bad = 0, Populated = 0;
		for (size_t i = 0; i < kBucketCount; ++i)
		{
			const size_t b = Off + i * kBucketStride;
			const uint64_t Head  = Qword(Scope, b + kBucketFirst);
			const uint64_t Uncom = Qword(Scope, b + kBucketFirstUncommitted);

			const bool ok = (Head  == 0 || PlausiblePtr(Head))
			             && (Uncom == 0 || PlausiblePtr(Uncom));
			if (!ok) { if (++Bad > kMaxMalformedBuckets) return -1; continue; }

			if (Head || Uncom) ++Populated;
		}

		if (Populated < 16) return -1;
		return Populated;
	}

	// Confirms a candidate by following a few chains and checking the nodes
	// really do point at objects carrying a schema class name.
	bool ValidateBuckets(const Ctx& C, const std::vector<uint8_t>& Scope, size_t Off)
	{
		std::vector<uintptr_t> Nodes;
		for (size_t i = 0; i < kBucketCount && Nodes.size() < 16; ++i)
		{
			const size_t b = Off + i * kBucketStride;
			const uint64_t Head = Qword(Scope, b + kBucketFirstUncommitted);
			const uint64_t Alt  = Qword(Scope, b + kBucketFirst);
			const uint64_t Use  = PlausiblePtr(Head) ? Head : Alt;
			if (PlausiblePtr(Use)) Nodes.push_back(static_cast<uintptr_t>(Use));
		}
		if (Nodes.size() < 3) return false;

		const auto NodeData = C.ReadEach<kNodeSize>(Nodes);

		std::vector<uintptr_t> NamePtrSlots;
		for (const auto& n : NodeData)
		{
			const uint64_t Data = Qword(n, kNodeData);
			if (PlausiblePtr(Data)) NamePtrSlots.push_back(static_cast<uintptr_t>(Data) + kClassName);
		}
		if (NamePtrSlots.size() < 3) return false;

		const auto NamePtrs = C.ReadEach<8>(NamePtrSlots);
		std::vector<uintptr_t> NameAddrs;
		for (const auto& p : NamePtrs) NameAddrs.push_back(static_cast<uintptr_t>(Qword(p, 0)));

		const auto Names = C.ReadStrings(NameAddrs);

		int Good = 0;
		for (const auto& s : Names)
			if (!s.empty() && (std::isupper(static_cast<unsigned char>(s[0])) || s[0] == '_' || s[0] == '?'))
				++Good;

		return Good >= 3;
	}

	// The CUtlMemoryPool header in front of the bucket array. Checking it is
	// what stops the probe latching onto an off-by-8 bucket base: shifted by
	// one slot the array still reads as "mostly pointers" (each bucket's
	// m_pFirst lands on the previous bucket's m_pFirstUncommitted) and would
	// validate, then quietly walk a subset of the chains.
	bool PlausibleHashHeader(const std::vector<uint8_t>& Scope, size_t Hash)
	{
		if (Hash + 0x14 > Scope.size()) return false;

		int32_t allocated = 0, peak = 0;
		std::memcpy(&allocated, Scope.data() + Hash + 0x0C, 4);
		std::memcpy(&peak,      Scope.data() + Hash + 0x10, 4);

		return allocated > 16 && allocated < 200000
		    && peak >= allocated && peak < 400000;
	}

	struct BindingsHash { uintptr_t Pool = 0; uintptr_t Buckets = 0; };

	BindingsHash FindBucketArray(const Ctx& C, uintptr_t Scope)
	{
		std::vector<uint8_t> Buf(kScopeProbeBytes);
		if (!C.ReadBlock(Scope, Buf.data(), Buf.size())) return {};

		// The known-good pair first: one validation round in the common case.
		{
			const size_t Hash = static_cast<size_t>(kScopeBindingsHint);
			const size_t Off  = Hash + static_cast<size_t>(kPoolSizeHint);
			if (PlausibleHashHeader(Buf, Hash) && ScoreBuckets(Buf, Off) > 0
			    && ValidateBuckets(C, Buf, Off))
				return { Scope + Hash, Scope + Off };
		}

		// Otherwise score every (hash offset, pool size) pair whose header and
		// bucket shape both hold up, then confirm the best few against memory.
		struct Cand { size_t Hash; size_t Off; int Score; };
		std::vector<Cand> Cands;
		for (std::ptrdiff_t Hash = kScopeProbeFirst; Hash <= kScopeProbeLast; Hash += 8)
		{
			if (!PlausibleHashHeader(Buf, static_cast<size_t>(Hash))) continue;

			for (std::ptrdiff_t Pool : kPoolSizes)
			{
				const size_t Off = static_cast<size_t>(Hash + Pool);
				const int Score = ScoreBuckets(Buf, Off);
				if (Score > 0) Cands.push_back({ static_cast<size_t>(Hash), Off, Score });
			}
		}

		std::sort(Cands.begin(), Cands.end(),
			[](const Cand& a, const Cand& b) { return a.Score > b.Score; });

		for (size_t i = 0; i < Cands.size() && i < 8; ++i)
			if (ValidateBuckets(C, Buf, Cands[i].Off))
			{
				Log::Info("[Schema] bindings bucket array at scope+0x{:X} (probed)", Cands[i].Off);
				return { Scope + Cands[i].Hash, Scope + Cands[i].Off };
			}

		Log::Warn("[Schema] no bucket array passed validation ({} shape candidates)", Cands.size());
		return {};
	}

	// ===================================================================
	// Stage 4 — walk every binding out of the hash
	// ===================================================================

	std::vector<uintptr_t> CollectBindings(const Ctx& C, const BindingsHash& H)
	{
		const uintptr_t BucketBase = H.Buckets;
		std::vector<uint8_t> Buckets(kBucketCount * kBucketStride);
		if (!C.ReadBlock(BucketBase, Buckets.data(), Buckets.size())) return {};

		std::unordered_set<uintptr_t> SeenNodes;
		std::vector<uintptr_t> Frontier;

		auto Push = [&](uint64_t v)
		{
			if (!PlausiblePtr(v)) return;
			const auto a = static_cast<uintptr_t>(v);
			if (SeenNodes.insert(a).second) Frontier.push_back(a);
		};

		for (size_t i = 0; i < kBucketCount; ++i)
		{
			const size_t b = i * kBucketStride;
			Push(Qword(Buckets, b + kBucketFirstUncommitted));
			Push(Qword(Buckets, b + kBucketFirst));
		}

		// Chains are walked breadth-first so every bucket advances one link
		// per scatter round instead of one round per link.
		std::unordered_set<uintptr_t> Bindings;
		for (size_t Round = 0; Round < kMaxChainRounds && !Frontier.empty(); ++Round)
		{
			if (SeenNodes.size() > kMaxBindings) break;

			const auto Nodes = C.ReadEach<kNodeSize>(Frontier);
			std::vector<uintptr_t> Next;

			for (const auto& n : Nodes)
			{
				const uint64_t Data = Qword(n, kNodeData);
				if (PlausiblePtr(Data)) Bindings.insert(static_cast<uintptr_t>(Data));

				const uint64_t Nxt = Qword(n, kNodeNext);
				if (PlausiblePtr(Nxt))
				{
					const auto a = static_cast<uintptr_t>(Nxt);
					if (SeenNodes.insert(a).second) Next.push_back(a);
				}
			}

			Frontier.swap(Next);
		}

		const size_t FromBuckets = Bindings.size();

		// Phase 2: the pool's free-blocks chain. Entries here are allocated
		// but not linked into a bucket, and a fair number of real bindings
		// live in it. The head offset moved between builds, so try the known
		// ones and keep the first that leads to a plausible node.
		size_t FromFreeList = 0;
		for (std::ptrdiff_t HeadOff : kFreeListHeads)
		{
			uint64_t Head = 0;
			if (!C.ReadBlock(H.Pool + HeadOff, &Head, sizeof(Head))) continue;
			if (!PlausiblePtr(Head)) continue;

			std::array<uint8_t, kNodeSize> Probe{};
			if (!C.ReadBlock(static_cast<uintptr_t>(Head), Probe.data(), Probe.size())) continue;
			if (!PlausiblePtr(Qword(Probe, kNodeData))) continue;

			// Singly linked and walked one node at a time — unlike the bucket
			// chains there is no breadth to batch across.
			uintptr_t Node = static_cast<uintptr_t>(Head);
			std::unordered_set<uintptr_t> SeenFree;
			while (PlausiblePtr(Node) && SeenFree.insert(Node).second
			       && SeenFree.size() + SeenNodes.size() < kMaxBindings)
			{
				std::array<uint8_t, kNodeSize> N{};
				if (!C.ReadBlock(Node, N.data(), N.size())) break;

				const uint64_t Data = Qword(N, kNodeData);
				if (PlausiblePtr(Data) && Bindings.insert(static_cast<uintptr_t>(Data)).second)
					++FromFreeList;

				Node = static_cast<uintptr_t>(Qword(N, kFreeNodeNext));
			}

			Log::Info("[Schema] free list at pool+0x{:X}: {} extra bindings", HeadOff, FromFreeList);
			break;
		}

		Log::Info("[Schema] bindings: {} from buckets, {} from free list", FromBuckets, FromFreeList);
		return { Bindings.begin(), Bindings.end() };
	}

	// ===================================================================
	// Stage 5 — read the field table of the classes we asked for
	// ===================================================================

	size_t ReadClassFields(const Ctx& C, const std::vector<uintptr_t>& Infos,
		const std::vector<std::string>& Names)
	{
		const auto Headers = C.ReadEach<kClassHeaderSize>(Infos);

		// Layout detection needs the +0x18 string, so fetch those first.
		std::vector<uintptr_t> AltAddrs;
		for (const auto& h : Headers) AltAddrs.push_back(static_cast<uintptr_t>(Qword(h, kClassAltName)));
		const auto AltNames = C.ReadStrings(AltAddrs);

		struct Pending { size_t Class; size_t Index; };
		std::vector<uintptr_t> FieldBlocks;
		std::vector<size_t>    FieldCounts;
		std::vector<size_t>    BlockOwner;

		for (size_t i = 0; i < Infos.size(); ++i)
		{
			const bool current = (AltNames[i] == Names[i]);
			const ClassLayout& L = current ? kCurrentLayout : kLegacyLayout;

			const auto Count = static_cast<size_t>(static_cast<uint16_t>(Word(Headers[i], L.FieldCount)));
			const uint64_t Fields = Qword(Headers[i], L.Fields);

			if (!Count || Count > kMaxFieldCount || !PlausiblePtr(Fields))
			{
				// Says which half of the decode went wrong: a bad count or
				// pointer under the "current" layout means the layout really
				// changed; falling into "legacy" means the +0x18 name marker
				// did not read back, which is a different bug.
				Log::Warn("[Schema] {} header rejected: layout={} count={} fields=0x{:X}",
					Names[i], current ? "current" : "legacy", Count, Fields);
				continue;
			}

			FieldBlocks.push_back(static_cast<uintptr_t>(Fields));
			FieldCounts.push_back(Count);
			BlockOwner.push_back(i);
		}

		// One raw read per class for its whole field array, then one scatter
		// round for every field name across every class.
		std::vector<std::vector<uint8_t>> Blocks(FieldBlocks.size());
		{
			ScatterRead sr(C.Conn->GetHandle(), C.Pid);
			for (size_t base = 0; base < FieldBlocks.size(); base += kMaxScatterRanges)
			{
				const size_t end = std::min(FieldBlocks.size(), base + kMaxScatterRanges);
				if (base) sr.Clear();
				for (size_t i = base; i < end; ++i)
				{
					Blocks[i].resize(FieldCounts[i] * kFieldStride);
					sr.AddRaw(FieldBlocks[i], static_cast<DWORD>(Blocks[i].size()), Blocks[i].data());
				}
				sr.Execute();
			}
		}

		std::vector<uintptr_t> NameAddrs;
		std::vector<Pending>   Owners;
		for (size_t i = 0; i < Blocks.size(); ++i)
			for (size_t f = 0; f < FieldCounts[i]; ++f)
			{
				NameAddrs.push_back(static_cast<uintptr_t>(Qword(Blocks[i], f * kFieldStride + kFieldName)));
				Owners.push_back({ BlockOwner[i], i * kMaxFieldCount + f });
			}

		const auto FieldNames = C.ReadStrings(NameAddrs);

		size_t Stored = 0;
		for (size_t n = 0; n < FieldNames.size(); ++n)
		{
			if (FieldNames[n].empty()) continue;

			const size_t Block = Owners[n].Index / kMaxFieldCount;
			const size_t Field = Owners[n].Index % kMaxFieldCount;

			int32_t Off = 0;
			std::memcpy(&Off, Blocks[Block].data() + Field * kFieldStride + kFieldOffset, 4);
			if (Off < 0 || Off > kMaxPlausibleFieldOffset) continue;

			s_Classes[Names[Owners[n].Class]][FieldNames[n]] = Off;
			++Stored;
		}

		return Stored;
	}
}

// =======================================================================

bool SchemaWalker::Resolve(DMA_Connection* Conn, DWORD Pid,
	const std::vector<std::string>& WantedClasses)
{
	s_Classes.clear();
	s_FieldCount.store(0);
	s_ClassCount.store(0);
	s_Ready.store(false);
	SetStatus("failed");

	if (!Conn || !Pid || WantedClasses.empty()) return false;

	const Ctx C{ Conn, Pid };
	const auto Started = std::chrono::steady_clock::now();

	// --- schemasystem.dll ---------------------------------------------
	// Resolved here rather than through GameModules because that list blocks
	// startup until every entry appears; a missing schemasystem.dll should
	// cost us the walk, not the whole session.
	uintptr_t ModBase = 0; size_t ModSize = 0;
	{
		PVMMDLL_MAP_MODULEENTRY e = nullptr;
		char Name[] = "schemasystem.dll";
		if (VMMDLL_Map_GetModuleFromNameU(Conn->GetHandle(), Pid, Name, &e, VMMDLL_MODULE_FLAG_NORMAL))
		{
			ModBase = e->vaBase;
			ModSize = e->cbImageSize;
			VMMDLL_MemFree(e);
		}
	}
	if (!ModBase || !ModSize)
	{
		SetStatus("schemasystem.dll not found");
		Log::Warn("[Schema] schemasystem.dll not found");
		return false;
	}

	// --- CSchemaSystem -> type scopes -> client.dll --------------------
	const auto Candidates = FindSchemaSystemCandidates(C, ModBase, ModSize);
	if (Candidates.empty())
	{
		SetStatus("CSchemaSystem instance not found");
		Log::Warn("[Schema] CSchemaSystem instance not found");
		return false;
	}

	uintptr_t ClientScope = 0;
	const size_t Tried = std::min(Candidates.size(), kMaxInstanceCandidates);
	for (size_t i = 0; i < Tried && !ClientScope; ++i)
	for (size_t i = 0; i < Tried && !ClientScope; ++i)
	{
		const auto Scopes = ReadTypeScopes(C, Candidates[i]);
		for (const auto& s : Scopes)
			if (s.Name == "client.dll") { ClientScope = s.Scope; break; }

		if (ClientScope)
			Log::Info("[Schema] CSchemaSystem=0x{:X} scopes={} client.dll scope=0x{:X}",
				Candidates[i], Scopes.size(), ClientScope);
	}
	if (!ClientScope)
	{
		const auto msg = std::format("client.dll type scope not found ({} of {} vtable matches tried)",
			Tried, Candidates.size());
		SetStatus(msg);
		Log::Warn("[Schema] {}", msg);
		return false;
	}

	// --- class bindings ------------------------------------------------
	const BindingsHash Hash = FindBucketArray(C, ClientScope);
	if (!Hash.Buckets)
	{
		SetStatus("class-binding hash not found");
		Log::Warn("[Schema] class-binding hash not found");
		return false;
	}

	const auto Bindings = CollectBindings(C, Hash);
	if (Bindings.empty())
	{
		SetStatus("no class bindings");
		Log::Warn("[Schema] no class bindings");
		return false;
	}

	// --- match the classes we were asked for ---------------------------
	std::vector<uintptr_t> NamePtrSlots;
	NamePtrSlots.reserve(Bindings.size());
	for (uintptr_t b : Bindings) NamePtrSlots.push_back(b + kClassName);

	const auto NamePtrs = C.ReadEach<8>(NamePtrSlots);
	std::vector<uintptr_t> NameAddrs;
	NameAddrs.reserve(NamePtrs.size());
	for (const auto& p : NamePtrs) NameAddrs.push_back(static_cast<uintptr_t>(Qword(p, 0)));

	const auto Names = C.ReadStrings(NameAddrs);

	const std::unordered_set<std::string> Wanted(WantedClasses.begin(), WantedClasses.end());
	std::vector<uintptr_t>   HitInfos;
	std::vector<std::string> HitNames;
	for (size_t i = 0; i < Bindings.size(); ++i)
		if (Wanted.contains(Names[i]))
		{
			HitInfos.push_back(Bindings[i]);
			HitNames.push_back(Names[i]);
		}

	// "No binding at all" and "binding found but its fields would not decode"
	// are different failures with different fixes, so separate them here
	// rather than reporting both as "produced no fields".
	{
		const std::unordered_set<std::string> Matched(HitNames.begin(), HitNames.end());
		for (const auto& want : WantedClasses)
			if (!Matched.contains(want))
				Log::Warn("[Schema] no binding named {} among {} collected", want, Bindings.size());
	}

	if (HitInfos.empty())
	{
		const auto msg = std::format("{} bindings, none of the {} wanted classes",
			Bindings.size(), WantedClasses.size());
		SetStatus(msg);
		Log::Warn("[Schema] {}", msg);
		return false;
	}

	const size_t Fields = ReadClassFields(C, HitInfos, HitNames);
	s_FieldCount.store(Fields);
	s_ClassCount.store(s_Classes.size());
	s_Ready.store(Fields > 0);

	const auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - Started).count();
	const auto summary = std::format("{}/{} classes, {} fields from {} bindings in {}ms",
		s_Classes.size(), WantedClasses.size(), Fields, Bindings.size(), Ms);
	SetStatus(summary);

	Log::Info("[Schema] {}", summary);

	// Name every class we asked for and did not get: the binding exists but
	// its field array did not read, or the class was renamed by the patch.
	// Without this the only symptom is a run of per-field fallback warnings.
	for (const auto& want : WantedClasses)
		if (!s_Classes.contains(want))
			Log::Warn("[Schema] class {} produced no fields", want);

	return s_Ready.load();
}

std::ptrdiff_t SchemaWalker::Find(std::string_view ClassName, std::string_view FieldName)
{
	auto c = s_Classes.find(std::string(ClassName));
	if (c == s_Classes.end()) return 0;

	auto f = c->second.find(std::string(FieldName));
	return f != c->second.end() ? f->second : 0;
}

bool   SchemaWalker::Ready()      { return s_Ready.load(); }
size_t SchemaWalker::ClassCount() { return s_ClassCount.load(); }
size_t SchemaWalker::FieldCount() { return s_FieldCount.load(); }

std::string SchemaWalker::Status()
{
	std::scoped_lock lock(s_StatusMutex);
	return s_Status;
}
