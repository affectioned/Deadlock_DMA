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

## Credits

Original project by [CyN1ckal](https://github.com/CyN1ckal/Deadlock_DMA).
