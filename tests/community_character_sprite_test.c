/* SPDX-License-Identifier: GPL-3.0-or-later
 * Actual character_sprite.c with synthetic archive/decryption primitives. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDK_ARM9 1
#define NNS_FROM_TOOL 1
#include <nitro.h>
#include <nnsys/g2d/fmt/g2d_Character_data.h>
#include "character_sprite.h"
#include "constants/graphics.h"
#define POKEPLATINUM_HEAP_H
#define POKEPLATINUM_NARC_H
#define POKEPLATINUM_POKEMON_H
#define POKEPLATINUM_POKEMON_SPRITE_H
#define FACE_FRONT 2

typedef struct Allocation { void *ptr; size_t bytes; BOOL live; } Allocation;
typedef struct Archive { NNSG2dCharacterData data; u8 raw[6400]; } Archive;
static Allocation allocations[32];
static int allocationCount, allocationAttempt, failAllocation;
static Archive *archive;
static BOOL archiveLive, decrypted, spotted;
static int narcFailure, decodeFailure, badData, decryptCount, spotCount, rawCopies;

static void *Heap_Alloc(enum HeapID heap, u32 bytes)
{
    (void)heap;
    if (++allocationAttempt == failAllocation) return NULL;
    assert(allocationCount < 32);
    void *ptr = malloc(bytes);
    assert(ptr);
    allocations[allocationCount++] = (Allocation){ptr, bytes, TRUE};
    memset(ptr, 0xCC, bytes);
    return ptr;
}
static void Heap_Free(void *ptr)
{
    assert(ptr);
    for (int i = 0; i < allocationCount; ++i) {
        if (allocations[i].ptr == ptr) {
            assert(allocations[i].live);
            allocations[i].live = FALSE;
            if (ptr == archive) archiveLive = FALSE;
            /* Quarantine storage: subsequent uses must fail our explicit live
             * assertion, rather than relying on allocator-specific reuse. */
            memset(ptr, 0xDD, allocations[i].bytes);
            return;
        }
    }
    assert(!"unknown allocation");
}
static void *NARC_AllocAndReadWholeMemberByIndexPair(enum NarcID narc, int idx, enum HeapID heap)
{
    (void)narc; (void)idx;
    if (narcFailure) return NULL;
    archive = Heap_Alloc(heap, sizeof(*archive));
    if (!archive) return NULL;
    archiveLive = TRUE;
    archive->data.W = 20;
    archive->data.H = 10;
    archive->data.szByte = badData == 2 ? 6399 : sizeof(archive->raw);
    archive->data.pRawData = badData == 1 ? NULL : archive->raw;
    for (unsigned i = 0; i < sizeof(archive->raw); ++i) archive->raw[i] = (u8)(i * 13 + i / 17);
    return archive;
}
static BOOL NNS_G2dGetUnpackedCharacterData(void *file, NNSG2dCharacterData **out)
{
    assert(file == archive && archiveLive);
    *out = badData == 3 ? NULL : &archive->data;
    return !decodeFailure;
}
static BOOL NNS_G2dGetUnpackedPaletteData(void *file, NNSG2dPaletteData **out)
{
    (void)file; (void)out;
    assert(!"palette entry point is outside this fixture");
    return FALSE;
}
static void PokemonSprite_Decrypt(u8 *data, enum NarcID narc)
{
    (void)narc;
    assert(archiveLive && data == archive->raw);
    assert(!decrypted);
    decrypted = TRUE;
    ++decryptCount;
    for (unsigned i = 0; i < sizeof(archive->raw); ++i) data[i] ^= 0xA5;
}
static void PokemonSprite_DrawSpindaSpots(u8 *data, u32 personality, BOOL animated)
{
    assert(archiveLive && decrypted && data == archive->raw);
    spotted = TRUE;
    ++spotCount;
    data[0] ^= (u8)personality;
    if (animated) data[40] ^= (u8)personality;
}
static void *TraceCopy(void *dest, const void *src, size_t bytes)
{
    uintptr_t source = (uintptr_t)src;
    if (archive && source >= (uintptr_t)archive->raw && source < (uintptr_t)archive->raw + sizeof(archive->raw)) {
        assert(archiveLive && decrypted);
        assert(source + bytes <= (uintptr_t)archive->raw + sizeof(archive->raw));
        ++rawCopies;
    }
    return memcpy(dest, src, bytes);
}
#define MI_CpuCopy16(source,dest,size) TraceCopy(dest,source,size)
#define memcpy TraceCopy
#include CHARACTER_SPRITE_SOURCE
#undef memcpy

