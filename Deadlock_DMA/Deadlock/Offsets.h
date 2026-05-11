#pragma once
#include "DMA/Memory/SigScan.h"

namespace Offsets
{
	bool ResolveOffsets(DMA_Connection* Conn);

	inline constexpr std::ptrdiff_t FirstEntityList = 0x10;
	inline std::ptrdiff_t GameEntitySystem = 0;
	inline std::ptrdiff_t LocalController = 0;
	inline std::ptrdiff_t ViewMatrix = 0;
	inline std::ptrdiff_t Prediction = 0; // client.dll::CPrediction (pointer to CPrediction instance)

	// Absolute address of CCitadel_Modifier_MeleeCharge::`vftable'. Resolved at startup
	// as clientBase + RVA. Used by ParryWarn to identify "enemy is charging heavy melee"
	// modifier instances by comparing the first 8 bytes of each modifier in the target's
	// modifier list. RVA-only — no sig — so update on game patch (see schema dump:
	// internal-sdk/client/CCitadel_Modifier_MeleeCharge.hpp).
	inline uintptr_t MeleeChargeVTable = 0;
	inline constexpr std::ptrdiff_t MeleeChargeVTableRVA = 0x21F0768;

	namespace CModifierProperty
	{
		inline constexpr std::ptrdiff_t m_vecModifiers = 0x38; // CUtlVector<CBaseModifier*> (count @+0, data @+8)
	}

	namespace CPrediction
	{
		// Field name unconfirmed — not exposed in schema dump, falls in unnamed padding at +0x68 of CPrediction.
		inline constexpr std::ptrdiff_t ServerTime = 0x68;
	}

	namespace CGameSceneNode
	{
		inline constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8; // VectorWS (12b)
		inline constexpr std::ptrdiff_t m_bDormant     = 0x103; // bool (1b)
	}

	namespace CSkeletonInstance 
	{
		inline constexpr std::ptrdiff_t m_modelState = 0x150; // CModelState (608b)
	}

	namespace CModelState
	{
		inline constexpr std::ptrdiff_t m_pBones    = 0x80;
		inline constexpr std::ptrdiff_t m_ModelName = 0xA8; // CUtlSymbolLarge (8b)
	}

	namespace C_BaseEntity
	{
		inline constexpr std::ptrdiff_t m_pGameSceneNode  = 0x330; // CGameSceneNode* (8b)
		inline constexpr std::ptrdiff_t m_pModifierProp   = 0x348; // CModifierProperty* (8b)
		inline constexpr std::ptrdiff_t m_iMaxHealth      = 0x350; // int32 (4b)
		inline constexpr std::ptrdiff_t m_iHealth         = 0x354; // int32 (4b)
		inline constexpr std::ptrdiff_t m_iTeamNum        = 0x3F3; // uint8 (1b)
		// Non-schema slot between m_nSubclassID (0x388) and m_nSimulationTick (0x398).
		// Set at runtime; null until the engine populates the subclass data table.
		// Source: github.com/neverlosecc/source2sdk (deadlock branch).
		inline constexpr std::ptrdiff_t m_pSubclassVData  = 0x390; // void* (8b)
		inline constexpr std::ptrdiff_t m_hOwnerEntity    = 0x51C; // CHandle< C_BaseEntity > (4b)
	}

	// Inside CitadelAbilityVData (the subclass-data type for ability entities
	// like citadel_ability_primary_weapon). m_WeaponInfo is an inline
	// CCitadelWeaponInfo (1912 bytes) at +0x158; m_flBulletSpeed is at +0xB4
	// inside it = +0x20C absolute. This is the muzzle speed for hitscan bullets,
	// not entity projectiles.
	namespace CitadelAbilityVData
	{
		inline constexpr std::ptrdiff_t m_flBulletSpeed = 0x20C; // float32 (4b) — base bullet speed in hu/s
	}

	namespace CCitadelPlayerController
	{
		inline constexpr std::ptrdiff_t m_hHeroPawn           = 0x8AC; // CHandle< C_CitadelPlayerPawn > (4b)
		inline constexpr std::ptrdiff_t m_PlayerDataGlobal = 0x8F0; // PlayerDataGlobal_t (816b)

		namespace PlayerDataGlobal_t
		{
			inline constexpr std::ptrdiff_t m_iHealthMax               = 0x10; // int32 (4b)
			inline constexpr std::ptrdiff_t m_nHeroID                  = 0x1C; // HeroID_t (4b)
			inline constexpr std::ptrdiff_t m_nTotalSouls              = 0x24;
			inline constexpr std::ptrdiff_t m_iHealth                  = 0x4C; // int32 (4b)
		}
	}

	namespace C_CitadelPlayerPawn
	{
		inline constexpr std::ptrdiff_t m_vecVelocity        = 0x438; // CNetworkVelocityVector (40b)
		inline constexpr std::ptrdiff_t m_nCurrencies		 = 0x12D8; // int32[6] (24b)
		inline constexpr std::ptrdiff_t m_nUnsecuredSouls    = 0x12E4;
	}

	namespace C_BasePlayerPawn
	{
		inline constexpr std::ptrdiff_t m_hController = 0x10A8; // CHandle< CBasePlayerController > (4b)
	}

	namespace C_CitadelTeam
	{
		constexpr std::ptrdiff_t m_vecFOWEntities = 0x6C8; // C_UtlVectorEmbeddedNetworkVar< STeamFOWEntity > (104b)

	}

	namespace STeamFOWEntity
	{
		constexpr std::ptrdiff_t m_nEntIndex     = 0x30; // CEntityIndex (4b)
		constexpr std::ptrdiff_t m_bVisibleOnMap = 0x41; // bool (1b)
	}
}
