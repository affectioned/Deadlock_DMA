#include "pch.h"

#include "Powerups.h"

#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"

namespace
{
	// Color-code by label so the four breakable variants (souls/health/powerup/
	// necro/crate) are visually distinct without needing per-type ImColor fields.
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
	std::scoped_lock Lock(EntityList::m_PowerupMutex);

	auto DrawList = ImGui::GetWindowDrawList();

	for (auto& P : EntityList::m_Powerups)
	{
		if (P.IsInvalid()) continue;

		if (P.IsDormant()) continue;

		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(P.m_Position, ScreenPos)) continue;

		ImU32 Color = PickColor(P.m_Label);

		DrawList->AddCircleFilled({ ScreenPos.x, ScreenPos.y }, fCircleRadius, Color);

		if (bShowLabel && P.m_Label)
		{
			std::string Text = P.m_Label;
			if (bShowDistance)
				Text = std::format("{} [{:.0f}m]", P.m_Label, P.DistanceFromLocalPlayer(true));

			auto TextSize = ImGui::CalcTextSize(Text.c_str());
			ImGui::SetCursorPos({ ScreenPos.x - (TextSize.x / 2.0f), ScreenPos.y + fCircleRadius + 2.0f });
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Color), "%s", Text.c_str());
		}
		else if (bShowDistance)
		{
			std::string Text = std::format("[{:.0f}m]", P.DistanceFromLocalPlayer(true));
			auto TextSize = ImGui::CalcTextSize(Text.c_str());
			ImGui::SetCursorPos({ ScreenPos.x - (TextSize.x / 2.0f), ScreenPos.y + fCircleRadius + 2.0f });
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Color), "%s", Text.c_str());
		}
	}
}
