#include "pch.h"

#include "DMA Thread.h"
#include "Input/Input Manager.h"
#include "IGameContext.h"

#pragma comment(lib, "Winmm.lib")

IGameContext* g_GameContext = nullptr;

extern std::atomic<bool> bRunning;

void DMA_Thread_Main()
{
	Log::Info("[DMA Thread] DMA Thread started.");

	DMA_Connection* conn = DMA_Connection::GetInstance();

	c_keys::InitKeyboard(conn);

	if (!g_GameContext || !g_GameContext->Initialize(conn))
	{
		Log::Error("[DMA Thread] Game initialization failed, requesting exit.");
		bRunning = false;
		return;
	}

	timeBeginPeriod(1);

	while (bRunning)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		g_GameContext->Tick(conn, std::chrono::steady_clock::now());
	}

	timeEndPeriod(1);

	conn->EndConnection();
}
