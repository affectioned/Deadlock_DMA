#pragma once

// Defaults follow Deadlock's own palette — see GUI/Theme/Theme.h for the full
// set and the reasoning. Amber and sapphire are the two team colors the game
// itself uses; everything else is picked to sit alongside them rather than
// against them. Nothing defaults to pure black: the overlay's compositing path
// treats RGB(0,0,0) as glass.
class ColorPicker
{
public:
	static void Render();
	static void MyColorPicker(const char* label, ImColor& color);

public:
	static inline bool bMasterToggle{ true };

	// Menu accent — drives Header/Button/Slider/CheckMark/etc. via Theme::Apply.
	static inline ImColor MenuAccent{ 0.83f, 0.53f, 0.05f, 1.0f };  // amber #D4870C

	static inline ImColor SinnersColor{ 0.70f, 0.30f, 0.80f, 1.0f };
	static inline ImColor BossColor{ 0.91f, 0.66f, 0.24f, 1.0f };    // brass-bright
	static inline ImColor XpOrbColor{ 0.50f, 0.69f, 0.41f, 1.0f };   // soul green

	// Breakable crates / on-map pickups. PowerupColor is the fallback for any
	// variant that isn't one of the specifically-colored types.
	static inline ImColor PowerupColor      { 0.69f, 0.55f, 0.34f, 1.0f }; // brass
	static inline ImColor PowerupSoulsColor { 0.50f, 0.69f, 0.41f, 1.0f };
	static inline ImColor PowerupHealthColor{ 0.35f, 0.78f, 0.45f, 1.0f };
	static inline ImColor PowerupNecroColor { 0.55f, 0.30f, 0.75f, 1.0f };
	// Labels resolved from in_world_item_panel's m_hTrackedEntity. "Item"
	// is the fallback when the tracked class name doesn't match any of
	// the friendly buckets in FriendlyLabelForClass().
	static inline ImColor PowerupIdolColor  { 0.91f, 0.66f, 0.24f, 1.0f };
	static inline ImColor PowerupRejuvColor { 0.75f, 0.22f, 0.17f, 1.0f };
	static inline ImColor PowerupXpColor    { 0.83f, 0.53f, 0.05f, 1.0f };
	static inline ImColor PowerupItemColor  { 0.31f, 0.46f, 0.77f, 1.0f }; // sapphire
	// Unresolved / not-yet-classified panels. Kept distinct from PowerupItemColor
	// so it's obvious when panel labels aren't being resolved.
	static inline ImColor PowerupPanelColor { 0.61f, 0.56f, 0.48f, 1.0f };
	static inline ImColor MonsterCampColor{ 0.69f, 0.55f, 0.34f, 1.0f };
	static inline ImColor LocalPlayerRadar{ 0.50f, 0.69f, 0.41f, 1.0f };
	static inline ImColor UnsecuredSoulsTextColor{ 0.91f, 0.86f, 0.78f, 1.0f };   // cream
	static inline ImColor UnsecuredSoulsHighlightedTextColor{ 0.91f, 0.66f, 0.24f, 1.0f };
	static inline ImColor FriendlyHealthStatusBarColor{ 0.50f, 0.69f, 0.41f, 1.0f };
	static inline ImColor EnemyHealthStatusBarColor{ 0.75f, 0.22f, 0.17f, 1.0f };
	static inline ImColor FriendlySoulsStatusBarColor{ 0.31f, 0.46f, 0.77f, 1.0f };
	static inline ImColor EnemySoulsStatusBarColor{ 0.83f, 0.53f, 0.05f, 1.0f };
	// Only used when Draw_Players::bHealthGradient is off; with it on the fill is
	// interpolated green → amber → red from remaining health.
	static inline ImColor HealthBarForegroundColor{ 0.50f, 0.69f, 0.41f, 1.0f };
	static inline ImColor HealthBarBackgroundColor{ 0.13f, 0.11f, 0.08f, 0.85f };
	static inline ImColor AimAssistFOVCircle{ 0.55f, 0.42f, 0.25f, 1.0f };
	static inline ImColor AimAssistFOVCircleActive{ 0.83f, 0.53f, 0.05f, 1.0f };
	static inline ImColor RadarBackgroundColor{ 0.09f, 0.07f, 0.05f, 0.92f };

public:
	static inline ImColor ArchMotherTeamColor{ 78, 118, 196 };   // sapphire #4E76C4
	static inline ImColor HiddenKingTeamColor{ 212, 135, 12 };    // amber #D4870C
	static inline ImColor SkeletonColorVisible{ 232, 220, 200 };  // cream
	static inline ImColor SkeletonColorInvisible{ 176, 141, 87 }; // brass
	// Box mirrors the skeleton's visibility split: friendlies + visible enemies
	// draw the Visible variant, enemies gated by FOW draw the Invisible variant.
	// Team coloring is still available on the nametag / health bar / radar, so
	// dropping it here doesn't lose the friendly-vs-enemy signal.
	static inline ImColor BoxColorVisible{ 232, 220, 200 };
	static inline ImColor BoxColorInvisible{ 176, 141, 87 };
};
