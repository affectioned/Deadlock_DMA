# Deadlock DMA

An open-source DMA client for Valve's **Deadlock**, written in **C++23**.

> **This is an actively maintained fork.** The [original repository](https://github.com/CyN1ckal/Deadlock_DMA) by CyN1ckal has been archived. All ongoing development and bug fixes happen here.

## Educational Purpose

This project is intended **strictly for educational and research purposes** — to study Direct Memory Access (DMA) techniques, Source 2 engine internals, and game client architecture. It demonstrates:

- How DMA hardware can read process memory without injecting into a target process
- How Source 2 entity systems, scatter reads, and networked field offsets are structured
- How ImGui overlays and input emulation devices interface with game data

**Do not use this software in online multiplayer games.** Using this tool in live games violates the terms of service of the game and the platform, harms other players, and may result in permanent bans or legal consequences. The authors take no responsibility for misuse.

## Requirements

- [MemProcFS FPGA](https://github.com/ufrisk/MemProcFS) — DMA hardware driver
- [Makcu](https://github.com/K4HVH/makcu-cpp) — USB HID mouse controller for aim assist
- Visual Studio 2022 or newer, with C++23 support

## Build

1. Open `Deadlock_DMA.sln` in **Visual Studio**
2. Select **Release | x64**
3. Build (**Ctrl+Shift+B**)

## Runtime DLLs

On first launch the built-in bootstrapper downloads the latest MemProcFS Windows release from GitHub and drops the required DLLs next to the executable. Delete any of them to force a re-download on the next run.

| File | Source |
|------|--------|
| `vmm.dll` | MemProcFS |
| `leechcore.dll` | MemProcFS |
| `leechcore_driver.dll` | MemProcFS |
| `FTD3XX.dll` | MemProcFS |
| `FTD3XXWU.dll` | MemProcFS |

Makcu C++ is linked statically, so no `makcu-cpp.dll` is needed at runtime.

## Keeping up with game updates

A Deadlock patch can move schema field offsets, the client.dll globals, and the
hero skeletons. Two things need refreshing, and neither failure mode is loud —
nothing crashes, the overlay just reads the wrong memory.

**1. Offsets.** `Deadlock/Offsets.cpp` resolves the four client.dll globals by
signature scan at runtime, each with a hardcoded fallback RVA, so a stale
fallback only bites if the pattern breaks too. Field offsets in
`Deadlock/Offsets.h` are plain constants and always need checking.

Both track a [dezlock-dump](https://github.com/dougwithseismic/dezlock-dump)
schema dump of the current build:

| Dump file | What to check against it |
|-----------|--------------------------|
| `client.txt`, `server.txt` | Field offsets — the `FLATTENED` section gives full layouts including inherited fields |
| `sdk/_patterns.hpp` | Signatures for the global pointers |
| `sdk/_globals.hpp` | Fallback RVAs for those globals |
| `_globals.txt` | Globals found by `.data` RTTI scan, for pointers with no pattern |

Watch for fields that stop being a fixed offset rather than simply moving —
e.g. an inline struct becoming a `CUtlOrderedMap`, whose internals the dump
does not describe.

**2. Hero bone tables.** `Deadlock/Const/BoneLists.hpp` is generated from the
game's VPKs by [BoneExtractor](BoneExtractor/README.md) — run it after a patch.
Bone indices shift often, frequently as a whole-skeleton renumber from one bone
inserted near the root, which silently aims every hitbox lookup one joint off.

It skips the work when `pak01_dir.vpk`'s hash is unchanged since the last run —
delete `vpk.cache` to force regeneration.

BoneExtractor locates the VPK through Steam's registry keys and takes no path
argument, so it expects to run on the machine with the game installed. The game
may be open at the time; it only reads the pack files. To keep that box
untouched, copy `game\citadel\pak01_dir.vpk` plus its `pak01_0xx.vpk` chunks to
the other machine, either mirroring the Steam directory layout or editing
`VPK_RELATIVE` / `FALLBACK_VPK` at the bottom of the script.

## Credits

Original project by [CyN1ckal](https://github.com/CyN1ckal/Deadlock_DMA).
