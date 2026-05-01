#include "pch.h"

#include "EntityList.h"
#include "GUI/Fuser/ESP/ESP.h"

#include "GUI/Query.h"

void EntityList::InitScatterHandle(DMA_Connection* Conn, Process* Proc)
{
	m_sr = std::make_unique<ScatterRead>(Conn->GetHandle(), Proc->GetPID());
}

void EntityList::FullUpdate(DMA_Connection* Conn, Process* Proc)
{
	UpdateCrucialInformation(Conn, Proc);
	UpdateEntityMap(Conn, Proc);
	UpdateEntityClassMap(Conn, Proc);
	SortEntityList();
	DiscoverFOWTeam(Conn, Proc);
}

void EntityList::UpdateCrucialInformation(DMA_Connection* Conn, Process* Proc)
{
	GetEntitySystemAddress(Conn, Proc);

	GetEntityListAddresses(Conn, Proc);
}

void EntityList::GetEntitySystemAddress(DMA_Connection* Conn, Process* Proc)
{
	uintptr_t EntitySystemPointer = Proc->GetModuleBase("client.dll") + Offsets::GameEntitySystem;
	uintptr_t LatestAddr = Proc->ReadMem<uintptr_t>(Conn, EntitySystemPointer);

	if (LatestAddr == m_EntitySystem_Address) return;

	m_EntitySystem_Address = LatestAddr;

	Log::Info("Entity System Address: 0x{:X}", m_EntitySystem_Address);
}

void EntityList::GetEntityListAddresses(DMA_Connection* Conn, Process* Proc)
{
	m_EntityList_Addresses.fill({});

	uintptr_t StartEntityListArray = m_EntitySystem_Address + 0x10;

	m_sr->Clear();
	m_sr->AddRaw(StartEntityListArray, MAX_ENTITY_LISTS * sizeof(uintptr_t), m_EntityList_Addresses.data());
	m_sr->Execute();
}

void EntityList::UpdateEntityMap(DMA_Connection* Conn, Process* Proc)
{
	std::scoped_lock Lock(m_PawnMutex, m_ControllerMutex);

	for (auto& Arr : m_CompleteEntityList)
		Arr.fill({});

	size_t EntityListSize = sizeof(CEntityIdentity) * MAX_ENTITIES;

	m_sr->Clear();

	for (int i = 0; i < MAX_ENTITY_LISTS; i++)
	{
		auto& Addr = m_EntityList_Addresses[i];
		auto& WriteAddr = m_CompleteEntityList[i][0];

		if (Addr == 0) continue;

		m_sr->AddRaw(Addr, static_cast<DWORD>(EntityListSize), &WriteAddr);
	}

	m_sr->Execute();

	DbgLog("Entity Map Updated.");
}

