#pragma once

// Deadlock's art direction is 1930s art-deco noir: warm brown-black grounds,
// brass rules, cream type, amber as the hot accent and sapphire as its cold
// counterpoint. The two team colors in Color Picker.h are already exactly that
// (HiddenKing = amber, ArchMother = sapphire) — this palette extends them to
// the rest of the UI so the menu stops looking like stock ImGui dark.
//
// Nothing here is pure black on purpose. The overlay HWND is layered +
// DwmExtendFrameIntoClientArea, so RGB(0,0,0) composites as glass; a "black"
// panel or text outline would punch a hole straight through to the game.
namespace Theme
{
	// Hex values sampled from Deadlock's in-game UI (shop, hero select, settings
	// panels). Brass `#E1C16E` and Amber `#FFBF00` are the game's own crosshair
	// presets; Cream `#EFDEBF` and the brown-black ground are from the shipping
	// panel chrome.
	inline constexpr ImU32 Base        = IM_COL32(0x1A, 0x16, 0x12, 0xFF); // window ground
	inline constexpr ImU32 Panel       = IM_COL32(0x25, 0x1F, 0x18, 0xFF); // child / popup
	inline constexpr ImU32 PanelRaised = IM_COL32(0x30, 0x28, 0x1E, 0xFF); // frame bg, button
	inline constexpr ImU32 PanelHover  = IM_COL32(0x3F, 0x34, 0x26, 0xFF);
	inline constexpr ImU32 BrassDim    = IM_COL32(0xB0, 0x95, 0x4A, 0xFF); // borders, rules
	inline constexpr ImU32 Brass       = IM_COL32(0xE1, 0xC1, 0x6E, 0xFF); // hovered rules, accent
	inline constexpr ImU32 Amber       = IM_COL32(0xFF, 0xBF, 0x00, 0xFF); // hot accent (= HiddenKing)
	inline constexpr ImU32 AmberHot    = IM_COL32(0xFF, 0xD1, 0x4A, 0xFF); // active / pressed
	inline constexpr ImU32 Sapphire    = IM_COL32(0x4E, 0x76, 0xC4, 0xFF); // secondary (= ArchMother)
	inline constexpr ImU32 Cream       = IM_COL32(0xEF, 0xDE, 0xBF, 0xFF); // primary text
	inline constexpr ImU32 TextDim     = IM_COL32(0x8A, 0x7E, 0x68, 0xFF); // disabled / hints
	inline constexpr ImU32 SoulGreen   = IM_COL32(0x7F, 0xB0, 0x69, 0xFF); // souls / economy
	inline constexpr ImU32 Danger      = IM_COL32(0xC0, 0x39, 0x2B, 0xFF); // low health, critical

	// Near-black used behind world-space text. Deliberately not RGB(0,0,0) —
	// see the note above about the compositor treating black as glass.
	inline constexpr ImU32 Outline     = IM_COL32(0x06, 0x05, 0x04, 0xD7);

	// DPI scale for the style metrics. Apply() bakes it in itself, so callers must
	// not also call ImGuiStyle::ScaleAllSizes — that would compound on every
	// re-apply. Set once at init, before the first Apply().
	void SetScale(float scale);

	// Writes the full palette + deco style metrics into ImGui::GetStyle().
	// Accent-derived entries follow ColorPicker::MenuAccent so the user's accent
	// choice still drives the UI.
	void Apply();

	// Cheap per-frame guard: re-runs Apply() only when the accent actually
	// changed. Replaces the 18 PushStyleColor calls the menu used to issue on
	// every single frame.
	void EnsureApplied();

	// Interpolate two packed colors. Used for health-bar gradients and for
	// deriving hover/active tints from the accent.
	ImU32 Mix(ImU32 a, ImU32 b, float t);

	// Semantic colors for status text, as ImVec4 for the ImGui::TextColored call
	// sites. Panels used to hardcode pure red / pure green / cornflower blue,
	// which read as error dialogs bolted onto the palette rather than part of it.
	ImVec4 Ok();
	ImVec4 Warn();
	ImVec4 Bad();
	ImVec4 Info();
	ImVec4 Muted();

	// Button palettes for actions that need to stand apart from the default.
	// Each pushes exactly 3 style colors — pop 3.
	void PushDangerButton();
	void PushAccentButton();
}
