#pragma once
#include "Deadlock/Const/EntityConfig.h"
#include "Deadlock/Classes/CEntityIdentity.h"
#include "Deadlock/Classes/Classes.h"

class EntityList
{
public: /* Interface methods */
	static void InitScatterHandle(DMA_Connection* Conn, Process* Proc);
	static void FullUpdate(DMA_Connection* Conn, Process* Proc);

	static void GetEntityListAddresses(DMA_Connection* Conn, Process* Proc);
	static void GetEntitySystemAddress(DMA_Connection* Conn, Process* Proc);

	static void UpdateCrucialInformation(DMA_Connection* Conn, Process* Proc);
	static void UpdateEntityMap(DMA_Connection* Conn, Process* Proc);
	static void UpdateEntityClassMap(DMA_Connection* Conn, Process* Proc);
	static void SortEntityList();
	// Discovers pawns and controllers via vtable match against cached
	// m_PlayerPawnVTable / m_PlayerControllerVTable. Called after SortEntityList
	// because both entity classes have null pName in current builds and cannot
	// be found through the class-name map.
	static void DiscoverPlayersByVTable(DMA_Connection* Conn, Process* Proc);
	static uintptr_t GetEntityAddressFromHandle(CHandle Handle);

	// Cache vtable pointers observed on the local player. Called by
	// Deadlock::UpdateLocalPlayerAddresses once local pawn/controller are known.
	// Ignores zero values so a transient scatter miss doesn't poison the cache.
	static void CachePlayerVTables(uintptr_t pawnVTable, uintptr_t ctrlVTable);

	// Fires from FullUpdate after every sort + vtable-discovery pass. Emits a
	// single log line when any bucket size changes — steady state is silent.
	static void LogEntityCountsIfChanged();


	static void FullControllerRefresh_lk(DMA_Connection* Conn, Process* Proc);
	static void FullControllerRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickControllerRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullSinnerRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullXpOrbRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickXpOrbRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullPowerupRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickPowerupRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullPawnRefresh_lk(DMA_Connection* Conn, Process* Proc);
	static void FullPawnRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickPawnRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullTrooperRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickTrooperRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullMonsterCampRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickMonsterCampRefresh(DMA_Connection* Conn, Process* Proc);

	// Reads base bullet speed for primary fire from the local pawn's
	// citadel_ability_primary_weapon -> m_pSubclassVData -> CCitadelWeaponInfo
	// at +0x158 -> m_flBulletSpeed at +0xB4. Latches into g_LocalBulletSpeed.
	static void RefreshPrimaryWeaponBulletSpeed(DMA_Connection* Conn, Process* Proc);

public: /* Interface variables */
	static inline std::mutex m_PawnMutex{};
	static inline int32_t m_LocalPawnIndex = -1;
	static inline std::vector<C_CitadelPlayerPawn> m_PlayerPawns{};

	static inline std::mutex m_ControllerMutex{};
	static inline int32_t m_LocalControllerIndex = -1;
	static inline std::vector<CCitadelPlayerController> m_PlayerControllers{};

	static inline std::mutex m_TrooperMutex{};
	static inline std::vector<C_NPC_Trooper> m_Troopers{};

	static inline std::mutex m_MonsterCampMutex{};
	static inline std::vector<C_BaseEntity> m_MonsterCamps{};

	static inline std::mutex m_SinnerMutex{};
	static inline std::vector<C_BaseEntity> m_Sinners{};

	static inline std::mutex m_XpOrbMutex{};
	static inline std::vector<C_BaseEntity> m_XpOrbs{};

	// Breakable boxes / on-map powerups (souls crates, health pickups, modifier
	// buffs, punchable powerups). All variants of CCitadel_BreakableProp* share
	// this bucket — Draw_Powerups colors by m_Label.
	static inline std::mutex m_PowerupMutex{};
	static inline std::vector<C_BaseEntity> m_Powerups{};

	// Base muzzle speed for the local pawn's primary weapon, in hu/s. Latched
	// from the weapon ability's VData by RefreshPrimaryWeaponBulletSpeed. Atomic
	// so the GUI/Aim Assist thread can read without a mutex. 0 means "not resolved
	// yet" — Aim Assist falls back to its default until the first read lands.
	static inline std::atomic<float> g_LocalBulletSpeed{ 0.0f };

	static inline std::mutex m_ClassMapMutex{};
	static inline std::unordered_map<std::string, uintptr_t> m_EntityClassMap{};
	// Reverse index maintained alongside m_EntityClassMap. Consumers that
	// already hold a class-name pointer (e.g. from a CEntityIdentity or a
	// resolved CHandle) can go straight to the class name without linear-
	// scanning the forward map.
	static inline std::unordered_map<uintptr_t, std::string> m_EntityClassNameByPtr{};

	// FOW (fog-of-war / minimap visibility), CS2-style. Populated from a
	// C_CitadelTeam's m_vecFOWEntities. Address-keyed for cheap pawn lookup.
	static inline std::mutex m_FOWMutex{};
	static inline uintptr_t m_FOWTeamAddress = 0;
	static inline std::unordered_map<uintptr_t, bool> m_FOWVisibleByAddr{};

	static void DiscoverFOWTeam(DMA_Connection* Conn, Process* Proc);
	static void FullFOWRefresh(DMA_Connection* Conn, Process* Proc);
	static bool IsEntityVisible(uintptr_t entityAddress);

private: /* Internal variables */
	static inline uintptr_t m_EntitySystem_Address = 0;
	static inline std::array<uintptr_t, MAX_ENTITY_LISTS> m_EntityList_Addresses{};
	static inline std::array<std::array<CEntityIdentity, MAX_ENTITIES>, MAX_ENTITY_LISTS> m_CompleteEntityList{};

	static inline std::vector<uintptr_t> m_PlayerController_Addresses{};
	static inline std::vector<uintptr_t> m_PlayerPawn_Addresses{};

	// Both CCitadelPlayerController and C_CitadelPlayerPawn have no class-name
	// registration in current builds (CEntityIdentity.pName == 0). Vtable
	// pointers, on the other hand, are stable per-session — we snapshot them
	// off the local player and match every null-pName entity against them.
	static inline uintptr_t m_PlayerPawnVTable = 0;
	static inline uintptr_t m_PlayerControllerVTable = 0;
	static inline std::vector<std::pair<uintptr_t, const char*>> m_TrooperAddresses{};
	static inline std::vector<std::pair<uintptr_t, const char*>> m_MonsterCampAddresses{};
	static inline std::vector<uintptr_t> m_SinnersAddresses{};
	static inline std::vector<uintptr_t> m_XpOrbAddresses{};
	// Stored as (address, label) so Draw_Powerups can color-code by type
	// without re-reading the entity classname every frame.
	static inline std::vector<std::pair<uintptr_t, const char*>> m_PowerupAddresses{};
	static inline std::vector<uintptr_t> m_PrimaryWeaponAbilityAddresses{};

	// Single ScatterRead shared across all DMA operations on the DMA thread.
	// Initialized once via InitScatterHandle(); cleared before each batch.
	static inline std::unique_ptr<ScatterRead> m_sr;

public: /* Debug features */
	static void PrintPlayerControllerAddresses();
	static void PrintPlayerControllers();
	static void PrintPlayerPawns();
	static void PrintClassMap();

public:
	static ETeam GetLocalPlayerTeam();
	static Vector3 GetLocalPawnPosition();
	static 	CCitadelPlayerController* GetAssociatedPC(const C_CitadelPlayerPawn& Pawn);
};