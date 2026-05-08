#include "pch.h"
#include <filesystem>

#include "GUI/Main Window/Main Window.h"
#include "DMA/DMA Thread.h"
#include "GUI/Config/Config.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/DeadlockContext.h"

std::atomic<bool> bRunning{ true };

// Catches console-close events (X button, Ctrl+C, Ctrl+Break, logoff/shutdown)
// so the active config persists when the user closes the window without
// pressing END. The OS gives this handler ~5 seconds before forcefully
// terminating the process for CTRL_CLOSE_EVENT, which is plenty for a JSON
// write. Returning TRUE tells the OS we handled the event.
static BOOL WINAPI OnConsoleExit(DWORD ctrlType)
{
	switch (ctrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		Log::Info("[Exit] Console close detected; saving active config...");
		Config::SaveActive();
		bRunning = false;
		return TRUE;
	}
	return FALSE;
}

int main()
{
	{
		wchar_t exePath[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		auto logPath = std::filesystem::path(exePath).parent_path() / "deadlock_dma.log";
		Log::Init(logPath.wstring());
	}

	Log::Info("Hello, DEADLOCK_DMA!");

	SetConsoleCtrlHandler(OnConsoleExit, TRUE);

	Config::LoadConfig("default");

	MainWindow::Initialize();

	MyMakcu::Initialize();

	g_GameContext = new DeadlockContext();

	GuiWatchdog::Start();

	std::thread DMAThread(DMA_Thread_Main);

	Log::Info("Press END to exit...");

	while (bRunning)
	{
		if (GetAsyncKeyState(VK_END) & 0x1) bRunning = false;
		MainWindow::OnFrame();
	}

	// Covers the END-key path. Console-close goes through OnConsoleExit which
	// also saves; the resulting double-save is harmless and idempotent.
	Config::SaveActive();

	DMAThread.join();

	system("pause");

	return 0;
}
