
# BoneExtractor
Extracts hero bone/hitbox data from Deadlock's VPK files and generates C++ headers used by the DMA overlay.

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

## Output
Both files are written to `Deadlock_DMA/Deadlock/Const/` automatically:

| File | Contents |
|------|----------|
| `BoneListTypes.hpp` | `HitboxSlot` enum, `BonePair` struct, `ModelBoneData` struct |
| `BoneLists.hpp` | `g_HeroModelData` unordered_map keyed by hero model path |

## Credits
- [Deadlock Bone Extractor](https://www.unknowncheats.me/forum/deadlock/744334-deadlock-bone-extractor.html) (UnknownCheats) — original concept and reference for extracting bone/hitbox data from Deadlock VPKs