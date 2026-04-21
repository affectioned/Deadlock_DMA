# DMA Client Base — Specification

This document specifies the compile-ready base layer for a DMA client. The base is intentionally game-agnostic. Game-specific logic (offsets, entity types, game state) plugs in on top through defined interfaces. Every section describes the exact API contract that must be satisfied for the project to compile.

---

## Directory Layout

```
DMA/
  DMA.h / DMA.cpp                   — VMM connection singleton
  IGameContext.h                     — IGameContext interface + g_GameContext extern
  Process.h / Process.cpp            — process + module discovery, single-value reads
  ScatterRead.h                      — RAII scatter read wrapper (header-only)
  SigScan.h / SigScan.cpp            — byte-pattern scan, address probe
  DMA Thread.h / DMA Thread.cpp      — CTimer, Timer alias, acquisition loop entry point
  Input Manager.h / Input Manager.cpp — kernel keystroke reader (gafAsyncKeyState)

Deadlock/                            — game layer (not part of the base)
  GameModules.h                      — process name + module list
  DeadlockContext.h / .cpp           — concrete IGameContext, owns all timers
  Const/EntityConfig.h               — MAX_ENTITIES / MAX_ENTITY_LISTS

main.cpp                             — entry point, thread launch, main loop
pch.h / pch.cpp                      — precompiled header
```

---

## pch.h

### Purpose
Single-include precompiled header. Everything every TU needs lives here.

### Required contents
```cpp
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
#include <atomic>

#define NOMINMAX
#include <algorithm>

// Debug-only print macro — no-op in Release to eliminate I/O pressure.
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

// DMA layer — include order is mandatory: DMA → ScatterRead → Process.
#include "DMA/DMA.h"
#include "DMA/ScatterRead.h"
#include "DMA/Process.h"
```

### Notes
- Do **not** include any game-layer headers here. The base must compile without a game layer present.
- `MAX_ENTITIES` / `MAX_ENTITY_LISTS` are game-specific — they live in `Deadlock/Const/EntityConfig.h`, not here.
- Every `.cpp` file in the project must `#include "pch.h"` as its **absolute first line**. This is an MSVC precompiled header requirement — any include before `pch.h` breaks the PCH and produces cryptic compile errors.

---

## DMA.h / DMA.cpp

### Purpose
Owns the single `VMM_HANDLE`. All other subsystems receive a `DMA_Connection*` and call `GetHandle()` rather than holding the handle themselves.

### API contract
```cpp
class DMA_Connection
{
public:
    // Returns (and lazily creates) the singleton instance.
    // Throws std::runtime_error if VMMDLL_Initialize fails.
    static DMA_Connection* GetInstance();

    VMM_HANDLE GetHandle() const;

    // Closes the VMM handle. Called by DMA_Thread_Main on exit.
    bool EndConnection();

private:
    DMA_Connection();   // calls VMMDLL_Initialize("-device", "fpga", "-waitinitialize")
    ~DMA_Connection();  // calls VMMDLL_Close

    static inline DMA_Connection* m_Instance = nullptr;
    VMM_HANDLE m_VMMHandle = nullptr;
};
```

### Notes
- `GetInstance()` creates `m_Instance` with `new` on first call; never recreates it.
- Constructor failure must throw — callers have no way to check a partially-constructed singleton.
- The `"fpga"` device string is currently hardcoded. To support other devices (USB3380, file replay) without recompiling, expose a `SetDevice(const char*)` that must be called before `GetInstance()`.

---

## IGameContext.h

### Purpose
Interface between the base DMA thread and the game layer. The DMA thread calls through this interface; the game layer provides a concrete implementation. Defined in the base (`DMA/`), implemented in the game layer.

