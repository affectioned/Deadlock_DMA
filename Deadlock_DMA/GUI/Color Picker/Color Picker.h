#pragma once

class ColorPicker
{
public:
	static void Render();
	static void MyColorPicker(const char* label, ImColor& color);

public:
	static inline bool bMasterToggle{ true };

	// Menu accent — drives Header/Button/Slider/CheckMark/etc. in the main menu.
	static inline ImColor MenuAccent{ 0.20f, 0.55f, 0.95f, 1.0f };

	static inline ImColor SinnersColor{ 0.65f,0.05f,0.7f,1.0f };
	static inline ImColor BossColor{ 1.0f,0.4f,0.0f,1.0f };
	static inline ImColor XpOrbColor{ 1.0f,0.85f,0.0f,1.0f };

	// Breakable crates / on-map pickups. PowerupColor is the fallback for any
	// variant that isn't one of the four specifically-colored types.
	static inline ImColor PowerupColor      { 0.85f,0.30f,0.85f,1.0f };
	static inline ImColor PowerupSoulsColor { 1.00f,0.85f,0.10f,1.0f };
	static inline ImColor PowerupHealthColor{ 0.20f,1.00f,0.20f,1.0f };
	static inline ImColor PowerupNecroColor { 0.55f,0.20f,0.85f,1.0f };
	// Labels resolved from in_world_item_panel's m_hTrackedEntity. "Item"
	// is the fallback when the tracked class name doesn't match any of
	// the friendly buckets in FriendlyLabelForClass().
	static inline ImColor PowerupIdolColor  { 1.00f,0.55f,0.10f,1.0f };
	static inline ImColor PowerupRejuvColor { 1.00f,0.20f,0.35f,1.0f };
	static inline ImColor PowerupXpColor    { 1.00f,0.85f,0.00f,1.0f };
	static inline ImColor PowerupItemColor  { 0.10f,0.65f,1.00f,1.0f };
	// Unresolved / not-yet-classified panels. Kept distinct from PowerupItemColor
	// so it's obvious when panel labels aren't being resolved.
	static inline ImColor PowerupPanelColor { 0.55f,0.55f,0.55f,1.0f };
	static inline ImColor MonsterCampColor{ 0.8f,0.8f,0.8f,1.0f };
	static inline ImColor LocalPlayerRadar{ 0.0f,1.0f,0.0f,1.0f };
	static inline ImColor UnsecuredSoulsTextColor{ 1.0f,1.0f,1.0f,1.0f };
	static inline ImColor UnsecuredSoulsHighlightedTextColor{ 1.0f,1.0f,0.0f,1.0f };
	static inline ImColor FriendlyHealthStatusBarColor{ 0.0f,0.8f,0.0f,1.0f };
	static inline ImColor EnemyHealthStatusBarColor{ 0.8f,0.0f,0.0f,1.0f };
	static inline ImColor FriendlySoulsStatusBarColor{ 0.0f,0.5f,1.0f,1.0f };
	static inline ImColor EnemySoulsStatusBarColor{ 1.0f,0.5f,0.0f,1.0f };
	static inline ImColor HealthBarForegroundColor{ 0.0f,0.8f,0.0f,1.0f };
	static inline ImColor HealthBarBackgroundColor{ 0.2f,0.2f,0.2f,1.0f };
	static inline ImColor AimAssistFOVCircle{ 1.0f,0.0f,0.0f,1.0f };
	static inline ImColor AimAssistFOVCircleActive{ 0.0f, 1.0f, 0.0f, 1.0f };
	static inline ImColor RadarBackgroundColor{ 0.0f,0.0f,0.0f,1.0f };

public:
	static inline ImColor ArchMotherTeamColor{ 78, 118, 196 };
	static inline ImColor HiddenKingTeamColor{ 212, 135, 12 };
	static inline ImColor SkeletonColorVisible{ 255,255,255 };
	static inline ImColor SkeletonColorInvisible{ 255,80,80 };
};