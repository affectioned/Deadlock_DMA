#include "pch.h"

#include "Offsets.h"
#include "SchemaWalker.h"
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

// Returns true when the signature resolved, false when the baked fallback RVA
// was used instead.
static bool ResolveOffset(DMA_Connection* Conn, DWORD pid, uintptr_t clientBase, uintptr_t clientEnd,
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

	return offset != 0;
}

namespace
{
	// Every field Offsets.h exposes that client.dll publishes through the
	// schema. The class name is the one that *declares* the field, which is
	// not always the class the rest of the codebase reads it from — Source 2
	// single inheritance puts every base at +0, so a field declared by
	// C_BaseEntity is at the same offset in C_CitadelPlayerPawn.
	struct SchemaBinding
	{
		const char*    Class;
		const char*    Field;
		std::ptrdiff_t* Target;
	};

	const SchemaBinding kSchemaBindings[] =
	{
		{ "CGameSceneNode",           "m_vecAbsOrigin",     &Offsets::CGameSceneNode::m_vecAbsOrigin },
		{ "CGameSceneNode",           "m_bDormant",         &Offsets::CGameSceneNode::m_bDormant },
		{ "CSkeletonInstance",        "m_modelState",       &Offsets::CSkeletonInstance::m_modelState },
		{ "CModelState",              "m_ModelName",        &Offsets::CModelState::m_ModelName },

		{ "C_BaseEntity",             "m_pGameSceneNode",   &Offsets::C_BaseEntity::m_pGameSceneNode },
		{ "C_BaseEntity",             "m_iMaxHealth",       &Offsets::C_BaseEntity::m_iMaxHealth },
		{ "C_BaseEntity",             "m_iHealth",          &Offsets::C_BaseEntity::m_iHealth },
		{ "C_BaseEntity",             "m_iTeamNum",         &Offsets::C_BaseEntity::m_iTeamNum },
		{ "C_BaseEntity",             "m_hOwnerEntity",     &Offsets::C_BaseEntity::m_hOwnerEntity },
		// Declared by C_BaseEntity even though we only ever read it off a pawn.
		{ "C_BaseEntity",             "m_vecVelocity",      &Offsets::C_CitadelPlayerPawn::m_vecVelocity },

		{ "CitadelAbilityVData",      "m_mapWeaponInfos",   &Offsets::CitadelAbilityVData::m_mapWeaponInfos },

		{ "CCitadelPlayerController", "m_hHeroPawn",        &Offsets::CCitadelPlayerController::m_hHeroPawn },
		{ "CCitadelPlayerController", "m_PlayerDataGlobal", &Offsets::CCitadelPlayerController::m_PlayerDataGlobal },
		{ "PlayerDataGlobal_t",       "m_iHealthMax",       &Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealthMax },
		{ "PlayerDataGlobal_t",       "m_nHeroID",          &Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nHeroID },
		{ "PlayerDataGlobal_t",       "m_iGoldNetWorth",    &Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_nTotalSouls },
		{ "PlayerDataGlobal_t",       "m_iHealth",          &Offsets::CCitadelPlayerController::PlayerDataGlobal_t::m_iHealth },

		{ "C_CitadelPlayerPawn",      "m_nCurrencies",      &Offsets::C_CitadelPlayerPawn::m_nCurrencies },
		{ "C_CitadelPlayerPawn",      "m_nLevel",           &Offsets::C_CitadelPlayerPawn::m_nLevel },
		{ "C_CitadelPlayerPawn",      "m_flRespawnTime",    &Offsets::C_CitadelPlayerPawn::m_flRespawnTime },
		{ "C_BasePlayerPawn",         "m_hController",      &Offsets::C_BasePlayerPawn::m_hController },

		{ "C_CitadelTeam",            "m_vecFOWEntities",   &Offsets::C_CitadelTeam::m_vecFOWEntities },
		{ "STeamFOWEntity",           "m_nEntIndex",        &Offsets::STeamFOWEntity::m_nEntIndex },
		{ "STeamFOWEntity",           "m_bVisibleOnMap",    &Offsets::STeamFOWEntity::m_bVisibleOnMap },
	};

