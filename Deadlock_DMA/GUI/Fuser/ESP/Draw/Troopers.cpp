#include "pch.h"

#include "Troopers.h"

#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"

void Draw_Troopers::operator()()
{
	std::scoped_lock Lock(EntityList::m_TrooperMutex, EntityList::m_ControllerMutex);

	auto WindowPos = ImGui::GetWindowPos();
	auto DrawList = ImGui::GetWindowDrawList();

	for (auto& Trooper : EntityList::m_Troopers)
	{
		if (Trooper.IsInvalid()) continue;

		if (Trooper.IsDormant()) continue;

		auto bIsFriend = Trooper.IsFriendly();

		if (bHideFriendly && bIsFriend)
			continue;

		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(Trooper.m_Position, ScreenPos)) continue;

		ImVec4 Color = (Trooper.m_TeamNum == ETeam::HIDDEN_KING) ? ColorPicker::HiddenKingTeamColor : ColorPicker::ArchMotherTeamColor;
		float yOffset = ScreenPos.y;

		if (Trooper.m_Label)
		{
			auto LabelSize = ImGui::CalcTextSize(Trooper.m_Label);
			ImGui::SetCursorPos({ ScreenPos.x - (LabelSize.x / 2.0f), yOffset });
			ImGui::TextColored(Color, Trooper.m_Label);
			yOffset += LabelSize.y;
		}

		std::string TrooperString = (Trooper.m_Label && Trooper.m_MaxHealth > 0)
			? std::format("{}/{}", Trooper.m_CurrentHealth, Trooper.m_MaxHealth)
			: std::format("{}", Trooper.m_CurrentHealth);
		auto TextSize = ImGui::CalcTextSize(TrooperString.c_str());
		ImGui::SetCursorPos({ ScreenPos.x - (TextSize.x / 2.0f), yOffset });
		ImGui::TextColored(Color, TrooperString.c_str());
	}
}