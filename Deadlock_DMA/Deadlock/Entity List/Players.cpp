#include "pch.h"

#include "EntityList.h"
#include "GUI/Query.h"

void EntityList::FullControllerRefresh_lk(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	// Acquire both atomically (Pawn, Controller) to match every other call site
	// and avoid lock-order inversion against Radar (Pawn-then-Controller).
	std::scoped_lock Lock(m_PawnMutex, m_ControllerMutex);

	FullControllerRefresh(Conn, Proc);
}

void EntityList::FullControllerRefresh(DMA_Connection* Conn, Process* Proc)
{
	m_PlayerControllers.clear();
	m_PlayerController_Addresses.clear();

	// Controllers don't register a class name in the entity system (pName == 0),
	// so derive their addresses from pawn m_hController handles instead.
	// Caller must hold m_PawnMutex + m_ControllerMutex.
	for (auto& Pawn : m_PlayerPawns)
	{
		uintptr_t addr = GetEntityAddressFromHandle(Pawn.m_hController);
		if (addr) m_PlayerController_Addresses.push_back(addr);
	}

	DbgLog("[EntityList] ControllerRefresh: {} controller addresses from pawn handles", m_PlayerController_Addresses.size());

	for (auto& addr : m_PlayerController_Addresses)
		m_PlayerControllers.emplace_back(CCitadelPlayerController(addr));

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.PrepareRead_2(*m_sr);
	m_sr->Execute();

	int validCount = 0, deadCount = 0;
	for (int i = 0; i < (int)m_PlayerControllers.size(); i++)
	{
		auto& PC = m_PlayerControllers[i];
		if (!PC.IsInvalid()) validCount++;
		if (PC.IsDead()) deadCount++;

		if (PC.IsLocalPlayer())
			m_LocalControllerIndex = i;
		else if (i == (int)m_PlayerControllers.size() - 1 && m_LocalControllerIndex == -1)
			m_LocalControllerIndex = -1;
	}
	DbgLog("[EntityList] ControllerRefresh: {}/{} valid, {} dead, local={}",
		validCount, m_PlayerControllers.size(), deadCount, m_LocalControllerIndex);
}

void EntityList::QuickControllerRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_ControllerMutex);

	m_sr->Clear();
	for (auto& PC : m_PlayerControllers)
		PC.QuickRead(*m_sr);
	m_sr->Execute();
}

void EntityList::FullPawnRefresh_lk(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_PawnMutex);

	FullPawnRefresh(Conn, Proc);
}

void EntityList::FullPawnRefresh(DMA_Connection* Conn, Process* Proc)
{
	m_PlayerPawns.clear();

	DbgLog("[EntityList] PawnRefresh: {} pawn addresses", m_PlayerPawn_Addresses.size());

	for (auto& addr : m_PlayerPawn_Addresses)
		m_PlayerPawns.emplace_back(C_CitadelPlayerPawn(addr));

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_1(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_2(*m_sr);
	m_sr->Execute();

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.PrepareRead_3(*m_sr);
	m_sr->Execute();

	int validPawns = 0;
	for (auto& Pawn : m_PlayerPawns)
	{
		Pawn.ExtractBones();
		Pawn.CacheBoneData();
		if (!Pawn.IsInvalid()) validPawns++;
	}
	DbgLog("[EntityList] PawnRefresh: {}/{} valid after reads", validPawns, m_PlayerPawns.size());

	for (int i = 0; i < (int)m_PlayerPawns.size(); i++)
	{
		if (m_PlayerPawns[i].IsLocalPlayer())
		{
			m_LocalPawnIndex = i;
			break;
		}
		else if (i == m_PlayerPawns.size() - 1)
		{
			m_LocalPawnIndex = -1;
		}
	}
}

void EntityList::QuickPawnRefresh(DMA_Connection* Conn, Process* Proc)
{
	if (Query::IsUsingPlayers() == false) return;

	std::scoped_lock Lock(m_PawnMutex);

	m_sr->Clear();
	for (auto& Pawn : m_PlayerPawns)
		Pawn.QuickRead(*m_sr);
	m_sr->Execute();

	for (auto& Pawn : m_PlayerPawns)
		Pawn.ExtractBones();
}

ETeam EntityList::GetLocalPlayerTeam()
{
	auto Return = ETeam::UNKNOWN;

	std::scoped_lock Lock(m_PawnMutex);

	if (m_LocalPawnIndex == -1)
		return Return;

	return m_PlayerPawns[m_LocalPawnIndex].m_TeamNum;
}

Vector3 EntityList::GetLocalPawnPosition()
{
	auto Return = Vector3();

	std::scoped_lock Lock(m_PawnMutex);

	if (m_LocalPawnIndex == -1)
		return Return;

	return m_PlayerPawns[m_LocalPawnIndex].m_Position;
}

/* m_ControllerMutex must be locked! */
CCitadelPlayerController* EntityList::GetAssociatedPC(const C_CitadelPlayerPawn& Pawn)
{
	auto PCAddress = EntityList::GetEntityAddressFromHandle(Pawn.m_hController);

	for (auto& PC : m_PlayerControllers)
	{
		if (PC.m_EntityAddress == PCAddress)
			return &PC;
	}

	return nullptr;
}
