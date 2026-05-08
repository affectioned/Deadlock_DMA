#include "pch.h"
#include "Main Menu.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/ESP/ESP.h"
#include "GUI/Fuser/Status Bars/Status Bars.h"
#include "GUI/Radar/Radar.h"
#include "GUI/Aimbot/Aimbot.h"
#include "GUI/Keybinds/Keybinds.h"
#include "GUI/Config/Config.h"
#include "GUI/Debug GUI/Player List/Player List.h"
#include "GUI/Debug GUI/Class List/Class List.h"
#include "GUI/Debug GUI/Trooper List/Trooper List.h"

namespace
{
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

	struct Tab
	{
		const char* label;
		void (*draw)();
	};

	// Order here is the tab order in the menu. Each draw fn writes only the
	// body — the panel functions had their ImGui::Begin/End stripped so they
	// can be hosted inside our shared TabItem.
	const Tab kTabs[] = {
		{ "General",  DrawGeneralTab },
		{ "Aimbot",   Aimbot::RenderSettings },
		{ "Fuser",    Fuser::RenderSettings },
		{ "ESP",      ESP::RenderSettings },
		{ "Colors",   ColorPicker::Render },
		{ "Keybinds", Keybinds::Render },
		{ "Config",   Config::Render },
		{ "Players",  PlayerList::Render },
		{ "Troopers", TrooperList::Render },
		{ "Classes",  ClassList::Render },
	};
}

void MainMenu::Render()
{
	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

	if (ImGui::Begin("DEADLOCK DMA"))
	{
		if (ImGui::BeginTabBar("##MainTabs", ImGuiTabBarFlags_Reorderable))
		{
			for (const auto& tab : kTabs)
			{
				if (ImGui::BeginTabItem(tab.label))
				{
					tab.draw();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}

		// Capture window geometry every frame so Config::SaveActive() picks up
		// the current pos/size on exit.
		WindowPos  = ImGui::GetWindowPos();
		WindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();
}