void EntityList::SortEntityList()
{
	m_TrooperAddresses.clear();
	m_PlayerPawn_Addresses.clear();
	m_PlayerController_Addresses.clear();
	m_MonsterCampAddresses.clear();
	m_SinnersAddresses.clear();
	m_XpOrbAddresses.clear();

	auto FindClass = [&](const std::string& name) -> uintptr_t {
		auto it = m_EntityClassMap.find(name);
		return it != m_EntityClassMap.end() ? it->second : 0;
	};

	uintptr_t PlayerPawnClassPtr    = FindClass("player");
	uintptr_t TrooperClassPtr       = FindClass("npc_trooper");
	uintptr_t TrooperBossClassPtr   = FindClass("npc_trooper_boss");
	uintptr_t TrooperNeutralClassPtr= FindClass("npc_trooper_neutral");
	uintptr_t BossTier2ClassPtr     = FindClass("npc_boss_tier2");
	uintptr_t BossTier3ClassPtr     = FindClass("npc_boss_tier3");
	uintptr_t SinnerClassPtr        = FindClass("npc_neutral_sinners_sacrifice");
	uintptr_t XpOrbClassPtr         = FindClass("item_xp");

	for (auto& List : m_CompleteEntityList)
	{
		for (auto& Entry : List)
		{
			if (!Entry.pEnt || !Entry.pName) continue;

			if      (TrooperClassPtr        && Entry.pName == TrooperClassPtr)        m_TrooperAddresses.emplace_back(Entry.pEnt, nullptr);
			else if (TrooperBossClassPtr     && Entry.pName == TrooperBossClassPtr)    m_TrooperAddresses.emplace_back(Entry.pEnt, "Walker");
			else if (TrooperNeutralClassPtr  && Entry.pName == TrooperNeutralClassPtr) m_TrooperAddresses.emplace_back(Entry.pEnt, "Neutral");
			else if (PlayerPawnClassPtr      && Entry.pName == PlayerPawnClassPtr)     m_PlayerPawn_Addresses.push_back(Entry.pEnt);
			else if (BossTier2ClassPtr       && Entry.pName == BossTier2ClassPtr)      m_MonsterCampAddresses.emplace_back(Entry.pEnt, "Tier 2");
			else if (BossTier3ClassPtr       && Entry.pName == BossTier3ClassPtr)      m_MonsterCampAddresses.emplace_back(Entry.pEnt, "Tier 3");
			else if (SinnerClassPtr          && Entry.pName == SinnerClassPtr)         m_SinnersAddresses.push_back(Entry.pEnt);
			else if (XpOrbClassPtr           && Entry.pName == XpOrbClassPtr)          m_XpOrbAddresses.push_back(Entry.pEnt);
		}
	}

	// Log only when the entity-count fingerprint changes — most cycles are no-ops.
	struct Counts { size_t pawns, troopers, bosses, sinners, orbs; };
	static Counts s_prev{ ~0ull, ~0ull, ~0ull, ~0ull, ~0ull };
	Counts cur{ m_PlayerPawn_Addresses.size(), m_TrooperAddresses.size(),
		m_MonsterCampAddresses.size(), m_SinnersAddresses.size(), m_XpOrbAddresses.size() };
	if (cur.pawns != s_prev.pawns || cur.troopers != s_prev.troopers || cur.bosses != s_prev.bosses
		|| cur.sinners != s_prev.sinners || cur.orbs != s_prev.orbs)
	{
		Log::Info("[EntityList] {} pawns, {} troopers, {} bosses, {} sinners, {} xporbs",
			cur.pawns, cur.troopers, cur.bosses, cur.sinners, cur.orbs);
		s_prev = cur;
	}
}

void EntityList::FullControllerRefresh_lk(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	// Acquire both atomically (Pawn, Controller) to match every other call site
	// and avoid lock-order inversion against Radar (Pawn-then-Controller).
	std::scoped_lock Lock(m_PawnMutex, m_ControllerMutex);

	FullControllerRefresh(Conn, Proc);
}

void EntityList::FullControllerRefresh(DMA_Connection* Conn, Process* Proc)
{
	m_PlayerControllers.clear();
	m_PlayerController_Addresses.clear();

	// Controllers don't register a class name in the entity system (pName == 0),
	// so derive their addresses from pawn m_hController handles instead.
	// Caller must hold m_PawnMutex + m_ControllerMutex.
	for (auto& Pawn : m_PlayerPawns)
	{
		uintptr_t addr = GetEntityAddressFromHandle(Pawn.m_hController);
		if (addr) m_PlayerController_Addresses.push_back(addr);
	}

	DbgLog("[EntityList] ControllerRefresh: {} controller addresses from pawn handles", m_PlayerController_Addresses.size());

	for (auto& addr : m_PlayerController_Addresses)
		m_PlayerControllers.emplace_back(CCitadelPlayerController(addr));

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.PrepareRead_2(*m_sr);
	m_sr->Execute();

	int validCount = 0, deadCount = 0;
	for (int i = 0; i < (int)m_PlayerControllers.size(); i++)
	{
		auto& PC = m_PlayerControllers[i];
		if (!PC.IsInvalid()) validCount++;
		if (PC.IsDead()) deadCount++;

		if (PC.IsLocalPlayer())
			m_LocalControllerIndex = i;
		else if (i == (int)m_PlayerControllers.size() - 1 && m_LocalControllerIndex == -1)
			m_LocalControllerIndex = -1;
	}
	DbgLog("[EntityList] ControllerRefresh: {}/{} valid, {} dead, local={}",
		validCount, m_PlayerControllers.size(), deadCount, m_LocalControllerIndex);
}