### API contract
```cpp
// DMA/IGameContext.h
// Note: this header cannot include pch.h (it is included by headers that pch.h
// itself pulls in). Forward-declare DMA_Connection instead of including DMA.h.
class DMA_Connection;

struct IGameContext
{
    // Called once after DMA_Connection is ready and keyboard is initialized.
    // Return false to abort — sets bRunning = false and exits the thread.
    virtual bool Initialize(DMA_Connection* conn) = 0;

    // Called every tick (~1 ms). Tick all timers here.
    virtual void Tick(DMA_Connection* conn,
                      std::chrono::steady_clock::time_point now) = 0;

    virtual ~IGameContext() = default;
};

// Defined in DMA Thread.cpp. Set by main() before launching DMA_Thread_Main.
extern IGameContext* g_GameContext;
```

### Notes
- `Shutdown` is intentionally absent. The interface only deals with initialization and per-tick work. There is nothing to clean up when all state lives in timers.
- `g_GameContext` is **declared** (`extern`) in `IGameContext.h` and **defined** (`= nullptr`) in `DMA Thread.cpp`. `main.cpp` assigns it before launching the thread. Do not define it anywhere else or you will get a duplicate symbol linker error.

---

## ScatterRead.h

### Purpose
RAII wrapper around `VMMDLL_SCATTER_HANDLE`. Header-only. One instance per thread; never share across threads.

### API contract
```cpp
class ScatterRead
{
public:
    ScatterRead(VMM_HANDLE hVMM, DWORD pid, DWORD flags = VMMDLL_FLAG_NOCACHE);
    ~ScatterRead();

    // Non-copyable.
    ScatterRead(const ScatterRead&) = delete;
    ScatterRead& operator=(const ScatterRead&) = delete;

    // Queue a typed single-value read. Written on Execute().
    template<typename T>
    void Add(uintptr_t addr, T* out);

    // Queue a raw byte-range read (bone arrays, strings, bulk structs).
    void AddRaw(uintptr_t addr, DWORD cb, void* out);

    // Fire all queued reads in one DMA transaction.
    void Execute();

    // Reset for the next batch. Retains the same PID and flags.
    void Clear();

private:
    DWORD                 m_Pid;
    DWORD                 m_Flags;
    VMMDLL_SCATTER_HANDLE m_Handle;
};
```

### Usage pattern
```cpp
ScatterRead sr(conn->GetHandle(), pid);
sr.Add(addr1, &val1);
sr.Add(addr2, &val2);
sr.Execute();
// use val1, val2
sr.Clear();
sr.Add(addr3, &val3);
sr.Execute();
```

Never call `Execute()` on an empty handle and never add to a handle after `Execute()` without calling `Clear()` first.

---

## Process.h / Process.cpp

### Purpose
Finds the target process by name, resolves a caller-supplied list of module base addresses and sizes, and exposes `ReadMem<T>` for one-off single-value reads.

### API contract
```cpp
class Process
{
public:
    // Blocks until the process is found, then resolves every module in moduleNames.
    bool GetProcessInfo(const std::string& processName,
                        const std::vector<std::string>& moduleNames,
                        DMA_Connection* conn);

    // Returns 0 if name was not in moduleNames at init time.
    uintptr_t GetModuleBase(const std::string& name) const;
    size_t    GetModuleSize(const std::string& name) const;
    DWORD     GetPID() const;

    template<typename T>
    T ReadMem(DMA_Connection* conn, uintptr_t address);

private:
    DWORD m_PID{ 0 };
    std::unordered_map<std::string, uintptr_t> m_Modules;
    std::unordered_map<std::string, size_t>    m_ModuleSizes;

    bool PopulateModules(const std::vector<std::string>& names, DMA_Connection* conn);
};
```

The module name strings are owned entirely by the game layer (`GameModules.h`). `Process` has no hardcoded names.

### Implementation rules
- `GetProcessInfo` polls with a 1-second sleep until the PID is non-zero.
- `PopulateModules` polls until every requested module resolves; some DLLs load after the process starts.
- `ReadMem<T>` creates a temporary `ScatterRead` internally. Only use it for one-off reads not on the hot path. Per-frame reads must use a persistent `ScatterRead`.

