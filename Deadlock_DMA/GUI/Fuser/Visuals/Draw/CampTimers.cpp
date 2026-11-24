#include "pch.h"

#include "CampTimers.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"
#include "GUI/Theme/Theme.h"

namespace
{
	// The game floats its own timer this far above the camp origin
	// (m_flNeutralCampRespawnTimerHeight); match it so ours sits on top.
	constexpr float kTagHeight = 100.0f;

	ImU32 U32(const ImColor& c) { return ImGui::ColorConvertFloat4ToU32(c.Value); }
}

std::string Draw_CampTimers::FormatCountdown(float seconds)
{
	const int s = static_cast<int>(std::ceil(std::max(seconds, 0.0f)));
	return s >= 60 ? std::format("{}:{:02}", s / 60, s % 60) : std::format("{}s", s);
}

void Draw_CampTimers::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();
	if (!snap.haveObjectiveTimers) return;

	if (bMidBossBanner)
		DrawMidBossBanner();

	if (!bWorld) return;

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& camp : snap.campTimers)
	{
		// Negative nextSpawnTime is "no timer"; it also yields remaining < 0.
		const float remaining = camp.nextSpawnTime - snap.matchClock;
		if (remaining <= 0.0f) continue; // camp is up

		const float distM = snap.haveLocalPosition
			? camp.origin.Distance(snap.localPosition) / HammerUnitsPerMeter
			: 0.0f;
		if (fMaxDistanceM > 0.0f && distM > fMaxDistanceM) continue;

		// Size falls off with distance like every other tag, but these are
		// objectives — never fade them out entirely, or the far ones the
		// feature exists for would vanish.
		const WorldText::Falloff falloff = WorldText::Compute(distM);
		const float alpha = std::max(falloff.alpha, 0.6f);

		ImVec2 anchor;
		const Vector3 tagPos{ camp.origin.x, camp.origin.y, camp.origin.z + kTagHeight };
		if (!snap.Project(tagPos, origin, anchor)) continue;

		const ImU32 col = WorldText::Fade(
			U32(camp.isMidBoss ? ColorPicker::MidBossTimerColor : ColorPicker::CampTimerColor), alpha);

		WorldText::Stack stack(dl, anchor, falloff.size);
		if (camp.isMidBoss)
			stack.Push("Mid Boss", col);
		stack.Push(FormatCountdown(remaining), col);
	}
}

void Draw_CampTimers::DrawMidBossBanner()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const float remaining = snap.nextMidBossSpawnTime - snap.matchClock;
	if (snap.nextMidBossSpawnTime <= 0.0f || remaining <= 0.0f) return;

	ImDrawList*  dl     = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetWindowPos();

	const ImVec2 anchor(origin.x + Fuser::m_ScreenSize.x * 0.5f, origin.y + Fuser::m_ScreenSize.y * 0.07f);
	WorldText::Draw(dl, anchor, std::format("MID BOSS  {}", FormatCountdown(remaining)),
		U32(ColorPicker::MidBossTimerColor), 18.0f);
}