static void Reset(void)
{
    for (int i = 0; i < allocationCount; ++i) {
        assert(!allocations[i].live);
        free(allocations[i].ptr);
    }
    allocationCount = allocationAttempt = failAllocation = 0;
    archive = NULL;
    archiveLive = decrypted = spotted = FALSE;
    narcFailure = decodeFailure = badData = decryptCount = spotCount = rawCopies = 0;
}
static void Expected(u8 *out, BOOL spinda, BOOL animated, int x)
{
    u8 raw[6400];
    unsigned offset = 0;
    for (unsigned i = 0; i < sizeof(raw); ++i) raw[i] = (u8)(i * 13 + i / 17) ^ 0xA5;
    if (spinda) { raw[0] ^= 0x53; if (animated) raw[40] ^= 0x53; }
    for (int row = 0; row < 10; ++row)
        for (int col = x; col < x + 10; ++col)
            for (int line = 0; line < 8; ++line) {
                memcpy(out + offset, raw + (row * 8 + line) * 80 + col * 4, 4);
                offset += 4;
            }
    assert(offset == 3200);
}
static void CheckUnchanged(const u8 *out)
{
    for (unsigned i = 0; i < 3200; ++i) assert(out[i] == 0x6B);
}
static void ExpectedPackedFrame(u8 *out, BOOL spinda, BOOL animated, int frame)
{
    u8 tiles[3200];
    Expected(tiles, spinda, animated, frame * 10);
    /* The established six hardware sprite rectangles, in their legacy order. */
    static const int rectangles[6][4] = {
        {0,0,8,8}, {8,0,2,4}, {8,4,2,4}, {0,8,4,2}, {4,8,4,2}, {8,8,2,2}
    };
    unsigned offset = 0;
    for (unsigned i = 0; i < 6; ++i)
        for (int row = rectangles[i][1]; row < rectangles[i][1] + rectangles[i][3]; ++row)
            for (int col = rectangles[i][0]; col < rectangles[i][0] + rectangles[i][2]; ++col) {
                memcpy(out + offset, tiles + (row * 10 + col) * 32, 32);
                offset += 32;
            }
    assert(offset == 3200);
}
int main(void)
{
    u8 expected[3200], out[3200];
    const enum NarcID narc = NARC_INDEX_POKETOOL__POKEGRA__PL_POKEGRA;
    const enum HeapID heap = HEAP_ID_SYSTEM;
    for (int spinda = 0; spinda < 2; ++spinda) {
        u8 expectedBoth[6400], guarded[6464];
        memset(guarded, 0x6B, sizeof(guarded));
        ExpectedPackedFrame(expectedBoth, spinda, TRUE, 0);
        ExpectedPackedFrame(expectedBoth + 3200, spinda, TRUE, 1);
        /* Actual TradeSequence_LoadPokemonBGTiles asks for one 20x10 region. */
        CharacterSprite_LoadPokemonSpriteRect(narc, 0, heap, 0, 0, 20, 10, guarded + 32,
            0x53, TRUE, FACE_FRONT, spinda ? SPECIES_SPINDA : SPECIES_BULBASAUR);
        assert(!archiveLive && decryptCount == 1 && spotCount == spinda);
        assert(memcmp(guarded + 32, expectedBoth, sizeof(expectedBoth)) == 0);
        for (int i = 0; i < 32; ++i) assert(guarded[i] == 0x6B && guarded[6432 + i] == 0x6B);
        Reset();
        for (int frame = 0; frame < 2; ++frame) {
            CharacterSprite_LoadPokemonSpriteRect(narc, 0, heap, frame * 10, 0, 10, 10, out,
                0x53, TRUE, FACE_FRONT, spinda ? SPECIES_SPINDA : SPECIES_BULBASAUR);
            assert(memcmp(out, expectedBoth + frame * 3200, sizeof(out)) == 0);
            Reset();
        }
    }
    {
        u8 expectedBoth[6400], outBoth[6400];
        const TileRegion both = {0, 0, 20, 10};
        ExpectedPackedFrame(expectedBoth, FALSE, FALSE, 0);
        ExpectedPackedFrame(expectedBoth + 3200, FALSE, FALSE, 1);
        CharacterSprite_LoadSpriteRegion(narc, 0, heap, &both, outBoth);
        assert(memcmp(outBoth, expectedBoth, sizeof(outBoth)) == 0 && !archiveLive);
        Reset();
    }
    {
        const TileRegion invalid[] = {{0,0,8,10},{0,0,10,8},{0,0,30,10},{0,0,20,20}};
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            memset(out, 0x6B, sizeof(out));
            CharacterSprite_LoadPokemonSpriteRegion(narc, 0, heap, &invalid[i], out,
                0x53, TRUE, FACE_FRONT, SPECIES_SPINDA);
            CharacterSprite_LoadSpriteRegion(narc, 0, heap, &invalid[i], out);
            CheckUnchanged(out);
            assert(allocationAttempt == 0);
            Reset();
        }
    }
    for (int spinda = 0; spinda < 2; ++spinda) {
        for (int frame = 0; frame < 2; ++frame) {
            void *data = CharacterSprite_GetTiledPokemonSprite(narc, 0, heap, frame * 10, 0, 10, 10, 0x53, TRUE, FACE_FRONT, spinda ? SPECIES_SPINDA : SPECIES_BULBASAUR);
            assert(data && !archiveLive && decryptCount == 1 && spotCount == spinda && rawCopies == 800);
            Expected(expected, spinda, TRUE, frame * 10);
            assert(memcmp(data, expected, sizeof(expected)) == 0);
            Heap_Free(data);
            Reset();
        }
    }
    CharacterSprite_LoadPokemonSprite(narc, 0, heap, out, 0x53, TRUE, FACE_FRONT, SPECIES_SPINDA);
    assert(!archiveLive && decrypted && spotted && rawCopies == 800);
    Reset();
    CharacterSprite_LoadTiledData(narc, 0, heap, 0, 0, 10, 10, out);
    Expected(expected, FALSE, FALSE, 0);
    assert(!archiveLive && memcmp(out, expected, sizeof(out)) == 0);
    Reset();
    void *tiles = CharacterSprite_LoadTiles(narc, 0, heap);
    assert(tiles && !archiveLive);
    Heap_Free(tiles);
    Reset();
    for (int fail = 1; fail <= 3; ++fail) {
        failAllocation = fail;
        assert(CharacterSprite_LoadTiles(narc, 0, heap) == NULL);
        assert(!archiveLive && !decryptCount);
        Reset();
    }
    for (int fail = 1; fail <= 2; ++fail) {
        failAllocation = fail;
        memset(out, 0x6B, sizeof(out));
        CharacterSprite_LoadPokemonSprite(narc, 0, heap, out, 0x53, TRUE, FACE_FRONT, SPECIES_SPINDA);
        CheckUnchanged(out);
        assert(!archiveLive && !decryptCount);
        Reset();
    }
    for (int failure = 0; failure < 5; ++failure) {
        if (failure == 0) narcFailure = TRUE;
        else if (failure == 1) decodeFailure = TRUE;
        else badData = failure - 1;
        memset(out, 0x6B, sizeof(out));
        CharacterSprite_LoadPokemonSprite(narc, 0, heap, out, 0x53, TRUE, FACE_FRONT, SPECIES_SPINDA);
        CheckUnchanged(out);
        assert(!archiveLive && !decryptCount);
        Reset();
    }
    memset(out, 0x6B, sizeof(out));
    CharacterSprite_LoadTiledData(narc, 0, heap, 20, 0, 10, 10, out);
    CheckUnchanged(out);
    assert(!archiveLive && !decryptCount);
    Reset();
    assert(CharacterSprite_GetTiledPokemonSprite(narc, 0, heap, 0, 0, 0, 10, 0, FALSE, FACE_FRONT, SPECIES_BULBASAUR) == NULL);
    assert(allocationAttempt == 0);
    puts("Actual character sprite ownership: decryption, Spinda and exact3200/6400-byte packed frames finish before archive cleanup; allocation/decode/region failures preserve output and release owners.");
    return 0;
}