---

## SigScan.h / SigScan.cpp

### Purpose
Scans a memory range for a byte-pattern signature, validates addresses, and provides a single-value scatter read helper against an arbitrary PID.

### API contract
```cpp
// Returns all PIDs whose long name contains 'name'.
std::vector<int> GetPidListFromName(DMA_Connection* conn, std::string name);

// Scans [rangeStart, rangeEnd) for a pattern.
// Pattern format: "48 8B 05 ? ? ? ?" — '?' is a wildcard byte.
// Returns the address of the first match, or 0 on failure.
uint64_t FindSignature(DMA_Connection* conn, const char* signature,
                       uint64_t rangeStart, uint64_t rangeEnd, int pid);

// Returns true if addr is mapped and readable in pid.
bool IsAddressReadable(DMA_Connection* conn, uint64_t addr, DWORD pid);

// Single-value scatter read against any PID.
// Used by SigScan helpers and Input Manager; not for hot-path game reads.
template<typename T>
T ReadFromPID(DMA_Connection* conn, uintptr_t address, DWORD pid);
```

### Notes
- `FindSignature` bulk-reads the entire range into a `std::vector<uint8_t>` and scans locally. Never do byte-by-byte DMA reads over the range.
- The hex digit lookup table in `SigScan.cpp` is a compile-time `static const char[]`. Do not replace it with `std::from_chars` or `sscanf`.
- `IsAddressReadable` is a single-byte probe used only at initialization time (offset resolution), not per-frame.

---

## DMA Thread.h / DMA Thread.cpp

### Purpose
`CTimer` — lightweight interval executor templated on duration and callable. `Timer` — type-erased alias for storing mixed-interval timers in a container. `DMA_Thread_Main` — dedicated acquisition thread entry point.

### CTimer contract
```cpp
template<typename T, typename F>
class CTimer
{
public:
    CTimer(T interval, F function);

    // Call every loop iteration. Executes function() if interval has elapsed.
    void Tick(std::chrono::steady_clock::time_point now);

private:
    T  m_Interval;
    F  m_Function;
    std::chrono::steady_clock::time_point m_LastExecutionTime{};
};
```

### Timer alias
```cpp
// Fixes CTimer to milliseconds + std::function so instances can be stored in a vector.
// All intervals passed to Timer must be expressed in milliseconds.
using Timer = CTimer<std::chrono::milliseconds, std::function<void()>>;
```

Use `CTimer<T, F>` directly when the type can be deduced (e.g. a single named timer). Use `Timer` when you need to store multiple timers in a `std::vector<Timer>`, as `DeadlockContext` does.

`DMA Thread.h` must include `IGameContext.h` at the top so that any file including `DMA Thread.h` (e.g. `DeadlockContext.h`) also gets the `Timer` alias and the `g_GameContext` extern in one include.

### DMA_Thread_Main
`DMA Thread.cpp` must contain:
- `#pragma comment(lib, "Winmm.lib")` — required for `timeBeginPeriod`/`timeEndPeriod`.
- `IGameContext* g_GameContext = nullptr;` — the definition (not just declaration) of the global.
- `extern std::atomic<bool> bRunning;` — defined in `main.cpp`, referenced here.

```cpp
void DMA_Thread_Main()
{
    DMA_Connection* conn = DMA_Connection::GetInstance();

    c_keys::InitKeyboard(conn);  // keyboard init is owned by the thread, not the game context

    if (!g_GameContext || !g_GameContext->Initialize(conn))
    {
        bRunning = false;
        return;
    }

    timeBeginPeriod(1);
    while (bRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g_GameContext->Tick(conn, std::chrono::steady_clock::now());
    }
    timeEndPeriod(1);

    conn->EndConnection();
}
```

