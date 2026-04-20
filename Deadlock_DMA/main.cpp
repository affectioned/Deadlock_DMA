#include "pch.h"
#include <filesystem>

#include "Deadlock/Deadlock.h"
#include "GUI/Main Window/Main Window.h"

#include "DMA/DMA Thread.h"

#include "GUI/Config/Config.h"

#include "Makcu/MyMakcu.h"

std::atomic<bool> bRunning{ true };

int main()
{
	{
		wchar_t exePath[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		auto logPath = std::filesystem::path(exePath).parent_path() / "deadlock_dma.log";
		_wfreopen(logPath.c_str(), L"w", stdout);
		setvbuf(stdout, nullptr, _IONBF, 0);
	}

	std::println("Hello, DEADLOCK_DMA!");

	Config::LoadConfig("default");

	MainWindow::Initialize();

	MyMakcu::Initialize();

	std::thread DMAThread(DMA_Thread_Main);

	std::println("Press END to exit...");

	while (bRunning)
	{
		if (GetAsyncKeyState(VK_END) & 0x1) bRunning = false;
		MainWindow::OnFrame();
	}

	DMAThread.join();

	system("pause");

	return 0;
}
