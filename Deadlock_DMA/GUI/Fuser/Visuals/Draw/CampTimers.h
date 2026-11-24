#pragma once

// Neutral camp and Mid Boss respawn countdowns, from the timers the server
// networks on C_CitadelGameRules. The game only draws its own in-world timer
// within m_flNeutralCampRespawnTimerShowDistance of the camp; these show every
// camp's, at any range, and on the radar.
class Draw_CampTimers
{
public:
	static void operator()();

	// "1:05" / "42s". Shared with the radar.
	static std::string FormatCountdown(float seconds);

public:
	static inline bool bMasterToggle{ true };
	static inline bool bWorld{ true };
	static inline bool bRadar{ true };
	// Top-of-screen Mid Boss countdown, readable without finding the pit.
	static inline bool bMidBossBanner{ true };
	// World tags past this are skipped. 0 disables the cull — the timers are
	// few enough that showing all of them is reasonable.
	static inline float fMaxDistanceM{ 0.0f };

private:
	static void DrawMidBossBanner();
};
