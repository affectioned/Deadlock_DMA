# Deadlock DMA

An open-source DMA cheat client for Valve's **Deadlock**, written in **C++23**.

> **This is an actively maintained fork.** The [original repository](https://github.com/CyN1ckal/Deadlock_DMA) by CyN1ckal has been archived. All ongoing development and bug fixes happen here.

<img width="1920" height="1079" alt="Showcase" src="https://raw.githubusercontent.com/CyN1ckal/Deadlock_DMA/refs/heads/master/Deadlock%201.1.png" />

## Educational Purpose

This project is intended **strictly for educational and research purposes** — to study Direct Memory Access (DMA) techniques, Source 2 engine internals, and game client architecture. It demonstrates:

- How DMA hardware can read process memory without injecting into a target process
- How Source 2 entity systems, scatter reads, and networked field offsets are structured
- How ImGui overlays and input emulation devices interface with game data

**Do not use this software in online multiplayer games.** Using cheats in live games violates the terms of service of the game and the platform, harms other players, and may result in permanent bans or legal consequences. The authors take no responsibility for misuse.

## Features

- **ESP** — players (skeleton, boxes, head circle, health bars, unsecured souls, velocity vector), troopers, monster camps, Sinner's Sacrifice
- **Aimbot** — smooth mouse movement via Makcu, Gaussian noise, velocity prediction, configurable FOV
- **Radar** — minimap overlay with team-coloured player icons, MOBA-style health bars, local player view ray
- **HUD** — team health/souls status bars, souls-per-minute display
- **Config system** — JSON save/load profiles

## Changes from upstream

- Updated offsets and entity layout for the latest game patch
- Replaced raw VMMDLL scatter calls with a type-safe `ScatterRead` RAII wrapper
- Replaced heuristic address validation with `IsAddressReadable()` probe
- Removed dead try/catch blocks around MemProcFS calls (API uses return values, not exceptions)
- Bone/hitbox data regenerated from current VPK via `BoneExtractor`; aimbot now targets by `HitboxSlot` instead of a static index map
- Parallel compilation (`/MP`) enabled; `imgui_demo.cpp` removed from build
- Removed all build configurations except `Release|x64` and `Debug|x64`
- Removed Tracy profiler and Catch2 test infrastructure

## Requirements

- [MemProcFS FPGA](https://github.com/ufrisk/MemProcFS) — DMA hardware driver
- [Makcu](https://github.com/K4HVH/makcu-cpp) — USB HID mouse controller for aimbot
- Visual Studio 2022 with C++23 support

## Build

1. Open `Deadlock_DMA.sln` in **Visual Studio 2022**
2. Select **Release | x64**
3. Build (**Ctrl+Shift+B**)

The post-build step automatically copies the required DLLs next to the executable.

## Runtime DLLs

Place the following files from [MemProcFS](https://github.com/ufrisk/MemProcFS) and [Makcu C++](https://github.com/K4HVH/makcu-cpp) next to the executable (handled automatically by the post-build step if the `Dependencies/` folder is populated):

| File | Source |
|------|--------|
| `FTD3XX.dll` | MemProcFS |
| `leechcore.dll` | MemProcFS |
| `leechcore_driver.dll` | MemProcFS |
| `vmm.dll` | MemProcFS |
| `makcu-cpp.dll` | Makcu C++ |

## Usage

- Press **Insert** to toggle the menu
- The Fuser window (ESP overlay) is a black fullscreen window — position it over your game monitor in the taskbar
- Configure monitor and resolution under the **Fuser** tab before first use
- Aimbot requires a connected Makcu device; status is shown in the **Aimbot** tab

## Credits

Original project by [CyN1ckal](https://github.com/CyN1ckal/Deadlock_DMA).
