#include "pch.h"

#include <unordered_set>

#include "EntityList.h"
#include "Deadlock/Deadlock.h"
#include "DMA/Memory/PhaseTimings.h"

void EntityList::InitScatterHandle(DMA_Connection* Conn, Process* Proc)
{
	m_sr = std::make_unique<ScatterRead>(Conn->GetHandle(), Proc->GetPID());
}

void EntityList::FullUpdate(DMA_Connection* Conn, Process* Proc)
{
	// UpdateCrucialInformation is two pointer reads — measurement noise floor,
	// deliberately not scoped. The four scoped children sum to ~FullUpdate.
	// EntityMap and FOW use SCATTER_SCOPE so the dump also carries batch
	// size; ClassMap's scatter is small and dwarfed by the new-class-name
	// filtering CPU work, so plain PHASE_SCOPE is fine there.
	UpdateCrucialInformation(Conn, Proc);
	{ SCATTER_SCOPE("FullUpdate::EntityMap", *m_sr); UpdateEntityMap(Conn, Proc); }
	{ PHASE_SCOPE ("FullUpdate::ClassMap");          UpdateEntityClassMap(Conn, Proc); }
	{ PHASE_SCOPE ("FullUpdate::Sort");               SortEntityList(); }
	{ SCATTER_SCOPE("FullUpdate::Players", *m_sr);   DiscoverPlayersByVTable(Conn, Proc); }
	{ SCATTER_SCOPE("FullUpdate::FOW", *m_sr);       DiscoverFOWTeam(Conn, Proc); }
	LogEntityCountsIfChanged();
}

void EntityList::CachePlayerVTables(uintptr_t pawnVTable, uintptr_t ctrlVTable)
{
	if (pawnVTable && pawnVTable != m_PlayerPawnVTable)
	{
		m_PlayerPawnVTable = pawnVTable;
		Log::Info("[EntityList] Cached pawn vtable: 0x{:X}", m_PlayerPawnVTable);
	}
	if (ctrlVTable && ctrlVTable != m_PlayerControllerVTable)
	{
		m_PlayerControllerVTable = ctrlVTable;
		Log::Info("[EntityList] Cached controller vtable: 0x{:X}", m_PlayerControllerVTable);
	}
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

	// A 0 read is a transient scatter failure (e.g. game deprioritized while
	// alt-tabbed) — keep the last-known-good pointer so visuals survive.
	if (LatestAddr == 0) return;
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
	// Source 2 class-name pointers are static globals in client.dll — once resolved
	// for a given class ptr, the string never moves. Cache resolved ptrs so we only
	// scatter-read genuinely new classes; steady-state FullUpdate skips the read.
	static std::unordered_set<uintptr_t> s_ResolvedClassPtrs;

	std::vector<uintptr_t> UniqueClassNames{};

	for (auto& List : m_CompleteEntityList)
	{
		for (auto& Entry : List)
		{
			if (Entry.pEnt == 0 || Entry.pName == 0) continue;
			if (s_ResolvedClassPtrs.contains(Entry.pName)) continue;

			if (std::find(UniqueClassNames.begin(), UniqueClassNames.end(), Entry.pName) != UniqueClassNames.end())
				continue;

			UniqueClassNames.push_back(Entry.pName);
		}
	}

	if (UniqueClassNames.empty()) return;

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
		m_EntityClassNameByPtr[UniqueClassNames[i]] = Name;
		s_ResolvedClassPtrs.insert(UniqueClassNames[i]);

		// TEMP ClassProbe — remove once real powerup class names are wired in
		// SortEntityList. Filters to breakable/item/pickup/powerup families so
		// the log isn't flooded by every prop_dynamic on the map.
		if (Name.find("breakable") != std::string::npos
			|| Name.find("item_")   != std::string::npos
			|| Name.find("pickup")  != std::string::npos
			|| Name.find("powerup") != std::string::npos
			|| Name.find("punchable") != std::string::npos)
		{
			Log::Info("[ClassProbe] {}", Name);
		}
	}

	DbgLog("Entity Class Map Updated ({} new classes).", UniqueClassNames.size());
}

