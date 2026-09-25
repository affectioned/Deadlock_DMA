#include "pch.h"
#include "Players.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Const/HeroEnum.hpp"
#include "Deadlock/Const/BoneLists.hpp"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fonts/Fonts.h"
#include "GUI/Theme/Theme.h"
#include "GUI/Fuser/Visuals/WorldText.h"

namespace
{
	ImU32 U32(const ImColor& c) { return ImGui::ColorConvertFloat4ToU32(c.Value); }

	// Health → color. Green through amber to red rather than a straight
	// green→red lerp, which passes through a muddy olive at 50%.
	ImU32 HealthColor(float pct)
	{
		pct = std::clamp(pct, 0.0f, 1.0f);
		return pct > 0.5f
			? Theme::Mix(Theme::Amber, Theme::SoulGreen, (pct - 0.5f) * 2.0f)
			: Theme::Mix(Theme::Danger, Theme::Amber, pct * 2.0f);
	}
}

void Draw_Players::operator()()
{
	const FrameSnapshot& snap = Snapshot::Current();

	const ImVec2 origin = ImGui::GetWindowPos();
	ImDrawList*  dl     = ImGui::GetWindowDrawList();

	for (const auto& view : snap.players)
	{
		if (bHideLocalPlayer && view.localPlayer) continue;
		if (bHideFriendly && view.friendly)       continue;

		// FOW gate: skip enemies the team minimap can't see. Friendlies are never
		// in the local team's FOW list, so PawnView pre-resolves them to visible.
		if (bVisibleOnly && !view.confirmedVisible) continue;

		const WorldText::Falloff falloff = WorldText::Compute(view.distanceMeters);
		if (falloff.alpha <= 0.0f) continue; // past the max-distance cull

		Ctx c{};
		c.snap     = &snap;
		c.view     = &view;
		c.dl       = dl;
		c.origin   = origin;
		c.textSize = falloff.size;
		c.alpha    = falloff.alpha;

		if (!snap.Project(view.pawn->m_Position, origin, c.feet))
			continue;

		if (view.controller->IsDead())
		{
			if (bShowRespawnTimer)
				DrawRespawnTimer(c);
			continue;
		}

		DrawPlayer(c);
	}
}

void Draw_Players::DrawPlayer(const Ctx& c)
{
	const Box box = ComputeBox(c);

	if (bDrawBox)
		DrawBox(c, box);

	if (bDrawBones)
		DrawSkeleton(c);

	if (bDrawHead)
		DrawHeadCircle(c);

	if (bDrawVelocityVector)
		DrawVelocityVector(c);

	if (bBoneNumbers)
		DrawBoneNumbers(c);

	const float textBottomY = DrawNameTag(c);

	if (bDrawHealthBar)
		DrawHealthBar(c, box, textBottomY);
}

Draw_Players::Box Draw_Players::ComputeBox(const Ctx& c)
{
	Box box{ 0.0f, 0.0f, 0.0f, 0.0f, false };

	const C_CitadelPlayerPawn& pawn = c.Pawn();
	if (!pawn.m_pBoneData) return box;

	// Preferred path: the projected extent of the bones we already trust enough
	// to render as the skeleton. Every one of those indices has live data, which
	// the raw 0..m_BoneCount range does not guarantee.
	if (bBoxFromBones && !pawn.m_pBoneData->pairs.empty())
	{
		float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
		int   hits = 0;

		auto accumulate = [&](int boneIndex)
		{
			if (boneIndex < 0 || boneIndex >= MAX_BONES) return;
			ImVec2 p;
			if (!c.snap->Project(pawn.m_BonePositions[boneIndex], c.origin, p)) return;
			minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
			minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
			++hits;
		};

		for (const auto& [start, end] : pawn.m_pBoneData->pairs)
		{
			accumulate(start);
			accumulate(end);
		}

		// A couple of stray bones projecting on-screen isn't a box — fall through
		// to the head/feet heuristic rather than drawing a sliver.
		if (hits >= 6 && maxY > minY)
		{
			// Bones are the skeleton centerline; the silhouette is wider and taller
			// than they are. Pad proportionally to on-screen height so the margin
			// holds at every distance.
			const float height = maxY - minY;
			const float padX   = height * 0.07f;
			const float padY   = height * 0.04f;
			box = { minX - padX, minY - padY, maxX + padX, maxY + padY, true };
			return box;
		}
	}

	// Fallback: head bone → pawn origin, width from on-screen height.
	const auto& headSlot = pawn.m_pBoneData->slotBones[static_cast<int>(HitboxSlot::Head)];
	if (headSlot.empty()) return box;

	ImVec2 head;
	if (!c.snap->Project(pawn.m_BonePositions[headSlot[0]], c.origin, head)) return box;

	float topY    = head.y;
	float bottomY = c.feet.y;
	if (bottomY <= topY) return box; // pawn upside-down on screen — bail

	// Head bone sits inside the skull, not at the crown.
	topY -= (bottomY - topY) * 0.08f;

	const float halfWidth = (bottomY - topY) * 0.25f; // typical humanoid aspect, ~1:2
	const float centerX   = (head.x + c.feet.x) * 0.5f;

	box = { centerX - halfWidth, topY, centerX + halfWidth, bottomY, true };
	return box;
}

