# Optional 0.1.10 ability rules

The reference catalog remains pinned to 0.1.7. A separate, immutable rules option
selects verified changes from the locally installed 0.1.10 update:
`PLATINUM_COMMUNITY_0110_ABILITIES=1`. It also requires the existing validated
modern catalog and `PLATINUM_COMMUNITY_MODERN_RULES=1`. The default preserves
the baseline rules. The ability option participates in peer compatibility;
sharing numeric catalog data does not imply sharing battle rules.

The current implementation covers ten added ability IDs within the admitted
source battle rules:

| Ability | Implemented rule |
| --- | --- |
| Big Pecks | Adds Defense to the existing ability-specific opponent stat-drop guard; self drops retain the original path. |
| Prankster | Adds one priority to a selected, nonzero status move before speed comparison. |
| Victory Star | Repeats integer accuracy multiplication by 110/100 once per living same-side bearer, after Compound Eyes. |
| Turboblaze, Teravolt | Use the original Mold Breaker bypass paths for redirection, immunity/type checks, ignorable abilities, the defensive Flower Gift boost, and party Heal Bell refresh. |
| Strong Jaw | Multiplies the reviewed biting-move power by 15/10. |
| Dark Aura, Fairy Aura | Multiply matching move power by 4/3 once when any living bearer is present. Multiple bearers do not compound the aura. |
| Infiltrator | Bypasses Reflect/Light Screen in damage calculation, hostile Mist and Substitute stat-drop checks, hostile Substitute script/damage routing, and the specific direct/indirect Safeguard comparison. The underlying shields remain active. |
| Unseen Fist | Lets contact moves bypass ordinary Protect through the actual move-hit override path. Non-contact, suppression and charge-turn checks retain their existing paths. |

Power changes run before the existing power multiplier and Technician checks.
This ordering and each integer division matter. Ability suppression uses the
existing effective-ability getter; aura and accuracy counts use the actual
living-battler counter.

Twenty stable label slots (124–143) and original short descriptions are provided
for display. Labels are not implementation claims. The remaining abilities,
four species-specific form mechanisms, ability-specific switch-in announcements
and trainer AI scoring are still pending. No later-game immunity or interaction
is inferred from an ability name. No additional modern moves are admitted by
this update.

Modern terrain-based priority barriers are not admitted by this client yet.
The Unseen Fist helper accepts an explicit priority-blocked input and refuses a
bypass when it is set. Its current caller handles ordinary Protect; that future
terrain result must be wired before admitting those modern barrier rules. This
unit does not claim their interactions work.

`tests/run_community_ability_updates.py` compiles extracted actual engine callers
against the real battle structures. It exercises full damage calculation,
accuracy, turn ordering, effective abilities, living-battler counts, defensive
bypass, party status refresh and barrier routing using synthetic
system/item/party boundaries.
Cases include gate-off behavior, suppression, dead/foreign-side bearers,
non-stacking auras, compounded accuracy rounding, power ordering with Technician,
and status versus damaging moves. Barrier cases cover preserved side-condition
data, self-target exceptions, held-item effects, substitute versus real-HP script
routing, Reflect damage and ordinary Protect. The stat-stage fixture additionally
checks Big Pecks and Infiltrator against Mist, Substitute and Clear Body.
Both ability text banks pass the real font encoder, and the four
changed battle translation units are compiled separately without running ninja.

These are source/fixture checks. Actual-game presentation and the remaining
0.1.10 ability interactions require their own acceptance checks.
