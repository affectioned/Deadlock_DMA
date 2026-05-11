#pragma once

class MainMenu
{
public:
	static void Render();
	static inline bool bVSync{ true };
	static inline int iTargetFPS{ 144 };

	// Persisted window geometry. Captured each frame from ImGui::GetWindowPos /
	// GetWindowSize and saved via Config so the menu opens where the user left
	// it across sessions. The {-1,-1} sentinel means "never positioned yet" and
	// triggers a one-time screen-center on first show.
	static inline ImVec2 WindowPos{ -1.0f, -1.0f };
	static inline ImVec2 WindowSize{ 600.0f, 480.0f };

	// Visibility + click-through. When closed, the layered overlay flips to
	// WS_EX_TRANSPARENT so mouse input passes through to the game. The toggle
	// key itself lives on Keybinds::Menu so the user can rebind it from the
	// Keybinds tab and have it persist via Config like every other binding.
	static inline bool bOpen{ true };
};