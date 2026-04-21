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
		inline constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
		inline constexpr std::ptrdiff_t m_bDormant     = 0x103;
		inline constexpr std::ptrdiff_t m_modelState   = 0x150;
	}

	namespace CModelState
	{
		inline constexpr std::ptrdiff_t m_pBones    = 0x80;
		inline constexpr std::ptrdiff_t m_ModelName = 0xA8;
	}

	namespace C_BaseEntity
	{
		inline constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
		inline constexpr std::ptrdiff_t m_iMaxHealth     = 0x350;
		inline constexpr std::ptrdiff_t m_iHealth        = 0x354;
		inline constexpr std::ptrdiff_t m_iTeamNum       = 0x3F3;
	}

	namespace CCitadelPlayerController
	{
		inline constexpr std::ptrdiff_t m_hHeroPawn           = 0x8AC;
		inline constexpr std::ptrdiff_t m_PlayerDataGlobal = 0x8F0;

		namespace PlayerDataGlobal_t
		{
			inline constexpr std::ptrdiff_t m_iLevel                  = 0x8;
			inline constexpr std::ptrdiff_t m_iHealthMax               = 0x10;
			inline constexpr std::ptrdiff_t m_nHeroID                  = 0x1C;
			inline constexpr std::ptrdiff_t m_nTotalSouls              = 0x24;
			inline constexpr std::ptrdiff_t m_iHealth                  = 0x4C;
			inline constexpr std::ptrdiff_t m_bUltimateTrained         = 0x70;
			inline constexpr std::ptrdiff_t m_flUltimateCooldownStart  = 0x74;
			inline constexpr std::ptrdiff_t m_flUltimateCooldownEnd    = 0x78;
		}
	}

	namespace C_CitadelPlayerPawn
	{
		inline constexpr std::ptrdiff_t m_vecVelocity        = 0x438;
		inline constexpr std::ptrdiff_t m_hController        = 0x10A8;
		inline constexpr std::ptrdiff_t m_angEyeAngles       = 0x11B0;
		inline constexpr std::ptrdiff_t m_flRespawnTime      = 0x130C;
		inline constexpr std::ptrdiff_t m_nTotalUnspentSouls = 0x12D8;
		inline constexpr std::ptrdiff_t m_nUnsecuredSouls    = 0x12E4;
	}
}
