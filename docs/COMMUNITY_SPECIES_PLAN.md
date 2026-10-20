# Modern species integration plan

The inspected 0.1.7 catalog contains 65 internal species IDs, 512 through 576.
They map to 65 distinct, sparse national numbers between 494 and 959. Internal
IDs, national numbers, personal-form indices and graphics slots are separate
namespaces. This document records source integration work, not playable-species
or complete reference parity. Exact mappings, reference strings and extracted
assets remain private.

## Available data and boundaries

The private importer now creates a pointer-free V4 catalog with personal data
(65 x44), modern learnsets (65 x260), numeric national/height/weight metadata
(65 x6), evolution data (65 x42), sprite animation/shadow data (65 x89), two-face
sprite heights (65 x2), icon palettes (65 bytes) and four graphics-form records
(4 x6), plus the32-byte Fairy move-menu palette. Existing 32 learnset supplements,
25 type overrides and 70 move override rows are retained. V4 uses a48-byte header
and36,584-byte payload; V1/V2/V3 caches must be regenerated. Every source table is
independently hash-pinned after the
whole reference artifact is verified. No table or string catalog is shipped.

The seven-slot evolution table has 33 nonzero records, using methods 4, 7, 64
and 65. The additional legacy friendship/Fairy-move evolution is a separate
priority rule, not an eighth slot in every species record. Graphics have four
extra form slots, 577 through 580; these must not replace mechanical personal
form indices 496 through 507 or change egg IDs 494 and 495.

| Resource | Base count | Supplied patched count | Source contract |
| --- | ---: | ---: | --- |
| `poketool/pokegra/pl_pokegra.narc` | 2964 | 3486 | Six members per graphics slot; original 494 slots preserved, final slot580 |
| `poketool/icongra/pl_poke_icon.narc` | 547 | 612 | Modern species icon member is internal ID+35; palette0..2 comes from catalog |
| `poketool/personal/evo.narc` | 508 | No patched archive | Modern evolution records require the compiled-data provider |
| `msgdata/pl_msg.narc` | 724 banks | 724 banks | Relevant banks expand by message count, not bank count |

Private comparison found every original sprite/icon archive member unchanged.
The restricted private merger now preserves source-generated original members
and adds the reviewed extension. It checks both whole-archive hashes, unchanged
prefixes, resource framing and output round trips before creating a fresh output.
The108 reserved empty sprite members for IDs494..511 are retained, followed by
276 character and138 palette members for65 species and four graphics forms.
Icons append65 character members. No artwork is included with the tool. Current
overlay support does not authorize these species archives automatically.

Patched species-name bank412 and article-name bank413 have577 entries, versus
496 in the base. Category711, English dex-entry706, weight707 and height709 have
577 entries, versus494. An actual source `msgenc` decode checked all65 names and
categories privately. All categories match numeric-table labels; four displayed
names differ from merely uppercasing those labels. Text banks are authoritative:
do not synthesize their spelling or cast UTF-8 metadata into DS character codes.

## Source integration inventory

| Layer | Exact source hooks | Status or required change |
| --- | --- | --- |
| Personal | `src/pokemon.c`: `SpeciesData_LoadSpecies`, `SpeciesData_LoadForm`; `include/struct_defs/species.h`:44-byte `SpeciesData` | Gated modern provider now precedes legacy NARC reads; keep original form mapping and type overrides. Special modern form mechanics still need separate adapters. |
| Learnsets | `src/pokemon.c`: `Pokemon_LoadCommunityLearnsetOf`; `src/port/community_learnset.c`; `src/move_reminder_data.c` | Gated modern provider now precedes legacy member bounds/read; API32, legacy20, native storage64 records+sentinel remain distinct. Reminder33 and breeding50 buffers are bounded. |
| Identifiers | `generated/species.txt`; `include/constants/species.h`; `Pokemon_GetFormNarcIndex`, `Pokemon_SanitizeFormId` | Base0..493, eggs494/495 and original form indices496..507 must retain values. Add a bounded modern-validity route instead of enlarging all global constants. |
| Names | `src/message_util.c`: `MessageUtil_SpeciesName`; `src/message.c`: `MessageLoader_GetSpeciesName`; `src/string_template.c`: species/article helpers | Require validated577-entry banks412/413, preserving encoded glyphs, original names and nickname capacities. |
| Sprite templates | `src/pokemon.c`: `BuildCommunityPokemonSpriteTemplate`, `BuildPokemonSpriteTemplate`, `BuildPokemonSpriteTemplateDP` | Gated catalog graphics slots now use the reviewed Platinum archive and six-member formula for both paths. Actual-body tests cover gender/front/back/shiny and four graphics forms; legacy egg/Spinda/form behavior is retained. |
| Sprite vertical position | `LoadPokemonSpriteYOffset`, `LoadPokemonDPSpriteHeight` in `src/pokemon.c` | Gated catalog two-face/form heights now precede base NARC access. Actual-body tests cover all65 and four forms. |
| Sprite motion/shadow | Shared `LoadPokemonSpriteData` and six `PokemonSprite_Load*` functions in `src/pokemon.c`; `include/struct_defs/species_sprite_data.h` | Gated89-byte modern data now precedes legacy reads. Two43-byte faces retain ten frames; signed Y/shadow-X and unsigned shadow-size are preserved. Observed animations use existing143-member `pokeanime/pl_poke_anm.narc`; no new animation scripts are imported. |
| Icons | `src/pokemon_icon.c`: `PokeIconSpriteIndex`, `PokeIconPaletteIndex`, `BoxPokemon_IconFormOffset` | Bounded icon/palette provider now follows egg handling. Modern forms share the base species icon. Unknown/disabled/invalid catalog requests retain legacy routes, including icon none fallback. Mail's separate stored-icon clamp remains outside this change. |
| Evolution | `src/pokemon.c`: `LoadSpeciesEvolutions`, `Pokemon_GetEvolutionTargetSpecies`; seven-slot `SpeciesEvolution` in `include/struct_defs/species.h` | Gated provider now precedes archive reads. Observed methods64/65 and separate priority friendship/Fairy rule have isolated actual-consumer tests; original item/level/everstone handling and legacy order are retained. Resulting species still needs the remaining rendering/save/behavior work. |
| Base/baby species | `Pokemon_GetBaseSpeciesFromPersonalData`, `Pokemon_GetBaseSpecies` in `src/pokemon.c`; `pms.narc` | Existing assert/table stop at493. Exact modern breeding/base-species behavior is not yet pinned; do not infer it from evolution rows. |
| Dex labels | `src/applications/pokedex/pokedex_text.c` | English text is indexed by internal species; national display needs explicit sparse mapping. Foreign-language banks and guards remain a separate unsupported route. |
| Dex size/weight | `src/pokedex_heightweight.c`: height/weight plus four comparison-position/scale getters | Base arrays cannot be indexed with512+. Numeric metadata alone does not supply comparison positions/scales or prove all conversion units. |
| Dex recording/save | `include/pokedex.h`; `src/pokedex.c`: `SpeciesInvalid`, encounter/capture/query functions | Original fixed arrays and recorded-language storage are part of save ABI. Choose versioned sidecar or an explicit migration; preserve original saves and caught-versus-seen state. |
| Dex lists/area | `src/applications/pokedex/pokedex_sort.c`, `pokedex_enc_data.c`, generated sort/filter resources | Current allocations use493 and encounter archive group offsets use `MAX_SPECIES`. A global increase would shift resource addressing; add explicit lists and reviewed area data. |
| Cries | `src/pokemon.c` cry entry points and `src/sound*.c` | Reference maps modern species to65 additional wave IDs771..835. Private sound resource import and playback mapping remain separate work. |

