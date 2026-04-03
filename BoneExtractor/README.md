# BoneExtractor

Extracts hero bone/hitbox data from Deadlock's VPK files and generates C++ headers used by the DMA overlay.

## Requirements

```
pip install vpk keyvalues3
```

## Usage

```
python BoneExtractor/bone_extractor.py [deadlock_vpk_dir] [output_hpp]
```

- **deadlock_vpk_dir** — path to Deadlock's `game/deadlock` folder (default: auto-detected via Steam)
- **output_hpp** — output path for `BoneLists.hpp` (default: `Deadlock_DMA/Deadlock/Const/BoneLists.hpp`)

`BoneListTypes.hpp` is written alongside `BoneLists.hpp` automatically.

## Output

| File | Contents |
|------|----------|
| `BoneListTypes.hpp` | `HitboxSlot` enum, `BonePair` struct, `ModelBoneData` struct |
| `BoneLists.hpp` | `g_HeroModelData` unordered_map keyed by hero model path |

Results are SHA-256 cached; re-running skips extraction if VPK content hasn't changed.
