#include "pch.h"
#include "Main Menu.h"
#include "GUI/Main Window/Main Window.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/Visuals/Visuals.h"
#include "GUI/Fuser/Status Bars/Status Bars.h"
#include "GUI/Radar/Radar.h"
#include "GUI/Aim Assist/Aim Assist.h"
#include "GUI/Keybinds/Keybinds.h"
#include "GUI/Config/Config.h"
#include "GUI/Debug GUI/Player List/Player List.h"
#include "GUI/Debug GUI/Class List/Class List.h"
#include "GUI/Debug GUI/Trooper List/Trooper List.h"

namespace
{
	// imgui_internal.h would expose IM_PI but it's heavy; define locally instead.
	constexpr float kPI = 3.14159265358979323846f;

	void DrawGeneralTab()
	{
		ImGui::SeparatorText("Performance");
		ImGui::Checkbox("VSync", &MainMenu::bVSync);
		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderInt("Target FPS", &MainMenu::iTargetFPS, 60, 240);

		ImGui::Spacing();
		ImGuiIO& io = ImGui::GetIO();
		ImGui::Text("ImGui IO FPS: %.1f", io.Framerate);
		ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);
		ImGui::Text("Delta Time: %.3f ms", io.DeltaTime * 1000.0f);
	}

	// Icon drawing primitives. Each takes the ImDrawList, the icon's center
	// point in screen space, a bounding-box edge length, and a color. Designed
	// to read clearly at ~14-18px and scale up cleanly.
	using IconFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

	void Icon_General(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Gear: ring of 8 teeth + center hub
		const float r = s * 0.5f;
		dl->AddCircle(c, r * 0.85f, col, 0, 1.5f);
		dl->AddCircle(c, r * 0.30f, col, 0, 1.5f);
		for (int i = 0; i < 8; ++i)
		{
			float a = i * (kPI / 4.0f);
			ImVec2 p1(c.x + cosf(a) * r * 0.85f, c.y + sinf(a) * r * 0.85f);
			ImVec2 p2(c.x + cosf(a) * r,         c.y + sinf(a) * r);
			dl->AddLine(p1, p2, col, 1.5f);
		}
	}

	void Icon_AimAssist(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Crosshair: circle + 4 ticks + center dot
		const float r = s * 0.5f;
		dl->AddCircle(c, r * 0.75f, col, 0, 1.5f);
		dl->AddLine(ImVec2(c.x - r,        c.y), ImVec2(c.x - r * 0.45f, c.y), col, 1.5f);
		dl->AddLine(ImVec2(c.x + r * 0.45f, c.y), ImVec2(c.x + r,         c.y), col, 1.5f);
		dl->AddLine(ImVec2(c.x, c.y - r),         ImVec2(c.x, c.y - r * 0.45f), col, 1.5f);
		dl->AddLine(ImVec2(c.x, c.y + r * 0.45f), ImVec2(c.x, c.y + r),         col, 1.5f);
		dl->AddCircleFilled(c, 1.2f, col);
	}

	void Icon_Fuser(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// HUD overlay frame: four L-shaped corner brackets
		const float r = s * 0.45f;
		const float arm = r * 0.5f;
		const ImVec2 corners[4] = {
			{ c.x - r, c.y - r }, { c.x + r, c.y - r },
			{ c.x - r, c.y + r }, { c.x + r, c.y + r },
		};
		const float sx[4] = { +1, -1, +1, -1 };
		const float sy[4] = { +1, +1, -1, -1 };
		for (int i = 0; i < 4; ++i)
		{
			dl->AddLine(corners[i], ImVec2(corners[i].x + sx[i] * arm, corners[i].y), col, 1.5f);
			dl->AddLine(corners[i], ImVec2(corners[i].x, corners[i].y + sy[i] * arm), col, 1.5f);
		}
	}

	void Icon_Visuals(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Eye: two arcs forming an almond + iris dot
		const float r = s * 0.5f;
		dl->PathClear();
		dl->PathArcTo(ImVec2(c.x, c.y + r * 0.7f), r * 1.05f, -kPI * 0.72f, -kPI * 0.28f, 14);
		dl->PathStroke(col, 0, 1.5f);
		dl->PathClear();
		dl->PathArcTo(ImVec2(c.x, c.y - r * 0.7f), r * 1.05f, kPI * 0.28f, kPI * 0.72f, 14);
		dl->PathStroke(col, 0, 1.5f);
		dl->AddCircleFilled(c, r * 0.28f, col);
	}

	void Icon_Radar(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Concentric circles + sweep line
		const float r = s * 0.5f;
		dl->AddCircle(c, r,         col, 0, 1.5f);
		dl->AddCircle(c, r * 0.62f, col, 0, 1.0f);
		dl->AddCircle(c, r * 0.25f, col, 0, 1.0f);
		dl->AddLine(c, ImVec2(c.x + cosf(-kPI * 0.25f) * r, c.y + sinf(-kPI * 0.25f) * r), col, 1.5f);
	}

	void Icon_Colors(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Three overlapping dots (color swatches)
		const float r = s * 0.20f;
		dl->AddCircle(ImVec2(c.x - r,         c.y - r * 0.5f), r, col, 0, 1.2f);
		dl->AddCircle(ImVec2(c.x + r,         c.y - r * 0.5f), r, col, 0, 1.2f);
		dl->AddCircle(ImVec2(c.x,             c.y + r * 0.9f), r, col, 0, 1.2f);
	}

	void Icon_Keybinds(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Keycap: rounded rect + small label line inside
		const float rx = s * 0.45f;
		const float ry = s * 0.36f;
		dl->AddRect(ImVec2(c.x - rx, c.y - ry), ImVec2(c.x + rx, c.y + ry), col, 2.0f, 0, 1.5f);
		dl->AddLine(ImVec2(c.x - rx * 0.4f, c.y + ry * 0.35f), ImVec2(c.x + rx * 0.4f, c.y + ry * 0.35f), col, 1.5f);
	}

	void Icon_Config(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Floppy disk: outer body with cut corner + slider + label
		const float r = s * 0.45f;
		const ImVec2 tl(c.x - r, c.y - r);
		const ImVec2 br(c.x + r, c.y + r);
		// body with notched top-right corner
		dl->AddLine(tl, ImVec2(br.x - r * 0.4f, tl.y), col, 1.5f);
		dl->AddLine(ImVec2(br.x - r * 0.4f, tl.y), ImVec2(br.x, tl.y + r * 0.4f), col, 1.5f);
		dl->AddLine(ImVec2(br.x, tl.y + r * 0.4f), br, col, 1.5f);
		dl->AddLine(br, ImVec2(tl.x, br.y), col, 1.5f);
		dl->AddLine(ImVec2(tl.x, br.y), tl, col, 1.5f);
		// metal slider near top
		dl->AddRect(ImVec2(c.x - r * 0.55f, tl.y), ImVec2(c.x + r * 0.25f, c.y - r * 0.45f), col, 0, 0, 1.0f);
		// label strip near bottom
		dl->AddRect(ImVec2(c.x - r * 0.6f, c.y + r * 0.05f), ImVec2(c.x + r * 0.6f, c.y + r * 0.75f), col, 0, 0, 1.0f);
	}

	void Icon_Players(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Person silhouette: head circle + shoulder arc
		const float r = s * 0.5f;
		dl->AddCircle(ImVec2(c.x, c.y - r * 0.45f), r * 0.30f, col, 0, 1.5f);
		dl->PathClear();
		dl->PathArcTo(ImVec2(c.x, c.y + r * 1.15f), r * 0.90f, -kPI * 0.82f, -kPI * 0.18f, 16);
		dl->PathStroke(col, 0, 1.5f);
	}

	void Icon_Troopers(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// Sword: blade diagonal + crossguard + pommel
		const float r = s * 0.5f;
		// blade
		dl->AddLine(ImVec2(c.x - r * 0.55f, c.y + r * 0.55f), ImVec2(c.x + r * 0.7f, c.y - r * 0.7f), col, 1.7f);
		// crossguard (perpendicular near hilt)
		dl->AddLine(ImVec2(c.x - r * 0.65f, c.y + r * 0.25f), ImVec2(c.x - r * 0.15f, c.y + r * 0.75f), col, 1.5f);
		// pommel
		dl->AddCircleFilled(ImVec2(c.x - r * 0.7f, c.y + r * 0.7f), 1.5f, col);
	}

	void Icon_Classes(ImDrawList* dl, ImVec2 c, float s, ImU32 col)
	{
		// List rows: bullet + line repeated three times
		const float r = s * 0.45f;
		for (int i = 0; i < 3; ++i)
		{
			float y = c.y - r * 0.7f + i * r * 0.7f;
			dl->AddCircleFilled(ImVec2(c.x - r * 0.85f, y), 1.3f, col);
			dl->AddLine(ImVec2(c.x - r * 0.55f, y), ImVec2(c.x + r * 0.85f, y), col, 1.5f);
		}
	}

	struct Tab
	{
		IconFn      icon;
		const char* label;
		void (*draw)();
	};

	// Order here is the tab order in the menu. Each draw fn writes only the
	// body — the panel functions had their ImGui::Begin/End stripped so they
	// can be hosted inside our shared sidebar layout.
	const Tab kTabs[] = {
		{ Icon_General,  "General",  DrawGeneralTab },
		{ Icon_AimAssist, "Aim Assist", AimAssist::RenderSettings },
		{ Icon_Fuser,    "Fuser",    Fuser::RenderSettings },
		{ Icon_Visuals,  "Visuals",  Visuals::RenderSettings },
		{ Icon_Radar,    "Radar",    Radar::RenderSettings },
		{ Icon_Colors,   "Colors",   ColorPicker::Render },
		{ Icon_Keybinds, "Keybinds", Keybinds::Render },
		{ Icon_Config,   "Config",   Config::Render },
		{ Icon_Players,  "Players",  PlayerList::Render },
		{ Icon_Troopers, "Troopers", TrooperList::Render },
		{ Icon_Classes,  "Classes",  ClassList::Render },
	};

	void SetClickThrough(bool clickThrough)
	{
		HWND hwnd = MainWindow::g_hWnd;
		if (!hwnd) return;
		LONG_PTR ex = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		LONG_PTR want = clickThrough ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT);
		if (want != ex)
			::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, want);
	}

	// Derive a coherent palette from the single accent color and push it for
	// every widget the user sees in the main menu. Returns the number of
	// PushStyleColor calls so the caller can pop the exact count.
	int PushAccentTheme()
	{
		auto mix = [](ImVec4 a, ImVec4 b, float t)
		{
			return ImVec4(a.x + (b.x - a.x) * t,
			              a.y + (b.y - a.y) * t,
			              a.z + (b.z - a.z) * t,
			              a.w + (b.w - a.w) * t);
		};
		auto withA = [](ImVec4 c, float a) { return ImVec4(c.x, c.y, c.z, a); };

		const ImVec4 accent = ColorPicker::MenuAccent.Value;
		const ImVec4 white  = ImVec4(1, 1, 1, 1);
		const ImVec4 black  = ImVec4(0, 0, 0, 1);
		const ImVec4 hover  = mix(accent, white, 0.20f);
		const ImVec4 active = mix(accent, white, 0.35f);
		const ImVec4 dim    = mix(accent, black, 0.55f);

		ImGui::PushStyleColor(ImGuiCol_Header,            withA(accent, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered,     withA(hover,  0.70f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive,      withA(active, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_Button,            withA(dim,    0.55f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered,     withA(hover,  0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,      withA(active, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,    withA(accent, 0.30f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive,     withA(accent, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab,        withA(accent, 0.90f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  active);
		ImGui::PushStyleColor(ImGuiCol_CheckMark,         active);
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive,     withA(dim,    0.85f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered,  withA(hover,  0.70f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive,   active);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip,        withA(accent, 0.25f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, withA(hover,  0.65f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive,  active);
		ImGui::PushStyleColor(ImGuiCol_TabHovered,        withA(hover,  0.85f));
		return 18;
	}
}

void MainMenu::Render()
{
	// Edge-detect the toggle key. High bit ('currently down') is robust against any
	// other code that consumes the low 'pressed since last call' bit via the same API.
	// When the user rebinds Keybinds::Menu the new key may already be held — pretend
	// it was already down so the rebind keystroke doesn't immediately fire as a toggle.
	{
		static bool sPrevDown = false;
		static uint32_t sLastSeenKey = 0;

		const uint32_t key = Keybinds::Menu.m_Key;
		if (key != sLastSeenKey)
		{
			sPrevDown = (::GetAsyncKeyState(key) & 0x8000) != 0;
			sLastSeenKey = key;
		}

		const bool down = key ? ((::GetAsyncKeyState(key) & 0x8000) != 0) : false;
		if (down && !sPrevDown)
		{
			MainMenu::bOpen = !MainMenu::bOpen;
			SetClickThrough(!MainMenu::bOpen);
		}
		sPrevDown = down;
	}

	if (!MainMenu::bOpen)
		return;

	// Resolve sentinel position on first show: center on the primary monitor.
	if (WindowPos.x < 0.0f || WindowPos.y < 0.0f)
	{
		const float screenW = static_cast<float>(::GetSystemMetrics(SM_CXSCREEN));
		const float screenH = static_cast<float>(::GetSystemMetrics(SM_CYSCREEN));
		WindowPos = ImVec2((screenW - WindowSize.x) * 0.5f, (screenH - WindowSize.y) * 0.5f);
	}

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

	const int accentPopCount = PushAccentTheme();
	if (ImGui::Begin("DEADLOCK DMA"))
	{
		static int sSelected = 0;
		constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));
		if (sSelected < 0 || sSelected >= kTabCount) sSelected = 0;

		const float sidebarWidth = 140.0f;
		ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_Borders);
		{
			const float lineH      = ImGui::GetTextLineHeight();
			const float iconBoxW   = lineH * 1.4f;   // gutter reserved on the left of every row
			const float iconSize   = lineH * 0.95f;  // icon's own bounding box
			ImDrawList* dl         = ImGui::GetWindowDrawList();

			for (int i = 0; i < kTabCount; ++i)
			{
				const ImVec2 rowStart = ImGui::GetCursorScreenPos();

				// Leading spaces shift the label past the icon gutter so the
				// Selectable's text doesn't overlap the icon.
				char row[64];
				snprintf(row, sizeof(row), "      %s##tab%d", kTabs[i].label, i);
				if (ImGui::Selectable(row, sSelected == i))
					sSelected = i;

				const ImVec2 iconCenter(rowStart.x + iconBoxW * 0.5f, rowStart.y + lineH * 0.5f);
				kTabs[i].icon(dl, iconCenter, iconSize, ImGui::GetColorU32(ImGuiCol_Text));
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##Content", ImVec2(0, 0));
		kTabs[sSelected].draw();
		ImGui::EndChild();

		// Capture window geometry every frame so Config::SaveActive() picks up
		// the current pos/size on exit.
		WindowPos  = ImGui::GetWindowPos();
		WindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();
	ImGui::PopStyleColor(accentPopCount);
}
