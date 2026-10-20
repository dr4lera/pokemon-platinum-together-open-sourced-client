"""Merge only reviewed modern assets into fresh PRIVATE source-runtime archives.

No assets are distributed by this script. Inputs are read-only; all archive and
member hashes are fixed to the inspected version. Output must be outside this
checkout, the pristine source runtime and the installed reference directory.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct

SOURCE_ROOT = Path(__file__).resolve().parents[2]
MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
MAX_MEMBERS = 2048


class AssetError(ValueError):
    pass


@dataclass(frozen=True)
class Narc:
    members: tuple[bytes, ...]
    names: bytes


@dataclass(frozen=True)
class MemberPin:
    index: int
    size: int
    sha256: str


@dataclass(frozen=True)
class ArchivePin:
    path: str
    base_count: int
    base_sha256: str
    reference_count: int
    reference_sha256: str
    replace: tuple[MemberPin, ...] = ()
    append: tuple[MemberPin, ...] = ()


ASSETS = (
    ArchivePin("wazaeffect/we.arc", 501,
        "386307a1a8f51fb5330d63001e45a913fda32ec0e7bad4edefa7790828062e9a", 850,
        "3bc77b0727486886e9822ba9e527858c3f68b3f691088f472f7202f40910c57d", replace=(
            MemberPin(468, 608, "7b2dacea6cbe9ce00ba17e85e539992f1d07d915a3d64f8712d3c42813ec359a"),
            MemberPin(469, 612, "c67dcda9dcca6a4098aa2219cc51da82182afad9038fb0cee2fc129778c5b940"),
            MemberPin(470, 552, "46fd4ece09df5441ad94100ce01aaf1b1eaeba43ec399af900f012a50a3dd195"),
            MemberPin(471, 520, "3e67ef2a3965a7cdf4594152d6f3242b67342fc64b99768108a5d3a3fd936135"),
            MemberPin(472, 776, "0512b565b85cfeaaa1de33e1b63ea7de8d236880105bfb25f44763fffa27e43a"),
            MemberPin(473, 852, "003efe4775cda34c5e68793abed3d937cf5df30e05133673ab0baf4f44cd29e5"))),
    ArchivePin("wazaeffect/effectdata/waza_particle.narc", 485,
        "07ac57247a417624360463e0c96024f7a98f3e3d4c94eac14e8c287e4b91ad1e", 494,
        "7511fddf9ad5678dfe041fa1903b7758ac682a30d9f36f08a38db62d735ffaa3", append=(
            MemberPin(485, 54360, "9ffd64974eda2841bb10d6807252df744282a537dd2f69ee38d94d2c4a100351"),)),
    ArchivePin("battle/graphic/pl_batt_obj.narc", 343,
        "0508d8818866b770ac4ec086316b31bb12b0617b6bdb727bd7862f985967db3a", 344,
        "aa0632165c944128975e2e9adf682f23f5830133fca1c325745f47ffa336537e", append=(
            MemberPin(343, 212, "7871bc7ac6e0f933c1bfef985036167f85c5a449a4f79710e5a99c4aedc7b803"),)),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def parse_narc(data):
    if not 52 <= len(data) <= MAX_ARCHIVE_BYTES:
        raise AssetError("NARC length is outside supported bounds")
    magic, bom, version, size, header_size, blocks = struct.unpack_from("<4sHHIHH", data)
    if (magic, bom, version, size, header_size, blocks) != (b"NARC", 0xfffe, 0x100, len(data), 16, 3):
        raise AssetError("Unsupported NARC header")
    offset = 16
    sections = []
    for expected in (b"BTAF", b"BTNF", b"GMIF"):
        if offset + 8 > len(data):
            raise AssetError("Truncated NARC block header")
        tag, length = struct.unpack_from("<4sI", data, offset)
        if tag != expected or length < 8 or length > len(data) - offset or length % 4:
            raise AssetError("Invalid NARC block order or length")
        sections.append(data[offset:offset + length])
        offset += length
    if offset != len(data):
        raise AssetError("Trailing NARC data")
    fat, names, image = sections
    if len(fat) < 12:
        raise AssetError("Truncated NARC member table")
    count, reserved = struct.unpack_from("<HH", fat, 8)
    if not 1 <= count <= MAX_MEMBERS or reserved or len(fat) != 12 + count * 8:
        raise AssetError("Invalid NARC member count/table")
    # The reviewed archives use the standard nameless root directory.
    if names != struct.pack("<4sIIHH", b"BTNF", 16, 4, 0, 1):
        raise AssetError("Unsupported named NARC directory")
    payload = image[8:]
    members = []
    seen = set()
    previous_end = 0
    for i in range(count):
        start, end = struct.unpack_from("<II", fat, 12 + i * 8)
        if (start, end) in seen:
            raise AssetError("Duplicate NARC member bounds")
        if start != ((previous_end + 3) & ~3) or not start < end <= len(payload):
            raise AssetError("Overlapping, reordered or out-of-range NARC member")
        if any(byte not in (0, 255) for byte in payload[previous_end:start]):
            raise AssetError("Unexpected NARC alignment payload")
        members.append(payload[start:end])
        seen.add((start, end))
        previous_end = end
    if len(payload) != ((previous_end + 3) & ~3) or any(byte not in (0, 255) for byte in payload[previous_end:]):
        raise AssetError("Unexpected NARC image tail")
    return Narc(tuple(members), names)


def encode_narc(archive):
    if not 1 <= len(archive.members) <= MAX_MEMBERS:
        raise AssetError("Invalid output NARC member count")
    payload, ranges = bytearray(), bytearray()
    for member in archive.members:
        if not member:
            raise AssetError("Empty NARC member")
        start = len(payload)
        payload += member
        ranges += struct.pack("<II", start, len(payload))
        payload += b"\xff" * (-len(payload) % 4)
    fat = struct.pack("<4sIHH", b"BTAF", 12 + len(ranges), len(archive.members), 0) + ranges
    image = struct.pack("<4sI", b"GMIF", 8 + len(payload)) + payload
    size = 16 + len(fat) + len(archive.names) + len(image)
    result = struct.pack("<4sHHIHH", b"NARC", 0xfffe, 0x100, size, 16, 3) + fat + archive.names + image
    if parse_narc(result) != archive:
        raise AssetError("Output archive failed round-trip verification")
    return result


def merge_members(base, reference, replacements, appends):
    result = list(base.members)
    indices = [pin.index for pin in (*replacements, *appends)]
    if len(indices) != len(set(indices)):
        raise AssetError("Duplicate selected member")
    for pin in replacements:
        if not 0 <= pin.index < len(base.members):
            raise AssetError("Replacement index outside source archive")
    for pin in (*replacements, *appends):
        if not 0 <= pin.index < len(reference.members):
            raise AssetError("Selected reference member missing")
        member = reference.members[pin.index]
        if len(member) != pin.size or digest(member) != pin.sha256:
            raise AssetError("Selected member hash/length mismatch")
    for pin in replacements:
        result[pin.index] = reference.members[pin.index]
    for pin in appends:
        if pin.index != len(result):
            raise AssetError("Append must retain the exact next archive index")
        result.append(reference.members[pin.index])
    for i, member in enumerate(base.members):
        if i not in indices and result[i] != member:
            raise AssetError("Unselected source member changed")
    return Narc(tuple(result), base.names)


def validate_particle(data):
    if len(data) < 32:
        raise AssetError("Truncated particle header")
    magic, version, resources, textures, zero, resource_size, texture_size, texture_offset, zero2 = struct.unpack_from("<IIHHIIIII", data)
    if (magic, version, resources, textures, zero, zero2) != (0x53504120, 0x315f3231, 14, 25, 0, 0):
        raise AssetError("Unsupported particle bundle header")
    if texture_offset != 32 + resource_size or texture_offset + texture_size != len(data):
        raise AssetError("Particle section bounds mismatch")
    offset = 32
    for _ in range(resources):
        if offset + 88 > texture_offset:
            raise AssetError("Truncated particle resource")
        flags = struct.unpack_from("<I", data, offset)[0]
        if (flags & 15) > 9 or data[offset + 71] >= textures:
            raise AssetError("Unsupported particle emission/texture index")
        offset += 88
        for bit, size in ((8,12), (9,12), (10,8), (11,12), (16,20), (24,8), (25,8), (26,16), (27,4), (28,8), (29,16)):
            if flags & (1 << bit):
                if offset + size > texture_offset:
                    raise AssetError("Truncated particle optional section")
                if bit == 11 and any(index >= textures for index in data[offset:offset + 8]):
                    raise AssetError("Particle texture-animation index outside bundle")
                offset += size
    if offset != texture_offset:
        raise AssetError("Particle resource layout does not match source ABI")
    for _ in range(textures):
        if offset + 32 > len(data):
            raise AssetError("Truncated particle texture")
        _, param, tex_size, pal_offset, pal_size, _, _, size = struct.unpack_from("<8I", data, offset)
        if (param & 15) != 6 or param & (1 << 17) or ((param >> 4) & 15) > 3 or ((param >> 8) & 15) > 3:
            raise AssetError("Unsupported particle texture format/dimensions/sharing")
        if (size < 32 or size > len(data) - offset or 32 + tex_size > size
                or pal_offset < 32 + tex_size or pal_offset + pal_size > size):
            raise AssetError("Particle texture bounds invalid")
        offset += size
    if offset != len(data):
        raise AssetError("Particle texture section tail")


def validate_icon(data):
    if len(data) < 4 or data[0] != 0x10 or int.from_bytes(data[1:4], "little") != 304:
        raise AssetError("Unsupported Fairy icon compression/header")
    output, offset = bytearray(), 4
    while len(output) < 304:
        if offset >= len(data):
            raise AssetError("Truncated Fairy icon compression flags")
        flags = data[offset]
        offset += 1
        for bit in range(7, -1, -1):
            if len(output) == 304:
                break
            if flags & (1 << bit):
                if offset + 2 > len(data):
                    raise AssetError("Truncated Fairy icon backreference")
                first, second = data[offset:offset + 2]
                offset += 2
                count, distance = (first >> 4) + 3, ((first & 15) << 8 | second) + 1
                if distance > len(output) or len(output) + count > 304:
                    raise AssetError("Fairy icon backreference outside output")
                for _ in range(count):
                    output.append(output[-distance])
            else:
                if offset >= len(data):
                    raise AssetError("Truncated Fairy icon literal")
                output.append(data[offset])
                offset += 1
    if len(data) - offset > 3 or any(data[offset:]):
        raise AssetError("Unexpected Fairy icon compression tail")
    if struct.unpack_from("<4sHHIHH", output) != (b"RGCN", 0xfeff, 0x101, 304, 16, 1):
        raise AssetError("Unsupported Fairy icon NCGR header")
    if output[16:20] != b"RAHC" or struct.unpack_from("<I", output, 20)[0] != 288:
        raise AssetError("Unsupported Fairy icon character section")


def read_archive(root, relative, count, sha256):
    path = (root / relative).resolve()
    if not path.is_relative_to(root) or not path.is_file() or not 52 <= path.stat().st_size <= MAX_ARCHIVE_BYTES:
        raise AssetError("Required archive is missing or outside its input directory")
    with path.open("rb") as stream:
        data = stream.read(MAX_ARCHIVE_BYTES + 1)
    if digest(data) != sha256:
        raise AssetError("Whole archive hash mismatch: " + relative)
    archive = parse_narc(data)
    if len(archive.members) != count:
        raise AssetError("Archive member count mismatch: " + relative)
    return archive


def prepare(output, base_runtime, reference_root):
    if Path(output).is_symlink():
        raise AssetError("Refusing an output symlink")
    output, base_runtime, reference_root = [Path(path).resolve() for path in (output, base_runtime, reference_root)]
    if any(output == root or output.is_relative_to(root) for root in (SOURCE_ROOT, base_runtime, reference_root)):
        raise AssetError("Private output must be outside checkout and both input directories")
    if output.exists() or output.is_symlink():
        raise AssetError("Refusing an existing output directory")
    if any(root == SOURCE_ROOT or root.is_relative_to(SOURCE_ROOT) for root in (base_runtime, reference_root)):
        raise AssetError("Inputs must be separate private runtime/reference directories")
    files, records = [], []
    for pin in ASSETS:
        base = read_archive(base_runtime, pin.path, pin.base_count, pin.base_sha256)
        reference = read_archive(reference_root, "patch/" + pin.path, pin.reference_count, pin.reference_sha256)
        merged = merge_members(base, reference, pin.replace, pin.append)
        if "particle" in pin.path:
            validate_particle(merged.members[485])
        elif "pl_batt_obj" in pin.path:
            validate_icon(merged.members[343])
        data = encode_narc(merged)
        files.append((pin.path, data))
        records.append({"path": pin.path, "size": len(data), "sha256": digest(data),
            "source_sha256": pin.base_sha256, "reference_sha256": pin.reference_sha256,
            "members": len(merged.members), "replaced": [p.index for p in pin.replace],
            "appended": [p.index for p in pin.append],
            "selected_members": [{"index": p.index, "size": p.size, "sha256": p.sha256} for p in (*pin.replace, *pin.append)]})
    manifest = {"format": "community-modern-assets-v1", "private": True,
        "purpose": "isolated source-client testing; never a release",
        "required_engine_features": ["fairy-type-18", "linear-emitter-mode-2"],
        "unselected_source_members_preserved": True, "runtime_verified": False,
        "files": records}
    # Nothing is created until every input, member and generated archive passes.
    # Exclusive directory/file creation also prevents replacing earlier output.
    output.mkdir(parents=True, exist_ok=False)
    for relative, data in files:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(data)
    with (output / "PRIVATE-MODERN-ASSETS.json").open("x", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--base-runtime", required=True, type=Path)
    parser.add_argument("--reference-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        prepare(args.output, args.base_runtime, args.reference_root)
    except (AssetError, OSError) as error:
        raise SystemExit(str(error)) from None
    print("Prepared three PRIVATE merged archives and their manifest; input files unchanged.")
    print("No executable, save, account, complete patched archive or gameplay verification is included.")


if __name__ == "__main__":
    main()
