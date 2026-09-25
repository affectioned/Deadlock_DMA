#include "pch.h"

#include "Settings.h"

#include "GUI/Aim Assist/Aim Assist.h"
#include "GUI/Color Picker/Color Picker.h"
#include "GUI/Config/Config.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/Status Bars/Status Bars.h"
#include "GUI/Fuser/Visuals/Visuals.h"
#include "GUI/Fuser/Visuals/Snapshot.h"
#include "GUI/Fuser/Visuals/WorldText.h"
#include "GUI/Fuser/Visuals/Draw/Camps.h"
#include "GUI/Fuser/Visuals/Draw/Players.h"
#include "GUI/Fuser/Visuals/Draw/Powerups.h"
#include "GUI/Fuser/Visuals/Draw/Sinners.h"
#include "GUI/Fuser/Visuals/Draw/Troopers.h"
#include "GUI/Fuser/Visuals/Draw/XpOrbs.h"
#include "GUI/Keybinds/Keybinds.h"
#include "GUI/Main Menu/Main Menu.h"
#include "GUI/Main Window/Main Window.h"
#include "GUI/Radar/Radar.h"
#include "GUI/Theme/Theme.h"

#include <cctype>

using json = nlohmann::json;

namespace
{
	// Enum label tables. Order must match the enum's integer values.
	const char* const kHitboxSlots[]       = { "Head", "Neck", "Torso", "Arms", "Legs" };
	const char* const kHealthBarPositions[] = { "Top", "Bottom", "Left", "Right" };
	const char* const kBoxStyles[]          = { "Full", "Corners" };

	std::vector<Setting> s_Table;
	json                 s_Defaults;
	bool                 s_Built = false;

	// Enum classes are stored through an int* view. Every enum in the registry
	// declares `: int`, so this is a same-size, same-alignment reinterpretation.
	static_assert(sizeof(HitboxSlot) == sizeof(int));
	static_assert(sizeof(EHealthBarPosition) == sizeof(int));
	static_assert(sizeof(EBoxStyle) == sizeof(int));

	Setting B(const char* path, const char* label, bool* p)
	{
		return Setting{ path, label, SettingType::Bool, p };
	}
	Setting I(const char* path, const char* label, void* p, float lo, float hi)
	{
		return Setting{ path, label, SettingType::Int, p, lo, hi };
	}
	Setting F(const char* path, const char* label, float* p, float lo, float hi, const char* fmt = "%.2f")
	{
		return Setting{ path, label, SettingType::Float, p, lo, hi, fmt };
	}
	Setting C(const char* path, const char* label, ImColor* p)
	{
		return Setting{ path, label, SettingType::Color, p };
	}
	Setting V2(const char* path, const char* label, ImVec2* p)
	{
		return Setting{ path, label, SettingType::Vec2, p };
	}
	Setting E(const char* path, const char* label, void* p, const char* const* names, int count)
	{
		return Setting{ path, label, SettingType::Enum, p, 0.0f, 0.0f, nullptr, names, count };
	}
	Setting K(const char* path, const char* label, CKeybind* p)
	{
		return Setting{ path, label, SettingType::Key, p };
	}

	json::json_pointer Pointer(const char* dotted)
	{
		std::string s = "/";
		for (const char* c = dotted; *c; ++c)
			s += (*c == '.') ? '/' : *c;
		return json::json_pointer(s);
	}

