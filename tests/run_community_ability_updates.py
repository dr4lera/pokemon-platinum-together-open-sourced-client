"""Actual engine callers for a bounded, opt-in 0.1.10 ability update.

Synthetic system/item/party boundaries require no game files or save. Damage,
accuracy, turn comparison, suppression/counting and party refresh are extracted
unchanged from their source definitions. Separate objects never touch ninja.
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

ROOT=Path(__file__).resolve().parents[1]
BUILD=Path(os.environ.get('COMMUNITY_TEST_BUILD',ROOT/'build_win64'))
CC=os.environ.get('CC') or shutil.which('gcc') or 'C:/msys64/ucrt64/bin/gcc.exe'
def run(args,**kwargs): subprocess.run([str(a) for a in args],check=True,**kwargs)
def function(source,prefix):
    match=re.search(re.escape(prefix)+r'[^;{]*\n\{',source)
    if not match: raise AssertionError(prefix)
    start=match.start(); end=match.end(); depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
def block(source,prefix,ending):
    start=source.index(prefix); end=source.index(ending,start)+len(ending)
    return source[start:end]
def flags(entry):
    args=shlex.split(entry['command'])[1:]
    for flag in ('-MQ','-MF','-o'):
        at=args.index(flag); del args[at:at+2]
    args=[a for a in args if a not in ('-MD','-c','-fpch-preprocess')]
    args=[a for a in args if not a.replace('\\','/').endswith(entry['file'].replace('\\','/'))]
    at=args.index('-include'); assert args[at+1]=='global_pch.h'
    args[at+1]=str(ROOT/'include/pch/global_pch.h')
    return args
library=(ROOT/'src/battle/battle_lib.c').read_text()
controller=(ROOT/'src/battle/battle_controller_player.c').read_text()
io=(ROOT/'src/battle/battle_io_command.c').read_text()
pieces=[block(library,'static const u8 sSpeedHalvingItemEffects[] = {','\n};'),
        block(library,'typedef struct ItemEffectTypePair','} ItemEffectTypePair;'),
        block(library,'static const ItemEffectTypePair sTypeBoostingItems[] = {','\n};'),
        block(library,'static const Fraction sStatStageBoosts[] = {','\n};'),
        block(library,'static const u16 sPunchingMoves[] = {','\n};'),
        block(library,'typedef struct DamageCalcParams','} DamageCalcParams;')]
for prefix in ('u8 Battler_Ability(', 'int BattleSystem_CountAbility(', 'BOOL Battler_IgnorableAbility(',
               'static BOOL CommunityAbility_IsBiteMove(', 'static int CommunityAbility_AdjustPower(',
               'int CommunityAbility_AdjustAccuracy(', 'static int CommunityAbility_MovePriority(',
               'static inline int CompareSpeed_ApplySimple(', 'u8 BattleSystem_CompareBattlerSpeed(',
               'int BattleSystem_CalcMoveDamage('):
    pieces.append(function(library,prefix))
pieces.extend([function(controller,'static inline int CalcMoveType('),
               function(controller,'static int BattleControllerPlayer_CheckMoveHitAccuracy('),
               function(io,'static void BtlIOCmd_RefreshPartyStatus(')])
commands=json.loads((BUILD/'compile_commands.json').read_text())
scratch=ROOT/'.build-tools'; scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ability-update-tests-',dir=scratch) as folder:
    temp=Path(folder); (temp/'generated').mkdir()
    run([sys.executable,ROOT/'subprojects/metang/metang.py','enum','--tag-name','Ability',
         '--guard','POKEPLATINUM_GENERATED_ABILITIES_H','--output',temp/'generated/abilities.h',ROOT/'generated/abilities.txt'])
    (temp/'actual_ability_updates.inc').write_text('\n\n'.join(pieces))
    entry=next(c for c in commands if c['file'].replace('\\','/').endswith('src/battle/battle_lib.c'))
    executable=temp/('ability-updates.exe' if os.name=='nt' else 'ability-updates')
    run([CC,'-I'+str(temp),*flags(entry),'-Werror=implicit-function-declaration',
         ROOT/'tests/community_ability_updates_test.c','-o',executable],cwd=BUILD)
    run([executable])
    env=os.environ.copy()
    env['PATH']=str(BUILD/'subprojects/yyjson-0.12.0')+os.pathsep+env.get('PATH','')
    msgenc=BUILD/'tools/msgenc'/('msgenc.exe' if os.name=='nt' else 'msgenc')
    for bank in ('ability_names','ability_descriptions'):
        source=ROOT/'res/text'/(bank+'.json')
        encoded=temp/(bank+'.bin')
        assert len(json.loads(source.read_text(encoding='utf-8'))['messages'])==144
        run([msgenc,'-e','--json','-c',ROOT/'tools/msgenc/charmap.txt',source,encoded],env=env)
        assert struct.unpack_from('<H',encoded.read_bytes())[0]==144
    print('Actual msgenc encoded both 144-entry original ability text banks')
    for name in ('src/battle/battle_lib.c','src/battle/battle_script.c','src/battle/battle_controller_player.c','src/battle/battle_io_command.c'):
        entry=next(c for c in commands if c['file'].replace('\\','/').endswith(name))
        run([CC,'-I'+str(temp),*flags(entry),'-Werror=implicit-function-declaration','-c',ROOT/name,
             '-o',temp/(Path(name).stem+'.o')],cwd=BUILD)
    print('Four changed battle translation units compiled separately')
