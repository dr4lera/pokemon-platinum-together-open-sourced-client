#ifndef COMMUNITY_CATALOG_H
#define COMMUNITY_CATALOG_H

#include <stddef.h>
#include <stdint.h>
#include "port/community_learnset.h"

#define COMMUNITY_MODERN_SPECIES_FIRST 512
#define COMMUNITY_MODERN_SPECIES_COUNT 65
#define COMMUNITY_PERSONAL_BYTES 44
#define COMMUNITY_SUPPLEMENT_COUNT 32
#define COMMUNITY_TYPE_OVERRIDE_COUNT 25
#define COMMUNITY_MOVE_OVERRIDE_COUNT 70
#define COMMUNITY_SPECIES_METADATA_BYTES 6
#define COMMUNITY_EVOLUTION_BYTES 42
#define COMMUNITY_SPECIES_SPRITE_BYTES 89
#define COMMUNITY_FORM_GRAPHICS_COUNT 4
#define COMMUNITY_FAIRY_MOVE_PALETTE_BYTES 32
#define COMMUNITY_MODERN_MOVE_FIRST 474
#define COMMUNITY_MODERN_MOVE_COUNT 374
#define COMMUNITY_STAT_STAGE_COUNT 41
#define COMMUNITY_SIDE_EFFECT_FIRST 145
#define COMMUNITY_SIDE_EFFECT_COUNT 33
#define COMMUNITY_METRONOME_EXCLUSION_COUNT 34
#define COMMUNITY_CATALOG_HEADER_BYTES 48
#define COMMUNITY_CATALOG_PAYLOAD_BYTES (COMMUNITY_MODERN_SPECIES_COUNT * (COMMUNITY_PERSONAL_BYTES + COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES + COMMUNITY_SPECIES_METADATA_BYTES + COMMUNITY_EVOLUTION_BYTES + COMMUNITY_SPECIES_SPRITE_BYTES + 2 + 1) + COMMUNITY_SUPPLEMENT_COUNT * 6 + COMMUNITY_TYPE_OVERRIDE_COUNT * 4 + COMMUNITY_MOVE_OVERRIDE_COUNT * 10 + COMMUNITY_FORM_GRAPHICS_COUNT * 6 + COMMUNITY_FAIRY_MOVE_PALETTE_BYTES + COMMUNITY_MODERN_MOVE_COUNT * 16 + COMMUNITY_STAT_STAGE_COUNT * 12 + COMMUNITY_SIDE_EFFECT_COUNT * 4 + COMMUNITY_METRONOME_EXCLUSION_COUNT * 2)
#define COMMUNITY_CATALOG_FILE_BYTES (COMMUNITY_CATALOG_HEADER_BYTES + COMMUNITY_CATALOG_PAYLOAD_BYTES)

typedef struct CommunityCatalog {
    int valid;
    uint8_t personal[COMMUNITY_MODERN_SPECIES_COUNT][COMMUNITY_PERSONAL_BYTES];
    uint8_t learnsets[COMMUNITY_MODERN_SPECIES_COUNT][COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES];
    uint8_t supplements[COMMUNITY_SUPPLEMENT_COUNT][6];
    uint8_t types[COMMUNITY_TYPE_OVERRIDE_COUNT][4];
    uint8_t moves[COMMUNITY_MOVE_OVERRIDE_COUNT][10];
    uint8_t metadata[COMMUNITY_MODERN_SPECIES_COUNT][COMMUNITY_SPECIES_METADATA_BYTES];
    uint8_t evolutions[COMMUNITY_MODERN_SPECIES_COUNT][COMMUNITY_EVOLUTION_BYTES];
    uint8_t spriteData[COMMUNITY_MODERN_SPECIES_COUNT][COMMUNITY_SPECIES_SPRITE_BYTES];
    uint8_t spriteHeights[COMMUNITY_MODERN_SPECIES_COUNT][2];
    uint8_t iconPalettes[COMMUNITY_MODERN_SPECIES_COUNT];
    uint8_t formGraphics[COMMUNITY_FORM_GRAPHICS_COUNT][6];
    uint8_t fairyMovePalette[COMMUNITY_FAIRY_MOVE_PALETTE_BYTES];
    uint8_t modernMoves[COMMUNITY_MODERN_MOVE_COUNT][16];
    uint8_t statStages[COMMUNITY_STAT_STAGE_COUNT][12];
    uint8_t sideEffectScripts[COMMUNITY_SIDE_EFFECT_COUNT][4];
    uint8_t metronomeExclusions[COMMUNITY_METRONOME_EXCLUSION_COUNT][2];
} CommunityCatalog;

