#include "pch.h"

#include "DeadlockContext.h"
#include "Deadlock.h"
#include "Entity List/EntityList.h"
#include "GUI/Keybinds/Keybinds.h"
#include "DMA/Memory/PhaseTimings.h"
#include "GUI/Watchdog/GuiWatchdog.h"

bool DeadlockContext::Initialize(DMA_Connection* conn)
{
	if (!Deadlock::Initialize(conn))
		return false;

	using ms = std::chrono::milliseconds;

	auto* proc = &Deadlock::Proc();

	// Wraps a timer callback in a named ScopedUs so the periodic dump shows
	// per-phase totals. Registry lookup happens once at wrapper construction
	// (it's mutex-guarded — we don't want it on every tick); each tick just
	// pays for two clock reads + an int add.
	auto timed = [](const char* name, std::function<void()> f) {
		PhaseUs& phase = PhaseTimings::Get(name);
		return [&phase, name, f = std::move(f)] {
			GuiWatchdog::DmaStage(name);
			ScopedUs scope(phase, name);
			f();
		};
	};

	m_Timers =
	{
		// Render-driven values: ESP/aimbot read these every frame. 8 ms = 125 Hz,
		// already 2× any practical monitor rate. Aligned with Yaw + QuickPawn so
		// they tend to fire on the same DMA tick.
		// View matrix update also derives client yaw — no separate timer.
		{ ms(8),     timed("ViewMatrix",      [conn]       { Deadlock::UpdateViewMatrix(conn); }) },
		{ ms(1000),  timed("ServerTime",      [conn]       { Deadlock::UpdateServerTime(conn); }) },
		// Local controller pointer effectively never moves during a match.
		{ ms(30000), timed("LocalAddrs",      [conn]       { Deadlock::UpdateLocalPlayerAddresses(conn); }) },

		// Per-entity refreshes:
		//   "Full" rebuilds wrappers from the address list (slow, infrequent).
		//   "Quick" updates per-frame fields (position, health) on existing wrappers.
		{ ms(3000),  timed("FullTrooper",     [conn, proc] { EntityList::FullTrooperRefresh(conn, proc); }) },
		{ ms(16),    timed("QuickTrooper",    [conn, proc] { EntityList::QuickTrooperRefresh(conn, proc); }) },

		{ ms(2000),  timed("FullPawn",        [conn, proc] { EntityList::FullPawnRefresh_lk(conn, proc); }) },
		// 4ms = 250 Hz, one fresh sample per 240Hz frame. ~1.2ms/tick * 250 = 30% of
		// the DMA thread, but still leaves headroom and eliminates jitter on
		// high-refresh monitors.
		{ ms(4),     timed("QuickPawn",       [conn, proc] { EntityList::QuickPawnRefresh(conn, proc); }) },

		{ ms(3000),  timed("FullCamp",        [conn, proc] { EntityList::FullMonsterCampRefresh(conn, proc); }) },
		{ ms(250),   timed("QuickCamp",       [conn, proc] { EntityList::QuickMonsterCampRefresh(conn, proc); }) },

		{ ms(2000),  timed("FullController",  [conn, proc] { EntityList::FullControllerRefresh_lk(conn, proc); }) },
		{ ms(300),   timed("QuickController", [conn, proc] { EntityList::QuickControllerRefresh(conn, proc); }) },

		{ ms(2000),  timed("FullSinner",      [conn, proc] { EntityList::FullSinnerRefresh(conn, proc); }) },

		{ ms(1000),  timed("FullXpOrb",       [conn, proc] { EntityList::FullXpOrbRefresh(conn, proc); }) },
		{ ms(16),    timed("QuickXpOrb",      [conn, proc] { EntityList::QuickXpOrbRefresh(conn, proc); }) },

		// Breakable crates/powerups don't move once spawned (except dropped pickups
		// which briefly vacuum toward the player). 1s full / 16ms quick mirrors XP
		// orbs — same scatter cost, same responsiveness for the vacuum animation.
		{ ms(1000),  timed("FullPowerup",     [conn, proc] { EntityList::FullPowerupRefresh(conn, proc); }) },
		{ ms(16),    timed("QuickPowerup",    [conn, proc] { EntityList::QuickPowerupRefresh(conn, proc); }) },

		// Primary-weapon ability VData read. Base bullet speed only changes on
		// hero swap or build change, so a slow cadence is fine.
		{ ms(2000),  timed("BulletSpeed",     [conn, proc] { EntityList::RefreshPrimaryWeaponBulletSpeed(conn, proc); }) },

		// Visibility: 60 Hz keeps "visible only" gating responsive when enemies
		// duck behind walls or step out of vision.
		{ ms(16),    timed("FullFOW",         [conn, proc] { EntityList::FullFOWRefresh(conn, proc); }) },

		// Discovers new entity addresses + scans for the populated FOW team.
		{ ms(1000),  timed("FullUpdate",      [conn, proc] { EntityList::FullUpdate(conn, proc); }) },

		{ ms(8),     timed("Keybinds",        [conn]       { Keybinds::OnDMAFrame(conn); }) },

		// Periodic bottleneck summary — totals over the prior 10s window.
		{ ms(10000), [] { PhaseTimings::DumpAndReset(); } },
	};

	return true;
}

void DeadlockContext::Tick(DMA_Connection* /*conn*/, std::chrono::steady_clock::time_point now)
{
	for (auto& t : m_Timers)
		t.Tick(now);
}
