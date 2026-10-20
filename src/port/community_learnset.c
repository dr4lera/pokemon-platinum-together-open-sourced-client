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
    size_t limit = words < COMMUNITY_LEARNSET_LEGACY_CAPACITY + 1 ? words : COMMUNITY_LEARNSET_LEGACY_CAPACITY + 1;
    for (size_t i = 0; i < limit; i++) {
        if (packed[i] == COMMUNITY_LEARNSET_TERMINATOR) {
            out->count = i;
            return COMMUNITY_LEARNSET_OK;
        }
        if (i == COMMUNITY_LEARNSET_LEGACY_CAPACITY) {
            return COMMUNITY_LEARNSET_TOO_LONG;
        }
        out->entries[i].move = packed[i] & 0x1ff;
        out->entries[i].level = packed[i] >> 9;
    }
    return COMMUNITY_LEARNSET_UNTERMINATED;
}

static uint32_t ReadLittleEndianWord(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

enum CommunityLearnsetResult CommunityLearnset_DecodeModern(const uint8_t *bytes, size_t length, CommunityLearnset *out)
{
    if (out == NULL) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    out->count = 0;
    if (bytes == NULL || length < 4 || length % 4 != 0 || length > COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES) {
        return COMMUNITY_LEARNSET_INVALID;
    }
    size_t words = length / 4;
    for (size_t i = 0; i < words; i++) {
        uint32_t word = ReadLittleEndianWord(bytes + i * 4);
        if (word == UINT32_MAX) {
            /* Validate the observed padding convention before exposing count. */
            for (size_t j = i + 1; j < words; j++) {
                if (ReadLittleEndianWord(bytes + j * 4) != 0) {
                    return COMMUNITY_LEARNSET_INVALID;
                }
            }
            out->count = i;
            return COMMUNITY_LEARNSET_OK;
        }
        if (i == COMMUNITY_LEARNSET_CAPACITY) {
            return COMMUNITY_LEARNSET_TOO_LONG;
        }
        uint16_t move = word & UINT16_MAX;
        uint32_t level = word >> 16;
        if (move == COMMUNITY_LEARNSET_TERMINATOR || level > 127) {
            return COMMUNITY_LEARNSET_INVALID;
        }
        out->entries[i].move = move;
        out->entries[i].level = (uint8_t)level;
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
