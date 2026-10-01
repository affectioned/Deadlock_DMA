#include "pch.h"
#include "DMA/DMA.h"
#include "Deadlock/Deadlock.h"
#include "GUI/Keybinds/Keybinds.h"
#include "Aim Assist.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Theme/Theme.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/Entity List/EntityList.h"

namespace
{
	float ConfidenceBias()
	{
		return std::clamp(AimAssist::fSessionConfidenceBias, 0.5f, 1.5f);
	}

	Humanizer::Params CurrentParams()
	{
		return Humanizer::Params{
			AimAssist::fReactionMeanMs,
			AimAssist::fReactionStdDevMs,
			AimAssist::fSnapMeanMs,
			AimAssist::fSnapStdDevMs,
			AimAssist::fSettleAlpha,
			AimAssist::fOvershootChance,
			AimAssist::fVelocityCapPxSec,
			AimAssist::fMissChance * ConfidenceBias(),
		};
	}
}

float AimAssist::EffectiveFOV()
{
	return fMaxPixelDistance * ConfidenceBias();
}

uint32_t AimAssist::TargetTracker::Observe(uint64_t Key, bool bVisible)
{
	const uint64_t Now = ++m_Seq;

	auto it = m_States.find(Key);
	if (it == m_States.end())
	{
		if (m_States.size() >= kMaxTracked)
		{
			auto Oldest = m_States.begin();
			for (auto c = m_States.begin(); c != m_States.end(); ++c)
				if (c->second.Seq < Oldest->second.Seq)
					Oldest = c;
			m_States.erase(Oldest);
		}
		it = m_States.emplace(Key, TargetState{}).first;
	}

	it->second.Seq = Now;
	it->second.Streak = bVisible ? it->second.Streak + 1 : 0;
	return it->second.Streak;
}

void AimAssist::RenderSettings()
{
	// Reconnect is handled by OnFrame's throttled retry, not tied to opening
	// this tab. Startup connect already ran via MyMakcu::Initialize().
	if (MyMakcu::m_Device.isConnected()) {
		ImGui::TextColored(Theme::Ok(), "Makcu Connected!");
	}
	else {
		ImGui::TextColored(Theme::Bad(), "Makcu Disconnected!");
	}

	ImGui::Checkbox("Enable Aim Assist", &bMasterToggle);
	ImGui::Separator();

	ImGui::SliderFloat("FOV", &fMaxPixelDistance, 10.0f, 500.0f, "%.1f");

	static constexpr const char* kSlotNames[] = { "Head", "Neck", "Torso", "Arms", "Legs" };
	int slotIndex = static_cast<int>(eHitboxSlot);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Hitbox", &slotIndex, kSlotNames, IM_ARRAYSIZE(kSlotNames)))
		eHitboxSlot = static_cast<HitboxSlot>(slotIndex);

	ImGui::SameLine();

	ImGui::Checkbox("FOV Circle", &bDrawMaxFOV);

	ImGui::Checkbox("Aim At Orbs", &bAimAtOrbs);

	ImGui::Checkbox("Allow Aim Through Occlusion", &bAllowAimThroughOcclusion);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Was \"Visible Only\", inverted. Off (default) skips every target the\n"
		                  "FOW oracle has not confirmed visible. On removes the gate entirely,\n"
		                  "including the minimum-visible-ticks dwell.");

	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderInt("Min Visible Ticks", &iMinVisibleTicks, 1, 10);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Consecutive confirmed-visible polls before a target becomes eligible.");

	ImGui::SeparatorText("Humanization");

	ImGui::SliderFloat("Reaction Mean (ms)", &fReactionMeanMs, 60.0f, 400.0f, "%.0f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Delay before the first motion of a new engagement. Clamped to [90, 350] ms.");

	ImGui::SliderFloat("Reaction StdDev (ms)", &fReactionStdDevMs, 0.0f, 120.0f, "%.0f");

	ImGui::SliderFloat("Snap Mean (ms)", &fSnapMeanMs, 30.0f, 200.0f, "%.0f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Duration of the flick phase, which covers 70-85%% of the angular distance.");

	ImGui::SliderFloat("Snap StdDev (ms)", &fSnapStdDevMs, 0.0f, 60.0f, "%.0f");

	ImGui::SliderFloat("Settle Alpha", &fSettleAlpha, 0.02f, 0.80f, "%.2f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Closing gain per 5 ms tick after the flick. Lower = slower, softer convergence.");

	ImGui::SliderFloat("Overshoot Chance", &fOvershootChance, 0.0f, 1.0f, "%.2f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Probability the flick targets 102-108%% of the delta and corrects back.");

	ImGui::SliderFloat("Velocity Cap (px/s)", &fVelocityCapPxSec, 200.0f, 6000.0f, "%.0f");

	ImGui::SliderFloat("Miss Chance", &fMissChance, 0.0f, 0.35f, "%.3f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Base per-engagement probability of a deliberate near-miss. Scales with range, capped at 0.35.");

	ImGui::SeparatorText("Session");

	ImGui::SliderFloat("Confidence Bias", &fSessionConfidenceBias, 0.5f, 1.5f, "%.2f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Multiplies both the FOV radius and the miss chance.\n"
		                  "Manual only — nothing auto-regulates this, and the cheat keeps no\n"
		                  "performance history of its own to regulate against.");
	ImGui::TextColored(Theme::Info(), "Effective FOV: %.0f px   Effective miss chance: %.3f",
		EffectiveFOV(), fMissChance * std::clamp(fSessionConfidenceBias, 0.5f, 1.5f));

	ImGui::SeparatorText("Prediction");

	ImGui::Checkbox("Lead Prediction", &bUsePrediction);
	if (bUsePrediction)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Manual Speed (m/s)", &fManualBulletSpeedMs, 0.0f, 1500.0f, "%.0f");
		ImGui::SameLine();
		ImGui::TextDisabled("(0 = auto)");

		const float liveHu = EntityList::g_LocalBulletSpeed.load(std::memory_order_relaxed);

		if (fManualBulletSpeedMs > 0.0f)
			ImGui::TextColored(Theme::Info(), "Using manual: %.0f m/s", fManualBulletSpeedMs);
		else if (liveHu > 0.0f)
			ImGui::TextColored(Theme::Ok(), "Auto-detected: %.0f m/s (from VData)", liveHu / HammerUnitsPerMeter);
		else
			ImGui::TextColored(Theme::Warn(), "Auto-detect pending — defaulting to %.0f m/s.", kDefaultBulletSpeedMs);
	}
}