## Dependency order and acceptance

1. Validate V4 data and early personal/learnset adapters in isolated tests. This
   stage is implemented. Data availability does not register modern species.
2. Review modern move/ability consumers before creating these species: current
   engine move IDs stop at473 and ability IDs at123. Provider data can refer to
   later behaviors and does not make those behaviors available.
3. Evolution consumers now use the saved overworld weather2..4, minimum-level
   non-night evaluation and separate priority friendship/Fairy rule. Actual
   consumer fixtures pass; runtime verification remains pending. Seven legacy
   slots remain unchanged.
4. Sprite/icon/height/animation-data routing and the private append merger now
   pass isolated consumer/resource tests. Verify one isolated modern species in
   summary, party and battle views after the shared graphics-loader lifetime and
   sparse-palette work is reviewed; do not infer runtime rendering from these
   tests alone.
5. Validate and select encoded species text banks; preserve egg handling and
   original-language fallback. Add numeric national display separately.
6. Settle modern dex persistence and base/baby-species behavior before capturing,
   trading, breeding or saving modern species in normal profiles. Test original
   save compatibility and exact seen/caught updates independently.
7. Expand progression, encounters, forms, cries, area/sort data and multiplayer
   provenance only from pinned behavior. Complete a per-species acceptance matrix
   before advertising all65 as playable or claiming1:1 behavior.

## Private graphics preparation and tests

```powershell
python tools/community/prepare-species-assets.py D:/PrivateSpeciesAssets --base-runtime D:/SourceBuiltRuntime --reference-root D:/OwnedReferenceGame
python tests/run_community_species_graphics.py
```

The destination must be fresh and outside this checkout and both inputs. The
tool reads only its two fixed archive paths plus the source runtime's unchanged
sprite-animation dependency. It emits two merged archives and
`PRIVATE-SPECIES-ASSETS.json`, with hashes, counts and pending runtime status.
It neither copies the entire reference patch tree nor modifies originals.
The sprite extension uses160x80 four-bit bitmap NCGR records with6400-byte raw
data and32-byte palettes; icons use1024-byte one-dimensional character records.
Dimensions, versions, serialized offsets and block/file sizes are checked.

The asset-free archive fixtures cover malformed/duplicate/out-of-range NARC
tables, intentional empty members, wrong hashes, changed original prefixes,
malformed character/palette bounds, output confinement, immutability and refusal
to overwrite. Synthetic fixture hashes are installed only inside the test
module; the production CLI has no hash override. `--archives-only` runs those
checks without generated build headers. The full runner also compiles actual
consumer bodies with the source-generated headers from `build_win64` (or
`COMMUNITY_TEST_BUILD`). Optional `--reference-root PATH` validates both pinned
real archives entirely in memory, without writing assets to the checkout.

Remaining shared graphics work includes the SDK_PORT sparse-palette loader and
the tiled-sprite data lifetime in `src/character_sprite.c`; numeric catalog data
cannot repair those library/lifetime issues. Modern species registration remains
blocked on the remaining engine, save, text, move/ability and resource work,
even when the explicit development gate is selected. No all-roster rendering or
playability claim is made by this unit.

## Private text resource merge

`prepare-species-text.py` selects only banks412/413/706/707/709/711 from the
pinned user-supplied archive. Each selected source/reference bank is hash checked;
the six replacement banks must contain577 bounded, terminated encoded entries.
All original decoded entries must match, even with a different encryption seed.
All other source-generated banks remain byte-for-byte unchanged, including the
community move/type text. Output is a fresh private directory with a hash manifest.
No text, archive or spelling catalog is published. The synthetic fixture rejects
truncation, entry overlap, oversized/unterminated strings, changed original text
and hash mismatches. Local exact-input validation also passed. This establishes
resource preparation, not completed summary/dex runtime or modern dex persistence.