	void Build()
	{
		s_Table = {
			// ---- Application -------------------------------------------------
			B ("MainMenu.bVSync",        "General: VSync",        &MainMenu::bVSync),
			I ("MainMenu.iTargetFPS",    "General: Target FPS",   &MainMenu::iTargetFPS, 60.0f, 240.0f),
			B ("MainMenu.bOpen",         "General: Menu Open",    &MainMenu::bOpen),
			V2("MainMenu.WindowPos",     "General: Menu Position", &MainMenu::WindowPos),
			V2("MainMenu.WindowSize",    "General: Menu Size",    &MainMenu::WindowSize),
			I ("MainMenu.MonitorIndex",  "General: Monitor",      &MainWindow::g_MonitorIndex, 0.0f, 8.0f),

			// ---- Aim assist --------------------------------------------------
			B ("AimAssist.bSettings",           "Aim Assist: Show Settings",   &AimAssist::bSettings),
			B ("AimAssist.bMasterToggle",       "Aim Assist: Enable",          &AimAssist::bMasterToggle),
			F ("AimAssist.fAlphaX",             "Aim Assist: Smoothing X",     &AimAssist::fAlphaX, 0.01f, 1.0f, "%.3f"),
			F ("AimAssist.fAlphaY",             "Aim Assist: Smoothing Y",     &AimAssist::fAlphaY, 0.01f, 1.0f, "%.3f"),
			F ("AimAssist.fGaussianNoise",      "Aim Assist: Noise",           &AimAssist::fGaussianNoise, 0.0f, 5.0f, "%.2f"),
			F ("AimAssist.fMaxPixelDistance",   "Aim Assist: FOV (pixels)",    &AimAssist::fMaxPixelDistance, 1.0f, 800.0f, "%.0f"),
			E ("AimAssist.eHitboxSlot",         "Aim Assist: Hitbox",          &AimAssist::eHitboxSlot, kHitboxSlots, IM_ARRAYSIZE(kHitboxSlots)),
			B ("AimAssist.bDrawMaxFOV",         "Aim Assist: Draw FOV Circle", &AimAssist::bDrawMaxFOV),
			B ("AimAssist.bAimAtOrbs",          "Aim Assist: Target Orbs",     &AimAssist::bAimAtOrbs),
			B ("AimAssist.bVisibleOnly",        "Aim Assist: Visible Only",    &AimAssist::bVisibleOnly),
			B ("AimAssist.bUsePrediction",      "Aim Assist: Lead Prediction", &AimAssist::bUsePrediction),
			F ("AimAssist.fManualBulletSpeedMs","Aim Assist: Bullet Speed m/s",&AimAssist::fManualBulletSpeedMs, 0.0f, 2000.0f, "%.0f"),

			// ---- Overlay host ------------------------------------------------
			B ("Fuser.bMasterToggle",       "Overlay: Enable",          &Fuser::bMasterToggle),
			V2("Fuser.ScreenSize",          "Overlay: Resolution",      &Fuser::m_ScreenSize),
			B ("Fuser.bDrawSoulsPerMinute", "Overlay: Souls Per Minute",&Fuser::bDrawSoulsPerMinute),

			B ("Fuser.Visuals.bMasterToggle", "Entities: Enable", &Visuals::bMasterToggle),

			// ---- World text --------------------------------------------------
			B ("Fuser.WorldText.bOutline",       "Text: Outline",            &WorldText::bOutline),
			B ("Fuser.WorldText.bDistanceScale", "Text: Scale With Distance",&WorldText::bDistanceScale),
			F ("Fuser.WorldText.fBaseTextSize",  "Text: Base Size",          &WorldText::fBaseTextSize, 8.0f, 32.0f, "%.0f"),
			F ("Fuser.WorldText.fMinTextSize",   "Text: Minimum Size",       &WorldText::fMinTextSize, 6.0f, 24.0f, "%.0f"),
			F ("Fuser.WorldText.fFadeStart",     "Text: Fade Start (m)",     &WorldText::fFadeStart, 0.0f, 200.0f, "%.0f"),
			F ("Fuser.WorldText.fMaxDistance",   "Text: Max Distance (m)",   &WorldText::fMaxDistance, 0.0f, 400.0f, "%.0f"),

			// ---- Extrapolation ----------------------------------------------
			B ("Fuser.Snapshot.bExtrapolate",        "ESP: Extrapolate Positions", &Snapshot::bExtrapolate),
			F ("Fuser.Snapshot.fMaxExtrapolationMs", "ESP: Max Extrapolation (ms)",&Snapshot::fMaxExtrapolationMs, 0.0f, 120.0f, "%.0f"),

			// ---- Players -----------------------------------------------------
			B ("Fuser.Draw_Players.bMasterToggle",   "Players: Enable",            &Draw_Players::bMasterToggle),
			B ("Fuser.Draw_Players.bHideFriendly",   "Players: Hide Friendly",     &Draw_Players::bHideFriendly),
			B ("Fuser.Draw_Players.bHideLocalPlayer","Players: Hide Local Player", &Draw_Players::bHideLocalPlayer),
			B ("Fuser.Draw_Players.bVisibleOnly",    "Players: Visible Only",      &Draw_Players::bVisibleOnly),
			B ("Fuser.Draw_Players.bDrawBones",      "Players: Bones",             &Draw_Players::bDrawBones),
			F ("Fuser.Draw_Players.fBonesThickness", "Players: Bones Thickness",   &Draw_Players::fBonesThickness, 0.1f, 5.0f, "%.1f"),
			B ("Fuser.Draw_Players.bDrawBox",        "Players: Box",               &Draw_Players::bDrawBox),
			F ("Fuser.Draw_Players.fBoxThickness",   "Players: Box Thickness",     &Draw_Players::fBoxThickness, 0.1f, 5.0f, "%.1f"),
			E ("Fuser.Draw_Players.eBoxStyle",       "Players: Box Style",         &Draw_Players::eBoxStyle, kBoxStyles, IM_ARRAYSIZE(kBoxStyles)),
			B ("Fuser.Draw_Players.bBoxFromBones",   "Players: Box From Bones",    &Draw_Players::bBoxFromBones),
			B ("Fuser.Draw_Players.bBoxFill",        "Players: Box Fill",          &Draw_Players::bBoxFill),
			B ("Fuser.Draw_Players.bDrawHead",       "Players: Head Circle",       &Draw_Players::bDrawHead),
			B ("Fuser.Draw_Players.bDrawVelocityVector","Players: Velocity Vector",&Draw_Players::bDrawVelocityVector),
			B ("Fuser.Draw_Players.bBoneNumbers",    "Players: Bone Numbers",      &Draw_Players::bBoneNumbers),
			B ("Fuser.Draw_Players.bDrawHealthBar",  "Players: Health Bar",        &Draw_Players::bDrawHealthBar),
			E ("Fuser.Draw_Players.eHealthBarPosition","Players: Health Bar Position",&Draw_Players::eHealthBarPosition, kHealthBarPositions, IM_ARRAYSIZE(kHealthBarPositions)),
			B ("Fuser.Draw_Players.bHealthGradient", "Players: Health Gradient",   &Draw_Players::bHealthGradient),
			B ("Fuser.Draw_Players.bDrawUnsecuredSouls","Players: Unsecured Souls",&Draw_Players::bDrawUnsecuredSouls),
			I ("Fuser.Draw_Players.UnsecuredSoulsMinimumThreshold",  "Players: Souls Minimum",  &Draw_Players::UnsecuredSoulsMinimumThreshold, 0.0f, 5000.0f),
			I ("Fuser.Draw_Players.UnsecuredSoulsHighlightThreshold","Players: Souls Highlight",&Draw_Players::UnsecuredSoulsHighlightThreshold, 0.0f, 5000.0f),
			B ("Fuser.Draw_Players.bShowDistance",   "Players: Show Distance",     &Draw_Players::bShowDistance),
			B ("Fuser.Draw_Players.bShowHeroLevel",  "Players: Hero Level",        &Draw_Players::bShowHeroLevel),
			B ("Fuser.Draw_Players.bShowRespawnTimer","Players: Respawn Timer",    &Draw_Players::bShowRespawnTimer),

			// ---- Troopers ----------------------------------------------------
			B ("Fuser.Draw_Troopers.bMasterToggle",    "Troopers: Enable",         &Draw_Troopers::bMasterToggle),
			B ("Fuser.Draw_Troopers.bHideFriendly",    "Troopers: Hide Friendly",  &Draw_Troopers::bHideFriendly),
			B ("Fuser.Draw_Troopers.bDrawLaneTroopers","Troopers: Lane Troopers",  &Draw_Troopers::bDrawLaneTroopers),
			B ("Fuser.Draw_Troopers.bDrawWalkers",     "Troopers: Walkers",        &Draw_Troopers::bDrawWalkers),
			B ("Fuser.Draw_Troopers.bDrawNeutrals",    "Troopers: Jungle Neutrals",&Draw_Troopers::bDrawNeutrals),

			// ---- World entities ----------------------------------------------
			B ("Fuser.Draw_Camps.bMasterToggle",   "World: Bosses",  &Draw_Camps::bMasterToggle),
			B ("Fuser.Draw_Sinners.bMasterToggle", "World: Sinners", &Draw_Sinners::bMasterToggle),
			B ("Fuser.Draw_XpOrbs.bMasterToggle",  "World: XP Orbs", &Draw_XpOrbs::bMasterToggle),

			B ("Fuser.Draw_Powerups.bMasterToggle", "Powerups: Enable",        &Draw_Powerups::bMasterToggle),
			B ("Fuser.Draw_Powerups.bShowLabel",    "Powerups: Show Label",    &Draw_Powerups::bShowLabel),
			B ("Fuser.Draw_Powerups.bShowDistance", "Powerups: Show Distance", &Draw_Powerups::bShowDistance),
			F ("Fuser.Draw_Powerups.fCircleRadius", "Powerups: Circle Radius", &Draw_Powerups::fCircleRadius, 1.0f, 12.0f, "%.1f"),

			// ---- Status bars -------------------------------------------------
			B ("Fuser.StatusBars.bMasterToggle",         "Status Bars: Enable",       &StatusBars::bMasterToggle),
			B ("Fuser.StatusBars.bRenderTeamHealthBar",  "Status Bars: Team Health",  &StatusBars::bRenderTeamHealthBar),
			B ("Fuser.StatusBars.bRenderTeamSoulsBar",   "Status Bars: Team Souls",   &StatusBars::bRenderTeamSoulsBar),
			B ("Fuser.StatusBars.bRenderUnspentSoulsBar","Status Bars: Unspent Souls",&StatusBars::bRenderUnspentSoulsBar),
			F ("Fuser.StatusBars.BarHeight",             "Status Bars: Height",       &StatusBars::BarHeight, 4.0f, 60.0f, "%.0f"),

			// ---- Radar -------------------------------------------------------
			B ("Radar.bMasterToggle",   "Radar: Enable",         &Radar::bMasterToggle),
			B ("Radar.bHideFriendly",   "Radar: Hide Friendly",  &Radar::bHideFriendly),
			B ("Radar.bMobaStyle",      "Radar: MOBA Style",     &Radar::bMobaStyle),
			B ("Radar.bPlayerCentered", "Radar: Player Centered",&Radar::bPlayerCentered),
			F ("Radar.fRadarScale",     "Radar: Scale",          &Radar::fRadarScale, 1.0f, 40.0f, "%.1f"),
			F ("Radar.fRaySize",        "Radar: View Ray Size",  &Radar::fRaySize, 10.0f, 400.0f, "%.0f"),

			// ---- Colors ------------------------------------------------------
			C ("ColorPicker.MenuAccent",              "Color: Menu Accent",       &ColorPicker::MenuAccent),
			C ("ColorPicker.SinnersColor",             "Color: Sinner's Sacrifice",&ColorPicker::SinnersColor),
			C ("ColorPicker.BossColor",                "Color: Boss",              &ColorPicker::BossColor),
			C ("ColorPicker.XpOrbColor",               "Color: XP Orb",            &ColorPicker::XpOrbColor),
			C ("ColorPicker.PowerupColor",             "Color: Powerup (Default)", &ColorPicker::PowerupColor),
			C ("ColorPicker.PowerupSoulsColor",        "Color: Powerup Souls",     &ColorPicker::PowerupSoulsColor),
			C ("ColorPicker.PowerupHealthColor",       "Color: Powerup Health",    &ColorPicker::PowerupHealthColor),
			C ("ColorPicker.PowerupNecroColor",        "Color: Powerup Necro",     &ColorPicker::PowerupNecroColor),
			C ("ColorPicker.PowerupIdolColor",         "Color: Powerup Idol",      &ColorPicker::PowerupIdolColor),
			C ("ColorPicker.PowerupRejuvColor",        "Color: Powerup Rejuv",     &ColorPicker::PowerupRejuvColor),
			C ("ColorPicker.PowerupXpColor",           "Color: Powerup XP",        &ColorPicker::PowerupXpColor),
			C ("ColorPicker.PowerupItemColor",         "Color: Powerup Item",      &ColorPicker::PowerupItemColor),
			C ("ColorPicker.PowerupPanelColor",        "Color: Powerup Panel",     &ColorPicker::PowerupPanelColor),
			C ("ColorPicker.MonsterCampColor",         "Color: Monster Camp",      &ColorPicker::MonsterCampColor),
			C ("ColorPicker.LocalPlayerRadar",         "Color: Local Player (Radar)",&ColorPicker::LocalPlayerRadar),
			C ("ColorPicker.ArchMotherTeamColor",      "Color: Sapphire Team",     &ColorPicker::ArchMotherTeamColor),
			C ("ColorPicker.HiddenKingTeamColor",      "Color: Amber Team",        &ColorPicker::HiddenKingTeamColor),
			C ("ColorPicker.BoxColorVisible",          "Color: Box (Visible)",     &ColorPicker::BoxColorVisible),
			C ("ColorPicker.BoxColorInvisible",        "Color: Box (Invisible)",   &ColorPicker::BoxColorInvisible),
			C ("ColorPicker.SkeletonColorVisible",     "Color: Skeleton (Visible)",&ColorPicker::SkeletonColorVisible),
			C ("ColorPicker.SkeletonColorInvisible",   "Color: Skeleton (Invisible)",&ColorPicker::SkeletonColorInvisible),
			C ("ColorPicker.UnsecuredSoulsTextColor",  "Color: Unsecured Souls",   &ColorPicker::UnsecuredSoulsTextColor),
			C ("ColorPicker.UnsecuredSoulsHighlightedTextColor","Color: Unsecured Souls (High)",&ColorPicker::UnsecuredSoulsHighlightedTextColor),
			C ("ColorPicker.FriendlyHealthStatusBarColor","Color: Friendly Health Bar",&ColorPicker::FriendlyHealthStatusBarColor),
			C ("ColorPicker.EnemyHealthStatusBarColor",  "Color: Enemy Health Bar",  &ColorPicker::EnemyHealthStatusBarColor),
			C ("ColorPicker.FriendlySoulsStatusBarColor","Color: Friendly Souls Bar",&ColorPicker::FriendlySoulsStatusBarColor),
			C ("ColorPicker.EnemySoulsStatusBarColor",   "Color: Enemy Souls Bar",   &ColorPicker::EnemySoulsStatusBarColor),
			C ("ColorPicker.HealthBarForegroundColor",   "Color: Health Bar Fill",   &ColorPicker::HealthBarForegroundColor),
			C ("ColorPicker.HealthBarBackgroundColor",   "Color: Health Bar Back",   &ColorPicker::HealthBarBackgroundColor),
			C ("ColorPicker.AimAssistFOVCircle",         "Color: FOV Circle",        &ColorPicker::AimAssistFOVCircle),
			C ("ColorPicker.AimAssistFOVCircleActive",   "Color: FOV Circle Active", &ColorPicker::AimAssistFOVCircleActive),
			C ("ColorPicker.RadarBackgroundColor",       "Color: Radar Background",  &ColorPicker::RadarBackgroundColor),

			// ---- Keybinds ----------------------------------------------------
			B ("Keybinds.bSettings",  "Keybinds: Show Settings", &Keybinds::bSettings),
			K ("Keybinds.Debug",      "Keybind: Debug",          &Keybinds::Debug),
			K ("Keybinds.AimAssist",  "Keybind: Aim Assist",     &Keybinds::AimAssist),
			K ("Keybinds.Menu",       "Keybind: Menu",           &Keybinds::Menu),
		};
	}

