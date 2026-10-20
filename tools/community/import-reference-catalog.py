"""Create a private runtime data cache from a user-supplied pinned reference.

The reference executable is read as bytes, never executed. Cache output must
remain outside the public source checkout and must not be published.
"""
import argparse
import hashlib
from pathlib import Path
import struct
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


def build_cache(image):
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
    payload = b"".join(tables)
    assert len(payload) == 29908
    return b"PTCAT03\0" + bytes.fromhex(REFERENCE_SHA256) + struct.pack("<II", len(payload), zlib.crc32(payload)) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if output == root or root in output.parents:
        raise SystemExit("Keep runtime reference data outside the public checkout")
    if output.exists():
        raise SystemExit("Refusing to replace an existing runtime cache")
    try:
        cache = build_cache(args.reference.read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from error
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(cache)
    print("Private V3 runtime cache created: 65 numeric species records, 4 graphics forms, learnsets/overrides and the Fairy move-menu palette.")
    print("Set PLATINUM_COMMUNITY_CATALOG to this file for source-client content development.")
    print("This data provider alone does not establish modern battle/rendering or multiplayer parity.")


if __name__ == "__main__":
    main()
