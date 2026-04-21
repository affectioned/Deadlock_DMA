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
	Log::Info("[EntityList] UpdateCrucialInformation...");
	UpdateCrucialInformation(Conn, Proc);
	Log::Info("[EntityList] UpdateEntityMap...");
	UpdateEntityMap(Conn, Proc);
	Log::Info("[EntityList] UpdateEntityClassMap...");
	UpdateEntityClassMap(Conn, Proc);

	if (!m_EntityClassMap.empty())
	{
		Log::Info("[EntityList] Class map ({} entries):", m_EntityClassMap.size());
		for (auto& [name, ptr] : m_EntityClassMap)
			Log::Info("  {}", name);
	}

	Log::Info("[EntityList] SortEntityList...");
	SortEntityList();
	Log::Info("[EntityList] FullUpdate done.");
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

	Log::Info("Entity Map Updated.");
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

	Log::Info("[EntityList] Sort: {} pawns, {} troopers, {} bosses, {} sinners, {} xporbs",
		m_PlayerPawn_Addresses.size(), m_TrooperAddresses.size(),
		m_MonsterCampAddresses.size(), m_SinnersAddresses.size(), m_XpOrbAddresses.size());
}

void EntityList::FullControllerRefresh_lk(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_ControllerMutex);

	FullControllerRefresh(Conn, Proc);
}

void EntityList::FullControllerRefresh(DMA_Connection* Conn, Process* Proc)
{
	m_PlayerControllers.clear();
	m_PlayerController_Addresses.clear();

	// Controllers don't register a class name in the entity system (pName == 0),
	// so derive their addresses from pawn m_hController handles instead.
	{
		std::scoped_lock PawnLock(m_PawnMutex);
		for (auto& Pawn : m_PlayerPawns)
		{
			uintptr_t addr = GetEntityAddressFromHandle(Pawn.m_hController);
			if (addr) m_PlayerController_Addresses.push_back(addr);
		}
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

	Log::Info("Trooper List Refreshed. Count: {}", m_Troopers.size());
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
		m_XpOrbs.emplace_back(C_BaseEntity(addr));

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.PrepareRead_1(*m_sr);
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
		Orb.QuickRead(*m_sr);
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

	Log::Info("Entity Class Map Updated.");
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
