"""Compile actual rule modules against small game API doubles; no ROM required."""
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
    raise SystemExit("Set CC to a C compiler to run the portable rule tests")

game_api = """#ifndef RULE_TEST_GAME_H
#define RULE_TEST_GAME_H
#include <stddef.h>
#include <stdint.h>
typedef struct Pokemon { uint32_t species, level, exp, ot; } Pokemon;
typedef struct TrainerInfo { int badges; } TrainerInfo;
typedef struct VarsFlags { int completed; } VarsFlags;
typedef struct SaveData { TrainerInfo trainer; VarsFlags flags; } SaveData;
typedef struct { int worldLevelCaps; } SIM_Config_prj_type;
enum { MON_DATA_EXPERIENCE, MON_DATA_SPECIES, MON_DATA_LEVEL };
SaveData *SaveData_Ptr(void);
TrainerInfo *SaveData_GetTrainerInfo(SaveData *);
int TrainerInfo_BadgeCount(TrainerInfo *);
VarsFlags *SaveData_GetVarsFlags(SaveData *);
int SystemFlag_CheckGameCompleted(VarsFlags *);
SIM_Config_prj_type *SIM_Config_prj_GetConfig(void);
uint32_t Pokemon_GetValue(Pokemon *, int, void *);
uint32_t Pokemon_GetSpeciesBaseExpAt(int, int);
#endif
"""

scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-rules-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "rule_test_game.h").write_text(game_api)
    for name in ["nitro.h", "port/sim_config_prj.h", "pokemon.h", "save_player.h",
                 "savedata.h", "system_flags.h", "trainer_info.h", "vars_flags.h"]:
        stub = temp / name
        stub.parent.mkdir(exist_ok=True)
        stub.write_text('#include "rule_test_game.h"\n')
    executable = temp / ("rules.exe" if os.name == "nt" else "rules")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(temp), "-I" + str(root / "include"),
                    str(root / "src/port/community_rules.c"),
                    str(root / "src/port/community_rules_game.c"),
                    str(root / "tests/community_rules_test.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

    # Exercise real project config and its existing INI parser separately.
    config_stubs = temp / "config"
    (config_stubs / "nitro").mkdir(parents=True)
    (config_stubs / "nitro/types.h").write_text(
        "#ifndef TEST_NITRO_TYPES_H\n#define TEST_NITRO_TYPES_H\n"
        "typedef int BOOL;\n#define TRUE 1\n#define FALSE 0\n#endif\n")
    (config_stubs / "nitro.h").write_text(
        '#include <stdio.h>\n#include <string.h>\n#include "nitro/types.h"\n')
    config_executable = temp / ("config.exe" if os.name == "nt" else "config-test")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(config_stubs), "-I" + str(root / "include"),
                    "-I" + str(root / "subprojects/libntr/include"),
                    str(root / "src/port/sim_config_prj.c"),
                    str(root / "subprojects/libntr/libraries/sim/src/config/ini.c"),
                    str(root / "tests/community_config_test.c"),
                    "-o", str(config_executable)], check=True)
    subprocess.run([str(config_executable)], cwd=temp, check=True)
