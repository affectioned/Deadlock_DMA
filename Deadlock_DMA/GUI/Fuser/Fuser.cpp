#include "pch.h"
#include "Fuser.h"
#include "ESP/ESP.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Status Bars/Status Bars.h"
#include "GUI/Aimbot/Aimbot.h"

void Fuser::Render()
{
	if (!bMasterToggle) return;

	// Position at absolute screen coordinates so ViewportsEnable gives the
	// Fuser its own OS window, completely separate from the menu window.
	ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
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

void Fuser::RenderSettings()
{
	if (!bSettings) return;

	ImGui::Begin("Fuser Settings", &bSettings);

	ImGui::Checkbox("Enable Fuser", &bMasterToggle);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();


	ImGui::SeparatorText("Screen Configuration");
	ImGui::Text("Overlay Resolution");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Width##Screen", &m_ScreenSize.x, 0.0f, 0.0f, "%.0f");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputFloat("Height##Screen", &m_ScreenSize.y, 0.0f, 0.0f, "%.0f");
	ImGui::TextDisabled("(Match your game resolution)");

	ImGui::Spacing();

	ImGui::SeparatorText("HUD Elements");
	ImGui::Checkbox("Souls Per Minute", &bDrawSoulsPerMinute);

	ImGui::Checkbox("Team Health Bar", &StatusBars::bRenderTeamHealthBar);
	ImGui::Checkbox("Team Souls Bar", &StatusBars::bRenderTeamSoulsBar);
	ImGui::Checkbox("Unspent Souls Bar", &StatusBars::bRenderUnspentSoulsBar);

	ImGui::End();
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