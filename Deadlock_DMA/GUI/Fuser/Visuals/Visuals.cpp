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
#include "Draw/Powerups.h"

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

	if (Draw_Powerups::bMasterToggle)
	{
		GuiWatchdog::GuiStage("Visuals/Powerups");
		Draw_Powerups::operator()();
	}

	ImGui::PopFont();
}

void Visuals::RenderSettings()
{
	ImGui::Checkbox("Enable Visuals", &bMasterToggle);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Players", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		ImGui::Checkbox("Enable##Players", &Draw_Players::bMasterToggle);

		ImGui::SeparatorText("Filters");
		ImGui::Checkbox("Hide Friendly",     &Draw_Players::bHideFriendly);
		ImGui::Checkbox("Hide Local Player", &Draw_Players::bHideLocalPlayer);
		ImGui::Checkbox("Visible Only",      &Draw_Players::bVisibleOnly);

		ImGui::SeparatorText("Skeleton & Box");
		ImGui::Checkbox("Bones", &Draw_Players::bDrawBones);
		if (Draw_Players::bDrawBones)
		{
			ImGui::Indent();
			ImGui::SetNextItemWidth(150.0f);
			ImGui::SliderFloat("Bones Thickness", &Draw_Players::fBonesThickness, 0.1f, 5.0f, "%.1f");
			ImGui::Unindent();
		}

		ImGui::Checkbox("Box", &Draw_Players::bDrawBox);
		if (Draw_Players::bDrawBox)
		{
			ImGui::Indent();
			ImGui::SetNextItemWidth(150.0f);
			ImGui::SliderFloat("Box Thickness", &Draw_Players::fBoxThickness, 0.1f, 5.0f, "%.1f");
			ImGui::Unindent();
		}

		ImGui::Checkbox("Head Circle",     &Draw_Players::bDrawHead);
		ImGui::Checkbox("Velocity Vector", &Draw_Players::bDrawVelocityVector);
		ImGui::Checkbox("Bone Numbers",    &Draw_Players::bBoneNumbers);

		ImGui::SeparatorText("Info Overlays");
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
		if (Draw_Players::bDrawUnsecuredSouls)
		{
			ImGui::Indent();
			ImGui::SetNextItemWidth(80.0f);
			ImGui::InputScalarN("Minimum Threshold",   ImGuiDataType_S32, &Draw_Players::UnsecuredSoulsMinimumThreshold,   1);
			ImGui::SetNextItemWidth(80.0f);
			ImGui::InputScalarN("Highlight Threshold", ImGuiDataType_S32, &Draw_Players::UnsecuredSoulsHighlightThreshold, 1);
			ImGui::Unindent();
		}

		ImGui::Checkbox("Show Distance", &Draw_Players::bShowDistance);
		ImGui::Checkbox("Hero Level",    &Draw_Players::bShowHeroLevel);
		ImGui::Checkbox("Respawn Timer", &Draw_Players::bShowRespawnTimer);

		ImGui::Unindent();
	}

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Troopers"))
	{
		ImGui::Indent();
		ImGui::Checkbox("Enable##Troopers",        &Draw_Troopers::bMasterToggle);
		ImGui::Checkbox("Hide Friendly##Troopers", &Draw_Troopers::bHideFriendly);
		ImGui::Checkbox("Lane Troopers",           &Draw_Troopers::bDrawLaneTroopers);
		ImGui::Checkbox("Walkers",                 &Draw_Troopers::bDrawWalkers);
		ImGui::Checkbox("Jungle Neutrals",         &Draw_Troopers::bDrawNeutrals);
		ImGui::Unindent();
	}

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Powerups / Breakables", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();
		ImGui::Checkbox("Enable##Powerups",        &Draw_Powerups::bMasterToggle);
		ImGui::Checkbox("Show Label##Powerups",    &Draw_Powerups::bShowLabel);
		ImGui::Checkbox("Show Distance##Powerups", &Draw_Powerups::bShowDistance);
		ImGui::SetNextItemWidth(150.0f);
		ImGui::SliderFloat("Circle Radius##Powerups", &Draw_Powerups::fCircleRadius, 1.0f, 12.0f, "%.1f");
		ImGui::Unindent();
	}

	ImGui::Spacing();
	ImGui::SeparatorText("World");
	ImGui::Checkbox("Bosses",  &Draw_Camps::bMasterToggle);
	ImGui::Checkbox("Sinners", &Draw_Sinners::bMasterToggle);
	ImGui::Checkbox("XP Orbs", &Draw_XpOrbs::bMasterToggle);
}