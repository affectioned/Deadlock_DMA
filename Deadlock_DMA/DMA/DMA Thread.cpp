#include "pch.h"

#include "DMA Thread.h"
#include "Input/Input Manager.h"
#include "IGameContext.h"

#pragma comment(lib, "Winmm.lib")

IGameContext* g_GameContext = nullptr;

extern std::atomic<bool> bRunning;

void DMA_Thread_Main()
{
	Log::Info("[DMA] thread started");

	DMA_Connection* conn = DMA_Connection::GetInstance();

	c_keys::InitKeyboard(conn);

	if (!g_GameContext || !g_GameContext->Initialize(conn))
	{
		Log::Error("[DMA] init failed, exiting");
		bRunning = false;
		return;
	}

	timeBeginPeriod(1);

	auto lastTlbRefresh = std::chrono::steady_clock::now();
	auto lastMemRefresh = lastTlbRefresh;

	while (bRunning)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		auto now = std::chrono::steady_clock::now();

		// MEM cache: physical-page contents. 50ms keeps scatter reads fresh
		// enough that per-frame QuickPawn/ViewMatrix never see stale values.
		if (now - lastMemRefresh >= std::chrono::milliseconds(50))
		{
			VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_MEM, 1);
			lastMemRefresh = now;
		}

		// TLB cache: virtual→physical page translations. Was 5s, which meant
		// after a map/session transition (page tables shift) VMMDLL kept using
		// stale mappings and entities appeared to vanish until the next flush.
		// 250ms full refresh caps that stall at a quarter second — indistinguishable
		// from live to the ESP. TLB refresh is heavier than MEM but still cheap.
		if (now - lastTlbRefresh >= std::chrono::milliseconds(250))
		{
			VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
			lastTlbRefresh = now;
		}

		g_GameContext->Tick(conn, now);
	}

	timeEndPeriod(1);

	conn->EndConnection();
}
