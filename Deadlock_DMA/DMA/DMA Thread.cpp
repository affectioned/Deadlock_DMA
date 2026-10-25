#include "pch.h"

#include "DMA Thread.h"
#include "Input/Input Manager.h"
#include "IGameContext.h"
#include "Deadlock/Deadlock.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Deadlock/Offsets.h"
#include "Deadlock/SchemaWalker.h"

#pragma comment(lib, "Winmm.lib")

IGameContext* g_GameContext = nullptr;

extern std::atomic<bool> bRunning;

// Watchdog cadence. Called every 2 s inside the tick loop while attached;
// bigger value = slower noticed, smaller value = more DMA chatter. Two
// seconds matches the EFT overlay — the game takes multiple seconds to
// relaunch anyway, so a tighter poll earns nothing.
static constexpr auto kAttachProbeInterval = std::chrono::seconds(2);

// Drop every per-process cache the game layer holds. The DMA connection
// itself (and the keyboard input handle, which is physical-page based) are
// unaffected — only state that keys off the attached PID gets wiped.
static void TearDownForReattach()
{
	Log::Info("[DMA] game process gone — tearing down per-process state");
	globals::g_attached.store(false, std::memory_order_release);

	Deadlock::Reset();
	EntityList::Reset();
	Offsets::Reset();
	SchemaWalker::Reset();
}

void DMA_Thread_Main()
{
	Log::Info("[DMA] thread started");

	DMA_Connection* conn = DMA_Connection::GetInstance();

	// One-time: physical-memory keyboard scan setup. Independent of the
	// game process, survives every reattach.
	c_keys::InitKeyboard(conn);

	timeBeginPeriod(1);

	// Outer loop: run the attach dance, then tick until the game dies, then
	// tear down and loop. bRunning only drops when the user quits the overlay,
	// so this is effectively while(bRunning).
	while (bRunning)
	{
		if (!g_GameContext)
		{
			Log::Error("[DMA] g_GameContext not set, exiting");
			bRunning = false;
			break;
		}

		// IGameContext::Initialize blocks inside Process::GetProcessInfo until
		// the game is up. That's exactly what we want on both first-start and
		// reattach — the DMA app sits here while the user restarts EFT without
		// needing to be restarted itself.
		if (!g_GameContext->Initialize(conn))
		{
			Log::Error("[DMA] init failed, retrying in 2s");
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}
		globals::g_attached.store(true, std::memory_order_release);
		Log::Info("[DMA] attached — entering tick loop");

		auto lastTlbRefresh    = std::chrono::steady_clock::now();
		auto lastMemRefresh    = lastTlbRefresh;
		auto lastAttachProbe   = lastTlbRefresh;

		// Inner tick loop. Exits on game-process death (attach probe) or
		// user shutdown (bRunning).
		while (bRunning)
		{
			std::this_thread::yield();

			FrameMarkStart("DMA");
			auto now = std::chrono::steady_clock::now();

			// Supervisor probe. Called on the DMA thread so we don't need a
			// third thread of our own — the probe is a single VMMDLL name
			// lookup, cheap enough to fit in a tick slot.
			if (now - lastAttachProbe >= kAttachProbeInterval)
			{
				if (!Deadlock::Proc().IsAlive(conn))
				{
					FrameMarkEnd("DMA");
					break;
				}
				lastAttachProbe = now;
			}

			if (now - lastMemRefresh >= std::chrono::milliseconds(50))
			{
				VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_MEM, 1);
				lastMemRefresh = now;
			}

			if (now - lastTlbRefresh >= std::chrono::milliseconds(250))
			{
				VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
				lastTlbRefresh = now;
			}

			g_GameContext->Tick(conn, now);
			FrameMarkEnd("DMA");
		}

		if (!bRunning) break;

		TearDownForReattach();
		// Loop back to Initialize — Process::GetProcessInfo re-blocks waiting
		// for the new instance.
	}

	timeEndPeriod(1);

	conn->EndConnection();
}
