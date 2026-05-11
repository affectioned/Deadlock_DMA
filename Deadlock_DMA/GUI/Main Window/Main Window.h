#pragma once

extern "C" __declspec(dllexport) void Render(ImGuiContext* ctx);

class MainWindow
{
public:
	static inline ID3D11Device* g_pd3dDevice = nullptr;
	static inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
	static inline IDXGISwapChain* g_pSwapChain = nullptr;
	static inline bool g_SwapChainOccluded = false;
	static inline UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
	static inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
	// Transparent clear: per-pixel alpha is what makes the layered overlay see-through.
	// Anything ImGui draws on top remains opaque (or as alpha'd) as-is.
	static inline ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	static inline WNDCLASSEXW wc{};

public:
	static bool CreateDeviceD3D(HWND hWnd);
	static void CleanupDeviceD3D();
	static void CreateRenderTarget();
	static void CleanupRenderTarget();
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	static bool Initialize();
	static bool OnFrame();
	static bool Cleanup();

private:
	static bool PreFrame();
	static bool PostFrame();

public:
	static inline HWND g_hWnd = 0;

	// Monitor selection. EnumerateMonitors returns the current displays in the order
	// EnumDisplayMonitors visits them. g_MonitorIndex is persisted via Config; an
	// out-of-range value falls back to the primary monitor at startup.
	struct MonitorInfo
	{
		int x, y, w, h;
		bool primary;
		std::wstring device;   // \\.\DISPLAY1 etc.
	};
	static std::vector<MonitorInfo> EnumerateMonitors();

	// Defer monitor change: SetWindowPos + ResizeBuffers + RTV recreate happens between
	// frames in PreFrame. Calling mid-frame (e.g. from a button click during ImGui draw)
	// would race the active swap-chain present.
	static void RequestMonitorApply(int index);

	static inline int g_MonitorIndex = 0;
};