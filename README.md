# Platinum Together community gameplay client

Source-controlled gameplay client migration for the community launcher and private
servers. This is a separately maintained fork of the public Platinum PC port,
not the original Platinum Together source. The target is matching 0.1.7 gameplay,
with the explicitly requested cap, OT and ball-use fixes.

**Migration in progress.** Together co-op, accounts/city integration, modern
content, trade recovery, custom trainer art and complete 1:1 parity are not yet
implemented or verified in this client. Working native battles/trades previously
confirmed by the user ran on separately prepared reference clients.

Implemented source rules:

- World EXP caps apply regardless of OT, including Candy and DayCare routes.
- Foreign OT does not add badge-based disobedience; real provenance is preserved.
- Ordinary balls require a wild battle and exactly one active opponent.
- Execution debits one available ball before throwing; stale/repeated actions fail.
- The existing caught-species battle HUD pipeline is retained.

[Build](docs/BUILD_COMMUNITY.md) · [Migration matrix](docs/MIGRATION_STATUS.md) ·
[Level caps](docs/COMMUNITY_LEVEL_CAPS.md) · [Provenance](THIRD_PARTY_NOTICES.md) ·
[Upstream documentation](docs/UPSTREAM_README.md)

The client is built from source. Users supply their own compatible ROM/assets.
No installed Together executable, private decompilation, saves or account keys are
included. This fork's community additions use GPL-3.0-or-later; upstream sources
and dependencies retain their respective notices and terms. See LICENSE-COMMUNITY.

The separate [launcher, backend and editors](https://github.com/dr4lera/pokemon-platinum-together-open-sourced-launcher)
remain available. New gameplay release publication is paused during migration.
