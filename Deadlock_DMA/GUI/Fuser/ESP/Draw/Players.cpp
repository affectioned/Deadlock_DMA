#include "pch.h"
#include "Players.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Deadlock/Const/HeroEnum.hpp"
#include "Deadlock/Const/BoneLists.hpp"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fonts/Fonts.h"

void Draw_Players::operator()()
{
	std::scoped_lock lock(EntityList::m_PawnMutex, EntityList::m_ControllerMutex);

	auto WindowPos = ImGui::GetWindowPos();
	auto DrawList = ImGui::GetWindowDrawList();

	for (auto& Pawn : EntityList::m_PlayerPawns)
	{
		if (Pawn.IsInvalid()) continue;

		auto AssociatedControllerAddr = EntityList::GetEntityAddressFromHandle(Pawn.m_hController);

		if (!AssociatedControllerAddr) continue;

		if (bHideLocalPlayer && Pawn.IsLocalPlayer()) continue;

		auto ControllerIt = std::find(EntityList::m_PlayerControllers.begin(), EntityList::m_PlayerControllers.end(), AssociatedControllerAddr);

		if (ControllerIt == EntityList::m_PlayerControllers.end())
			continue;

		if (ControllerIt->IsInvalid()) continue;

		if (ControllerIt->IsDead()) continue;

		// FOW gate: when enabled, skip enemies the team minimap doesn't see.
		// Friendlies always render — bypass the gate for them.
		if (bVisibleOnly && !ControllerIt->IsFriendly() && !EntityList::IsEntityVisible(Pawn.m_EntityAddress))
			continue;

		DrawPlayer(*ControllerIt, Pawn);
	}
}

void Draw_Players::DrawPlayer(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn)
{
	if (bHideFriendly && PC.IsFriendly())
		return;

	Vector2 ScreenPos{};
	if (!Deadlock::WorldToScreen(Pawn.m_Position, ScreenPos)) return;

	auto DrawList = ImGui::GetWindowDrawList();
	auto WindowPos = ImGui::GetWindowPos();

	if (bDrawBox)
		DrawBox(PC, Pawn, DrawList, WindowPos);

	if (bDrawBones)
		DrawSkeleton(PC, Pawn, DrawList, WindowPos);

	if (bDrawHead)
		DrawHeadCircle(PC, Pawn, DrawList, WindowPos);

	if (bDrawVelocityVector)
		DrawVelocityVector(Pawn, DrawList, WindowPos);

	if (bBoneNumbers)
		DrawBoneNumbers(Pawn);

	int LineNumber = 0;

	DrawNameTag(PC, Pawn, DrawList, WindowPos, LineNumber);

	if (bDrawHealthBar)
		DrawHealthBar(PC, Pawn, ImVec2(ScreenPos.x + WindowPos.x, ScreenPos.y + WindowPos.y), DrawList, LineNumber);
}

