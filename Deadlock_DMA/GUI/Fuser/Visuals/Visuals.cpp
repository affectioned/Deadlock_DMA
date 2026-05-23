#include "pch.h"

#include "Visuals.h"

#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Watchdog/GuiWatchdog.h"

#include "Draw/Players.h"
#include "Draw/Troopers.h"
#include "Draw/Camps.h"
#include "Draw/Sinners.h"
#include "Draw/XpOrbs.h"

void Visuals::OnFrame()
{
	if (!bMasterToggle)
		return;

	auto DrawList = ImGui::GetWindowDrawList();
	auto WindowPos = ImGui::GetWindowPos();

	ImGui::PushFont(nullptr, 16.0f);

	if (Draw_Players::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/Players");
		Draw_Players::operator()();
	}

	if (Draw_Troopers::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/Troopers");
		Draw_Troopers::operator()();
	}

	if (Draw_Camps::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/Camps");
		Draw_Camps::operator()();
	}

	if (Draw_Sinners::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/Sinners");
		Draw_Sinners::operator()();
	}

	if (Draw_XpOrbs::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/XpOrbs");
		Draw_XpOrbs::operator()();
	}

	ImGui::PopFont();
}

void Visuals::RenderSettings()
{
	ImGui::Checkbox("Enable Visuals", &bMasterToggle);

	if (ImGui::CollapsingHeader("Players", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		ImGui::Checkbox("Master Toggle", &Draw_Players::bMasterToggle);
		ImGui::Checkbox("Hide Friendly", &Draw_Players::bHideFriendly);

		ImGui::Checkbox("Bones", &Draw_Players::bDrawBones);
		if (Draw_Players::bDrawBones)
		{
			ImGui::Indent();
			ImGui::SliderFloat("Bones Thickness", &Draw_Players::fBonesThickness, 0.1f, 5.0f, "%.1f");
			ImGui::Unindent();
		};

		ImGui::Checkbox("Box", &Draw_Players::bDrawBox);
		if (Draw_Players::bDrawBox)
		{
			ImGui::Indent();
			ImGui::SliderFloat("Box Thickness", &Draw_Players::fBoxThickness, 0.1f, 5.0f, "%.1f");
			ImGui::Unindent();
		};

		ImGui::Checkbox("Head Circle", &Draw_Players::bDrawHead);
		ImGui::Checkbox("Velocity Vector", &Draw_Players::bDrawVelocityVector);
		ImGui::Checkbox("Health Bars", &Draw_Players::bDrawHealthBar);
		if (Draw_Players::bDrawHealthBar)
		{
			ImGui::Indent();
			static constexpr const char* kHealthBarPositions[] = { "Top", "Bottom", "Left", "Right" };
			int hbPos = static_cast<int>(Draw_Players::eHealthBarPosition);
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::Combo("Position", &hbPos, kHealthBarPositions, IM_ARRAYSIZE(kHealthBarPositions)))
				Draw_Players::eHealthBarPosition = static_cast<EHealthBarPosition>(hbPos);
			ImGui::Unindent();
		}
		ImGui::Checkbox("Unsecured Souls", &Draw_Players::bDrawUnsecuredSouls);
		ImGui::Indent();
		ImGui::SetNextItemWidth(50.0f);
		ImGui::InputScalarN("Minimum Threshold", ImGuiDataType_S32, &Draw_Players::UnsecuredSoulsMinimumThreshold, 1);
		ImGui::SetNextItemWidth(50.0f);
		ImGui::InputScalarN("Highlight Threshold", ImGuiDataType_S32, &Draw_Players::UnsecuredSoulsHighlightThreshold, 1);
		ImGui::Unindent();
		ImGui::Checkbox("Hide Local", &Draw_Players::bHideLocalPlayer);
		ImGui::Checkbox("Show Distance", &Draw_Players::bShowDistance);
		ImGui::Checkbox("Bone Numbers", &Draw_Players::bBoneNumbers);
		ImGui::Checkbox("Visible Only", &Draw_Players::bVisibleOnly);

		ImGui::Unindent();
	}

	ImGui::Checkbox("Draw Troopers", &Draw_Troopers::bMasterToggle);
	ImGui::Checkbox("Hide Friendly Troopers", &Draw_Troopers::bHideFriendly);

	ImGui::Checkbox("Draw Bosses", &Draw_Camps::bMasterToggle);

	ImGui::Checkbox("Draw Sinners", &Draw_Sinners::bMasterToggle);

	ImGui::Checkbox("Draw XP Orbs", &Draw_XpOrbs::bMasterToggle);
}