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
		if (p.count <= 0 || p.count > 500) continue;
		if (p.max < p.count || p.max > 500) continue;
		if (p.ptr == 0) continue;
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
			Log::Info("[FOW] Team entity: 0x{:X} (count={})", bestAddr, bestCount);
		else
			Log::Info("[FOW] Team entity lost (was 0x{:X})", m_FOWTeamAddress);
		m_FOWTeamAddress = bestAddr;
		m_FOWVisibleByAddr.clear();
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

	int32_t  count = 0;
	uint64_t ptr = 0;
	int32_t  max = 0;
	m_sr->Clear();
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWCountOff,   &count);
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWDataPtrOff, &ptr);
	m_sr->Add(teamAddr + Offsets::C_CitadelTeam::m_vecFOWEntities + kFOWMaxOff,     &max);
	m_sr->Execute();

	if (count <= 0 || count > 500 || !ptr || max < count)
	{
		std::scoped_lock lk(m_FOWMutex);
		m_FOWVisibleByAddr.clear();
		return;
	}

	std::vector<uint8_t> raw(static_cast<size_t>(count) * kSTeamFOWEntitySize);
	m_sr->Clear();
	m_sr->AddRaw(static_cast<uintptr_t>(ptr), static_cast<DWORD>(raw.size()), raw.data());
	m_sr->Execute();

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
		if (entIndex < 0) continue;
		size_t listIdx  = static_cast<size_t>(entIndex) / MAX_ENTITIES;
		size_t entryIdx = static_cast<size_t>(entIndex) % MAX_ENTITIES;
		if (listIdx >= MAX_ENTITY_LISTS) continue;
		uintptr_t addr = m_CompleteEntityList[listIdx][entryIdx].pEnt;
		if (!addr) continue;
		m_FOWVisibleByAddr[addr] = visible;
	}
}

bool EntityList::IsEntityVisible(uintptr_t entityAddress)
{
	std::scoped_lock lk(m_FOWMutex);
	if (m_FOWVisibleByAddr.empty()) return true; // no FOW data — fall open
	auto it = m_FOWVisibleByAddr.find(entityAddress);
	if (it == m_FOWVisibleByAddr.end()) return false; // not tracked = not visible
	return it->second;
}
