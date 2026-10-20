"""Exercise the real catalog parser/provider with synthetic data and optional
version-pinned user reference. Original bytes are never written into this tree.
"""
import argparse
import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = "3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9"


class Entry(ctypes.Structure):
    _fields_ = [("move", ctypes.c_uint16), ("level", ctypes.c_uint8)]


class Learnset(ctypes.Structure):
    _fields_ = [("count", ctypes.c_size_t), ("entries", Entry * 32)]


class Catalog(ctypes.Structure):
    _fields_ = [("valid", ctypes.c_int), ("personal", (ctypes.c_uint8 * 44) * 65),
                ("learnsets", (ctypes.c_uint8 * 260) * 65), ("supplements", (ctypes.c_uint8 * 6) * 32),
                ("types", (ctypes.c_uint8 * 4) * 25), ("moves", (ctypes.c_uint8 * 10) * 70),
                ("metadata", (ctypes.c_uint8 * 6) * 65), ("evolutions", (ctypes.c_uint8 * 42) * 65),
                ("spriteData", (ctypes.c_uint8 * 89) * 65), ("spriteHeights", (ctypes.c_uint8 * 2) * 65),
                ("iconPalettes", ctypes.c_uint8 * 65), ("formGraphics", (ctypes.c_uint8 * 6) * 4), ("fairyMovePalette", ctypes.c_uint8 * 32)]


class Metadata(ctypes.Structure):
    _fields_ = [("nationalDex", ctypes.c_uint16), ("height", ctypes.c_uint16), ("weight", ctypes.c_uint16)]


