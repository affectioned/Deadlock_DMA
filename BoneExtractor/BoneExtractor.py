#!/usr/bin/env python3
"""
BoneExtractor – extracts hero skeleton/hitbox data from Deadlock's VPK.

All VPK I/O and Source2 resource decompilation goes through
Source2Viewer-CLI; Python only parses the CLI's text KV3 output. This keeps
us aligned with Valve's format churn without having to track every new KV3
BinaryType in the Python keyvalues3 package.

Dependencies (install once, via requirements.txt):
    pip install -r requirements.txt   (keyvalues3 >= 0.4 for KV3 v4/v5 text parsing)

Source2Viewer-CLI.exe must be in BoneExtractor/vrf/ (version 21.0 or newer).
Download cli-windows-x64.zip from
    https://github.com/ValveResourceFormat/ValveResourceFormat/releases
and unzip into BoneExtractor/vrf/, or set the VRF_CLI env var to point at
Source2Viewer-CLI.exe elsewhere.

Usage:
    python BoneExtractor.py [path/to/BoneLists.hpp]

Outputs four files into the same directory as BoneLists.hpp:
    BoneLists.hpp      – g_HeroModelData data table
    BoneListTypes.hpp  – type definitions (BonePair, ModelBoneData, HitboxSlot)
    HeroEnum.hpp       – HeroId enum, one entry per m_HeroID in heroes.vdata_c
    HeroMap.hpp        – HeroNames::HeroNameMap, HeroId -> display name
"""

import os
import re
import sys
import hashlib
import subprocess
import winreg

# keyvalues3 >= 0.4 is required for KV3 v4/v5 text parsing. 0.4+ needs Python
# 3.12+; on older Pythons pip silently falls back to 0.3 which can't parse
# Deadlock's current hero data.
if sys.version_info < (3, 12):
    sys.exit(
        f"[error] Python 3.12+ required (found {sys.version_info.major}.{sys.version_info.minor}).\n"
        "        keyvalues3 >= 0.4 needs it for KV3 v4/v5 support; older wheels\n"
        "        cannot parse Deadlock's current hero data."
    )

try:
    from keyvalues3.textreader import KV3TextReader
except ImportError:
    sys.exit("[error] Missing dependency: pip install -r requirements.txt")


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
# Source2Viewer-CLI driver
#
# All VPK reads and resource decompiles go through the CLI. `_s2v_blocks`
# invokes `-b DATA,MDAT` (or similar) to stream named resource blocks as
# text KV3, which we split per source file and parse with KV3TextReader.
# ──────────────────────────────────────────────────────────────────

_VRF_CLI_DEFAULT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "vrf", "Source2Viewer-CLI.exe",
)
VRF_CLI = os.environ.get("VRF_CLI") or _VRF_CLI_DEFAULT

# Headers Source2Viewer-CLI prints per source file processed in a batch,
# and per block inside each source file. Keep both patterns anchored to
# a line start so we don't match body text that happens to use the words.
_FILE_HDR_RE  = re.compile(rb'^\[\d+/\d+\]\s+(\S+)\s*$',  re.MULTILINE)
_BLOCK_HDR_RE = re.compile(rb'^--- Data for block "([A-Z0-9]+)" ---\r?\n',
                            re.MULTILINE)
# Max approximate chars for a Windows command line is ~8191; we chunk `-f`
# path lists well under that to stay safely under CreateProcess limits.
_BATCH_CHARS = 6000


def _require_cli() -> None:
    if not os.path.exists(VRF_CLI):
        sys.exit(
            f"[error] Source2Viewer-CLI.exe not found at {VRF_CLI}\n"
            "        Download cli-windows-x64.zip from\n"
            "          https://github.com/ValveResourceFormat/ValveResourceFormat/releases\n"
            "        and unzip into BoneExtractor/vrf/, or set VRF_CLI to its path."
        )


def _run_cli(args: list[str], timeout: int = 300) -> bytes:
    """Run the CLI, return raw stdout bytes. Raises on non-zero exit."""
    proc = subprocess.run(
        [VRF_CLI, *args],
        capture_output=True, check=False, timeout=timeout,
    )
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", errors="replace").rstrip()
        raise RuntimeError(f"Source2Viewer-CLI exited {proc.returncode}: {err}")
    return proc.stdout


