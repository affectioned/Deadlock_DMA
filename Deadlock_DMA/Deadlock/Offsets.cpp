#include "pch.h"

#include "Offsets.h"
#include "Deadlock.h"
#include "GameModules.h"

// Resolve a RIP-relative MOV/LEA reference and return the module-relative offset.
// sigHit   – absolute address of the first byte of the matched instruction
// dispOff  – byte offset within the instruction where the 32-bit displacement lives
// instrSz  – total size of the instruction (RIP advances past it before applying disp)
// Returns 0 if the resolved address is not readable (false-positive match).
static ptrdiff_t ResolveRIP(DMA_Connection* Conn, DWORD pid,
                             uintptr_t clientBase, uint64_t sigHit,
                             int dispOff, int instrSz)
{
	int32_t disp = ReadFromPID<int32_t>(Conn, sigHit + dispOff, pid);
	uintptr_t absAddr = static_cast<uintptr_t>(sigHit) + instrSz + disp;
	if (!IsAddressReadable(Conn, absAddr, pid))
		return 0;
	return static_cast<ptrdiff_t>(absAddr - clientBase);
}

static void ResolveOffset(DMA_Connection* Conn, DWORD pid, uintptr_t clientBase, uintptr_t clientEnd,
                           const char* name, ptrdiff_t& target, ptrdiff_t fallback,
                           const char* sig, int dispOff, int instrSz)
{
	uint64_t hit = FindSignature(Conn, sig, clientBase, clientEnd, pid);
	ptrdiff_t offset = hit ? ResolveRIP(Conn, pid, clientBase, hit, dispOff, instrSz) : 0;

	if (offset)
	{
		target = offset;
		Log::Info("[Off] {}=0x{:X}", name, target);
	}
	else
	{
		target = fallback;
		Log::Warn("[Off] {}=0x{:X} (fallback, sig failed)", name, target);
	}
}

bool Offsets::ResolveOffsets(DMA_Connection* Conn)
{
	DWORD pid = Deadlock::Proc().GetPID();
	uintptr_t clientBase = Deadlock::Proc().GetModuleBase(GameModules::ClientDll);
	uintptr_t clientEnd  = clientBase + Deadlock::Proc().GetModuleSize(GameModules::ClientDll);

	// Patterns and fallback RVAs below track the dezlock-dump schema dump
	// (sdk/_patterns.hpp + sdk/_globals.hpp) for the current build.
	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"GameEntitySystem", Offsets::GameEntitySystem, 0x3CFC7C0,
		"48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3", 3, 7);

	// No dump pattern for the local-controller global; fallback RVA comes from
	// the dump's .data RTTI scan (_globals.txt: CCitadelPlayerController @
	// client.dll+0x31836E0, pointer).
	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"LocalController", Offsets::LocalController, 0x31836E0,
		"48 3B 35 ? ? ? ? 75 ? 48 C7 05", 3, 7);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"ViewMatrix", Offsets::ViewMatrix, 0x3BC84C0,
		"48 8D 0D ? ? ? ? 48 C1 E0 06", 3, 7);

	// LEA, so this resolves to the CPrediction instance itself — callers use it
	// directly instead of dereferencing a pointer global as they did before.
	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"CPrediction", Offsets::Prediction, 0x3237B80,
		"48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54", 3, 7);

	DbgLog("All offsets resolved.");
	return true;
}
