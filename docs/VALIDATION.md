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

No original installation, running game or player save was modified for these
checks. No native gameplay, full asset packaging, new-client networking, modern
content, save compatibility or 1:1 Together parity is established by this record.
User-confirmed reference-client battles and trades are separate evidence.
