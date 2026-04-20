#include "pch.h"
#include "DMA/DMA.h"
#include "GUI/Keybinds/Keybinds.h"
#include "Aimbot.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Color Picker/Color Picker.h"
#include "Makcu/MyMakcu.h"

void Aimbot::RenderSettings()
{
	if (!bSettings) return;

	static bool bFirstFrame{ true };
	if (bFirstFrame)
	{
		(void)MyMakcu::m_Device.connect();
		bFirstFrame = false;
	}

	ImGui::Begin("Aimbot", &bSettings);

	if (MyMakcu::m_Device.isConnected()) {
		ImGui::TextColored(ImColor(0, 255, 0), "Makcu Connected!");
	}
	else {
		ImGui::TextColored(ImColor(255, 0, 0), "Makcu Disconnected!");
	}

	ImGui::SliderFloat("Alpha X", &fAlphaX, 0.01f, 1.0f, "%.2f");

	ImGui::SliderFloat("Alpha Y", &fAlphaY, 0.01f, 1.0f, "%.2f");

	ImGui::SliderFloat("Gaussian Noise", &fGaussianNoise, 0.0f, 2.0f, "%.2f");

	ImGui::SliderFloat("Max Pixel Distance", &fMaxPixelDistance, 10.0f, 500.0f, "%.1f");

	struct VelocityPreset { const char* name; float value; };
	static constexpr VelocityPreset kPresets[] = {
		{ "Custom",  0.0f   },
		{ "Hitscan", 999999.0f },
		{ "Fast",    25000.0f  },
		{ "Medium",  18000.0f  },
		{ "Slow",    10000.0f  },
	};

	// Find which preset matches current value (Custom if none)
	int presetIndex = 0;
	for (int i = 1; i < IM_ARRAYSIZE(kPresets); i++)
	{
		if (fBulletVelocity == kPresets[i].value)
		{
			presetIndex = i;
			break;
		}
	}

	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Velocity Preset", &presetIndex, [](void* data, int idx) {
		return static_cast<const VelocityPreset*>(data)[idx].name;
	}, (void*)kPresets, IM_ARRAYSIZE(kPresets)))
	{
		if (presetIndex != 0)
			fBulletVelocity = kPresets[presetIndex].value;
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("##BulletVelocity", &fBulletVelocity, 0.0f, 0.0f, "%.0f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Bullet velocity in engine units/s\nChange to override preset");

	ImGui::SliderFloat("Latency Compensation (ms)", &fLatencyMs, 0.0f, 100.0f, "%.1f");

	ImGui::Checkbox("Prediction", &bPrediction);

	static constexpr const char* kSlotNames[] = { "Head", "Neck", "Torso", "Arms", "Legs" };
	int slotIndex = static_cast<int>(eHitboxSlot);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Hitbox", &slotIndex, kSlotNames, IM_ARRAYSIZE(kSlotNames)))
		eHitboxSlot = static_cast<HitboxSlot>(slotIndex);

	ImGui::SameLine();

	ImGui::Checkbox("FOV Circle", &bDrawMaxFOV);

	ImGui::End();
}


#include "Deadlock/Entity List/EntityList.h"
Vector2 Aimbot::GetAimDelta(const Vector2& CenterScreen)
{
	std::scoped_lock lk(EntityList::m_PawnMutex, EntityList::m_ControllerMutex);

	Vector2 BestTargetDelta{};
	float BestDistance = FLT_MAX;

	// Grab local position once — we hold m_PawnMutex so direct access is safe
	Vector3 LocalPos{};
	if (EntityList::m_LocalPawnIndex >= 0)
		LocalPos = EntityList::m_PlayerPawns[EntityList::m_LocalPawnIndex].m_Position;

	// Smoothing lag: exponential filter with factor α takes (1-α)/α frames to converge.
	// At 5ms per frame this is how far behind the cursor is when tracking a moving target.
	const float kFrameMs    = 5.0f;
	const float smoothAlpha = std::max(std::min(fAlphaX, fAlphaY), 0.001f);
	const float smoothingLagMs = kFrameMs * (1.0f - smoothAlpha) / smoothAlpha;

	const float totalLatencySec = (fLatencyMs + smoothingLagMs) / 1000.0f;
	const float bulletSpeed = std::max(fBulletVelocity, 1.0f);

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

		HitboxSlot slot = eHitboxSlot;
		int FinalAimpointIndex = GetHeroBoneSlot(Pawn.GetModelPath(), slot);
		if (FinalAimpointIndex < 0) continue;

		Vector3 AimPointWorldPos = Pawn.m_BonePositions[FinalAimpointIndex];

		if (bPrediction && EntityList::m_LocalPawnIndex >= 0)
		{
			// Iterative travel-time prediction (2 passes converges well for fast targets)
			// Pass 1: estimate travel time to current bone position
			// Pass 2: refine using the predicted position's distance (more accurate at range)
			Vector3 PredictedPos = AimPointWorldPos;
			for (int i = 0; i < 2; i++)
			{
				float dist = LocalPos.Distance(PredictedPos);
				float travelTime = std::min(dist / bulletSpeed + totalLatencySec, 0.5f);
				PredictedPos = AimPointWorldPos + Pawn.m_Velocity * travelTime;
			}
			AimPointWorldPos = PredictedPos;
		}

		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(AimPointWorldPos, ScreenPos)) continue;

		Vector2 Delta = ScreenPos - CenterScreen;
		float Distance = sqrtf(Delta.x * Delta.x + Delta.y * Delta.y);

		if (Distance > fMaxPixelDistance) continue;

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTargetDelta = Delta;
		}
	}

	return BestTargetDelta;
}

void Aimbot::OnFrame(DMA_Connection* Conn)
{
	if (MyMakcu::m_Device.isConnected() == false) return;

	static auto LastTime = std::chrono::steady_clock::time_point();

	auto CurrentTime = std::chrono::high_resolution_clock::now();
	auto DeltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - LastTime).count();

	if (DeltaTime < 5) return;

	LastTime = CurrentTime;

	auto WindowSize = Fuser::m_ScreenSize;
	Vector2 CenterScreen{ WindowSize.x / 2.0f, WindowSize.y / 2.0f };

	Vector2 Delta = GetAimDelta(CenterScreen);

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

void Aimbot::RenderFOVCircle()
{
	if (!bDrawMaxFOV)
		return;

	auto WindowSize = ImGui::GetWindowSize();
	auto WindowPos = ImGui::GetWindowPos();
	ImVec2 CenterScreen{ WindowPos.x + (WindowSize.x / 2.0f), WindowPos.y + (WindowSize.y / 2.0f) };

	ImColor circleColor = bIsActive ? ColorPicker::AimbotFOVCircleActive : ColorPicker::AimbotFOVCircle;

	ImGui::GetWindowDrawList()->AddCircle(CenterScreen, fMaxPixelDistance, circleColor, 100, 1.5f);
}