void Draw_Players::DrawHealthBar(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn, const ImVec2& PawnScreenPos, ImDrawList* DrawList, int& LineNumber)
{
	constexpr float HorizontalWidth = 80.0f;
	constexpr float VerticalThickness = 8.0f;
	constexpr float SideGap = 4.0f;
	constexpr float Padding = 2.0f;

	const float TextLineHeight = ImGui::GetTextLineHeight();
	const float HealthPercent  = PC.m_MaxHealth > 0
		? static_cast<float>(PC.m_CurrentHealth) / static_cast<float>(PC.m_MaxHealth)
		: 0.0f;

	const bool bVertical = (eHealthBarPosition == EHealthBarPosition::Left ||
	                        eHealthBarPosition == EHealthBarPosition::Right);

	// For vertical layouts the bar height tracks the on-screen player box
	// (head bone → feet). Falls back to Bottom if bones aren't ready yet.
	float BoxTopY = 0.0f, BoxBottomY = 0.0f, BoxHalfWidth = 0.0f;
	bool bHaveBoxBounds = false;
	if (bVertical && Pawn.m_pBoneData)
	{
		const auto& headSlot = Pawn.m_pBoneData->slotBones[static_cast<int>(HitboxSlot::Head)];
		if (!headSlot.empty())
		{
			Vector2 Head2D, Feet2D;
			if (Deadlock::WorldToScreen(Pawn.m_BonePositions[headSlot[0]], Head2D) &&
			    Deadlock::WorldToScreen(Pawn.m_Position, Feet2D))
			{
				const ImVec2 WindowPos = ImGui::GetWindowPos();
				float topY    = Head2D.y + WindowPos.y;
				float bottomY = Feet2D.y + WindowPos.y;
				if (bottomY > topY)
				{
					float height = bottomY - topY;
					topY -= height * 0.08f; // match DrawBox: head bone sits inside the skull
					BoxTopY      = topY;
					BoxBottomY   = bottomY;
					BoxHalfWidth = (bottomY - topY) * 0.25f;
					bHaveBoxBounds = true;
				}
			}
		}
	}

	const EHealthBarPosition Pos = (bVertical && !bHaveBoxBounds)
		? EHealthBarPosition::Bottom
		: eHealthBarPosition;

	ImVec2 BarTopLeft, BarBottomRight;
	switch (Pos)
	{
	case EHealthBarPosition::Top:
	{
		const float TopY = bHaveBoxBounds ? BoxTopY : PawnScreenPos.y - TextLineHeight - SideGap;
		BarTopLeft     = ImVec2(PawnScreenPos.x - HorizontalWidth * 0.5f, TopY - TextLineHeight - SideGap);
		BarBottomRight = ImVec2(BarTopLeft.x + HorizontalWidth, BarTopLeft.y + TextLineHeight);
		break;
	}
	case EHealthBarPosition::Bottom:
	{
		BarTopLeft     = ImVec2(PawnScreenPos.x - HorizontalWidth * 0.5f, PawnScreenPos.y + LineNumber * TextLineHeight);
		BarBottomRight = ImVec2(BarTopLeft.x + HorizontalWidth, BarTopLeft.y + TextLineHeight);
		break;
	}
	case EHealthBarPosition::Left:
	{
		const float CenterX = PawnScreenPos.x;
		BarTopLeft     = ImVec2(CenterX - BoxHalfWidth - SideGap - VerticalThickness, BoxTopY);
		BarBottomRight = ImVec2(BarTopLeft.x + VerticalThickness, BoxBottomY);
		break;
	}
	case EHealthBarPosition::Right:
	{
		const float CenterX = PawnScreenPos.x;
		BarTopLeft     = ImVec2(CenterX + BoxHalfWidth + SideGap, BoxTopY);
		BarBottomRight = ImVec2(BarTopLeft.x + VerticalThickness, BoxBottomY);
		break;
	}
	}

	// Background frame.
	DrawList->AddRectFilled(BarTopLeft, BarBottomRight, ColorPicker::HealthBarBackgroundColor);

	// Foreground fill: horizontal grows left→right; vertical grows bottom→top
	// (depleting health drops the level like a thermometer).
	const ImVec2 InnerTL(BarTopLeft.x + Padding,     BarTopLeft.y + Padding);
	const ImVec2 InnerBR(BarBottomRight.x - Padding, BarBottomRight.y - Padding);
	const float InnerW = InnerBR.x - InnerTL.x;
	const float InnerH = InnerBR.y - InnerTL.y;

	if (Pos == EHealthBarPosition::Left || Pos == EHealthBarPosition::Right)
	{
		const float FillH = InnerH * HealthPercent;
		DrawList->AddRectFilled(ImVec2(InnerTL.x, InnerBR.y - FillH), InnerBR,
		                        ColorPicker::HealthBarForegroundColor);
	}
	else
	{
		DrawList->AddRectFilled(InnerTL, ImVec2(InnerTL.x + InnerW * HealthPercent, InnerBR.y),
		                        ColorPicker::HealthBarForegroundColor);
	}

	// Numeric label: centered inside horizontal bars; floated above for verticals
	// since the column is too narrow for legible text.
	std::string HealthText = std::format("{0:d}", PC.m_CurrentHealth);
	const float LabelHeight = TextLineHeight - Padding;
	ImGui::PushFont(nullptr, LabelHeight);
	const ImVec2 TextSize = ImGui::CalcTextSize(HealthText.c_str());

	ImVec2 TextPos;
	if (Pos == EHealthBarPosition::Left || Pos == EHealthBarPosition::Right)
		TextPos = ImVec2(BarTopLeft.x + (VerticalThickness - TextSize.x) * 0.5f,
		                 BarTopLeft.y - TextSize.y - 1.0f);
	else
		TextPos = ImVec2((BarTopLeft.x + BarBottomRight.x - TextSize.x) * 0.5f,
		                 BarTopLeft.y + Padding);

	DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), HealthText.c_str());
	ImGui::PopFont();

	if (Pos == EHealthBarPosition::Bottom)
		LineNumber++;
}

