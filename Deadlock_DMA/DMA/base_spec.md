# DMA Client Base — Specification

This document specifies the compile-ready base layer for a DMA client. The base is intentionally game-agnostic. Game-specific logic (offsets, object types, game state) plugs in on top through defined interfaces. Every section describes the exact API contract that must be satisfied for the project to compile.

---

## Directory Layout

```
DMA/
  DMA.h / DMA.cpp                      — VMM connection singleton
  IGameContext.h                       — IGameContext interface + g_GameContext extern
  DMA Thread.h / DMA Thread.cpp        — CTimer, Timer alias, acquisition loop entry point

  Logging/
    Log.h / Log.cpp                    — thread-safe logger (console + file)

  Memory/
    ScatterRead.h                      — RAII scatter read wrapper (header-only)
    Process.h / Process.cpp            — process + module discovery, single-value reads
    SigScan.h / SigScan.cpp            — byte-pattern scan, address probe

  Input/
    Input Manager.h / Input Manager.cpp — kernel keystroke reader (gafAsyncKeyState)

Game/                                — game layer (not part of the base, rename per project)
  GameModules.h                      — process name + module list
  GameContext.h / .cpp               — concrete IGameContext, owns timer threads
  Const/Config.h                     — game-specific constants (object pool sizes, loop bounds)

main.cpp                             — entry point, thread launch, main loop
pch.h / pch.cpp                      — precompiled header
```

The `Game/` directory name is a placeholder. Rename it to match your target (e.g. `Deadlock/`, `Fortnite/`, `Valorant/`). The base (`DMA/`, `main.cpp`, `pch.h`) requires **zero changes** between games.

---

## pch.h

### Purpose
Single-include precompiled header. Everything every translation unit needs lives here.

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

#include "vmmdll.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// DMA layer — include order is mandatory: Log → DMA → Memory/ScatterRead → Memory/Process.
#include "DMA/Logging/Log.h"
#include "DMA/DMA.h"
#include "DMA/Memory/ScatterRead.h"
#include "DMA/Memory/Process.h"
```

### Notes
- Do **not** include any game-layer headers here. The base must compile without a game layer present.
- Game-specific constants (entity limits, pool sizes, etc.) belong in the game layer, not here.
- Every `.cpp` file in the project must `#include "pch.h"` as its **absolute first line**. This is an MSVC precompiled header requirement — any include before `pch.h` breaks the PCH and produces cryptic compile errors.
- `DbgLog(...)` (defined in `DMA/Log.h`) is the debug-only print macro. It is a no-op in Release builds.

---

## Log.h / Log.cpp

### Purpose
Thread-safe logger. Writes every message to both the console (`stdout`) and a log file next to the exe. All other code calls `Log::Info`, `Log::Warn`, `Log::Error`, or `Log::Debug` instead of `std::println` directly.

### Required project settings
- **C++ Language Standard:** `/std:c++latest` (or `/std:c++23`). Required for `std::format_string`, `std::println`, and `std::format` with the argument-forwarding signatures used in `Log.h`.
- **Preprocessor:** define `DBGPRINT` in Debug configurations to enable `DbgLog(...)`. In Release it is a no-op with zero code generated.

### API contract
```cpp
class Log
{
public:
    // Call once at startup. Opens (or creates) the log file at logPath.
    // Must be called before any Info/Warn/Error/Debug calls.
    static void Init(const std::wstring& logPath);

    // Formatted log helpers — same syntax as std::format / std::println.
    template<typename... Args>
    static void Info (std::format_string<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Warn (std::format_string<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Debug(std::format_string<Args...> fmt, Args&&... args);

private:
    static void Write(std::string_view prefix, std::string msg);
};

// DbgLog(...) — expands to Log::Debug(...) when DBGPRINT is defined; no-op otherwise.
#ifdef DBGPRINT
#define DbgLog(...) Log::Debug(__VA_ARGS__)
#else
#define DbgLog(...)
#endif
```

### Output format
Each line is prefixed with a fixed-width level tag:
```
[INFO] message
[WARN] message
[ERR ] message
[DBG ] message    (DBGPRINT builds only)
```

