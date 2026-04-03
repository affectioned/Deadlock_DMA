
#!/usr/bin/env python3
"""
BoneExtractor – extracts hero skeleton/hitbox data from Deadlock's VPK.

Dependencies (install once):
    pip install vpk keyvalues3

Usage:
    python bone_extractor.py [path/to/BoneLists.hpp]

Outputs two files into the same directory as BoneLists.hpp:
    BoneLists.hpp      – g_HeroModelData data table
    BoneListTypes.hpp  – type definitions (BonePair, ModelBoneData, HitboxSlot)
"""

import io
import os
import re
import sys
import struct
import hashlib
import winreg

try:
    import vpk
except ImportError:
    sys.exit("[error] Missing dependency: pip install vpk")

try:
    import keyvalues3 as kv3
except ImportError:
    sys.exit("[error] Missing dependency: pip install keyvalues3")


# ──────────────────────────────────────────────────────────────────
# Steam locator
# ──────────────────────────────────────────────────────────────────

def _reg_str(hive, key_path: str, value_name: str) -> str | None:
    try:
        with winreg.OpenKey(hive, key_path) as k:
            val, _ = winreg.QueryValueEx(k, value_name)
            return val if isinstance(val, str) else None
    except OSError:
        return None


def _find_steam_root() -> str | None:
    candidates = [
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam",             "InstallPath"),
        (winreg.HKEY_CURRENT_USER,  r"SOFTWARE\Valve\Steam",             "SteamPath"),
        (winreg.HKEY_CURRENT_USER,  r"SOFTWARE\Valve\Steam",             "InstallPath"),
    ]
    for hive, key, val in candidates:
        p = _reg_str(hive, key, val)
        if p:
            return p
    return None


def _library_roots(steam_root: str) -> list[str]:
    roots = [steam_root]
    vdf = os.path.join(steam_root, r"steamapps\libraryfolders.vdf")
    if os.path.exists(vdf):
        for m in re.finditer(r'"path"\s+"([^"]+)"', open(vdf, encoding="utf-8").read()):
            path = m.group(1).replace("\\\\", "\\")
            if path:
                roots.append(path)
    return roots


def find_vpk(relative: str) -> str | None:
    root = _find_steam_root()
    if not root:
        return None
    for lib in _library_roots(root):
        candidate = os.path.join(lib, relative)
        if os.path.exists(candidate):
            print(f"[*] Auto-detected: {candidate}")
            return candidate
    return None


# ──────────────────────────────────────────────────────────────────
# Source2 resource file: extract DATA block
# ──────────────────────────────────────────────────────────────────

def extract_data_block(resource_bytes: bytes) -> bytes | None:
    """Pull the raw DATA block bytes out of a compiled Source2 resource file."""
    if len(resource_bytes) < 16:
        return None

    # Header: uint32 fileSize, uint16 headerVersion, uint16 version,
    #         uint32 blockOffset, uint32 blockCount
    header_version = struct.unpack_from("<H", resource_bytes, 4)[0]
    if header_version != 12:
        return None

    block_offset, block_count = struct.unpack_from("<II", resource_bytes, 8)
    # blockOffset is relative to position 8 (where blockOffset field lives)
    table_start = 8 + block_offset

    for i in range(block_count):
        entry = table_start + i * 12
        if entry + 12 > len(resource_bytes):
            break
        tag = resource_bytes[entry:entry + 4]
        if tag != b"DATA":
            continue
        rel_off, size = struct.unpack_from("<II", resource_bytes, entry + 4)
        # rel_off is relative to the position of the offset field itself (entry+4)
        abs_off = (entry + 4) + rel_off
        if abs_off + size > len(resource_bytes):
            return None
        return resource_bytes[abs_off: abs_off + size]

    return None


# ──────────────────────────────────────────────────────────────────
# Hitbox slot classification (mirrors GameBoneExtractor.cs)
# ──────────────────────────────────────────────────────────────────

