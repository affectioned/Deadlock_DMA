#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

void EntityList::FullXpOrbRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingXpOrbs() == false) return;

	std::scoped_lock Lock(m_XpOrbMutex);

	m_XpOrbs.clear();

	for (auto& addr : m_XpOrbAddresses)
		m_XpOrbs.emplace_back(addr);

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.PrepareRead_1(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.PrepareRead_2(*m_sr);
	m_sr->Execute();
}

void EntityList::QuickXpOrbRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingXpOrbs() == false) return;

	std::scoped_lock Lock(m_XpOrbMutex);

	m_sr->Clear();
	for (auto& Orb : m_XpOrbs)
		Orb.QuickRead(*m_sr, /*bReadHealth*/ false);
	m_sr->Execute();
}