### Implementation rules
- A single `static std::mutex` serialises all writes — safe to call from both the DMA thread and the GUI thread.
- `Init` opens the file with `std::ios::trunc` so each run starts a fresh log.
- `Write` calls `std::println` for the console and `ofstream <<` + `flush()` for the file. Both happen inside the lock.
- `<fstream>` is only included in `Log.cpp`, not in `Log.h`.
- `Log.h` includes `<format>` and `<string>` for `std::format_string` and `std::string`. All other STL headers come from `pch.h`.

### Notes
- **Always use the `Log` class for all output.** Never call `std::println`, `printf`, `std::cout`, or any other print function directly anywhere in the codebase. Every message — startup, status, warning, error, debug — must go through `Log::Info`, `Log::Warn`, `Log::Error`, `Log::Debug`, or `DbgLog`. This keeps output consistent, thread-safe, and always written to the log file.
- Do **not** redirect `stdout` with `_wfreopen`. `Log::Init` opens its own file handle independently.
- Messages emitted before `Log::Init` returns appear on the console only — acceptable for the very first startup line.

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

    // Called every tick (nominally 1 ms). Tick your CTimers here.
    virtual void Tick(DMA_Connection* conn,
                      std::chrono::steady_clock::time_point now) = 0;

    virtual ~IGameContext() = default;
};

// Defined in DMA Thread.cpp. Set by main() before launching DMA_Thread_Main.
extern IGameContext* g_GameContext;
```

### Notes
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

    // Queue a raw byte-range read (arrays, strings, bulk structs).
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
- `PopulateModules` polls until every requested module resolves; some modules load after the process starts.
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

Use `CTimer<T, F>` directly when the type can be deduced (e.g. a single named timer). Use `Timer` when you need to store multiple timers in a `std::vector<Timer>`, as the concrete `GameContext` does.

`DMA Thread.h` must include `IGameContext.h` at the top so that any file including `DMA Thread.h` (e.g. `GameContext.h`) also gets the `Timer` alias and the `g_GameContext` extern in one include.

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
Entry point. Opens the log, initialises subsystems in order, sets `g_GameContext`, launches the DMA thread, and runs the GUI main loop.

### Required implementation
```cpp
#include "pch.h"
#include <filesystem>

#include "GUI/Main Window/Main Window.h"
#include "DMA/DMA Thread.h"
#include "GUI/Config/Config.h"
#include "Game/GameContext.h"   // swap for your concrete IGameContext

std::atomic<bool> bRunning{ true };

