#include "pch.h"
#include "Fuser.h"
#include "ESP/ESP.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Status Bars/Status Bars.h"
#include "GUI/Aimbot/Aimbot.h"

void Fuser::Render()
{
	if (!bMasterToggle) return;

	// Position at the selected monitor's origin so ViewportsEnable gives the
	// Fuser its own OS window, completely separate from the menu window.
	ImGui::SetNextWindowPos(m_MonitorPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(m_ScreenSize, ImGuiCond_Always);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::Begin("##Fuser", nullptr,
		ImGuiWindowFlags_NoDecoration              |
		ImGuiWindowFlags_NoMove                    |
		ImGuiWindowFlags_NoResize                  |
		ImGuiWindowFlags_NoFocusOnAppearing        |
		ImGuiWindowFlags_NoDocking                 |
		ImGuiWindowFlags_NoScrollWithMouse         |
		ImGuiWindowFlags_NoInputs                  |
		ImGuiWindowFlags_NoNav);
	auto WindowPos = ImGui::GetWindowPos();
	auto DrawList = ImGui::GetWindowDrawList();

	Aimbot::RenderFOVCircle();

	ESP::OnFrame();

	RenderSoulsPerMinute();

	StatusBars::Render();

	ImGui::End();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

void Fuser::RenderContent()
{
	if (!bSettings) return;

	ImGui::Checkbox("Enable Fuser", &bMasterToggle);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();


	ImGui::SeparatorText("Screen Configuration");

	auto& monitors = ImGui::GetPlatformIO().Monitors;
	int monitorCount = monitors.Size;

	// Build a display string for the currently selected monitor
	auto MonitorLabel = [&](int idx) -> std::string {
		if (idx >= monitorCount) return "None";
		auto& m = monitors[idx];
		return std::string("Monitor ") + std::to_string(idx + 1) +
			" (" + std::to_string((int)m.MainSize.x) + "x" + std::to_string((int)m.MainSize.y) + ")";
	};

	std::string currentLabel = MonitorLabel(m_MonitorIndex);
	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("Monitor##FuserMonitor", currentLabel.c_str()))
	{
		for (int i = 0; i < monitorCount; i++)
		{
			bool selected = (m_MonitorIndex == i);
			if (ImGui::Selectable(MonitorLabel(i).c_str(), selected))
			{
				m_MonitorIndex = i;
				m_MonitorPos  = monitors[i].MainPos;
				if (bAutoDetectResolution)
					m_ScreenSize = monitors[i].MainSize;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::Checkbox("Auto-Detect Resolution", &bAutoDetectResolution);
	if (bAutoDetectResolution)
	{
		if (m_MonitorIndex < monitorCount)
			m_ScreenSize = monitors[m_MonitorIndex].MainSize;
		ImGui::BeginDisabled();
	}
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Width##Screen", &m_ScreenSize.x, 0.0f, 0.0f, "%.0f");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Height##Screen", &m_ScreenSize.y, 0.0f, 0.0f, "%.0f");
	if (bAutoDetectResolution)
		ImGui::EndDisabled();

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

	ZoneScoped;

	std::scoped_lock PawnLock(EntityList::m_ControllerMutex);

	if (EntityList::m_LocalControllerIndex < 0) return;

	auto& LocalController = EntityList::m_PlayerControllers[EntityList::m_LocalControllerIndex];

	auto Souls = LocalController.m_TotalSouls;
	float SoulsPerSecond = 0.0f;

	{
		std::scoped_lock timeLock(Deadlock::m_ServerTimeMutex);
		SoulsPerSecond = static_cast<float>(LocalController.m_TotalSouls) / Deadlock::m_ServerTime;
	}

	auto SoulsPerMinute = SoulsPerSecond * 60.0f;
	ImGui::PushFont(nullptr, 24.0f);
	ImGui::SetCursorPos({ 2.0f, m_ScreenSize.y - ImGui::GetTextLineHeight() });
	ImGui::Text("%.1f Souls/Min", SoulsPerMinute);
	ImGui::PopFont();
}