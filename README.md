# Platinum Together community gameplay client

Source-controlled gameplay client migration for the community launcher and private
servers. This is a separately maintained fork of the public Platinum PC port,
not the original Platinum Together source. The target is matching 0.1.7 gameplay,
with the explicitly requested cap, OT and ball-use fixes.

The Windows source executable builds and has reached the new-game introduction
in an isolated runtime. Native accounts, parties, global chat and signed game
links are implemented and tested against the real backend. The engine's wireless
provider passes discovery, joining, MP data and cancellation fixtures. Native
Union Room battles/trades still need two-game verification; shared-world play,
Inverna actors, modern-species completion, custom trainer art and full 1:1 parity
remain under development. Earlier user-confirmed gameplay used reference clients.

Implemented source rules:

- World EXP caps apply regardless of OT, including Candy and DayCare routes.
- Foreign OT does not add badge-based disobedience; real provenance is preserved.
- Ordinary balls require a wild battle and exactly one active opponent.
- Execution debits one available ball before throwing; stale/repeated actions fail.
- The existing caught-species battle HUD pipeline is retained.
- An opt-in, validated private catalog provides modern numeric data, evolution
  rules and six Fairy moves. This is not a completed playable 65-species roster.

[Build](docs/BUILD_COMMUNITY.md) · [Migration matrix](docs/MIGRATION_STATUS.md) ·
[Level caps](docs/COMMUNITY_LEVEL_CAPS.md) · [Provenance](THIRD_PARTY_NOTICES.md) ·
[Validation](docs/VALIDATION.md) · [Upstream documentation](docs/UPSTREAM_README.md)

[Native online](docs/COMMUNITY_ONLINE.md) · [Wireless adapter](docs/COMMUNITY_WIRELESS.md) ·
[Modern species](docs/COMMUNITY_SPECIES_PLAN.md) · [Fairy moves](docs/COMMUNITY_FAIRY_MOVES.md)

The client is built from source. Users supply their own compatible ROM/assets.
No installed Together executable, private decompilation, saves or account keys are
included. This fork's community additions use GPL-3.0-or-later; upstream sources
and dependencies retain their respective notices and terms. See LICENSE-COMMUNITY.

The separate [launcher, backend and editors](https://github.com/dr4lera/pokemon-platinum-together-open-sourced-launcher)
remain available. New gameplay release publication is paused during migration.
