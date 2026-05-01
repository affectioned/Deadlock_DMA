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
		// Render-driven values: ESP/aimbot read these every frame. 8 ms = 125 Hz,
		// already 2× any practical monitor rate. Aligned with Yaw + QuickPawn so
		// they tend to fire on the same DMA tick.
		{ ms(8),     [conn]       { Deadlock::UpdateViewMatrix(conn); } },
		{ ms(8),     [conn]       { Deadlock::UpdateClientYaw(conn); } },
		{ ms(1000),  [conn]       { Deadlock::UpdateServerTime(conn); } },
		// Local controller pointer effectively never moves during a match.
		{ ms(30000), [conn]       { Deadlock::UpdateLocalPlayerAddresses(conn); } },

		// Per-entity refreshes:
		//   "Full" rebuilds wrappers from the address list (slow, infrequent).
		//   "Quick" updates per-frame fields (position, health) on existing wrappers.
		{ ms(3000),  [conn, proc] { EntityList::FullTrooperRefresh(conn, proc); } },
		{ ms(16),    [conn, proc] { EntityList::QuickTrooperRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullPawnRefresh_lk(conn, proc); } },
		{ ms(8),     [conn, proc] { EntityList::QuickPawnRefresh(conn, proc); } },

		{ ms(3000),  [conn, proc] { EntityList::FullMonsterCampRefresh(conn, proc); } },
		{ ms(250),   [conn, proc] { EntityList::QuickMonsterCampRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullControllerRefresh_lk(conn, proc); } },
		{ ms(300),   [conn, proc] { EntityList::QuickControllerRefresh(conn, proc); } },

		{ ms(2000),  [conn, proc] { EntityList::FullSinnerRefresh(conn, proc); } },

		{ ms(1000),  [conn, proc] { EntityList::FullXpOrbRefresh(conn, proc); } },
		{ ms(16),    [conn, proc] { EntityList::QuickXpOrbRefresh(conn, proc); } },

		// Visibility: 60 Hz keeps "visible only" gating responsive when enemies
		// duck behind walls or step out of vision.
		{ ms(16),    [conn, proc] { EntityList::FullFOWRefresh(conn, proc); } },

		// Discovers new entity addresses + scans for the populated FOW team.
		{ ms(1000),  [conn, proc] { EntityList::FullUpdate(conn, proc); } },

		{ ms(8),     [conn]       { Keybinds::OnDMAFrame(conn); } },
	};

	return true;
}

void DeadlockContext::Tick(DMA_Connection* /*conn*/, std::chrono::steady_clock::time_point now)
{
	for (auto& t : m_Timers)
		t.Tick(now);
}
