#include "pch.h"

#include "Sinners.h"

#include "Deadlock/Deadlock.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"

void Draw_Sinners::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& sinner : snap.sinners)
	{
		if (sinner.IsInvalid()) continue;
		if (sinner.IsDormant()) continue;

		const WorldText::Falloff falloff = WorldText::Compute(snap.DistanceMeters(sinner));
		if (falloff.alpha <= 0.0f) continue;

		ImVec2 anchor;
		if (!snap.Project(sinner.m_Position, origin, anchor)) continue;

		WorldText::Draw(dl, anchor, std::format("[{}]", sinner.m_CurrentHealth),
			WorldText::Fade(ImGui::ColorConvertFloat4ToU32(ColorPicker::SinnersColor.Value), falloff.alpha),
			falloff.size);
	}
}
