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
#include "DMA/DMA Thread.h"

namespace
{
	// Mirrors Deadlock's in-game shop/hero-select chrome: a top header with
	// title + status, a primary tab row (COMBAT/VISUALS/SYSTEM/DEBUG) with a
	// 2px brass underline on the active tab, a secondary row of sub-tabs
	// filtered to that group, and a footer rule with build + hotkey.

	struct Tab
	{
		const char* label;
		const char* group;   // primary category this section belongs to
		void (*draw)();
	};

	// Order within a group here is the order shown in the secondary tab row.
	// The primary tab order (kGroups below) is independent — if you reorder a
	// section, update nothing else.
	const Tab kTabs[] = {
		{ "Aim Assist", "COMBAT",  AimAssist::RenderSettings },
		{ "Keybinds",   "COMBAT",  Keybinds::Render          },

		{ "Overlay",    "VISUALS", Fuser::RenderSettings     },
		{ "Entities",   "VISUALS", Visuals::RenderSettings   },
		{ "Radar",      "VISUALS", Radar::RenderSettings     },
		{ "Colors",     "VISUALS", ColorPicker::Render       },

		{ "General",    "SYSTEM",  nullptr                   }, // DrawGeneralTab, bound below
		{ "Config",     "SYSTEM",  Config::Render            },

		{ "Players",    "DEBUG",   PlayerList::Render        },
		{ "Troopers",   "DEBUG",   TrooperList::Render       },
		{ "Classes",    "DEBUG",   ClassList::Render         },
	};
	constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));

	// Primary category order along the top tab row.
	const char* const kGroups[] = { "COMBAT", "VISUALS", "SYSTEM", "DEBUG" };
	constexpr int kGroupCount = (int)(sizeof(kGroups) / sizeof(kGroups[0]));

	void DrawGeneralTab()
	{
		ImGui::Checkbox("VSync", &MainMenu::bVSync);
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderInt("Target FPS", &MainMenu::iTargetFPS, 60, 240);

		ImGui::Spacing();
		ImGuiIO& io = ImGui::GetIO();
		ImGui::TextColored(Theme::Muted(), "ImGui FPS");  ImGui::SameLine(140.0f); ImGui::Text("%.1f", io.Framerate);
		ImGui::TextColored(Theme::Muted(), "Frame time"); ImGui::SameLine(140.0f); ImGui::Text("%.3f ms", 1000.0f / io.Framerate);
		ImGui::TextColored(Theme::Muted(), "Delta");      ImGui::SameLine(140.0f); ImGui::Text("%.3f ms", io.DeltaTime * 1000.0f);
	}

	// Letter-spaced uppercase text, drawn via ImDrawList so we aren't at the
	// mercy of ImGui's built-in text layout. Returns the pixel width consumed.
	float DrawUpperSpaced(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 col, float spacing = 2.5f)
	{
		float x = pos.x;
		for (const char* p = text; *p; ++p)
		{
			char c = (*p >= 'a' && *p <= 'z') ? char(*p - 32) : *p;
			char buf[2] = { c, 0 };
			dl->AddText(ImVec2(x, pos.y), col, buf);
			x += ImGui::CalcTextSize(buf).x + (c == ' ' ? 3.0f : spacing);
		}
		return x - pos.x;
	}

	float MeasureUpperSpaced(const char* text, float spacing = 2.5f)
	{
		float w = 0.0f;
		for (const char* p = text; *p; ++p)
		{
			char c = (*p >= 'a' && *p <= 'z') ? char(*p - 32) : *p;
			char buf[2] = { c, 0 };
			w += ImGui::CalcTextSize(buf).x + (c == ' ' ? 3.0f : spacing);
		}
		return w;
	}

	// Four L-shaped brass corner brackets inside the given rect. Deadlock uses
	// these to frame active cards and the shop panel.
	void DecoCornerBrackets(ImDrawList* dl, ImVec2 tl, ImVec2 br, float arm, float thick, ImU32 col)
	{
		dl->AddLine(tl, ImVec2(tl.x + arm, tl.y), col, thick);
		dl->AddLine(tl, ImVec2(tl.x, tl.y + arm), col, thick);
		dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x - arm, tl.y), col, thick);
		dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + arm), col, thick);
		dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x + arm, br.y), col, thick);
		dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y - arm), col, thick);
		dl->AddLine(br, ImVec2(br.x - arm, br.y), col, thick);
		dl->AddLine(br, ImVec2(br.x, br.y - arm), col, thick);
	}

	// A single top-row tab button. Returns true when clicked. Active tab gets a
	// 2px brass bar flush under the baseline — Deadlock's active-tab signature.
	bool DecoTab(const char* label, bool active, float padX = 18.0f, float padY = 10.0f)
	{
		ImDrawList* dl   = ImGui::GetWindowDrawList();
		const float textW = MeasureUpperSpaced(label);
		const float textH = ImGui::GetTextLineHeight();
		const ImVec2 size(textW + padX * 2.0f, textH + padY * 2.0f);

		const ImVec2 tl = ImGui::GetCursorScreenPos();
		const ImVec2 br(tl.x + size.x, tl.y + size.y);

		ImGui::PushID(label);
		ImGui::InvisibleButton("##tab", size);
		const bool clicked = ImGui::IsItemClicked();
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		const ImU32 col = active  ? Theme::Cream
		                  : hovered ? Theme::Cream
		                            : Theme::TextDim;

		DrawUpperSpaced(dl, ImVec2(tl.x + padX, tl.y + padY), label, col);

		if (active)
		{
			// 2px brass bar flush with the tab-row bottom.
			dl->AddRectFilled(ImVec2(tl.x + padX, br.y - 2.0f),
			                  ImVec2(br.x - padX, br.y),
			                  Theme::Brass);
		}
		else if (hovered)
		{
			dl->AddRectFilled(ImVec2(tl.x + padX, br.y - 2.0f),
			                  ImVec2(br.x - padX, br.y),
			                  IM_COL32(0xE1, 0xC1, 0x6E, 0x50));
		}

		return clicked;
	}

	// Secondary sub-tab, under the primary row. Smaller, lighter weight. Active
	// one carries a dimmer brass underline than the primary tabs.
	bool DecoSubTab(const char* label, bool active)
	{
		ImDrawList* dl   = ImGui::GetWindowDrawList();
		const float padX = 12.0f;
		const float padY = 6.0f;
		const ImVec2 textSize = ImGui::CalcTextSize(label);
		const ImVec2 size(textSize.x + padX * 2.0f, textSize.y + padY * 2.0f);

		const ImVec2 tl = ImGui::GetCursorScreenPos();
		const ImVec2 br(tl.x + size.x, tl.y + size.y);

		ImGui::PushID(label);
		ImGui::InvisibleButton("##subtab", size);
		const bool clicked = ImGui::IsItemClicked();
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		const ImU32 col = active ? Theme::Brass
		                           : hovered ? Theme::Cream
		                                     : Theme::TextDim;

		dl->AddText(ImVec2(tl.x + padX, tl.y + padY), col, label);

		if (active)
		{
			dl->AddLine(ImVec2(tl.x + padX, br.y - 1.0f),
			            ImVec2(br.x - padX, br.y - 1.0f),
			            Theme::BrassDim, 1.5f);
		}

		return clicked;
	}

	void SetClickThrough(bool clickThrough)
	{
		HWND hwnd = MainWindow::g_hWnd;
		if (!hwnd) return;
		LONG_PTR ex = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		LONG_PTR want = clickThrough ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT);
		if (want != ex)
			::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, want);
	}

	// Default tab index in each group. Persists across group switches so a user
	// bouncing between COMBAT and VISUALS stays on the sub-section they picked.
	int sSelectedInGroup[kGroupCount] = { 0, 0, 0, 0 };

	int GroupIndexOf(const char* g)
	{
		for (int i = 0; i < kGroupCount; ++i)
			if (strcmp(kGroups[i], g) == 0) return i;
		return 0;
	}

	// Enumerate the global tab indices that belong to a group, in kTabs order.
	int CollectGroupTabs(int groupIdx, int out[kTabCount])
	{
		int n = 0;
		for (int i = 0; i < kTabCount; ++i)
			if (strcmp(kTabs[i].group, kGroups[groupIdx]) == 0)
				out[n++] = i;
		return n;
	}

	void CallTabDraw(int tabIdx)
	{
		// SYSTEM/General is bound inline because DrawGeneralTab lives in this
		// TU — kTabs can't hold a pointer to a static with internal linkage
		// that is declared later in the same namespace via a const global init.
		if (strcmp(kTabs[tabIdx].label, "General") == 0)
		{
			DrawGeneralTab();
			return;
		}
		if (kTabs[tabIdx].draw) kTabs[tabIdx].draw();
	}
}

