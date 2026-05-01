#include "pch.h"

#include "EntityList.h"

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

	uintptr_t PlayerPawnClassPtr       = FindClass("player");
	uintptr_t PlayerControllerClassPtr = FindClass("citadel_player_controller");
	uintptr_t TrooperClassPtr          = FindClass("npc_trooper");
	uintptr_t TrooperBossClassPtr      = FindClass("npc_trooper_boss");
	uintptr_t TrooperNeutralClassPtr   = FindClass("npc_trooper_neutral");
	uintptr_t BossTier2ClassPtr        = FindClass("npc_boss_tier2");
	uintptr_t BossTier3ClassPtr        = FindClass("npc_boss_tier3");
	uintptr_t SinnerClassPtr           = FindClass("npc_neutral_sinners_sacrifice");
	uintptr_t XpOrbClassPtr            = FindClass("item_xp");

	for (auto& List : m_CompleteEntityList)
	{
		for (auto& Entry : List)
		{
			if (!Entry.pEnt || !Entry.pName) continue;

			if      (TrooperClassPtr           && Entry.pName == TrooperClassPtr)          m_TrooperAddresses.emplace_back(Entry.pEnt, nullptr);
			else if (TrooperBossClassPtr        && Entry.pName == TrooperBossClassPtr)      m_TrooperAddresses.emplace_back(Entry.pEnt, "Walker");
			else if (TrooperNeutralClassPtr     && Entry.pName == TrooperNeutralClassPtr)   m_TrooperAddresses.emplace_back(Entry.pEnt, "Neutral");
			else if (PlayerPawnClassPtr         && Entry.pName == PlayerPawnClassPtr)       m_PlayerPawn_Addresses.push_back(Entry.pEnt);
			else if (PlayerControllerClassPtr   && Entry.pName == PlayerControllerClassPtr) m_PlayerController_Addresses.push_back(Entry.pEnt);
			else if (BossTier2ClassPtr          && Entry.pName == BossTier2ClassPtr)        m_MonsterCampAddresses.emplace_back(Entry.pEnt, "Tier 2");
			else if (BossTier3ClassPtr          && Entry.pName == BossTier3ClassPtr)        m_MonsterCampAddresses.emplace_back(Entry.pEnt, "Tier 3");
			else if (SinnerClassPtr             && Entry.pName == SinnerClassPtr)           m_SinnersAddresses.push_back(Entry.pEnt);
			else if (XpOrbClassPtr              && Entry.pName == XpOrbClassPtr)            m_XpOrbAddresses.push_back(Entry.pEnt);
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

uintptr_t EntityList::GetEntityAddressFromHandle(CHandle Handle)
{
	if (!Handle.IsValid()) return 0;

	auto ListIndex = Handle.GetEntityListIndex();
	auto EntityIndex = Handle.GetEntityEntryIndex();

	return m_CompleteEntityList[ListIndex][EntityIndex].pEnt;
}
