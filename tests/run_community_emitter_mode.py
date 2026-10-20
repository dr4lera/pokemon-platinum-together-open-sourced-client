"""Compile the actual bounded emitter adapter, setters and interpolation logic."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get("COMMUNITY_TEST_BUILD", root / "build_win64"))
compiler = os.environ.get("CC") or shutil.which("gcc")
if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/gcc.exe"
if not compiler:
    raise SystemExit("Set CC to a C compiler")
def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]
source = (root / "src/battle_anim/script_funcs_3.c").read_text()
helpers = (root / "src/battle_anim/battle_anim_helpers.c").read_text()
header = (root / "include/battle_anim/battle_anim_helpers.h").read_text()
emitter = (root / "lib/spl/include/spl_emitter.h").read_text()
pieces = []
for prefix, end_marker in (("enum ValueLerpParam", "\n};"), ("enum XYTransformParam", "\n};"),
                           ("typedef struct ValueLerpContext", "} ValueLerpContext;"),
                           ("typedef struct XYTransformContext", "} XYTransformContext;")):
    start = header.index(prefix)
    pieces.append(header[start:header.index(end_marker, start) + len(end_marker)])
start = source.index("typedef struct EmitterAnimationContext")
pieces.append(source[start:source.index("// Vars for MoveEmitterViewportTop", start)])
for signature in ("fx32 BattleAnimMath_GetStepSize(", "void PosLerpContext_Init(",
                  "BOOL PosLerpContext_Update(", "void ValueLerpContext_Init("):
    pieces.append(function(helpers, signature))
for signature in ("static inline void SPLEmitter_SetPosX(", "static inline void SPLEmitter_SetPosY("):
    pieces.append(function(emitter, signature))
for signature in ("static void EmitterAnimationContext_Init(", "static BOOL IsEmitterActive(",
                  "static void BattleAnimTask_MoveEmitterA2BLinear(", "void BattleAnimScriptFunc_MoveEmitterA2BLinear("):
    pieces.append(function(source, signature))
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="emitter-mode-tests-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "actual_emitter_mode.inc").write_text("\n\n".join(pieces))
    executable = temp / ("emitter.exe" if os.name == "nt" else "emitter")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(temp),
                    "-I" + str(root / "include"), "-I" + str(build), root / "tests/community_emitter_mode_test.c",
                    "-o", executable], check=True)
    subprocess.run([executable], check=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(c for c in commands if c["file"].replace("\\", "/").endswith("src/battle_anim/script_funcs_3.c"))
    args = shlex.split(entry["command"])
    for flag in ("-MQ", "-MF", "-o"):
        index = args.index(flag)
        del args[index:index+2]
    args = [a for a in args if a not in ("-MD", "-fpch-preprocess")]
    if "-include" in args:
        index = args.index("-include")
        assert args[index+1] == "global_pch.h"
        args[index+1] = str(root / "include/pch/global_pch.h")
    subprocess.run([compiler, *args[1:], "-o", temp / "script_funcs_3.o"], cwd=build, check=True)
    print("Complete animation source translation unit compiled independently")
