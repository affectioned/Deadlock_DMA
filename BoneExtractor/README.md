
# BoneExtractor
Extracts hero bone/hitbox data and hero ids from Deadlock's game files and generates C++ headers used by the DMA overlay.

Every VPK read and Source 2 resource decompile goes through **Source2Viewer-CLI** (part of [ValveResourceFormat](https://github.com/ValveResourceFormat/ValveResourceFormat)). It is well maintained and keeps up with Valve's KV3 format churn, so the extractor stays correct across game patches without us tracking new `KV3BinaryType` ids in Python. Python only drives the CLI and parses its text KV3 output.

## Requirements

### 1. Python 3.12 or newer
`keyvalues3 >= 0.4` is required to parse KV3 v4/v5 (what Deadlock currently emits); the 0.4 wheels need Python 3.12+. On older Pythons pip silently installs `keyvalues3 0.3`, which can't parse current hero data.

Create the venv with an explicit interpreter version:
```
py -3.13 -m venv venv        (or -3.12)
venv\Scripts\activate
pip install -r requirements.txt
```

Check `py -0` to see which Python versions are installed; grab a newer one from python.org or `winget install Python.Python.3.13` if needed.

### 2. Source2Viewer-CLI (version 21.0 or newer)

1. Go to https://github.com/ValveResourceFormat/ValveResourceFormat/releases
2. Download **`cli-windows-x64.zip`** (self-contained, no .NET install required)
3. Extract the zip contents into `BoneExtractor/vrf/`. The folder must contain `Source2Viewer-CLI.exe`.

Alternatively, point the `VRF_CLI` environment variable at `Source2Viewer-CLI.exe` wherever you keep it.

The `vrf/` folder is gitignored.

## Usage
Run from inside the `BoneExtractor` directory with the venv active:
```
python BoneExtractor.py
```
The script auto-detects your Deadlock VPK path via Steam. No arguments needed.

Results are cached in `vpk.cache`, keyed by a hash of every input the headers are built from. Delete it to force regeneration.

## Output
All four files are written to `Deadlock_DMA/Deadlock/Const/` automatically and are regenerated wholesale — edit the script, not them:

| File | Contents |
|------|----------|
| `BoneListTypes.hpp` | `HitboxSlot` enum, `BonePair` struct, `ModelBoneData` struct |
| `BoneLists.hpp` | `g_HeroModelData` unordered_map keyed by hero model path |
| `HeroEnum.hpp` | `HeroId` enum, one entry per `m_HeroID` in `scripts/heroes.vdata_c` |
| `HeroMap.hpp` | `HeroNames::HeroNameMap`, `HeroId` to display name |

### How it works

1. `Source2Viewer-CLI --vpk_list -e vmdl_c -f models/heroes/,models/heroes_staging/,models/heroes_wip/` enumerates hero models from the VPK.
2. For each batch of models, `Source2Viewer-CLI -b DATA,MDAT` streams the resource blocks as text KV3. Batches are sized to stay under the Windows command-line length limit; the per-file `[n/m] <path>` markers in stdout split the output back out per model.
3. Each block is parsed with `keyvalues3.textreader.KV3TextReader`. DATA provides `m_modelSkeleton` (bones + parents); MDAT provides `m_hitboxsets.default` (per-bone hitbox group id + shape radius).
4. `scripts/heroes.vdata_c` is dumped the same way (`-b DATA`) to resolve hero ids.
5. The loose `citadel_gc_hero_names_english.txt` sits beside the VPK, not inside it, so it's read directly from disk.

### Hero ids
Ids come from `scripts/heroes.vdata_c` inside the VPK. Display names come from `citadel_gc_hero_names_english.txt`, which ships **loose** under `game/citadel/resource/localization/` rather than packed, so the script reads it from disk next to the VPK. Without it, names fall back to codenames.

Heroes flagged `m_bDisabled` cannot spawn, but their ids are emitted anyway — behind a comment — so an unexpected `m_HeroID` still resolves to a name instead of falling through the lookup.

Identifiers are derived from the display name (`"Grey Talon"` to `GreyTalon`). Four heroes need a manual entry in `NAME_OVERRIDES` where the game data does not match what the overlay should show, e.g. localization calls Doorman "The Doorman" and has no entry at all for `hero_testhero`.

## Credits
- [Deadlock Bone Extractor](https://www.unknowncheats.me/forum/deadlock/744334-deadlock-bone-extractor.html) (UnknownCheats) — original concept and reference for extracting bone/hitbox data from Deadlock VPKs
