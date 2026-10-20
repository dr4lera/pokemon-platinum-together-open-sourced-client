# Community client level caps

The provisional local client rule is enabled by default. The native project
configuration file supports:

```ini
[CommunityRules]
WorldLevelCaps=true
```

Caps by badge count 0–8 are 14, 22, 26, 32, 37, 41, 44, 50 and 62. Completing
the main game removes the cap. Battle EXP is clamped after all bonuses, including
foreign-OT bonuses, and before cached EXP, party EXP and the displayed award.
Already-overlevel Pokémon retain their level and EXP and gain no further EXP
until progression permits it. Imports, trades and raw Pokémon setters do not
rewrite provenance or reduce an existing Pokémon's level.

Rare Candy eligibility and application both refuse level gain at the cap.
DayCare withdrawal and copied EXP previews use the same clamp, keeping displayed
levels and withdrawal fees consistent. These routes were confirmed by inspection
of the privately held 0.1.7 reference. Frontier level normalization remains its
separate game rule.

This setting is local configuration, not a complete Together world rule. World
selection, saved per-world rules, negotiated peer progression, co-op EXP sharing
and corresponding multiplayer determinism are pending. The public foundation
still excludes link battles from EXP. These changes do not claim those systems
or full Together parity.

Run `python tests/run_community_rules.py` for portable C tests of table boundaries,
overflow-safe arithmetic, overlevel preservation, progression, disabled/completed
rules and OT independence through game API doubles. These tests compile the
actual rule modules, use an invented growth curve, and require no game assets.
They do not establish UI, real growth-table, save migration or in-game parity.
