# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Open `Deadlock_DMA.sln` in Visual Studio 2022 and build with **Release | x64**. There is no command-line shorthand configured — use the IDE or MSBuild directly:

```
msbuild Deadlock_DMA.sln /p:Configuration=Release /p:Platform=x64
```

**Build configurations:**
- `Release | x64` — production build
- `Debug | x64` — debug build (no optimizations)

**Runtime DLLs required** (from `Dependencies/MemProcFS/`, copied automatically by post-build step):
`FTD3XX.dll`, `leechcore.dll`, `leechcore_driver.dll`, `vmm.dll`, `makcu-cpp.dll`

## Architecture

The codebase has three layers that map cleanly to directories:

### 1. DMA Layer (`DMA/`)
Low-level memory access via MemProcFS (VMMDLL). Subfolders:

**`Memory/ScatterRead.h`** — RAII wrapper around `VMMDLL_SCATTER_HANDLE`. Non-copyable; one instance per thread. Methods: `Add<T>(addr, T*)`, `AddRaw(addr, cb, void*)`, `Execute()`, `Clear()`. Include order in `pch.h` matters: `ScatterRead.h` must come before `Process.h`.

**`Memory/Process.h`** — provides `ReadMem<T>()` for single-value reads (creates a temporary `ScatterRead` internally). Use scatter batching for any per-frame work.

**`Memory/SigScan.h/cpp`** — `FindSignature()` scans a byte range for a pattern. `IsAddressReadable()` probes a single byte via `VMMDLL_MemReadEx` — used to validate resolved RIP-relative addresses. `ResolveOffset()` in `Offsets.cpp` combines scan + RIP resolve + fallback without exceptions (MemProcFS API uses return values, not exceptions).

**`Input/Input Manager.h/cpp`** — reads keyboard state from the target process via `gafAsyncKeyStateExport`. `c_keys::InitKeyboard()` resolves the export address; `c_keys::IsKeyDown()` reads bitfield state from target memory.

**`Logging/Log.h/cpp`** — thread-safe logger. Always use `Log::Info`, `Log::Warn`, `Log::Error`, `Log::Debug` instead of `std::println` directly. `DbgLog(...)` expands to `Log::Debug(...)` when `DBGPRINT` is defined, no-op in Release. `Log::Init(path)` opens the log file (called once from `main`). **Never use `std::println` or `DbgPrintln` — always use the Log class.**

**`DMA Thread.h/cpp`** — main acquisition loop on a dedicated thread. One persistent `ScatterRead sr` is created once and reused. The loop runs every 1 ms; individual update functions are gated by `CTimer` intervals. `Keybinds::OnDMAFrame` is called last and fires the aimbot if active.

**`DMA.cpp`** — `VMMDLL_Initialize` args are `{"-device", "FPGA", "-waitinitialize"}`. Do **not** add `-memmap auto` — it intermittently fails to bring up the FPGA and a normal MemProcFS test will work while the app fails. If a saved memmap file is needed for stability, use `-memmap path\to\mmap.txt` instead of `auto`.

