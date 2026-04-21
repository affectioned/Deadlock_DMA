#include "pch.h"

#include "DeadlockContext.h"
#include "Deadlock.h"
#include "Entity List/EntityList.h"
#include "GUI/Keybinds/Keybinds.h"

bool DeadlockContext::Initialize(DMA_Connection* conn)
{
	if (!Deadlock::Initialize(conn))
		return false;

	using ms = std::chrono::milliseconds;

	auto* proc = &Deadlock::Proc();

	m_Timers =
	{
		{ ms(2),     [conn]       { Deadlock::UpdateViewMatrix(conn); } },
		{ ms(10),    [conn]       { Deadlock::UpdateClientYaw(conn); } },
		{ ms(1000),  [conn]       { Deadlock::UpdateServerTime(conn); } },
		{ ms(15000), [conn]       { Deadlock::UpdateLocalPlayerAddresses(conn); } },

		{ ms(3000),  [conn, proc] { EntityList::FullTrooperRefresh(conn, proc); } },
		{ ms(16),    [conn, proc] { EntityList::QuickTrooperRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullPawnRefresh_lk(conn, proc); } },
		{ ms(8),     [conn, proc] { EntityList::QuickPawnRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullMonsterCampRefresh(conn, proc); } },
		{ ms(100),   [conn, proc] { EntityList::QuickMonsterCampRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullControllerRefresh_lk(conn, proc); } },
		{ ms(150),   [conn, proc] { EntityList::QuickControllerRefresh(conn, proc); } },

		{ ms(1000),  [conn, proc] { EntityList::FullSinnerRefresh(conn, proc); } },

		{ ms(500),   [conn, proc] { EntityList::FullXpOrbRefresh(conn, proc); } },
		{ ms(16),    [conn, proc] { EntityList::QuickXpOrbRefresh(conn, proc); } },

		{ ms(5000),  [conn, proc] { EntityList::FullUpdate(conn, proc); } },

		{ ms(5),     [conn]       { Keybinds::OnDMAFrame(conn); } },
	};

	return true;
}

void DeadlockContext::Tick(DMA_Connection* /*conn*/, std::chrono::steady_clock::time_point now)
{
	for (auto& t : m_Timers)
		t.Tick(now);
}
