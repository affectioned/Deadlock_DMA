#pragma once

#include "makcu/makcu.h"
#include "Deadlock/Const/BoneListTypes.hpp"
#include <random>


class Aimbot
{
public:
	static void RenderSettings();
	static void OnFrame(DMA_Connection* Conn);
	static void RenderFOVCircle();

public:
	static inline bool bSettings{ true };
	static inline float fAlphaX{ 0.12f };
	static inline float fAlphaY{ 0.10f };
	static inline float fGaussianNoise{ 0.8f };
	static inline float fMaxPixelDistance{ 100.0f };
	static inline float fBulletVelocity{ 20000.0f };
	static inline float fLatencyMs{ 10.0f };
	static inline HitboxSlot eHitboxSlot{ HitboxSlot::Head };
	static inline bool bDrawMaxFOV{ true };
	static inline bool bPrediction{ true };
	static inline bool bIsActive = false;

private:
	static Vector2 GetAimDelta(const Vector2& CenterScreen);
	static inline std::random_device rd;
	static inline std::mt19937 gen{ rd() };
};