void EntityList::QuickControllerRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_ControllerMutex);

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.QuickRead(*m_sr);
	m_sr->Execute();
}

void EntityList::FullPawnRefresh_lk(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_PawnMutex);

	FullPawnRefresh(Conn, Proc);
}

void EntityList::FullPawnRefresh(DMA_Connection* Conn, Process* Proc)
{
	m_PlayerPawns.clear();

	DbgLog("[EntityList] PawnRefresh: {} pawn addresses", m_PlayerPawn_Addresses.size());

	for (auto& addr : m_PlayerPawn_Addresses)
		m_PlayerPawns.emplace_back(C_CitadelPlayerPawn(addr));

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_2(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_3(*m_sr);
	m_sr->Execute();

	int validPawns = 0;
	for (auto& Pawn : m_PlayerPawns)
	{
		Pawn.ExtractBones();
		Pawn.CacheBoneData();
		if (!Pawn.IsInvalid()) validPawns++;
	}
	DbgLog("[EntityList] PawnRefresh: {}/{} valid after reads", validPawns, m_PlayerPawns.size());

	for (int i = 0; i < (int)m_PlayerPawns.size(); i++)
	{
		if (m_PlayerPawns[i].IsLocalPlayer())
		{
			m_LocalPawnIndex = i;
			break;
		}
		else if (i == m_PlayerPawns.size() - 1)
		{
			m_LocalPawnIndex = -1;
		}
	}
}

void EntityList::QuickPawnRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_PawnMutex);

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.QuickRead(*m_sr);
	m_sr->Execute();

	for (auto& Pawn : m_PlayerPawns)
		Pawn.ExtractBones();
}

void EntityList::FullMonsterCampRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingCamps() == false) return;

	std::scoped_lock Lock(m_MonsterCampMutex);

	m_MonsterCamps.clear();

	for (auto& [addr, label] : m_MonsterCampAddresses)
	{
		auto& c = m_MonsterCamps.emplace_back(C_BaseEntity(addr));
		c.m_Label = label;
	}

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.PrepareRead_1(*m_sr, true, true);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.PrepareRead_2(*m_sr);
	m_sr->Execute();
}

void EntityList::QuickMonsterCampRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingCamps() == false) return;

	std::scoped_lock Lock(m_MonsterCampMutex);

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.QuickRead(*m_sr);
	m_sr->Execute();
}

void EntityList::FullTrooperRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingTroopers() == false) return;

	std::scoped_lock Lock(m_TrooperMutex);

	m_Troopers.clear();

	for (auto& [addr, label] : m_TrooperAddresses)
	{
		auto& t = m_Troopers.emplace_back(C_NPC_Trooper(addr));
		t.m_Label = label;
	}

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.PrepareRead_2(*m_sr);
	m_sr->Execute();

	DbgLog("Trooper List Refreshed. Count: {}", m_Troopers.size());
}

void EntityList::QuickTrooperRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingTroopers() == false) return;

	std::scoped_lock Lock(m_TrooperMutex);

	if (m_Troopers.empty()) return;

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.QuickRead(*m_sr);
	m_sr->Execute();
}

void EntityList::FullSinnerRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingSinners() == false) return;

	std::scoped_lock Lock(m_SinnerMutex);

	m_Sinners.clear();

	for (auto& addr : m_SinnersAddresses)
		m_Sinners.emplace_back(C_BaseEntity(addr));

	m_sr->Clear();
	for (auto& Sinner : m_Sinners)
		Sinner.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Sinner : m_Sinners)
		Sinner.PrepareRead_2(*m_sr);
	m_sr->Execute();
}

void EntityList::FullXpOrbRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingXpOrbs() == false) return;

	std::scoped_lock Lock(m_XpOrbMutex);

	m_XpOrbs.clear();

	for (auto& addr : m_XpOrbAddresses)
		m_XpOrbs.emplace_back(addr);

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.PrepareRead_1(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.PrepareRead_2(*m_sr);
	m_sr->Execute();
}

