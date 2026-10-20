"""Create a private runtime data cache from a user-supplied pinned reference.

The reference executable is read as bytes, never executed. Cache output must
remain outside the public source checkout and must not be published.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import zlib

REFERENCE_SHA256 = "3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9"
TABLES = [
    (0xE10620, 65 * 44, "4195b15b594f01d8ef0e8258858778c4f6ca2b74bdcf093c8b54c4a5cced0f9b"),
    (0xE14F00, 65 * 260, "2ad800f459a0b51ec713a895f6a942e4d0da4fbfeb7ae6d80d50ed502f36e4a7"),
    (0xE103E0, 32 * 6, "7d64998f81ac4f3ea334f862a8c7238055cd18ccbf0d05b3006afbf70a2f18b2"),
    (0xE100A0, 25 * 4, "2d468b80130e8cbf8a9c41f54f0af912805fc78c72ba376c2dadc33bf1de8fcb"),
    (0xE10120, 70 * 10, "e332f9f9aac1424fab8af5a616532d12b2660551b9fa666f08bf458347b61988"),
]
SPECIES_TABLES = [
    (0xE13800, 65 * 48, "23c8bd474259c800d91bec829c7443ea440ec2eca4447cea7d526ea17e06d918"),
    (0xE14440, 65 * 42, "48a717f6e53574533c9a6ca01cd6a6ffabe3b87eb0cc719ce212551165edd0ee"),
    (0xE191C0, 65 * 89, "582e9ca17c7e72bb51c5c974413029e16083c8e16c7283650ed61299157592f5"),
    (0xE19120, 65 * 2, "72b7cbb70a7008ae6f01f1ddffae0c3782620bb3ecf3caac67e4cae688557e28"),
    (0xE1A860, 65, "29c78830f751aa381b592bc133ec4ddaa4f6e9aa330905af3a4707c8e453ef99"),
    (0xE1A8B0, 4 * 6, "c08dd772533e3e5e7615437e8d190cdb867e2eee05126b3bed9f0b549c7eda89"),
]
FAIRY_MOVE_PALETTE = (0xDE1B04, 32, "ec786db7408190bda79b5d3a97fbedecdc1c27dbc7a4b463642e980b8f18a29b")
STAT_STAGES = (0xE1ABE0, 41 * 12, "6090c97b08f5a1069c483907bf89056fc29f15c22dcf101719505843578fcf62")
SIDE_EFFECTS = (0xDE6F80, 178 * 4, "f08ccd8d3ae66165b408c0216fecbab4eb4835523bf3c565a643cf5e26996b0b")
METRONOME_EXCLUSIONS = (0xE1AB80, 34 * 2, "969b61d30433036bc66c709b2aa0a7663739d54739deae081c4ac182afa6fb09")
FORM_STATS = (0xE1AEA0, 3 * 8, "cccd1a3cd0a3b90a0fb4adc35bf5231e7736548ec4a33a032db2fea1aacc510b")
MOVE_ARCHIVE_SHA256 = "481cca9e8e3861e267dcef73c7cd6d3a02b69e3a3fc1369411ea93393b05a344"


def build_cache(image, move_archive):
    """Hash-pin every input and remove all metadata pointers/string fields."""
    if hashlib.sha256(image).hexdigest() != REFERENCE_SHA256:
        raise ValueError("Reference does not match the pinned 0.1.7 artifact")
    tables = []
    for index, (offset, length, digest) in enumerate(TABLES + SPECIES_TABLES + [FAIRY_MOVE_PALETTE]):
        data = image[offset:offset + length]
        if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError("Reference data table differs from its selected version")
        if index == len(TABLES):
            # Native rows contain process pointers at8/16/32/40. Only these
            # three unsigned numeric fields belong in the portable cache.
            data = b"".join(data[i:i + 2] + data[i + 24:i + 28] for i in range(0, length, 48))
        tables.append(data)
    if hashlib.sha256(move_archive).hexdigest() != MOVE_ARCHIVE_SHA256:
        raise ValueError("Move archive differs from the pinned 0.1.7 artifact")
    spec = importlib.util.spec_from_file_location("community_catalog_assets", Path(__file__).with_name("prepare-modern-assets.py"))
    assets = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = assets
    spec.loader.exec_module(assets)
    archive = assets.parse_narc(move_archive)
    if len(archive.members) != 851 or any(len(record) != 16 for record in archive.members):
        raise ValueError("Move archive record layout mismatch")
    tables.append(b"".join(archive.members[474:848]))
    for offset, length, digest in (STAT_STAGES, SIDE_EFFECTS, METRONOME_EXCLUSIONS, FORM_STATS):
        data = image[offset:offset + length]
        if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError("Modern battle table differs from its selected version")
        if (offset, length, digest) == SIDE_EFFECTS:
            data = data[145 * 4:]
        tables.append(data)
    payload = b"".join(tables)
    assert len(payload) == 36608
    return b"PTCAT05\0" + bytes.fromhex(REFERENCE_SHA256) + struct.pack("<II", len(payload), zlib.crc32(payload)) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--move-archive", required=True, type=Path, help="Explicit pinned private patch/poketool/waza/pl_waza_tbl.narc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if output == root or root in output.parents:
        raise SystemExit("Keep runtime reference data outside the public checkout")
    if output.exists():
        raise SystemExit("Refusing to replace an existing runtime cache")
    try:
        cache = build_cache(args.reference.read_bytes(), args.move_archive.read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from error
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(cache)
    print("Private V5 cache created: species/graphics data, 374 move metadata records and bounded battle tables.")
    print("Set PLATINUM_COMMUNITY_CATALOG to this file for source-client content development.")
    print("This data provider alone does not establish modern battle/rendering or multiplayer parity.")


if __name__ == "__main__":
    main()
