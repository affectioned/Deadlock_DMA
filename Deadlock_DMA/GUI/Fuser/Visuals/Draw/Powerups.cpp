#include "pch.h"

#include "Powerups.h"

#include "Deadlock/Deadlock.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"

namespace
{
	// Color-code by label so the breakable variants are visually distinct without
	// needing per-type ImColor fields.
	ImU32 PickColor(const char* Label)
	{
		if (!Label) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupColor.Value);
		if (std::strcmp(Label, "Souls")   == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupSoulsColor.Value);
		if (std::strcmp(Label, "Health")  == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupHealthColor.Value);
		if (std::strcmp(Label, "Necro")   == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupNecroColor.Value);
		if (std::strcmp(Label, "Idol")    == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupIdolColor.Value);
		if (std::strcmp(Label, "Rejuv")   == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupRejuvColor.Value);
		if (std::strcmp(Label, "XP")      == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupXpColor.Value);
		if (std::strcmp(Label, "Item")    == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupItemColor.Value);
		if (std::strcmp(Label, "Panel")   == 0) return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupPanelColor.Value);
		return ImGui::ColorConvertFloat4ToU32(ColorPicker::PowerupColor.Value);
	}
}

void Draw_Powerups::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& P : snap.powerups)
	{
		if (P.IsInvalid()) continue;
		if (P.IsDormant()) continue;

		const float distance = snap.DistanceMeters(P);
		const WorldText::Falloff falloff = WorldText::Compute(distance);
		if (falloff.alpha <= 0.0f) continue;

		ImVec2 p;
		if (!snap.Project(P.m_Position, origin, p)) continue;

		const ImU32 col = WorldText::Fade(PickColor(P.m_Label), falloff.alpha);

		dl->AddCircleFilled(p, fCircleRadius, col);

		std::string text;
		if (bShowLabel && P.m_Label)
			text = bShowDistance ? std::format("{} [{:.0f}m]", P.m_Label, distance) : P.m_Label;
		else if (bShowDistance)
			text = std::format("[{:.0f}m]", distance);

		if (!text.empty())
			WorldText::Draw(dl, ImVec2(p.x, p.y + fCircleRadius + 2.0f), text, col, falloff.size);
	}
}
