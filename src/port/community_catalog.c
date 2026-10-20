#include "port/community_catalog.h"
#include <string.h>

static unsigned Read16(const uint8_t *bytes)
{
    return (unsigned)bytes[0] | ((unsigned)bytes[1] << 8);
}

static uint32_t Read32(const uint8_t *bytes)
{
    return (uint32_t)Read16(bytes) | ((uint32_t)Read16(bytes + 2) << 16);
}

static uint32_t Crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
        }
    }
    return ~crc;
}

int CommunityCatalog_Decode(const uint8_t *bytes, size_t length, CommunityCatalog *out)
{
    static const uint8_t referenceHash[32] = {
        0x36,0x88,0xd6,0xb1,0xc5,0xb7,0x62,0xb9,0xa1,0xf8,0xd7,0x4b,0x64,0xf3,0xa6,0xa6,
        0x2d,0x4b,0x07,0xd5,0x25,0x07,0x4a,0x94,0xb4,0x32,0xe4,0xb0,0xb2,0x86,0x6e,0xc9
    };
    if (out == NULL) return 0;
    out->valid = 0;
    if (bytes == NULL || length != COMMUNITY_CATALOG_FILE_BYTES
        || memcmp(bytes, "PTCAT04\0", 8) != 0
        || memcmp(bytes + 8, referenceHash, sizeof(referenceHash)) != 0
        || Read32(bytes + 40) != COMMUNITY_CATALOG_PAYLOAD_BYTES
        || Crc32(bytes + COMMUNITY_CATALOG_HEADER_BYTES, COMMUNITY_CATALOG_PAYLOAD_BYTES) != Read32(bytes + 44)) {
        return 0;
    }
    const uint8_t *personal = bytes + COMMUNITY_CATALOG_HEADER_BYTES;
    const uint8_t *learnsets = personal + sizeof(out->personal);
    const uint8_t *supplements = learnsets + sizeof(out->learnsets);
    const uint8_t *types = supplements + sizeof(out->supplements);
    const uint8_t *moves = types + sizeof(out->types);
    const uint8_t *metadata = moves + sizeof(out->moves);
    const uint8_t *evolutions = metadata + sizeof(out->metadata);
    const uint8_t *spriteData = evolutions + sizeof(out->evolutions);
    const uint8_t *spriteHeights = spriteData + sizeof(out->spriteData);
    const uint8_t *iconPalettes = spriteHeights + sizeof(out->spriteHeights);
    const uint8_t *formGraphics = iconPalettes + sizeof(out->iconPalettes);
    const uint8_t *fairyMovePalette = formGraphics + sizeof(out->formGraphics);
    const uint8_t *modernMoves = fairyMovePalette + sizeof(out->fairyMovePalette);
    const uint8_t *statStages = modernMoves + sizeof(out->modernMoves);
    const uint8_t *sideEffectScripts = statStages + sizeof(out->statStages);
    const uint8_t *metronomeExclusions = sideEffectScripts + sizeof(out->sideEffectScripts);
    for (size_t i = 0; i < COMMUNITY_MODERN_MOVE_COUNT; i++) {
        const uint8_t *r = modernMoves + i * 16;
        int priority = r[10] < 128 ? r[10] : (int)r[10] - 256;
        if (Read16(r) > 409 || r[2] > 2 || r[4] > 18 || r[5] > 100 || r[6] == 0 || r[6] > 40
            || r[7] > 100 || Read16(r + 8) > 256 || priority < -6 || priority > 4
            || r[13] > 4 || r[14] || r[15]) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_STAT_STAGE_COUNT; i++) {
        const uint8_t *r = statStages + i * 12;
        if (Read16(r) < COMMUNITY_MODERN_MOVE_FIRST || Read16(r) >= COMMUNITY_MODERN_MOVE_FIRST + COMMUNITY_MODERN_MOVE_COUNT
            || r[2] > 4 || r[3] > 3 || r[11]) return 0;
        for (size_t j = 4; j < 11; j++) {
            int delta = r[j] < 128 ? r[j] : (int)r[j] - 256;
            if (delta < -2 || delta > 3) return 0;
        }
        for (size_t j = 0; j < i; j++) if (Read16(statStages + j * 12) == Read16(r)) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_SIDE_EFFECT_COUNT; i++) {
        uint32_t script = Read32(sideEffectScripts + i * 4);
        if (script < 297 || script > 338) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_METRONOME_EXCLUSION_COUNT; i++) {
        unsigned move = Read16(metronomeExclusions + i * 2);
        if (move < COMMUNITY_MODERN_MOVE_FIRST || move >= COMMUNITY_MODERN_MOVE_FIRST + COMMUNITY_MODERN_MOVE_COUNT) return 0;
        for (size_t j = 0; j < i; j++) if (Read16(metronomeExclusions + j * 2) == move) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_FAIRY_MOVE_PALETTE_BYTES; i += 2) {
        if (Read16(fairyMovePalette + i) > 0x7fff) return 0;
    }
    CommunityLearnset decoded;
    for (size_t i = 0; i < COMMUNITY_MODERN_SPECIES_COUNT; i++) {
        const uint8_t *record = personal + i * COMMUNITY_PERSONAL_BYTES;
        if (record[6] > 18 || record[7] > 18 || record[19] > 5 || record[22] > 143 || record[23] > 143) return 0;
        if (CommunityLearnset_DecodeModern(learnsets + i * COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES,
            COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES, &decoded) != COMMUNITY_LEARNSET_OK) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_SUPPLEMENT_COUNT; i++) {
        const uint8_t *record = supplements + i * 6;
        unsigned species = Read16(record), move = Read16(record + 2);
        if (species == 0 || species > 507 || move < 468 || move > 473 || record[4] > 100 || record[5] != 0) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_TYPE_OVERRIDE_COUNT; i++) {
        const uint8_t *record = types + i * 4;
        if (Read16(record) == 0 || Read16(record) > 507 || record[2] > 18 || record[3] > 18) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_MOVE_OVERRIDE_COUNT; i++) {
        const uint8_t *record = moves + i * 10;
        if (Read16(record) == 0 || Read16(record) >= 468 || (record[2] != 255 && record[2] > 18) || record[3] != 0) return 0;
        for (size_t field = 4; field <= 8; field += 2) {
            unsigned value = Read16(record + field);
            if (value != UINT16_MAX && value > 255) return 0;
        }
    }
    for (size_t i = 0; i < COMMUNITY_MODERN_SPECIES_COUNT; i++) {
        const uint8_t *record = metadata + i * COMMUNITY_SPECIES_METADATA_BYTES;
        unsigned dex = Read16(record);
        if (dex < 494 || dex > 959 || Read16(record + 2) == 0 || Read16(record + 4) == 0 || iconPalettes[i] > 2) return 0;
        for (size_t j = 0; j < i; j++) {
            if (Read16(metadata + j * COMMUNITY_SPECIES_METADATA_BYTES) == dex) return 0;
        }
        for (size_t j = 0; j < COMMUNITY_EVOLUTION_BYTES; j += 6) {
            const uint8_t *evolution = evolutions + i * COMMUNITY_EVOLUTION_BYTES + j;
            unsigned method = Read16(evolution), param = Read16(evolution + 2), target = Read16(evolution + 4);
            if (method == 0 && param == 0 && target == 0) continue;
            if (!((target > 0 && target <= 493) || (target >= 512 && target <= 576))) return 0;
            if (method == 7) {
                if (param == 0 || param > 536) return 0;
            } else if (method == 4 || method == 64 || method == 65) {
                if (param == 0 || param > 100) return 0;
            } else return 0;
        }
        /* The 89-byte record is the existing two-face sprite ABI. More
         * animations/resources still require separate compatibility review. */
        const uint8_t *sprite = spriteData + i * COMMUNITY_SPECIES_SPRITE_BYTES;
        for (size_t face = 0; face < 2; face++) {
            for (size_t frame = 0; frame < 10; frame++) {
                uint8_t id = sprite[face * 43 + 3 + frame * 4];
                if (id != 255 && id > 1) return 0;
            }
        }
        if (sprite[88] > 3) return 0;
    }
    for (size_t i = 0; i < COMMUNITY_FORM_GRAPHICS_COUNT; i++) {
        const uint8_t *record = formGraphics + i * 6;
        unsigned species = Read16(record);
        if (species < 512 || species > 576 || record[2] == 0 || record[5] != 0) return 0;
        for (size_t j = 0; j < i; j++) {
            const uint8_t *earlier = formGraphics + j * 6;
            if (Read16(earlier) == species && earlier[2] == record[2]) return 0;
        }
    }
    memcpy(out->personal, personal, sizeof(out->personal));
    memcpy(out->learnsets, learnsets, sizeof(out->learnsets));
    memcpy(out->supplements, supplements, sizeof(out->supplements));
    memcpy(out->types, types, sizeof(out->types));
    memcpy(out->moves, moves, sizeof(out->moves));
    memcpy(out->metadata, metadata, sizeof(out->metadata));
    memcpy(out->evolutions, evolutions, sizeof(out->evolutions));
    memcpy(out->spriteData, spriteData, sizeof(out->spriteData));
    memcpy(out->spriteHeights, spriteHeights, sizeof(out->spriteHeights));
    memcpy(out->iconPalettes, iconPalettes, sizeof(out->iconPalettes));
    memcpy(out->formGraphics, formGraphics, sizeof(out->formGraphics));
    memcpy(out->fairyMovePalette, fairyMovePalette, sizeof(out->fairyMovePalette));
    memcpy(out->modernMoves, modernMoves, sizeof(out->modernMoves));
    memcpy(out->statStages, statStages, sizeof(out->statStages));
    memcpy(out->sideEffectScripts, sideEffectScripts, sizeof(out->sideEffectScripts));
    memcpy(out->metronomeExclusions, metronomeExclusions, sizeof(out->metronomeExclusions));
    out->valid = 1;
    return 1;
}

