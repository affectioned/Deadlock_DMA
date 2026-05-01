#include "pch.h"
#include "GuiWatchdog.h"

#include "DMA/Logging/Log.h"
#include "Deadlock/Entity List/EntityList.h"
#include "Deadlock/Deadlock.h"

#include <thread>
#include <chrono>

void GuiWatchdog::Start()
{
	bool expected = false;
	if (!s_Running.compare_exchange_strong(expected, true)) return;
	std::thread(WatchdogLoop).detach();
	Log::Info("[Watchdog] Started.");
}

void GuiWatchdog::GuiStage(const char* name)
{
	s_LastGuiStage.store(name, std::memory_order_relaxed);
}

void GuiWatchdog::DmaStage(const char* name)
{
	s_LastDmaStage.store(name, std::memory_order_relaxed);
}

void GuiWatchdog::Tick()
{
	auto frame = s_FrameCounter.fetch_add(1, std::memory_order_relaxed) + 1;

	using clk = std::chrono::steady_clock;
	static auto lastLog = clk::time_point{};
	auto now = clk::now();
	if (now - lastLog >= std::chrono::seconds(5))
	{
		lastLog = now;
		Log::Info("[GUI] heartbeat frame={} stage={} dmaStage={}",
			frame,
			s_LastGuiStage.load(std::memory_order_relaxed),
			s_LastDmaStage.load(std::memory_order_relaxed));
	}
}

void GuiWatchdog::WatchdogLoop()
{
	using namespace std::chrono_literals;
	using clk = std::chrono::steady_clock;

	uint64_t lastSeen = 0;
	auto lastSeenAt = clk::now();
	bool stalled = false;
	auto lastReReport = clk::time_point{};

	auto Probe = [](const char* name, std::mutex& m)
	{
		if (m.try_lock())
		{
			m.unlock();
			Log::Info("[Watchdog]   {}: free", name);
		}
		else
		{
			Log::Warn("[Watchdog]   {}: HELD", name);
		}
	};

	for (;;)
	{
		std::this_thread::sleep_for(500ms);

		auto cur = s_FrameCounter.load(std::memory_order_relaxed);
		auto now = clk::now();

		if (cur != lastSeen)
		{
			lastSeen = cur;
			lastSeenAt = now;
			if (stalled)
			{
				Log::Info("[Watchdog] GUI recovered at frame={} stage={}",
					cur, s_LastGuiStage.load(std::memory_order_relaxed));
				stalled = false;
			}
			continue;
		}

		auto stalledFor = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeenAt);
		if (stalledFor < 2s) continue;

		bool firstReport = !stalled;
		bool periodicRepeat = stalled && (now - lastReReport >= 5s);
		if (!firstReport && !periodicRepeat) continue;

		stalled = true;
		lastReReport = now;

		const char* gstage = s_LastGuiStage.load(std::memory_order_relaxed);
		const char* dstage = s_LastDmaStage.load(std::memory_order_relaxed);
		Log::Warn("[Watchdog] GUI STALL: frame={} stuck {}ms guiStage={} dmaStage={}",
			cur, stalledFor.count(), gstage, dstage);

		Probe("PawnMutex",        EntityList::m_PawnMutex);
		Probe("ControllerMutex",  EntityList::m_ControllerMutex);
		Probe("TrooperMutex",     EntityList::m_TrooperMutex);
		Probe("MonsterCampMutex", EntityList::m_MonsterCampMutex);
		Probe("SinnerMutex",      EntityList::m_SinnerMutex);
		Probe("XpOrbMutex",       EntityList::m_XpOrbMutex);
		Probe("ClassMapMutex",    EntityList::m_ClassMapMutex);
		Probe("ServerTimeMutex",  Deadlock::m_ServerTimeMutex);
	}
}
