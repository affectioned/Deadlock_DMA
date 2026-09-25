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
#include "GUI/Theme/Theme.h"
#include "GUI/Settings/Settings.h"
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
		// Group heading this row sits under; nullptr continues the previous group.
		// Eleven flat rows gave no sense of which panel does what.
		const char* group;
	};

	// Order here is the tab order in the menu. Each draw fn writes only the
	// body — the panel functions had their ImGui::Begin/End stripped so they
	// can be hosted inside our shared sidebar layout.
	const Tab kTabs[] = {
		{ Icon_AimAssist, "Aim Assist", AimAssist::RenderSettings, "Combat"  },
		{ Icon_Fuser,     "Overlay",    Fuser::RenderSettings,     "Visuals" },
		{ Icon_Visuals,   "Entities",   Visuals::RenderSettings,   nullptr   },
		{ Icon_Radar,     "Radar",      Radar::RenderSettings,     nullptr   },
		{ Icon_Colors,    "Colors",     ColorPicker::Render,       nullptr   },
		{ Icon_General,   "General",    DrawGeneralTab,            "System"  },
		{ Icon_Keybinds,  "Keybinds",   Keybinds::Render,          nullptr   },
		{ Icon_Config,    "Config",     Config::Render,            nullptr   },
		{ Icon_Players,   "Players",    PlayerList::Render,        "Debug"   },
		{ Icon_Troopers,  "Troopers",   TrooperList::Render,       nullptr   },
		{ Icon_Classes,   "Classes",    ClassList::Render,         nullptr   },
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

	// Style lives in ImGui::GetStyle() and is rewritten only when the accent
	// changes, instead of 18 PushStyleColor calls on every frame.
	Theme::EnsureApplied();

	if (ImGui::Begin("DEADLOCK DMA"))
	{
		static int sSelected = 0;
		constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));
		if (sSelected < 0 || sSelected >= kTabCount) sSelected = 0;

		const float sidebarWidth = 158.0f;
		ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_Borders);
		{
			const float lineH    = ImGui::GetTextLineHeight();
			const float iconBoxW = lineH * 1.5f;   // gutter reserved on the left of every row
			const float iconSize = lineH * 0.95f;  // icon's own bounding box
			ImDrawList* dl       = ImGui::GetWindowDrawList();

			for (int i = 0; i < kTabCount; ++i)
			{
				if (kTabs[i].group)
				{
					if (i > 0) ImGui::Spacing();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Theme::Brass));
					ImGui::SeparatorText(kTabs[i].group);
					ImGui::PopStyleColor();
				}

				const ImVec2 rowStart = ImGui::GetCursorScreenPos();
				const bool   selected = (sSelected == i);

				// An empty Selectable owns the whole row; icon and label are both
				// drawn into the gutter-aware positions below. The old version
				// padded the label with six literal spaces to clear the icon,
				// which only lined up because the font happened to be monospaced.
				ImGui::PushID(i);
				if (ImGui::Selectable("##tab", selected, 0, ImVec2(0.0f, lineH)))
					sSelected = i;
				ImGui::PopID();

				const ImU32 rowCol = selected
					? ImGui::GetColorU32(ImGuiCol_Text)
					: Theme::TextDim;

				kTabs[i].icon(dl, ImVec2(rowStart.x + iconBoxW * 0.5f, rowStart.y + lineH * 0.5f),
				              iconSize, rowCol);
				dl->AddText(ImVec2(rowStart.x + iconBoxW, rowStart.y), rowCol, kTabs[i].label);

				// Amber rule down the left edge of the active row — deco framing,
				// and it survives the accent being changed.
				if (selected)
				{
					dl->AddRectFilled(ImVec2(rowStart.x - 4.0f, rowStart.y),
					                  ImVec2(rowStart.x - 2.0f, rowStart.y + lineH),
					                  ImGui::ColorConvertFloat4ToU32(ColorPicker::MenuAccent.Value));
				}
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##Content", ImVec2(0, 0));
		Settings::RenderSearch();
		kTabs[sSelected].draw();
		ImGui::EndChild();

		// Capture window geometry every frame so Config::SaveActive() picks up
		// the current pos/size on exit.
		WindowPos  = ImGui::GetWindowPos();
		WindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();
}