int CommunityCatalog_FairyMovePalette(const CommunityCatalog *catalog, uint8_t out[COMMUNITY_FAIRY_MOVE_PALETTE_BYTES])
{
    if (catalog == NULL || !catalog->valid || out == NULL) return 0;
    memcpy(out, catalog->fairyMovePalette, COMMUNITY_FAIRY_MOVE_PALETTE_BYTES);
    return 1;
}

int CommunityCatalog_ModernMove(const CommunityCatalog *catalog, unsigned move, uint8_t out[16])
{
    if (!catalog || !catalog->valid || !out || move < COMMUNITY_MODERN_MOVE_FIRST
        || move >= COMMUNITY_MODERN_MOVE_FIRST + COMMUNITY_MODERN_MOVE_COUNT) return 0;
    memcpy(out, catalog->modernMoves[move - COMMUNITY_MODERN_MOVE_FIRST], 16);
    return 1;
}

int CommunityCatalog_StatStage(const CommunityCatalog *catalog, unsigned move, uint8_t out[12])
{
    if (!catalog || !catalog->valid || !out) return 0;
    for (size_t i = 0; i < COMMUNITY_STAT_STAGE_COUNT; i++) {
        if (Read16(catalog->statStages[i]) == move) {
            memcpy(out, catalog->statStages[i], 12);
            return 1;
        }
    }
    return 0;
}