AimAssist::AimTarget AimAssist::GetAimDelta(DMA_Connection* Conn, const Vector2& CenterScreen)
{
	AimTarget Best{};
	float BestDistance = FLT_MAX;
	const float FovRadius = EffectiveFOV();

	auto Consider = [&](const Vector3& WorldPos, uintptr_t Key, float RangeMeters, const Vector3* pOrigin)
	{
		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(WorldPos, ScreenPos)) return;

		Vector2 Delta = ScreenPos - CenterScreen;
		float Distance = sqrtf(Delta.x * Delta.x + Delta.y * Delta.y);

		if (Distance > FovRadius) return;
		if (Distance >= BestDistance) return;

		// Miss offsets are expressed as a fraction of the target's apparent
		// size, so a fixed pixel bias doesn't become a range-invariant tell.
		float RadiusPx = 8.0f;
		Vector2 OriginScreen{};
		if (pOrigin && Deadlock::WorldToScreen(*pOrigin, OriginScreen))
		{
			const float hx = OriginScreen.x - ScreenPos.x;
			const float hy = OriginScreen.y - ScreenPos.y;
			RadiusPx = std::max(sqrtf(hx * hx + hy * hy) * 0.10f, 3.0f);
		}

		BestDistance = Distance;
		Best = AimTarget{ true, static_cast<uint64_t>(Key), Delta, RangeMeters, RadiusPx };
	};

	{
		GuiWatchdog::DmaStage("Aim Assist/awaiting Pawn+Controller");
		std::scoped_lock lk(EntityList::m_PawnMutex, EntityList::m_ControllerMutex);
		GuiWatchdog::DmaStage("Aim Assist/scanning pawns");

		// Resolve lead-prediction inputs while the pawn lock is held, so the
		// shooter position and the target bones come from the same snapshot.
		// Internal math is hu/s to match memory units; UI exposes m/s.
		const bool bPredictEnabled = bUsePrediction && EntityList::m_LocalPawnIndex >= 0;
		const Vector3 LocalPos = bPredictEnabled
			? EntityList::m_PlayerPawns[EntityList::m_LocalPawnIndex].m_Position
			: Vector3{};
		const float BulletSpeed = [&] {
			if (!bPredictEnabled) return 0.0f;
			if (fManualBulletSpeedMs > 0.0f) return fManualBulletSpeedMs * HammerUnitsPerMeter;
			float liveHu = EntityList::g_LocalBulletSpeed.load(std::memory_order_relaxed);
			return liveHu > 0.0f ? liveHu : kDefaultBulletSpeedMs * HammerUnitsPerMeter;
		}();

		auto LeadPredict = [&](const Vector3& WorldPos, const Vector3& Velocity) -> Vector3
		{
			// Two-iteration fixed-point: t = dist / v, predicted = bone + vel * t.
			// Converges well within a pixel at typical engagement ranges.
			Vector3 pred = WorldPos;
			for (int i = 0; i < 2; i++)
				pred = WorldPos + Velocity * (pred.Distance(LocalPos) / BulletSpeed);
			return pred;
		};

		for (auto& Pawn : EntityList::m_PlayerPawns)
		{
			if (Pawn.IsInvalid() || Pawn.IsLocalPlayer() || Pawn.IsFriendly())
				continue;

			auto ControllerAddress = EntityList::GetEntityAddressFromHandle(Pawn.m_hController);

			if (!ControllerAddress)
				continue;

			auto ControllerIt = std::ranges::find(EntityList::m_PlayerControllers, C_BaseEntity{ ControllerAddress });

			if (ControllerIt == EntityList::m_PlayerControllers.end())
				continue;

			if (ControllerIt->IsDead())
				continue;

			const bool bVisible = EntityList::IsEntityConfirmedVisible(Pawn.m_EntityAddress);
			const uint32_t Streak = m_Tracker.Observe(Pawn.m_EntityAddress, bVisible);

			// Streak is 0 whenever the pawn is occluded, so this one test covers
			// both the visibility gate and the minimum-dwell requirement.
			if (!bAllowAimThroughOcclusion && Streak < static_cast<uint32_t>(std::max(iMinVisibleTicks, 1)))
				continue;

			HitboxSlot slot = eHitboxSlot;
			int FinalAimpointIndex = GetHeroBoneSlot(Pawn.GetModelPath(), slot);
			if (FinalAimpointIndex < 0) continue;

			const Vector3& Bone = Pawn.m_BonePositions[FinalAimpointIndex];
			const Vector3 Aimpoint = bPredictEnabled ? LeadPredict(Bone, Pawn.m_Velocity) : Bone;
			Consider(Aimpoint, Pawn.m_EntityAddress, Pawn.DistanceFromLocalPlayer(true), &Pawn.m_Position);
		}
	}

	if (bAimAtOrbs)
	{
		GuiWatchdog::DmaStage("Aim Assist/awaiting XpOrb");
		std::scoped_lock orbLk(EntityList::m_XpOrbMutex);
		GuiWatchdog::DmaStage("Aim Assist/scanning orbs");

		for (auto& Orb : EntityList::m_XpOrbs)
		{
			if (Orb.IsInvalid() || Orb.IsDormant()) continue;
			Consider(Orb.m_Position, Orb.m_EntityAddress, Orb.DistanceFromLocalPlayer(true), nullptr);
		}
	}

	GuiWatchdog::DmaStage("Aim Assist/done");
	return Best;
}

