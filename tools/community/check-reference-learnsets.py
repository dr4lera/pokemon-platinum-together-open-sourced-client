"""Optional offline check against a user-supplied, version-pinned reference.

No reference bytes or generated runtime assets are written into the checkout.
Only the community decoder is compiled and executed; the game never runs.
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

ARTIFACT_SHA256 = "3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9"
TABLE_OFFSET = 0xE14F00
RECORD_BYTES = 260
RECORD_COUNT = 65
TABLE_SHA256 = "2ad800f459a0b51ec713a895f6a942e4d0da4fbfeb7ae6d80d50ed502f36e4a7"


class Entry(ctypes.Structure):
    _fields_ = [("move", ctypes.c_uint16), ("level", ctypes.c_uint8)]


class Learnset(ctypes.Structure):
    _fields_ = [("count", ctypes.c_size_t), ("entries", Entry * 32)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--compiler")
    args = parser.parse_args()
    image = args.reference.read_bytes()
    if hashlib.sha256(image).hexdigest() != ARTIFACT_SHA256:
        raise SystemExit("Reference artifact does not match the selected 0.1.7 version")
    table = image[TABLE_OFFSET:TABLE_OFFSET + RECORD_COUNT * RECORD_BYTES]
    if hashlib.sha256(table).hexdigest() != TABLE_SHA256:
        raise SystemExit("Modern learnset table differs from the pinned reference")

    root = Path(__file__).resolve().parents[2]
    compiler = args.compiler or os.environ.get("CC") or shutil.which("gcc")
    if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").exists():
        compiler = "C:/msys64/ucrt64/bin/gcc.exe"
    if not compiler:
        raise SystemExit("Set CC or --compiler to a native GCC-compatible compiler")
    scratch = root / ".build-tools"
    scratch.mkdir(exist_ok=True)
    counts = []
    largest_move = 0
    with tempfile.TemporaryDirectory(prefix="reference-learnsets-", dir=scratch) as folder:
        library_path = Path(folder) / ("learnsets.dll" if os.name == "nt" else "learnsets.so")
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-shared"]
        if os.name != "nt":
            command.append("-fPIC")
        command += ["-I" + str(root / "include"), str(root / "src/port/community_learnset.c"), "-o", str(library_path)]
        subprocess.run(command, check=True)
        library = ctypes.CDLL(str(library_path))
        decode = library.CommunityLearnset_DecodeModern
        decode.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.POINTER(Learnset)]
        decode.restype = ctypes.c_int
        try:
            for index in range(RECORD_COUNT):
                record = table[index * RECORD_BYTES:(index + 1) * RECORD_BYTES]
                words = struct.unpack("<65I", record)
                count = words.index(0xFFFFFFFF)
                expected = [(word & 0xFFFF, word >> 16) for word in words[:count]]
                data = (ctypes.c_uint8 * RECORD_BYTES).from_buffer_copy(record)
                output = Learnset()
                if decode(data, RECORD_BYTES, ctypes.byref(output)) != 0:
                    raise SystemExit(f"Decoder rejected reference species ID {512 + index}")
                actual = [(entry.move, entry.level) for entry in output.entries[:output.count]]
                if actual != expected:
                    raise SystemExit(f"Record order or content differs for species ID {512 + index}")
                counts.append(count)
                largest_move = max([largest_move] + [move for move, _ in actual])
        finally:
            # Windows keeps loaded DLLs locked; release our own library on errors too.
            import _ctypes
            if os.name == "nt":
                _ctypes.FreeLibrary(library._handle)
            else:
                _ctypes.dlclose(library._handle)
    print(f"Reference learnsets: all {len(counts)} records match; entries {min(counts)}..{max(counts)}, highest move ID {largest_move}.")
    print("Offline decoder verification only; modern gameplay/provider activation remains pending.")


if __name__ == "__main__":
    main()