void EntityList::QuickXpOrbRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingXpOrbs() == false) return;

	std::scoped_lock Lock(m_XpOrbMutex);

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.QuickRead(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();
}

uintptr_t EntityList::GetEntityAddressFromHandle(CHandle Handle)
{
	if (!Handle.IsValid()) return 0;

	auto ListIndex = Handle.GetEntityListIndex();
	auto EntityIndex = Handle.GetEntityEntryIndex();

	return m_CompleteEntityList[ListIndex][EntityIndex].pEnt;
}

void EntityList::UpdateEntityClassMap(DMA_Connection* Conn, Process* Proc)
{
	std::vector<uintptr_t> UniqueClassNames{};

	for (auto& List : m_CompleteEntityList)
	{
		for (auto& Entry : List)
		{
			if (Entry.pEnt == 0 || Entry.pName == 0) continue;

			if (std::find(UniqueClassNames.begin(), UniqueClassNames.end(), Entry.pName) != UniqueClassNames.end())
				continue;

			UniqueClassNames.push_back(Entry.pName);
		}
	}

	struct NameBuffer
	{
		char Name[64]{ 0 };
	};

	auto Buffer = std::make_unique<NameBuffer[]>(UniqueClassNames.size());

	m_sr->Clear();

	for (int i = 0; i < UniqueClassNames.size(); i++)
		m_sr->AddRaw(UniqueClassNames[i], sizeof(NameBuffer), &Buffer.get()[i]);

	m_sr->Execute();

	std::scoped_lock Lock(m_ClassMapMutex);

	for (int i = 0; i < UniqueClassNames.size(); i++)
	{
		std::string Name = Buffer.get()[i].Name;

		if (Name.empty()) continue;

		m_EntityClassMap[Name] = UniqueClassNames[i];
	}

	DbgLog("Entity Class Map Updated.");
}

void EntityList::PrintPlayerControllerAddresses()
{
	for (auto& Addr : m_PlayerController_Addresses)
		Log::Info("PlayerController: 0x{:X}", Addr);
}

void EntityList::PrintPlayerControllers()
{
	if (m_PlayerControllers.empty())
	{
		Log::Info("No PlayerControllers found.");
		return;
	}

	for (auto& PC : m_PlayerControllers)
	{
		if (PC.IsInvalid()) continue;

		auto PawnIt = std::find(m_PlayerPawns.begin(), m_PlayerPawns.end(), EntityList::GetEntityAddressFromHandle(PC.m_hHeroPawn));

		if (PawnIt == std::end(m_PlayerPawns)) continue;

		if (PawnIt->IsInvalid()) continue;

		Log::Info("Player found! hPawn: {0:d}, hController {1:d}  Pawn @ {2:.0f} with {3:d} hp", PC.m_hHeroPawn.GetEntityEntryIndex(), PawnIt->m_hController.GetEntityEntryIndex(), PawnIt->m_BonePositions[0].z, PC.m_CurrentHealth);
	}
}

void EntityList::PrintPlayerPawns()
{
	for (auto& Pawn : m_PlayerPawns)
		Log::Info("PlayerPawn: 0x{0:X} | GameSceneNode: {1:X}", Pawn.m_EntityAddress, Pawn.m_GameSceneNodeAddress);
}

void EntityList::PrintClassMap()
{
	for (auto& [name, addr] : m_EntityClassMap)
		Log::Info("Class: {} | Address: 0x{:X}", name, addr);
}

// FOW vector layout (C_CitadelTeam::m_vecFOWEntities at +0x6C8):
//   +0x00 int32  m_nCount
//   +0x04 pad
//   +0x08 ptr    m_pData (heap, count * sizeof(STeamFOWEntity))
//   +0x10 int32  m_nMaxCount
// STeamFOWEntity layout (0x60 each):
//   +0x30 int32  m_nEntIndex
//   +0x34 int32  m_nTeam
//   +0x41 bool   m_bVisibleOnMap
namespace
{
	constexpr std::ptrdiff_t kFOWVecOffset       = 0x6C8;
	constexpr std::ptrdiff_t kFOWCountOff        = 0x00;
	constexpr std::ptrdiff_t kFOWDataPtrOff      = 0x08;
	constexpr std::ptrdiff_t kFOWMaxOff          = 0x10;
	constexpr std::size_t    kSTeamFOWEntitySize = 0x60;
	constexpr std::ptrdiff_t kFOWEntIndexOff     = 0x30;
	constexpr std::ptrdiff_t kFOWVisibleOff      = 0x41;
}

