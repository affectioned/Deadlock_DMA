#include "pch.h"
#include "Color Picker.h"
#include "GUI/Fuser/Visuals/Visuals.h"

#include <cctype>
#include <cstring>

namespace
{
	// Case-insensitive substring match for the filter box.
	bool MatchFilter(const char* label, const char* needle)
	{
		if (!needle || !needle[0]) return true;
		size_t lh = std::strlen(label);
		size_t nh = std::strlen(needle);
		if (nh > lh) return false;
		for (size_t i = 0; i + nh <= lh; ++i)
		{
			bool ok = true;
			for (size_t j = 0; j < nh; ++j)
			{
				if (std::tolower((unsigned char)label[i + j]) != std::tolower((unsigned char)needle[j])) { ok = false; break; }
			}
			if (ok) return true;
		}
		return false;
	}

	// One compact row: swatch on the left (click to open picker), label on the right.
	// Returns true if the row was actually drawn (filter passed).
	bool Row(const char* label, ImColor& color, const char* filter)
	{
		if (!MatchFilter(label, filter)) return false;
		ImGui::PushID(label);
		ImGui::ColorEdit4("##swatch", &color.Value.x,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
		ImGui::PopID();
		return true;
	}

	// Section header that only draws if any row in the section would pass the filter.
	// Caller passes the list of labels in the section so we can probe ahead.
	bool SectionVisible(const char* filter, std::initializer_list<const char*> labels)
	{
		if (!filter || !filter[0]) return true;
		for (auto* lbl : labels) if (MatchFilter(lbl, filter)) return true;
		return false;
	}
}

void ColorPicker::Render()
{
	static char sFilter[64] = {};
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##colorfilter", "Filter colors...", sFilter, sizeof(sFilter));

	ImGui::Spacing();

	if (SectionVisible(sFilter, { "Menu Accent" }))
	{
		ImGui::SeparatorText("Theme");
		Row("Menu Accent", MenuAccent, sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Sinner's Sacrifice", "Boss", "XP Orb" }))
	{
		ImGui::SeparatorText("Entities");
		Row("Sinner's Sacrifice", SinnersColor, sFilter);
		Row("Boss",               BossColor,    sFilter);
		Row("XP Orb",              XpOrbColor,   sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Skeleton (Visible)", "Skeleton (Invisible)", "Unsecured Souls Text", "Unsecured Souls Highlighted Text" }))
	{
		ImGui::SeparatorText("Players");
		Row("Skeleton (Visible)",               SkeletonColorVisible,                sFilter);
		Row("Skeleton (Invisible)",             SkeletonColorInvisible,              sFilter);
		Row("Unsecured Souls Text",             UnsecuredSoulsTextColor,             sFilter);
		Row("Unsecured Souls Highlighted Text", UnsecuredSoulsHighlightedTextColor,  sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Friendly Health Status Bar", "Enemy Health Status Bar", "Friendly Souls Status Bar", "Enemy Souls Status Bar" }))
	{
		ImGui::SeparatorText("Team Status Bars");
		Row("Friendly Health Status Bar", FriendlyHealthStatusBarColor, sFilter);
		Row("Enemy Health Status Bar",    EnemyHealthStatusBarColor,    sFilter);
		Row("Friendly Souls Status Bar",  FriendlySoulsStatusBarColor,  sFilter);
		Row("Enemy Souls Status Bar",     EnemySoulsStatusBarColor,     sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Health Bar Foreground", "Health Bar Background" }))
	{
		ImGui::SeparatorText("Visuals Health Bar");
		Row("Health Bar Foreground", HealthBarForegroundColor, sFilter);
		Row("Health Bar Background", HealthBarBackgroundColor, sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Aim Assist FOV Circle", "Aim Assist FOV Circle Active" }))
	{
		ImGui::SeparatorText("Aim Assist");
		Row("Aim Assist FOV Circle",        AimAssistFOVCircle,       sFilter);
		Row("Aim Assist FOV Circle Active", AimAssistFOVCircleActive, sFilter);
		ImGui::Spacing();
	}

	if (SectionVisible(sFilter, { "Radar Background" }))
	{
		ImGui::SeparatorText("Radar");
		Row("Radar Background", RadarBackgroundColor, sFilter);
	}
}

void ColorPicker::MyColorPicker(const char* label, ImColor& color)
{
	ImGui::SetNextItemWidth(150.0f);
	ImGui::ColorEdit4(label, &color.Value.x);
}