SLOT_HEAD  = 0
SLOT_NECK  = 1
SLOT_TORSO = 2
SLOT_ARMS  = 3
SLOT_LEGS  = 4
SLOT_COUNT = 5
SLOT_NAMES = ["Head", "Neck", "Torso", "Arms", "Legs"]


def _classify_by_name(lower: str) -> int:
    if lower == "head":                          return SLOT_HEAD
    if lower.startswith("neck_"):                return SLOT_NECK
    if (lower.startswith("pelvis") or
            lower.startswith("spine_") or
            lower.startswith("belly_")):         return SLOT_TORSO
    if (lower.startswith("arm_lower_") or
            lower.startswith("clavicle_")):      return SLOT_ARMS
    if (lower.startswith("leg_lower") or
            lower.startswith("ankle_")):         return SLOT_LEGS
    return -1


def _classify_hitbox(group_id: int, bone_lower: str) -> int:
    if group_id == 1:         return SLOT_HEAD
    if group_id in (2, 3):    return SLOT_TORSO
    if group_id in (4, 5):    return SLOT_ARMS
    if group_id in (6, 7):    return SLOT_LEGS
    if group_id == 0:         return _classify_by_name(bone_lower)
    return -1  # accessories/equipment


def _is_body_bone(lower: str) -> bool:
    return lower in ("pelvis", "head") or any(lower.startswith(p) for p in (
        "spine_", "neck_", "clavicle_",
        "arm_upper_", "arm_lower_",
        "hand_l", "hand_r",
        "leg_upper_", "leg_lower_",
        "ankle_", "ball_",
    ))


# ──────────────────────────────────────────────────────────────────
# Extraction from a parsed KV3 root dict
# ──────────────────────────────────────────────────────────────────

def _kv_get(obj, *keys, default=None):
    """Safe nested get for dicts; tolerates missing keys."""
    for k in keys:
        if not isinstance(obj, dict):
            return default
        obj = obj.get(k, default)
        if obj is default:
            return default
    return obj


def extract_model_data(root) -> dict | None:
    """
    Extract bone pairs, bone IDs, and hitbox slot data from a parsed KV3 root.
    Returns a dict with keys 'pairs', 'ids', 'slot_bones', or None on failure.
    """
    # keyvalues3 returns either a KV3File (subscriptable) or a plain dict
    data = root.value if hasattr(root, "value") else root

    skel = _kv_get(data, "m_modelSkeleton")
    if skel is None:
        return None

    bone_names  = _kv_get(skel, "m_boneName",  default=[])
    bone_parents = _kv_get(skel, "m_nParent",  default=[])

    if not bone_names or len(bone_names) != len(bone_parents):
        return None

    # Normalise bone names to lowercase
    bones = [(str(n).lower(), int(p)) for n, p in zip(bone_names, bone_parents)]
    n_bones = len(bones)

    # ---- Bone pairs ----
    pairs = []
    for i, (name, parent) in enumerate(bones):
        if parent < 0 or parent >= n_bones:
            continue
        if not _is_body_bone(name):
            continue
        if not _is_body_bone(bones[parent][0]):
            continue
        pairs.append((parent, i, bones[parent][0], name))

    # ---- Bone IDs (sorted by name) ----
    ids = {name: i for i, (name, _) in enumerate(bones) if _is_body_bone(name)}

    # ---- Hitbox groups ----
    groups: list[list[tuple[int, str, float]]] = [[] for _ in range(SLOT_COUNT)]
    seen_per_slot: list[set[int]] = [set() for _ in range(SLOT_COUNT)]

    hitbox_sets = _kv_get(data, "m_HitboxSets", default=[])
    for hb_set in hitbox_sets:
        if not isinstance(hb_set, dict):
            continue
        if str(hb_set.get("m_name", "")).lower() != "default":
            continue

        for hb in hb_set.get("m_HitBoxes", []):
            if not isinstance(hb, dict):
                continue
            bone_name  = str(hb.get("m_sBoneName", "")).lower()
            group_id   = int(hb.get("m_nGroupId", 0))
            radius     = float(hb.get("m_flShapeRadius", 0.0))

            slot = _classify_hitbox(group_id, bone_name)
            if slot < 0:
                continue

            bone_idx = next((i for i, (n, _) in enumerate(bones) if n == bone_name), None)
            if bone_idx is None:
                continue
            if bone_idx in seen_per_slot[slot]:
                continue
            seen_per_slot[slot].add(bone_idx)
            groups[slot].append((bone_idx, bone_name, radius))

        break  # found "default" set

    # Fallback: fill empty slots from skeleton by name
    for i, (name, _) in enumerate(bones):
        slot = _classify_by_name(name)
        if slot < 0 or groups[slot]:
            continue
        groups[slot].append((i, name, 0.0))

    # Sort each group by radius descending
    for g in groups:
        g.sort(key=lambda e: e[2], reverse=True)

    slot_bones = [[e[0] for e in g] for g in groups]

    return {"pairs": pairs, "ids": ids, "slot_bones": slot_bones}


