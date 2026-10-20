#include "character_sprite.h"

#include <nitro.h>
#include <limits.h>
#include <string.h>

#include "constants/graphics.h"
#include "constants/heap.h"
#include "constants/narc.h"
#include "constants/species.h"

#include "heap.h"
#include "narc.h"
#include "pokemon.h"
#include "pokemon_sprite.h"

#define SUB_REGION_ORDER { { 0x0, 0x0, 0x8, 0x8 }, { 0x8, 0x0, 0x2, 0x4 }, { 0x8, 0x4, 0x2, 0x4 }, { 0x0, 0x8, 0x4, 0x2 }, { 0x4, 0x8, 0x4, 0x2 }, { 0x8, 0x8, 0x2, 0x2 } }

static void CharacterSprite_CopyTileFromUntiledArray(const u8 *src, u8 *dest, int *srcOffset, int *destOffset, int tileWidth, int rowWidth)
{
    for (int i = 0; i < TILE_HEIGHT_PIXELS; i++) {
        memcpy(&dest[*destOffset], &src[*srcOffset], tileWidth);
        *srcOffset += rowWidth;
        *destOffset += tileWidth;
    }
}

/* The unpacked descriptor/raw data borrow this archive. Desktop cleanup may
 * also release the descriptor when the archive is freed. Return its owner so
 * the caller retains both through decryption, spot drawing and tile copying. */
static void *CharacterSprite_LoadSpriteData(enum NarcID narcID, int narcIdx, enum HeapID heapID, NNSG2dCharacterData **charData)
{
    if (charData == NULL) {
        return NULL;
    }
    *charData = NULL;

    void *ngcrFile = NARC_AllocAndReadWholeMemberByIndexPair(narcID, narcIdx, heapID);
    if (ngcrFile == NULL) {
        return NULL;
    }
    if (!NNS_G2dGetUnpackedCharacterData(ngcrFile, charData) || *charData == NULL || (*charData)->pRawData == NULL) {
        Heap_Free(ngcrFile);
        *charData = NULL;
        return NULL;
    }
    return ngcrFile;
}

static BOOL CharacterSprite_ValidRegion(int x, int y, int width, int height, const NNSG2dCharacterData *data)
{
    if (data == NULL || data->pRawData == NULL || x < 0 || y < 0 || width <= 0 || height <= 0
        || x > data->W || width > data->W - x || y > data->H || height > data->H - y) {
        return FALSE;
    }
    u64 lastByte = (((u64)(y + height) * TILE_HEIGHT_PIXELS - 1) * data->W + x + width) * TILE_WIDTH_4BPP;
    return lastByte <= INT_MAX && lastByte <= data->szByte;
}

static int CharacterSprite_TiledSize(int width, int height)
{
    if (width <= 0 || height <= 0 || width > INT_MAX / TILE_SIZE_4BPP / height) {
        return 0;
    }
    return width * height * TILE_SIZE_4BPP;
}

static BOOL CharacterSprite_TileUntiledPokemonSprite(int x, int y, int width, int height, NNSG2dCharacterData *monSpriteData, void *dest)
{
    u8 *untiledSpriteData = NULL;
    u8 *tiledData = NULL;
    int startIndex;
    int tiledDataOffset;
    int spriteWidth;

    if (dest == NULL || !CharacterSprite_ValidRegion(x, y, width, height, monSpriteData)) {
        return FALSE;
    }

    untiledSpriteData = monSpriteData->pRawData;
    tiledData = dest;
    spriteWidth = (monSpriteData->W * TILE_WIDTH_4BPP);
    startIndex = (x * TILE_WIDTH_4BPP) + (y * spriteWidth);
    tiledDataOffset = 0;

    for (int row = y; row < (y + height); row++) {
        for (int col = x; col < (x + width); col++) {
            startIndex = (col * TILE_WIDTH_4BPP) + (row * spriteWidth * 8);
            CharacterSprite_CopyTileFromUntiledArray(untiledSpriteData, tiledData, &startIndex, &tiledDataOffset, TILE_WIDTH_4BPP, spriteWidth);
        }
    }
    return TRUE;
}