void Draw_Players::DrawBox(const Ctx& c, const Box& box)
{
	if (!box.valid) return;

	const ImU32 base = c.view->visible
		? U32(ColorPicker::BoxColorVisible)
		: U32(ColorPicker::BoxColorInvisible);
	const ImU32 col = WorldText::Fade(base, c.alpha);

	const ImVec2 tl(box.left, box.top);
	const ImVec2 br(box.right, box.bottom);

	if (bBoxFill)
		c.dl->AddRectFilled(tl, br, WorldText::Fade(base, c.alpha * 0.12f));

	if (eBoxStyle == EBoxStyle::Full)
	{
		c.dl->AddRect(tl, br, col, 0.0f, 0, fBoxThickness);
		return;
	}

	// Corner brackets: arms are a quarter of the shorter side, so the shape stays
	// recognizable on a narrow distant box instead of closing into a full rect.
	const float w   = br.x - tl.x;
	const float h   = br.y - tl.y;
	const float arm = std::max(std::min(w, h) * 0.25f, 2.0f);

	const float xs[2] = { tl.x, br.x };
	const float ys[2] = { tl.y, br.y };
	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			const float dx = (i == 0) ? arm : -arm;
			const float dy = (j == 0) ? arm : -arm;
			const ImVec2 corner(xs[i], ys[j]);
			c.dl->AddLine(corner, ImVec2(corner.x + dx, corner.y), col, fBoxThickness);
			c.dl->AddLine(corner, ImVec2(corner.x, corner.y + dy), col, fBoxThickness);
		}
	}
}

