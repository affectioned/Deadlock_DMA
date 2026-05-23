#pragma once

#include "makcu/makcu.h"
#include "Deadlock/Const/BoneListTypes.hpp"
#include "Deadlock/Engine/Vector2.h"
#include <random>


class AimAssist
{
public:
	static void RenderSettings();
	static void OnFrame(DMA_Connection* Conn);
	static void RenderFOVCircle();

public:
	static inline bool bSettings{ true };
	static inline bool bMasterToggle{ true };   // kill switch — when false aim assist never activates
	static inline float fAlphaX{ 0.12f };
	static inline float fAlphaY{ 0.10f };
	static inline float fGaussianNoise{ 0.8f };
	static inline float fMaxPixelDistance{ 100.0f };
	static inline HitboxSlot eHitboxSlot{ HitboxSlot::Head };
	static inline bool bDrawMaxFOV{ true };
	static inline bool bAimAtOrbs{ false };
	static inline bool bVisibleOnly{ false };
	static inline bool bIsActive = false;

	// Lead prediction. Auto-detect path reads the base bullet speed from the
	// local pawn's primary-weapon-ability VData (CCitadelWeaponInfo). That's
	// the *template* base — hero stat scaling and item bonuses aren't applied;
	// use the manual override for exact accuracy.
	// All speeds in m/s; converted to hammer-units/sec at use site.
	static inline bool bUsePrediction{ true };
	static inline float fManualBulletSpeedMs{ 0.0f };   // 0 = auto-detect; >0 = override
	static constexpr float kDefaultBulletSpeedMs = 480.0f; // ~25000 hu/s baseline

private:
	static inline Vector2 GetAimDelta(DMA_Connection* Conn, const Vector2& CenterScreen);
	static inline std::random_device rd;
	static inline std::mt19937 gen{ rd() };
};