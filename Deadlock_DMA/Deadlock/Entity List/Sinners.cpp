#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

void EntityList::FullSinnerRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingSinners() == false) return;

	std::scoped_lock Lock(m_SinnerMutex);

	m_Sinners.clear();

	for (auto& addr : m_SinnersAddresses)
		m_Sinners.emplace_back(C_BaseEntity(addr));

	m_sr->Clear();
	for (auto& Sinner : m_Sinners)
		Sinner.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Sinner : m_Sinners)
		Sinner.PrepareRead_2(*m_sr);
	m_sr->Execute();
}
