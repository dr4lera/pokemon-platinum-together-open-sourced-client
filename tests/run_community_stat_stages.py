"""Compile actual battle consumers against real engine structs, without assets.

The catalog and presentation/system calls are synthetic fixture boundaries. All
queue, opcode, shared stat-change and pointer-routing logic comes from source.
The shared build is read-only; compiler outputs stay in a disposable directory.
"""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get('COMMUNITY_TEST_BUILD', ROOT / 'build_win64'))
CC = os.environ.get('CC') or shutil.which('gcc') or 'C:/msys64/ucrt64/bin/gcc.exe'

def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def function(source, signature):
    # Select the definition, never a forward declaration.
    start = source.index(signature + '\n{')
    begin = source.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

def compile_flags(entry):
    args = shlex.split(entry['command'])[1:]
    for flag in ('-MQ', '-MF', '-o'):
        at = args.index(flag)
        del args[at:at + 2]
    args = [a for a in args if a not in ('-MD', '-c', '-fpch-preprocess')]
    args = [a for a in args if not a.replace('\\', '/').endswith(entry['file'].replace('\\', '/'))]
    at = args.index('-include')
    assert args[at + 1] == 'global_pch.h'
    args[at + 1] = str(ROOT / 'include/pch/global_pch.h')
    return args

commands = json.loads((BUILD / 'compile_commands.json').read_text())
script = (ROOT / 'src/battle/battle_script.c').read_text()
library = (ROOT / 'src/battle/battle_lib.c').read_text()
pieces = [function(script, signature) for signature in (
    'static int BattleScript_Read(BattleContext *battleCtx)',
    'static void BattleScript_Iter(BattleContext *battleCtx, int i)',
    'static BOOL BtlCmd_End(BattleSystem *battleSys, BattleContext *battleCtx)',
    'static inline BOOL AbilityBlocksSpecificStatReduction(BattleContext *battleCtx, int statOffset, int ability, int stat)',
    'static inline void SetupNicknameStatMsg(BattleContext *battleCtx, int msgID, int statOffset)',
    'static inline void SetupNicknameAbilityStatMsg(BattleContext *battleCtx, int msgID, int statOffset)',
    'static inline void SetupNicknameAbilityNicknameAbilityMsg(BattleContext *battleCtx, int msgID)',
    'static BOOL BtlCmd_ChangeStatStage(BattleSystem *battleSys, BattleContext *battleCtx)',
    'static BOOL CommunityStatStage_Alive(BattleContext *battleCtx, int battler)',
    'static BOOL CommunityStatStage_Predicate(BattleContext *battleCtx, int battler, unsigned predicate)',
    'static BOOL CommunityStatStage_Build(BattleSystem *battleSys, BattleContext *battleCtx)',
    'static BOOL CommunityStatStage_Next(BattleContext *battleCtx)',
    'static BOOL BtlCmd_CommunityAction(BattleSystem *battleSys, BattleContext *battleCtx)',
)]
pieces.append(function(library, 'static int MapSideEffectToSubscript(BattleContext *battleCtx, enum BattleSideEffectType type, u32 effect)'))
pieces.append(function(library, 'void BattleContext_Init(BattleContext *battleCtx)'))
scratch = ROOT / '.build-tools'
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='stat-stage-tests-', dir=scratch) as folder:
    temp = Path(folder)
    (temp / 'generated').mkdir()
    run([sys.executable, ROOT / 'subprojects/metang/metang.py', 'enum', '--tag-name', 'Ability',
         '--guard', 'POKEPLATINUM_GENERATED_ABILITIES_H', '--output', temp / 'generated/abilities.h',
         ROOT / 'generated/abilities.txt'])
    (temp / 'actual_stat_stage.inc').write_text('\n\n'.join(pieces))
    entry = next(c for c in commands if c['file'].replace('\\', '/').endswith('src/battle/battle_script.c'))
    flags = compile_flags(entry)
    executable = temp / ('stat-stage.exe' if os.name == 'nt' else 'stat-stage')
    run([CC, '-I' + str(temp), *flags, '-Werror=implicit-function-declaration',
         ROOT / 'tests/community_stat_stage_test.c', '-o', executable], cwd=BUILD)
    run([executable])

    # Exercise the real command macro and enumeration together.
    assembly = temp / 'command.s'
    assembly.write_text('#include "macros/btlcmd.inc"\n.data\nCommunityAction 0, 0, _fail\nEnd\n_fail:\nEnd\n')
    obj = temp / 'command.o'
    include_flags = [f for f in flags if f.startswith('-I')]
    run([CC, '-x', 'assembler-with-cpp', '-c', *include_flags,
         '-I' + str(ROOT / 'asm'), assembly, '-o', obj], cwd=BUILD)
    binary = temp / 'command.bin'
    sibling = Path(CC).with_name('objcopy.exe' if os.name == 'nt' else 'objcopy')
    objcopy = sibling if sibling.exists() else shutil.which('objcopy')
    if not objcopy:
        raise SystemExit('Set CC to a compiler with objcopy available')
    run([objcopy, '-O', 'binary', '-j', '.data', obj, binary])
    words = struct.unpack('<6I', binary.read_bytes()[:24])
    assert words == (223, 0, 0, 1, 222, 222), words
    print('Actual extension command macro/relative failure encoding passed')

    # Compile only affected translation units; never run the shared build.
    for name in ('src/battle/battle_script.c', 'src/battle/battle_lib.c'):
        entry = next(c for c in commands if c['file'].replace('\\', '/').endswith(name))
        run([CC, '-I' + str(temp), *compile_flags(entry), '-Werror=implicit-function-declaration', '-c',
             ROOT / name, '-o', temp / (Path(name).stem + '.o')], cwd=BUILD)
    print('Changed battle translation units compiled separately')