// Find the populated team entity. The server only replicates the local team's
// FOW view to the client, so among the 5 team entities, exactly one has
// non-zero count. Re-scan periodically (cheap, runs from FullUpdate at 5s) so
// team-switch / spectator changes pick up the right team.
void EntityList::DiscoverFOWTeam(DMA_Connection* Conn, Process* Proc)
{
	constexpr int kScanCount = 128;

	struct Probe
	{
		uintptr_t addr;
		int32_t   count;
		uint64_t  ptr;
		int32_t   max;
	};

	std::vector<Probe> probes;
	probes.reserve(kScanCount);
	for (int i = 0; i < kScanCount; i++)
	{
		auto& id = m_CompleteEntityList[0][i];
		if (!id.pEnt) continue;
		probes.push_back(Probe{ id.pEnt, 0, 0, 0 });
	}
	if (probes.empty()) return;

	m_sr->Clear();
	for (auto& p : probes)
	{
		m_sr->Add(p.addr + kFOWVecOffset + kFOWCountOff,   &p.count);
		m_sr->Add(p.addr + kFOWVecOffset + kFOWDataPtrOff, &p.ptr);
		m_sr->Add(p.addr + kFOWVecOffset + kFOWMaxOff,     &p.max);
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
			Log::Info("[FOW] No populated team entity found");
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
	m_sr->Add(teamAddr + kFOWVecOffset + kFOWCountOff,   &count);
	m_sr->Add(teamAddr + kFOWVecOffset + kFOWDataPtrOff, &ptr);
	m_sr->Add(teamAddr + kFOWVecOffset + kFOWMaxOff,     &max);
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

	// Resolve entIndex -> entity address via m_CompleteEntityList[0] and stash
	// keyed by address so aimbot / ESP can do an O(1) lookup with the
	// pawn pointer they already hold.
	std::scoped_lock lk(m_FOWMutex);
	m_FOWVisibleByAddr.clear();
	m_FOWVisibleByAddr.reserve(count);
	for (int i = 0; i < count; i++)
	{
		const uint8_t* entry = raw.data() + static_cast<size_t>(i) * kSTeamFOWEntitySize;
		int32_t entIndex = 0;
		std::memcpy(&entIndex, entry + kFOWEntIndexOff, 4);
		bool visible = entry[kFOWVisibleOff] != 0;
		if (entIndex < 0 || entIndex >= (int)m_CompleteEntityList[0].size()) continue;
		uintptr_t addr = m_CompleteEntityList[0][entIndex].pEnt;
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

ETeam EntityList::GetLocalPlayerTeam()
{
	auto Return = ETeam::UNKNOWN;

	std::scoped_lock Lock(m_PawnMutex);

	if (m_LocalPawnIndex == -1)
		return Return;

	return m_PlayerPawns[m_LocalPawnIndex].m_TeamNum;
}

Vector3 EntityList::GetLocalPawnPosition()
{
	auto Return = Vector3();

	std::scoped_lock Lock(m_PawnMutex);

	if (m_LocalPawnIndex == -1)
		return Return;

	return m_PlayerPawns[m_LocalPawnIndex].m_Position;
}

/* m_ControllerMutex must be locked! */
CCitadelPlayerController* EntityList::GetAssociatedPC(const C_CitadelPlayerPawn& Pawn)
{
	auto PCAddress = EntityList::GetEntityAddressFromHandle(Pawn.m_hController);

	for (auto& PC : m_PlayerControllers)
	{
		if (PC.m_EntityAddress == PCAddress)
			return &PC;
	}

	return nullptr;
}
