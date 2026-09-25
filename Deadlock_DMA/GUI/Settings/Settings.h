#pragma once
#include "..\..\..\Dependencies\nlohmann\json.hpp"

// Single source of truth for every persisted, user-facing setting.
//
// Config.cpp used to carry two hand-written mirrors of the whole settings tree —
// SerializeConfig and DeserializeConfig, ~340 lines between them — so adding a
// setting meant three edits (declare, serialize, deserialize) and forgetting the
// third failed silently. It had already drifted: Radar::bPlayerCentered, the
// three trooper sub-filters, Draw_Players::fBoxThickness's box style, five of the
// powerup colors and both box colors were declared but never saved.
//
// One table drives save, load, reset-to-default and the settings search box, so
// a new setting is one row and cannot be half-wired.
enum class SettingType
{
	Bool,
	Int,
	Float,
	Color,  // ImColor, stored as packed uint32 (matches the pre-registry format)
	Vec2,   // ImVec2, stored as a 2-element array
	Enum,   // enum class with int underlying type, stored as int
	Key,    // CKeybind, stored as { m_Key, m_bTargetPC, m_bRadarPC }
};

struct Setting
{
	// Dotted JSON path. Deliberately identical to the key structure the old
	// hand-written serializer produced, so configs saved before the registry
	// still load without migration.
	const char* path;
	// Human label, used by the search box. Prefixed with its panel ("Players: Box")
	// so a hit is meaningful on its own, away from its tab.
	const char* label;
	SettingType type;
	void*       ptr;

	// Int / Float only.
	float       min{ 0.0f };
	float       max{ 0.0f };
	const char* fmt{ nullptr };

	// Enum only.
	const char* const* names{ nullptr };
	int                nameCount{ 0 };
};

namespace Settings
{
	const std::vector<Setting>& All();

	nlohmann::json ToJson();
	void FromJson(const nlohmann::json& j);

	// Restores the values the binaries' in-header initializers started with,
	// captured the first time All() ran — before any config load.
	void ResetAll();

	// ResetAll limited to one path prefix, e.g. "ColorPicker." to restore the
	// Deadlock palette without discarding the rest of the user's setup.
	void ResetPrefix(const char* pathPrefix);

	// Draws one setting's editor. Returns true if the value changed.
	// labelOverride replaces the registry label, for tabs where the search-
	// oriented "Panel: Thing" prefix is redundant.
	bool RenderWidget(const Setting& s, const char* labelOverride = nullptr);

	// Search field, drawn above the active tab. Typing filters the whole registry
	// and edits hits in place, so a setting can be changed without knowing which
	// of the eleven tabs owns it.
	void RenderSearch();
}
