#pragma once
#include <thread>
#include <chrono>
#include <string>
#include <print>
#include <vector>
#include <functional>
#include <unordered_map>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

#define NOMINMAX
#include <algorithm>

#include "vmmdll.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "DMA/DMA.h"
#include "DMA/Memory/ScatterRead.h"
#include "DMA/Memory/Process.h"
#include "DMA/Logging/Log.h"

// Deadlock/Offsets.h and Deadlock/Deadlock.h are deliberately NOT in the PCH.
// Both change often; including them here forced a full PCH rebuild (which
// invalidates every TU) on every edit. Consumers include them explicitly.