# ──────────────────────────────────────────────────────────────────
# Output generation (mirrors OutputBuilder.cs exactly)
# ──────────────────────────────────────────────────────────────────

def build_types_header() -> str:
    enum_lines = "".join(
        f"    {SLOT_NAMES[s]:<5} = {s},\n" for s in range(SLOT_COUNT)
    )
    enum_lines += f"    Count = {SLOT_COUNT},\n"
    return (
        "// Auto-generated by Tools/BoneExtractor - DO NOT EDIT\n"
        "// Regenerate: python BoneExtractor/bone_extractor.py\n"
        "#pragma once\n"
        "#include <cstdint>\n"
        "#include <string>\n"
        "#include <unordered_map>\n"
        "#include <vector>\n"
        "\n"
        "// clang-format off\n"
        "\n"
        "// Mirrors HitboxSlot in Tools/BoneExtractor/bone_extractor.py.\n"
        "// Order matches the slotBones array in HeroSkeletonPairs.hpp.\n"
        "enum class HitboxSlot : int\n"
        "{\n"
        f"{enum_lines}"
        "};\n"
        "\n"
        "inline constexpr int kHitboxSlotCount = static_cast<int>(HitboxSlot::Count);\n"
        "\n"
        "struct BonePair\n"
        "{\n"
        "    int idA;\n"
        "    int idB;\n"
        "};\n"
        "\n"
        "// slotBones uses a C-array instead of std::array to avoid double-brace\n"
        "// requirements in MSVC aggregate initialization.\n"
        "// Sorted by hitbox radius descending; empty means the slot is absent for this model.\n"
        "struct ModelBoneData\n"
        "{\n"
        "    std::vector<BonePair>                pairs;\n"
        "    std::unordered_map<std::string, int> ids;\n"
        "    std::vector<int16_t>                 slotBones[kHitboxSlotCount];\n"
        "};\n"
        "\n"
        "// clang-format on\n"
    )


def build_pairs_header(model_map: dict[str, dict]) -> str:
    lines = [
        "// Auto-generated by Tools/BoneExtractor - DO NOT EDIT\n",
        "// Regenerate: python BoneExtractor/bone_extractor.py\n",
        "#pragma once\n",
        '#include "BoneListTypes.hpp"\n',
        "\n",
        "// clang-format off\n",
        "inline const std::unordered_map<std::string, ModelBoneData>\n",
        "    g_HeroModelData =\n",
        "{\n\n",
    ]

    for model_path, model in sorted(model_map.items()):
        lines.append(f"    // {model_path}\n")
        lines.append(f'    {{ "{model_path}", {{\n')

        # pairs
        lines.append("        /* pairs */ {\n")
        for idA, idB, nameA, nameB in model["pairs"]:
            lines.append(f"            BonePair{{ {idA:3}, {idB:3} }},  // {nameA} -> {nameB}\n")
        lines.append("        },\n")

        # ids
        lines.append("        /* ids */ {\n")
        for name in sorted(model["ids"]):
            idx = model["ids"][name]
            lines.append(f'            {{ "{name}", {idx:3} }},\n')
        lines.append("        },\n")

        # slotBones
        lines.append("        /* slotBones */ {\n")
        for s, slot_name in enumerate(SLOT_NAMES):
            bones = model["slot_bones"][s]
            if not bones:
                lines.append(f"            /* [{s}] {slot_name:<5} */ {{}},\n")
            else:
                indices = ", ".join(str(b) for b in bones)
                lines.append(f"            /* [{s}] {slot_name:<5} */ {{ {indices} }},\n")
        lines.append("        },\n")

        lines.append("    }},\n\n")

    lines.append("};\n")
    lines.append("// clang-format on\n")
    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Cache helpers