void Draw_Players::DrawHealthBar(const Ctx& c, const Box& box, float textBottomY)
{
	constexpr float HorizontalWidth  = 80.0f;
	constexpr float VerticalThickness = 8.0f;
	constexpr float SideGap = 4.0f;
	constexpr float Padding = 2.0f;

	const CCitadelPlayerController& PC = c.PC();

	const float barHeight    = std::max(c.textSize, 6.0f);
	const float healthPercent = PC.m_MaxHealth > 0
		? std::clamp(static_cast<float>(PC.m_CurrentHealth) / static_cast<float>(PC.m_MaxHealth), 0.0f, 1.0f)
		: 0.0f;

	const bool wantsVertical = (eHealthBarPosition == EHealthBarPosition::Left ||
	                            eHealthBarPosition == EHealthBarPosition::Right);

	// Vertical layouts track the box; without a box there's nothing to track, so
	// they degrade to Bottom rather than drawing at a guessed height.
	const EHealthBarPosition pos = (wantsVertical && !box.valid)
		? EHealthBarPosition::Bottom
		: eHealthBarPosition;

	// Width scales with the box so a distant target's bar doesn't dwarf it.
	const float horizontalWidth = box.valid
		? std::clamp(box.right - box.left, 24.0f, HorizontalWidth)
		: HorizontalWidth;

	ImVec2 tl, br;
	switch (pos)
	{
	case EHealthBarPosition::Top:
	{
		const float topY = box.valid ? box.top : c.feet.y - barHeight - SideGap;
		tl = ImVec2(c.feet.x - horizontalWidth * 0.5f, topY - barHeight - SideGap);
		br = ImVec2(tl.x + horizontalWidth, tl.y + barHeight);
		break;
	}
	case EHealthBarPosition::Bottom:
	{
		tl = ImVec2(c.feet.x - horizontalWidth * 0.5f, textBottomY);
		br = ImVec2(tl.x + horizontalWidth, tl.y + barHeight);
		break;
	}
	case EHealthBarPosition::Left:
	{
		tl = ImVec2(box.left - SideGap - VerticalThickness, box.top);
		br = ImVec2(tl.x + VerticalThickness, box.bottom);
		break;
	}
	case EHealthBarPosition::Right:
	default:
	{
		tl = ImVec2(box.right + SideGap, box.top);
		br = ImVec2(tl.x + VerticalThickness, box.bottom);
		break;
	}
	}

	const bool vertical = (pos == EHealthBarPosition::Left || pos == EHealthBarPosition::Right);

	const ImU32 fill = bHealthGradient
		? HealthColor(healthPercent)
		: U32(ColorPicker::HealthBarForegroundColor);

	c.dl->AddRectFilled(tl, br, WorldText::Fade(U32(ColorPicker::HealthBarBackgroundColor), c.alpha));
	// Brass hairline around the bar — reads as deco framing and separates the bar
	// from whatever is behind it.
	c.dl->AddRect(tl, br, WorldText::Fade(Theme::BrassDim, c.alpha * 0.8f), 0.0f, 0, 1.0f);

	const ImVec2 innerTL(tl.x + Padding, tl.y + Padding);
	const ImVec2 innerBR(br.x - Padding, br.y - Padding);

	if (innerBR.x > innerTL.x && innerBR.y > innerTL.y)
	{
		if (vertical)
		{
			// Grows bottom→top: depleting health drops the level like a thermometer.
			const float fillH = (innerBR.y - innerTL.y) * healthPercent;
			c.dl->AddRectFilled(ImVec2(innerTL.x, innerBR.y - fillH), innerBR,
			                    WorldText::Fade(fill, c.alpha));
		}
		else
		{
			const float fillW = (innerBR.x - innerTL.x) * healthPercent;
			c.dl->AddRectFilled(innerTL, ImVec2(innerTL.x + fillW, innerBR.y),
			                    WorldText::Fade(fill, c.alpha));
		}
	}

	// Numeric label: inside horizontal bars; floated above verticals, whose
	// column is too narrow for legible text.
	const std::string text = std::format("{}", PC.m_CurrentHealth);
	const float labelSize  = std::max(barHeight - Padding * 2.0f, 8.0f);
	const ImU32 labelCol   = WorldText::Fade(Theme::Cream, c.alpha);

	if (vertical)
		WorldText::Draw(c.dl, ImVec2((tl.x + br.x) * 0.5f, tl.y - labelSize - 1.0f),
		                text, labelCol, labelSize, WorldText::Align::Center);
	else
		WorldText::Draw(c.dl, ImVec2((tl.x + br.x) * 0.5f, tl.y + Padding),
		                text, labelCol, labelSize, WorldText::Align::Center);
}

void Draw_Players::DrawSkeleton(const Ctx& c)
{
	const C_CitadelPlayerPawn& pawn = c.Pawn();
	if (!pawn.m_pBoneData || pawn.m_pBoneData->pairs.empty()) return;

	const ImU32 col = WorldText::Fade(c.view->visible
		? U32(ColorPicker::SkeletonColorVisible)
		: U32(ColorPicker::SkeletonColorInvisible), c.alpha);

	for (const auto& [startBone, endBone] : pawn.m_pBoneData->pairs)
	{
		if (startBone >= MAX_BONES || endBone >= MAX_BONES) continue;

		ImVec2 a, b;
		if (!c.snap->Project(pawn.m_BonePositions[startBone], c.origin, a)) continue;
		if (!c.snap->Project(pawn.m_BonePositions[endBone],   c.origin, b)) continue;

		c.dl->AddLine(a, b, col, fBonesThickness);
	}
}

