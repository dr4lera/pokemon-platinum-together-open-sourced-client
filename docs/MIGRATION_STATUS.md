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
- Decompilation is a behavioral reference, not original source recovered by this
  fork. The current milestone keeps that reference private. Permission records
  are retained privately; preserve third-party notices in any later work. No complete
  1:1 client reconstruction or native feature parity is claimed.

## Acceptance matrix

| System | Current evidence | Required before parity |
| --- | --- | --- |
| Sinnoh engine/platform | Windows x64 source executable builds; isolated startup reaches new-game introduction with actual framebuffer capture | Story/world playthrough and release packaging |
| World level caps | Own source clamps battle/Candy/DayCare EXP; C/config checks and full client compilation pass | Client play, per-world rules and peer progression negotiation |
| Traded Pokémon obedience | Community executable compiles with normal obedience regardless of OT; provenance helpers retained | Client play and battle parity |
| Ball eligibility | Shared wild-only, exactly-one-live-target UI/execution checks; C checks and full compilation pass | Client play and co-op integration |
| Ball inventory | Successful debit required; duplicate actor/turn actions rejected; authorization and target bound; full compilation passes | Client play, per-player co-op inventory and failure recovery |
| Battle caught marker | Reference native caught query → HUD → draw inspected | Preserve known/caught distinction and modern/trade registration |
| Accounts, parties, chat | Native signed adapter and real-backend fixtures pass; game UI compiles | Rendered flow and packaged HTTPS checks; friends/Inverna actors |
| Native wireless battles/trades | Own Service, signed game-link backend and semantic WM provider implemented; two-adapter discovery/join/data/cancellation fixtures pass | Two actual source games through native battle/trade screens and interruption recovery |
| Shared-world co-op/Inverna | User-confirmed on prepared reference clients | Source gameplay implementation and two-client parity checks |
| Modern species, abilities, forms, moves | Strict V5 numeric catalog, early personal/learnset/evolution consumers and six Fairy moves compile and pass fixtures | Runtime graphics verification, remaining move/ability behavior, cries, registration, dex/save integration |
| Learnset representation | Bounded decoders and V5 provider compile; all65 hash-pinned lists match offline; supplements and actual consumer fixtures pass | Full later-move behavior and runtime per-species verification |
| Saves/trade recovery | Reference format and journal partly inspected | Own compatible IO, closed-copy migration, interruption checks |
| Rendering, controls, audio, story | Public base engine available | Match reference behavior and chosen/custom trainer appearances |
| 0.1.10 additions | Installed artifact independently hash-pinned; private archive comparison started | Separate code/behavior audit and tested update adapters |

## Explicit deviations requested

Level caps apply to traded and locally obtained Pokémon alike. Preserve actual
OT/TID/SID and trade provenance while removing foreign-OT obedience penalties.
Refuse ordinary balls when two opponents remain, and always refuse trainer-owned
targets. Recheck at execution, consume exactly one available ball for each actual
throw, and never duplicate inventory through queued actions. Show the caught-ball
HUD marker for caught species, including received trades; merely seen is distinct.

## Release gate

See [the source validation record](VALIDATION.md) for the precise build and test
boundary. A successfully linked executable is an early migration milestone.

The combined, Inverna and world server dev12 previews are published separately.
Gameplay-client binary releases remain paused during the migration. Source
milestones must be labeled accurately. A source checkout, successful build, or
isolated rule test does not establish a complete independently playable client,
Together multiplayer support, or 1:1 parity. Never include ROMs, copied game
executables, extracted runtime assets, private references, accounts or saves.

## Current source units

The own-client backend link is opt-in and combined-role only. Its current58
tests include signed renewal, bounded sessions, cancellation withdrawal and
teardown. Native Service and actual WM-provider fixtures use fresh disposable
identities and the real backend. They retain receive buffers through the ARM9
acknowledgment and discard unindicated old-epoch data after cancellation. SDK
callback recipients are fixtures; these are not two-game battle/trade evidence.

The modern gate is explicitly selected before startup and remains immutable.
Its V5 file is generated privately from the pinned reference; no catalog bytes
or reference assets ship here. Original source-generated animation archives may
be privately merged with only the six reviewed scripts, particle485 and icon343.
Whole patched animation archives are not enabled by the restricted overlay.

Next acceptance work: actual battle scenes and two-client native application
flows, then remaining move/ability semantics,
dex/save compatibility and Together world/appearance behavior. Keep source
milestones public while the gameplay binary release remains gated by evidence.

The next bounded graphics unit now routes modern sprite templates, heights,
89-byte animation/shadow records and icons before legacy resource access. Its
actual-consumer fixture covers65 IDs/four graphics forms, front/back/gender/shiny,
egg/Spinda fallback and signed fields. Private append-only graphics and six-bank
text preparation preserve original resources and pass structural tests; they
contain no bundled assets. A character-sprite ownership fix retains NCGR data
through decrypt/Spinda/tiling and passes a regression that fails the old source.
The desktop SDK palette loader now handles validated packed palettes with a
larger expanded VRAM extent. The full Windows client links with these changes;
fresh single-client Fairy and Sylveon battles completed normally, with visible
modern sprites and unchanged capped EXP. This is distinct from two-client
battle/trade completion.

## Newly available update reference

The user installed0.1.10 on2026-10-20 and requested comparison. Its package
manifest identifies0.1.10; the18,513,408-byte executable has SHA256
`45185bc3f1849c6b93910efef451ea70cd7a03fd81dd4733e736eae65ed0b4d8`.
The PE version resource still reports0.1.7, so it is not used as the identity.
A separate private snapshot verifies all14 package-owned patch archives against
the installer hashes: six match the retained0.1.7 copy, seven changed and one
was added. Changed resources include battle subscripts, field scripts/matrices,
text, species graphics/icons, particles and move animations. The separate native
reference analysis exported all67,794 recognized internal functions and566
external entries after bounded timeout retries, with zero remaining export
failures. This is private pseudocode, not recovered original source or an
exhaustiveness claim beyond recognized functions. Inventory/export evidence is not
proof that update features are implemented or that servers accept its client.
Existing0.1.7 private importers remain pinned and deliberately reject this new
executable. The source client continues from the tested baseline while update
differences are reviewed and adapted explicitly.

Two fresh source clients now authenticate, discover each other, render native
Union Room avatars in color, exchange invitations and confirm teams through the
normal menus. A native battle completed several synchronized turns before one
peer reported a receive/parser error and the other later timed out. Battle/trade
acceptance remains open while that concrete failure is repaired. No original
profile, save or installed executable was changed by those tests.
