#include "pch.h"

#include "Troopers.h"

#include "Deadlock/Deadlock.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"

void Draw_Troopers::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& trooper : snap.troopers)
	{
		if (trooper.IsInvalid()) continue;
		if (trooper.IsDormant()) continue;

		const bool isWalker  = trooper.m_Label && std::strcmp(trooper.m_Label, "Walker")  == 0;
		const bool isNeutral = trooper.m_Label && std::strcmp(trooper.m_Label, "Neutral") == 0;
		const bool isLane    = !isWalker && !isNeutral;

		if (isLane    && !bDrawLaneTroopers) continue;
		if (isWalker  && !bDrawWalkers)      continue;
		if (isNeutral && !bDrawNeutrals)     continue;

		if (bHideFriendly && snap.IsFriendly(trooper))
			continue;

		const float distance = snap.DistanceMeters(trooper);
		const WorldText::Falloff falloff = WorldText::Compute(distance);
		if (falloff.alpha <= 0.0f) continue;

		ImVec2 anchor;
		if (!snap.Project(trooper.m_Position, origin, anchor)) continue;

		const ImU32 col = WorldText::Fade(
			ImGui::ColorConvertFloat4ToU32(trooper.m_TeamNum == ETeam::HIDDEN_KING
				? ColorPicker::HiddenKingTeamColor.Value
				: ColorPicker::ArchMotherTeamColor.Value),
			falloff.alpha);

		WorldText::Stack stack(dl, anchor, falloff.size);

		if (trooper.m_Label)
			stack.Push(trooper.m_Label, col);

		stack.Push(trooper.m_MaxHealth > 0
			? std::format("{}/{}", trooper.m_CurrentHealth, trooper.m_MaxHealth)
			: std::format("{}", trooper.m_CurrentHealth), col);
	}
}