typedef struct CommunitySpeciesMetadata {
    uint16_t nationalDex;
    uint16_t height;
    uint16_t weight;
} CommunitySpeciesMetadata;

/* The cache is generated locally from a version-pinned user-supplied reference.
 * CRC detects damage, not malicious modification. No executable is loaded. */
int CommunityCatalog_Decode(const uint8_t *bytes, size_t length, CommunityCatalog *out);
int CommunityCatalog_Personal(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_PERSONAL_BYTES]);
enum CommunityLearnsetResult CommunityCatalog_Learnset(const CommunityCatalog *catalog, unsigned species, CommunityLearnset *out);
/* Native insertion is stable after all entries at the same level, skips move
 * IDs already present, and preserves supplemental table order. */
enum CommunityLearnsetResult CommunityCatalog_Supplement(const CommunityCatalog *catalog, unsigned species, CommunityLearnset *learnset);
int CommunityCatalog_Types(const CommunityCatalog *catalog, unsigned archiveIndex, uint8_t types[2]);
/* Applies only the native type/power/accuracy/PP override fields to a 16-byte
 * move record; type255 and signed -1 preserve the existing field. */
int CommunityCatalog_Move(const CommunityCatalog *catalog, unsigned move, uint8_t record[16]);
/* Pointer-free numeric data only. These accessors do not register species,
 * expand save layouts or make unreviewed resources safe for the engine. */
int CommunityCatalog_Metadata(const CommunityCatalog *catalog, unsigned species, CommunitySpeciesMetadata *out);
int CommunityCatalog_Evolutions(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_EVOLUTION_BYTES]);
int CommunityCatalog_SpriteData(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_SPECIES_SPRITE_BYTES]);
int CommunityCatalog_SpriteHeight(const CommunityCatalog *catalog, unsigned species, unsigned form, unsigned face, uint8_t *out);
int CommunityCatalog_SpriteIndex(const CommunityCatalog *catalog, unsigned species, unsigned form, unsigned *out);
int CommunityCatalog_Icon(const CommunityCatalog *catalog, unsigned species, unsigned *archiveIndex, uint8_t *palette);
int CommunityCatalog_FairyMovePalette(const CommunityCatalog *catalog, uint8_t out[COMMUNITY_FAIRY_MOVE_PALETTE_BYTES]);
/* Numeric metadata only: these do not admit new scripts or expand MAX_MOVES.
 * Full move-specific engine behavior must be reviewed separately. */
int CommunityCatalog_ModernMove(const CommunityCatalog *catalog, unsigned move, uint8_t out[16]);
int CommunityCatalog_StatStage(const CommunityCatalog *catalog, unsigned move, uint8_t out[12]);
int CommunityCatalog_SideEffectScript(const CommunityCatalog *catalog, unsigned pointer, unsigned *out);
int CommunityCatalog_MetronomeExcluded(const CommunityCatalog *catalog, unsigned move);

/* Game-thread-only, lazy, immutable. Explicit file selection; no automatic
 * discovery of an installed game or access to its save/profile directories. */
const CommunityCatalog *CommunityCatalog_Runtime(void);
#endif
