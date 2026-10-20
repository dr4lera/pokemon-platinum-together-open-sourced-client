# Modern battle migration plan

This is an integration plan, not a claim that the complete modern roster works.
The current source implements the separately documented six Fairy moves. A
private audit has identified 374 further move records and 20 added ability IDs in
the 0.1.7 reference. Catalog presence alone does not establish engine support.
No reference tables, scripts, text banks or assets are distributed here.

## Dependencies

1. Extend generated move identifiers and all `MAX_MOVES` consumers while
   preserving original IDs, the 16-byte `MoveTable` layout and reserved tails.
   `generated/moves.txt`, `src/move_table.c`, `include/battle/ai_context.h` and
   `tools/dataproc/src/moveproc.c` must agree on valid records and archive slots.
   Text names/descriptions and animation admission require separate validation.
2. Add a versioned, hash-pinned private numeric provider for the complete move
   records, stat-change definitions, side-effect routing and random-move
   exclusions. Reject incomplete caches rather than accepting a partial catalog.
   Existing personal, learnset and save layouts do not need to change for this.
3. Extend the battle-script VM deliberately. The reference adds one opcode with
   three operands and 16 implemented selectors; selector 3 is invalid. The original
   command table ends at 222; this first unit registers 223. Added effects also
   need 33 new side-effect routing entries beyond the original 145-entry table
   and 49 new subscripts.
   `src/battle/battle_script.c`, `include/data/scripts/btlcmd.h`,
   `asm/macros/btlcmd.inc`, `include/data/move_side_effect_subscripts.h` and the
   generated pointer/subscript namespaces must change together.
4. Implement named battle state and all producer/consumer hooks before admitting
   scripts that depend on it. This includes queued stat changes, protection,
   terrain/side conditions, item/party changes, switching and end-of-turn cleanup.
   `include/battle/battle_context.h`, `src/battle/battle_lib.c` and controller
   phases are part of this work. Reusing an existing effect number is insufficient
   to prove a modern move has no additional damage, targeting or priority hook.
5. Add abilities through their real engine hooks: suppression/defensive bypass,
   entry, priority, accuracy, power, status protection, stat drops, fainting,
   forms and visual refresh. Existing byte-sized ability storage can represent
   the observed IDs. Generated labels or imported personal data must not be
   advertised as implemented ability behavior.

## First functional unit

The first stage now implements original opcode decoding and selectors 0/1/5 for
the 41-entry stat-change family: target/predicate selection, a 24-entry battle-local
queue, shared stat-stage processing and bounded provider-backed pointer routing.
The queue resets with the existing action initializer; its larger delta is
consumed once. Target enum 1 means the side-effect recipient, which may differ
from the defender. Predicate 3 tests poison/toxic, rather than every major status.

`tests/run_community_stat_stages.py` uses the real engine structures and extracts
the actual opcode, queue, shared stat-change, initializer and routing functions.
It checks ordering, partner/death filtering, capacity, stage limits, a three-stage
increase, existing ability/Mist/Substitute prevention, failure jumps, gate-off
behavior and message grouping. It also assembles the real command macro and
compiles the affected translation units separately from the shared build.

This unit does not admit additional move scripts through normal gameplay.
`MAX_MOVES` and the functional move-provider range remain unchanged. Unsupported
selectors explicitly fail. Modern forced-grounding/levitation producers are
still pending; the grounded predicate currently covers state represented by the
existing engine. Other selectors include larger move-specific dispatchers and
must follow their state dependencies. Importing the full script archives before
those dependencies exist would produce incorrect battles.

## Version boundary

The separately installed 0.1.10 update is under review. Its effect and move-script
archives match 0.1.7, while its subscript archive changes two existing members and
adds four. Static inspection also identifies new gameplay hooks for 16 added
abilities. These changes require an explicit adaptation decision and independent
tests; 0.1.7 extraction offsets cannot be applied to the newer executable.

Archive membership, metadata import, built code, tested rules and verified game
rendering are separate completion criteria. Animation resources and callbacks
remain subject to their own compatibility checks.
