#include "pch.h"
#include "DMA/DMA.h"
#include "Deadlock/Deadlock.h"
#include "GUI/Keybinds/Keybinds.h"
#include "Aim Assist.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/Entity List/EntityList.h"

void AimAssist::RenderSettings()
{
	static bool bFirstFrame{ true };
	if (bFirstFrame)
	{
		(void)MyMakcu::m_Device.connect();
		bFirstFrame = false;
	}

	if (MyMakcu::m_Device.isConnected()) {
		ImGui::TextColored(ImColor(0, 255, 0), "Makcu Connected!");
	}
	else {
		ImGui::TextColored(ImColor(255, 0, 0), "Makcu Disconnected!");
	}

	ImGui::Checkbox("Enable Aim Assist", &bMasterToggle);
	ImGui::Separator();

	ImGui::SliderFloat("Alpha X", &fAlphaX, 0.01f, 1.0f, "%.2f");

	ImGui::SliderFloat("Alpha Y", &fAlphaY, 0.01f, 1.0f, "%.2f");

	ImGui::SliderFloat("Gaussian Noise", &fGaussianNoise, 0.0f, 2.0f, "%.2f");

	ImGui::SliderFloat("FOV", &fMaxPixelDistance, 10.0f, 500.0f, "%.1f");

	static constexpr const char* kSlotNames[] = { "Head", "Neck", "Torso", "Arms", "Legs" };
	int slotIndex = static_cast<int>(eHitboxSlot);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Hitbox", &slotIndex, kSlotNames, IM_ARRAYSIZE(kSlotNames)))
		eHitboxSlot = static_cast<HitboxSlot>(slotIndex);

	ImGui::SameLine();

	ImGui::Checkbox("FOV Circle", &bDrawMaxFOV);

	ImGui::Checkbox("Aim At Orbs", &bAimAtOrbs);

	ImGui::Checkbox("Visible Only", &bVisibleOnly);

	ImGui::Checkbox("Lead Prediction", &bUsePrediction);
	if (bUsePrediction)
	{
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Manual Speed (m/s)", &fManualBulletSpeedMs, 0.0f, 1500.0f, "%.0f");
		ImGui::SameLine();
		ImGui::TextDisabled("(0 = auto)");

		const float liveHu = EntityList::g_LocalBulletSpeed.load(std::memory_order_relaxed);

		if (fManualBulletSpeedMs > 0.0f)
			ImGui::TextColored(ImColor(80, 180, 255), "Using manual: %.0f m/s", fManualBulletSpeedMs);
		else if (liveHu > 0.0f)
			ImGui::TextColored(ImColor(0, 255, 0), "Auto-detected: %.0f m/s (from VData)", liveHu / HammerUnitsPerMeter);
		else
			ImGui::TextColored(ImColor(255, 200, 0), "Auto-detect pending — defaulting to %.0f m/s.", kDefaultBulletSpeedMs);
	}
}


Vector2 AimAssist::GetAimDelta(DMA_Connection* Conn, const Vector2& CenterScreen)
{
	Vector2 BestTargetDelta{};
	float BestDistance = FLT_MAX;

	auto Consider = [&](const Vector3& WorldPos)
	{
		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(WorldPos, ScreenPos)) return;

		Vector2 Delta = ScreenPos - CenterScreen;
		float Distance = sqrtf(Delta.x * Delta.x + Delta.y * Delta.y);

		if (Distance > fMaxPixelDistance) return;

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTargetDelta = Delta;
		}
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

			if (bVisibleOnly && !EntityList::IsEntityVisible(Pawn.m_EntityAddress))
				continue;

			HitboxSlot slot = eHitboxSlot;
			int FinalAimpointIndex = GetHeroBoneSlot(Pawn.GetModelPath(), slot);
			if (FinalAimpointIndex < 0) continue;

			const Vector3& Bone = Pawn.m_BonePositions[FinalAimpointIndex];
			Consider(bPredictEnabled ? LeadPredict(Bone, Pawn.m_Velocity) : Bone);
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
			Consider(Orb.m_Position);
		}
	}

	GuiWatchdog::DmaStage("Aim Assist/done");
	return BestTargetDelta;
}

void AimAssist::OnFrame(DMA_Connection* Conn)
{
	if (MyMakcu::m_Device.isConnected() == false) return;

	static auto LastTime = std::chrono::steady_clock::time_point();

	auto CurrentTime = std::chrono::high_resolution_clock::now();
	auto DeltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - LastTime).count();

	if (DeltaTime < 5) return;

	LastTime = CurrentTime;

	auto WindowSize = Fuser::m_ScreenSize;
	Vector2 CenterScreen{ WindowSize.x / 2.0f, WindowSize.y / 2.0f };

	Vector2 Delta = GetAimDelta(Conn, CenterScreen);

	// Lerp from no movement toward full delta
	// fAlphaX/Y in (0.0, 1.0]: lower = smoother
	Vector2 MoveAmount{
		std::lerp(0.0f, Delta.x, fAlphaX),
		std::lerp(0.0f, Delta.y, fAlphaY)
	};

	// https://en.cppreference.com/w/cpp/numeric/random/normal_distribution.html
	std::normal_distribution<float> noise(0.0f, fGaussianNoise);

	MoveAmount.x += noise(gen);
	MoveAmount.y += noise(gen);

	(void)MyMakcu::m_Device.mouseMove(static_cast<int32_t>(MoveAmount.x), static_cast<int32_t>(MoveAmount.y));
}

void AimAssist::RenderFOVCircle()
{
	if (!bDrawMaxFOV)
		return;

	auto WindowSize = ImGui::GetWindowSize();
	auto WindowPos = ImGui::GetWindowPos();
	ImVec2 CenterScreen{ WindowPos.x + (WindowSize.x / 2.0f), WindowPos.y + (WindowSize.y / 2.0f) };

	ImColor circleColor = bIsActive ? ColorPicker::AimAssistFOVCircleActive : ColorPicker::AimAssistFOVCircle;

	ImGui::GetWindowDrawList()->AddCircle(CenterScreen, fMaxPixelDistance, circleColor, 100, 1.5f);
}