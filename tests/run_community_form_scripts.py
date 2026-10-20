# SPDX-License-Identifier: GPL-3.0-or-later
"""Check actual assembled form-script IDs and refusal slots; no private assets."""
import argparse
import importlib.util
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=root / 'build_win64')
parser.add_argument('--baseline', type=Path, help='Optional private earlier source-generated297-member NARC')
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('form_script_archive', root / 'tools/community/prepare-modern-assets.py')
assets = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = assets
spec.loader.exec_module(assets)
commands = re.findall(r'^ScriptCommand\((\w+),', (root / 'include/data/scripts/btlcmd.h').read_text(), re.M)
vars = (root / 'generated/battle_script_vars.txt').read_text().splitlines()
ops = (root / 'generated/battle_script_opcodes.txt').read_text().splitlines()
reserved = struct.pack('<5I', commands.index('BTLCMD_UPDATEVAR'), ops.index('OPCODE_FLAG_ON'), vars.index('BTLVAR_MOVE_STATUS_FLAGS'), 1 << 6, commands.index('BTLCMD_END'))
archive = assets.parse_narc((args.build / 'res/battle/scripts/sub_seq.narc').read_bytes())
assert len(archive.members) == 346
assert all(member == reserved for member in archive.members[297:339])
names = ['form_change', 'school_form', 'school_solo', 'stance_attack', 'stance_defense', 'disguise_break', 'battle_bond']
order = (root / 'res/battle/scripts/subscripts/sub_seq.order').read_text().splitlines()
enums = (root / 'generated/battle_subscripts.txt').read_text().splitlines()
assert len(order) == len(enums) == 346
for index, name in enumerate(names, 339):
 assert order[index] == 'subscript_community_' + name
 assert enums[index] == 'BATTLE_SUBSCRIPT_COMMUNITY_' + name.upper()
 member = archive.members[index]
 assert len(member) >= 4 and len(member) % 4 == 0
 assert struct.unpack_from('<I', member, len(member) - 4)[0] == commands.index('BTLCMD_END')
if args.baseline:
 baseline = assets.parse_narc(args.baseline.read_bytes())
 assert len(baseline.members) == 297 and archive.members[:297] == baseline.members
 print('All297 earlier source-generated subscripts remain byte-identical')
print('Actual assembled form archive:346 members,42 explicit refusal slots and seven stable form IDs passed')