int CommunityCatalog_SideEffectScript(const CommunityCatalog *catalog, unsigned pointer, unsigned *out)
{
    if (!catalog || !catalog->valid || !out || pointer < COMMUNITY_SIDE_EFFECT_FIRST
        || pointer >= COMMUNITY_SIDE_EFFECT_FIRST + COMMUNITY_SIDE_EFFECT_COUNT) return 0;
    *out = Read32(catalog->sideEffectScripts[pointer - COMMUNITY_SIDE_EFFECT_FIRST]);
    return 1;
}

int CommunityCatalog_MetronomeExcluded(const CommunityCatalog *catalog, unsigned move)
{
    if (!catalog || !catalog->valid) return 0;
    for (size_t i = 0; i < COMMUNITY_METRONOME_EXCLUSION_COUNT; i++) {
        if (Read16(catalog->metronomeExclusions[i]) == move) return 1;
    }
    return 0;
}

static int ModernIndex(unsigned species)
{
    return species >= COMMUNITY_MODERN_SPECIES_FIRST && species < COMMUNITY_MODERN_SPECIES_FIRST + COMMUNITY_MODERN_SPECIES_COUNT
        ? (int)(species - COMMUNITY_MODERN_SPECIES_FIRST) : -1;
}

int CommunityCatalog_Personal(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_PERSONAL_BYTES])
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || out == NULL) return 0;
    memcpy(out, catalog->personal[index], COMMUNITY_PERSONAL_BYTES);
    return 1;
}

