#include "port/community_learnset.h"

enum CommunityLearnsetResult CommunityLearnset_DecodeLegacy(const uint16_t *packed, size_t words, CommunityLearnset *out)
{
    if (out == NULL) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    out->count = 0;
    if (packed == NULL || words == 0) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    size_t limit = words < COMMUNITY_LEARNSET_CAPACITY + 1 ? words : COMMUNITY_LEARNSET_CAPACITY + 1;
    for (size_t i = 0; i < limit; i++) {
        if (packed[i] == COMMUNITY_LEARNSET_TERMINATOR) {
            out->count = i;
            return COMMUNITY_LEARNSET_OK;
        }
        if (i == COMMUNITY_LEARNSET_CAPACITY) {
            return COMMUNITY_LEARNSET_TOO_LONG;
        }
        out->entries[i].move = packed[i] & 0x1ff;
        out->entries[i].level = packed[i] >> 9;
    }
    return COMMUNITY_LEARNSET_UNTERMINATED;
}

enum CommunityLearnsetResult CommunityLearnset_FromEntries(const CommunityLearnsetEntry *entries, size_t count, CommunityLearnset *out)
{
    if (out == NULL) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    out->count = 0;
    if (count > COMMUNITY_LEARNSET_CAPACITY) {
        return COMMUNITY_LEARNSET_TOO_LONG;
    }
    if (count != 0 && entries == NULL) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    for (size_t i = 0; i < count; i++) {
        if (entries[i].move == COMMUNITY_LEARNSET_TERMINATOR || entries[i].level > 127) {
            return COMMUNITY_LEARNSET_INVALID;
        }
    }
    for (size_t i = 0; i < count; i++) {
        out->entries[i] = entries[i];
    }
    out->count = count;
    return COMMUNITY_LEARNSET_OK;
}

static int ValidLearnset(const CommunityLearnset *learnset)
{
    return learnset != NULL && learnset->count <= COMMUNITY_LEARNSET_CAPACITY;
}

int CommunityLearnset_NextAtLevel(const CommunityLearnset *learnset, unsigned level, size_t *cursor, uint16_t *move)
{
    if (!ValidLearnset(learnset) || cursor == NULL || move == NULL || *cursor > learnset->count) {
        return 0;
    }
    while (*cursor < learnset->count) {
        CommunityLearnsetEntry entry = learnset->entries[(*cursor)++];
        if (entry.level == level) {
            *move = entry.move;
            return 1;
        }
    }
    return 0;
}

size_t CommunityLearnset_CopyMoveIds(const CommunityLearnset *learnset, uint16_t *moves, size_t capacity)
{
    if (!ValidLearnset(learnset) || moves == NULL) {
        return 0;
    }
    size_t count = learnset->count < capacity ? learnset->count : capacity;
    for (size_t i = 0; i < count; i++) {
        moves[i] = learnset->entries[i].move;
    }
    return count;
}

size_t CommunityLearnset_ReminderMoves(const CommunityLearnset *learnset, unsigned level, const uint16_t *known, size_t knownCount, uint16_t *moves, size_t capacity)
{
    if (moves == NULL || capacity == 0) {
        return 0;
    }
    moves[0] = COMMUNITY_LEARNSET_TERMINATOR;
    if (!ValidLearnset(learnset) || (knownCount != 0 && known == NULL)) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < learnset->count && count < capacity - 1; i++) {
        uint16_t move = learnset->entries[i].move;
        if (learnset->entries[i].level > level) {
            continue;
        }
        size_t j;
        for (j = 0; j < knownCount; j++) {
            if (known[j] == move) break;
        }
        if (j != knownCount) continue;
        for (j = 0; j < count; j++) {
            if (moves[j] == move) break;
        }
        if (j == count) moves[count++] = move;
    }
    moves[count] = COMMUNITY_LEARNSET_TERMINATOR;
    return count;
}
