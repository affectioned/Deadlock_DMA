#pragma once

class Draw_Powerups
{
public:
	static void operator()();

public:
	static inline bool bMasterToggle{ true };
	static inline bool bShowLabel{ false };
	static inline bool bShowDistance{ false };
	static inline float fCircleRadius{ 5.0f };
};
