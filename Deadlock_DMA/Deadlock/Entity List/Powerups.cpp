#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

#include "Deadlock/Engine/CHandle.h"

namespace
{
	// CInWorldItemPanel::m_hTrackedEntity offset (source2sdk client dump,
	// CInWorldItemPanel.hpp). The handle resolves to the pickup the panel is
	// floating above, which is what we actually want to name in the ESP.
	constexpr uintptr_t kInWorldItemPanel_hTrackedEntity = 0xBF0;

	// Maps a tracked-entity class name (e.g. "citadel_item_pickup_idol") to a
	// short display label. Substring match keeps this stable across renames
	// like item_health_regen → item_pickup_health.
	const char* FriendlyLabelForClass(const std::string& c)
	{
		if (c.find("rejuv")     != std::string::npos) return "Rejuv";
		if (c.find("idol")      != std::string::npos) return "Idol";
		if (c.find("punchable") != std::string::npos) return "Souls";
		if (c.find("health")    != std::string::npos) return "Health";
		if (c.find("necro")     != std::string::npos) return "Necro";
		if (c.find("item_xp")   != std::string::npos) return "XP";
		return "Item";
	}

	// Cache: tracked-entity class-name pointer → resolved friendly label.
	// Class-name pointers are stable string globals in client.dll for the
	// life of the process, so this fills in during the first refresh after
	// each class first appears and never invalidates.
	// Guarded by the powerup mutex (only touched inside FullPowerupRefresh).
	std::unordered_map<uintptr_t, const char*> s_PanelLabelByClassPtr;

	const char* ResolvePanelLabel(uintptr_t classPtr)
	{
		if (!classPtr) return "Panel";

		if (auto it = s_PanelLabelByClassPtr.find(classPtr); it != s_PanelLabelByClassPtr.end())
			return it->second;

		std::scoped_lock lk(EntityList::m_ClassMapMutex);
		auto it = EntityList::m_EntityClassNameByPtr.find(classPtr);
		if (it == EntityList::m_EntityClassNameByPtr.end()) return "Panel";

		const char* label = FriendlyLabelForClass(it->second);
		s_PanelLabelByClassPtr[classPtr] = label;
		return label;
	}
}

void EntityList::FullPowerupRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPowerups() == false) return;

	std::scoped_lock Lock(m_PowerupMutex);

	m_Powerups.clear();

	for (auto& [addr, label] : m_PowerupAddresses)
	{
		C_BaseEntity Ent(addr);
		Ent.m_Label = label;
		m_Powerups.emplace_back(Ent);
	}

	m_sr->Clear();
	for (auto& P : m_Powerups)
		P.PrepareRead_1(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& P : m_Powerups)
		P.PrepareRead_2(*m_sr);
	m_sr->Execute();

	// Extra pass for in_world_item_panel: resolve m_hTrackedEntity → tracked
	// pickup's class name → short label. Panels far outnumber crates, and
	// leaving them all as "Panel" is useless visually; this turns the ESP
	// into a labelled minimap of every interactive pickup on the map.
	struct PanelWork
	{
		C_BaseEntity* panel;
		uint32_t      handleRaw;
	};
	std::vector<PanelWork> panels;
	panels.reserve(m_Powerups.size());
	for (auto& P : m_Powerups)
	{
		if (P.IsInvalid()) continue;
		if (!P.m_Label || std::strcmp(P.m_Label, "Panel") != 0) continue;
		panels.push_back({ &P, 0u });
	}

	if (!panels.empty())
	{
		m_sr->Clear();
		for (auto& w : panels)
			m_sr->Add(w.panel->m_EntityAddress + kInWorldItemPanel_hTrackedEntity, &w.handleRaw);
		m_sr->Execute();

		for (auto& w : panels)
		{
			CHandle h; h.Data = w.handleRaw;
			if (!h.IsValid()) continue;

			const size_t listIdx  = h.GetEntityListIndex();
			const size_t entryIdx = h.GetEntityEntryIndex();
			if (listIdx >= MAX_ENTITY_LISTS || entryIdx >= MAX_ENTITIES) continue;

			w.panel->m_Label = ResolvePanelLabel(m_CompleteEntityList[listIdx][entryIdx].pName);
		}
	}
}

void EntityList::QuickPowerupRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPowerups() == false) return;

	std::scoped_lock Lock(m_PowerupMutex);

	m_sr->Clear();
	for (auto& P : m_Powerups)
		P.QuickRead(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();
}