def list_vpk_paths(vpk_path: str, extension: str, prefix: str) -> list[str]:
    """Enumerate VPK contents via `--vpk_list -e <ext> -f <prefix>`.
    Returns lowercased inner paths (no CRC/size suffix)."""
    out = _run_cli(["-i", vpk_path, "--vpk_list", "-e", extension, "-f", prefix])
    paths: list[str] = []
    for line in out.decode("utf-8", errors="replace").splitlines():
        # Lines are "<path> CRC:<hex> size:<n>"
        path = line.split(" ", 1)[0].strip()
        if path:
            paths.append(path.lower())
    return paths


def _chunk_paths(paths: list[str], max_chars: int) -> list[list[str]]:
    """Split a path list into chunks so each `-f` value stays under a byte cap."""
    chunks: list[list[str]] = [[]]
    size = 0
    for p in paths:
        # +1 for the comma separator between entries
        incr = len(p) + 1
        if chunks[-1] and size + incr > max_chars:
            chunks.append([])
            size = 0
        chunks[-1].append(p)
        size += incr
    return [c for c in chunks if c]


def dump_blocks_batch(vpk_path: str, inner_paths: list[str],
                      blocks: str) -> dict[str, dict[str, list[bytes]]]:
    """
    Run CLI once (or in chunks) for the given inner paths and return:
        { inner_path: { block_name: [raw_kv3_text_bytes_per_occurrence, ...] } }

    Each inner path gets its own entry; a file with multiple blocks of the
    same type (e.g. several MDAT blocks per vmdl_c) returns them in order.
    """
    if not inner_paths:
        return {}

    result: dict[str, dict[str, list[bytes]]] = {}
    for chunk in _chunk_paths(inner_paths, _BATCH_CHARS):
        joined = ",".join(chunk)
        out = _run_cli(["-i", vpk_path, "-f", joined, "-b", blocks])

        # Split the stdout stream by "[n/m] <path>" file headers. Everything
        # between two file headers belongs to the first one.
        file_markers = list(_FILE_HDR_RE.finditer(out))
        for i, m in enumerate(file_markers):
            path = m.group(1).decode("utf-8", errors="replace").lower()
            seg_start = m.end()
            seg_end   = file_markers[i + 1].start() if i + 1 < len(file_markers) else len(out)
            segment   = out[seg_start:seg_end]

            per_block: dict[str, list[bytes]] = {}
            blk_hits = list(_BLOCK_HDR_RE.finditer(segment))
            for j, bh in enumerate(blk_hits):
                name  = bh.group(1).decode("ascii")
                bstart = bh.end()
                bend   = blk_hits[j + 1].start() if j + 1 < len(blk_hits) else len(segment)
                per_block.setdefault(name, []).append(segment[bstart:bend].rstrip())
            result[path] = per_block
    return result


def dump_blocks_single(vpk_path: str, inner_path: str,
                       blocks: str) -> dict[str, list[bytes]]:
    """Convenience wrapper for a single file; matches dump_blocks_batch's inner dict shape."""
    batch = dump_blocks_batch(vpk_path, [inner_path], blocks)
    return batch.get(inner_path.lower(), {})


def _parse_kv3(blob: bytes):
    """Parse one KV3 text block (already stripped of its '--- Data for block ...' header)."""
    return KV3TextReader().parse(blob.decode("utf-8", errors="replace"))


# ──────────────────────────────────────────────────────────────────
# Hitbox + model extraction (consume parsed KV3 roots)
# ──────────────────────────────────────────────────────────────────

def hitboxes_from_mdat_roots(mdat_roots) -> list[tuple[str, int, float]]:
    """
    Gather (bone name, group id, shape radius) for the "default" hitbox set
    from an iterable of already-parsed MDAT KV3 roots.
    """
    out: list[tuple[str, int, float]] = []
    seen: set[str] = set()

    for root in mdat_roots:
        mesh = root.value if hasattr(root, "value") else root

        for entry in _kv_get(mesh, "m_hitboxsets", default=[]) or []:
            if not isinstance(entry, dict):
                continue
            hb_set = entry.get("value")
            if not isinstance(hb_set, dict):
                continue
            set_name = str(hb_set.get("m_name") or entry.get("key") or "").lower()
            if set_name != "default":
                continue

            for hb in hb_set.get("m_HitBoxes") or []:
                if not isinstance(hb, dict):
                    continue
                bone_name = str(hb.get("m_sBoneName", "")).lower()
                if not bone_name or bone_name in seen:
                    continue
                seen.add(bone_name)
                out.append((bone_name,
                            int(hb.get("m_nGroupId", 0)),
                            float(hb.get("m_flShapeRadius", 0.0))))

    return out


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