void Draw_Players::DrawHeadCircle(const Ctx& c)
{
	const C_CitadelPlayerPawn& pawn = c.Pawn();
	if (!pawn.m_pBoneData) return;

	const auto& headSlot = pawn.m_pBoneData->slotBones[static_cast<int>(HitboxSlot::Head)];
	if (headSlot.empty()) return;

	ImVec2 head;
	if (!c.snap->Project(pawn.m_BonePositions[headSlot[0]], c.origin, head)) return;

	// Radius from hammer-unit distance, matching the pre-snapshot behavior.
	const float distance = c.view->distanceMeters * HammerUnitsPerMeter;
	if (distance < 0.1f) return;

	const float radius = 5.0f * (1000.0f / (distance + 100.0f));

	const ImU32 col = WorldText::Fade(c.view->visible
		? U32(ColorPicker::SkeletonColorVisible)
		: U32(ColorPicker::SkeletonColorInvisible), c.alpha);

	c.dl->AddCircle(head, radius, col, 32, fBonesThickness);
}

void Draw_Players::DrawVelocityVector(const Ctx& c)
{
	const C_CitadelPlayerPawn& pawn = c.Pawn();

	ImVec2 start, end;
	if (!c.snap->Project(pawn.m_Position, c.origin, start)) return;
	if (!c.snap->Project(pawn.m_Position + pawn.m_Velocity, c.origin, end)) return;

	c.dl->AddLine(start, end, WorldText::Fade(Theme::Cream, c.alpha), 3.0f);
}

void Draw_Players::DrawBoneNumbers(const Ctx& c)
{
	const C_CitadelPlayerPawn& pawn = c.Pawn();
	const int count = std::clamp(pawn.m_BoneCount, 0, MAX_BONES);

	for (int i = 0; i < count; i++)
	{
		ImVec2 p;
		if (!c.snap->Project(pawn.m_BonePositions[i], c.origin, p)) continue;

		WorldText::Draw(c.dl, p, std::to_string(i), WorldText::Fade(Theme::Cream, c.alpha), 12.0f);
	}
}

float Draw_Players::DrawNameTag(const Ctx& c)
{
	const CCitadelPlayerController& PC   = c.PC();
	const C_CitadelPlayerPawn&      pawn = c.Pawn();

	const ImU32 teamCol = WorldText::Fade(PC.m_TeamNum == ETeam::HIDDEN_KING
		? U32(ColorPicker::HiddenKingTeamColor)
		: U32(ColorPicker::ArchMotherTeamColor), c.alpha);

	std::string tag;
	if (bShowHeroLevel && pawn.m_nLevel > 0)
		tag += std::format("[{}] ", pawn.m_nLevel);

	tag += PC.GetHeroName();

	if (bShowDistance)
		tag += std::format(" [{:.0f}m]", c.view->distanceMeters);

	WorldText::Stack stack(c.dl, c.feet, c.textSize);
	stack.Push(tag, teamCol);

	if (bDrawUnsecuredSouls && pawn.m_UnsecuredSouls >= UnsecuredSoulsMinimumThreshold)
	{
		const bool highlight = pawn.m_UnsecuredSouls > UnsecuredSoulsHighlightThreshold;
		const ImU32 soulCol = WorldText::Fade(highlight
			? U32(ColorPicker::UnsecuredSoulsHighlightedTextColor)
			: U32(ColorPicker::UnsecuredSoulsTextColor), c.alpha);

		stack.Push(std::format("{} {}", pawn.m_UnsecuredSouls,
		                       highlight ? "UNSECURED" : "Unsecured"), soulCol);
	}

	return stack.Cursor().y;
}

void Draw_Players::DrawRespawnTimer(const Ctx& c)
{
	// Same tag content as a live player (level / hero / distance), with the timer
	// appended underneath.
	const float y = DrawNameTag(c);

	const float remaining = c.Pawn().m_flRespawnTime - c.snap->serverTime;

	WorldText::Draw(c.dl, ImVec2(c.feet.x, y),
		remaining > 0.0f ? std::format("DEAD ({:.1f}s)", remaining) : std::string("DEAD"),
		WorldText::Fade(Theme::Danger, c.alpha), c.textSize);
}
