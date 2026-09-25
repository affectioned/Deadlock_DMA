#include "pch.h"
#include "WorldText.h"
#include "GUI/Theme/Theme.h"

namespace
{
	// Four axis-aligned offsets rather than eight: at 1px it reads the same and
	// halves the glyph count, which matters when 40+ entities each draw 2-3 lines.
	constexpr ImVec2 kOutlineOffsets[4] = {
		{ -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f },
	};

	ImFont* CurrentFont() { return ImGui::GetFont(); }

	float ResolveSize(float size)
	{
		return size > 0.0f ? size : ImGui::GetFontSize();
	}
}

float WorldText::Draw(ImDrawList* dl, ImVec2 anchor, std::string_view text, ImU32 color,
                      float size, Align align)
{
	if (!dl || text.empty()) return 0.0f;

	const float  px    = ResolveSize(size);
	ImFont*      font  = CurrentFont();
	const char*  begin = text.data();
	const char*  end   = text.data() + text.size();

	const ImVec2 extent = font->CalcTextSizeA(px, FLT_MAX, 0.0f, begin, end);

	ImVec2 pos = anchor;
	switch (align)
	{
	case Align::Center: pos.x -= extent.x * 0.5f; break;
	case Align::Right:  pos.x -= extent.x;        break;
	case Align::Left:                             break;
	}

	// Snap to whole pixels — subpixel glyph origins on a 1px outline turn into
	// visible fringing at small sizes.
	// IM_ROUND lives in imgui_internal.h, which we don't pull in here.
	pos.x = std::floor(pos.x + 0.5f);
	pos.y = std::floor(pos.y + 0.5f);

	if (bOutline)
	{
		// Outline alpha tracks the fill so a faded tag's halo fades with it,
		// instead of leaving a dark smudge behind vanishing text.
		const float fillAlpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
		const ImU32 outline   = Fade(Theme::Outline, fillAlpha);

		for (const ImVec2& o : kOutlineOffsets)
			dl->AddText(font, px, ImVec2(pos.x + o.x, pos.y + o.y), outline, begin, end);
	}

	dl->AddText(font, px, pos, color, begin, end);
	return extent.y;
}

void WorldText::Stack::Push(std::string_view text, ImU32 color)
{
	const float h = Draw(m_dl, ImVec2(m_anchor.x, m_anchor.y + m_offset), text, color, m_size, m_align);
	// An empty or failed line must still advance predictably, otherwise a
	// conditional line silently collapses the rows beneath it.
	m_offset += (h > 0.0f) ? h : ResolveSize(m_size);
}

WorldText::Falloff WorldText::Compute(float distanceMeters)
{
	Falloff f{ fBaseTextSize, 1.0f };

	if (fMaxDistance > 0.0f && distanceMeters > fMaxDistance)
	{
		f.alpha = 0.0f;
		return f;
	}

	if (!bDistanceScale)
		return f;

	// Hyperbolic falloff: full size up to ~10m, then shrinking toward the floor.
	// Keeps a nearby enemy's tag readable without a distant one covering the map.
	const float d     = std::max(distanceMeters, 1.0f);
	const float scale = 20.0f / (d + 10.0f);
	f.size = std::clamp(fBaseTextSize * scale, fMinTextSize, fBaseTextSize);

	if (fFadeStart > 0.0f && distanceMeters > fFadeStart)
	{
		// Fade over the span between fFadeStart and fMaxDistance. With culling
		// disabled there's no far edge, so fall back to a fixed 40m ramp.
		const float span = (fMaxDistance > fFadeStart) ? (fMaxDistance - fFadeStart) : 40.0f;
		f.alpha = std::clamp(1.0f - (distanceMeters - fFadeStart) / span, 0.15f, 1.0f);
	}

	return f;
}

ImU32 WorldText::Fade(ImU32 color, float alpha)
{
	if (alpha >= 1.0f) return color;

	const ImU32 a = static_cast<ImU32>(std::clamp(alpha, 0.0f, 1.0f) *
	                                   static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF));
	return (color & ~static_cast<ImU32>(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
}
