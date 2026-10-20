# Modern form lifecycle

The source implements the reviewed baseline lifecycle for Battle Bond, Stance
Change, Disguise and Schooling. These hooks require explicitly enabled modern
rules and the validated V5 catalog. They use the baseline gate independently of
the optional ability updates from the newer reference version.

## Party and battle state

Normal successful evolution from the preceding species to Greninja selects its
Battle Bond starting form only when friendship exceeds 219. Normal ability
calculation then derives Battle Bond for that starting form. Direct species
assignment and forced evolution do not acquire this extra evolution behavior.

The V5 provider supplies three battle stat overrides. Transitions derive attack,
defense, speed, special attack and special defense from the actual party
Pokémon's level, IVs, EVs and nature, then reapply Power Trick if active. HP,
types, ability and stat stages remain intact. Disguise has no extra stat row;
its broken appearance retains the current stats. The personal loader does not
apply battle-only stats or abilities to unrelated forms.

Schooling checks newly loaded battlers after legacy switch-in form checks and
checks all battlers at turn end after experience, battle-over and replacement
handling. It requires level 20 or higher and HP strictly above one quarter for
school form. One presentation script runs at a time in speed order.

Stance Change checks after status disruption and before obedience, PP decrement
and the ordinary move script. Damaging moves select attack form; the reviewed
defensive move selects defense form. Suppressed abilities, transformed Pokémon
and fainted battlers do not trigger these transitions.

Disguise intercepts direct damage after Substitute routing and before endurance
effects. Its first eligible hit breaks the disguise, discards incoming damage
and schedules the reviewed one-eighth-max-HP cost, with a minimum of one. Mold
Breaker and its enabled aliases use the existing ability-ignore predicate.

Battle Bond records opposing direct knockouts during damage routing and checks
the active attacker at move end, after battle-over handling. It does not
transform after the final knockout has already ended the battle. Transformed
Battle Bond and broken Disguise state are remembered by personality across
switches in bounded battle-local lists. Action initialization preserves these
lists; newly allocated battle contexts start empty. Temporary transitions do not
rewrite party save fields.

The affected multi-hit move's dormant handlers set power 20 before later
multipliers and select three hits for variable-hit requests, ahead of Skill
Link. Explicit fixed-hit requests and an already running multi-hit sequence
remain unchanged. The predicate checks form, species and Transform status
rather than adding an unsupported extra ability or HP condition.

## Presentation and verification

Seven original source scripts use existing form-refresh, substitute, mosaic,
sound, HP-update and wait commands. The preceding 42 reserved script IDs
explicitly fail, preserving the reviewed namespace without admitting unrelated
reference scripts. Eighteen original messages include contiguous ally, wild and
foe variants where the formatter requires them. No extracted scripts, private
stat tables, art or permission correspondence are distributed.

`python tests/run_community_forms.py` exercises the actual stat calculator,
evolution helper, form handlers, damage controller, before-move controller,
turn-end controller, move-end controller, multi-hit command and action
initializer against synthetic party/catalog/system boundaries. It covers
thresholds, nature and Power Trick, Substitute and Mold Breaker, suppression and
Transform, one-shot script resumes, final-knockout ordering and bounded switch
persistence. It encodes text with the real message compiler, assembles all seven
scripts and separately compiles five changed translation units. The species
loader fixture verifies Battle Bond starting-form ability selection through the
actual personal loader.

Rendered form changes, refresh callbacks and the actual HP-cost script remain
native acceptance checks. The full modern move catalog is still unadmitted:
the defensive stance move and Battle Bond's affected multi-hit move require
their own move/resource admission. Do not describe this as full roster, full
moveset or complete form parity before those checks and admissions pass.