void AimAssist::OnFrame(DMA_Connection* Conn)
{
	// Throttled reconnect: if the device was unplugged (or the startup
	// connect from MyMakcu::Initialize failed because it wasn't plugged in
	// yet), retry every 2s. Previously this retry lived in RenderSettings,
	// so it only fired if the user opened the Aim Assist tab.
	if (!MyMakcu::m_Device.isConnected())
	{
		static auto lastAttempt = std::chrono::steady_clock::time_point{};
		auto now = std::chrono::steady_clock::now();
		if (now - lastAttempt >= std::chrono::seconds(2))
		{
			(void)MyMakcu::m_Device.connect();
			lastAttempt = now;
		}
		return;
	}

	if (!bMasterToggle)
	{
		m_Humanizer.Reset();
		return;
	}

	static auto LastTime = std::chrono::steady_clock::time_point();

	const auto CurrentTime = std::chrono::steady_clock::now();
	const auto Gap = CurrentTime - LastTime;

	if (Gap < std::chrono::milliseconds(5)) return;

	LastTime = CurrentTime;

	// OnFrame only runs while the aim key is held, so a long gap means the key
	// was released — the next press has to pay a fresh reaction delay.
	if (Gap > std::chrono::milliseconds(250))
		m_Humanizer.Reset();

	const float DeltaSeconds = std::min(std::chrono::duration<float>(Gap).count(), 0.050f);

	auto WindowSize = Fuser::m_ScreenSize;
	Vector2 CenterScreen{ WindowSize.x / 2.0f, WindowSize.y / 2.0f };

	const AimTarget Target = GetAimDelta(Conn, CenterScreen);

	if (!Target.bValid)
	{
		m_Humanizer.Reset();
		return;
	}

	const Humanizer::Params Params = CurrentParams();

	if (!m_Humanizer.Armed() || m_Humanizer.Key() != Target.Key)
		m_Humanizer.Begin(Target.Key, Target.RadiusPx, Target.RangeMeters, Params);

	const Vector2 Move = m_Humanizer.Step(Target.Delta, DeltaSeconds, Params);

	if (Move.x == 0.0f && Move.y == 0.0f)
		return;

	(void)MyMakcu::m_Device.mouseMove(static_cast<int32_t>(Move.x), static_cast<int32_t>(Move.y));
}

void AimAssist::RenderFOVCircle()
{
	if (!bDrawMaxFOV)
		return;

	auto WindowSize = ImGui::GetWindowSize();
	auto WindowPos = ImGui::GetWindowPos();
	ImVec2 CenterScreen{ WindowPos.x + (WindowSize.x / 2.0f), WindowPos.y + (WindowSize.y / 2.0f) };

	ImColor circleColor = bIsActive ? ColorPicker::AimAssistFOVCircleActive : ColorPicker::AimAssistFOVCircle;

	ImGui::GetWindowDrawList()->AddCircle(CenterScreen, EffectiveFOV(), circleColor, 100, 1.5f);
}
