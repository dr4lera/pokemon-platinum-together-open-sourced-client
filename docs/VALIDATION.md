# Source milestone checks

On 2026-10-20, the community client linked successfully as a Windows x64
`build_win64/main.exe` using GCC 15.2.0, Meson 1.12.0 and the dependency revisions
in `dependency-lock.json`. This is compilation evidence, not a gameplay test.

The real engine compile caught an incompatible pointer type in our capture-turn
mask: the SDK's `u32` is `unsigned long`, whereas the portable transaction helper
uses `unsigned`. The new internal context field now uses the helper's exact
type; no cast or aliasing workaround is used.

Checks run:

```text
python tests/run_community_rules.py
  community level-cap rules: passed
  community level-cap configuration: passed
python tests/run_community_learnsets.py
  community learnsets: decoder, bounded consumers and actual Move Reminder passed
gcc -std=c99 -Wall -Wextra -Werror -Iinclude tests/capture_rules_core_test.c ...
  capture rules core: target eligibility and transactional debit checks passed
ninja -C build_win64 -j 8 main.exe
  Linking target main.exe: succeeded
```

The level-cap checks compile the actual arithmetic, game adapter and INI parser
against small game API doubles. The capture checks exercise eligibility and
transactional inventory behavior. Independent read-only reviews covered engine
call sites, OT provenance, post-bonus EXP ordering, Candy/DayCare guards and
capture authorization/target lifetime; those reviews did not execute a game.
The learnset checks exercise the actual record API and Move Reminder consumer,
including legacy ordering, malformed inputs and synthetic move IDs above 511.
Review also caught an ARM file-handle leak in an existing archive-size helper
that the new loader would have called. The loader now owns and closes its archive
on every path, avoiding a change to the old helper's behavior. The final Windows
client links with that fix.

The published Linux rule workflow passes all three suites. The changed-source
publication lint reported zero failures and four identifier warnings; every
warned identifier already exists in the pinned public engine foundation. No new
closed-reference decompiler identifiers or copied reference files were added.

No original installation, running game or player save was modified for these
checks. No native gameplay, full asset packaging, new-client networking, modern
content, save compatibility or 1:1 Together parity is established by this record.
User-confirmed reference-client battles and trades are separate evidence.