BODY_CORES = (
    "spine_", "neck_", "clavicle_",
    "arm_upper_", "arm_lower_",
    "hand_l", "hand_r",
    "leg_upper_", "leg_lower_",
    "ankle_", "ball_",
)
BODY_EXACT = ("pelvis", "head")

# Rig helpers that ride along a real bone: twists, bend helpers, aim targets.
# They are not part of the silhouette and only ever drew as stray stubs.
HELPER_RE = re.compile(r"(_twist\d*|_notwist|_hlp(_[lr])?|_aim(_upv)?)$")


def _split_bone(lower: str) -> tuple[str, str] | None:
    """Split a bone name into (rig prefix, canonical body-bone name).

    Several heroes carry more than one body in a single skeleton and tell them
    apart by prefix: Baba's hut has the unprefixed pelvis/leg chain while the
    witch riding it is baba_spine_0 .. baba_head, Nano's cat is cat_*, Rat
    King's minions are minion0_*. Matching the canonical name as a suffix
    covers all of them.

    Returns None for bones that are not part of a body - props, weapon
    attachments, and the helpers matched by HELPER_RE.
    """
    if HELPER_RE.search(lower):
        return None

    for name in BODY_EXACT:
        if lower == name:
            return "", name
        if lower.endswith("_" + name):
            return lower[:-len(name) - 1], name

    for core in BODY_CORES:
        i = lower.find(core)
        if i == 0:
            return "", lower
        if i > 0 and lower[i - 1] == "_":
            return lower[:i - 1], lower[i:]

    return None


def _kv_get(obj, *keys, default=None):
    """Safe nested get for dicts; tolerates missing keys."""
    for k in keys:
        if not isinstance(obj, dict):
            return default
        obj = obj.get(k, default)
        if obj is default:
            return default
    return obj


