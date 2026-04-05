#include "pch.h"
#include "Main Menu.h"

#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/ESP/ESP.h"
#include "GUI/Fuser/Status Bars/Status Bars.h"
#include "GUI/Aimbot/Aimbot.h"
#include "GUI/Radar/Radar.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Keybinds/Keybinds.h"
#include "GUI/Config/Config.h"
#include "GUI/Debug GUI/Player List/Player List.h"
#include "GUI/Debug GUI/Class List/Class List.h"
#include "GUI/Debug GUI/Trooper List/Trooper List.h"

void MainMenu::Render()
{
	// Toggle visibility with Insert
	static bool bInsertWasDown = false;
	bool bInsertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
	if (bInsertDown && !bInsertWasDown)
		bVisible = !bVisible;
	bInsertWasDown = bInsertDown;

	if (!bVisible) return;

	// Position outside the main (Fuser) viewport on first use so ImGui
	// creates a separate OS window for the menu (requires ViewportsEnable).
	auto* mainVP = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(mainVP->Pos.x + mainVP->Size.x + 10.f, mainVP->Pos.y),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(600.f, 500.f), ImGuiCond_FirstUseEver);
	ImGui::Begin("DEADLOCK DMA", &bVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	if (ImGui::BeginTabBar("##MainTabs"))
	{
		// ── General ─────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("General"))
		{
			ImGui::SeparatorText("Performance");
			ImGui::Checkbox("VSync", &bVSync);
			ImGui::SameLine();
			ImGui::TextDisabled(bVSync ? "(on - capped to refresh rate)" : "(off - uncapped)");

			ImGuiIO& io = ImGui::GetIO();
			ImGui::Text("FPS: %.1f  Frame: %.3f ms", io.Framerate, 1000.0f / io.Framerate);

			ImGui::EndTabItem();
		}

		// ── ESP ──────────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("ESP"))
		{
			ESP::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Aimbot ───────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Aimbot"))
		{
			Aimbot::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Radar ────────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Radar"))
		{
			Radar::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Fuser ────────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Fuser"))
		{
			Fuser::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Colors ───────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Colors"))
		{
			ColorPicker::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Keybinds ─────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Keybinds"))
		{
			Keybinds::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Config ───────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Config"))
		{
			Config::RenderContent();
			ImGui::EndTabItem();
		}

		// ── Debug ────────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Debug"))
		{
			ImGui::SeparatorText("Debug Windows");
			ImGui::Checkbox("Player List",  &PlayerList::bSettings);
			ImGui::Checkbox("Class List",   &ClassList::bSettings);
			ImGui::Checkbox("Trooper List", &TrooperList::bSettings);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}
