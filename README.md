# Platinum Together community gameplay client

**Development checkpoint — modernization is incomplete.**

This separately maintained fork of the public Platinum PC port targets the
installed Together0.1.10 feature set, retaining the0.1.7 behavioral baseline and
the requested cap, OT and ball-use changes. It is not the original Together source.

The Windows client builds. Fresh native runs verify single-client battles,
Union Room discovery, colored trainer sprites and authenticated nametags.
Two-client battle completion, trades and interrupted-save recovery remain open;
a prior battle receive failure has targeted parser repairs awaiting a completed
retry. User-confirmed gameplay on the earlier launcher used reference clients.

Modernization currently includes an opt-in65-species data provider, six Fairy
moves, ten gated ability rules, tested form lifecycles, and persistent modern
seen/caught flags. The0.1.10 roster contains71 added species; its six additional
species, the full modern movesets, remaining ability interactions, updated
options/UI, shared-world/Inverna gameplay and custom avatar rendering are pending.
Custom trainer art decoding/downloads pass checks; visible per-actor art is pending.

See the [checkpoint record](docs/CHECKPOINT_2026_10_20.md) for tested scope and
remaining work. This source checkpoint is not a finished gameplay release.

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
