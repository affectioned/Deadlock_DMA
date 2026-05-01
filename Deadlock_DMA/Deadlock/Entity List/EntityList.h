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
	static uintptr_t GetEntityAddressFromHandle(CHandle Handle);


	static void FullControllerRefresh_lk(DMA_Connection* Conn, Process* Proc);
	static void FullControllerRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickControllerRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullSinnerRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullXpOrbRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickXpOrbRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullPawnRefresh_lk(DMA_Connection* Conn, Process* Proc);
	static void FullPawnRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickPawnRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullTrooperRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickTrooperRefresh(DMA_Connection* Conn, Process* Proc);

	static void FullMonsterCampRefresh(DMA_Connection* Conn, Process* Proc);
	static void QuickMonsterCampRefresh(DMA_Connection* Conn, Process* Proc);

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

	static inline std::mutex m_ClassMapMutex{};
	static inline std::unordered_map<std::string, uintptr_t> m_EntityClassMap{};

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
	static inline std::vector<std::pair<uintptr_t, const char*>> m_TrooperAddresses{};
	static inline std::vector<std::pair<uintptr_t, const char*>> m_MonsterCampAddresses{};
	static inline std::vector<uintptr_t> m_SinnersAddresses{};
	static inline std::vector<uintptr_t> m_XpOrbAddresses{};

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