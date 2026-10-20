#!/usr/bin/env python3
"""Pure CDX1 corruption/compatibility checks; optional actual native integration."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument("--engine-build",type=Path)
parser.add_argument("--pokedex-source",type=Path,default=root/"src/pokedex.c",help="Optional private proposed-source copy during a shared build barrier")
args=parser.parse_args()
cc=os.environ.get("CC","C:/msys64/ucrt64/bin/gcc.exe" if os.name=="nt" else "cc")
scratch_parent=root/".build-tools"
scratch_parent.mkdir(exist_ok=True)

def function(source,name):
    match=re.search(r"^(?:static )?[\w *]+\b"+name+r"\([^;]*?\)\s*\{",source,re.M)
    if not match:raise RuntimeError("Missing actual public function:"+name)
    end,depth=match.end(),1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[match.start():end]

with tempfile.TemporaryDirectory(prefix="community-modern-dex-",dir=scratch_parent) as temp:
    scratch=Path(temp)
    executable=scratch/"codec.exe"
    subprocess.run([cc,"-std=c11","-Wall","-Wextra","-Werror","-I"+str(root/"include"),str(root/"tests/community_modern_dex_test.c"),str(root/"src/port/community_modern_dex.c"),"-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
    if args.engine_build:
        commands=json.loads((args.engine_build.resolve()/"compile_commands.json").read_text())
        entry=next(item for item in commands if item["file"].endswith("/pokedex.c"))
        compile_args=shlex.split(entry["command"])
        compile_args[0]=cc
        compile_args=compile_args[:compile_args.index("-MD")]
        functions=scratch/"native_functions.c"
        functions.write_text("\n".join(function((root/filename).read_text(),name) for filename,name in (
            ("src/game_records.c","GameRecords_IncrementTrainerScoreOnCatch"),
            ("src/battle/battle_system.c","BattleSystem_HasCaughtSpecies"),
            ("src/savedata.c","SaveData_CalculateChecksum"))))
        executable=scratch/"native.exe"
        defines=[]
        for name,path in (("POKEDEX_SOURCE",args.pokedex_source.resolve()),("POKEDEX_LANGUAGE_SOURCE",root/"src/pokedex_language.c"),("CATCH_RECORDS_SOURCE",root/"src/save_catchrecords.c"),("SDK_CRC_SOURCE",root/"subprojects/libntr/libraries/math/src/crc.c"),("NATIVE_CHECKSUM_SOURCE",functions)):
            defines.append('-D'+name+'="'+path.as_posix()+'"')
        subprocess.run(compile_args+defines+["-ffunction-sections","-fdata-sections","-Wl,--gc-sections",str(root/"tests/community_modern_dex_native_test.c"),str(root/"src/port/community_modern_dex.c"),"-o",str(executable)],cwd=entry["directory"],check=True)
        subprocess.run([str(executable),str(scratch/"fresh-native-page.bin")],check=True)
        print("Actual native Init/Copy/Encounter/Capture/trade catch/marker query and CRC16 page roundtrip PASS; native sizeof804/offsets unchanged")
print("CDX1 pure codec:65 species, all496 bit corruptions, canonical bounds, legacy admission, immutable failure and native low7 preservation PASS")