	json Serialize(const std::vector<Setting>& table)
	{
		json j;

		for (const Setting& s : table)
		{
			const auto ptr = Pointer(s.path);

			switch (s.type)
			{
			case SettingType::Bool:  j[ptr] = *static_cast<bool*>(s.ptr);  break;
			case SettingType::Int:   j[ptr] = *static_cast<int*>(s.ptr);   break;
			case SettingType::Float: j[ptr] = *static_cast<float*>(s.ptr); break;
			case SettingType::Enum:  j[ptr] = *reinterpret_cast<int*>(s.ptr); break;
			case SettingType::Color:
				j[ptr] = static_cast<uint32_t>(*static_cast<ImColor*>(s.ptr));
				break;
			case SettingType::Vec2:
			{
				const ImVec2& v = *static_cast<ImVec2*>(s.ptr);
				j[ptr] = json::array({ v.x, v.y });
				break;
			}
			case SettingType::Key:
			{
				const CKeybind& k = *static_cast<CKeybind*>(s.ptr);
				j[ptr] = json{
					{ "m_Key",       k.m_Key },
					{ "m_bTargetPC", k.m_bTargetPC },
					{ "m_bRadarPC",  k.m_bRadarPC },
				};
				break;
			}
			}
		}

		return j;
	}

	// Case-insensitive substring test for the search box.
	bool Matches(const char* haystack, const char* needle)
	{
		if (!needle || !needle[0]) return true;

		const size_t hl = std::strlen(haystack);
		const size_t nl = std::strlen(needle);
		if (nl > hl) return false;

		for (size_t i = 0; i + nl <= hl; ++i)
		{
			size_t j = 0;
			for (; j < nl; ++j)
			{
				if (std::tolower((unsigned char)haystack[i + j]) != std::tolower((unsigned char)needle[j]))
					break;
			}
			if (j == nl) return true;
		}
		return false;
	}
}