def extract_model_data(root, hitboxes: list[tuple[str, int, float]]) -> dict | None:
    """
    Extract bone pairs, bone IDs, and hitbox slot data from a parsed KV3 root.
    `hitboxes` comes from hitboxes_from_mdat_roots() over the same resource.
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

    MAX_PAIRS = 70

    # ---- Bone pairs ----
    # Bones are stored root-first in Source2 skeletons, so iterating in index
    # order naturally prioritises the core spine/head chain before extremities.
    pairs = []
    for i, (name, parent) in enumerate(bones):
        if len(pairs) >= MAX_PAIRS:
            break
        if parent < 0 or parent >= n_bones:
            continue
        child  = _split_bone(name)
        anchor = _split_bone(bones[parent][0])
        if not child or not anchor:
            continue
        # Same rig only. Where the prefixes differ the parent link is mounting
        # hardware - witch to hut, shoulder to cat - not a limb, and drawing it
        # throws a line across the model.
        if child[0] != anchor[0]:
            continue
        pairs.append((parent, i, bones[parent][0], name))

    # ---- Bone IDs (sorted by name) ----
    ids = {name: i for i, (name, _) in enumerate(bones) if _split_bone(name)}

    # ---- Hitbox groups ----
    groups: list[list[tuple[int, str, float]]] = [[] for _ in range(SLOT_COUNT)]
    seen_per_slot: list[set[int]] = [set() for _ in range(SLOT_COUNT)]

    for bone_name, group_id, radius in hitboxes:
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

    # Fallback by bone name, for slots the hitbox set never covered. Snapshot
    # which slots are empty first — appending here must not stop the loop from
    # collecting the rest of that slot's bones.
    empty_slots = {s for s in range(SLOT_COUNT) if not groups[s]}
    for i, (name, _) in enumerate(bones):
        slot = _classify_by_name(name)
        if slot < 0 or slot not in empty_slots:
            continue
        if i in seen_per_slot[slot]:
            continue
        seen_per_slot[slot].add(i)
        groups[slot].append((i, name, 0.0))

    # Sort each group by radius descending
    for g in groups:
        g.sort(key=lambda e: e[2], reverse=True)

    slot_bones = [[e[0] for e in g] for g in groups]

    return {"pairs": pairs, "ids": ids, "slot_bones": slot_bones}


# ──────────────────────────────────────────────────────────────────
# Hero ids and display names
# ──────────────────────────────────────────────────────────────────

# Identifiers and display strings the game data does not give us verbatim.
# Keyed by codename (the heroes.vdata_c key minus its "hero_" prefix):
#   krill       - the overlay has always shown the shorter "Mo+Krill".
#   targetdummy - "Dummy" is what the overlay called it before this was
#                 generated; keeping it avoids churning the name in the UI.
#   doorman     - localization says "The Doorman", the hero is just Doorman.
#   testhero    - no localization entry at all, so derivation would produce
#                 "Testhero" from the codename.
NAME_OVERRIDES: dict[str, tuple[str, str]] = {
    "krill":       ("MoKrill", "Mo+Krill"),
    "targetdummy": ("Dummy",   "Dummy"),
    "doorman":     ("Doorman", "Doorman"),
    "testhero":    ("TestHero", "TestHero"),
}


def find_hero_names_file(vpk_path: str) -> str | None:
    """
    Locate citadel_gc_hero_names_english.txt, which ships loose rather than
    packed - the VPK carries no citadel_gc_* localization at all.
    """
    citadel_dir = os.path.dirname(vpk_path)
    candidate = os.path.join(
        citadel_dir, "resource", "localization",
        "citadel_gc_hero_names", "citadel_gc_hero_names_english.txt")
    return candidate if os.path.exists(candidate) else None


def parse_hero_names(path: str) -> dict[str, str]:
    """Map codename -> display name from the "hero_<codename>:n" loc keys."""
    raw = open(path, "rb").read()
    for enc in ("utf-8-sig", "utf-16", "utf-8"):
        try:
            text = raw.decode(enc)
            break
        except UnicodeDecodeError:
            continue
    else:
        return {}

    return {m.group(1).lower(): m.group(2) for m in
            re.finditer(r'"hero_([a-z0-9_]+):n"\s*"([^"]*)"', text, re.IGNORECASE)}


def to_identifier(text: str) -> str:
    """"Grey Talon" -> "GreyTalon"; keeps C++ identifiers out of the data."""
    return "".join(w[:1].upper() + w[1:]
                   for w in re.split(r"[^A-Za-z0-9]+", text) if w)


def collect_heroes(vpk_path: str, loc_names: dict[str, str]) -> list[dict]:
    """
    Pull every hero carrying an m_HeroID out of heroes.vdata_c (via CLI).

    Disabled heroes are kept: they cannot spawn, but covering their ids means
    an unexpected m_HeroID still resolves to a name instead of falling
    through the lookup. They are flagged so the output can group them.
    """
    blocks = dump_blocks_single(vpk_path, "scripts/heroes.vdata_c", "DATA")
    data_blobs = blocks.get("DATA") or []
    if not data_blobs:
        return []

    root = _parse_kv3(data_blobs[0])
    entries = root.value if hasattr(root, "value") else root
    if not isinstance(entries, dict):
        return []

    heroes: list[dict] = []
    for key, hero in entries.items():
        if not key.startswith("hero_") or key == "hero_base":
            continue
        if not isinstance(hero, dict):
            continue
        hero_id = hero.get("m_HeroID")
        if hero_id is None:
            continue

        codename = key[len("hero_"):]
        override = NAME_OVERRIDES.get(codename.lower())
        if override:
            ident, display = override
        else:
            display = loc_names.get(codename.lower()) or to_identifier(codename)
            ident = to_identifier(display)

        heroes.append({
            "id":       int(hero_id),
            "codename": codename,
            "ident":    ident,
            "display":  display,
            "disabled": bool(hero.get("m_bDisabled")),
        })

    heroes.sort(key=lambda h: (h["disabled"], h["ident"].lower()))
    return heroes


# ──────────────────────────────────────────────────────────────────
# Output generation (mirrors OutputBuilder.cs exactly)
# ──────────────────────────────────────────────────────────────────

BANNER = (
    "// Auto-generated by BoneExtractor - DO NOT EDIT\n"
    "// Regenerate: python BoneExtractor/BoneExtractor.py\n"
)


def build_types_header() -> str:
    enum_lines = "".join(
        f"    {SLOT_NAMES[s]:<5} = {s},\n" for s in range(SLOT_COUNT)
    )
    enum_lines += f"    Count = {SLOT_COUNT},\n"
    return (
        f"{BANNER}"
        "#pragma once\n"
        "#include <cstdint>\n"
        "#include <string>\n"
        "#include <unordered_map>\n"
        "#include <vector>\n"
        "\n"
        "// clang-format off\n"
        "\n"
        "// Mirrors HitboxSlot in BoneExtractor/BoneExtractor.py.\n"
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
        BANNER,
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


DISABLED_NOTE = "Disabled in heroes.vdata_c - present for id coverage, never spawns."


def _hero_block(heroes: list[dict], indent: str, fmt, comma: bool = True) -> str:
    """
    Render one hero list, live heroes first and the disabled ones behind a
    note. With `comma`, entries are separated like list elements and the last
    one goes without a trailing comma; without it they are plain statements.
    """
    live = [h for h in heroes if not h["disabled"]]
    disabled = [h for h in heroes if h["disabled"]]

    rows: list[tuple[bool, str]] = [(True, fmt(h)) for h in live]
    if disabled:
        rows.append((False, ""))
        rows.append((False, f"// {DISABLED_NOTE}"))
        rows += [(True, fmt(h)) for h in disabled]

    last_entry = max((i for i, (is_entry, _) in enumerate(rows) if is_entry),
                     default=-1)

    out = []
    for i, (is_entry, text) in enumerate(rows):
        if not text:
            out.append("\n")
        elif comma and is_entry and i != last_entry:
            out.append(f"{indent}{text},\n")
        else:
            out.append(f"{indent}{text}\n")
    return "".join(out)


def build_hero_enum_header(heroes: list[dict]) -> str:
    entries = _hero_block(heroes, "\t",
                          lambda h: "{} = {}".format(h["ident"], h["id"]))
    return (
        f"{BANNER}"
        "#pragma once\n"
        '#include "pch.h"\n'
        "\n"
        "enum class HeroId : int32_t\n"
        "{\n"
        f"{entries}"
        "};\n"
    )


def build_hero_map_header(heroes: list[dict]) -> str:
    names = _hero_block(
        heroes, "\t",
        lambda h: 'const inline std::string {} = "{}";'.format(
            h["ident"], h["display"]),
        comma=False)

    entries = _hero_block(
        heroes, "\t\t",
        lambda h: "{{HeroId::{0}, {0}}}".format(h["ident"]))

    return (
        f"{BANNER}"
        "#pragma once\n"
        '#include "pch.h"\n'
        '#include "HeroEnum.hpp"\n'
        "\n"
        "namespace HeroNames\n"
        "{\n"
        f"{names}"
        "\n"
        "\tconst inline std::unordered_map<HeroId, std::string_view> HeroNameMap = {\n"
        f"{entries}"
        "\t};\n"
        "}\n"
    )


# ──────────────────────────────────────────────────────────────────
# Cache helpers
# ──────────────────────────────────────────────────────────────────

def sha256_files(*paths: str | None) -> str:
    """Hash every input the headers are generated from, in order."""
    h = hashlib.sha256()
    for path in paths:
        if path is None:
            continue
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
# Hero models live under one of three prefixes depending on release state;
# all three go in the same CLI `-f` call (comma-separated).
HERO_PREFIXES = "models/heroes/,models/heroes_staging/,models/heroes_wip/"
HERO_RE = re.compile(r"^models/heroes(?:_staging|_wip)?/", re.IGNORECASE)
HERO_VDATA = "scripts/heroes.vdata_c"

# Default output: one directory up from this script, then into Deadlock_DMA/Deadlock/Const/
_SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_OUT = os.path.normpath(os.path.join(_SCRIPT_DIR, r"..\Deadlock_DMA\Deadlock\Const\BoneLists.hpp"))


def main():
    pairs_path = sys.argv[1] if len(sys.argv) > 1 else _DEFAULT_OUT
    out_dir    = os.path.dirname(os.path.abspath(pairs_path))
    types_path = os.path.join(out_dir, "BoneListTypes.hpp")
    enum_path  = os.path.join(out_dir, "HeroEnum.hpp")
    map_path   = os.path.join(out_dir, "HeroMap.hpp")
    # Anchored to the script, not the cwd, so the cache is found wherever the
    # script is run from.
    cache_path = os.path.join(_SCRIPT_DIR, "vpk.cache")

    _require_cli()

    # Find VPK
    vpk_path = find_vpk(VPK_RELATIVE) or (FALLBACK_VPK if os.path.exists(FALLBACK_VPK) else None)
    if not vpk_path:
        sys.exit("[error] VPK not found. Is Deadlock installed?")

    # Hero display names ship loose beside the VPK rather than inside it
    loc_path = find_hero_names_file(vpk_path)
    if loc_path:
        print(f"[*] Hero names: {loc_path}")
    else:
        print("[warn] citadel_gc_hero_names_english.txt not found - hero names "
              "will fall back to codenames", file=sys.stderr)

    # Cache check
    current_hash = sha256_files(vpk_path, loc_path)
    if is_up_to_date(cache_path, current_hash,
                     pairs_path, types_path, enum_path, map_path):
        print("[*] Game files unchanged - skipping extraction")
        print(f"[*] Delete '{cache_path}' to force regeneration")
        return

    print(f"[*] VPK: {vpk_path}")
    print(f"[*] CLI: {VRF_CLI}")

    # Enumerate hero models in one call. VPK lists include _lod variants and
    # the _prefabs thirdperson camera stubs; strip both.
    all_paths = list_vpk_paths(vpk_path, "vmdl_c", HERO_PREFIXES)
    hero_paths = [
        p for p in all_paths
        if HERO_RE.match(p) and "_lod" not in p and "/_prefabs/" not in p
    ]
    print(f"[*] {len(hero_paths)} candidate hero models to process")

    # Decompile DATA + MDAT blocks for every hero vmdl_c. Batched so each CLI
    # invocation stays under the Windows command-line length limit; stdout is
    # split into per-file blocks on the "[n/m] <path>" markers.
    print("[*] Dumping DATA+MDAT blocks via Source2Viewer-CLI...")
    blocks_by_path = dump_blocks_batch(vpk_path, hero_paths, "DATA,MDAT")

    model_map: dict[str, dict] = {}
    processed = skipped = 0

    for filepath in hero_paths:
        model_path = filepath[:-2].lower()  # strip "_c" suffix
        if model_path in model_map:
            continue

        blocks = blocks_by_path.get(filepath, {})
        data_blobs = blocks.get("DATA") or []
        mdat_blobs = blocks.get("MDAT") or []
        if not data_blobs:
            skipped += 1
            continue

        try:
            root = _parse_kv3(data_blobs[0])
        except Exception as e:
            print(f"  [err] {filepath}: DATA parse failed: {e}", file=sys.stderr)
            skipped += 1
            continue

        mdat_roots = []
        for blob in mdat_blobs:
            try:
                mdat_roots.append(_parse_kv3(blob))
            except Exception:
                # Individual MDAT parse failures are tolerable — hitboxes are
                # best-effort and the per-name fallback picks them up.
                continue
        hitboxes = hitboxes_from_mdat_roots(mdat_roots)

        model = extract_model_data(root, hitboxes)
        if model is None:
            skipped += 1
            continue

        has_data = bool(model["pairs"]) or any(model["slot_bones"])
        if not has_data:
            skipped += 1
            continue

        pair_note = " (capped)" if len(model['pairs']) == 70 else ""
        print(f"  [ok] {model_path:<72}  {len(model['pairs'])} pairs{pair_note}  {len(model['ids'])} ids")
        model_map[model_path] = model
        processed += 1

    print(f"\n[*] {processed} models extracted, {skipped} skipped")

    if not model_map:
        sys.exit("[error] No data extracted")

    # Hero ids
    loc_names = parse_hero_names(loc_path) if loc_path else {}
    heroes = collect_heroes(vpk_path, loc_names)
    if not heroes:
        sys.exit(f"[error] No hero ids extracted from {HERO_VDATA}")

    live = sum(1 for h in heroes if not h["disabled"])
    print(f"[*] {len(heroes)} hero ids ({live} live, {len(heroes) - live} disabled)")

    os.makedirs(out_dir, exist_ok=True)

    with open(types_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_types_header())
    with open(pairs_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_pairs_header(model_map))
    with open(enum_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_hero_enum_header(heroes))
    with open(map_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(build_hero_map_header(heroes))
    with open(cache_path, "w") as f:
        f.write(current_hash)

    print(f"[+] Written: {types_path}")
    print(f"[+] Written: {pairs_path}  ({len(model_map)} models)")
    print(f"[+] Written: {enum_path}  ({len(heroes)} ids)")
    print(f"[+] Written: {map_path}  ({len(heroes)} names)")


if __name__ == "__main__":
    main()
