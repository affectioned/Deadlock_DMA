#include "pch.h"
#include "Radar.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"
#include "GUI/Theme/Theme.h"
#include "GUI/Utils/ImageLoading.h"
#include "Deadlock/Const/ETeam.h"
#include <numbers>

void Radar::Render()
{
	if (!bMasterToggle) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(ColorPicker::RadarBackgroundColor));

	ImGui::Begin("Radar", nullptr, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

	DrawRadarBackground();

	DrawEntities();

	ImGui::End();

	ImGui::PopStyleColor(1);
}

void Radar::RenderSettings()
{
	ImGui::Checkbox("Enable Radar", &bMasterToggle);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::SetNextItemWidth(150.0f);
	ImGui::SliderFloat("Radar Scale", &fRadarScale, 1.0f, 50.0f, "%.1f");
	ImGui::Checkbox("Hide Friendly", &bHideFriendly);
	ImGui::Checkbox("MOBA Style", &bMobaStyle);
	ImGui::Checkbox("Player-Centered", &bPlayerCentered);
}

ImVec2 Radar::GetLocalPlayerScreenPos(const ImVec2& RadarWindowCenter, const Vector3& RadarCenterGamePos) {
	if (bPlayerCentered) {
		return RadarWindowCenter;
	}
	else {
		const FrameSnapshot& snap = Snapshot::Current();
		const Vector3 LocalPlayerPos = snap.localPosition;

		ImVec2 Delta = { LocalPlayerPos.x - RadarCenterGamePos.x, LocalPlayerPos.y - RadarCenterGamePos.y };

		Delta.x /= fRadarScale;
		Delta.y /= fRadarScale;

		return ImVec2(RadarCenterGamePos.x + Delta.x, RadarCenterGamePos.y + Delta.y);
	}
}

Vector3 Radar::FindRadarTopLeftCoords(const Vector3& CenterRadarGamePosition, const ETeam& LocalTeam)
{
	auto RadarSizeGameUnits = GetRadarSizeInGameUnits();

	switch (LocalTeam) {
	case ETeam::ARCH_MOTHER:
		return Vector3{ CenterRadarGamePosition.x + (RadarSizeGameUnits.x / 2.0f), CenterRadarGamePosition.y - (RadarSizeGameUnits.y / 2.0f), 0.0f };
	case ETeam::HIDDEN_KING:
	default:
		return Vector3{ CenterRadarGamePosition.x - (RadarSizeGameUnits.x / 2.0f), CenterRadarGamePosition.y + (RadarSizeGameUnits.y / 2.0f), 0.0f };
	}
}

Vector3 Radar::FindRadarBottomRightCoords(const Vector3& CenterRadarGamePosition, const ETeam& LocalTeam)
{
	auto RadarSizeGameUnits = GetRadarSizeInGameUnits();

	switch (LocalTeam) {
	case ETeam::ARCH_MOTHER:
		return Vector3{ CenterRadarGamePosition.x - (RadarSizeGameUnits.x / 2.0f), CenterRadarGamePosition.y + (RadarSizeGameUnits.y / 2.0f), 0.0f };
	case ETeam::HIDDEN_KING:
	default:
		return Vector3{ CenterRadarGamePosition.x + (RadarSizeGameUnits.x / 2.0f), CenterRadarGamePosition.y - (RadarSizeGameUnits.y / 2.0f), 0.0f };
	}
}

ImVec2 Radar::GetRadarSizeInGameUnits()
{
	auto WindowSize = ImGui::GetWindowSize();
	return ImVec2(WindowSize.x * fRadarScale, WindowSize.y * fRadarScale);
}

// Takes the snapshot's precomputed view rather than calling Pawn.IsLocalPlayer(),
// which reaches into the live EntityList without holding its lock.
static ImColor GetRadarColor(const FrameSnapshot::PawnView& View) {
	if (View.localPlayer)
		return ColorPicker::LocalPlayerRadar;

	if (View.pawn->m_TeamNum == ETeam::HIDDEN_KING) {
		return ColorPicker::HiddenKingTeamColor;
	}
	else {
		return ColorPicker::ArchMotherTeamColor;
	}
}