int main()
{
    {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        auto logPath = std::filesystem::path(exePath).parent_path() / "client.log";
        Log::Init(logPath.wstring());
    }

    Log::Info("Starting up...");

    Config::LoadConfig("default");
    MainWindow::Initialize();

    g_GameContext = new GameContext();

    std::thread DMAThread(DMA_Thread_Main);

    Log::Info("Press END to exit...");
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
- `std::atomic<bool> bRunning{ true }` — **defined in `main.cpp`**, referenced as `extern` in `DMA Thread.cpp`. Controls both the acquisition loop and the main window loop. Set to `false` to trigger shutdown from either thread.

### Startup order (mandatory)
1. `Log::Init` — must be first. Opens the log file before any other subsystem emits messages.
2. `Config::LoadConfig` — config values must be ready before window/renderer init.
3. `MainWindow::Initialize` — creates the D3D11 device and ImGui context.
4. Any additional peripheral init (mouse emulation, etc.).
5. `g_GameContext = new ...` — must be set before the thread starts.
6. `std::thread(DMA_Thread_Main)` — DMA thread starts here.
7. Main loop — `MainWindow::OnFrame()` each iteration, exit key check.
8. `join()` — waits for `EndConnection()` to complete.

---

## Game Layer — What to Implement

These files are **not** part of the base. They are required for the project to link. Organise them under your game directory (e.g. `Game/`, `Deadlock/`, `Fortnite/`).

### GameModules.h
Defines the process name and the list of modules passed to `Process::GetProcessInfo`. All name strings live here; `Process` has no hardcoded values.

```cpp
namespace GameModules
{
    inline constexpr const char* ProcessName = "game.exe";

    // Add one entry per module whose base address you need.
    inline std::vector<std::string> ModuleList()
    {
        return { ProcessName };
    }
}
```

Common patterns by engine:
- **Source 2** — `"game.exe"` + `"client.dll"` (main game logic lives in client.dll)
- **Unreal Engine** — typically just `"game.exe"` (all game code is in the exe or a single shipping DLL)
- **Unity / custom** — varies; add whatever modules your offset resolution needs

### Const/Config.h
Game-specific constants used as loop bounds and array sizes throughout the game layer. Every game needs at least the maximum number of objects the engine can hold in a single iteration pass — without this the object-list reading loop has no upper bound.

```cpp
// Game/Const/Config.h
// Values are engine-specific. Check the SDK or reverse the entity system to find them.

inline constexpr size_t MAX_ACTORS      = 2048;  // upper bound for a single object-list pass
inline constexpr size_t MAX_ACTOR_LISTS = 64;    // number of sub-lists (Source 2 only)
```

Common values by engine:

| Engine        | Constant           | Typical value |
|---------------|--------------------|---------------|
| Source 2      | `MAX_ENTITIES`     | 512 – 8192    |
| Source 2      | `MAX_ENTITY_LISTS` | 64            |
| Unreal Engine | `MAX_UOBJECTS`     | 65536+        |
| Unity IL2CPP  | `MAX_OBJECTS`      | varies        |

Do **not** put these constants in `pch.h`. They belong in the game layer only.

### GameContext : IGameContext
Concrete context. `Initialize` calls game-state init and registers all `CTimer`s. `Tick` fires each timer every millisecond.

```cpp
class GameContext : public IGameContext
{
public:
    bool Initialize(DMA_Connection* conn) override;
    void Tick(DMA_Connection* conn, std::chrono::steady_clock::time_point now) override
    {
        for (auto& t : m_Timers) t.Tick(now);
    }
private:
    std::vector<Timer> m_Timers;   // Timer = CTimer<milliseconds, function<void()>>
};
```

`Initialize` populates `m_Timers` with one entry per game function:

```cpp
bool GameContext::Initialize(DMA_Connection* conn)
{
    if (!GameState::Initialize(conn))
        return false;

    using ms = std::chrono::milliseconds;

    m_Timers =
    {
        { ms(/* TODO */), [conn] { GameState::UpdateViewMatrix(conn); } },
        { ms(/* TODO */), [conn] { GameState::UpdatePlayers(conn); } },
        { ms(/* TODO */), [conn] { GameState::FullEntityRebuild(conn); } },
        // ... one entry per function
    };

    return true;
}
```

#### Timer interval TODO list

Every function registered as a timer needs a tuned interval. Fill these in for your game:

- [ ] **View / camera matrix** — how often the projection matrix must be re-read. Typical: `2`–`8` ms.
- [ ] **Local player address** — how often to re-resolve the local player pointer. Can be slow; re-read only on respawn or match start. Typical: `5000`–`15000` ms.
- [ ] **Full actor/entity list rebuild** — scans the engine object list from scratch and re-populates all entity vectors. Expensive; run infrequently. Typical: `1000`–`5000` ms.
- [ ] **Quick actor update** (positions, health, state) — reads only already-known object addresses; no list scan. Must be fast enough for smooth ESP. Typical: `8`–`16` ms.
- [ ] **Full NPC / minion refresh** — if the game has NPC types tracked separately from players. Typical: `1000`–`3000` ms.
- [ ] **Quick NPC update** — position-only pass on known NPC addresses. Typical: `16`–`100` ms.
- [ ] **Server time / game clock** — how often to sync the server-side game clock. Typical: `500`–`1000` ms.
- [ ] **Aimbot / keybind poll** — how often to check key state and fire aimbot logic. Must be tight. Typical: `5`–`16` ms.
- [ ] **Any other game-specific reads** — e.g. item pickups, world state, score. Set intervals based on how quickly the data changes and how stale it can be before it affects gameplay.

Guideline: if a slow timer (> 500 ms) consistently blocks a fast one (< 16 ms), split them into separate `GameContext` implementations chained from the same `IGameContext`, or simply profile and reduce the slow function's work per call.

### Offset resolution
Resolve dynamic offsets (pointers, globals) at startup using `FindSignature` + a RIP-relative read via `ReadFromPID`. Fall back to a hardcoded RVA when the signature fails. Log both outcomes with `Log::Info` / `Log::Warn` so offset regressions are visible at a glance.

### Object / entity reading
The multi-stage scatter-read pattern applies regardless of engine:
1. Enqueue all fields for a batch of objects into a shared `ScatterRead`.
2. Call `Execute()` once per stage.
3. Call `Clear()` before enqueuing the next stage.

Never read fields one object at a time on the hot path. Engine-specific differences:

| Engine      | Object list location                        | Notes |
|-------------|---------------------------------------------|-------|
| Source 2    | Entity system at a scanned global pointer   | 32–64 entity sublists, each holding `CEntityIdentity` entries |
| Unreal Engine | `GObjects` (TUObjectArray) / `GWorld`    | Iterate `ObjObjects.Objects` array; filter by class via `ClassPrivate` pointer |
| Unity IL2CPP | Static field table via metadata           | Walk `Il2CppClass` → static field pointer |

---

## Compile Checklist

- [ ] Project C++ language standard is set to `/std:c++latest` (C++23). Required for `std::format_string` and `std::println`.
- [ ] Debug configuration defines the `DBGPRINT` preprocessor symbol to enable `DbgLog(...)`.
- [ ] Every `.cpp` file has `#include "pch.h"` as its absolute first line.
- [ ] `pch.h` has no game-layer includes and no game-specific constants.
- [ ] `pch.h` includes `DMA/Log.h` before `DMA/DMA.h`.
- [ ] No file calls `std::println` directly — all output goes through `Log::Info`, `Log::Warn`, `Log::Error`, `Log::Debug`, or `DbgLog`.
- [ ] `DMA/Log.cpp` is added to the project and compiles cleanly.
- [ ] `Log::Init` is the first call in `main()` before any other subsystem.
- [ ] `DMA/IGameContext.h` forward-declares `DMA_Connection` instead of including `DMA.h`.
- [ ] `DMA/DMA Thread.h` includes `IGameContext.h` at the top.
- [ ] `DMA/DMA Thread.cpp` defines `IGameContext* g_GameContext = nullptr` and `extern std::atomic<bool> bRunning`.
- [ ] `DMA/DMA Thread.cpp` has `#pragma comment(lib, "Winmm.lib")`.
- [ ] `DMA/DMA Thread.cpp` has no game-layer includes.
- [ ] `DMA/Process.h` has no hardcoded module names.
- [ ] `DMA.h`, `ScatterRead.h`, `SigScan.h`, `Input Manager.h` have no game-layer includes.
- [ ] `main.cpp` defines `std::atomic<bool> bRunning{ true }`.
- [ ] `g_GameContext` is assigned in `main.cpp` before `DMA_Thread_Main` is launched.
- [ ] `g_GameContext` is not defined anywhere other than `DMA Thread.cpp`.
- [ ] A concrete `IGameContext` implementation is compiled and linked.
- [ ] Every function registered as a `Timer` in `GameContext::Initialize` has a tuned interval (no `/* TODO */` placeholders left).
- [ ] `Game/Const/Config.h` defines at least one object-count constant used as a loop bound in the object-list reader.
- [ ] `Const/Config.h` is not included in `pch.h`.
- [ ] `c_keys::InitKeyboard` is called only from `DMA_Thread_Main`, not from the game context.

---

## Porting to a New Game

| What                            | Where                                                    |
|---------------------------------|----------------------------------------------------------|
| Process and module names        | `GameModules.h`                                          |
| Object pool / array size bounds | `Const/Config.h`                                         |
| Dynamic offset resolution       | game layer init (called from `GameContext::Initialize`)  |
| Object / entity reading         | `Classes/` or equivalent                                 |
| Object list scan logic          | game layer (called by a timer)                           |
| Game state (view matrix, etc.)  | `GameState.h/.cpp` or equivalent                         |
| Timer intervals per function    | `GameContext::Initialize` (see interval TODO list)       |

The base (`DMA/`, `main.cpp`, `pch.h`) requires **zero changes** between games.
