#include "pch.h"
#include "Theme.h"
#include "GUI/Color Picker/Color Picker.h"

namespace
{
	ImVec4 V(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

	ImVec4 A(ImU32 c, float alpha)
	{
		ImVec4 v = V(c);
		v.w = alpha;
		return v;
	}

	// Unscaled baseline, captured on the first Apply(). Every Apply() resets to it
	// before writing the deco metrics and scaling once, so repeated re-applies are
	// idempotent — ScaleAllSizes compounds if you let it run twice over the same
	// style, and it also touches fields this file never sets.
	ImGuiStyle s_Base;
	bool       s_HaveBase = false;
	float      s_Scale    = 1.0f;
}

void Theme::SetScale(float scale)
{
	s_Scale = (scale > 0.0f) ? scale : 1.0f;
}

ImU32 Theme::Mix(ImU32 a, ImU32 b, float t)
{
	const ImVec4 va = V(a), vb = V(b);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(
		va.x + (vb.x - va.x) * t,
		va.y + (vb.y - va.y) * t,
		va.z + (vb.z - va.z) * t,
		va.w + (vb.w - va.w) * t));
}

ImVec4 Theme::Ok()    { return V(SoulGreen); }
ImVec4 Theme::Warn()  { return V(AmberHot); }
ImVec4 Theme::Bad()   { return V(Danger); }
ImVec4 Theme::Info()  { return V(Sapphire); }
ImVec4 Theme::Muted() { return V(TextDim); }

