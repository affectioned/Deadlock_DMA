
# BoneExtractor
Extracts hero bone/hitbox data from Deadlock's VPK files and generates C++ headers used by the DMA overlay.

## Requirements
Python 3.x with a virtual environment:
```
python -m venv venv
venv\Scripts\activate
pip install vpk keyvalues3
```

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