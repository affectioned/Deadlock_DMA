#include "pch.h"

#include "Status Bars.h"

#include "Deadlock/Entity List/EntityList.h"

#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"
#include "GUI/Theme/Theme.h"

void StatusBars::RenderGenericComparisonBar(ValuePair Values, ColorPair Colors, int& LineNumber, ImDrawList* DrawList, const ImVec2& WindowPos, const ImVec2& WindowSize)
{
	constexpr float TotalWidth = 200.0f;
	constexpr float Padding = 2.0f;
	const ImU32 BackgroundColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.13f, 0.11f, 0.08f, 0.80f));

	ImVec2 CenterPos = ImVec2(WindowPos.x + (WindowSize.x * 0.5f), WindowPos.y + (WindowSize.y * 0.5f));

	/* Draw background */
	ImVec2 TopLeft = ImVec2(CenterPos.x - (TotalWidth * 0.5f), (BarHeight * LineNumber));
	ImVec2 BottomRight = ImVec2(TopLeft.x + TotalWidth, TopLeft.y + BarHeight);
	DrawList->AddRectFilled(TopLeft, BottomRight, BackgroundColor);

	constexpr float EffectiveWidth = TotalWidth - (Padding * 2.0f);
	TopLeft.x += Padding;
	TopLeft.y += Padding;
	BottomRight.x -= Padding;
	BottomRight.y -= Padding;
	DrawList->AddRectFilled(TopLeft, BottomRight, Colors.second);

	// Both sides are 0 in a lobby and before the first controller read lands;
	// the divide used to produce NaN and feed it straight into AddRectFilled.
	const uint32_t TotalValue = Values.first + Values.second;
	const float FirstValuePercentage = TotalValue > 0
		? static_cast<float>(Values.first) / static_cast<float>(TotalValue)
		: 0.5f;

	BottomRight.x = TopLeft.x + (EffectiveWidth * FirstValuePercentage);
	DrawList->AddRectFilled(TopLeft, BottomRight, Colors.first);

	WorldText::Draw(DrawList, ImVec2(CenterPos.x, TopLeft.y),
		std::format("{} / {}", Values.first, Values.second), Theme::Cream);

	LineNumber++;
}

void StatusBars::RenderTeamHealthBars(GameStatistics& Stats, const ImVec2& WindowPos, const ImVec2& WindowSize, ImDrawList* DrawList, int& LineNumber)
{

	RenderGenericComparisonBar(
		ValuePair(Stats.m_FriendlyTeamHealth, Stats.m_EnemyTeamHealth),
		ColorPair(ColorPicker::FriendlyHealthStatusBarColor, ColorPicker::EnemyHealthStatusBarColor),
		LineNumber,
		DrawList,
		WindowPos,
		WindowSize
	);
}

void StatusBars::RenderTeamSoulsBars(GameStatistics& Stats, const ImVec2& WindowPos, const ImVec2& WindowSize, ImDrawList* DrawList, int& LineNumber)
{
	RenderGenericComparisonBar(
		ValuePair(Stats.m_FriendlySoulsCollected, Stats.m_EnemySoulsCollected),
		ColorPair(ColorPicker::FriendlySoulsStatusBarColor, ColorPicker::EnemySoulsStatusBarColor),
		LineNumber,
		DrawList,
		WindowPos,
		WindowSize
	);
}

void StatusBars::RenderUnspentSoulsBar(GameStatistics& Stats, const ImVec2& WindowPos, const ImVec2& WindowSize, ImDrawList* DrawList, int& LineNumber)
{
	RenderGenericComparisonBar(
		ValuePair(Stats.m_FriendlyUnspentSouls, Stats.m_EnemyUnspentSouls),
		ColorPair(ColorPicker::FriendlySoulsStatusBarColor, ColorPicker::EnemySoulsStatusBarColor),
		LineNumber,
		DrawList,
		WindowPos,
		WindowSize
	);
}

void StatusBars::Render()
{
	auto WindowPos = ImGui::GetWindowPos();
	auto DrawList = ImGui::GetWindowDrawList();
	auto WindowSize = ImGui::GetWindowSize();

	GameStatistics Stats;

	int LineNumber = 3;

	if (bRenderTeamHealthBar)
		RenderTeamHealthBars(Stats, WindowPos, WindowSize, DrawList, LineNumber);

	if (bRenderTeamSoulsBar)
		RenderTeamSoulsBars(Stats, WindowPos, WindowSize, DrawList, LineNumber);

	if (bRenderUnspentSoulsBar)
		RenderUnspentSoulsBar(Stats, WindowPos, WindowSize, DrawList, LineNumber);
}

GameStatistics::GameStatistics()
{
	// The frame snapshot has already joined pawns to controllers and resolved
	// friendly/enemy, so this no longer needs either mutex or the linear
	// std::find over m_PlayerPawns it used to run once per controller.
	const FrameSnapshot& snap = Snapshot::Current();

	for (const auto& view : snap.players)
	{
		const CCitadelPlayerController& Controller = *view.controller;
		const C_CitadelPlayerPawn&      Pawn       = *view.pawn;

		if (view.friendly)
		{
			if (Controller.m_CurrentHealth > 0)
				m_FriendlyTeamHealth += Controller.m_CurrentHealth;
			m_FriendlySoulsCollected += Controller.m_TotalSouls;
			m_FriendlyUnspentSouls += Pawn.m_TotalUnspentSouls;
		}
		else
		{
			if (Controller.m_CurrentHealth > 0)
				m_EnemyTeamHealth += Controller.m_CurrentHealth;
			m_EnemySoulsCollected += Controller.m_TotalSouls;
			m_EnemyUnspentSouls += Pawn.m_TotalUnspentSouls;
		}
	}
}