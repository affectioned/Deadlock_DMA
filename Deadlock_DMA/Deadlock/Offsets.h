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
		inline constexpr std::ptrdiff_t m_pGameSceneNode = 0x330; // CGameSceneNode* (8b)
		inline constexpr std::ptrdiff_t m_iMaxHealth     = 0x350; // int32 (4b)
		inline constexpr std::ptrdiff_t m_iHealth        = 0x354; // int32 (4b)
		inline constexpr std::ptrdiff_t m_iTeamNum       = 0x3F3; // uint8 (1b)
	}

	namespace CCitadelPlayerController
	{
		inline constexpr std::ptrdiff_t m_hHeroPawn           = 0x8AC; // CHandle< C_CitadelPlayerPawn > (4b)
		inline constexpr std::ptrdiff_t m_PlayerDataGlobal = 0x8F0; // PlayerDataGlobal_t (816b)

		namespace PlayerDataGlobal_t
		{
			inline constexpr std::ptrdiff_t m_iLevel                  = 0x8; // int32 (4b)
			inline constexpr std::ptrdiff_t m_iHealthMax               = 0x10; // int32 (4b)
			inline constexpr std::ptrdiff_t m_nHeroID                  = 0x1C; // HeroID_t (4b)
			inline constexpr std::ptrdiff_t m_nTotalSouls              = 0x24; 
			inline constexpr std::ptrdiff_t m_iHealth                  = 0x4C; // int32 (4b)
		}
	}

	namespace C_CitadelPlayerPawn
	{
		inline constexpr std::ptrdiff_t m_vecVelocity        = 0x438; // CNetworkVelocityVector (40b)
		inline constexpr std::ptrdiff_t m_angEyeAngles       = 0x11B0; // QAngle (12b)
		inline constexpr std::ptrdiff_t m_flRespawnTime      = 0x130C; // GameTime_t (4b)
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
}
