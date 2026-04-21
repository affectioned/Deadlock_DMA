#include "pch.h"
#include <filesystem>

#include "GUI/Main Window/Main Window.h"
#include "DMA/DMA Thread.h"
#include "GUI/Config/Config.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/DeadlockContext.h"

std::atomic<bool> bRunning{ true };

int main()
{
	{
		wchar_t exePath[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		auto logPath = std::filesystem::path(exePath).parent_path() / "deadlock_dma.log";
		Log::Init(logPath.wstring());
	}

	Log::Info("Hello, DEADLOCK_DMA!");

	Config::LoadConfig("default");

	MainWindow::Initialize();

	MyMakcu::Initialize();

	g_GameContext = new DeadlockContext();

	std::thread DMAThread(DMA_Thread_Main);

	Log::Info("Press END to exit...");

	while (bRunning)
	{
		if (GetAsyncKeyState(VK_END) & 0x1) bRunning = false;
		MainWindow::OnFrame();
	}

	DMAThread.join();

	system("pause");

	return 0;
}
