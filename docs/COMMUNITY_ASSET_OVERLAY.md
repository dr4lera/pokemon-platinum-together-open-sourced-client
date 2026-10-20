# Private runtime asset overlay

The desktop SDK 4 filesystem opener accepts an explicit absolute runtime directory
through `PLATINUM_COMMUNITY_ASSET_ROOT`. Set it before launching the community
client. It is the directory containing the user's own extracted resources and
optional `patch` directory. The repository supplies no game files and does not
download, extract, modify or publish the selected directory.

With that setting, resource lookup tries the private `patch/<resource>` first,
then the private base resource, then the ordinary working-directory base resource.
Each tier retains original spelling followed by lowercase fallback. An absent,
empty, relative or inaccessible root leaves working-directory base lookup
available. With no setting, there is no implicit working-directory `patch` lookup.
The normal missing-file extraction dialog and retry remain in place.

Only these private patch resources are currently admitted:

- `poketool/personal/pl_personal.narc`
- `poketool/personal/wotbl.narc`
- `msgdata/pl_msg.narc`
- `battle/graphic/pl_batt_obj.narc`

These are the inspected personal-data, legacy learnset, text and battle-icon
boundaries. A resource name in this list does not validate the contents of an
arbitrary supplied archive. Existing game parsers remain responsible for archive
contents. This resolver adds no modern species, Fairy mechanics, learnset
supplements, move catalog or UI behavior by itself. Whole text/icon archives can
contain additional unused entries; their presence does not implement those features.

Battle move/effect/subscript archives, animations and all other private patch
paths are skipped. Ordinary base lookup still works for them. In particular,
`battle/skill/*.narc` and `wazaeffect/we.arc` remain outside the patch allowlist
until their commands and required resources are supported by the source engine.

Resource paths must be bounded relative ASCII game paths. The `rom:` prefix and
its optional single slash are recognized. Absolute paths, other schemes, drive
prefixes, backslashes, empty components, traversal, control characters and trailing
dots are rejected. A direct `patch/` resource prefix is also rejected so callers
cannot bypass the patch allowlist through ordinary base lookup. Invalid resource paths return failure without opening the
extraction dialog. Private root reads are confined using final opened-handle
paths on Windows; POSIX walks directory handles and rejects relative symlinks.
This confinement applies to private root lookup. Existing working-directory base
files remain the user's normal runtime input. SDK 5, Switch and ARM filesystem behavior
are unchanged.

Run `python tests/run_community_asset_overlay.py`. It compiles the actual resolver
and the actual `FS_OpenFile` body with small SDK/UI doubles, and uses temporary
original text fixtures, not game assets. Checks cover normalization, patch/base
preference, case fallback, excluded battle patches, invalid roots, invalid paths,
file metadata and extraction retry. Symlink confinement runs where the host can
create symlinks; unsupported host privileges are reported explicitly. A complete
client build and in-game resource verification are separate checks.