void Theme::PushDangerButton()
{
	ImGui::PushStyleColor(ImGuiCol_Button,        A(Danger, 0.55f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, A(Danger, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  V(Danger));
}

void Theme::PushAccentButton()
{
	const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ColorPicker::MenuAccent.Value);
	ImGui::PushStyleColor(ImGuiCol_Button,        A(accent, 0.55f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, A(accent, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  V(Mix(accent, Cream, 0.30f)));
}

void Theme::Apply()
{
	ImGuiStyle& s = ImGui::GetStyle();

	if (!s_HaveBase)
	{
		s_Base     = s;
		s_HaveBase = true;
	}
	s = s_Base;

	// Deco metrics: square panels, thin rules, generous gutters. Rounding is
	// what reads as "generic ImGui" more than any single color, so it's near-zero
	// everywhere except the small inner controls.
	s.WindowRounding    = 0.0f;
	s.ChildRounding     = 0.0f;
	s.PopupRounding     = 0.0f;
	s.FrameRounding     = 2.0f;
	s.GrabRounding      = 2.0f;
	s.ScrollbarRounding = 0.0f;
	s.TabRounding       = 0.0f;

	s.WindowBorderSize  = 1.0f;
	s.ChildBorderSize   = 1.0f;
	s.PopupBorderSize   = 1.0f;
	s.FrameBorderSize   = 1.0f;
	s.SeparatorTextBorderSize = 1.0f;

	s.WindowPadding     = ImVec2(12.0f, 10.0f);
	s.FramePadding      = ImVec2(8.0f, 4.0f);
	s.ItemSpacing       = ImVec2(8.0f, 6.0f);
	s.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
	s.IndentSpacing     = 18.0f;
	s.ScrollbarSize     = 12.0f;
	s.GrabMinSize       = 10.0f;

	s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
	s.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
	s.SeparatorTextPadding = ImVec2(18.0f, 4.0f);

	// The accent stays user-tunable; everything hot is derived from it so a
	// change to MenuAccent restyles the whole menu coherently.
	const ImU32 accent    = ImGui::ColorConvertFloat4ToU32(ColorPicker::MenuAccent.Value);
	const ImU32 accentHot = Mix(accent, Cream, 0.30f);

	ImVec4* c = s.Colors;
	c[ImGuiCol_Text]                  = V(Cream);
	c[ImGuiCol_TextDisabled]          = V(TextDim);
	c[ImGuiCol_WindowBg]              = A(Base, 0.97f);
	c[ImGuiCol_ChildBg]               = A(Panel, 0.85f);
	c[ImGuiCol_PopupBg]               = A(Panel, 0.98f);
	c[ImGuiCol_Border]                = A(BrassDim, 0.65f);
	c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

	c[ImGuiCol_FrameBg]               = A(PanelRaised, 0.90f);
	c[ImGuiCol_FrameBgHovered]        = A(PanelHover, 1.00f);
	c[ImGuiCol_FrameBgActive]         = A(accent, 0.35f);

	c[ImGuiCol_TitleBg]               = A(Panel, 1.00f);
	c[ImGuiCol_TitleBgActive]         = A(PanelRaised, 1.00f);
	c[ImGuiCol_TitleBgCollapsed]      = A(Panel, 0.80f);
	c[ImGuiCol_MenuBarBg]             = A(Panel, 1.00f);

	c[ImGuiCol_ScrollbarBg]           = A(Base, 0.50f);
	c[ImGuiCol_ScrollbarGrab]         = A(BrassDim, 0.50f);
	c[ImGuiCol_ScrollbarGrabHovered]  = A(Brass, 0.70f);
	c[ImGuiCol_ScrollbarGrabActive]   = A(accent, 0.90f);

	c[ImGuiCol_CheckMark]             = V(accentHot);
	c[ImGuiCol_SliderGrab]            = A(accent, 0.90f);
	c[ImGuiCol_SliderGrabActive]      = V(accentHot);

	c[ImGuiCol_Button]                = A(PanelRaised, 0.95f);
	c[ImGuiCol_ButtonHovered]         = A(accent, 0.45f);
	c[ImGuiCol_ButtonActive]          = A(accent, 0.70f);

	c[ImGuiCol_Header]                = A(accent, 0.32f);
	c[ImGuiCol_HeaderHovered]         = A(accent, 0.55f);
	c[ImGuiCol_HeaderActive]          = A(accent, 0.75f);

	c[ImGuiCol_Separator]             = A(BrassDim, 0.55f);
	c[ImGuiCol_SeparatorHovered]      = A(Brass, 0.85f);
	c[ImGuiCol_SeparatorActive]       = V(accentHot);

	c[ImGuiCol_ResizeGrip]            = A(BrassDim, 0.35f);
	c[ImGuiCol_ResizeGripHovered]     = A(Brass, 0.65f);
	c[ImGuiCol_ResizeGripActive]      = A(accent, 0.90f);

	c[ImGuiCol_InputTextCursor]       = V(accentHot);

	c[ImGuiCol_Tab]                   = A(Panel, 1.00f);
	c[ImGuiCol_TabHovered]            = A(accent, 0.50f);
	c[ImGuiCol_TabSelected]           = A(PanelRaised, 1.00f);
	c[ImGuiCol_TabSelectedOverline]   = V(accent);
	c[ImGuiCol_TabDimmed]             = A(Panel, 0.70f);
	c[ImGuiCol_TabDimmedSelected]     = A(PanelRaised, 0.80f);
	c[ImGuiCol_TabDimmedSelectedOverline] = A(BrassDim, 0.60f);

	c[ImGuiCol_PlotLines]             = V(Brass);
	c[ImGuiCol_PlotLinesHovered]      = V(accentHot);
	c[ImGuiCol_PlotHistogram]         = V(accent);
	c[ImGuiCol_PlotHistogramHovered]  = V(accentHot);

	c[ImGuiCol_TableHeaderBg]         = A(PanelRaised, 1.00f);
	c[ImGuiCol_TableBorderStrong]     = A(BrassDim, 0.80f);
	c[ImGuiCol_TableBorderLight]      = A(BrassDim, 0.40f);
	c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_TableRowBgAlt]         = A(PanelRaised, 0.35f);

	c[ImGuiCol_TextLink]              = V(Sapphire);
	c[ImGuiCol_TextSelectedBg]        = A(accent, 0.35f);
	c[ImGuiCol_TreeLines]             = A(BrassDim, 0.50f);
	c[ImGuiCol_DragDropTarget]        = V(accentHot);
	c[ImGuiCol_NavCursor]             = A(accent, 0.90f);
	c[ImGuiCol_NavWindowingHighlight] = A(Brass, 0.70f);
	c[ImGuiCol_NavWindowingDimBg]     = A(Base, 0.55f);
	c[ImGuiCol_ModalWindowDimBg]      = A(Base, 0.65f);

	// Scaled last, exactly once, from the unscaled baseline above.
	s.ScaleAllSizes(s_Scale);
}

void Theme::EnsureApplied()
{
	// ImU32 comparison rather than ImVec4 so a bit-identical accent never
	// triggers a redundant restyle.
	static ImU32 sLastAccent = 0;
	static bool  sApplied    = false;

	const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ColorPicker::MenuAccent.Value);
	if (sApplied && accent == sLastAccent)
		return;

	Apply();
	sLastAccent = accent;
	sApplied    = true;
}