enum CommunityLearnsetResult CommunityCatalog_Learnset(const CommunityCatalog *catalog, unsigned species, CommunityLearnset *out)
{
    if (out == NULL) return COMMUNITY_LEARNSET_INVALID;
    out->count = 0;
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0) return COMMUNITY_LEARNSET_INVALID;
    return CommunityLearnset_DecodeModern(catalog->learnsets[index], COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES, out);
}

enum CommunityLearnsetResult CommunityCatalog_Supplement(const CommunityCatalog *catalog, unsigned species, CommunityLearnset *learnset)
{
    if (catalog == NULL || !catalog->valid || learnset == NULL || learnset->count > COMMUNITY_LEARNSET_CAPACITY) return COMMUNITY_LEARNSET_INVALID;
    CommunityLearnset result = *learnset;
    for (size_t i = 0; i < COMMUNITY_SUPPLEMENT_COUNT; i++) {
        const uint8_t *record = catalog->supplements[i];
        if (Read16(record) != species) continue;
        uint16_t move = (uint16_t)Read16(record + 2);
        size_t index = 0;
        while (index < result.count && result.entries[index].move != move) index++;
        if (index != result.count) continue;
        if (result.count == COMMUNITY_LEARNSET_CAPACITY) return COMMUNITY_LEARNSET_TOO_LONG;
        index = 0;
        while (index < result.count && result.entries[index].level <= record[4]) index++;
        memmove(result.entries + index + 1, result.entries + index, (result.count - index) * sizeof(result.entries[0]));
        result.entries[index].move = move;
        result.entries[index].level = record[4];
        result.count++;
    }
    *learnset = result;
    return COMMUNITY_LEARNSET_OK;
}

int CommunityCatalog_Types(const CommunityCatalog *catalog, unsigned archiveIndex, uint8_t types[2])
{
    if (catalog == NULL || !catalog->valid || types == NULL) return 0;
    for (size_t i = 0; i < COMMUNITY_TYPE_OVERRIDE_COUNT; i++) {
        if (Read16(catalog->types[i]) == archiveIndex) {
            memcpy(types, catalog->types[i] + 2, 2);
            return 1;
        }
    }
    return 0;
}

int CommunityCatalog_Move(const CommunityCatalog *catalog, unsigned move, uint8_t record[16])
{
    if (catalog == NULL || !catalog->valid || record == NULL) return 0;
    for (size_t i = 0; i < COMMUNITY_MOVE_OVERRIDE_COUNT; i++) {
        const uint8_t *override = catalog->moves[i];
        if (Read16(override) != move) continue;
        if (override[2] != 255) record[4] = override[2];
        if (Read16(override + 4) != UINT16_MAX) record[3] = (uint8_t)Read16(override + 4);
        if (Read16(override + 6) != UINT16_MAX) record[5] = (uint8_t)Read16(override + 6);
        if (Read16(override + 8) != UINT16_MAX) record[6] = (uint8_t)Read16(override + 8);
        return 1;
    }
    return 0;
}

