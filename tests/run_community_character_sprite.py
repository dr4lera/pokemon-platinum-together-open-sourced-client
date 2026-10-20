#!/usr/bin/env python3
"""Exercise the actual tile-copy/ownership code with synthetic archive storage."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "C:/msys64/ucrt64/bin/gcc.exe" if os.name == "nt" else "cc"))
parser.add_argument("--source", type=Path, default=root / "src/character_sprite.c")
parser.add_argument("--expect-failure", action="store_true")
args = parser.parse_args()
scratch_parent = root / ".build-tools"
scratch_parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-character-", dir=scratch_parent) as temporary:
    scratch = Path(temporary)
    (scratch / "nitro").mkdir()
    (scratch / "generated").mkdir()
    species = (root / "generated/species.txt").read_text().splitlines()
    (scratch / "generated/species.h").write_text("enum Species {\n" + ",\n".join(species) + "\n};\n", encoding="utf-8")
    (scratch / "nitro/types.h").write_text("""#ifndef FIXTURE_TYPES_H
#define FIXTURE_TYPES_H
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#endif
""", encoding="utf-8")
    (scratch / "nitro.h").write_text("""#ifndef FIXTURE_NITRO_H
#define FIXTURE_NITRO_H
#include <nitro/types.h>
typedef int GXTexFmt; typedef int GXOBJVRamModeChar;
#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))
#define GF_ASSERT assert
#endif
""", encoding="utf-8")
    executable = scratch / ("character.exe" if os.name == "nt" else "character")
    subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-sign-compare", "-Wno-discarded-qualifiers", "-Wno-multichar", "-DCHARACTER_SPRITE_SOURCE=\"" + args.source.resolve().as_posix() + "\"", "-I" + str(scratch), "-I" + str(root / "include"), "-I" + str(root / "subprojects/libntrsystem/include"), str(root / "tests/community_character_sprite_test.c"), "-o", str(executable)], check=True)
    result = subprocess.run([str(executable)])
    if args.expect_failure:
        if result.returncode == 0:
            raise SystemExit("Expected the old ownership path to fail")
        print("Old source failed the live-owner assertion as expected.")
    else:
        result.check_returncode()
