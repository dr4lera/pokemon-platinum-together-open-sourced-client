"""Test actual portable learnset code and Move Reminder without game assets."""
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
    raise SystemExit("Set CC to a C compiler")

game_api = """#ifndef LEARNSET_TEST_GAME_H
#define LEARNSET_TEST_GAME_H
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef int BOOL;
enum HeapID { HEAP_ID_TEST };
enum { MON_DATA_SPECIES, MON_DATA_FORM, MON_DATA_LEVEL, MON_DATA_MOVE1 };
#define LEARNED_MOVES_MAX 4
#define LEVEL_UP_MOVESET_TERMINATOR UINT16_MAX
typedef struct Pokemon { unsigned species, form, level; uint16_t moves[4]; } Pokemon;
typedef struct MoveReminderData { int unused; } MoveReminderData;
void *Heap_Alloc(int heapID, size_t bytes);
void Heap_Free(void *);
uint32_t Pokemon_GetValue(Pokemon *, int field, void *out);
u16 *MoveReminderData_GetMoves(Pokemon *, enum HeapID);
BOOL MoveReminderData_HasMoves(u16 *);
#endif
"""
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="learnset-tests-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "learnset_test_game.h").write_text(game_api)
    for name in ["nitro.h", "move_reminder_data.h", "heap.h", "pokemon.h"]:
        (temp / name).write_text('#include "learnset_test_game.h"\n')
    executable = temp / ("learnsets.exe" if os.name == "nt" else "learnsets")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(temp), "-I" + str(root / "include"),
                    str(root / "src/port/community_learnset.c"),
                    str(root / "src/move_reminder_data.c"),
                    str(root / "tests/community_learnset_test.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