void Radar::DrawEntities()
{
	const FrameSnapshot& snap = Snapshot::Current();

	auto DrawList = ImGui::GetWindowDrawList();
	auto WindowPos = ImGui::GetWindowPos();
	auto WindowSize = ImGui::GetWindowSize();

	const ImVec2 RadarWindowCenter = { WindowPos.x + (WindowSize.x / 2.0f), WindowPos.y + (WindowSize.y / 2.0f) };

	const ETeam LocalPlayerTeam = snap.haveLocalTeam ? snap.localTeam : ETeam::UNKNOWN;

	const auto RadarCenterGamePos = GetRadarCenterScreenPos();

	// No locks: pawns, controllers and the pawn↔controller join all come from the
	// frame snapshot, which also means no linear GetAssociatedPC scan per entity.
	for (const auto& View : snap.players)
	{
		const C_CitadelPlayerPawn& Pawn = *View.pawn;

		if (Pawn.IsDormant())
			continue;

		const Vector3 RawRelativePos = { Pawn.m_Position.x - RadarCenterGamePos.x, Pawn.m_Position.y - RadarCenterGamePos.y, Pawn.m_Position.z - RadarCenterGamePos.z };

		const auto GetFinalScreenPos = [](const Vector3& RawRelativePos, const ImVec2& RadarWindowCenter, float fRadarScale, const ETeam& LocalTeam) -> ImVec2 {
			switch (LocalTeam) {
			case ETeam::ARCH_MOTHER:
				return { RadarWindowCenter.x - (RawRelativePos.x / fRadarScale), RadarWindowCenter.y + (RawRelativePos.y / fRadarScale) };
			case ETeam::HIDDEN_KING:
			default:
				return { RadarWindowCenter.x + (RawRelativePos.x / fRadarScale), RadarWindowCenter.y - (RawRelativePos.y / fRadarScale) };
			}
			};

		const ImVec2 FinalScreenPos = GetFinalScreenPos(RawRelativePos, RadarWindowCenter, fRadarScale, LocalPlayerTeam);

		if (View.localPlayer) {
			DrawLocalPlayerViewRay(DrawList, FinalScreenPos, LocalPlayerTeam);
		}

		if (bHideFriendly && View.friendly) continue;

		DrawList->AddCircleFilled(FinalScreenPos, 5.0f, GetRadarColor(View));

		DrawPlayer(View, FinalScreenPos);
	}
}

void Radar::DrawLocalPlayerViewRay(ImDrawList* DrawList, const ImVec2& ScreenPos, const ETeam& LocalTeam)
{
	auto Rad = Deadlock::GetClientYaw() + std::numbers::pi_v<float>;

	ImVec2 LineEnd = { ScreenPos.x - (fRaySize * std::sin(Rad)), ScreenPos.y - (fRaySize * std::cos(Rad)) };

	if (LocalTeam == ETeam::HIDDEN_KING) {
		LineEnd = { ScreenPos.x + (fRaySize * std::sin(Rad)), ScreenPos.y + (fRaySize * std::cos(Rad)) };
	}

	DrawList->AddLine(ScreenPos, LineEnd, ImGui::ColorConvertFloat4ToU32(ColorPicker::LocalPlayerRadar.Value), 2.0f);
}

void Radar::DrawPlayer(const FrameSnapshot::PawnView& View, const ImVec2& RadarPos)
{
	int LineNumber = 0;
	auto DrawList = ImGui::GetWindowDrawList();
	DrawNameTag(View, DrawList, RadarPos, LineNumber);

	if (bMobaStyle) {
		DrawHealthBar(*View.controller, DrawList, RadarPos, LineNumber);
	}
}

void Radar::DrawNameTag(const FrameSnapshot::PawnView& View, ImDrawList* DrawList, const ImVec2& AnchorPos, int& LineNumber) {
	const std::string_view Name = View.controller->GetHeroName();
	const float LineH = ImGui::GetTextLineHeight();

	// Outlined: radar labels sit on top of the map texture, where plain text in a
	// team color is close to unreadable over light terrain.
	WorldText::Draw(DrawList, ImVec2(AnchorPos.x, AnchorPos.y + LineNumber * LineH),
		Name, GetRadarColor(View));

	LineNumber++;
}

