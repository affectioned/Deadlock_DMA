#include "pch.h"
#include "Color Picker.h"
#include "GUI/Fuser/Visuals/Visuals.h"
#include "GUI/Settings/Settings.h"

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

	// Registry labels are prefixed for the global search ("Color: Boss"); inside
	// this tab the prefix is noise.
	const char* StripPrefix(const char* label)
	{
		const char* colon = std::strstr(label, ": ");
		return colon ? colon + 2 : label;
	}
}

void ColorPicker::Render()
{
	static char sFilter[64] = {};

	const float resetWidth = 130.0f;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputTextWithHint("##colorfilter", "Filter colors...", sFilter, sizeof(sFilter));

	ImGui::SameLine();
	if (ImGui::Button("Deadlock Palette", ImVec2(resetWidth, 0.0f)))
		Settings::ResetPrefix("ColorPicker.");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Restore every color to the Deadlock default palette");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Driven off the settings registry rather than a hand-written list of rows.
	// The old version enumerated its sections by hand and had silently fallen
	// behind: both box colors, five powerup colors, the camp color, the radar
	// local-player color and both team colors were declared but had no row here.
	int shown = 0;
	for (const Setting& s : Settings::All())
	{
		if (s.type != SettingType::Color) continue;

		const char* label = StripPrefix(s.label);
		if (!MatchFilter(label, sFilter)) continue;

		Settings::RenderWidget(s, label);
		++shown;
	}

	if (shown == 0)
		ImGui::TextDisabled("No colors match \"%s\"", sFilter);
}

void ColorPicker::MyColorPicker(const char* label, ImColor& color)
{
	ImGui::SetNextItemWidth(150.0f);
	ImGui::ColorEdit4(label, &color.Value.x);
}