No game-layer headers are included in `DMA Thread.cpp`. All game logic is behind `g_GameContext`.

---

## Input Manager.h / Input Manager.cpp

### Purpose
Reads the kernel `gafAsyncKeyState` bitmap via DMA to detect key presses without any user-mode API calls. Works on both Windows 10 (EAT export path) and Windows 11 (session pointer path).

### c_keys API
```cpp
class c_keys
{
public:
    // Must be called once from DMA_Thread_Main before any IsKeyDown call.
    static bool InitKeyboard(DMA_Connection* conn);

    static bool IsInitialized();

    // Refreshes the key state bitmap (internally rate-limited to 100 ms).
    static void UpdateKeys(DMA_Connection* conn);

    // Returns true if the given virtual key is currently held.
    static bool IsKeyDown(DMA_Connection* conn, uint32_t virtualKeyCode);

private:
    static inline uint64_t gafAsyncKeyStateExport = 0;
    static inline uint8_t  state_bitmap[64]{};
    static inline uint8_t  previous_state_bitmap[256 / 8]{};
    static inline uint64_t win32kbase = 0;
    static inline int      win_logon_pid = 0;
    static inline c_registry registry;
    static inline std::chrono::time_point<std::chrono::system_clock> start
        = std::chrono::system_clock::now();
    static inline bool m_bInitialized = false;
};
```

### InitKeyboard flow
1. Read `HKLM\...\CurrentBuild` and `UBR` to determine the Windows build number.
2. **Windows 11 (build > 22000):** locate `win32ksgd.sys` or `win32k.sys` in each `csrss.exe` PID; scan for `g_session_global_slots` signature; walk the session pointer chain to `gafAsyncKeyStateExport`.
3. **Windows 10 and earlier:** read the EAT of `win32kbase.sys` in `winlogon.exe` for the `gafAsyncKeyState` export; fall back to PDB symbols.

### Notes
- `InitKeyboard` is called by `DMA_Thread_Main` before `g_GameContext->Initialize`. Do not call it from the game context.
- `UpdateKeys` is called inside `IsKeyDown` automatically when the 100 ms cooldown elapses.
- Fully game-agnostic — no changes needed when porting.

---

## main.cpp

### Purpose
Entry point. Sets up logging, initialises subsystems in order, sets `g_GameContext`, launches the DMA thread, and runs the GUI main loop.

### Required implementation
```cpp
#include "pch.h"
#include <filesystem>

#include "GUI/Main Window/Main Window.h"
#include "DMA/DMA Thread.h"
#include "GUI/Config/Config.h"
#include "Makcu/MyMakcu.h"
#include "Deadlock/DeadlockContext.h"  // swap this for another game's context

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

    g_GameContext = new DeadlockContext();

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
```

### Global ownership
- `std::atomic<bool> bRunning{ true }` — **defined in `main.cpp`**, referenced as `extern` in `DMA Thread.cpp`. Controls the acquisition loop and the main window loop. Set to `false` to trigger shutdown from either thread.

### Startup order (mandatory)
1. Log redirection — must be first.
2. `Config::LoadConfig` — config values must be ready before window/renderer init.
3. `MainWindow::Initialize` — creates the D3D11 device and ImGui context.
4. Mouse emulation init (if applicable).
5. `g_GameContext = new ...` — must be set before the thread starts.
6. `std::thread(DMA_Thread_Main)` — DMA thread starts here.
7. Main loop — `MainWindow::OnFrame()` each iteration, END key check.
8. `join()` — waits for `EndConnection()` to complete.

---

## Game Layer — What to Implement

These files are **not** part of the base. They are required for the project to link.

### GameModules.h
Defines the process name and the module list passed to `Process::GetProcessInfo`.

```cpp
namespace GameModules
{
    inline constexpr const char* ProcessName = "deadlock.exe";
    inline constexpr const char* ClientDll   = "client.dll";

    inline std::vector<std::string> ModuleList()
    {
        return { ProcessName, ClientDll };
    }
}
```

