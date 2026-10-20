"""Exercise the actual MoveTable loaders and actual immutable content gate."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("gcc")
if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/gcc.exe"
if not compiler:
    raise SystemExit("Set CC to a C11 compiler")
env = os.environ.copy()
env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="move-table-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "generated").mkdir()
    (temp / "constants").mkdir()
    (temp / "overlay011").mkdir()
    (temp / "nitro.h").write_text("#pragma once\n#include <stdint.h>\ntypedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t s8;\n#define ALIGN_4\n")
    (temp / "constants/graphics.h").write_text("#define RGB(r,g,b) ((r)|((g)<<5)|((b)<<10))\n#define PLTT_DEST(n) ((n)*16)\n#define PALETTE_SIZE_BYTES 32\n")
    (temp / "overlay011/move_palettes.h").write_text('#pragma once\n#include "generated/pokemon_types.h"\ntypedef struct PaletteData {int unused;} PaletteData;\nenum PaletteBufferID {PLTTBUF_SUB_BG};\nconst u16 *GetMoveTypePalette(int);\nvoid LoadMoveSelectPltt(PaletteData*,enum PokemonType,int,enum PaletteBufferID,int);\nvoid PaletteData_LoadBuffer(PaletteData*,const void*,enum PaletteBufferID,int,unsigned);\n')
    (temp / "narc.h").write_text("#define NARC_INDEX_POKETOOL__WAZA__PL_WAZA_TBL 1\nvoid NARC_ReadFromMemberByIndexPair(void*,int,int,int,unsigned);\nvoid NARC_ReadWholeMemberByIndexPair(void*,int,int);\n")
    for name, enum in (("moves", "FixtureMove"), ("move_attributes", "MoveAttribute")):
        values = (root / "generated" / (name + ".txt")).read_text().splitlines()
        (temp / "generated" / (name + ".h")).write_text("#pragma once\nenum " + enum + " {\n" + ",\n".join(values) + "\n};\n")
    (temp / "constants/moves.h").write_text('#include "generated/moves.h"\n')
    values=(root / "generated/pokemon_types.txt").read_text().splitlines()
    (temp / "generated/pokemon_types.h").write_text("#pragma once\nenum PokemonType {\n"+",\n".join(values)+"\n};\n")
    flags = [compiler, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror", "-Wno-unused-value", "-I" + str(temp), "-I" + str(root / "include"), "-include", str(temp / "nitro.h")]
    catalog = temp / "catalog.o"
    subprocess.run([*flags, "-DCommunityCatalog_Runtime=CatalogOfflineRuntime", "-c", str(root / "src/port/community_catalog.c"), "-o", str(catalog)], check=True, env=env)
    executable = temp / ("moves.exe" if os.name == "nt" else "moves")
    subprocess.run([*flags, "-DPLATINUM_COMMUNITY_CLIENT=1", str(root / "src/move_table.c"), str(root / "src/overlay011/move_palettes.c"), str(root / "src/port/community_modern_rules.c"), str(root / "src/port/community_learnset.c"), str(root / "tests/community_move_table_test.c"), str(catalog), "-o", str(executable)], check=True, env=env)
    for provider, selection, ability_selection, expected, updates in ((0, "1", "1", 0, 0), (1, "0", "1", 0, 0), (1, "yes", "1", 0, 0), (1, "1", "0", 1, 0), (1, "1", "yes", 1, 0), (1, "1", "1", 1, 1)):
        run_env = {**env, "PLATINUM_COMMUNITY_MODERN_RULES": selection, "PLATINUM_COMMUNITY_0110_ABILITIES": ability_selection}
        subprocess.run([str(executable), str(provider), str(expected), str(updates)], check=True, env=run_env)
