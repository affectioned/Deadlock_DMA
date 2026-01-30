#include "pch.h"
#include "Color Picker.h"
#include "GUI/Fuser/ESP/ESP.h"

void ColorPicker::Render()
{
	if (!bMasterToggle) return;

	ImGui::Begin("Color Picker", &bMasterToggle);

	// HiddenKing Team Colors
	if (ImGui::CollapsingHeader("HiddenKing Team", ImGuiTreeNodeFlags_DefaultOpen))
	{
		MyColorPicker("Name Tag", ColorPicker::HiddenKingNameTagColor);
		MyColorPicker("Bone", ColorPicker::HiddenKingBoneColor);
		MyColorPicker("Radar", ColorPicker::HiddenKingRadarColor);
		MyColorPicker("Trooper", ColorPicker::HiddenKingTrooperColor);
		MyColorPicker("Health Status Bar", ColorPicker::HiddenKingHealthStatusBarColor);
		MyColorPicker("Souls Status Bar", ColorPicker::HiddenKingSoulsStatusBarColor);
	}

	// Archmother Team Colors
	if (ImGui::CollapsingHeader("Archmother Team", ImGuiTreeNodeFlags_DefaultOpen))
	{
		MyColorPicker("Name Tag", ColorPicker::ArchmotherNameTagColor);
		MyColorPicker("Bone", ColorPicker::ArchmotherBoneColor);
		MyColorPicker("Radar", ColorPicker::ArchmotherRadarColor);
		MyColorPicker("Trooper", ColorPicker::ArchmotherTrooperColor);
		MyColorPicker("Health Status Bar", ColorPicker::ArchmotherHealthStatusBarColor);
		MyColorPicker("Souls Status Bar", ColorPicker::ArchmotherSoulsStatusBarColor);
	}

	// Neutral/Shared Colors
	if (ImGui::CollapsingHeader("Neutral & Shared", ImGuiTreeNodeFlags_DefaultOpen))
	{
		MyColorPicker("Sinner's Sacrifice", ColorPicker::SinnersColor);
		MyColorPicker("Monster Camp", ColorPicker::MonsterCampColor);
		MyColorPicker("Unsecured Souls Text", ColorPicker::UnsecuredSoulsTextColor);
		MyColorPicker("Unsecured Souls Highlighted Text", ColorPicker::UnsecuredSoulsHighlightedTextColor);
		MyColorPicker("Health Bar Foreground", ColorPicker::HealthBarForegroundColor);
		MyColorPicker("Health Bar Background", ColorPicker::HealthBarBackgroundColor);
		MyColorPicker("Aimbot FOV Circle", ColorPicker::AimbotFOVCircle);
		MyColorPicker("Aimbot FOV Circle Active", ColorPicker::AimbotFOVCircleActive);
		MyColorPicker("Radar Background", ColorPicker::RadarBackgroundColor);
	}

	ImGui::End();
}

void ColorPicker::MyColorPicker(const char* label, ImColor& color)
{
	ImGui::SetNextItemWidth(150.0f);
	ImGui::ColorEdit4(label, &color.Value.x);
}