static BOOL CharacterSprite_SpeciesIsSpinda(enum Species species)
{
    enum Species listItem;
    const int speciesList[] = {
        SPECIES_SPINDA,
        SPECIES_BAD_EGG,
    };

    for (int i = 0; i < MAX_SPECIES; i++) {
        listItem = speciesList[i];

        if (listItem == SPECIES_BAD_EGG) {
            break;
        }

        if (listItem == species) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL CharacterSprite_LoadTiledPokemonSprite(enum NarcID narcID, int narcIdx, enum HeapID heapID, int x, int y, int width, int height, void *buffer, u32 personality, BOOL isAnimated, int face, enum Species species)
{
    BOOL speciesIsSpinda;
    NNSG2dCharacterData *monSpriteData = NULL;

    if (buffer == NULL) {
        return FALSE;
    }
    void *ncgrFile = CharacterSprite_LoadSpriteData(narcID, narcIdx, heapID, &monSpriteData);
    if (ncgrFile == NULL) {
        return FALSE;
    }
    if (!CharacterSprite_ValidRegion(x, y, width, height, monSpriteData)
        || monSpriteData->szByte < 2 * MON_SPRITE_FRAME_MAX_SIZE_BYTES) {
        Heap_Free(ncgrFile);
        return FALSE;
    }
    PokemonSprite_Decrypt(monSpriteData->pRawData, narcID);

    speciesIsSpinda = CharacterSprite_SpeciesIsSpinda(species);

    if ((face == FACE_FRONT) && (speciesIsSpinda == TRUE)) {
        PokemonSprite_DrawSpindaSpots(monSpriteData->pRawData, personality, isAnimated);
    }

    BOOL success = CharacterSprite_TileUntiledPokemonSprite(x, y, width, height, monSpriteData, buffer);
    Heap_Free(ncgrFile);
    return success;
}

static void *CharacterSprite_GetTiledPokemonSprite(enum NarcID narcID, int narcIdx, enum HeapID heapID, int x, int y, int width, int height, u32 personality, BOOL isAnimated, int face, enum Species species)
{
    int dataSize = CharacterSprite_TiledSize(width, height);
    if (dataSize == 0) {
        return NULL;
    }
    void *buffer = Heap_Alloc(heapID, dataSize);
    if (buffer == NULL) {
        return NULL;
    }
    if (!CharacterSprite_LoadTiledPokemonSprite(narcID, narcIdx, heapID, x, y, width, height, buffer, personality, isAnimated, face, species)) {
        Heap_Free(buffer);
        return NULL;
    }

    return buffer;
}

static BOOL CharacterSprite_LoadTiledDataInternal(enum NarcID narcID, int narcIdx, enum HeapID heapID, int x, int y, int width, int height, void *dest)
{
    NNSG2dCharacterData *characterData = NULL;
    if (dest == NULL) {
        return FALSE;
    }
    void *ncgrFile = CharacterSprite_LoadSpriteData(narcID, narcIdx, heapID, &characterData);
    if (ncgrFile == NULL) {
        return FALSE;
    }
    if (!CharacterSprite_ValidRegion(x, y, width, height, characterData)
        || characterData->szByte < 2 * MON_SPRITE_FRAME_MAX_SIZE_BYTES) {
        Heap_Free(ncgrFile);
        return FALSE;
    }
    PokemonSprite_Decrypt(characterData->pRawData, narcID);
    BOOL success = CharacterSprite_TileUntiledPokemonSprite(x, y, width, height, characterData, dest);
    Heap_Free(ncgrFile);
    return success;
}

void CharacterSprite_LoadTiledData(enum NarcID narcID, int narcIdx, enum HeapID heapID, int x, int y, int width, int height, void *dest)
{
    (void)CharacterSprite_LoadTiledDataInternal(narcID, narcIdx, heapID, x, y, width, height, dest);
}

static void *CharacterSprite_GetTiledSprite(enum NarcID narcID, int characterIdx, enum HeapID heapID, int x, int y, int width, int height)
{
    void *buffer;
    int numBytes = CharacterSprite_TiledSize(width, height);
    if (numBytes == 0) {
        return NULL;
    }
    buffer = Heap_Alloc(heapID, numBytes);
    if (buffer == NULL) {
        return NULL;
    }
    if (!CharacterSprite_LoadTiledDataInternal(narcID, characterIdx, heapID, x, y, width, height, buffer)) {
        Heap_Free(buffer);
        return NULL;
    }

    return buffer;
}

static void CharacterSprite_CopyTilesRect(int sourceWidth, int sourceHeight, int x, int y, int width, int height, int *destOffset, const void *src, void *dest)
{
    u8 *srcPtr;
    u8 *destPtr;
    int srcTile;
    int finishCol;
    int finishRow;

    srcPtr = src;
    destPtr = dest;
    finishCol = x + width;
    finishRow = y + height;

    for (int row = y; row < finishRow; row++) {
        for (int col = x; col < finishCol; col++) {
            srcTile = (col * TILE_SIZE_4BPP) + (row * TILE_SIZE_4BPP * sourceWidth);
            memcpy(&destPtr[*destOffset], &srcPtr[srcTile], TILE_SIZE_4BPP);
            *destOffset += TILE_SIZE_4BPP;
        }
    }
}

static void CharacterSprite_CopyTilesRegion(int sourceWidth, int sourceHeight, const TileRegion *subRegion, int *destOffset, const void *sourceSprite, void *dest)
{
    CharacterSprite_CopyTilesRect(sourceWidth, sourceHeight, subRegion->x, subRegion->y, subRegion->width, subRegion->height, destOffset, sourceSprite, dest);
}

/* Public packed-sprite consumers use one 10x10 frame or the trade animation's
 * two horizontally adjacent frames. Keep each frame's six hardware rectangles
 * in the existing order; the second frame starts at source column ten. */
static BOOL CharacterSprite_ValidPackedFrames(int width, int height)
{
    return (width == 10 || width == 20) && height == 10;
}

static void CharacterSprite_CopyPackedFrames(int width, int height, const void *source, void *dest)
{
    const TileRegion subRegions[] = SUB_REGION_ORDER;
    int destOffset = 0;
    for (int frame = 0; frame < width / 10; ++frame) {
        for (int i = 0; i < NELEMS(subRegions); ++i) {
            TileRegion region = subRegions[i];
            region.x += frame * 10;
            CharacterSprite_CopyTilesRegion(width, height, &region, &destOffset, source, dest);
        }
    }
}

static BOOL CharacterSprite_LoadSpriteRect(enum NarcID narcID, int characterIdx, enum HeapID heapID, int x, int y, int width, int height, void *dest)
{
    void *sprite = NULL;

    if (dest == NULL || !CharacterSprite_ValidPackedFrames(width, height)) {
        return FALSE;
    }
    sprite = CharacterSprite_GetTiledSprite(narcID, characterIdx, heapID, x, y, width, height);
    if (sprite == NULL) {
        return FALSE;
    }

    CharacterSprite_CopyPackedFrames(width, height, sprite, dest);

    Heap_Free(sprite);
    return TRUE;
}

void CharacterSprite_LoadSpriteRegion(enum NarcID narcID, int narcIdx, enum HeapID heapID, const TileRegion *region, void *dest)
{
    if (region != NULL) {
        (void)CharacterSprite_LoadSpriteRect(narcID, narcIdx, heapID, region->x, region->y, region->width, region->height, dest);
    }
}

void CharacterSprite_LoadSpriteFrame0(enum NarcID narcID, int narcIdx, enum HeapID heapID, void *dest)
{
    const TileRegion region = FRAME_0_REGION;
    CharacterSprite_LoadSpriteRegion(narcID, narcIdx, heapID, &region, dest);
}

void *CharacterSprite_LoadTiles(enum NarcID narcID, int narcIdx, enum HeapID heapID)
{
    int size = (MON_SPRITE_FRAME_WIDTH_TILES * MON_SPRITE_FRAME_HEIGHT_TILES * TILE_SIZE_4BPP);
    void *spriteData = Heap_Alloc(heapID, size);
    if (spriteData == NULL) {
        return NULL;
    }
    const TileRegion region = FRAME_0_REGION;
    if (!CharacterSprite_LoadSpriteRect(narcID, narcIdx, heapID, region.x, region.y, region.width, region.height, spriteData)) {
        Heap_Free(spriteData);
        return NULL;
    }

    return spriteData;
}

void *CharacterSprite_LoadPalette(enum NarcID narcID, int paletteIndex, enum HeapID heapID)
{
    NNSG2dPaletteData *paletteData;
    void *nclrFile;
    void *returnPtr;
    BOOL success;

    returnPtr = Heap_Alloc(heapID, PALETTE_SIZE_BYTES);
    nclrFile = NARC_AllocAndReadWholeMemberByIndexPair(narcID, paletteIndex, heapID);
    success = NNS_G2dGetUnpackedPaletteData(nclrFile, &paletteData);

    GF_ASSERT(success == TRUE);
    MI_CpuCopy16(paletteData->pRawData, returnPtr, PALETTE_SIZE_BYTES);

    Heap_Free(nclrFile);

    return returnPtr;
}

void CharacterSprite_LoadPokemonSpriteRect(enum NarcID narcID, int narcIdx, enum HeapID heapID, int x, int y, int width, int height, u8 *dest, u32 personality, BOOL isAnimated, int face, enum Species species)
{
    void *tiledData = NULL;

    if (dest == NULL || !CharacterSprite_ValidPackedFrames(width, height)) {
        return;
    }

    tiledData = CharacterSprite_GetTiledPokemonSprite(narcID, narcIdx, heapID, x, y, width, height, personality, isAnimated, face, species);
    if (tiledData == NULL) {
        return;
    }

    CharacterSprite_CopyPackedFrames(width, height, tiledData, dest);

    Heap_Free(tiledData);
}

void CharacterSprite_LoadPokemonSpriteRegion(enum NarcID narcID, int narcIdx, enum HeapID heapID, const TileRegion *frameTiles, void *dest, u32 personality, BOOL isAnimated, int face, enum Species species)
{
    if (frameTiles != NULL) {
        CharacterSprite_LoadPokemonSpriteRect(narcID, narcIdx, heapID, frameTiles->x, frameTiles->y, frameTiles->width, frameTiles->height, dest, personality, isAnimated, face, species);
    }
}

void CharacterSprite_LoadPokemonSprite(enum NarcID narcID, int narcIdx, enum HeapID heapID, void *destPtr, u32 personality, BOOL isAnimated, int face, enum Species species)
{
    const TileRegion region = FRAME_0_REGION;

    CharacterSprite_LoadPokemonSpriteRegion(narcID, narcIdx, heapID, &region, destPtr, personality, isAnimated, face, species);
}
