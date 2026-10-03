
# BoneExtractor
Extracts hero bone/hitbox data and hero ids from Deadlock's game files and generates C++ headers used by the DMA overlay.

## Requirements
**Python 3.12 or newer.** `keyvalues3 >= 0.4` is needed to parse KV3 v4/v5, which is what Deadlock's current hero models ship as; those wheels require Python 3.12+. On older Pythons, pip silently installs `keyvalues3 0.3`, which fails on every hero model with `Invalid binary KV3 magic`.

Create the venv with an explicit interpreter version:
```
py -3.13 -m venv venv        (or -3.12)
venv\Scripts\activate
pip install vpk keyvalues3
```

Check `py -0` to see which Python versions are installed; grab a newer one from python.org or `winget install Python.Python.3.13` if needed.

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

### Hero ids
Ids come from `scripts/heroes.vdata_c` inside the VPK. Display names come from `citadel_gc_hero_names_english.txt`, which ships **loose** under `game/citadel/resource/localization/` rather than packed, so the script reads it from disk next to the VPK. Without it, names fall back to codenames.

Heroes flagged `m_bDisabled` cannot spawn, but their ids are emitted anyway — behind a comment — so an unexpected `m_HeroID` still resolves to a name instead of falling through the lookup.

Identifiers are derived from the display name (`"Grey Talon"` to `GreyTalon`). Four heroes need a manual entry in `NAME_OVERRIDES` where the game data does not match what the overlay should show, e.g. localization calls Doorman "The Doorman" and has no entry at all for `hero_testhero`.

## Credits
- [Deadlock Bone Extractor](https://www.unknowncheats.me/forum/deadlock/744334-deadlock-bone-extractor.html) (UnknownCheats) — original concept and reference for extracting bone/hitbox data from Deadlock VPKs