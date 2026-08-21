#include "pch.h"

#include "EntityList.h"

#include <cstring>

// FOW vector wrapper layout (Offsets::C_CitadelTeam::m_vecFOWEntities):
//   +0x00 int32  m_nCount
//   +0x04 pad
//   +0x08 ptr    m_pData (heap, count * sizeof(STeamFOWEntity))
//   +0x10 int32  m_nMaxCount
// Per-entry field offsets live in Offsets::STeamFOWEntity.
namespace
{
	constexpr std::ptrdiff_t kFOWCountOff        = 0x00;
	constexpr std::ptrdiff_t kFOWDataPtrOff      = 0x08;
	constexpr std::ptrdiff_t kFOWMaxOff          = 0x10;
	constexpr std::size_t    kSTeamFOWEntitySize = 0x60;
}

// Find the populated team entity. The server only replicates the local team's
// FOW view to the client, so among the 5 team entities, exactly one has
// non-zero count. Re-scan periodically (cheap, runs from FullUpdate at 1s) so
// team-switch / spectator changes pick up the right team.
void EntityList::DiscoverFOWTeam(DMA_Connection* Conn, Process* Proc)
{
	struct Probe
	{
		uintptr_t addr;
		int32_t   count;
		uint64_t  ptr;
		int32_t   max;
	};

	// The cached team address is stable for the whole match; the 512-slot
	// full scan below was costing ~7-8ms per FullUpdate. Fast path re-probes
	// only the cached address (one Execute, 3 targets). A tripwire runs the
	// full scan every kFullScanInterval calls anyway, so if the game engine
	// ever repurposes the slot with bytes that happen to pass the sanity
	// check, we recover within ~kFullScanInterval seconds.
	static int s_CallsSinceFullScan = 0;
	constexpr int kFullScanInterval = 30;    // ~30s at the 1Hz FullUpdate cadence
	auto ProbePassesSanity = [](int32_t count, uint64_t ptr, int32_t max) {
		return count > 0 && count <= 500 && ptr != 0 && max >= count && max <= 500;
	};

	// Menu ↔ match transitions can leave the previous state's team entity
	// alive long enough to keep passing the fast-path sanity check, so we'd
	// otherwise read FOW from the wrong team for up to kFullScanInterval
	// ticks and every real pawn would fall out of m_FOWVisibleByAddr →
	// IsEntityVisible → false → visuals hidden. A big pawn-count swing
	// (menu ~3 → match 12+) is the reliable transition signal; respawn /
	// disconnect noise moves the count by at most 1-2, so a threshold of
	// >2 catches transitions without over-firing.
	static size_t s_LastPawnCount = 0;
	const size_t curPawnCount = m_PlayerPawn_Addresses.size();
	const bool bigPawnShift =
		curPawnCount > s_LastPawnCount + 2 || s_LastPawnCount > curPawnCount + 2;
	s_LastPawnCount = curPawnCount;

	if (!bigPawnShift && m_FOWTeamAddress != 0 && ++s_CallsSinceFullScan < kFullScanInterval)
	{
		int32_t  count = 0;
		uint64_t ptr   = 0;
		int32_t  max   = 0;

		m_sr->Clear();
		m_sr->Add(m_FOWTeamAddress + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWCountOff,   &count);
		m_sr->Add(m_FOWTeamAddress + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWDataPtrOff, &ptr);
		m_sr->Add(m_FOWTeamAddress + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWMaxOff,     &max);
		m_sr->Execute();

		if (ProbePassesSanity(count, ptr, max)) return;
		// Cached address no longer looks like a team — fall through to a
		// full scan and reset the tripwire.
	}
	s_CallsSinceFullScan = 0;

	// Scan the entire list 0. Teams aren't guaranteed to live at low indices —
	// in busy matches the early slots fill with troopers/pawns/etc. and team
	// entities can land anywhere in the list.
	std::vector<Probe> probes;
	probes.reserve(MAX_ENTITIES);
	for (size_t i = 0; i < MAX_ENTITIES; i++)
	{
		auto& id = m_CompleteEntityList[0][i];
		if (!id.pEnt) continue;
		probes.push_back(Probe{ id.pEnt, 0, 0, 0 });
	}
	if (probes.empty()) return;

	m_sr->Clear();
	for (auto& p : probes)
	{
		m_sr->Add(p.addr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWCountOff,   &p.count);
		m_sr->Add(p.addr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWDataPtrOff, &p.ptr);
		m_sr->Add(p.addr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWMaxOff,     &p.max);
	}
	m_sr->Execute();

	uintptr_t bestAddr = 0;
	int32_t   bestCount = 0;
	for (auto& p : probes)
	{
		if (!ProbePassesSanity(p.count, p.ptr, p.max)) continue;
		if (p.count > bestCount)
		{
			bestCount = p.count;
			bestAddr = p.addr;
		}
	}

	std::scoped_lock lk(m_FOWMutex);
	if (bestAddr != m_FOWTeamAddress)
	{
		if (bestAddr)
			Log::Info("[FOW] team=0x{:X} n={}", bestAddr, bestCount);
		else
			Log::Info("[FOW] team lost (was 0x{:X})", m_FOWTeamAddress);
		m_FOWTeamAddress = bestAddr;
		m_FOWVisibleByAddr.clear();
		// Data from the old team no longer applies; force aim/strict-hide
		// to fail closed until FullFOWRefresh repopulates for the new team.
		m_bFOWReady.store(false, std::memory_order_release);
	}
}

