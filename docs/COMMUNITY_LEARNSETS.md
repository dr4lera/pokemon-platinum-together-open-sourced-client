# Bounded learnset records

`CommunityLearnset` holds up to 32 ordered move/level records. Move IDs are 16-bit
fields, separate from their levels; records can therefore represent IDs above 511.
This is an API capability, not a modern catalog or evidence that added moves work.
The existing species, save structures, archive indices and NARC files are unchanged.

The legacy decoder retains the original 9-bit move / 7-bit level interpretation,
including level 0 and duplicate entries. It requires a terminator within 20 records
plus one terminator word. Any padding after the first terminator is ignored;
missing terminators or excess records expose an empty result. The game adapter
checks member size, permits the legacy archive's alignment padding, and bounds
its read before decoding. Its owned archive handle closes on success and every
rejection path, including ARM builds. The old packed loader remains for source compatibility;
the engine consumers below now use the bounded record loader.

The separate modern byte decoder accepts the inspected reference storage format:
up to 65 little-endian 32-bit words, with move ID in the low 16 bits and level in
the high 16 bits. The first `0xffffffff` terminates the list; any remaining words
must be zero padding. Native storage has room for 64 records plus the sentinel;
the installed 0.1.7 modern catalog actually contains at most 23 records per
species. Our API capacity of 32 covers that observed maximum without claiming
support for every possible native storage slot. Lists over 32, invalid field
values, malformed byte lengths, missing sentinels or nonzero padding expose an
empty result. Levels retain the API's 0–127 range. This decoder is not connected
to the game's species loader: gameplay still loads only the legacy NARC format.

Default moves retain archive order, stop at the first entry above the Pokémon's
level, and retain existing duplicate/add/replace behavior. Level-up learning scans
for exact-level entries and advances its cursor as before. The breeding export
returns ordered move IDs with duplicates and a maximum of 32; its existing DayCare
caller has 50 slots. Move Reminder retains its level filter, known-move exclusion,
first-occurrence ordering and deduplication, with space for 32 records plus a
bounded output terminator. The legacy decoder remains independently limited to
20 records and the NARC member/read bounds are unchanged.

Run `python tests/run_community_learnsets.py`. Tests compile the actual decoder,
record consumers and Move Reminder implementation against game API doubles. They
cover empty, padded, full and malformed legacy and modern lists; rejection of a
21st legacy record; duplicate/order semantics; cursor/output bounds; known-move
suppression; actual Move Reminder output for 23 and 32 synthetic modern records;
and synthetic 512/700/65534 move IDs.
No game assets or private reference fixtures are included. The NARC game adapter,
default-move and level-up integration compile in the complete Windows client;
in-game verification remains pending. A modern record provider, move data/effects, sprites and
species catalogs remain separate migration work.