void Draw_Players::DrawBox(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn, ImDrawList* DrawList, const ImVec2& WindowPos)
{
	// Vertical extent: head bone (top) → pawn origin (feet). Width derived
	// from the on-screen height — keeps the box proportional regardless of
	// distance / FOV. Falls back gracefully when the head bone is missing.
	if (!Pawn.m_pBoneData) return;
	const auto& headSlot = Pawn.m_pBoneData->slotBones[static_cast<int>(HitboxSlot::Head)];
	if (headSlot.empty()) return;
	int HeadBoneIndex = headSlot[0];

	Vector2 Head2D, Feet2D;
	if (!Deadlock::WorldToScreen(Pawn.m_BonePositions[HeadBoneIndex], Head2D)) return;
	if (!Deadlock::WorldToScreen(Pawn.m_Position, Feet2D)) return;

	float topY = Head2D.y + WindowPos.y;
	float bottomY = Feet2D.y + WindowPos.y;
	if (bottomY <= topY) return; // pawn upside-down on screen — bail

	// Head bone sits inside the skull, not at the crown — push the top up so
	// the box covers the visible head instead of cutting through it.
	float height = bottomY - topY;
	topY -= height * 0.08f;
	height = bottomY - topY;

	float halfWidth = height * 0.25f; // typical humanoid aspect, ~1:2
	float centerX = ((Head2D.x + Feet2D.x) * 0.5f) + WindowPos.x;

	auto BoxColor = PC.m_TeamNum == ETeam::HIDDEN_KING
		? ColorPicker::HiddenKingTeamColor
		: ColorPicker::ArchMotherTeamColor;

	DrawList->AddRect(
		ImVec2(centerX - halfWidth, topY),
		ImVec2(centerX + halfWidth, bottomY),
		BoxColor,
		0.0f, 0, fBoxThickness);
}

void Draw_Players::DrawSkeleton(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn, ImDrawList* DrawList, const ImVec2& WindowPos)
{
	if (!Pawn.m_pBoneData || Pawn.m_pBoneData->pairs.empty()) return;

	for (const auto& [StartBone, EndBone] : Pawn.m_pBoneData->pairs)
	{
		if (StartBone >= MAX_BONES || EndBone >= MAX_BONES) continue;

		Vector2 Start2D, End2D;

		if (!Deadlock::WorldToScreen(Pawn.m_BonePositions[StartBone], Start2D)) continue;
		if (!Deadlock::WorldToScreen(Pawn.m_BonePositions[EndBone], End2D)) continue;

		ImVec2 Start = ImVec2(Start2D.x + WindowPos.x, Start2D.y + WindowPos.y);
		ImVec2 End = ImVec2(End2D.x + WindowPos.x, End2D.y + WindowPos.y);
		DrawList->AddLine(Start, End, ColorPicker::SkeletonColor, fBonesThickness);
	}
}

void Draw_Players::DrawHeadCircle(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn, ImDrawList* DrawList, const ImVec2& WindowPos)
{
	if (!Pawn.m_pBoneData) return;
	const auto& headSlot = Pawn.m_pBoneData->slotBones[static_cast<int>(HitboxSlot::Head)];
	if (headSlot.empty()) return;
	int HeadBoneIndex = headSlot[0];

	Vector2 Head2D;
	if (!Deadlock::WorldToScreen(Pawn.m_BonePositions[HeadBoneIndex], Head2D)) return;

	ImVec2 HeadPos = ImVec2(Head2D.x + WindowPos.x, Head2D.y + WindowPos.y);

	float Distance = Pawn.DistanceFromLocalPlayer(false);
	if (Distance < 0.1f) return;

	float BaseRadius = 5.f;

	// Scale radius inversely with distance
	float DistanceScale = 1000.f / (Distance + 100.f);
	float HeadRadius = BaseRadius * DistanceScale;

	DrawList->AddCircle(HeadPos, HeadRadius, ColorPicker::SkeletonColor, 32, fBonesThickness);
}