int CommunityCatalog_Metadata(const CommunityCatalog *catalog, unsigned species, CommunitySpeciesMetadata *out)
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || out == NULL) return 0;
    const uint8_t *record = catalog->metadata[index];
    out->nationalDex = (uint16_t)Read16(record);
    out->height = (uint16_t)Read16(record + 2);
    out->weight = (uint16_t)Read16(record + 4);
    return 1;
}

int CommunityCatalog_Evolutions(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_EVOLUTION_BYTES])
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || out == NULL) return 0;
    memcpy(out, catalog->evolutions[index], COMMUNITY_EVOLUTION_BYTES);
    return 1;
}

int CommunityCatalog_SpriteData(const CommunityCatalog *catalog, unsigned species, uint8_t out[COMMUNITY_SPECIES_SPRITE_BYTES])
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || out == NULL) return 0;
    memcpy(out, catalog->spriteData[index], COMMUNITY_SPECIES_SPRITE_BYTES);
    return 1;
}

static int FormGraphicsIndex(const CommunityCatalog *catalog, unsigned species, unsigned form)
{
    if (form != 0) {
        for (size_t i = 0; i < COMMUNITY_FORM_GRAPHICS_COUNT; i++) {
            if (Read16(catalog->formGraphics[i]) == species && catalog->formGraphics[i][2] == form) return (int)i;
        }
    }
    return -1;
}

int CommunityCatalog_SpriteHeight(const CommunityCatalog *catalog, unsigned species, unsigned form, unsigned face, uint8_t *out)
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || out == NULL) return 0;
    int formIndex = FormGraphicsIndex(catalog, species, form);
    *out = formIndex < 0 ? catalog->spriteHeights[index][face != 2] : catalog->formGraphics[formIndex][3 + (face != 2)];
    return 1;
}

int CommunityCatalog_SpriteIndex(const CommunityCatalog *catalog, unsigned species, unsigned form, unsigned *out)
{
    if (catalog == NULL || !catalog->valid || ModernIndex(species) < 0 || out == NULL) return 0;
    int formIndex = FormGraphicsIndex(catalog, species, form);
    *out = formIndex < 0 ? species : 577 + (unsigned)formIndex;
    return 1;
}

int CommunityCatalog_Icon(const CommunityCatalog *catalog, unsigned species, unsigned *archiveIndex, uint8_t *palette)
{
    int index = ModernIndex(species);
    if (catalog == NULL || !catalog->valid || index < 0 || archiveIndex == NULL || palette == NULL) return 0;
    *archiveIndex = species + 35;
    *palette = catalog->iconPalettes[index];
    return 1;
}

#ifdef PLATINUM_COMMUNITY_CLIENT
#include <stdio.h>
#include <stdlib.h>

const CommunityCatalog *CommunityCatalog_Runtime(void)
{
    static CommunityCatalog catalog;
    static int attempted;
    if (!attempted) {
        attempted = 1;
        const char *path = getenv("PLATINUM_COMMUNITY_CATALOG");
        if (path != NULL && path[0] != '\0') {
            FILE *file = fopen(path, "rb");
            if (file != NULL) {
                uint8_t *bytes = malloc(COMMUNITY_CATALOG_FILE_BYTES);
                if (bytes != NULL) {
                    size_t read = fread(bytes, 1, COMMUNITY_CATALOG_FILE_BYTES, file);
                    int extra = fgetc(file);
                    if (read == COMMUNITY_CATALOG_FILE_BYTES && extra == EOF && !ferror(file)) {
                        CommunityCatalog_Decode(bytes, read, &catalog);
                    }
                    free(bytes);
                }
                fclose(file);
            }
            if (!catalog.valid) fprintf(stderr, "Community catalog could not be validated; modern content remains disabled.\n");
        }
    }
    return catalog.valid ? &catalog : NULL;
}
#else
const CommunityCatalog *CommunityCatalog_Runtime(void)
{
    return NULL;
}
#endif