	// Walks the live schema and overwrites every binding above that resolves.
	// A field the walk can't produce keeps the value baked into Offsets.h, so
	// a schema-system change costs accuracy on that one field, not startup.
	void ApplySchema(DMA_Connection* Conn, DWORD pid)
	{
		std::vector<std::string> Wanted;
		for (const auto& b : kSchemaBindings)
			if (std::find(Wanted.begin(), Wanted.end(), b.Class) == Wanted.end())
				Wanted.emplace_back(b.Class);

		if (!SchemaWalker::Resolve(Conn, pid, Wanted))
		{
			Log::Warn("[Off] schema walk failed ({}) — all field offsets baked",
				SchemaWalker::Status());
			return;
		}

		size_t Applied = 0, Moved = 0, Missing = 0;
		for (const auto& b : kSchemaBindings)
		{
			const std::ptrdiff_t Live = SchemaWalker::Find(b.Class, b.Field);
			if (!Live)
			{
				Log::Warn("[Off] {}::{} unresolved — baked 0x{:X}", b.Class, b.Field, *b.Target);
				++Missing;
				continue;
			}

			if (Live != *b.Target)
			{
				Log::Info("[Off] {}::{} 0x{:X} (baked 0x{:X})", b.Class, b.Field, Live, *b.Target);
				++Moved;
			}

			*b.Target = Live;
			++Applied;
		}

		// m_nCurrencies[3]. Not a field of its own, so it rides the array it
		// indexes into instead of being resolved or baked separately.
		Offsets::C_CitadelPlayerPawn::m_nUnsecuredSouls =
			Offsets::C_CitadelPlayerPawn::m_nCurrencies + 3 * sizeof(int32_t);

		Log::Info("[Off] schema: {} applied, {} moved since last dump, {} baked",
			Applied, Moved, Missing);
	}
}

bool Offsets::ResolveOffsets(DMA_Connection* Conn)
{
	DWORD pid = Deadlock::Proc().GetPID();
	uintptr_t clientBase = Deadlock::Proc().GetModuleBase(GameModules::ClientDll);
	uintptr_t clientEnd  = clientBase + Deadlock::Proc().GetModuleSize(GameModules::ClientDll);

	// Field offsets first, so the schema summary precedes the signature scans
	// in the log. The module globals below are not schema-published and are
	// still found by pattern.
	ApplySchema(Conn, pid);

	// Patterns and fallback RVAs below track the dezlock-dump schema dump
	// (sdk/_patterns.hpp + sdk/_globals.hpp) for the current build.
	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"GameEntitySystem", Offsets::GameEntitySystem, 0x3CFEDA0,
		"48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3", 3, 7);

	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"ViewMatrix", Offsets::ViewMatrix, 0x3BCAD80,
		"48 8D 0D ? ? ? ? 48 C1 E0 06", 3, 7);

	// LEA, so this resolves to the CPrediction instance itself — callers use it
	// directly instead of dereferencing a pointer global as they did before.
	ResolveOffset(Conn, pid, clientBase, clientEnd,
		"CPrediction", Offsets::Prediction, 0x323AE00,
		"48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54", 3, 7);

	// dwLocalPlayerPawn has no pattern of its own in the dump — it is published
	// as a member of the CPrediction instance, so it rides that scan instead of
	// carrying an RVA that goes stale on its own schedule.
	Offsets::LocalPlayerPawn = Offsets::Prediction + Offsets::CPrediction::LocalPlayerPawn;
	Log::Info("[Off] LocalPlayerPawn=0x{:X}", Offsets::LocalPlayerPawn);

	DbgLog("All offsets resolved.");
	Offsets::bResolved.store(true, std::memory_order_release);
	return true;
}