# ──────────────────────────────────────────────────────────────────

def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def is_up_to_date(cache_path: str, current_hash: str, *output_paths: str) -> bool:
    if not all(os.path.exists(p) for p in output_paths):
        return False
    try:
        return open(cache_path).read().strip() == current_hash
    except OSError:
        return False


# ──────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────

VPK_RELATIVE = r"steamapps\common\Deadlock\game\citadel\pak01_dir.vpk"
FALLBACK_VPK = r"C:\Program Files (x86)\Steam\steamapps\common\Deadlock\game\citadel\pak01_dir.vpk"
HERO_RE = re.compile(r"^models/heroes(?:_staging|_wip)?/", re.IGNORECASE)

# Default output: one directory up from this script, then into Deadlock_DMA/Deadlock/Const/
_SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_OUT = os.path.normpath(os.path.join(_SCRIPT_DIR, r"..\Deadlock_DMA\Deadlock\Const\BoneLists.hpp"))


def main():
    pairs_path = sys.argv[1] if len(sys.argv) > 1 else _DEFAULT_OUT
    out_dir    = os.path.dirname(os.path.abspath(pairs_path))
    types_path = os.path.join(out_dir, "BoneListTypes.hpp")
    cache_path = "vpk.cache"

    # Find VPK
    vpk_path = find_vpk(VPK_RELATIVE) or (FALLBACK_VPK if os.path.exists(FALLBACK_VPK) else None)
    if not vpk_path:
        sys.exit("[error] VPK not found. Is Deadlock installed?")

    # Cache check
    current_hash = sha256_file(vpk_path)
    if is_up_to_date(cache_path, current_hash, pairs_path, types_path):
        print("[*] VPK unchanged - skipping extraction")
        print(f"[*] Delete '{cache_path}' to force regeneration")
        return

    print(f"[*] Opening VPK: {vpk_path}")
    pak = vpk.open(vpk_path)

    model_map: dict[str, dict] = {}
    processed = skipped = 0

    for filepath in pak:
        if not filepath.endswith(".vmdl_c"):
            continue
        if not HERO_RE.match(filepath):
            continue
        if "_lod" in filepath.lower():
            continue

        model_path = filepath[:-2].lower()  # strip "_c" suffix
        if model_path in model_map:
            continue

        try:
            raw  = pak[filepath].read()
            data = extract_data_block(raw)
            if data is None:
                skipped += 1
                continue

            root   = kv3.read(io.BytesIO(data))
            model  = extract_model_data(root)
            if model is None:
                skipped += 1
                continue

            has_data = bool(model["pairs"]) or any(model["slot_bones"])
            if not has_data:
                skipped += 1
                continue

            print(f"  [ok] {model_path:<72}  {len(model['pairs'])} pairs  {len(model['ids'])} ids")
            model_map[model_path] = model
            processed += 1

        except Exception as e:
            print(f"  [err] {filepath}: {e}", file=sys.stderr)
            skipped += 1

    print(f"\n[*] {processed} models extracted, {skipped} skipped")

    if not model_map:
        sys.exit("[error] No data extracted")

    os.makedirs(out_dir, exist_ok=True)

    with open(types_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_types_header())
    with open(pairs_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_pairs_header(model_map))
    with open(cache_path, "w") as f:
        f.write(current_hash)

    print(f"[+] Written: {types_path}")
    print(f"[+] Written: {pairs_path}  ({len(model_map)} models)")


if __name__ == "__main__":
    main()