const std::vector<Setting>& Settings::All()
{
	if (!s_Built)
	{
		Build();
		s_Built = true;
		// Captured before any config load, so these are the in-header initializers.
		s_Defaults = Serialize(s_Table);
	}
	return s_Table;
}

nlohmann::json Settings::ToJson()
{
	return Serialize(All());
}

void Settings::FromJson(const nlohmann::json& j)
{
	for (const Setting& s : All())
	{
		const auto ptr = Pointer(s.path);
		if (!j.contains(ptr))
			continue;

		const json& v = j.at(ptr);

		// A hand-edited or older config can hold the wrong type for a key. Skip
		// the entry rather than letting nlohmann throw out of the whole load.
		try
		{
			switch (s.type)
			{
			case SettingType::Bool:
				if (v.is_boolean()) *static_cast<bool*>(s.ptr) = v.get<bool>();
				break;
			case SettingType::Int:
				if (v.is_number()) *static_cast<int*>(s.ptr) = v.get<int>();
				break;
			case SettingType::Float:
				if (v.is_number()) *static_cast<float*>(s.ptr) = v.get<float>();
				break;
			case SettingType::Enum:
				if (v.is_number())
				{
					const int raw = v.get<int>();
					// Clamp: a stale config naming a since-removed enumerator must
					// not index past the label table in RenderWidget.
					if (s.nameCount > 0)
						*reinterpret_cast<int*>(s.ptr) = std::clamp(raw, 0, s.nameCount - 1);
					else
						*reinterpret_cast<int*>(s.ptr) = raw;
				}
				break;
			case SettingType::Color:
				if (v.is_number_unsigned())
					*static_cast<ImColor*>(s.ptr) = ImColor(v.get<uint32_t>());
				break;
			case SettingType::Vec2:
				if (v.is_array() && v.size() == 2)
					*static_cast<ImVec2*>(s.ptr) = ImVec2(v[0].get<float>(), v[1].get<float>());
				break;
			case SettingType::Key:
			{
				if (!v.is_object()) break;
				CKeybind& k = *static_cast<CKeybind*>(s.ptr);
				if (v.contains("m_Key"))       k.m_Key       = v["m_Key"].get<uint32_t>();
				if (v.contains("m_bTargetPC")) k.m_bTargetPC = v["m_bTargetPC"].get<bool>();
				if (v.contains("m_bRadarPC"))  k.m_bRadarPC  = v["m_bRadarPC"].get<bool>();
				break;
			}
			}
		}
		catch (const json::exception& e)
		{
			Log::Warn("[Settings] skipping '{}': {}", s.path, e.what());
		}
	}
}