void EntityList::FullFOWRefresh(DMA_Connection* Conn, Process* Proc)
{
	uintptr_t teamAddr;
	{
		std::scoped_lock lk(m_FOWMutex);
		teamAddr = m_FOWTeamAddress;
	}
	if (!teamAddr) return;

	// `m_pData` and `m_nMaxCount` only change when the FOW vector reallocates
	// — rare. Cache them and read the entry buffer from the cached pointer in
	// the SAME scatter that fetches the new count/ptr/max. Steady state is one
	// Execute() instead of two; on reallocation we fall back to a second pass.
	// Cache is per-team-address so a team-switch invalidates it cleanly.
	static uintptr_t s_CachedTeamAddr = 0;
	static uintptr_t s_CachedDataPtr  = 0;
	static int32_t   s_CachedMaxCount = 0;

	if (s_CachedTeamAddr != teamAddr)
	{
		s_CachedTeamAddr = teamAddr;
		s_CachedDataPtr  = 0;
		s_CachedMaxCount = 0;
	}

	int32_t  count = 0;
	uint64_t ptr   = 0;
	int32_t  max   = 0;

	const size_t cachedBytes = static_cast<size_t>(s_CachedMaxCount) * kSTeamFOWEntitySize;
	std::vector<uint8_t> raw(cachedBytes);

	m_sr->Clear();
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWCountOff,   &count);
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWDataPtrOff, &ptr);
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWMaxOff,     &max);
	if (s_CachedDataPtr && cachedBytes > 0)
		m_sr->AddRaw(s_CachedDataPtr, static_cast<DWORD>(cachedBytes), raw.data());
	m_sr->Execute();

	if (count <= 0 || count > 500 || !ptr || max < count)
	{
		std::scoped_lock lk(m_FOWMutex);
		m_FOWVisibleByAddr.clear();
		m_bFOWReady.store(false, std::memory_order_release);
		return;
	}

	// Reallocation (or first run): the speculative read above used a stale ptr
	// or undersized buffer — re-fetch from the new ptr in a second scatter and
	// update the cache for next tick.
	if (ptr != s_CachedDataPtr || max != s_CachedMaxCount)
	{
		s_CachedDataPtr  = static_cast<uintptr_t>(ptr);
		s_CachedMaxCount = max;
		raw.assign(static_cast<size_t>(max) * kSTeamFOWEntitySize, 0);
		m_sr->Clear();
		m_sr->AddRaw(static_cast<uintptr_t>(ptr), static_cast<DWORD>(raw.size()), raw.data());
		m_sr->Execute();
	}

	// Resolve entIndex -> entity address. Entity indices in Source 2 are global
	// (same scheme as CHandle): list = idx / MAX_ENTITIES, entry = idx % MAX_ENTITIES.
	// Stash keyed by address so aimbot / ESP can do an O(1) lookup with the pawn
	// pointer they already hold.
	std::scoped_lock lk(m_FOWMutex);
	m_FOWVisibleByAddr.clear();
	m_FOWVisibleByAddr.reserve(count);
	for (int i = 0; i < count; i++)
	{
		const uint8_t* entry = raw.data() + static_cast<size_t>(i) * kSTeamFOWEntitySize;
		int32_t entIndex = 0;
		std::memcpy(&entIndex, entry + Offsets::STeamFOWEntity::m_nEntIndex, 4);
		bool visible = entry[Offsets::STeamFOWEntity::m_bVisibleOnMap] != 0;
		// entIndex 0 is world / CGameRules — never a legitimate FOW entry. If
		// the entry buffer scatter partially fails (game deprioritized on a
		// second monitor), every entry decodes as 0 and would otherwise map
		// slot 0's address to visible=false, leaving m_FOWVisibleByAddr
		// non-empty but missing every real pawn — IsEntityVisible then
		// returns false for every enemy and bVisibleOnly hides them all.
		if (entIndex <= 0) continue;
		size_t listIdx  = static_cast<size_t>(entIndex) / MAX_ENTITIES;
		size_t entryIdx = static_cast<size_t>(entIndex) % MAX_ENTITIES;
		if (listIdx >= MAX_ENTITY_LISTS) continue;
		uintptr_t addr = m_CompleteEntityList[listIdx][entryIdx].pEnt;
		if (!addr) continue;
		m_FOWVisibleByAddr[addr] = visible;
	}
	m_bFOWReady.store(true, std::memory_order_release);
}

bool EntityList::IsEntityVisible(uintptr_t entityAddress)
{
	std::scoped_lock lk(m_FOWMutex);
	if (m_FOWVisibleByAddr.empty()) return true; // no FOW data — fall open
	auto it = m_FOWVisibleByAddr.find(entityAddress);
	if (it == m_FOWVisibleByAddr.end()) return false; // not tracked = not visible
	return it->second;
}

bool EntityList::IsEntityConfirmedVisible(uintptr_t entityAddress)
{
	// Aim-assist / strict-hide path: unknown must never mean "aim at it."
	// Skip the mutex entirely when FOW hasn't been populated for this team
	// yet — the map lookup would answer "not tracked" anyway.
	if (!m_bFOWReady.load(std::memory_order_acquire)) return false;
	std::scoped_lock lk(m_FOWMutex);
	auto it = m_FOWVisibleByAddr.find(entityAddress);
	if (it == m_FOWVisibleByAddr.end()) return false;
	return it->second;
}
