#include "pch.h"
#include "KeyboardPump.h"
#include "GUI/Main Menu/Main Menu.h"
#include "GUI/Keybinds/Keybinds.h"

namespace
{
	// VK → ImGuiKey. Mirrors the mapping in ImGui's Win32 backend
	// (ImGui_ImplWin32_VirtualKeyToImGuiKey, which is file-static).
	ImGuiKey VkToImGuiKey(int vk)
	{
		if (vk >= 'A' && vk <= 'Z') return (ImGuiKey)(ImGuiKey_A + (vk - 'A'));
		if (vk >= '0' && vk <= '9') return (ImGuiKey)(ImGuiKey_0 + (vk - '0'));
		if (vk >= VK_F1 && vk <= VK_F24) return (ImGuiKey)(ImGuiKey_F1 + (vk - VK_F1));
		if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return (ImGuiKey)(ImGuiKey_Keypad0 + (vk - VK_NUMPAD0));
		switch (vk)
		{
		case VK_TAB:        return ImGuiKey_Tab;
		case VK_LEFT:       return ImGuiKey_LeftArrow;
		case VK_RIGHT:      return ImGuiKey_RightArrow;
		case VK_UP:         return ImGuiKey_UpArrow;
		case VK_DOWN:       return ImGuiKey_DownArrow;
		case VK_PRIOR:      return ImGuiKey_PageUp;
		case VK_NEXT:       return ImGuiKey_PageDown;
		case VK_HOME:       return ImGuiKey_Home;
		case VK_END:        return ImGuiKey_End;
		case VK_INSERT:     return ImGuiKey_Insert;
		case VK_DELETE:     return ImGuiKey_Delete;
		case VK_BACK:       return ImGuiKey_Backspace;
		case VK_SPACE:      return ImGuiKey_Space;
		case VK_RETURN:     return ImGuiKey_Enter;
		case VK_ESCAPE:     return ImGuiKey_Escape;
		case VK_OEM_7:      return ImGuiKey_Apostrophe;
		case VK_OEM_COMMA:  return ImGuiKey_Comma;
		case VK_OEM_MINUS:  return ImGuiKey_Minus;
		case VK_OEM_PERIOD: return ImGuiKey_Period;
		case VK_OEM_2:      return ImGuiKey_Slash;
		case VK_OEM_1:      return ImGuiKey_Semicolon;
		case VK_OEM_PLUS:   return ImGuiKey_Equal;
		case VK_OEM_4:      return ImGuiKey_LeftBracket;
		case VK_OEM_5:      return ImGuiKey_Backslash;
		case VK_OEM_6:      return ImGuiKey_RightBracket;
		case VK_OEM_3:      return ImGuiKey_GraveAccent;
		case VK_CAPITAL:    return ImGuiKey_CapsLock;
		case VK_SCROLL:     return ImGuiKey_ScrollLock;
		case VK_NUMLOCK:    return ImGuiKey_NumLock;
		case VK_SNAPSHOT:   return ImGuiKey_PrintScreen;
		case VK_PAUSE:      return ImGuiKey_Pause;
		case VK_LSHIFT:     return ImGuiKey_LeftShift;
		case VK_RSHIFT:     return ImGuiKey_RightShift;
		case VK_LCONTROL:   return ImGuiKey_LeftCtrl;
		case VK_RCONTROL:   return ImGuiKey_RightCtrl;
		case VK_LMENU:      return ImGuiKey_LeftAlt;
		case VK_RMENU:      return ImGuiKey_RightAlt;
		case VK_LWIN:       return ImGuiKey_LeftSuper;
		case VK_RWIN:       return ImGuiKey_RightSuper;
		case VK_APPS:       return ImGuiKey_Menu;
		case VK_ADD:        return ImGuiKey_KeypadAdd;
		case VK_SUBTRACT:   return ImGuiKey_KeypadSubtract;
		case VK_MULTIPLY:   return ImGuiKey_KeypadMultiply;
		case VK_DIVIDE:     return ImGuiKey_KeypadDivide;
		case VK_DECIMAL:    return ImGuiKey_KeypadDecimal;
		}
		return ImGuiKey_None;
	}

	// Skip mouse buttons, the menu toggle key, and unmapped VK 0.
	bool ShouldSkipVk(int vk)
	{
		if (vk == 0) return true;
		if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON
			|| vk == VK_XBUTTON1 || vk == VK_XBUTTON2)
			return true;
		if ((uint32_t)vk == Keybinds::Menu.m_Key) return true;
		return false;
	}
}

namespace GuiInput
{
	void PumpKeyboard()
	{
		static BYTE sPrev[256] = {};
		static bool sMenuWasOpen = false;

		BYTE cur[256];
		for (int vk = 0; vk < 256; ++vk)
			cur[vk] = (::GetAsyncKeyState(vk) & 0x8000) ? 0x80 : 0;

		// While the menu is closed we don't want game input bleeding into ImGui.
		// Clearing the open-flag here ensures the next open captures a fresh baseline.
		if (!MainMenu::bOpen)
		{
			sMenuWasOpen = false;
			return;
		}

		// First frame after menu opens: snapshot baseline, emit nothing. Otherwise
		// keys that happened to be held when the user pressed the toggle would
		// register as fresh key-down events for ImGui.
		if (!sMenuWasOpen)
		{
			memcpy(sPrev, cur, sizeof(sPrev));
			sMenuWasOpen = true;
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		// Modifier meta-keys. ImGui tracks these alongside the physical key events.
		io.AddKeyEvent(ImGuiMod_Ctrl,  cur[VK_CONTROL] != 0);
		io.AddKeyEvent(ImGuiMod_Shift, cur[VK_SHIFT]   != 0);
		io.AddKeyEvent(ImGuiMod_Alt,   cur[VK_MENU]    != 0);
		io.AddKeyEvent(ImGuiMod_Super, (cur[VK_LWIN] | cur[VK_RWIN]) != 0);

		const HKL layout = ::GetKeyboardLayout(::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr));

		for (int vk = 1; vk < 256; ++vk)
		{
			if (cur[vk] == sPrev[vk]) continue;
			if (ShouldSkipVk(vk)) { sPrev[vk] = cur[vk]; continue; }

			const bool down = cur[vk] != 0;

			if (ImGuiKey key = VkToImGuiKey(vk); key != ImGuiKey_None)
				io.AddKeyEvent(key, down);

			// Translate to printable character on key-down. ToUnicodeEx walks the
			// foreground window's keyboard layout so dead keys / shifted symbols
			// resolve correctly. Filter out control codes (0x00-0x1F, 0x7F) so
			// Ctrl+A doesn't emit a stray 0x01 into InputText.
			if (down)
			{
				WCHAR buf[8] = {};
				const UINT scan = ::MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, layout);
				const int n = ::ToUnicodeEx(vk, scan, cur, buf, _countof(buf), 0, layout);
				for (int i = 0; i < n; ++i)
				{
					const WCHAR c = buf[i];
					if (c >= 0x20 && c != 0x7F)
						io.AddInputCharacter((unsigned int)c);
				}
			}

			sPrev[vk] = cur[vk];
		}
	}
}
