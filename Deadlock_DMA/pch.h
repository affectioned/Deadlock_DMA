#pragma once
#include <thread>
#include <chrono>
#include <string>
#include <print>
#include <vector>
#include <functional>
#include <unordered_map>
#include <array>
#include <memory>
#include <mutex>

#define NOMINMAX
#include <algorithm>

#ifdef DBGPRINT
#define DbgPrintln(...) std::println(__VA_ARGS__)
#else
#define DbgPrintln(...)
#endif

#include "vmmdll.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "DMA/DMA.h"
#include "DMA/ScatterRead.h"
#include "DMA/Process.h"

#include "Deadlock/Offsets.h"
#include "Deadlock/Deadlock.h"