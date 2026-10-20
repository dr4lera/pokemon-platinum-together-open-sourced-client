"""Disposable own-client Union Room tests; no profiles, saves, shared build or game."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CXX") or shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
cc = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="union-presentation-",dir=scratch) as directory:
    temp=Path(directory)
    exe=temp / "binding.exe"
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-I"+str(root/"include"),
                    str(root/"tests/community_union_binding_test.cpp"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    commands=root/"build_win64/compile_commands.json"
    if not commands.exists():
        raise SystemExit("Configure the native build first for actual SDK/header compilation")
    entries=json.loads(commands.read_text())
    native=next(x for x in entries if x["file"].endswith("/unk_0205C22C.c"))
    cpp=next(x for x in entries if x["file"].endswith("/gui_community_online.cpp"))
    def flags(entry):
        args=shlex.split(entry["command"])[1:];result=[];i=0
        while i<len(args):
            arg=args[i]
            if arg in ("-o","-MQ","-MF"):
                i+=2;continue
            if arg in ("-c","-MD","-fpch-preprocess") or arg==entry["file"]:
                i+=1;continue
            if arg=="-include":
                # Use saved source header, never the shared build's generated PCH.
                result.extend([arg,str(root/"include/pch/global_pch.h")]);i+=2;continue
            result.append(arg);i+=1
        return result
    for filename,entry,tool in [("src/port/community_union_labels.c",native,cc),
            ("src/unk_0205C22C.c",native,cc),("src/overlay005/fieldmap.c",native,cc),
            ("src/port/community_union_presentation.cpp",cpp,compiler)]:
        subprocess.run([tool,*flags(entry),"-fsyntax-only",str(root/filename)],cwd=entry["directory"],check=True)
    # Actual renderer/controller is linked against its real SDK and engine types.
    renderer=temp/"labels.exe"
    subprocess.run([cc,*flags(native),"-DSDL_MAIN_HANDLED","-ffunction-sections","-fdata-sections",
                    str(root/"tests/community_union_labels_test.c"),"-Wl,--gc-sections","-o",str(renderer)],
                   cwd=native["directory"],check=True)
    subprocess.run([str(renderer)],check=True)
    room_source=(root/"src/unk_0205C22C.c").read_text()
    start=room_source.index("static int sub_0205C340(UnkStruct_0205C22C *param0, int param1, WMBssDesc *param2, PalPad *param3)\n{")
    end=room_source.index("static void sub_0205C51C(",start)
    (temp/"union_room_under_test.inc").write_text(room_source[start:end])
    room=temp/"room.exe"
    subprocess.run([cc,*flags(native),"-DSDL_MAIN_HANDLED","-I"+str(temp),"-ffunction-sections","-fdata-sections",
                    str(root/"tests/community_union_room_test.c"),"-Wl,--gc-sections","-o",str(room)],
                   cwd=native["directory"],check=True)
    subprocess.run([str(room)],check=True)
print("Union Room isolated tests and actual native SDK header compilation PASS")
