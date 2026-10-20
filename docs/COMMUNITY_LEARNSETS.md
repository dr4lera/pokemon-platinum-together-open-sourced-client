# Bounded learnset records

`CommunityLearnset` holds up to 20 ordered move/level records. Move IDs are 16-bit
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

Default moves retain archive order, stop at the first entry above the Pokémon's
level, and retain existing duplicate/add/replace behavior. Level-up learning scans
for exact-level entries and advances its cursor as before. The breeding export
returns ordered move IDs with duplicates and a maximum of 20; its existing DayCare
caller has 50 slots. Move Reminder retains its level filter, known-move exclusion,
first-occurrence ordering and deduplication, with a bounded output terminator.

Run `python tests/run_community_learnsets.py`. Tests compile the actual decoder,
record consumers and Move Reminder implementation against game API doubles. They
cover empty, padded, full and malformed legacy lists; duplicate/order semantics;
cursor/output bounds; known-move suppression; and synthetic 512/700/65534 move IDs.
No game assets or private reference fixtures are included. The NARC game adapter,
default-move and level-up integration compile in the complete Windows client;
in-game verification remains pending. A modern record provider, move data/effects, sprites and
species catalogs remain separate migration work.
