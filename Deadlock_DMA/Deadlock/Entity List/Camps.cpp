#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

void EntityList::FullMonsterCampRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingCamps() == false) return;

	std::scoped_lock Lock(m_MonsterCampMutex);

	m_MonsterCamps.clear();

	for (auto& [addr, label] : m_MonsterCampAddresses)
	{
		auto& c = m_MonsterCamps.emplace_back(C_BaseEntity(addr));
		c.m_Label = label;
	}

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.PrepareRead_1(*m_sr, true, true);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.PrepareRead_2(*m_sr);
	m_sr->Execute();
}

void EntityList::QuickMonsterCampRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingCamps() == false) return;

	std::scoped_lock Lock(m_MonsterCampMutex);

	m_sr->Clear();
	for (auto& Camp : m_MonsterCamps)
		Camp.QuickRead(*m_sr);
	m_sr->Execute();
}
