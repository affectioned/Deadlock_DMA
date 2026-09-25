#include "pch.h"

#include "Camps.h"

#include "Deadlock/Deadlock.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"

void Draw_Camps::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& camp : snap.camps)
	{
		if (camp.IsInvalid()) continue;
		if (camp.IsDormant()) continue;
		if (camp.m_CurrentHealth < 1) continue;

		const WorldText::Falloff falloff = WorldText::Compute(snap.DistanceMeters(camp));
		if (falloff.alpha <= 0.0f) continue;

		ImVec2 anchor;
		if (!snap.Project(camp.m_Position, origin, anchor)) continue;

		const ImU32 col = WorldText::Fade(
			ImGui::ColorConvertFloat4ToU32(ColorPicker::BossColor.Value), falloff.alpha);

		WorldText::Stack stack(dl, anchor, falloff.size);

		if (camp.m_Label)
			stack.Push(camp.m_Label, col);

		stack.Push(camp.m_MaxHealth > 0
			? std::format("[{}/{}]", camp.m_CurrentHealth, camp.m_MaxHealth)
			: std::format("[{}]", camp.m_CurrentHealth), col);
	}
}