void Draw_Players::DrawVelocityVector(const C_CitadelPlayerPawn& Pawn, ImDrawList* DrawList, const ImVec2& WindowPos)
{
	Vector3 FuturePosition = Pawn.m_Position + Pawn.m_Velocity;
	Vector2 Start2D, End2D;
	if (!Deadlock::WorldToScreen(Pawn.m_Position, Start2D)) return;
	if (!Deadlock::WorldToScreen(FuturePosition, End2D)) return;
	ImVec2 Start = ImVec2(Start2D.x + WindowPos.x, Start2D.y + WindowPos.y);
	ImVec2 End = ImVec2(End2D.x + WindowPos.x, End2D.y + WindowPos.y);
	DrawList->AddLine(Start, End, ImColor(255, 255, 255), 5.0f);
}

void Draw_Players::DrawBoneNumbers(const C_CitadelPlayerPawn& Pawn)
{
	ImGui::PushFont(nullptr, 12.0f);

	for (int i = 0; i < MAX_BONES; i++)
	{
		Vector2 ScreenPos{};
		if (!Deadlock::WorldToScreen(Pawn.m_BonePositions[i], ScreenPos)) continue;

		std::string BoneString = std::to_string(i);
		auto TextSize = ImGui::CalcTextSize(BoneString.c_str());

		ImGui::SetCursorPos(ImVec2(ScreenPos.x - (TextSize.x / 2.0f), ScreenPos.y));
		ImGui::Text(BoneString.c_str());
	}

	ImGui::PopFont();
}

void Draw_Players::DrawNameTag(const CCitadelPlayerController& PC, const C_CitadelPlayerPawn& Pawn, ImDrawList* DrawList, const ImVec2& WindowPos, int& LineNumber)
{
	Vector2 ScreenPos{};
	if (!Deadlock::WorldToScreen(Pawn.m_Position, ScreenPos)) return;

	std::string NameTagString{};

	NameTagString += std::format("{0:s} ", PC.GetHeroName());

	if (bShowDistance)
		NameTagString += std::format("[{0:.0f}m] ", Pawn.DistanceFromLocalPlayer(true));

	if (NameTagString.back() == ' ') NameTagString.pop_back();

	auto TextSize = ImGui::CalcTextSize(NameTagString.c_str());

	auto NameTagColor = PC.m_TeamNum == ETeam::HIDDEN_KING ? ColorPicker::HiddenKingTeamColor : ColorPicker::ArchMotherTeamColor;

	ImGui::SetCursorPos(ImVec2(ScreenPos.x - (TextSize.x / 2.0f), ScreenPos.y + (LineNumber * TextSize.y)));
	ImGui::TextColored(NameTagColor, NameTagString.c_str());

	LineNumber++;

	if (bDrawUnsecuredSouls)
		DrawUnsecuredSouls(Pawn, ScreenPos, LineNumber);
}

void Draw_Players::DrawUnsecuredSouls(const C_CitadelPlayerPawn& Pawn, const Vector2& ScreenPos, int& LineNumber)
{
	if (Pawn.m_UnsecuredSouls < UnsecuredSoulsMinimumThreshold)
		return;

	ImColor UnsecuredSoulColor = ColorPicker::UnsecuredSoulsTextColor;
	std::string SoulText = std::format("{} ", Pawn.m_UnsecuredSouls);

	if (Pawn.m_UnsecuredSouls > UnsecuredSoulsHighlightThreshold)
	{
		UnsecuredSoulColor = ColorPicker::UnsecuredSoulsHighlightedTextColor;
		SoulText += "UNSECURED";
	}
	else
		SoulText += "Unsecured";

	auto SoulTextSize = ImGui::CalcTextSize(SoulText.c_str());
	ImGui::SetCursorPos(ImVec2(ScreenPos.x - (SoulTextSize.x / 2.0f), ScreenPos.y + (LineNumber * SoulTextSize.y)));
	ImGui::TextColored(UnsecuredSoulColor, SoulText.c_str());

	LineNumber++;
}
