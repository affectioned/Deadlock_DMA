#pragma once
#include <thread>
#include <chrono>
#include <string>
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

#include "DMA/DMA.h"
#include "DMA/Memory/ScatterRead.h"
#include "DMA/Memory/Process.h"
#include "DMA/Logging/Log.h"

// Deadlock/Offsets.h and Deadlock/Deadlock.h are deliberately NOT in the PCH.
// Both change often; including them here forced a full PCH rebuild (which
// invalidates every TU) on every edit. Consumers include them explicitly.
//
// <print> and the ImGui backend headers (imgui_impl_win32/dx11) are also
// intentionally out of PCH — <print> is C++23 and heavy, ImGui backends drag
// in extra Windows/D3D11 machinery, and both are used by only 1–2 TUs. Every
// other TU used to pay their compile cost for no reason.