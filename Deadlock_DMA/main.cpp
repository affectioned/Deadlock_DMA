#include "pch.h"
#include <filesystem>

#include "GUI/Main Window/Main Window.h"
#include "DMA/DMA Thread.h"
#include "GUI/Config/Config.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/DeadlockContext.h"
#include "Bootstrap/Bootstrap.h"
#include "DMA/Memory/PhaseTimings.h"

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
		Log::Info("[Exit] saving config");
		Config::SaveActive();
		bRunning = false;
		return TRUE;
	}
	return FALSE;
}

static PROCESS_INFORMATION s_tracyProc{};

static void LaunchTracyCapture(const std::filesystem::path& exeDir)
{
	auto capture = exeDir / "tracy-capture.exe";
	if (!std::filesystem::exists(capture)) return;

	auto traceFile = exeDir / "trace.tracy";
	std::wstring cmd = L"\"" + capture.wstring() + L"\" -o \"" + traceFile.wstring() + L"\" -f";

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, exeDir.wstring().c_str(), &si, &s_tracyProc))
	{
		Log::Info("[Tracy] capture started (pid=%lu)", s_tracyProc.dwProcessId);
	}
}

static void StopTracyCapture()
{
	if (!s_tracyProc.hProcess) return;
	GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, s_tracyProc.dwProcessId);
	if (WaitForSingleObject(s_tracyProc.hProcess, 3000) == WAIT_TIMEOUT)
		TerminateProcess(s_tracyProc.hProcess, 0);
	CloseHandle(s_tracyProc.hProcess);
	CloseHandle(s_tracyProc.hThread);
	s_tracyProc = {};
}

int main(int argc, char** argv)
{
	std::filesystem::path exeDir;
	{
		wchar_t exePath[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		exeDir = std::filesystem::path(exePath).parent_path();
		auto logPath = exeDir / "deadlock_dma.log";
		Log::Init(logPath.wstring());
	}

	bool tracy  = false;
	bool uiOnly = false;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--tracy") == 0) tracy = true;
		else if (strcmp(argv[i], "--ui-only") == 0) uiOnly = true;
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			printf("Deadlock DMA\n\n"
			       "  --ui-only   Run the overlay and menu with no DMA connection. Skips the\n"
			       "              MemProcFS bootstrap, the Makcu link and the DMA thread, so the\n"
			       "              UI can be reviewed without the game or a second PC. Entity\n"
			       "              lists stay empty, so nothing is drawn in the world.\n"
			       "  --tracy     Launch tracy-capture and write profiling_report.json.\n"
			       "  --help      This text.\n");
			return 0;
		}
	}

	if (tracy)
	{
		LaunchTracyCapture(exeDir);
		PhaseTimings::SetJsonPath(exeDir / "profiling_report.json");
		Log::Info("[Tracy] profiling report -> profiling_report.json");
	}

	SetConsoleCtrlHandler(OnConsoleExit, TRUE);

	// Must run before anything touches vmm.dll — the DLL is delay-loaded so
	// the process reaches this point even if the DLLs are missing, but the
	// first VMMDLL_* call will fault otherwise. --ui-only never reaches a
	// VMMDLL_* call, so it skips the download entirely.
	if (!uiOnly && !Bootstrap::EnsureRuntimeDlls())
	{
		Log::Error("Bootstrap failed; MemProcFS DLLs unavailable. Aborting.");
		system("pause");
		return 1;
	}

	Config::LoadConfig("default");

	MainWindow::Initialize();

	if (uiOnly)
		Log::Info("[UI] --ui-only: no DMA thread, no Makcu, entity lists stay empty");
	else
		MyMakcu::Initialize();

	std::thread DMAThread;
	if (!uiOnly)
	{
		g_GameContext = new DeadlockContext();
		DMAThread = std::thread(DMA_Thread_Main);
	}

	GuiWatchdog::Start();

	Log::Info("END to exit");

	while (bRunning)
	{
		if (GetAsyncKeyState(VK_END) & 0x1) bRunning = false;
		MainWindow::OnFrame();
	}

	// Covers the END-key path. Console-close goes through OnConsoleExit which
	// also saves; the resulting double-save is harmless and idempotent.
	Config::SaveActive();

	PhaseTimings::FinalizeJson();
	StopTracyCapture();

	if (DMAThread.joinable())
		DMAThread.join();

	system("pause");

	return 0;
}
