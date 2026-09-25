#include "pch.h"

#include "XpOrbs.h"

#include "Deadlock/Deadlock.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"

void Draw_XpOrbs::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& orb : snap.xpOrbs)
	{
		if (orb.IsInvalid()) continue;
		if (orb.IsDormant()) continue;

		const WorldText::Falloff falloff = WorldText::Compute(snap.DistanceMeters(orb));
		if (falloff.alpha <= 0.0f) continue;

		ImVec2 p;
		if (!snap.Project(orb.m_Position, origin, p)) continue;

		dl->AddCircleFilled(p, 4.0f,
			WorldText::Fade(ImGui::ColorConvertFloat4ToU32(ColorPicker::XpOrbColor.Value), falloff.alpha));
	}
}
