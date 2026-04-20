#include "pch.h"

#include "XpOrbs.h"

#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"

void Draw_XpOrbs::operator()()
{
	std::scoped_lock Lock(EntityList::m_XpOrbMutex);

	auto DrawList = ImGui::GetWindowDrawList();

	for (auto& Orb : EntityList::m_XpOrbs)
	{
		if (Orb.IsInvalid()) continue;

		if (Orb.IsDormant()) continue;

		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(Orb.m_Position, ScreenPos)) continue;

		DrawList->AddCircleFilled(
			{ ScreenPos.x, ScreenPos.y },
			4.0f,
			ImGui::ColorConvertFloat4ToU32(ColorPicker::XpOrbColor.Value)
		);
	}
}
