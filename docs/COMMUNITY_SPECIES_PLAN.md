# Modern species integration plan

The inspected 0.1.7 catalog contains 65 internal species IDs, 512 through 576.
They map to 65 distinct, sparse national numbers between 494 and 959. Internal
IDs, national numbers, personal-form indices and graphics slots are separate
namespaces. This document records source integration work, not playable-species
or complete reference parity. Exact mappings, reference strings and extracted
assets remain private.

## Available data and boundaries

The private importer now creates a pointer-free V3 catalog with personal data
(65 x44), modern learnsets (65 x260), numeric national/height/weight metadata
(65 x6), evolution data (65 x42), sprite animation/shadow data (65 x89), two-face
sprite heights (65 x2), icon palettes (65 bytes) and four graphics-form records
(4 x6). Existing 32 learnset supplements, 25 type overrides and 70 move override
rows are retained. Every source table is independently hash-pinned after the
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
A later restricted private merge can preserve source-generated original members
and add the reviewed extension; it must validate archive/member bounds and the
unchanged prefix before writing a fresh runtime. Current asset merge/overlay
support does not authorize these species archives automatically.

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
| Sprite templates | `src/pokemon.c`: `BuildPokemonSpriteTemplate`, `BuildPokemonSpriteTemplateDP` | Use catalog graphics slot then six-member formula; modern requests using the DP path need the reviewed Platinum archive path. Keep shiny/gender/front/back indexing. |
| Sprite vertical position | `LoadPokemonSpriteYOffset` and its DP sibling in `src/pokemon.c` | Use catalog two-face/form height before base height-NARC access; do not index base resources with512+. |
| Sprite motion/shadow | Six `PokemonSprite_Load*` functions in `src/pokemon.c`; `include/struct_defs/species_sprite_data.h` | Read89-byte modern data before legacy species-offset reads. Two43-byte faces each contain ten frames; final fields are signed Y/shadow-X and unsigned shadow-size. Animation IDs still need resource/engine compatibility validation. |
| Icons | `src/pokemon_icon.c`: `PokeIconSpriteIndex`, `PokeIconPaletteIndex`, `BoxPokemon_IconFormOffset` | Original functions currently map IDs>493 to none. Add bounded icon provider after egg handling; modern forms share species icon unless independently proved otherwise. |
| Evolution | `src/pokemon.c`: `LoadSpeciesEvolutions`, `Pokemon_GetEvolutionTargetSpecies`; seven-slot `SpeciesEvolution` in `include/struct_defs/species.h` | Gated provider now precedes archive reads. Observed methods64/65 and separate priority friendship/Fairy rule have isolated actual-consumer tests; original item/level/everstone handling and legacy order are retained. Resulting species still needs the remaining rendering/save/behavior work. |
| Base/baby species | `Pokemon_GetBaseSpeciesFromPersonalData`, `Pokemon_GetBaseSpecies` in `src/pokemon.c`; `pms.narc` | Existing assert/table stop at493. Exact modern breeding/base-species behavior is not yet pinned; do not infer it from evolution rows. |
| Dex labels | `src/applications/pokedex/pokedex_text.c` | English text is indexed by internal species; national display needs explicit sparse mapping. Foreign-language banks and guards remain a separate unsupported route. |
| Dex size/weight | `src/pokedex_heightweight.c`: height/weight plus four comparison-position/scale getters | Base arrays cannot be indexed with512+. Numeric metadata alone does not supply comparison positions/scales or prove all conversion units. |
| Dex recording/save | `include/pokedex.h`; `src/pokedex.c`: `SpeciesInvalid`, encounter/capture/query functions | Original fixed arrays and recorded-language storage are part of save ABI. Choose versioned sidecar or an explicit migration; preserve original saves and caught-versus-seen state. |
| Dex lists/area | `src/applications/pokedex/pokedex_sort.c`, `pokedex_enc_data.c`, generated sort/filter resources | Current allocations use493 and encounter archive group offsets use `MAX_SPECIES`. A global increase would shift resource addressing; add explicit lists and reviewed area data. |
| Cries | `src/pokemon.c` cry entry points and `src/sound*.c` | Reference maps modern species to65 additional wave IDs771..835. Private sound resource import and playback mapping remain separate work. |

## Dependency order and acceptance

1. Validate V3 data and early personal/learnset adapters in isolated tests. This
   stage is implemented; all other consumers still require bounded routing.
2. Review modern move/ability consumers before creating these species: current
   engine move IDs stop at473 and ability IDs at123. Provider data can refer to
   later behaviors and does not make those behaviors available.
3. Evolution consumers now use the saved overworld weather2..4, minimum-level
   non-night evaluation and separate priority friendship/Fairy rule. Actual
   consumer fixtures pass; runtime verification remains pending. Seven legacy
   slots remain unchanged.
4. Add sprite/icon/height/animation-data routing and a narrow private archive
   merger. Validate original prefix preservation and each referenced resource,
   then verify one isolated modern species in summary, party and battle views.
5. Validate and select encoded species text banks; preserve egg handling and
   original-language fallback. Add numeric national display separately.
6. Settle modern dex persistence and base/baby-species behavior before capturing,
   trading, breeding or saving modern species in normal profiles. Test original
   save compatibility and exact seen/caught updates independently.
7. Expand progression, encounters, forms, cries, area/sort data and multiplayer
   provenance only from pinned behavior. Complete a per-species acceptance matrix
   before advertising all65 as playable or claiming1:1 behavior.

Next independently testable unit is bounded sprite/icon/animation-data routing
plus a private archive compatibility validator. Modern species registration
remains blocked on the remaining engine
and resource work, even when the explicit development gate is selected.