void Settings::ResetAll()
{
	// Forces the table + default capture before reading s_Defaults.
	(void)All();
	FromJson(s_Defaults);
}

void Settings::ResetPrefix(const char* pathPrefix)
{
	(void)All();

	const size_t n = std::strlen(pathPrefix);

	// Build a defaults subset containing only the matching paths, then run it
	// through the normal load path so every type is handled in one place.
	json subset;
	for (const Setting& s : s_Table)
	{
		if (std::strncmp(s.path, pathPrefix, n) != 0) continue;

		const auto ptr = Pointer(s.path);
		if (s_Defaults.contains(ptr))
			subset[ptr] = s_Defaults.at(ptr);
	}

	FromJson(subset);
}

bool Settings::RenderWidget(const Setting& s, const char* labelOverride)
{
	const char* label = labelOverride ? labelOverride : s.label;

	bool changed = false;
	ImGui::PushID(s.path);

	switch (s.type)
	{
	case SettingType::Bool:
		changed = ImGui::Checkbox(label, static_cast<bool*>(s.ptr));
		break;

	case SettingType::Int:
		ImGui::SetNextItemWidth(160.0f);
		changed = ImGui::SliderInt(label, static_cast<int*>(s.ptr),
		                           static_cast<int>(s.min), static_cast<int>(s.max));
		break;

	case SettingType::Float:
		ImGui::SetNextItemWidth(160.0f);
		changed = ImGui::SliderFloat(label, static_cast<float*>(s.ptr),
		                             s.min, s.max, s.fmt ? s.fmt : "%.2f");
		break;

	case SettingType::Enum:
	{
		ImGui::SetNextItemWidth(160.0f);
		int value = *reinterpret_cast<int*>(s.ptr);
		if (ImGui::Combo(label, &value, s.names, s.nameCount))
		{
			*reinterpret_cast<int*>(s.ptr) = std::clamp(value, 0, s.nameCount - 1);
			changed = true;
		}
		break;
	}

	case SettingType::Color:
	{
		ImColor& col = *static_cast<ImColor*>(s.ptr);
		changed = ImGui::ColorEdit4("##swatch", &col.Value.x,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
			ImGuiColorEditFlags_AlphaPreviewHalf);
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
		break;
	}

	case SettingType::Vec2:
	{
		ImVec2& v = *static_cast<ImVec2*>(s.ptr);
		ImGui::SetNextItemWidth(160.0f);
		changed = ImGui::InputFloat2(label, &v.x, "%.0f");
		break;
	}

	case SettingType::Key:
		// Keybind capture is a modal interaction owned by CKeybind::Render; the
		// search box links to it rather than reimplementing it.
		ImGui::TextDisabled("%s - set in the Keybinds tab", label);
		break;
	}

	ImGui::PopID();
	return changed;
}

void Settings::RenderSearch()
{
	static char sQuery[64] = {};

	const float resetWidth = 70.0f;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputTextWithHint("##settingsearch", "Search all settings...", sQuery, sizeof(sQuery));

	ImGui::SameLine();
	if (ImGui::Button("Reset", ImVec2(resetWidth, 0.0f)))
		ResetAll();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Restore every setting to its default");

	if (!sQuery[0])
	{
		ImGui::Separator();
		return;
	}

	ImGui::Spacing();

	int hits = 0;
	for (const Setting& s : All())
	{
		// Path is searched too, so "powerup" finds the colors as well as the
		// toggles even though their labels are prefixed differently.
		if (!Matches(s.label, sQuery) && !Matches(s.path, sQuery))
			continue;

		if (hits == 0)
			ImGui::SeparatorText("Search Results");

		// Cap the list: an empty-ish query like "a" matches most of the registry,
		// and a 100-row dump is not a usable answer.
		if (++hits > 14)
		{
			ImGui::TextDisabled("... narrow the search to see more");
			break;
		}

		RenderWidget(s);
	}

	if (hits == 0)
		ImGui::TextDisabled("No settings match \"%s\"", sQuery);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}
