#pragma once

// World-space text for the overlay.
//
// Replaces the old SetCursorPos + ImGui::TextColored pattern every Draw_* file
// used to open-code. Three reasons it had to go:
//
//   1. Widget text is laid out against the host window's cursor, so a tag can
//      be clipped by the window rect and every tag pays for an ID-stack push,
//      ItemAdd and content-size update it has no use for.
//   2. ImGui::Text* takes a printf format. Passing a hero name or entity label
//      straight in — which is what the old call sites did — treats any '%' in
//      game data as a conversion specifier.
//   3. There was nowhere to hang an outline, so light-colored tags disappeared
//      over bright geometry.
//
// AddText on the window draw list fixes all three.
namespace WorldText
{
	// Horizontal alignment relative to the anchor x.
	enum class Align { Center, Left, Right };

	// One line of outlined text. `size` is the pixel height; 0 uses the current
	// font size. Returns the advance height so callers can stack manually.
	float Draw(ImDrawList* dl, ImVec2 anchor, std::string_view text, ImU32 color,
	           float size = 0.0f, Align align = Align::Center);

	// Stacks consecutive lines downward from a fixed anchor. Every entity tag in
	// the overlay is a short vertical run of centered lines, so this is the shape
	// the call sites actually want.
	class Stack
	{
	public:
		Stack(ImDrawList* dl, ImVec2 anchor, float size = 0.0f, Align align = Align::Center)
			: m_dl(dl), m_anchor(anchor), m_size(size), m_align(align) {}

		void Push(std::string_view text, ImU32 color);

		// Pixels consumed so far — lets a caller place a health bar below the
		// text block without guessing at line counts.
		float Height() const { return m_offset; }

		// Where the next line would land. Used by the health bar's Bottom mode.
		ImVec2 Cursor() const { return ImVec2(m_anchor.x, m_anchor.y + m_offset); }

	private:
		ImDrawList* m_dl;
		ImVec2      m_anchor;
		float       m_size;
		Align       m_align;
		float       m_offset{ 0.0f };
	};

	// Distance falloff shared by every entity tag so text scales and fades
	// consistently. `distanceMeters` is the value already shown in nametags.
	struct Falloff
	{
		float size;   // pixel height to draw at
		float alpha;  // 0..1 multiplier; 0 means "cull this entity"
	};

	Falloff Compute(float distanceMeters);

	// Applies Compute()'s alpha to a packed color without touching its RGB.
	ImU32 Fade(ImU32 color, float alpha);

	// Tunables, surfaced in the Visuals tab and persisted by Settings.
	inline bool  bOutline{ true };
	inline bool  bDistanceScale{ true };
	inline float fBaseTextSize{ 16.0f };
	inline float fMinTextSize{ 10.0f };
	// Beyond fFadeStart tags dim toward zero; at fMaxDistance they're culled.
	// 0 for fMaxDistance disables culling entirely.
	inline float fFadeStart{ 60.0f };
	inline float fMaxDistance{ 0.0f };
}
