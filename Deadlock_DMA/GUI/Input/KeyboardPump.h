#pragma once

namespace GuiInput
{
	// Polls keyboard state and feeds key + character events into ImGui's IO.
	// Required because the overlay window uses WS_EX_NOACTIVATE, so it never
	// receives WM_KEYDOWN/WM_CHAR via the message pump. Call once per frame
	// before ImGui::NewFrame().
	//
	// Gated on MainMenu::bOpen so closed-menu state doesn't leak game keystrokes
	// into ImGui. On the open transition, current key state is captured as the
	// baseline so we don't emit key-down for keys that were already held.
	void PumpKeyboard();
}
