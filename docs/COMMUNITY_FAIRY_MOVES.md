# Bounded Fairy battle support

The source catalog reserves Fairy type 18 and adds six moves without renumbering
existing moves: Fairy Wind 468, Disarming Voice 469, Draining Kiss 470, Play Rough
471, Dazzling Gleam 472 and Moonblast 473. These records are original source
descriptions and metadata implementing the inspected reference behavior.

| Move | Class | Power | Accuracy | PP | Additional behavior |
|---|---|---:|---:|---:|---|
| Fairy Wind | Special | 40 | 100 | 30 | Single target |
| Disarming Voice | Special | 40 | Always hits | 15 | Adjacent opponents; sound |
| Draining Kiss | Special | 50 | 100 | 10 | Contact; recover three quarters of actual damage |
| Play Rough | Physical | 90 | 90 | 10 | Contact; 10% chance to lower Attack one stage |
| Dazzling Gleam | Special | 80 | 100 | 10 | Adjacent opponents |
| Moonblast | Special | 95 | 100 | 15 | 30% chance to lower Special Attack one stage |

The six moves reuse existing battle effects and the generic move-effect script.
The drain subscript preserves the ordinary half-damage path for existing moves;
Draining Kiss takes a separate three-quarter path before the existing Big Root,
Liquid Ooze and Heal Block processing. Disarming Voice joins the engine's sound
list so Soundproof blocks it. Source AI heuristics have their own older sound
checks; reference parity for those heuristics remains unverified.

The legacy type chart remains intact. `CommunityModernRules_Enabled()` selects a
separate modern chart in damage/effectiveness calculations, immunity override
helpers and AI matchup queries. Fairy is super effective against Fighting,
Dragon and Dark; resisted by Fire, Poison and Steel. Fighting, Bug and Dark are
resisted by Fairy; Poison and Steel are super effective against Fairy; Dragon
does no damage to Fairy. Modern Steel loses its Ghost and Dark resistances.
The original Foresight separator, Ghost immunities and terminal marker retain
their ordering. A repeated defensive type is applied once.

The gate is supplied by validated runtime catalog selection. This unit does not
define a default-on gate, a world selection protocol or peer negotiation.
Metronome retains the original 1–467 range while the gate is off. With this
bounded source catalog it can select 1–473 when enabled. The reference's larger
modern move catalog and its full Metronome range are not implemented by these
six records alone. Species/form retyping, older move stat/type overrides and
supplementary learnsets belong to the separate runtime catalog adapter.

The type-icon loader maps Fairy to member 343 and palette 1 of the supported
private battle-icon archive. Contest icons now follow `NUM_POKEMON_TYPES` rather
than a hardcoded 18. The source type-name bank contains the nineteenth Fairy entry. No Fairy sprite, extracted icon, private text bank or animation archive
is supplied by this repository.

Each new source animation file explicitly includes the public base's reserved
animation as a **build placeholder**. It does not reproduce the reference move's
visuals. Static inspection found the six reference scripts use existing VM
commands and callbacks; Moonblast additionally requires the implemented mode-2
emitter adaptation. The private merge tool described in
`COMMUNITY_EMITTER_MODE.md` can replace only these six placeholders and append
their required particle bundle and Fairy icon after exact hash validation.
Rendering/audio verification remains pending. The restricted filesystem overlay
still excludes battle animation and command archives; merely selecting a
private runtime directory does not activate those archives. The merge tool
does not change that allowlist or enable modern gameplay.

The move generator no longer appends the three inaccessible old dummy move
records at 468–470. It emits 474 actual records, preserves the six exact IDs,
and retains reserved animation/script entries 474–500. Both script-order files
remain 501 entries long. Existing original move records and ordering are retained.

Run `python tests/run_community_fairy_moves.py` with an existing foundation build
(default `build_win64`, override `COMMUNITY_TEST_BUILD`). It uses a C compiler,
the matching `objcopy`, generated public headers and unchanged tool dependency
libraries. All new headers, objects, executables, archives and fixtures go into a
disposable test directory. It compiles actual engine fragments and four full
changed translation units independently; assembles the actual drain script and
checks its branch/rounding behavior; and independently builds/runs the actual
move generator to verify every field of the six records and their archive
indices. The actual text encoder also encodes all four generated move text banks
and must reject a synthetic unsupported character. No game binary, private
assets or private catalog fixtures are read.
These checks establish source behavior and compilation, not in-game or animation
parity. A complete client link and runtime verification are separate checks.

The move-menu palette has its own bounded catalog accessor. It copies the
validated 32-byte private palette into aligned, stable storage; an invalid type
uses the ordinary fallback. The actual palette consumer fixture covers this
path. A native battle first revealed the missing index-18 palette; the fix
compiles, but complete battle/animation verification is still pending.
