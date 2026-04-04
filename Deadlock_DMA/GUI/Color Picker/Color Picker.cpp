#include "pch.h"
#include "Color Picker.h"
#include "GUI/Fuser/ESP/ESP.h"

void ColorPicker::RenderContent()
{
	MyColorPicker("Sinner's Sacrifice",                 SinnersColor);
	MyColorPicker("Monster Camp",                       MonsterCampColor);
	MyColorPicker("Unsecured Souls Text",               UnsecuredSoulsTextColor);
	MyColorPicker("Unsecured Souls Highlighted",        UnsecuredSoulsHighlightedTextColor);
	MyColorPicker("Friendly Health Status Bar",         FriendlyHealthStatusBarColor);
	MyColorPicker("Enemy Health Status Bar",            EnemyHealthStatusBarColor);
	MyColorPicker("Friendly Souls Status Bar",          FriendlySoulsStatusBarColor);
	MyColorPicker("Enemy Souls Status Bar",             EnemySoulsStatusBarColor);
	MyColorPicker("Health Bar Foreground",              HealthBarForegroundColor);
	MyColorPicker("Health Bar Background",              HealthBarBackgroundColor);
	MyColorPicker("Aimbot FOV Circle",                  AimbotFOVCircle);
	MyColorPicker("Aimbot FOV Circle Active",           AimbotFOVCircleActive);
	MyColorPicker("Radar Background",                   RadarBackgroundColor);
	MyColorPicker("Team: Hidden King",                  HiddenKingTeamColor);
	MyColorPicker("Team: Arch Mother",                  ArchMotherTeamColor);
	MyColorPicker("Skeleton",                           SkeletonColor);
}

void ColorPicker::Render()
{
	if (!bMasterToggle) return;
	ImGui::Begin("Color Picker", &bMasterToggle);
	RenderContent();
	ImGui::End();
}

void ColorPicker::MyColorPicker(const char* label, ImColor& color)
{
	ImGui::SetNextItemWidth(150.0f);
	ImGui::ColorEdit4(label, &color.Value.x);
}