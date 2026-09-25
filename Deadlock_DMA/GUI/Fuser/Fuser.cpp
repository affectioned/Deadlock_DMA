#include "pch.h"
#include "Fuser.h"
#include "Visuals/Visuals.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Status Bars/Status Bars.h"
#include "GUI/Aim Assist/Aim Assist.h"
#include "GUI/Main Window/Main Window.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "Visuals/Snapshot.h"
#include "Visuals/WorldText.h"
#include "GUI/Theme/Theme.h"

namespace
{
	// Set by the "Center on Screen" button; consumed (cleared) on the next Render()
	// to force the overlay back to (0,0). TU-private so toggling it doesn't trigger
	// a rebuild of every header consumer.
	bool s_bRequestRecenter = false;
}

void Fuser::Render()
{
	if (!bMasterToggle) return;

	ImGui::SetNextWindowPos(ImVec2(0, 0), s_bRequestRecenter ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	s_bRequestRecenter = false;
	ImGui::SetNextWindowSize(Fuser::m_ScreenSize);
	// The fullscreen opaque-black background is required, not incidental. The
	// overlay HWND is WS_EX_LAYERED + DwmExtendFrameIntoClientArea(-1), where DWM
	// renders pure black as glass — so this rect is what gives the overlay a
	// known, fully-opaque alpha surface that the ESP then draws onto. Dropping it
	// for ImGuiWindowFlags_NoBackground leaves the surface at the clear color's
	// alpha of 0 and the overlay stops compositing correctly.
	//
	// Written as 1.0f, not the 255.0f this used to carry. ImGui saturates the
	// float alpha on conversion so both produce the same opaque black; 255.0f just
	// looked like a byte value that had wandered into a float field.
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("Fuser", nullptr, ImGuiWindowFlags_NoDecoration);

	GuiWatchdog::GuiStage("Fuser/AimAssistFOV");
	AimAssist::RenderFOVCircle();

	GuiWatchdog::GuiStage("Fuser/Visuals");
	Visuals::OnFrame();

	GuiWatchdog::GuiStage("Fuser/SoulsPerMin");
	RenderSoulsPerMinute();

	GuiWatchdog::GuiStage("Fuser/StatusBars");
	StatusBars::Render();

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}

void Fuser::RenderSettings()
{
	ImGui::Checkbox("Enable Fuser", &bMasterToggle);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::SeparatorText("Display");
	{
		auto monitors = MainWindow::EnumerateMonitors();
		if (monitors.empty())
		{
			ImGui::TextDisabled("No monitors detected.");
		}
		else
		{
			const int curr = std::clamp(MainWindow::g_MonitorIndex, 0, (int)monitors.size() - 1);

			char preview[256];
			snprintf(preview, sizeof(preview), "%d: %dx%d %s",
				curr,
				monitors[curr].w, monitors[curr].h,
				monitors[curr].primary ? "(primary)" : "");

			ImGui::SetNextItemWidth(260.0f);
			if (ImGui::BeginCombo("Monitor", preview))
			{
				for (int i = 0; i < (int)monitors.size(); ++i)
				{
					char label[256];
					snprintf(label, sizeof(label), "%d: %dx%d @ (%d,%d) %s",
						i,
						monitors[i].w, monitors[i].h,
						monitors[i].x, monitors[i].y,
						monitors[i].primary ? "(primary)" : "");

					const bool selected = (i == curr);
					if (ImGui::Selectable(label, selected) && i != curr)
						MainWindow::RequestMonitorApply(i);
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::Button("Match selected monitor"))
			{
				m_ScreenSize.x = (float)monitors[curr].w;
				m_ScreenSize.y = (float)monitors[curr].h;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Sets Visuals resolution to %dx%d", monitors[curr].w, monitors[curr].h);
		}
	}

	ImGui::Spacing();

	ImGui::SeparatorText("Screen Configuration");
	ImGui::Text("Overlay Resolution");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Width##Screen", &m_ScreenSize.x, 0.0f, 0.0f, "%.0f");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Height##Screen", &m_ScreenSize.y, 0.0f, 0.0f, "%.0f");
	ImGui::TextDisabled("(Match your game resolution)");

	if (ImGui::Button("Center on Screen"))
		s_bRequestRecenter = true;

	ImGui::Spacing();

	ImGui::SeparatorText("HUD Elements");
	ImGui::Checkbox("Souls Per Minute", &bDrawSoulsPerMinute);

	ImGui::Checkbox("Team Health Bar", &StatusBars::bRenderTeamHealthBar);
	ImGui::Checkbox("Team Souls Bar", &StatusBars::bRenderTeamSoulsBar);
	ImGui::Checkbox("Unspent Souls Bar", &StatusBars::bRenderUnspentSoulsBar);
}

void Fuser::RenderSoulsPerMinute()
{
	if (!bDrawSoulsPerMinute) return;

	const FrameSnapshot& snap = Snapshot::Current();

	// Local controller index is only meaningful against the live vector, so
	// resolve the local controller out of the snapshot by address instead.
	uintptr_t localCtrl = 0;
	{
		std::scoped_lock lock(Deadlock::m_LocalAddressMutex);
		localCtrl = Deadlock::m_LocalPlayerControllerAddress;
	}
	if (!localCtrl) return;

	const CCitadelPlayerController* pc = nullptr;
	for (const auto& candidate : snap.controllers)
	{
		if (candidate.m_EntityAddress == localCtrl) { pc = &candidate; break; }
	}
	if (!pc) return;

	// Guard the divide: serverTime is 0 until the first read lands, and in a
	// fresh lobby, which used to produce inf on the HUD.
	if (snap.serverTime < 1.0f) return;

	const float soulsPerMinute = (static_cast<float>(pc->m_TotalSouls) / snap.serverTime) * 60.0f;

	const ImVec2 origin = ImGui::GetWindowPos();
	WorldText::Draw(ImGui::GetWindowDrawList(),
		ImVec2(origin.x + 8.0f, origin.y + m_ScreenSize.y - 30.0f),
		std::format("{:.1f} Souls/Min", soulsPerMinute),
		Theme::SoulGreen, 22.0f, WorldText::Align::Left);
}