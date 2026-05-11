#include "pch.h"

#include "Main Window.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "GUI/Main Menu/Main Menu.h"
#include "GUI/Fonts/Fonts.h"
#include "GUI/Radar/Radar.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Watchdog/GuiWatchdog.h"
#include "GUI/Input/KeyboardPump.h"

namespace
{
	// -1 means "no apply pending". Drained in PreFrame between frames.
	int s_PendingMonitorIndex = -1;

	BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp)
	{
		auto& list = *reinterpret_cast<std::vector<MainWindow::MonitorInfo>*>(lp);
		MONITORINFOEXW mi{};
		mi.cbSize = sizeof(mi);
		if (::GetMonitorInfoW(hMon, &mi))
		{
			list.push_back(MainWindow::MonitorInfo{
				mi.rcMonitor.left,
				mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				(mi.dwFlags & MONITORINFOF_PRIMARY) != 0,
				std::wstring{ mi.szDevice }
			});
		}
		return TRUE;
	}
}

std::vector<MainWindow::MonitorInfo> MainWindow::EnumerateMonitors()
{
	std::vector<MonitorInfo> list;
	::EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&list));
	return list;
}

void MainWindow::RequestMonitorApply(int index)
{
	s_PendingMonitorIndex = index;
}

void Render(ImGuiContext* ctx)
{
	ImGui::SetCurrentContext(ctx);

	if (Fonts::m_IBMPlexMonoSemiBold == nullptr)
		Fonts::Initialize(ImGui::GetIO());

	ImGui::PushFont(Fonts::m_IBMPlexMonoSemiBold, 16.0f);

	// Overlay layer — fullscreen ESP draw + minimap. Always on (gated by their
	// own master toggles).
	GuiWatchdog::GuiStage("Fuser::Render");
	Fuser::Render();
	GuiWatchdog::GuiStage("Radar::Render");
	Radar::Render();

	// Settings layer — single tabbed window hosts every panel's body
	// (Aimbot/Fuser/ESP/Colors/Keybinds/Config/PlayerList/TrooperList/ClassList).
	GuiWatchdog::GuiStage("MainMenu::Render");
	MainMenu::Render();

	ImGui::PopFont();
}

bool MainWindow::OnFrame()
{
	GuiWatchdog::GuiStage("MainWindow::PreFrame");
	PreFrame();

	GuiWatchdog::GuiStage("MainWindow::Render");
	Render(ImGui::GetCurrentContext());

	GuiWatchdog::GuiStage("MainWindow::PostFrame");
	PostFrame();

	GuiWatchdog::GuiStage("MainWindow::idle");
	GuiWatchdog::Tick();
	return true;
}

bool MainWindow::CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	// Disable DXGI's default Alt+Enter fullscreen behavior.
	// - You are free to leave this enabled, but it will not work properly with multiple viewports.
	// - This must be done for all windows associated to the device. Our DX11 backend does this automatically for secondary viewports that it creates.
	IDXGIFactory* pSwapChainFactory;
	if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory))))
		pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	CreateRenderTarget();
	return true;
}

void MainWindow::CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void MainWindow::CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void MainWindow::CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool MainWindow::Initialize()
{
	ImGui_ImplWin32_EnableDpiAwareness();

	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	int originX = 0, originY = 0;
	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);
	{
		auto monitors = EnumerateMonitors();
		if (g_MonitorIndex >= 0 && g_MonitorIndex < (int)monitors.size())
		{
			const auto& m = monitors[g_MonitorIndex];
			originX = m.x; originY = m.y;
			screenW = m.w; screenH = m.h;
		}
	}

	wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"MainWindow", nullptr };
	::RegisterClassExW(&wc);

	// Layered fullscreen overlay: WS_POPUP for no chrome; WS_EX_LAYERED + LWA_ALPHA(255)
	// + DwmExtendFrameIntoClientArea makes DWM honor the swap chain's per-pixel alpha so
	// transparent pixels (clear_color a=0) actually pass through. WS_EX_NOACTIVATE keeps
	// focus on the game; WS_EX_TOPMOST keeps the overlay above borderless game windows.
	g_hWnd = ::CreateWindowExW(
		WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
		wc.lpszClassName, L"DEADLOCK DMA",
		WS_POPUP,
		originX, originY, screenW, screenH,
		nullptr, nullptr, wc.hInstance, nullptr);

	MARGINS margins{ -1, -1, -1, -1 };
	::DwmExtendFrameIntoClientArea(g_hWnd, &margins);
	::SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);

	// Initialize Direct3D
	if (!CreateDeviceD3D(g_hWnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(g_hWnd, SW_SHOWDEFAULT);
	::UpdateWindow(g_hWnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	// Docking + multi-viewport disabled: viewports reparent ImGui windows as native OS
	// windows, which fights the single layered-overlay HWND we just created.

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
	io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(g_hWnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	return false;
}

bool MainWindow::Cleanup()
{
	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(g_hWnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return false;
}

bool MainWindow::PreFrame()
{
	MSG msg;
	while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
			return false;
	}

	// Handle window resize (we don't resize directly in the WM_SIZE handler)
	if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
	{
		CleanupRenderTarget();
		g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
		g_ResizeWidth = g_ResizeHeight = 0;
		CreateRenderTarget();
	}

	// Drain pending monitor switch. Done between frames so no draws are mid-flight
	// when the swap-chain back buffer is reallocated.
	if (s_PendingMonitorIndex >= 0)
	{
		auto monitors = EnumerateMonitors();
		if (s_PendingMonitorIndex < (int)monitors.size())
		{
			const auto& m = monitors[s_PendingMonitorIndex];
			CleanupRenderTarget();
			::SetWindowPos(g_hWnd, HWND_TOPMOST, m.x, m.y, m.w, m.h,
				SWP_NOACTIVATE | SWP_SHOWWINDOW);
			g_pSwapChain->ResizeBuffers(0, m.w, m.h, DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
			g_MonitorIndex = s_PendingMonitorIndex;
		}
		s_PendingMonitorIndex = -1;
	}

	// Pump keyboard state into ImGui IO. WS_EX_NOACTIVATE means WM_KEYDOWN/WM_CHAR
	// never reach our WndProc, so we synthesize events from polled key state.
	GuiInput::PumpKeyboard();

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	return true;
}

bool MainWindow::PostFrame()
{
	ImGui::Render();
	const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
	g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present with VSync toggle from Main Menu
	HRESULT hr = g_pSwapChain->Present(MainMenu::bVSync ? 1 : 0, 0);

	return true;
}