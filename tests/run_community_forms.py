"""Exercise actual form consumers with synthetic party/catalog boundaries.

No reference resources, save, game launch or shared ninja invocation is needed.
"""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get('COMMUNITY_TEST_BUILD', ROOT / 'build_win64'))
CC = os.environ.get('CC') or shutil.which('gcc') or 'C:/msys64/ucrt64/bin/gcc.exe'


def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)


def function(source, prefix):
    match = re.search(re.escape(prefix) + r'[^;{]*\n\{', source)
    assert match, prefix
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def flags(entry):
    args = shlex.split(entry['command'])[1:]
    for flag in ('-MQ', '-MF', '-o'):
        at = args.index(flag)
        del args[at:at + 2]
    args = [a for a in args if a not in ('-MD', '-c', '-fpch-preprocess')]
    args = [a for a in args if not a.replace('\\', '/').endswith(entry['file'].replace('\\', '/'))]
    at = args.index('-include')
    args[at + 1] = str(ROOT / 'include/pch/global_pch.h')
    return args


library = (ROOT / 'src/battle/battle_lib.c').read_text()
pokemon = (ROOT / 'src/pokemon.c').read_text()
controller = (ROOT / 'src/battle/battle_controller_player.c').read_text()
script = (ROOT / 'src/battle/battle_script.c').read_text()
start = pokemon.index('static const s8 sNatureStatAffinities[][5] = {')
end = pokemon.index('\n};', start) + 3
pieces = [pokemon[start:end], function(pokemon, 'static u16 Pokemon_GetNatureStatValue('),
          function(pokemon, 'void Pokemon_ApplyCommunityEvolutionForm('),
          function(pokemon, 'int Pokemon_CalcCommunityFormStats(')]
for prefix in ('u8 Battler_Ability(', 'BOOL Battler_IgnorableAbility(',
               'static BOOL CommunityForms_Eligible(', 'static BOOL CommunityForms_Remembered(',
               'static void CommunityForms_Remember(', 'static void CommunityForms_SetForm(',
               'static void CommunityForms_Change(', 'void CommunityForms_PartyLoaded(',
               'int CommunityForms_CheckSchooling(', 'int CommunityForms_BeforeMove(',
               'int CommunityForms_InterceptDamage(', 'void CommunityForms_MarkKnockout(',
               'int CommunityForms_AfterMove(', 'int CommunityForms_BondShuriken(', 'int CommunityForms_AdjustPower('):
    pieces.append(function(library, prefix))
pieces.append(function(controller, 'static void BattleControllerPlayer_UpdateHP('))
pieces.append(function(library, 'void BattleContext_Init('))
for prefix in ('enum BeforeMoveState {', 'enum ObedienceCheckResult {'):
    start = controller.index(prefix)
    pieces.append(controller[start:controller.index('\n};', start) + 3])
for prefix in ('static void BattleControllerPlayer_BeforeMove(',
               'static void BattleControllerPlayer_TurnEnd(', 'static void BattleControllerPlayer_MoveEnd('):
    pieces.append(function(controller, prefix))
for prefix in ('static int BattleScript_Read(', 'static void BattleScript_Iter(', 'static BOOL BtlCmd_SetMultiHit('):
    pieces.append(function(script, prefix))
commands = json.loads((BUILD / 'compile_commands.json').read_text())
scratch = ROOT / '.build-tools'
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='forms-tests-', dir=scratch) as folder:
    temp = Path(folder)
    (temp / 'generated').mkdir()
    for name, tag in (('abilities', 'Ability'), ('battle_subscripts', 'BattleSubscript')):
        run([sys.executable, ROOT / 'subprojects/metang/metang.py', 'enum', '--tag-name', tag,
             '--guard', 'COMMUNITY_TEST_' + name.upper(), '--output', temp / 'generated' / (name + '.h'),
             ROOT / 'generated' / (name + '.txt')])
    (temp / 'actual_forms.inc').write_text('\n\n'.join(pieces))
    entry = next(c for c in commands if c['file'].replace('\\', '/').endswith('src/battle/battle_lib.c'))
    executable = temp / ('forms.exe' if os.name == 'nt' else 'forms')
    run([CC, '-I' + str(temp), *flags(entry), '-Werror=implicit-function-declaration',
         ROOT / 'tests/community_forms_test.c', '-o', executable], cwd=BUILD)
    run([executable])

    # Actual text compiler also creates the temporary message enum used by scripts.
    env = os.environ.copy()
    env['PATH'] = str(BUILD / 'subprojects/yyjson-0.12.0') + os.pathsep + env.get('PATH', '')
    bank = temp / 'res/text/bank/battle_strings.h'
    bank.parent.mkdir(parents=True)
    msgenc = BUILD / 'tools/msgenc' / ('msgenc.exe' if os.name == 'nt' else 'msgenc')
    run([msgenc, '-e', '--json', '-c', ROOT / 'tools/msgenc/charmap.txt',
         '-H', 'res/text/bank/battle_strings.h', ROOT / 'res/text/battle_strings.json', temp / 'battle_strings.bin'], env=env, cwd=temp)
    names = [m['id'] for m in json.loads((ROOT / 'res/text/battle_strings.json').read_text(encoding='utf-8'))['messages']]
    for group in ('SchoolForm', 'SchoolSolo', 'DisguiseBroken', 'BondStrength', 'BondForm'):
        first = names.index('BattleStrings_Text_Community' + group + '_Ally')
        assert names[first:first+3] == ['BattleStrings_Text_Community' + group + '_' + side for side in ('Ally', 'Wild', 'Foe')]
    include_flags = [f for f in flags(entry) if f.startswith('-I')]
    for name in ('form_change', 'school_form', 'school_solo', 'stance_attack', 'stance_defense', 'disguise_break', 'battle_bond'):
        run([CC, '-x', 'assembler-with-cpp', '-c', '-I' + str(temp), *include_flags,
             '-I' + str(ROOT / 'asm'), ROOT / 'res/battle/scripts/subscripts' / ('subscript_community_' + name + '.s'),
             '-o', temp / (name + '.o')], cwd=BUILD)
    print('Actual battle text encoded and all seven form scripts assembled')

    for name in ('src/pokemon.c', 'src/evolution.c', 'src/battle/battle_lib.c', 'src/battle/battle_controller_player.c', 'src/battle/battle_script.c'):
        entry = next(c for c in commands if c['file'].replace('\\', '/').endswith(name))
        run([CC, '-I' + str(temp), *flags(entry), '-Werror=implicit-function-declaration', '-c',
             ROOT / name, '-o', temp / (Path(name).stem + '.o')], cwd=BUILD)
    print('Five changed form translation units compiled separately')
