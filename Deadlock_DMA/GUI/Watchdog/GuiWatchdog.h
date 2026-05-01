#pragma once
#include <atomic>
#include <cstdint>

class GuiWatchdog
{
public:
	static void Start();
	static void GuiStage(const char* name);
	static void DmaStage(const char* name);
	static void Tick();

	static inline std::atomic<const char*> s_LastGuiStage{ "init" };
	static inline std::atomic<const char*> s_LastDmaStage{ "init" };
	static inline std::atomic<uint64_t>    s_FrameCounter{ 0 };

private:
	static inline std::atomic<bool> s_Running{ false };
	static void WatchdogLoop();
};

