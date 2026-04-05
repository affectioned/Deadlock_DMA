#pragma once

class Fuser
{
public:
	static void Render();
	static void RenderContent();
	static void RenderSoulsPerMinute();

public:
	static inline bool bSettings{ true };
	static inline bool bMasterToggle{ true };
	static inline bool bDrawSoulsPerMinute{ true };
	static inline ImVec2 m_ScreenSize{ 1920.0f, 1080.0f };
	static inline ImVec2 m_MonitorPos{ 0.0f, 0.0f };
	static inline int m_MonitorIndex{ 0 };
	static inline bool bAutoDetectResolution{ true };
};