"""Isolated actual engine fragments, assembled drain script and move generator.

No game assets are read. Existing build tools/headers are read-only dependencies;
all generated outputs and compilation stay in a disposable test directory.
"""
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get("COMMUNITY_TEST_BUILD", root / "build_win64"))
compiler = os.environ.get("CC") or shutil.which("gcc")
if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/gcc.exe"
if not compiler:
    raise SystemExit("Set CC to a C compiler")
def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)
def function(source, signature):
    start = source.index(signature)
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]
def array(source, declaration):
    start = source.index(declaration)
    return source[start:source.index("\n};", start) + 3]
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="fairy-moves-tests-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "generated").mkdir()
    tags = {"moves": "Move", "pokemon_types": "PokemonType"}
    for name, tag in tags.items():
        run([sys.executable, root / "subprojects/metang/metang.py", "enum", "--tag-name", tag,
             "--guard", "POKEPLATINUM_GENERATED", "--output", temp / "generated" / (name + ".h"),
             root / "generated" / (name + ".txt")])
    library = (root / "src/battle/battle_lib.c").read_text()
    script = (root / "src/battle/battle_script.c").read_text()
    icons = (root / "src/type_icon.c").read_text()
    pieces = [array(library, "static const u8 sTypeMatchupMultipliers"),
              array(library, "static const u8 sModernTypeMatchupMultipliers"),
              function(library, "static const u8 (*TypeMatchupTable"),
              function(library, "static int TypeMatchupCount"),
              function(library, "BOOL BattleSystem_TypeMatchup("),
              function(library, "int BattleSystem_TypeMatchupMultiplier("),
              array(library, "static u16 sSoundMoves"),
              function(script[script.index("static BOOL BtlCmd_Metronome(BattleSystem *battleSys, BattleContext *battleCtx)\n{"):], "static BOOL BtlCmd_Metronome"),
              array(icons, "ALIGN_4 static const u32 sMoveTypeIconIndex"),
              array(icons, "ALIGN_4 static const u8 sMoveTypeIconPaletteIndex")]
    (temp / "actual_fairy_engine.inc").write_text("\n\n".join(pieces))
    executable = temp / ("fairy.exe" if os.name == "nt" else "fairy")
    run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(temp), "-I" + str(build),
         root / "tests/community_fairy_moves_test.c", "-o", executable])
    run([executable])

    # Compile changed engine translation units against the real SDK/game headers.
    commands = json.loads((build / "compile_commands.json").read_text())
    import shlex
    for name in ("src/battle/battle_lib.c", "src/battle/battle_script.c", "src/type_icon.c",
                 "src/applications/pokemon_summary_screen/sprites.c"):
        entry = next(c for c in commands if c["file"].replace("\\", "/").endswith(name))
        args = shlex.split(entry["command"])
        output = temp / (Path(name).stem + ".o")
        # Remove dependency output options so this cannot touch the shared build.
        for flag in ("-MQ", "-MF", "-o"):
            index = args.index(flag)
            del args[index:index + 2]
        args = [a for a in args if a != "-MD"]
        args = [a for a in args if a != "-fpch-preprocess"]
        if "-include" in args:
            index = args.index("-include")
            assert args[index + 1] == "global_pch.h"
            # Parse the original header freshly; a shared PCH can be stale and
            # would conceal this test's temporary generated enum headers.
            args[index + 1] = str(root / "include/pch/global_pch.h")
        run([compiler, "-I" + str(temp), *args[1:], "-o", output], cwd=build)
    print("Changed engine translation units compiled separately")

    # Assemble the actual drain script with its real command macros. Only data
    # directives are emitted, so the host assembler can compile it without ARM.
    assembly = temp / "drain.o"
    run([compiler, "-x", "assembler-with-cpp", "-I" + str(temp), "-I" + str(root / "asm"),
         "-I" + str(root / "include"), "-I" + str(build), "-c",
         root / "res/battle/scripts/subscripts/subscript_drain_half_damage_dealt.s", "-o", assembly])
    sibling_objcopy = Path(compiler).with_name("objcopy.exe" if os.name == "nt" else "objcopy")
    objcopy = str(sibling_objcopy) if sibling_objcopy.exists() else shutil.which("objcopy")
    if not objcopy:
        raise SystemExit("objcopy is required for the assembled script check")
    run([objcopy, "-O", "binary", "-j", ".text", assembly, temp / "drain.bin"])
    data = (temp / "drain.bin").read_bytes()
    words = struct.unpack("<" + "I" * (len(data) // 4), data)
    commands_text = (root / "include/data/scripts/btlcmd.h").read_text()
    commands_by_id = re.findall(r"ScriptCommand\((BTLCMD_\w+),", commands_text)
    def drain(move, damage):
        variables = {24: move, 32: 0, 54: damage}
        pc = 0
        for _ in range(30):
            command = commands_by_id[words[pc]]
            if command == "BTLCMD_CHECKITEMHOLDEFFECT":
                return variables[32]
            if command == "BTLCMD_UPDATEVARFROMVAR":
                op, target, source = words[pc+1:pc+4]
                assert op == 7
                variables[target] = variables[source]
                pc += 4
            elif command == "BTLCMD_COMPAREVARTOVALUE":
                op, source, value, jump = words[pc+1:pc+5]
                assert op == 0
                pc += 5 + (jump if variables[source] == value else 0)
            elif command == "BTLCMD_DIVIDEVARBYVALUE":
                source, divisor = words[pc+1:pc+3]
                before = variables[source]
                result = abs(before) // divisor * (-1 if before < 0 else 1)
                variables[source] = result or (-1 if before < 0 else 1) if before else 0
                pc += 3
            elif command == "BTLCMD_UPDATEVAR":
                op, source, value = words[pc+1:pc+4]
                assert op == 12
                variables[source] *= value
                pc += 4
            elif command == "BTLCMD_GOTO":
                pc += 2 + words[pc+1]
            else:
                raise AssertionError(command)
        raise AssertionError("Unbounded drain script")
    for damage in (0, -1, -2, -3, -4, -8, -99):
        expected = -(abs(damage) * 3 // 4) if damage else 0
        assert drain(470, damage) == (expected or (-1 if damage else 0))
        assert drain(71, damage) == (-(abs(damage) // 2) or (-1 if damage else 0))
    print("Actual assembled drain script preserves half drain and adds three-quarter drain")

    # Build the actual move generator and common layer in isolation. Reuse only
    # unchanged dependency libraries, never the shared generator executable.
    tool_tags = {"move_battle_effects": "MoveBattleEffect", "move_classes": "MoveClass",
                 "move_contest_effects": "MoveContestEffect", "move_flags": "MoveFlag",
                 "move_ranges": "MoveRange", "pokemon_contest_types": "PokemonContestType"}
    for name, tag in tool_tags.items():
        args = [sys.executable, root / "subprojects/metang/metang.py",
                "mask" if name in ("move_flags", "move_ranges") else "enum", "--tag-name", tag,
                "--guard", "POKEPLATINUM_GENERATED", "--output", temp / "generated" / (name + ".h")]
        if name == "move_ranges":
            args.append("--no-auto")
        run([*args, root / "generated" / (name + ".txt")])
    generator = temp / ("moveproc.exe" if os.name == "nt" else "moveproc")
    include_paths = [temp, root / "include", root / "tools/dataproc/src",
                     root / "tools/dataproc/lib/include", root / "tools/nitroarc/lib/include",
                     root / "tools/enumproc", build]
    libraries = [build / "tools/dataproc/libdataproc.a", build / "tools/nitroarc/libnitroarc.a",
                 build / "tools/enumproc/liblibenum.a", build / "tools/enumproc/liblibexpr.a"]
    if os.name == "nt":
        libraries.append(build / "subprojects/yyjson-0.12.0/libyyjson.dll.a")
    else:
        libraries.append(build / "subprojects/yyjson-0.12.0/libyyjson.so")
    run([compiler, "-std=gnu17", *["-I" + str(p) for p in include_paths],
         '-DREPO_INCLUDE="' + (root / "include").as_posix() + '"',
         '-DREPO_BUILD="' + temp.as_posix() + '"',
         '-DTEMPLATES_DIR="' + (root / "tools/dataproc/data").as_posix() + '"',
         root / "tools/dataproc/src/common.c", root / "tools/dataproc/src/moveproc.c",
         "-Wl,--start-group", *libraries, "-Wl,--end-group", "-o", generator])
    env = os.environ.copy()
    env["PATH"] = str(build / "subprojects/yyjson-0.12.0") + os.pathsep + env.get("PATH", "")
    output = temp / "moves-output"
    output.mkdir()
    run([generator, "-M", output / "moves.d", "-o", output, temp / "generated/moves.h", root / "res/moves"], env=env)
    # The generator accepting JSON does not imply the game font can encode it.
    # Exercise the actual text encoder and character map for every emitted bank.
    msgenc = build / "tools/msgenc" / ("msgenc.exe" if os.name == "nt" else "msgenc")
    charmap = root / "tools/msgenc/charmap.txt"
    for bank in ("move_names", "move_names_uppercase", "move_descriptions", "moves_used_in_battle"):
        source_bank = output / (bank + ".json")
        encoded_bank = output / (bank + ".bin")
        run([msgenc, "-e", "--json", "-c", charmap, source_bank, encoded_bank], env=env)
        messages = json.loads(source_bank.read_text(encoding="utf-8"))["messages"]
        assert struct.unpack_from("<H", encoded_bank.read_bytes())[0] == len(messages)
    invalid_bank = temp / "unsupported_character.json"
    invalid_bank.write_text(json.dumps({"key": 15241, "messages": [{"id": "Unsupported", "en_US": "foe's"}]}))
    rejected = subprocess.run([str(msgenc), "-e", "--json", "-c", str(charmap),
                               str(invalid_bank), str(temp / "unsupported_character.bin")],
                              env=env, capture_output=True, text=True)
    assert rejected.returncode != 0, "Encoder must reject an unsupported ASCII apostrophe"
    print("Actual msgenc encodes all four generated move text banks and rejects an unsupported character")
    archive = (output / "move_data.narc").read_bytes()
    fat_size = struct.unpack_from("<I", archive, 20)[0]
    count = struct.unpack_from("<H", archive, 24)[0]
    assert count == 474, count
    fnt = 16 + fat_size
    image = fnt + struct.unpack_from("<I", archive, fnt + 4)[0] + 8
    expected = [(0,1,40,18,100,30,0,0,0,50,5,1), (17,1,40,18,0,15,0,4,0,50,17,2),
                (3,1,50,18,100,10,0,0,0,19,20,2), (68,0,90,18,90,10,10,0,0,51,5,2),
                (0,1,80,18,100,10,0,4,0,18,5,1), (71,1,95,18,100,15,30,0,0,18,22,1)]
    for move, fields in enumerate(expected, 468):
        begin, end = struct.unpack_from("<II", archive, 28 + move * 8)
        record = archive[image + begin:image + end]
        assert len(record) == 16
        assert struct.unpack("<HBBBBBBHbBBB", record[:14]) == fields
        assert record[14:] == b"\0\0"
    for filename, part in (("anim_scripts.order", "anim"), ("move_scripts.order", "script")):
        lines = (output / filename).read_text().splitlines()
        assert len(lines) == 501, (filename, len(lines))
        for move, stem in enumerate(("fairy_wind", "disarming_voice", "draining_kiss", "play_rough", "dazzling_gleam", "moonblast"), 468):
            assert stem in lines[move] and part in lines[move], (filename, move, lines[move])
        assert ".shared" in lines[474]
    print("Actual move generator emits exact six records and stable reserved archive indices")