void EntityList::SortEntityList()
{
	m_TrooperAddresses.clear();
	m_PlayerPawn_Addresses.clear();
	m_PlayerController_Addresses.clear();
	m_MonsterCampAddresses.clear();
	m_SinnersAddresses.clear();
	m_XpOrbAddresses.clear();
	m_PowerupAddresses.clear();
	m_PrimaryWeaponAbilityAddresses.clear();

	auto FindClass = [&](const std::string& name) -> uintptr_t {
		auto it = m_EntityClassMap.find(name);
		return it != m_EntityClassMap.end() ? it->second : 0;
	};

	// Class names for pawns and controllers ARE registered ("player" and
	// "citadel_player_controller"), but the pName pointers live in the game's
	// runtime string pool (heap 0x58...) instead of client.dll .rdata (0x7FFB...).
	// The class-map read still resolves them correctly. DiscoverPlayersByVTable
	// runs after this as a safety net in case the pool moves them again.
	uintptr_t PlayerPawnClassPtr       = FindClass("player");
	uintptr_t PlayerControllerClassPtr = FindClass("citadel_player_controller");
	uintptr_t TrooperClassPtr          = FindClass("npc_trooper");
	uintptr_t TrooperBossClassPtr      = FindClass("npc_trooper_boss");
	uintptr_t TrooperNeutralClassPtr   = FindClass("npc_trooper_neutral");
	uintptr_t BossTier2ClassPtr        = FindClass("npc_boss_tier2");
	uintptr_t BossTier3ClassPtr        = FindClass("npc_boss_tier3");
	uintptr_t SinnerClassPtr             = FindClass("npc_neutral_sinners_sacrifice");
	uintptr_t XpOrbClassPtr              = FindClass("item_xp");
	uintptr_t PrimaryWeaponAbilityClass  = FindClass("citadel_ability_primary_weapon");

	// On-map breakable pickups. Names verified live via ClassProbe on 2026-07-06.
	// If a future build renames any of these, re-enable a ClassProbe pass to find
	// the new strings.
	uintptr_t PunchableGoldClass         = FindClass("citadel_item_punchable_gold");
	uintptr_t PickupIdolClass            = FindClass("citadel_item_pickup_idol");
	// World-space UI panel Valve attaches to every interactable pickup (see
	// sdk client/CInWorldItemPanel.hpp). We render the panel's own position
	// as a proxy for "there's a pickup here"; resolving m_hTrackedEntity to
	// color-code by the underlying item is a later refinement.
	uintptr_t InWorldItemPanelClass      = FindClass("in_world_item_panel");

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
			else if (PunchableGoldClass         && Entry.pName == PunchableGoldClass)       m_PowerupAddresses.emplace_back(Entry.pEnt, "Souls");
			else if (PickupIdolClass            && Entry.pName == PickupIdolClass)          m_PowerupAddresses.emplace_back(Entry.pEnt, "Idol");
			else if (InWorldItemPanelClass      && Entry.pName == InWorldItemPanelClass)    m_PowerupAddresses.emplace_back(Entry.pEnt, "Panel");
			else if (PrimaryWeaponAbilityClass  && Entry.pName == PrimaryWeaponAbilityClass) m_PrimaryWeaponAbilityAddresses.push_back(Entry.pEnt);
		}
	}

}

void EntityList::LogEntityCountsIfChanged()
{
	// Log only when the entity-count fingerprint changes — most cycles are no-ops.
	struct Counts { size_t pawns, ctrls, troopers, bosses, sinners, orbs, powerups; };
	static Counts s_prev{ ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull };
	Counts cur{ m_PlayerPawn_Addresses.size(), m_PlayerController_Addresses.size(),
		m_TrooperAddresses.size(), m_MonsterCampAddresses.size(),
		m_SinnersAddresses.size(), m_XpOrbAddresses.size(), m_PowerupAddresses.size() };
	if (cur.pawns != s_prev.pawns || cur.ctrls != s_prev.ctrls
		|| cur.troopers != s_prev.troopers || cur.bosses != s_prev.bosses
		|| cur.sinners != s_prev.sinners || cur.orbs != s_prev.orbs || cur.powerups != s_prev.powerups)
	{
		Log::Info("[EntityList] {} pawns, {} ctrls, {} troopers, {} bosses, {} sinners, {} xporbs, {} powerups",
			cur.pawns, cur.ctrls, cur.troopers, cur.bosses, cur.sinners, cur.orbs, cur.powerups);
		s_prev = cur;
	}
}

void EntityList::DiscoverPlayersByVTable(DMA_Connection* Conn, Process* Proc)
{
	// Bootstrap: nothing to match until Deadlock::UpdateLocalPlayerAddresses
	// has snapshotted at least the pawn vtable off the local player. On the
	// very first FullUpdate both caches are 0 and we exit — the local pawn
	// resolves right after via the LocalController global, and the next
	// FullUpdate lands here with a populated cache.
	if (m_PlayerPawnVTable == 0 && m_PlayerControllerVTable == 0)
		return;

	std::vector<uintptr_t> candidates;
	for (auto& List : m_CompleteEntityList)
	{
		for (auto& Entry : List)
		{
			if (Entry.pEnt && Entry.pName == 0)
				candidates.push_back(Entry.pEnt);
		}
	}

	if (candidates.empty()) return;

	std::vector<uintptr_t> vtables(candidates.size(), 0);

	m_sr->Clear();
	for (size_t i = 0; i < candidates.size(); i++)
		m_sr->Add(candidates[i], &vtables[i]);
	m_sr->Execute();

	std::scoped_lock Lock(m_PawnMutex, m_ControllerMutex);

	for (size_t i = 0; i < candidates.size(); i++)
	{
		if (vtables[i] == 0) continue;
		if (m_PlayerPawnVTable && vtables[i] == m_PlayerPawnVTable)
			m_PlayerPawn_Addresses.push_back(candidates[i]);
		else if (m_PlayerControllerVTable && vtables[i] == m_PlayerControllerVTable)
			m_PlayerController_Addresses.push_back(candidates[i]);
	}
}

uintptr_t EntityList::GetEntityAddressFromHandle(CHandle Handle)
{
	if (!Handle.IsValid()) return 0;

	auto ListIndex = Handle.GetEntityListIndex();
	auto EntityIndex = Handle.GetEntityEntryIndex();

	return m_CompleteEntityList[ListIndex][EntityIndex].pEnt;
}
