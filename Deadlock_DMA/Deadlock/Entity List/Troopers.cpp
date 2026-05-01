#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

void EntityList::FullTrooperRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingTroopers() == false) return;

	std::scoped_lock Lock(m_TrooperMutex);

	m_Troopers.clear();

	for (auto& [addr, label] : m_TrooperAddresses)
	{
		auto& t = m_Troopers.emplace_back(C_NPC_Trooper(addr));
		t.m_Label = label;
	}

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.PrepareRead_2(*m_sr);
	m_sr->Execute();

	DbgLog("Trooper List Refreshed. Count: {}", m_Troopers.size());
}

void EntityList::QuickTrooperRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingTroopers() == false) return;

	std::scoped_lock Lock(m_TrooperMutex);

	if (m_Troopers.empty()) return;

	m_sr->Clear();
	for (auto& Trooper : m_Troopers)
		Trooper.QuickRead(*m_sr);
	m_sr->Execute();
}
