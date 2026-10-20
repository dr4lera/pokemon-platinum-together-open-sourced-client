#!/usr/bin/env python3
"""Build the actual desktop palette loader with real SDK format headers.

Only platform scalar/GX declarations and unused SDK aggregate includes are
isolated. Fixtures are original synthetic bytes; no game files are consumed.
The guarded allocation catches reads beyond the72-byte palette input. Unsized
API allocation extent cannot be inferred and is intentionally not claimed safe.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "C:/msys64/ucrt64/bin/gcc.exe" if os.name == "nt" else "cc"))
args = parser.parse_args()
scratch_parent = (root / ".build-tools").resolve()
scratch_parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-palette-", dir=scratch_parent) as temporary:
    scratch = Path(temporary)
    assert scratch.resolve().is_relative_to(scratch_parent)
    (scratch / "nitro").mkdir()
    (scratch / "nitro/types.h").write_text("""#ifndef FIXTURE_NITRO_TYPES_H
#define FIXTURE_NITRO_TYPES_H
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#endif
""", encoding="utf-8")
    (scratch / "nitro.h").write_text("""#ifndef NITRO_H_
#define NITRO_H_
#include <nitro/types.h>
typedef enum {GX_TEXFMT_NONE=0,GX_TEXFMT_A3I5=1,GX_TEXFMT_PLTT4=2,GX_TEXFMT_PLTT16=3,GX_TEXFMT_PLTT256=4} GXTexFmt;
typedef int GXOBJVRamModeChar;
#define OS_Printf printf
#endif
""", encoding="utf-8")
    executable = scratch / ("palette.exe" if os.name == "nt" else "palette")
    subprocess.run([args.cc, "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-multichar", "-I" + str(scratch), "-I" + str(root / "subprojects/libntrsystem/include"), "-I" + str(root / "subprojects/libntrsystem/libraries/g2d/include"), str(root / "tests/community_palette_test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
