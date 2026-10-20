# Community client fork

This checkout is the user's separately maintained community client. The user
explicitly requested AI-assisted development and publication of its source.
Upstream cybervisi0n/pokeplatinum does not accept AI-created contributions; do
not submit this fork's changes as upstream pull requests or code suggestions.

Preserve upstream provenance and third-party notices. Do not claim the original
game, upstream engine, or decompiled reference was authored by this project.
Keep Platinum Together decompilation/disassembly and all private player data
outside this repository. Ship no installed game executables, ROMs, extracted
runtime assets, saves, account keys, or private research.
Keep permission correspondence private and outside this checkout. Do not put its
text in source files, history, issues, releases, documentation or support tickets.

The modernization target is installed Platinum Together0.1.10. Retain the
pinned0.1.7 behavioral baseline while adapting verified update changes, with
the user's explicit deviations: world level caps apply regardless of original
trainer; foreign OT does not add obedience restrictions; ordinary Poké Balls
target only wild Pokémon and require exactly one remaining opponent; every
actual throw must consume a ball; caught species show the native battle marker.
A separate 0.1.10 artifact is now installed and user-authorized for comparison:
SHA256 45185bc3f1849c6b93910efef451ea70cd7a03fd81dd4733e736eae65ed0b4d8,
18,513,408 bytes. The install manifest identifies0.1.10; its PE version field
still reads0.1.7. Keep this version's evidence and extraction offsets separate
from the0.1.7 baseline. Port verified update differences with explicit tests and
compatibility changes; do not silently combine data from different versions.

Record implemented, code-inspected, built, and in-game-verified scope separately
in docs/MIGRATION_STATUS.md. Never advertise full 1:1 parity before its acceptance
matrix passes. Preserve original saves; development uses separate local profiles.

Current work is frozen at the user's2026-10-20 checkpoint request. See
`docs/CHECKPOINT_2026_10_20.md` before resuming; modernization and completed
native multiplayer acceptance remain unfinished. Do not describe this checkpoint
as a finished gameplay release.
