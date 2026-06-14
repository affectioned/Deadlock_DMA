#include "pch.h"

#include "DMA Thread.h"
#include "Input/Input Manager.h"
#include "IGameContext.h"

#pragma comment(lib, "Winmm.lib")

IGameContext* g_GameContext = nullptr;

extern std::atomic<bool> bRunning;

void DMA_Thread_Main()
{
	Log::Info("[DMAThread]: Started.");

	DMA_Connection* conn = DMA_Connection::GetInstance();

	c_keys::InitKeyboard(conn);

	if (!g_GameContext || !g_GameContext->Initialize(conn))
	{
		Log::Error("[DMAThread]: Game initialization failed, requesting exit.");
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

		if (now - lastMemRefresh >= std::chrono::milliseconds(100))
		{
			VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_MEM, 1);
			lastMemRefresh = now;
		}

		if (now - lastTlbRefresh >= std::chrono::seconds(5))
		{
			VMMDLL_ConfigSet(conn->GetHandle(), VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
			lastTlbRefresh = now;
		}

		g_GameContext->Tick(conn, now);
	}

	timeEndPeriod(1);

	conn->EndConnection();
}
