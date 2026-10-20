# Local content catalog

The original community data provider reads a bounded 36,656-byte `PTCAT05`
runtime cache. Generate it from a user-supplied, hash-pinned 0.1.7 executable and explicit patched move archive:

```powershell
python tools/community/import-reference-catalog.py D:/Reference/Platinum.exe D:/PrivateRuntime/catalog.ptc --move-archive D:/Reference/patch/poketool/waza/pl_waza_tbl.narc
$env:PLATINUM_COMMUNITY_CATALOG = 'D:/PrivateRuntime/catalog.ptc'
```

Output must stay outside this source checkout. The importer refuses another
artifact/version, changed table hashes and an existing output file. It reads
the executable as data and never executes or patches it. No original data is
included in this repository or its CI fixtures.

V5 deliberately rejects V1/V2/V3 caches; regenerate the private file with the current
importer. The 48-byte header carries the selected artifact hash, exact payload
length and CRC. The 36,608-byte payload retains the five original arrays, then
stores 65 numeric metadata records (three little-endian u16 fields: national
number, height and weight), 65 seven-slot evolution records (42 bytes each),
65 sprite-animation records (89 bytes each), 65 two-face height records,
65 icon palette bytes, four six-byte graphics-form records and the32-byte
Fairy move-menu palette. Metadata process
pointers, names, categories and descriptions are excluded entirely.

The cache carries 65 modern personal records and learnsets, 32 supplemental
legacy learnset entries, 25 species/form type overrides and 70 move-property
override rows. The latter contain all observed 3 type, 51 power, 24 accuracy and
13 PP changes; unchanged sentinel fields preserve the base entry. Runtime code
checks format, selected-artifact identity, exact length, CRC and bounded record
structure before exposing any catalog. CRC detects damage and is not a signature.

Legacy supplements are inserted after existing records at the same level,
preserving table order and skipping already-known move IDs. Capacity failures
leave the original list unchanged. For example, Clefairy gains Disarming Voice
at level 1 and Moonblast at level 46. Archive form indices are retained for the
legacy type/supplement lookups.

Modern legacy-rule support is an explicit development setting:

```powershell
$env:PLATINUM_COMMUNITY_MODERN_RULES = '1'
```

It requires a valid selected cache and is frozen on its first game-thread query.
The gate changes the legacy type chart, personal types, base move properties
and learnset supplements together. It defaults off. This is a local development
setting pending per-world rules and multiplayer negotiation.

The 65-species personal/learnset API now bypasses legacy archives before any
out-of-range read when the development gate is enabled. Numeric metadata,
evolution and sprite/icon APIs are available and checked against the private
reference. Evolution loading now bypasses legacy archives for modern IDs. Normal
level-up evaluation adds the observed minimum-level/non-night and saved-overworld
rain rules. The separate priority Eevee rule requires friendship160 and a known
Fairy move, after Everstone checks; other evolution classes retain legacy rules.
Their complete name, dex, sprite and battle integration is
still pending. In particular, graphics accessors do not load archives and modern
learnsets may name moves beyond the implemented engine catalog. Merely selecting
this cache does not enable complete modern
Pokémon support. See [bounded Fairy support](COMMUNITY_FAIRY_MOVES.md) for its
separate resource and animation requirements.

Run `python tests/run_community_catalog.py` for asset-free parser, bounds,
override and insertion checks. The optional `--reference PATH` additionally
checks every imported record and applied override against the selected private
reference; `--private-workspace PATH` chooses an existing temporary-data parent
outside the checkout when the host restricts its normal temporary directory.
`python tests/run_community_species_loaders.py` checks the actual personal and
learnset consumer bodies: all 65 modern IDs avoid base archive access; original
form indices, disabled-gate behavior and archive lifetimes remain intact.
`python tests/run_community_evolutions.py` executes the actual evolution bodies
with synthetic state, checking weather2..4, day/night and level boundaries,
priority, friendship159/160, Everstone, item evolution, all seven legacy slots,
disabled gating and all65 modern evolution archive bypasses. These checks do not
establish that the resulting modern species is safe to create/save or render.

V5 appends374 sixteen-byte move metadata records (IDs474..847),41 twelve-byte
stat-stage records,33 new side-effect script routes and34 Metronome exclusions.
The archive and compiled tables are independently hash-pinned; reserved archive
rows848..850 are excluded. The runtime parser validates effect/class/type,
accuracy/PP/chance/range/signed-priority/padding, unique bounded stat rows,
side-effect script IDs and unique exclusion IDs. Accessors leave destinations
unchanged when refusing invalid inputs. Wireless compatibility includes every
new array. These providers do not expand `MAX_MOVES`, load new scripts or enable
unreviewed moves. Native move-specific state, damage and lifecycle hooks remain
separate work; reusing an old effect script alone does not establish parity.

V5 appends three8-byte numeric form-stat records (species/form and five non-HP
base stats). Exact keys must have existing graphics mappings; zero forms/stats,
duplicates and missing mappings are refused. The accessor changes its five-byte
output only on an exact valid match. V1–V4 caches are rejected. This metadata
does not by itself enable a battle form or its lifecycle.
