# SPDX-License-Identifier: GPL-3.0-or-later
"""Merge six pinned species text banks into a fresh PRIVATE runtime archive.

No text or game resource is bundled. The generated source archive is retained
except for the six reviewed banks; their original decoded entries must match.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

SOURCE_ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("community_modern_assets_text", Path(__file__).with_name("prepare-modern-assets.py"))
assets = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = assets
_spec.loader.exec_module(assets)

REFERENCE_SHA = "9fafea3234d1051501b693facc4f3c9eae9d08e0d3a70363f12ffa101532676e"
# index, original count, original hash, reference hash, maximum decoded units
BANKS = (
    (412, 496, "bdfb6361087ff00a9139a020f360b8afb5b68bb8a712ca4411cab5c86c6fb73b", "c161b7b358c65e3c308244ec0be19ecaa49a96c6c45270230852d800fb2771d8", 11),
    (413, 496, "54c6670fa785e53c75ba2270668970ef16d9dff2e384dda38541d45a55adc671", "9cefe4ab8ab2aa44adb4dde99ee662ff17f1df84a653d7af68fd1c4a08ed0257", 22),
    (706, 494, "ba086305104347170ea4db95a4666b4f08c6f4aa8b0719e19d7abc7c6bca3ef3", "df4fe8abc7f1771a970e030e0667b00033844b03400a10489f96bcec52aba171", 152),
    (707, 494, "8ad729e03579242690a693771819209e788a0a5a2dc5da55e6bc2f8a16465fdc", "025d6709becddf70cf195319be3598bdc6704f747e58fd25ccb47d4f0176da2a", 12),
    (709, 494, "7283d4d3454d6a17f93313280e05c7cb0b8da32694ee6c7aaeb3f42f65547da6", "ebecf6058b5cf5db9b1ea27df7b60af5730ca934c26783d503e4835f0a2b09c3", 8),
    (711, 494, "6c23c14f4f123d76ce56722b5a02cb265cac0b582b2a8632bbb1bdd08c146626", "cd58e0003802a564c90ce01c022a0176704c83965b05bdc3710a095647f50694", 21),
)


def decode_bank(data, maximum=4096):
    if len(data) < 4 or len(data) > 1024 * 1024:
        raise assets.AssetError("Invalid species text bank size")
    count, seed = struct.unpack_from("<HH", data)
    end = 4 + count * 8
    if not 1 <= count <= 2048 or end > len(data):
        raise assets.AssetError("Invalid species text entry table")
    result = []
    for i in range(count):
        key = seed * 765 * (i + 1) & 65535
        key |= key << 16
        offset, length = (value ^ key for value in struct.unpack_from("<II", data, 4 + i * 8))
        if offset != end or offset % 2 or not 1 <= length <= maximum or length * 2 > len(data) - offset:
            raise assets.AssetError("Invalid, overlapping or oversized species text entry")
        words = struct.unpack_from("<" + "H" * length, data, offset)
        decoded = tuple(value ^ (((i + 1) * 596947 + j * 18749) & 65535) for j, value in enumerate(words))
        if decoded[-1] != 65535:
            raise assets.AssetError("Unterminated species text entry")
        result.append(decoded)
        end = offset + length * 2
    if len(data) != end:
        raise assets.AssetError("Unexpected species text bank tail")
    return result


def merge_banks(base, reference, pins=BANKS, reference_count=577):
    if len(base.members) != 724 or len(reference.members) != 724:
        raise assets.AssetError("Expected 724 message banks")
    result = list(base.members)
    if len({pin[0] for pin in pins}) != len(pins):
        raise assets.AssetError("Duplicate selected text bank")
    for index, original_count, base_hash, reference_hash, maximum in pins:
        if not 0 <= index < len(result):
            raise assets.AssetError("Selected bank outside archive")
        original, modern = base.members[index], reference.members[index]
        if assets.digest(original) != base_hash or assets.digest(modern) != reference_hash:
            raise assets.AssetError("Species text bank hash mismatch")
        old_entries, entries = decode_bank(original, maximum), decode_bank(modern, maximum)
        if len(old_entries) != original_count or len(entries) != reference_count or entries[:original_count] != old_entries:
            raise assets.AssetError("Species text count/original prefix mismatch")
        result[index] = modern
    selected = {pin[0] for pin in pins}
    if any(member != result[i] for i, member in enumerate(base.members) if i not in selected):
        raise assets.AssetError("Unselected message bank changed")
    return assets.Narc(tuple(result), base.names)


def prepare(base_path, reference_path, output):
    base_path, reference_path, output = (path.resolve() for path in (base_path, reference_path, output))
    if output.exists() or output == SOURCE_ROOT or SOURCE_ROOT in output.parents:
        raise assets.AssetError("Output must be a fresh private directory outside the source checkout")
    if any(path.parent == output or output in path.parents or path.parent in output.parents for path in (base_path, reference_path)):
        raise assets.AssetError("Output must be separate from both input directories")
    base_bytes, reference_bytes = base_path.read_bytes(), reference_path.read_bytes()
    if assets.digest(reference_bytes) != REFERENCE_SHA:
        raise assets.AssetError("Pinned reference message archive mismatch")
    merged = assets.encode_narc(merge_banks(assets.parse_narc(base_bytes), assets.parse_narc(reference_bytes)))
    output.mkdir(parents=True, exist_ok=False)
    target = output / "msgdata/pl_msg.narc"
    target.parent.mkdir()
    target.write_bytes(merged)
    (output / "PRIVATE-SPECIES-TEXT.json").write_text(json.dumps({
        "format": "community-private-species-text-v1", "private": True,
        "purpose": "isolated own-client testing; never a release", "runtime_verified": False,
        "path": "msgdata/pl_msg.narc", "sha256": assets.digest(merged),
        "source_sha256": assets.digest(base_bytes), "reference_sha256": REFERENCE_SHA,
        "selected_banks": [pin[0] for pin in BANKS], "bank_count": 724,
        "decoded_original_prefixes_preserved": True, "unselected_banks_preserved": True,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-archive", required=True, type=Path)
    parser.add_argument("--reference-archive", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        prepare(args.source_archive, args.reference_archive, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")
    print("Private species text merged: six validated banks; original entries and all unselected banks preserved.")
