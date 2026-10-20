"""Append reviewed species graphics to fresh PRIVATE source-runtime archives.

No reference artwork is distributed. Both inputs are read-only, output is new
and outside the checkout and both input trees. This does not register species,
change saves or establish complete modern gameplay/cry compatibility.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct

SOURCE_ROOT = Path(__file__).resolve().parents[2]
MAX_ARCHIVE_BYTES = 32 * 1024 * 1024
MAX_MEMBERS = 4096


class AssetError(ValueError):
    pass


@dataclass(frozen=True)
class Narc:
    members: tuple[bytes, ...]
    names: bytes


@dataclass(frozen=True)
class ArchivePin:
    path: str
    base_count: int
    base_sha256: str
    reference_count: int
    reference_sha256: str


ASSETS = (
    ArchivePin("poketool/pokegra/pl_pokegra.narc", 2964,
        "d99ed134b21b03ae7eef8a1c8aff3072fa50ead053179bb747c008e1cfb7ed9f", 3486,
        "ea485c3250039de59642a7a33f8d96582ecf99fcd5bd600fc5b040e21f530527"),
    ArchivePin("poketool/icongra/pl_poke_icon.narc", 547,
        "faf07645f4989ccee23e20a8052368b61ade363ce018bc36087e33a80424b760", 612,
        "28337894ec8710c270afca549dbdda2ef63a6856aaf9605d85c960c85240f04d"),
)
ANIMATION_PATH = "pokeanime/pl_poke_anm.narc"
ANIMATION_COUNT = 143
ANIMATION_SHA256 = "2299c76f657dc870b240264e3bedf25e931ee3adc767a0c9a28b4ff20409a869"
REQUIRED_ANIMATIONS = (1, 2, 5, 6, 15, 17, 84)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_resource(root, relative):
    path = (root / relative).resolve()
    if root not in path.parents or not path.is_file() or path.stat().st_size > MAX_ARCHIVE_BYTES:
        raise AssetError("Resource is missing, outside its input tree or oversized")
    with path.open("rb") as stream:
        data = stream.read(MAX_ARCHIVE_BYTES + 1)
    if len(data) > MAX_ARCHIVE_BYTES:
        raise AssetError("Oversized resource")
    return data


def parse_narc(data):
    if not 52 <= len(data) <= MAX_ARCHIVE_BYTES:
        raise AssetError("NARC length outside supported bounds")
    if struct.unpack_from("<4sHHIHH", data) != (b"NARC", 0xfffe, 0x100, len(data), 16, 3):
        raise AssetError("Unsupported NARC header")
    offset = 16
    sections = []
    for expected in (b"BTAF", b"BTNF", b"GMIF"):
        if offset + 8 > len(data):
            raise AssetError("Truncated NARC block header")
        tag, length = struct.unpack_from("<4sI", data, offset)
        if tag != expected or length < 8 or length > len(data) - offset or length % 4:
            raise AssetError("Invalid NARC block bounds/order")
        sections.append(data[offset:offset + length])
        offset += length
    if offset != len(data):
        raise AssetError("Trailing NARC data")
    fat, names, image = sections
    if len(fat) < 12:
        raise AssetError("Truncated NARC table")
    count, reserved = struct.unpack_from("<HH", fat, 8)
    if not 1 <= count <= MAX_MEMBERS or reserved or len(fat) != 12 + count * 8:
        raise AssetError("Invalid NARC member count/table")
    if names != struct.pack("<4sIIHH", b"BTNF", 16, 4, 0, 1):
        raise AssetError("Unsupported named NARC directory")
    payload = image[8:]
    members = []
    previous_end = 0
    for i in range(count):
        start, end = struct.unpack_from("<II", fat, 12 + i * 8)
        # Empty original members and the 108 deliberate reserved extension
        # members are legal. Nonempty overlap/alias/reordering is not.
        if start != (previous_end + 3) & ~3 or not start <= end <= len(payload):
            raise AssetError("Overlapping, reordered or out-of-range NARC member")
        if any(byte not in (0, 255) for byte in payload[previous_end:start]):
            raise AssetError("Unexpected NARC alignment bytes")
        members.append(payload[start:end])
        previous_end = end
    if len(payload) != (previous_end + 3) & ~3 or any(byte not in (0, 255) for byte in payload[previous_end:]):
        raise AssetError("Unexpected NARC payload tail")
    return Narc(tuple(members), names)


def encode_narc(archive):
    if not 1 <= len(archive.members) <= MAX_MEMBERS:
        raise AssetError("Invalid output member count")
    payload, ranges = bytearray(), bytearray()
    for member in archive.members:
        start = len(payload)
        payload += member
        ranges += struct.pack("<II", start, len(payload))
        payload += b"\xff" * (-len(payload) % 4)
    fat = struct.pack("<4sIHH", b"BTAF", 12 + len(ranges), len(archive.members), 0) + ranges
    image = struct.pack("<4sI", b"GMIF", 8 + len(payload)) + payload
    result = struct.pack("<4sHHIHH", b"NARC", 0xfffe, 0x100, 16 + len(fat) + len(archive.names) + len(image), 16, 3) + fat + archive.names + image
    if parse_narc(result) != archive:
        raise AssetError("Output archive round-trip failed")
    return result


def validate_character(data, icon=False):
    raw_size = 1024 if icon else 6400
    version = 0x101 if icon else 0x100
    if len(data) != 48 + raw_size:
        raise AssetError("Unexpected character resource size")
    if struct.unpack_from("<4sHHIHH", data) != (b"RGCN", 0xfeff, version, len(data), 16, 1):
        raise AssetError("Unsupported character header")
    if struct.unpack_from("<4sI", data, 16) != (b"RAHC", len(data) - 16):
        raise AssetError("Invalid character block bounds")
    expected = (65535, 65535, 3, 16, 0, raw_size, 24) if icon else (10, 20, 3, 0, 1, raw_size, 24)
    if struct.unpack_from("<HHIIIII", data, 24) != expected:
        raise AssetError("Unsupported character dimensions/format/data bounds")


def validate_palette(data):
    if len(data) != 72:
        raise AssetError("Unexpected palette resource size")
    if struct.unpack_from("<4sHHIHH", data) != (b"RLCN", 0xfeff, 0x100, 72, 16, 1):
        raise AssetError("Unsupported palette header")
    if struct.unpack_from("<4sI", data, 16) != (b"TTLP", 56):
        raise AssetError("Invalid palette block bounds")
    # Preserve the exact existing base-resource format word. Only32 bytes of
    # palette data exist at40; block size56 must not become a copy length.
    if struct.unpack_from("<IIII", data, 24) != (0x000a0004, 0, 32, 16):
        raise AssetError("Unsupported palette serialized layout/data bounds")


def merge(pin, base_data, reference_data):
    if digest(base_data) != pin.base_sha256 or digest(reference_data) != pin.reference_sha256:
        raise AssetError("Archive hash differs from reviewed source/reference version")
    base, reference = parse_narc(base_data), parse_narc(reference_data)
    if len(base.members) != pin.base_count or len(reference.members) != pin.reference_count:
        raise AssetError("Archive member count differs from selected version")
    if base.names != reference.names or reference.members[:pin.base_count] != base.members:
        raise AssetError("Reference changed an original source archive member")
    for index in range(pin.base_count, pin.reference_count):
        member = reference.members[index]
        if pin.path == ASSETS[0].path:
            if index < 512 * 6:
                if member:
                    raise AssetError("Reserved graphics slots must remain empty")
            elif index % 6 < 4:
                validate_character(member)
            else:
                validate_palette(member)
        else:
            validate_character(member, icon=True)
    result = encode_narc(Narc(base.members + reference.members[pin.base_count:], base.names))
    if parse_narc(result).members[:pin.base_count] != base.members:
        raise AssetError("Original source members changed during merge")
    return result


def prepare(output, base_root, reference_root):
    output, base_root, reference_root = output.resolve(), base_root.resolve(), reference_root.resolve()
    for forbidden in (SOURCE_ROOT.resolve(), base_root, reference_root):
        if output == forbidden or forbidden in output.parents:
            raise AssetError("Private output must be outside checkout and both input trees")
    if output.exists():
        raise AssetError("Refusing an existing output directory")
    for root in (base_root, reference_root):
        if not root.is_dir():
            raise AssetError("Input runtime directory is missing")
    animation = read_resource(base_root, ANIMATION_PATH)
    if digest(animation) != ANIMATION_SHA256:
        raise AssetError("Source sprite-animation dependency hash differs")
    scripts = parse_narc(animation).members
    if len(scripts) != ANIMATION_COUNT or any(not scripts[i] or len(scripts[i]) % 4 for i in REQUIRED_ANIMATIONS):
        raise AssetError("Source sprite-animation dependency bounds differ")
    staged, records = [], []
    for pin in ASSETS:
        data = merge(pin, read_resource(base_root, pin.path), read_resource(reference_root, "patch/" + pin.path))
        staged.append((pin.path, data))
        records.append(dict(path=pin.path, bytes=len(data), sha256=digest(data), original_members=pin.base_count,
                            members=pin.reference_count, first_appended=pin.base_count))
    manifest = dict(private=True, runtime_verified=False, schema=1, archives=records,
                    dependency=dict(path=ANIMATION_PATH, members=ANIMATION_COUNT, sha256=ANIMATION_SHA256),
                    required_features=["validated-v4-species-catalog", "bounded-species-graphics-consumers", "reviewed-sdk-port-palette-loader"],
                    exclusions=["species-registration", "modern-dex-save", "modern-cries", "complete-modern-moves-and-abilities"])
    # All hashes, prefixes and resource records are validated before creating
    # any output. Never replace originals or merge the whole patch directory.
    output.mkdir(parents=True)
    for relative, data in staged:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(data)
    with (output / "PRIVATE-SPECIES-ASSETS.json").open("x", encoding="utf8") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--base-runtime", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(args.output, args.base_runtime, args.reference_root)
    except (AssetError, OSError) as error:
        raise SystemExit(str(error)) from error
    print("Reviewed species graphics appended to fresh PRIVATE archives; runtime verification and other modern systems remain pending.")


if __name__ == "__main__":
    main()
