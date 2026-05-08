#pragma once

class MainMenu
{
public:
	static void Render();
	static inline bool bVSync{ true };
	static inline int iTargetFPS{ 144 };

	// Persisted window geometry. Captured each frame from ImGui::GetWindowPos /
	// GetWindowSize and saved via Config so the menu opens where the user left
	// it across sessions. Defaults pin it near the top-left at a sensible size.
	static inline ImVec2 WindowPos{ 60.0f, 60.0f };
	static inline ImVec2 WindowSize{ 600.0f, 480.0f };
};