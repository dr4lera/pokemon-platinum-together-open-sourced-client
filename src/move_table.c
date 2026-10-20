#include "move_table.h"
#include "port/community_catalog.h"
#include "port/community_modern_rules.h"

#include <nitro.h>
#include <string.h>

#include "constants/moves.h"

#include "narc.h"

static void LoadMoveEntry(int move, MoveTable *entry);

#define COMMUNITY_LEGACY_MOVE_COUNT 468

static int LoadCommunityFairyEntry(int move, MoveTable *entry)
{
    /* Same functional records as the six source-owned res/moves definitions.
     * Their original IDs are stable; unused Gen4 tail records are not moves. */
    static const u8 records[6][16] = {
        { 0,0,1,40,18,100,30,0,0,0,0,50,5,1,0,0 },
        { 17,0,1,40,18,0,15,0,4,0,0,50,17,2,0,0 },
        { 3,0,1,50,18,100,10,0,0,0,0,19,20,2,0,0 },
        { 68,0,0,90,18,90,10,10,0,0,0,51,5,2,0,0 },
        { 0,0,1,80,18,100,10,0,4,0,0,18,5,1,0,0 },
        { 71,0,1,95,18,100,15,30,0,0,0,18,22,1,0,0 }
    };
    if (move < COMMUNITY_LEGACY_MOVE_COUNT || move >= COMMUNITY_LEGACY_MOVE_COUNT + 6 || !CommunityModernRules_Enabled()) return 0;
    memcpy(entry, records[move - COMMUNITY_LEGACY_MOVE_COUNT], sizeof(records[0]));
    return 1;
}

void MoveTable_Load(void *buf)
{
    memset(buf, 0, sizeof(MoveTable) * MAX_MOVES);
    /* A user's original Gen4 archive contains only 471 records. Do not read the
     * expanded MAX_MOVES through its old reserved tail and beyond EOF. */
    NARC_ReadFromMemberByIndexPair(buf, NARC_INDEX_POKETOOL__WAZA__PL_WAZA_TBL, 0, 0, sizeof(MoveTable) * COMMUNITY_LEGACY_MOVE_COUNT);
    if (CommunityModernRules_Enabled()) {
        MoveTable *entries = buf;
        const CommunityCatalog *catalog = CommunityCatalog_Runtime();
        for (unsigned move = 0; move < MAX_MOVES; move++) {
            if (move >= COMMUNITY_LEGACY_MOVE_COUNT) LoadCommunityFairyEntry(move, &entries[move]);
            CommunityCatalog_Move(catalog, move, (uint8_t *)&entries[move]);
        }
    }
}

u32 MoveTable_LoadParam(int move, enum MoveAttribute param)
{
    MoveTable moveData;

    LoadMoveEntry(move, &moveData);
    return MoveTable_Get(&moveData, param);
}

u8 MoveTable_CalcMaxPP(u16 move, u8 ppUps)
{
    if (ppUps > 3) {
        ppUps = 3;
    }

    u8 pp = MoveTable_LoadParam(move, MOVEATTRIBUTE_PP);
    pp += (pp * 20 * ppUps) / 100;

    return pp;
}

u32 MoveTable_Get(MoveTable *entry, enum MoveAttribute param)
{
    u32 result;

    switch (param) {
    case MOVEATTRIBUTE_EFFECT:
        result = entry->effect;
        break;
    case MOVEATTRIBUTE_CLASS:
        result = entry->class;
        break;
    case MOVEATTRIBUTE_POWER:
        result = entry->power;
        break;
    case MOVEATTRIBUTE_TYPE:
        result = entry->type;
        break;
    case MOVEATTRIBUTE_ACCURACY:
        result = entry->accuracy;
        break;
    case MOVEATTRIBUTE_PP:
        result = entry->pp;
        break;
    case MOVEATTRIBUTE_EFFECT_CHANCE:
        result = entry->effectChance;
        break;
    case MOVEATTRIBUTE_RANGE:
        result = entry->range;
        break;
    case MOVEATTRIBUTE_PRIORTY:
        result = entry->priority;
        break;
    case MOVEATTRIBUTE_FLAGS:
        result = entry->flags;
        break;
    case MOVEATTRIBUTE_CONTEST_EFFECT:
        result = entry->contest.effect;
        break;
    case MOVEATTRIBUTE_CONTEST_TYPE:
        result = entry->contest.type;
        break;
    }

    return result;
}

static void LoadMoveEntry(int move, MoveTable *entry)
{
    if (move < 0 || move >= MAX_MOVES) {
        memset(entry, 0, sizeof(*entry));
        return;
    }
    if (move >= COMMUNITY_LEGACY_MOVE_COUNT) {
        if (!LoadCommunityFairyEntry(move, entry)) memset(entry, 0, sizeof(*entry));
        return;
    }
    NARC_ReadWholeMemberByIndexPair(entry, NARC_INDEX_POKETOOL__WAZA__PL_WAZA_TBL, move);
    if (CommunityModernRules_Enabled()) CommunityCatalog_Move(CommunityCatalog_Runtime(), move, (uint8_t *)entry);
}
