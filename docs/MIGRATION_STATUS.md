# Community gameplay client migration

The user requested an independently built, source-controlled gameplay client on
2026-10-20. Existing launcher/server/editor repositories do not contain the
Platinum Together gameplay source. Their native-play tests used a separately
prepared, unmodified copy of the supplied game.

## Fixed reference and foundation

- Behavioral reference: legitimately installed Platinum Together 0.1.7, SHA-256
  `3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9`.
- Public engine foundation: cybervisi0n/pokeplatinum, `pc_port` commit
  `35067868f64f23d25d17ac3e536cfdd9a7f40523`, based on pret/pokeplatinum and libntr.
- The reference executable has both signed disk-file checks and a code-memory
  watchdog. This migration does not patch or disable them; it builds another client.
- Private decompilation is a behavioral reference, not published source in this
  fork. No complete 1:1 client reconstruction or native feature parity is claimed.

## Acceptance matrix

| System | Current evidence | Required before parity |
| --- | --- | --- |
| Sinnoh engine/platform | Public source foundation obtained | Build and run with user-supplied ROM assets |
| World level caps | Own source clamps battle/Candy/DayCare EXP; portable C and config checks pass | Full client build/play, per-world rules and peer progression negotiation |
| Traded Pokémon obedience | Own community build returns normal obedience regardless of OT; provenance helpers retained | Full client build/play and battle parity |
| Ball eligibility | Own source shares wild-only, exactly-one-live-target checks between UI and execution; C core checks pass | Full client build/play and co-op integration |
| Ball inventory | Own source requires successful debit, rejects duplicate actor/turn actions, binds throw authorization and target | Full client build/play, per-player co-op inventory and failure recovery |
| Battle caught marker | Reference native caught query → HUD → draw inspected | Preserve known/caught distinction and modern/trade registration |
| Accounts, friends, Inverna | Our backend implemented and tested | Own native client protocol integration |
| Co-op/relay, battles, trades | User-confirmed on reference clients | Own client implementation and two-client parity checks |
| Modern species, abilities, forms, moves | Present in supplied 0.1.7 assets/code | Version-pinned catalog and complete battle behavior coverage |
| Saves/trade recovery | Reference format and journal partly inspected | Own compatible IO, closed-copy migration, interruption checks |
| Rendering, controls, audio, story | Public base engine available | Match reference behavior and chosen/custom trainer appearances |
| 0.1.10 additions | Announced; artifact unavailable | Released artifact and separate comparison |

## Explicit deviations requested

Level caps apply to traded and locally obtained Pokémon alike. Preserve actual
OT/TID/SID and trade provenance while removing foreign-OT obedience penalties.
Refuse ordinary balls when two opponents remain, and always refuse trainer-owned
targets. Recheck at execution, consume exactly one available ball for each actual
throw, and never duplicate inventory through queued actions. Show the caught-ball
HUD marker for caught species, including received trades; merely seen is distinct.

## Release gate

Launcher/server dev12 releases remain paused during the client migration. Source
milestones must be labeled accurately. A source checkout, successful build, or
isolated rule test does not establish a complete independently playable client,
Together multiplayer support, or 1:1 parity. Never include ROMs, copied game
executables, extracted runtime assets, private references, accounts or saves.