### 2. Game Layer (`Deadlock/`)
Translates raw memory into game objects. All class and field names match the exact Valve/Source 2 SDK names from the schema dump at `C:\Users\admin\Downloads\schema-dump\deadlock\`.

**`Offsets.h`** — all struct field offsets. Static offsets are `constexpr`; dynamic offsets (`GameEntitySystem`, `LocalController`, `ViewMatrix`, `Prediction`) are resolved at runtime via `Offsets::ResolveOffsets()` in `Offsets.cpp`. `Offsets::Prediction` holds the RVA of the `CPrediction` global pointer; `Offsets::CPrediction::ServerTime` (0x68) is the field within it. **This is the first file to update when the game patches.**

**`Deadlock.h/cpp`** — global game state (view matrix, local player addresses, server time, client yaw). All fields are mutex-protected for cross-thread access.

**`Entity List/EntityList.h/cpp`** — scans the game's entity system and maintains six mutex-protected entity vectors: `m_PlayerControllers`, `m_PlayerPawns`, `m_Troopers`, `m_MonsterCamps`, `m_Sinners`, `m_XpOrbs`. `FullUpdate` rebuilds the entity list (called every tick). The per-type Refresh functions (`PawnRefresh`, `ControllerRefresh`, etc.) do incremental updates — adding new entities with full 2–3 stage init, quick-updating existing ones.

**FOW (minimap visibility)** — `m_FOWVisibleByAddr` (under `m_FOWMutex`) maps entity address → `m_bVisibleOnMap`, sourced from the local team's `C_CitadelTeam::m_vecFOWEntities` (offset `0x6C8`, wrapper layout `[0x00] int count, [0x08] T* data, [0x10] int max`; entries are `STeamFOWEntity` size `0x60` with `m_nEntIndex` at `0x30`, `m_bVisibleOnMap` at `0x41`). `DiscoverFOWTeam` (called from `FullUpdate` every 5 s) scans low entity indices for the populated team — only one of the 5 team entities has non-zero FOW data because the server only replicates the local team's view. `FullFOWRefresh` runs every 16 ms. `IsEntityVisible(addr)` is fail-open (returns true) when no FOW data is loaded yet, fail-closed otherwise. CS2 same idiom; `C_CitadelTeam` does not register a class string in the entity-class map, hence the discovery scan instead of `FindClass`.

**Entity class name strings** — the strings used in `SortEntityList` / `FindClass()` do not follow a consistent naming pattern. Confirmed live entity name strings (from Debug GUI class list export):
- `player` → `C_CitadelPlayerPawn` (player pawns)
- `citadel_player_controller` → `CCitadelPlayerController`
- `npc_trooper` → `C_NPC_Trooper` (lane minions)
- `npc_trooper_boss` → `C_NPC_TrooperBoss` (Walkers)
- `npc_trooper_neutral` → `C_NPC_TrooperNeutral` (jungle neutrals)
- `npc_boss_tier2` / `npc_boss_tier3` → major bosses (Patron etc.), tracked in `m_MonsterCamps`
- `npc_neutral_sinners_sacrifice` → Sinners
- `item_xp` → XP orbs
- `citadel_item_pickup_rejuv_herotest` → Rejuvenator pickups (note `_herotest` suffix is the real entity name)
- `destroyable_building` → shrines, guardians, destructible objectives
- Punchable buff boxes (`C_Citadel_PunchablePowerup`) do **not** appear in the entity class map

**Entity labels** — `C_BaseEntity` has a `const char* m_Label` field set at sort time. `m_TrooperAddresses` and `m_MonsterCampAddresses` are `vector<pair<uintptr_t, const char*>>`. Labels: regular troopers = `nullptr`, Walkers = `"Walker"`, neutrals = `"Neutral"`, tier-2 boss = `"Tier 2"`, tier-3 boss = `"Tier 3"`. `C_NPC_Trooper::PrepareRead_1` uses `m_Label != nullptr` to gate the `m_iMaxHealth` read (Walkers only).

**`Classes/`** — C++ wrappers for game entities. Each class uses a multi-stage scatter read pattern:
- `PrepareRead_1` — entity-level fields + controller handle
- `PrepareRead_2` — scene node address → position + dormant flag
- `PrepareRead_3` — bone array + model path string (pawns only)
- `QuickRead` — position + health only (used in the fast loop)

All `PrepareRead_*` methods only *enqueue* reads into the caller's `ScatterRead&`; the caller calls `sr.Execute()` after each stage and `sr.Clear()` before the next.

**`Const/`** — hero enums, team enum, and generated bone/hitbox data. `BoneLists.hpp` and `BoneListTypes.hpp` are auto-generated by `BoneExtractor/` (Python 3 script that reads Deadlock VPK files). Run the extractor after hero model changes.

### 3. GUI Layer (`GUI/`)
ImGui + DirectX 11 overlay. The main GUI thread calls `MainWindow::OnFrame()` each frame, which calls into `Fuser::Render()`. `Query::IsUsing*()` functions gate which entity types are rendered (and also determine which entity types the DMA thread bothers refreshing). `Deadlock::WorldToScreen()` handles the view matrix projection.

**Aimbot** — `Aimbot::OnFrame` is a single-frame function (no loop, no DMA reads). It reads only from mutex-protected cached entity data. `Keybinds::OnDMAFrame` manages `Aimbot::bIsActive` and calls `OnFrame` when the key is held. Toggles: `bAimAtOrbs` (XP orbs in the hideout test mode — gate is just `!IsInvalid() && !IsDormant()`; `m_flAttackableTime` looked promising but is a *next-cycle predictor*, not a current-state flag, so it's not used) and `bVisibleOnly` (FOW gate via `EntityList::IsEntityVisible(pawn.m_EntityAddress)`). The FOV slider (`fMaxPixelDistance`) drives both the targeting cutoff and the on-screen FOV circle.

**Watchdog** (`GUI/Watchdog/GuiWatchdog.h/.cpp`) — debug-only stall detector. GUI thread sets `GuiStage(name)` breadcrumbs and calls `Tick()` once per frame; DMA thread sets `DmaStage(name)`. A separate watchdog thread polls the frame counter every 500 ms and if it hasn't advanced for 2 s it dumps `[Watchdog] GUI STALL: ...` plus a `try_lock` probe of every shared mutex (`PawnMutex`, `ControllerMutex`, `TrooperMutex`, `MonsterCampMutex`, `SinnerMutex`, `XpOrbMutex`, `ClassMapMutex`, `ServerTimeMutex`) so we know which one is held. Heartbeat at 5 s. Started from `main`.

## Key Patterns

**Scatter reads** — never read individual fields one at a time. Batch all reads into a `ScatterRead`, call `Execute()` once, then extract results. Use `Clear()` between stages in the same batch. The `ScatterRead` instance is owned by the calling thread; never share it across threads.

**MemProcFS error handling** — the API signals failure via return values (`false`, `0`, `nullptr`), not C++ exceptions. Never use try/catch to handle MemProcFS failures; use `if` checks instead.

**Offset naming** — offsets are always relative to the base pointer of the named class, not to any parent. `CGameSceneNode::m_modelState` is actually in `CSkeletonInstance` (which the game stores at the same pointer), so the offset is relative to the scene node pointer.

**Bone stride** — bone data in `CModelState` is packed at stride `0x20` (32 bytes per entry). `C_CitadelPlayerPawn::ExtractBones()` copies the `Vector3` from the start of each stride slot.

**Hero bone data** — after reading the model path string, call `CacheBoneData()` to resolve the hero and set `m_BoneCount` to the minimum needed bones rather than the `MAX_BONES=70` fallback.

**SDK reference** — upstream struct definitions live at `github.com/neverlosecc/source2sdk` (branch `deadlock`). When the game updates, compare changed HPP files there against `Offsets.h`. A local schema dump also lives at `C:\Users\admin\Downloads\schema-dump\deadlock\sdk\` — auto-generated HPP files with confirmed field offsets.

**Entity list addresses** — `GetEntityListAddresses` does a single bulk scatter read of `MAX_ENTITY_LISTS * 8` bytes directly into `m_EntityList_Addresses.data()` (contiguous `std::array<uintptr_t>`), not 32 individual reads.

**`C_CitadelPlayerPawn` fields read** — `PrepareRead_1` reads: `m_hController` (0x10A8), `m_nUnsecuredSouls` (0x12E4), `m_nTotalUnspentSouls` (0x12D8), `m_vecVelocity` (0x438), `m_angEyeAngles` (0x11B0), `m_flRespawnTime` (0x130C).

**GUI** — `Fuser::Render()` is a black, fullscreen, no-decoration ImGui window that acts as the ESP overlay. It must always cover the entire screen. Never add decoration, input handling, or non-zero window padding to it. The main menu is a separate OS window (via `ImGuiConfigFlags_ViewportsEnable`) toggled with Insert.

**Player health** — player health/max health come from `PlayerDataGlobal_t` (inline struct at `controller + 0x8F0`), not from `C_BaseEntity::m_iHealth`. The pawn's engine-level health field is unreliable for players. NPC health (troopers, bosses) uses `C_BaseEntity::m_iHealth` (0x354) and `m_iMaxHealth` (0x350) directly.

**DMA thread timer intervals** — `ViewMatrix`: 2ms, `Yaw`: 10ms, `ServerTime`: 1s, `LocalControllerAddress`: 15s, `FullTrooper`: 3s, `QuickTrooper`: 16ms, `FullPawn`: 2s, `QuickPawn`: 8ms, `FullMonsterCamp`: 2s, `QuickMonsterCamp`: 100ms, `FullController`: 2s, `QuickController`: 150ms, `FullSinner`: 1s, `FullXpOrb`: 500ms, `QuickXpOrb`: 16ms, `FullFOWRefresh`: 16ms, `FullUpdate`: 5s, `Keybinds`: 5ms. Verbose per-refresh log lines use `DbgLog` (no-op in Release) to avoid I/O pressure.

**Lock-order audit** — entity mutexes that are ever held simultaneously must be acquired via a single `std::scoped_lock(a, b)` (uses `std::lock`'s deadlock-avoidance algorithm). Every site that needs both Pawn and Controller does it this way: `Aimbot::GetAimDelta`, `Players::operator()`, `StatusBars`, `Player List`, `EntityList::UpdateEntityMap`, `EntityList::FullControllerRefresh_lk`, `Radar::DrawEntities`. Do **not** lock them sequentially — `FullControllerRefresh_lk` (Controller→Pawn) and `Radar::DrawEntities` (Pawn→Controller) used to do that and produced an AB/BA deadlock against each other across the GUI/DMA threads. `m_FOWMutex` is taken alone — never nested with Pawn/Controller. `m_ServerTimeMutex` is taken alone or briefly inside the Aimbot orb pass.

**Aimbot orb scan** — splits Pawn+Controller and XpOrb into two separate scopes inside `GetAimDelta` so we never hold three entity mutexes at once. Releases Pawn+Controller before grabbing XpOrb to keep the GUI's ESP path from blocking on stale Aimbot work.

**Console hygiene** — most per-cycle entity-list logs (`UpdateCrucialInformation`, `UpdateEntityMap`, `UpdateEntityClassMap`, `SortEntityList`, `FullUpdate done`, `Trooper List Refreshed`, `Entity Map Updated`, `Entity Class Map Updated`) are silent in Release; the per-name class-map dump was removed (the game churns its class registry mid-read and was leaking garbled strings into the log). The `[EntityList] N pawns, M troopers, ...` summary fires only when any count changes; `Local Player Controller/Pawn` fires only on change; `[FOW] Team entity` fires only on change. When adding new logs in the DMA hot path, prefer `DbgLog` unless it's an event/transition.
