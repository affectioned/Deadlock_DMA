#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

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