void Radar::DrawHealthBar(const CCitadelPlayerController& PC, ImDrawList* DrawList, const ImVec2& AnchorPos, int& LineNumber)
{
	constexpr float HealthBarWidth = 80.0f;
	constexpr float Padding = 2.0f;
	constexpr float UnpaddedWidth = HealthBarWidth - (Padding * 2.0f);

	const float LineH = ImGui::GetTextLineHeight();

	const float maxHp = (PC.m_MaxHealth > 0) ? static_cast<float>(PC.m_MaxHealth) : 1.0f;
	const float hpPct = std::clamp(static_cast<float>(PC.m_CurrentHealth) / maxHp, 0.0f, 1.0f);

	// Bar rect under the anchor, centered
	ImVec2 barTL = { AnchorPos.x - (HealthBarWidth * 0.5f), AnchorPos.y + (LineNumber * LineH) };
	ImVec2 barBR = { barTL.x + HealthBarWidth,             barTL.y + LineH };

	DrawList->AddRectFilled(barTL, barBR, ColorPicker::HealthBarBackgroundColor);
	DrawList->AddRect(barTL, barBR, Theme::BrassDim, 0.0f, 0, 1.0f);

	// Inner (filled) portion, gradient-colored to match the world health bars.
	ImVec2 fillTL = { barTL.x + Padding, barTL.y + Padding };
	ImVec2 fillBR = { fillTL.x + (UnpaddedWidth * hpPct), barBR.y - Padding };
	const ImU32 fill = hpPct > 0.5f
		? Theme::Mix(Theme::Amber, Theme::SoulGreen, (hpPct - 0.5f) * 2.0f)
		: Theme::Mix(Theme::Danger, Theme::Amber, hpPct * 2.0f);
	DrawList->AddRectFilled(fillTL, fillBR, fill);

	WorldText::Draw(DrawList, ImVec2(AnchorPos.x, barTL.y + Padding),
		std::format("{}", PC.m_CurrentHealth), Theme::Cream, LineH - Padding);

	LineNumber++;
}

void Radar::DrawRadarBackground() {
	static CTextureInfo RadarBackgroundTexture{};

	if (!RadarBackgroundTexture.pTexture) {
		auto Ret = LoadTextureFromFile("Resources/Map.png");
		if (Ret) {
			RadarBackgroundTexture = *Ret;
		}
	}

	if (!RadarBackgroundTexture.pTexture) return;

	auto DrawList = ImGui::GetWindowDrawList();
	auto WindowPos = ImGui::GetWindowPos();
	auto WindowSize = ImGui::GetWindowSize();

	Vector3 CenterRadarGamePosition = GetRadarCenterScreenPos();

	const FrameSnapshot& snap = Snapshot::Current();
	const ETeam LocalTeam = snap.haveLocalTeam ? snap.localTeam : ETeam::UNKNOWN;

	auto TopLeftGameCoords = FindRadarTopLeftCoords(CenterRadarGamePosition, LocalTeam);
	auto BottomRightGameCoords = FindRadarBottomRightCoords(CenterRadarGamePosition, LocalTeam);

	auto UV_TL = GetUVFromCoords(TopLeftGameCoords, LocalTeam);
	auto UV_BR = GetUVFromCoords(BottomRightGameCoords, LocalTeam);

	ImGui::SetCursorPos({ 0.0f, 0.0f });
	ImGui::Image(RadarBackgroundTexture.pTexture, WindowSize, UV_TL, UV_BR);
	ImGui::GetWindowDrawList()->AddRectFilled(WindowPos, { WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y }, IM_COL32(0x2C, 0x24, 0x1B, 90));
}

Vector3 Radar::GetRadarCenterScreenPos()
{
	static const Vector3 DefaultCenter{ 0.0f, 0.0f, 0.0f };

	if (bPlayerCentered) {
		// From the snapshot, so the radar centre moves with the same extrapolated
		// origin the world ESP uses instead of lagging a poll behind it.
		return Snapshot::Current().localPosition;
	}

	return DefaultCenter;
}

ImVec2 Radar::GetUVFromCoords(const Vector3& GameCoords, ETeam LocalTeam)
{
	/* In range of (0,MapSize) */
	ImVec2 NormalizedCoords = { GameCoords.x + (MapSize.x / 2.0f), GameCoords.y + (MapSize.y / 2.0f) };

	ImVec2 UV = { NormalizedCoords.x / MapSize.x, 1.0f - (NormalizedCoords.y / MapSize.y) };

	return UV;
}