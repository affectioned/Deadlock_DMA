#pragma once

#include "makcu/makcu.h"
#include "Deadlock/Const/BoneListTypes.hpp"
#include "Deadlock/Engine/Vector2.h"
#include "Humanizer.h"

#include <cstdint>
#include <unordered_map>


class AimAssist
{
public:
	static void RenderSettings();
	static void OnFrame(DMA_Connection* Conn);
	static void RenderFOVCircle();

public:
	static inline bool bSettings{ true };
	static inline bool bMasterToggle{ true };   // kill switch — when false aim assist never activates
	static inline float fMaxPixelDistance{ 100.0f };
	static inline HitboxSlot eHitboxSlot{ HitboxSlot::Head };
	static inline bool bDrawMaxFOV{ true };
	static inline bool bAimAtOrbs{ false };
	static inline bool bAllowAimThroughOcclusion{ false };
	static inline bool bIsActive = false;

	// Humanization. Reaction / snap timings are drawn per engagement from
	// N(mean, stddev); the settle alpha is the closing gain per 5 ms tick.
	static inline float fReactionMeanMs{ 190.0f };
	static inline float fReactionStdDevMs{ 35.0f };
	static inline float fSnapMeanMs{ 80.0f };
	static inline float fSnapStdDevMs{ 15.0f };
	static inline float fSettleAlpha{ 0.25f };
	static inline float fOvershootChance{ 0.35f };
	static inline float fVelocityCapPxSec{ 1800.0f };
	static inline float fMissChance{ 0.08f };
	static inline int   iMinVisibleTicks{ 2 };

	// Manual aggression dial, multiplying both the FOV radius and the miss
	// probability. Deliberately not auto-regulated — nothing cheat-side tracks
	// performance, so there is no K/D-shaped state for a report to corroborate.
	static inline float fSessionConfidenceBias{ 1.0f };

	// Lead prediction. Auto-detect path reads the base bullet speed from the
	// local pawn's primary-weapon-ability VData (CCitadelWeaponInfo). That's
	// the *template* base — hero stat scaling and item bonuses aren't applied;
	// use the manual override for exact accuracy.
	// All speeds in m/s; converted to hammer-units/sec at use site.
	static inline bool bUsePrediction{ true };
	static inline float fManualBulletSpeedMs{ 0.0f };   // 0 = auto-detect; >0 = override
	static constexpr float kDefaultBulletSpeedMs = 480.0f; // ~25000 hu/s baseline

private:
	struct AimTarget
	{
		bool     bValid{ false };
		uint64_t Key{ 0 };
		Vector2  Delta{};
		float    RangeMeters{ 0.0f };
		float    RadiusPx{ 8.0f };
	};

	class TargetTracker
	{
	public:
		struct TargetState
		{
			uint32_t Streak{ 0 };
			uint64_t Seq{ 0 };
			int16_t  LastBone{ -1 };
		};

		// unordered_map keeps references valid across insert and across erase of
		// other keys, so the caller may hold this until its next Observe call.
		TargetState& Observe(uint64_t Key, bool bVisible);

	private:
		// LRU bound = 32 — Observe runs under the pawn lock, so a larger map
		// produces visible stalls under full-match ten-pawn churn.
		static constexpr size_t kMaxTracked = 32;

		std::unordered_map<uint64_t, TargetState> m_States;
		uint64_t m_Seq{ 0 };
	};

	static AimTarget GetAimDelta(DMA_Connection* Conn, const Vector2& CenterScreen);
	static float EffectiveFOV();

	static inline TargetTracker m_Tracker{};
	static inline Humanizer m_Humanizer{};
};