void MainMenu::Render()
{
	// Toggle key edge-detect. See the note in the pre-redo file: the high bit
	// is robust against other code consuming the low 'pressed since last call'
	// bit, and a key just rebound in the UI pretends it was already down so
	// the rebind keystroke itself doesn't immediately flip the menu.
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

	// Sentinel position on first show: center on primary monitor.
	if (WindowPos.x < 0.0f || WindowPos.y < 0.0f)
	{
		const float screenW = static_cast<float>(::GetSystemMetrics(SM_CXSCREEN));
		const float screenH = static_cast<float>(::GetSystemMetrics(SM_CYSCREEN));
		WindowPos = ImVec2((screenW - WindowSize.x) * 0.5f, (screenH - WindowSize.y) * 0.5f);
	}

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

	Theme::EnsureApplied();

	// Chromeless window: we draw our own title row, so ImGui's title bar would
	// be dead weight above it. The content fills edge-to-edge and we lay out
	// the header, tabs, body, and footer by hand.
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
	                       | ImGuiWindowFlags_NoCollapse
	                       | ImGuiWindowFlags_NoScrollbar
	                       | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("DEADLOCK DMA", nullptr, flags))
	{
		ImGui::PopStyleVar();

		ImDrawList* dl   = ImGui::GetWindowDrawList();
		const ImVec2 wtl = ImGui::GetWindowPos();
		const ImVec2 wsz = ImGui::GetWindowSize();
		const ImVec2 wbr(wtl.x + wsz.x, wtl.y + wsz.y);

		// --- Title row -----------------------------------------------------
		// "DEADLOCK  DMA" letter-spaced on the left, status chip on the right.
		// Doubles as the drag region.
		const float titleH = 46.0f;
		ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
		ImGui::InvisibleButton("##drag", ImVec2(wsz.x, titleH));
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			ImVec2 d = ImGui::GetIO().MouseDelta;
			ImGui::SetWindowPos(ImVec2(wtl.x + d.x, wtl.y + d.y));
		}

		DrawUpperSpaced(dl, ImVec2(wtl.x + 22.0f, wtl.y + 15.0f), "DEADLOCK  DMA", Theme::Cream, 3.5f);

		// Status chip, right-aligned. Dot colour reflects attach state.
		{
			const bool  attached = globals::g_attached.load(std::memory_order_acquire);
			const char* statusTxt = attached ? "ATTACHED" : "WAITING";
			const ImU32 dotCol    = attached ? IM_COL32(0x7F, 0xB0, 0x69, 0xFF) : Theme::Amber;

			char fpsBuf[32];
			snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", ImGui::GetIO().Framerate);

			const float statusW = MeasureUpperSpaced(statusTxt, 2.0f);
			const float fpsW    = ImGui::CalcTextSize(fpsBuf).x;
			const float gap     = 14.0f;
			const float totalW  = 10.0f + statusW + gap + fpsW;

			const float baseX = wbr.x - 22.0f - totalW;
			const float baseY = wtl.y + 17.0f;

			dl->AddCircleFilled(ImVec2(baseX + 5.0f, baseY + ImGui::GetTextLineHeight() * 0.5f), 3.5f, dotCol);
			DrawUpperSpaced(dl, ImVec2(baseX + 14.0f, baseY), statusTxt, Theme::Cream, 2.0f);
			dl->AddText(ImVec2(baseX + 14.0f + statusW + gap, baseY), Theme::TextDim, fpsBuf);
		}

		// Double brass rule under the title.
		{
			const float y = wtl.y + titleH - 10.0f;
			dl->AddRectFilled(ImVec2(wtl.x + 22.0f, y),      ImVec2(wbr.x - 22.0f, y + 2.0f), Theme::Brass);
			dl->AddLine      (ImVec2(wtl.x + 22.0f, y + 6.0f), ImVec2(wbr.x - 22.0f, y + 6.0f), Theme::Brass, 1.0f);
		}

		// --- Primary tab row ----------------------------------------------
		ImGui::SetCursorPos(ImVec2(16.0f, titleH + 8.0f));
		static int sPrimary = 0;
		if (sPrimary < 0 || sPrimary >= kGroupCount) sPrimary = 0;

		for (int g = 0; g < kGroupCount; ++g)
		{
			if (g > 0) ImGui::SameLine(0.0f, 2.0f);
			if (DecoTab(kGroups[g], sPrimary == g))
				sPrimary = g;
		}

		// Thin brass divider below the primary tab row.
		{
			const float y = wtl.y + titleH + 8.0f + ImGui::GetTextLineHeight() + 20.0f + 4.0f;
			dl->AddLine(ImVec2(wtl.x + 22.0f, y), ImVec2(wbr.x - 22.0f, y), Theme::BrassDim, 1.0f);
		}

		// --- Secondary sub-tab row ----------------------------------------
		ImGui::SetCursorPos(ImVec2(16.0f, titleH + 8.0f + ImGui::GetTextLineHeight() + 20.0f + 12.0f));

		int groupTabs[kTabCount];
		const int groupTabCount = CollectGroupTabs(sPrimary, groupTabs);
		int& selectedInGroup = sSelectedInGroup[sPrimary];
		if (selectedInGroup < 0 || selectedInGroup >= groupTabCount) selectedInGroup = 0;

		for (int i = 0; i < groupTabCount; ++i)
		{
			if (i > 0) ImGui::SameLine(0.0f, 4.0f);
			if (DecoSubTab(kTabs[groupTabs[i]].label, selectedInGroup == i))
				selectedInGroup = i;
		}

		// --- Body ---------------------------------------------------------
		const float bodyTop = titleH + 8.0f + ImGui::GetTextLineHeight() + 20.0f + 12.0f + 36.0f;
		const float footerH = 28.0f;

		ImGui::SetCursorPos(ImVec2(22.0f, bodyTop));
		ImGui::BeginChild("##body", ImVec2(wsz.x - 44.0f, wsz.y - bodyTop - footerH),
		                  ImGuiChildFlags_None);

		// Settings::RenderSearch owns both the search field and the filter
		// behaviour: when its field is non-empty it prints the filtered
		// results list and the panel-specific draw() is skipped for that
		// frame. Returning early here when it draws results is what panels
		// did before; keep that contract.
		Settings::RenderSearch();
		CallTabDraw(groupTabs[selectedInGroup]);

		ImGui::EndChild();

		// --- Footer -------------------------------------------------------
		{
			const float y = wbr.y - footerH + 2.0f;
			dl->AddLine(ImVec2(wtl.x + 22.0f, y), ImVec2(wbr.x - 22.0f, y), Theme::BrassDim, 1.0f);

			const char* hotkey = "END TO EXIT   ·   TOGGLE: INSERT";
			DrawUpperSpaced(dl, ImVec2(wtl.x + 22.0f, y + 10.0f), hotkey, Theme::TextDim, 1.5f);
		}

		// Capture geometry for Config persistence.
		WindowPos  = ImGui::GetWindowPos();
		WindowSize = ImGui::GetWindowSize();

		// Resize grip in the bottom-right, so the chromeless window is still
		// user-resizable. Draws its own brass triangle; hit-tests a 16px box.
		{
			const ImVec2 gripTL(wbr.x - 16.0f, wbr.y - 16.0f);
			ImGui::SetCursorScreenPos(gripTL);
			ImGui::InvisibleButton("##grip", ImVec2(16.0f, 16.0f));
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				ImVec2 d = ImGui::GetIO().MouseDelta;
				ImVec2 cur = ImGui::GetWindowSize();
				const float minW = 560.0f, minH = 400.0f;
				ImVec2 target(std::max(minW, cur.x + d.x), std::max(minH, cur.y + d.y));
				ImGui::SetWindowSize(target);
			}
			const ImU32 gripCol = ImGui::IsItemHovered() ? Theme::Brass : Theme::BrassDim;
			for (int i = 0; i < 3; ++i)
			{
				float off = 4.0f + i * 4.0f;
				dl->AddLine(ImVec2(wbr.x - off, wbr.y - 3.0f),
				            ImVec2(wbr.x - 3.0f, wbr.y - off),
				            gripCol, 1.2f);
			}
		}

		// Corner brackets last so they sit on top of everything.
		DecoCornerBrackets(dl, wtl, wbr, 14.0f, 1.5f, Theme::Brass);
	}
	else
	{
		ImGui::PopStyleVar();
	}
	ImGui::End();
}
