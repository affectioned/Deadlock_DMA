#pragma once

class CKeybind
{
public:
	std::string m_Name{};
	uint32_t m_Key{ 0 };
	bool m_bTargetPC{ false };
	bool m_bRadarPC{ false };
	bool m_bWaitingForKey{ false };
	// Local-only keybinds (e.g. the GUI menu toggle) are read from the source PC
	// running this app and have no Target/Radar variants. Render hides the PC
	// columns for these rows.
	bool m_bLocalOnly{ false };

public:
	void Render();
	const bool IsActive(DMA_Connection* Conn) const;
	const char* GetKeyName(uint32_t vkCode);
};

class Keybinds
{
public:
	static void Render();
	static void OnDMAFrame(DMA_Connection* Conn);

public:
	static inline bool bSettings{ true };
	static inline CKeybind Debug  = { "Debug",  VK_F12,      true,  true,  false, false };
	static inline CKeybind AimAssist = { "Aim Assist", VK_XBUTTON2, true,  true,  false, false };
	static inline CKeybind Menu   = { "Menu",   VK_INSERT,   false, false, false, true  };
};