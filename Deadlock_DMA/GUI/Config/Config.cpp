#include "pch.h"

#include "Config.h"

#include "GUI/Settings/Settings.h"
#include "GUI/Theme/Theme.h"

#include <shlobj.h>
#include <fstream>

namespace
{
	// Tracks the last successfully loaded or saved config so SaveActive() (called
	// on exit) writes back to the same file the user is working with, instead of
	// always clobbering "default".
	std::string s_ActiveConfig = "default";
}

std::string Config::getConfigDir() {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
		std::filesystem::path appDataPath(path);
		appDataPath /= "DEADLOCK-DMA";
		appDataPath /= "Configs";
		if (!std::filesystem::exists(appDataPath)) {
			std::filesystem::create_directories(appDataPath);
		}
		return appDataPath.string();
	}

	std::filesystem::path fallback("Configs");
	if (!std::filesystem::exists(fallback))
		std::filesystem::create_directory(fallback);
	return fallback.string();
}

std::string Config::getConfigPath(const std::string& configName) {
	std::filesystem::path p = getConfigDir();
	p /= configName + ".json";
	return p.string();
}

void Config::RefreshConfigFilesList(std::vector<std::string>& outList) {
	outList.clear();
	try {
		for (const auto& entry : std::filesystem::directory_iterator(getConfigDir())) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				outList.push_back(entry.path().stem().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		Log::Warn("[Config] Error listing config files: {}", e.what());
	}
}

void Config::Render()
{
	static char configNameBuf[128] = "default";
	static int selectedConfig = -1;
	static std::vector<std::string> configFiles;
	static bool bFirstRun = true;

	if (bFirstRun)
	{
		RefreshConfigFilesList(configFiles);
		bFirstRun = false;
	}

	ImGui::BeginChild("##ConfigActions", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), true);

	ImGui::SeparatorText("Config Name");
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##ConfigName", "Enter config name...", configNameBuf, IM_ARRAYSIZE(configNameBuf));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Save/Load/Delete buttons
	ImGui::SeparatorText("Actions");

	ImVec2 buttonSize(ImGui::GetContentRegionAvail().x, 0);

	if (ImGui::Button("Save Config", buttonSize))
	{
		if (strlen(configNameBuf) > 0)
		{
			SaveConfig(configNameBuf);
			RefreshConfigFilesList(configFiles);
		}
	}

	if (ImGui::Button("Load Config", buttonSize))
	{
		if (strlen(configNameBuf) > 0)
		{
			LoadConfig(configNameBuf);
		}
	}

	Theme::PushDangerButton();

	bool canDelete = selectedConfig >= 0 && selectedConfig < static_cast<int>(configFiles.size());
	if (!canDelete) ImGui::BeginDisabled();

	if (ImGui::Button("Delete Config", buttonSize))
	{
		std::string fileToDelete = Config::getConfigPath(configFiles[selectedConfig]);
		if (std::filesystem::exists(fileToDelete))
		{
			std::error_code ec;
			std::filesystem::remove(fileToDelete, ec);
			if (!ec)
			{
				selectedConfig = -1;
				strncpy_s(configNameBuf, sizeof(configNameBuf), "default", _TRUNCATE);
				RefreshConfigFilesList(configFiles);
			}
		}
	}

	if (!canDelete) ImGui::EndDisabled();
	ImGui::PopStyleColor(3);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Utilities
	ImGui::SeparatorText("Utilities");

	if (ImGui::Button("Open Config Folder", buttonSize))
	{
		ShellExecuteA(nullptr, "open", getConfigDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	if (ImGui::Button("Refresh List", buttonSize))
	{
		RefreshConfigFilesList(configFiles);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Info
	ImGui::SeparatorText("Info");
	ImGui::TextDisabled("Total configs: %zu", configFiles.size());
	if (selectedConfig >= 0)
	{
		ImGui::TextDisabled("Selected: %s", configFiles[selectedConfig].c_str());
	}

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##ConfigList", ImVec2(0, 0), true);

	ImGui::SeparatorText("Available Configs");

	if (!configFiles.empty()) {
		for (size_t i = 0; i < configFiles.size(); ++i)
		{
			bool isSelected = (selectedConfig == static_cast<int>(i));

			if (ImGui::Selectable(configFiles[i].c_str(), isSelected))
			{
				selectedConfig = static_cast<int>(i);
				strncpy_s(configNameBuf, sizeof(configNameBuf), configFiles[i].c_str(), _TRUNCATE);
			}

			// Double-click to load
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				LoadConfig(configFiles[i]);
			}
		}
	}

	ImGui::EndChild();
}

void Config::SaveConfig(const std::string& configName) {
	Log::Info("[Config] Saving config: {}", configName);
	json j = Settings::ToJson();
	std::ofstream file(getConfigPath(configName));
	if (!file.is_open())
		return;
	file << std::setw(4) << j;
	file.close();
	s_ActiveConfig = configName;
}

bool Config::LoadConfig(const std::string& configName) {
	Log::Info("[Config] Loading config: {}", configName);
	std::ifstream file(getConfigPath(configName));
	if (!file.is_open())
	{
		Log::Warn("[Config] Failed to open config file: {}", getConfigPath(configName));
		return false;
	}
	json j;
	try {
		file >> j;
	}
	catch (...) {
		file.close();
		return false;
	}
	file.close();
	Settings::FromJson(j);
	s_ActiveConfig = configName;
	return true;
}

void Config::SaveActive() {
	SaveConfig(s_ActiveConfig);
}