def wrap(payload):
    return b"PTCAT03\0" + bytes.fromhex(REFERENCE) + struct.pack("<II", len(payload), zlib.crc32(payload)) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--private-workspace", type=Path, help="Optional existing directory outside the source checkout for private temporary data")
    args = parser.parse_args()
    compiler = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
    scratch = ROOT / ".build-tools"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="catalog-tests-", dir=scratch) as folder:
        path = Path(folder) / ("catalog.dll" if os.name == "nt" else "catalog.so")
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-shared"]
        if os.name != "nt":
            command.append("-fPIC")
        subprocess.run(command + ["-I" + str(ROOT / "include"), str(ROOT / "src/port/community_catalog.c"),
                                  str(ROOT / "src/port/community_learnset.c"), "-o", str(path)], check=True)
        library = ctypes.CDLL(str(path))
        decode = library.CommunityCatalog_Decode
        decode.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.POINTER(Catalog)]
        decode.restype = ctypes.c_int
        personal = library.CommunityCatalog_Personal
        personal.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint8)]
        personal.restype = ctypes.c_int
        learnset = library.CommunityCatalog_Learnset
        learnset.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(Learnset)]
        learnset.restype = ctypes.c_int
        supplement = library.CommunityCatalog_Supplement
        supplement.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(Learnset)]
        supplement.restype = ctypes.c_int
        types = library.CommunityCatalog_Types
        types.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint8)]
        types.restype = ctypes.c_int
        move_override = library.CommunityCatalog_Move
        move_override.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint8)]
        move_override.restype = ctypes.c_int
        metadata = library.CommunityCatalog_Metadata
        metadata.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(Metadata)]
        metadata.restype = ctypes.c_int
        evolutions = library.CommunityCatalog_Evolutions
        evolutions.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint8)]
        evolutions.restype = ctypes.c_int
        sprite_data = library.CommunityCatalog_SpriteData
        sprite_data.argtypes = evolutions.argtypes
        sprite_data.restype = ctypes.c_int
        sprite_index = library.CommunityCatalog_SpriteIndex
        sprite_index.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        sprite_index.restype = ctypes.c_int
        sprite_height = library.CommunityCatalog_SpriteHeight
        sprite_height.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint8)]
        sprite_height.restype = ctypes.c_int
        icon = library.CommunityCatalog_Icon
        icon.argtypes = [ctypes.POINTER(Catalog), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint8)]
        icon.restype = ctypes.c_int
        try:
            catalog = Catalog()

            def load(blob):
                data = (ctypes.c_uint8 * len(blob)).from_buffer_copy(blob)
                return decode(data, len(blob), ctypes.byref(catalog))

            records = bytearray(65 * 44)
            lists = bytearray()
            for index in range(65):
                records[index * 44] = index + 1
                lists += struct.pack("<II", ((index + 1) << 16) | (600 + index), 0xFFFFFFFF) + bytes(252)
            supplements = struct.pack("<HHBB", 35, 473, 46, 0) + struct.pack("<HHBB", 35, 469, 1, 0) * 31
            type_records = b"".join(struct.pack("<HBB", index + 1, 18, 18) for index in range(25))
            move_records = struct.pack("<HBxhhh", 1, 18, 90, -1, 15) + b"".join(struct.pack("<HBxhhh", index + 1, 255, -1, -1, -1) for index in range(1, 70))
            old_payload = records + lists + supplements + type_records + move_records
            meta_records = b"".join(struct.pack("<HHH", 494 + i, i + 1, i + 2) for i in range(65))
            evo_records = bytearray(65 * 42)
            evo_records[:6] = struct.pack("<HHH", 64, 39, 513)
            evo_records[42:48] = struct.pack("<HHH", 65, 50, 514)
            sprites = bytes(65 * 89)
            heights = bytes(value for i in range(65) for value in (i, i + 1))
            palettes = bytes(i % 3 for i in range(65))
            forms = b"".join(struct.pack("<HBBBB", 512 + i, i % 2 + 1, 70 + i, 80 + i, 0) for i in range(4))
            payload = old_payload + meta_records + evo_records + sprites + heights + palettes + forms + struct.pack("<16H", *range(16))
            blob = wrap(payload)
            assert len(blob) == 29956 and load(blob) == 1
            for damaged in (blob[:-1], blob + b"\0", bytes([blob[0] ^ 1]) + blob[1:], blob[:8] + bytes(32) + blob[40:], blob[:-1] + b"\1", b"PTCAT01\0" + blob[8:], b"PTCAT02\0" + blob[8:], wrap(old_payload)):
                assert load(damaged) == 0 and catalog.valid == 0
            for offset, value in ((6, 19), (19, 6), (22, 144), (65 * 44 + 8, 1), (19951, 1), (19954, 19), (20084, 19), (len(payload) - 1, 128)):
                damaged = bytearray(payload)
                damaged[offset] = value
                assert load(wrap(damaged)) == 0 and catalog.valid == 0
            meta_at = len(old_payload)
            evo_at = meta_at + len(meta_records)
            sprite_at = evo_at + len(evo_records)
            palette_at = sprite_at + len(sprites) + len(heights)
            form_at = palette_at + len(palettes)
            # Correct CRC does not hide invalid numeric references, duplicate
            # dex/form keys, sprite frame indices or archive palette bounds.
            for offset, replacement in ((meta_at, b"\0\0"), (meta_at + 2, b"\0\0"),
                (meta_at + 6, meta_records[:2]), (evo_at, b"\x42\0"),
                (evo_at + 2, b"\x65\0"), (evo_at + 4, b"\xfe\x01"),
                (sprite_at + 3, b"\x02"), (sprite_at + 88, b"\x04"),
                (palette_at, b"\x03"), (form_at, b"\xff\xff"),
                (form_at + 2, b"\0"), (form_at + 5, b"\x01"),
                (form_at + 6, forms[:3])):
                damaged = bytearray(payload)
                damaged[offset:offset + len(replacement)] = replacement
                assert load(wrap(damaged)) == 0 and catalog.valid == 0
            assert load(blob) == 1
            output = (ctypes.c_uint8 * 44)()
            moves = Learnset()
            for species in (511, 577, 65535):
                assert personal(ctypes.byref(catalog), species, output) == 0
                assert learnset(ctypes.byref(catalog), species, ctypes.byref(moves)) == 1 and moves.count == 0
            for index in range(65):
                assert personal(ctypes.byref(catalog), 512 + index, output) == 1 and output[0] == index + 1
                assert learnset(ctypes.byref(catalog), 512 + index, ctypes.byref(moves)) == 0
                assert moves.count == 1 and moves.entries[0].move == 600 + index and moves.entries[0].level == index + 1
                meta = Metadata()
                assert metadata(ctypes.byref(catalog), 512 + index, ctypes.byref(meta)) == 1
                assert (meta.nationalDex, meta.height, meta.weight) == (494 + index, index + 1, index + 2)
                archive = ctypes.c_uint(0)
                palette = ctypes.c_uint8(0)
                assert icon(ctypes.byref(catalog), 512 + index, ctypes.byref(archive), ctypes.byref(palette)) == 1
                assert (archive.value, palette.value) == (547 + index, index % 3)
                assert sprite_index(ctypes.byref(catalog), 512 + index, 0, ctypes.byref(archive)) == 1 and archive.value == 512 + index
                for face in (0, 1, 2, 3):
                    assert sprite_height(ctypes.byref(catalog), 512 + index, 0, face, ctypes.byref(palette)) == 1
                    assert palette.value == index + (face != 2)
            evo = (ctypes.c_uint8 * 42)()
            sprite = (ctypes.c_uint8 * 89)()
            assert evolutions(ctypes.byref(catalog), 512, evo) == 1 and bytes(evo) == evo_records[:42]
            assert sprite_data(ctypes.byref(catalog), 512, sprite) == 1 and bytes(sprite) == sprites[:89]
            for index in range(4):
                assert sprite_index(ctypes.byref(catalog), 512 + index, index % 2 + 1, ctypes.byref(archive)) == 1 and archive.value == 577 + index
                for face in (0, 2):
                    assert sprite_height(ctypes.byref(catalog), 512 + index, index % 2 + 1, face, ctypes.byref(palette)) == 1
                    assert palette.value == (70 if face == 2 else 80) + index
                assert sprite_index(ctypes.byref(catalog), 512 + index, 255, ctypes.byref(archive)) == 1 and archive.value == 512 + index
            for species in (0, 511, 577, 65535):
                meta = Metadata(1, 2, 3)
                assert metadata(ctypes.byref(catalog), species, ctypes.byref(meta)) == 0 and (meta.nationalDex, meta.height, meta.weight) == (1, 2, 3)
                assert evolutions(ctypes.byref(catalog), species, evo) == 0
                assert sprite_data(ctypes.byref(catalog), species, sprite) == 0
                assert sprite_index(ctypes.byref(catalog), species, 0, ctypes.byref(archive)) == 0
                assert sprite_height(ctypes.byref(catalog), species, 0, 2, ctypes.byref(palette)) == 0
                assert icon(ctypes.byref(catalog), species, ctypes.byref(archive), ctypes.byref(palette)) == 0
            moves.count = 3
            for entry, pair in zip(moves.entries, [(1, 1), (2, 46), (3, 50)]):
                entry.move, entry.level = pair
            assert supplement(ctypes.byref(catalog), 35, ctypes.byref(moves)) == 0
            assert [(e.move, e.level) for e in moves.entries[:moves.count]] == [(1, 1), (469, 1), (2, 46), (473, 46), (3, 50)]
            before = bytes(moves)
            assert supplement(ctypes.byref(catalog), 35, ctypes.byref(moves)) == 0 and bytes(moves) == before
            moves.count = 32
            for index, entry in enumerate(moves.entries):
                entry.move, entry.level = index + 1, 1
            before = bytes(moves)
            assert supplement(ctypes.byref(catalog), 35, ctypes.byref(moves)) == 2 and bytes(moves) == before
            type_pair = (ctypes.c_uint8 * 2)()
            assert types(ctypes.byref(catalog), 1, type_pair) == 1 and bytes(type_pair) == bytes([18, 18])
            assert types(ctypes.byref(catalog), 508, type_pair) == 0
            move_record = (ctypes.c_uint8 * 16)(*range(16))
            assert move_override(ctypes.byref(catalog), 1, move_record) == 1
            assert bytes(move_record) == bytes([0, 1, 2, 90, 18, 5, 15, 7, 8, 9, 10, 11, 12, 13, 14, 15])
            before = bytes(move_record)
            assert move_override(ctypes.byref(catalog), 2, move_record) == 1 and bytes(move_record) == before
            assert move_override(ctypes.byref(catalog), 468, move_record) == 0 and bytes(move_record) == before
            assert metadata(None, 512, ctypes.byref(meta)) == 0
            assert metadata(ctypes.byref(catalog), 512, None) == 0
            assert evolutions(ctypes.byref(catalog), 512, None) == 0
            assert sprite_data(ctypes.byref(catalog), 512, None) == 0
            assert sprite_height(ctypes.byref(catalog), 512, 0, 2, None) == 0
            assert sprite_index(ctypes.byref(catalog), 512, 0, None) == 0
            assert icon(ctypes.byref(catalog), 512, None, ctypes.byref(palette)) == 0
            assert icon(ctypes.byref(catalog), 512, ctypes.byref(archive), None) == 0
            print("Community V3 catalog: strict format/integrity, numeric species/form/sprite bounds and transactional supplements passed.")

            if args.reference:
                # Use system temp: extracted runtime bytes never enter the checkout.
                private_root = args.private_workspace.resolve() if args.private_workspace else None
                if private_root and (private_root == ROOT or ROOT in private_root.parents or not private_root.is_dir()):
                    raise SystemExit("Private test workspace must be an existing directory outside the checkout")
                with tempfile.TemporaryDirectory(prefix="community-private-catalog-", dir=private_root) as private:
                    imported = Path(private) / "catalog.ptc"
                    subprocess.run([os.sys.executable, str(ROOT / "tools/community/import-reference-catalog.py"), str(args.reference), str(imported)], check=True)
                    assert load(imported.read_bytes()) == 1
                    image = args.reference.read_bytes()
                    assert hashlib.sha256(image).hexdigest() == REFERENCE
                    for index in range(65):
                        assert bytes(catalog.personal[index]) == image[0xE10620 + index * 44:0xE10620 + (index + 1) * 44]
                        assert bytes(catalog.learnsets[index]) == image[0xE14F00 + index * 260:0xE14F00 + (index + 1) * 260]
                        assert learnset(ctypes.byref(catalog), 512 + index, ctypes.byref(moves)) == 0
                        native = image[0xE13800 + index * 48:0xE13800 + (index + 1) * 48]
                        expected_meta = native[:2] + native[24:28]
                        assert bytes(catalog.metadata[index]) == expected_meta
                        assert metadata(ctypes.byref(catalog), 512 + index, ctypes.byref(meta)) == 1
                        assert (meta.nationalDex, meta.height, meta.weight) == struct.unpack("<HHH", expected_meta)
                        assert bytes(catalog.evolutions[index]) == image[0xE14440 + index * 42:0xE14440 + (index + 1) * 42]
                        assert bytes(catalog.spriteData[index]) == image[0xE191C0 + index * 89:0xE191C0 + (index + 1) * 89]
                        assert bytes(catalog.spriteHeights[index]) == image[0xE19120 + index * 2:0xE19120 + (index + 1) * 2]
                        assert catalog.iconPalettes[index] == image[0xE1A860 + index]
                        assert evolutions(ctypes.byref(catalog), 512 + index, evo) == 1 and bytes(evo) == bytes(catalog.evolutions[index])
                        assert sprite_data(ctypes.byref(catalog), 512 + index, sprite) == 1 and bytes(sprite) == bytes(catalog.spriteData[index])
                        assert icon(ctypes.byref(catalog), 512 + index, ctypes.byref(archive), ctypes.byref(palette)) == 1
                        assert archive.value == 547 + index and palette.value == catalog.iconPalettes[index]
                    assert bytes(catalog.formGraphics) == image[0xE1A8B0:0xE1A8B0 + 24]
                    assert bytes(catalog.fairyMovePalette) == image[0xDE1B04:0xDE1B24]
                    for i, row in enumerate(catalog.formGraphics):
                        species, form, front, back, unused = struct.unpack("<HBBBB", bytes(row))
                        assert sprite_index(ctypes.byref(catalog), species, form, ctypes.byref(archive)) == 1 and archive.value == 577 + i
                        for face, expected in ((2, front), (0, back)):
                            assert sprite_height(ctypes.byref(catalog), species, form, face, ctypes.byref(palette)) == 1 and palette.value == expected
                    assert bytes(catalog.supplements) == image[0xE103E0:0xE103E0 + 192]
                    assert bytes(catalog.types) == image[0xE100A0:0xE100A0 + 100]
                    assert bytes(catalog.moves) == image[0xE10120:0xE10120 + 700]
                    for override in catalog.types:
                        species, first, second = struct.unpack("<HBB", bytes(override))
                        assert types(ctypes.byref(catalog), species, type_pair) == 1 and bytes(type_pair) == bytes([first, second])
                    for override in catalog.moves:
                        move, type_value, power, accuracy, pp = struct.unpack("<HBxhhh", bytes(override))
                        move_record = (ctypes.c_uint8 * 16)(*range(16))
                        assert move_override(ctypes.byref(catalog), move, move_record) == 1
                        expected = list(range(16))
                        for index, value in ((4, type_value if type_value != 255 else -1), (3, power), (5, accuracy), (6, pp)):
                            if value >= 0:
                                expected[index] = value
                        assert bytes(move_record) == bytes(expected)
                    moves.count = 1
                    moves.entries[0].move, moves.entries[0].level = 1, 1
                    assert supplement(ctypes.byref(catalog), 35, ctypes.byref(moves)) == 0
                    assert [(e.move, e.level) for e in moves.entries[:moves.count]] == [(1, 1), (469, 1), (473, 46)]
                    print("Pinned reference: all V3 numeric tables and move palette and four graphics forms match; personal/learnsets and 32/25/70 legacy overrides retained.")
        finally:
            import _ctypes
            if os.name == "nt":
                _ctypes.FreeLibrary(library._handle)
            else:
                _ctypes.dlclose(library._handle)


if __name__ == "__main__":
    main()
