#pragma once
#include "Deadlock/Const/PatronTeam.hpp"

class ColorPicker
{
public:
	static void Render();
	static void MyColorPicker(const char* label, ImColor& color);
	static ImU32 GetColorByPatron(PatronTeam team);

public:
	static inline bool bMasterToggle{ true };

	// Team-specific colors - HiddenKing (Team 2) - #ffac11
	static inline ImColor HiddenKingNameTagColor{ 1.0f, 0.674f, 0.067f, 1.0f };
	static inline ImColor HiddenKingBoneColor{ 1.0f, 0.674f, 0.067f, 1.0f };
	static inline ImColor HiddenKingRadarColor{ 1.0f, 0.674f, 0.067f, 1.0f };
	static inline ImColor HiddenKingTrooperColor{ 1.0f, 0.674f, 0.067f, 1.0f };
	static inline ImColor HiddenKingHealthStatusBarColor{ 1.0f, 0.674f, 0.067f, 1.0f };
	static inline ImColor HiddenKingSoulsStatusBarColor{ 1.0f, 0.674f, 0.067f, 1.0f };

	// Team-specific colors - Archmother (Team 3) - #3874ae
	static inline ImColor ArchmotherNameTagColor{ 0.220f, 0.455f, 0.682f, 1.0f };
	static inline ImColor ArchmotherBoneColor{ 0.220f, 0.455f, 0.682f, 1.0f };
	static inline ImColor ArchmotherRadarColor{ 0.220f, 0.455f, 0.682f, 1.0f };
	static inline ImColor ArchmotherTrooperColor{ 0.220f, 0.455f, 0.682f, 1.0f };
	static inline ImColor ArchmotherHealthStatusBarColor{ 0.220f, 0.455f, 0.682f, 1.0f };
	static inline ImColor ArchmotherSoulsStatusBarColor{ 0.220f, 0.455f, 0.682f, 1.0f };

	// Neutral/shared colors
	static inline ImColor SinnersColor{ 0.65f, 0.05f, 0.7f, 1.0f };
	static inline ImColor MonsterCampColor{ 0.8f, 0.8f, 0.8f, 1.0f };
	static inline ImColor UnsecuredSoulsTextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	static inline ImColor UnsecuredSoulsHighlightedTextColor{ 1.0f, 1.0f, 0.0f, 1.0f };
	static inline ImColor HealthBarForegroundColor{ 0.0f, 0.8f, 0.0f, 1.0f };
	static inline ImColor HealthBarBackgroundColor{ 0.2f, 0.2f, 0.2f, 1.0f };
	static inline ImColor AimbotFOVCircle{ 1.0f, 0.0f, 0.0f, 1.0f };
	static inline ImColor AimbotFOVCircleActive{ 0.0f, 1.0f, 0.0f, 1.0f };
	static inline ImColor RadarBackgroundColor{ 0.0f, 0.0f, 0.0f, 1.0f };
};