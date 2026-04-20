#include "pch.h"

#include "Camps.h"

#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"

void Draw_Camps::operator()()
{
	std::scoped_lock Lock(EntityList::m_MonsterCampMutex);

	auto WindowPos = ImGui::GetWindowPos();
	auto DrawList = ImGui::GetWindowDrawList();

	for (auto& Camp : EntityList::m_MonsterCamps)
	{
		if (Camp.IsInvalid()) continue;

		if (Camp.IsDormant()) continue;

		if (Camp.m_CurrentHealth < 1) continue;

		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(Camp.m_Position, ScreenPos)) continue;

		float yOffset = ScreenPos.y;

		if (Camp.m_Label)
		{
			auto LabelSize = ImGui::CalcTextSize(Camp.m_Label);
			ImGui::SetCursorPos({ ScreenPos.x - (LabelSize.x / 2.0f), yOffset });
			ImGui::TextColored(ColorPicker::BossColor.Value, Camp.m_Label);
			yOffset += LabelSize.y;
		}

		std::string CampString = (Camp.m_MaxHealth > 0)
			? std::format("[{}/{}]", Camp.m_CurrentHealth, Camp.m_MaxHealth)
			: std::format("[{}]", Camp.m_CurrentHealth);
		auto TextSize = ImGui::CalcTextSize(CampString.c_str());
		ImGui::SetCursorPos({ ScreenPos.x - (TextSize.x / 2.0f), yOffset });
		ImGui::TextColored(ColorPicker::BossColor.Value, CampString.c_str());
	}
}