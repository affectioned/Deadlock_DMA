#pragma once

#include "Deadlock/Engine/Vector2.h"

#include <cstdint>
#include <mutex>
#include <random>

class Humanizer
{
public:
	struct Params
	{
		float ReactionMeanMs;
		float ReactionStdDevMs;
		float SnapMeanMs;
		float SnapStdDevMs;
		float SettleAlpha;
		float OvershootChance;
		float VelocityCapPxSec;
		float MissChance;
	};

	void Begin(uint64_t Key, float RadiusPx, float RangeMeters, const Params& P);
	void Reset();

	bool     Armed() const;
	uint64_t Key() const;

	Vector2 Step(const Vector2& Delta, float DeltaSeconds, const Params& P);

private:
	enum class Phase { Idle, React, Snap, Settle };

	mutable std::mutex m_Mutex;

	std::mt19937 m_Gen{ std::random_device{}() };

	Phase    m_Phase{ Phase::Idle };
	uint64_t m_Key{ 0 };

	float m_ReactSeconds{ 0.0f };
	float m_SnapSeconds{ 0.0f };
	float m_SnapGain{ 0.0f };
	float m_Elapsed{ 0.0f };
	float m_SnapEased{ 0.0f };

	Vector2 m_SnapVector{};
	Vector2 m_MissBias{};
	Vector2 m_Residual{};
};