Add entries to `ModuleList()` for any additional DLLs you need base addresses for.

### Const/EntityConfig.h
Engine-level entity system constants. Values are Source 2 / Deadlock-specific.

```cpp
inline constexpr size_t MAX_ENTITIES     = 512;
inline constexpr size_t MAX_ENTITY_LISTS = 64;
```

### DeadlockContext : IGameContext
Concrete context. `Initialize` calls game-state init and registers all timers. `Tick` calls `t.Tick(now)` on each. Uses `std::vector<Timer>` (`Timer` = `CTimer<milliseconds, function<void()>>`).

```cpp
class DeadlockContext : public IGameContext
{
public:
    bool Initialize(DMA_Connection* conn) override;
    void Tick(DMA_Connection* conn, std::chrono::steady_clock::time_point now) override;
private:
    std::vector<Timer> m_Timers;
};
```

### Timer intervals (Deadlock)
| Timer                  | Interval  |
|------------------------|-----------|
| ViewMatrix             | 2 ms      |
| ClientYaw              | 10 ms     |
| ServerTime             | 1000 ms   |
| LocalControllerAddress | 15000 ms  |
| FullTrooper            | 3000 ms   |
| QuickTrooper           | 16 ms     |
| FullPawn               | 2000 ms   |
| QuickPawn              | 8 ms      |
| FullMonsterCamp        | 2000 ms   |
| QuickMonsterCamp       | 100 ms    |
| FullController         | 2000 ms   |
| QuickController        | 150 ms    |
| FullSinner             | 1000 ms   |
| FullXpOrb              | 500 ms    |
| QuickXpOrb             | 16 ms     |
| FullUpdate             | 5000 ms   |
| Keybinds               | 5 ms      |

---

## Compile Checklist

- [ ] Every `.cpp` file has `#include "pch.h"` as its first line.
- [ ] `pch.h` has no game-layer includes and no game-specific constants.
- [ ] `DMA/IGameContext.h` forward-declares `DMA_Connection` instead of including `DMA.h`.
- [ ] `DMA/DMA Thread.h` includes `IGameContext.h` at the top.
- [ ] `DMA/DMA Thread.cpp` defines `IGameContext* g_GameContext = nullptr` and `extern std::atomic<bool> bRunning`.
- [ ] `DMA/DMA Thread.cpp` has `#pragma comment(lib, "Winmm.lib")`.
- [ ] `DMA/DMA Thread.cpp` has no game-layer includes (`EntityList`, `Deadlock`, etc.).
- [ ] `DMA/Process.h` has no hardcoded module names.
- [ ] `DMA.h`, `ScatterRead.h`, `SigScan.h`, `Input Manager.h` have no game-layer includes.
- [ ] `main.cpp` defines `std::atomic<bool> bRunning{ true }`.
- [ ] `g_GameContext` is assigned in `main.cpp` before `DMA_Thread_Main` is launched.
- [ ] `g_GameContext` is not defined anywhere other than `DMA Thread.cpp`.
- [ ] A concrete `IGameContext` implementation is compiled and linked.
- [ ] `c_keys::InitKeyboard` is called only from `DMA_Thread_Main`, not from the game context.

---

## Porting to a New Game

| What                          | Where                            |
|-------------------------------|----------------------------------|
| Process and module names      | `GameModules.h`                  |
| Entity system constants       | `Const/EntityConfig.h`           |
| Offsets                       | `Offsets.h`                      |
| Entity types and reads        | `Classes/`                       |
| Entity list scan logic        | `EntityList.cpp`                 |
| Game state (view matrix, etc.)| `GameState.h/cpp`                |
| Timer registrations           | `GameContext::Initialize`        |

The base (`DMA/`, `main.cpp`, `pch.h`) requires **zero changes** between games.
