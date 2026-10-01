#include "pch.h"
#include "Humanizer.h"

#include <chrono>
#include <cmath>

namespace
{
	constexpr float kTwoPi = 6.28318530718f;

	float Normal(std::mt19937& Gen, float Mean, float StdDev, float Lo, float Hi)
	{
		std::normal_distribution<float> d(Mean, std::max(StdDev, 0.0f));
		return std::clamp(d(Gen), Lo, Hi);
	}

	float Uniform(std::mt19937& Gen, float Lo, float Hi)
	{
		std::uniform_real_distribution<float> d(Lo, Hi);
		return d(Gen);
	}

	// Bell-shaped velocity, zero accel at both ends — a constant-rate ramp is the
	// clearest replay tell there is.
	float MinimumJerk(float u)
	{
		return u * u * u * (10.0f + u * (-15.0f + 6.0f * u));
	}
}

void Humanizer::Begin(uint64_t Key, float RadiusPx, float RangeMeters, const Params& P)
{
	std::scoped_lock lk(m_Mutex);

	const uint64_t Tick = static_cast<uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	m_Gen.seed(static_cast<std::mt19937::result_type>((Key ^ Tick) & 0xFFFFFFFFull));

	m_Key       = Key;
	m_Phase     = Phase::React;
	m_Elapsed   = 0.0f;
	m_SnapEased = 0.0f;
	m_SnapVector = {};
	m_Residual   = {};

	m_ReactSeconds = Normal(m_Gen, P.ReactionMeanMs, P.ReactionStdDevMs, 90.0f, 350.0f) * 0.001f;
	m_SnapSeconds  = Normal(m_Gen, P.SnapMeanMs, P.SnapStdDevMs, 30.0f, 160.0f) * 0.001f;

	m_SnapGain = Uniform(m_Gen, 0.70f, 0.85f);
	if (Uniform(m_Gen, 0.0f, 1.0f) < P.OvershootChance)
		m_SnapGain = Uniform(m_Gen, 1.02f, 1.08f);

	m_MissBias = {};
	const float MissChance = std::min(P.MissChance * (1.0f + RangeMeters / 25.0f), 0.35f);
	if (Uniform(m_Gen, 0.0f, 1.0f) < MissChance)
	{
		const float Magnitude = Uniform(m_Gen, 0.25f, 0.8f) * RadiusPx;
		const float Theta     = Uniform(m_Gen, 0.0f, kTwoPi);
		m_MissBias = { std::cos(Theta) * Magnitude, std::sin(Theta) * Magnitude };
	}
}

void Humanizer::Reset()
{
	std::scoped_lock lk(m_Mutex);

	m_Phase      = Phase::Idle;
	m_Key        = 0;
	m_Elapsed    = 0.0f;
	m_SnapEased  = 0.0f;
	m_SnapVector = {};
	m_MissBias   = {};
	m_Residual   = {};
}

bool Humanizer::Armed() const
{
	std::scoped_lock lk(m_Mutex);
	return m_Phase != Phase::Idle;
}

uint64_t Humanizer::Key() const
{
	std::scoped_lock lk(m_Mutex);
	return m_Key;
}

Vector2 Humanizer::Step(const Vector2& Delta, float DeltaSeconds, const Params& P)
{
	std::scoped_lock lk(m_Mutex);

	if (m_Phase == Phase::Idle)
		return {};

	const Vector2 Aim{ Delta.x + m_MissBias.x, Delta.y + m_MissBias.y };

	m_Elapsed += DeltaSeconds;

	if (m_Phase == Phase::React)
	{
		if (m_Elapsed < m_ReactSeconds)
			return {};

		m_Phase      = Phase::Snap;
		m_Elapsed   -= m_ReactSeconds;
		m_SnapEased  = 0.0f;
		// Frozen at flick onset: the snap runs open-loop off this estimate and
		// the settle phase absorbs whatever the target did meanwhile.
		m_SnapVector = Aim;
	}

	Vector2 Move{};

	if (m_Phase == Phase::Snap)
	{
		const float u     = std::min(m_Elapsed / m_SnapSeconds, 1.0f);
		const float Eased = MinimumJerk(u);
		const float Step  = (Eased - m_SnapEased) * m_SnapGain;
		m_SnapEased = Eased;

		Move = { m_SnapVector.x * Step, m_SnapVector.y * Step };

		if (u >= 1.0f)
			m_Phase = Phase::Settle;
	}
	else
	{
		const float PerTick = std::clamp(P.SettleAlpha, 0.01f, 0.99f);
		const float Alpha   = 1.0f - std::pow(1.0f - PerTick, DeltaSeconds / 0.005f);
		Move = { Aim.x * Alpha, Aim.y * Alpha };
	}

	const float Cap = std::max(P.VelocityCapPxSec, 1.0f) * DeltaSeconds;
	const float Mag = std::sqrt(Move.x * Move.x + Move.y * Move.y);
	if (Mag > Cap && Mag > 0.0f)
	{
		const float Scale = Cap / Mag;
		Move = { Move.x * Scale, Move.y * Scale };
	}

	// The HID link only carries whole counts; without carrying the fraction the
	// settle phase truncates to zero and never converges.
	Move = { Move.x + m_Residual.x, Move.y + m_Residual.y };

	const Vector2 Emit{ std::trunc(Move.x), std::trunc(Move.y) };
	m_Residual = { Move.x - Emit.x, Move.y - Emit.y };

	return Emit;
}
