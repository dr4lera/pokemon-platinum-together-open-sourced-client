#ifndef COMMUNITY_LEARNSET_H
#define COMMUNITY_LEARNSET_H

#include <stddef.h>
#include <stdint.h>

#define COMMUNITY_LEARNSET_CAPACITY 20
#define COMMUNITY_LEARNSET_TERMINATOR UINT16_MAX

typedef struct CommunityLearnsetEntry {
    uint16_t move;
    uint8_t level;
} CommunityLearnsetEntry;

typedef struct CommunityLearnset {
    size_t count;
    CommunityLearnsetEntry entries[COMMUNITY_LEARNSET_CAPACITY];
} CommunityLearnset;

enum CommunityLearnsetResult {
    COMMUNITY_LEARNSET_OK,
    COMMUNITY_LEARNSET_INVALID,
    COMMUNITY_LEARNSET_TOO_LONG,
    COMMUNITY_LEARNSET_UNTERMINATED
};

/* Legacy input remains 9 move bits / 7 level bits. Padding after its first
 * terminator is ignored. No partial result is exposed for malformed input. */
enum CommunityLearnsetResult CommunityLearnset_DecodeLegacy(const uint16_t *packed, size_t words, CommunityLearnset *out);
/* Separate fields support move IDs above 511 without changing legacy encoding.
 * Records retain their original order and duplicates; level 0 is permitted. */
enum CommunityLearnsetResult CommunityLearnset_FromEntries(const CommunityLearnsetEntry *entries, size_t count, CommunityLearnset *out);
int CommunityLearnset_NextAtLevel(const CommunityLearnset *learnset, unsigned level, size_t *cursor, uint16_t *move);
size_t CommunityLearnset_CopyMoveIds(const CommunityLearnset *learnset, uint16_t *moves, size_t capacity);
/* Returns a count excluding the always-written terminator when capacity > 0. */
size_t CommunityLearnset_ReminderMoves(const CommunityLearnset *learnset, unsigned level, const uint16_t *known, size_t knownCount, uint16_t *moves, size_t capacity);

/* Game adapter: loads the existing NARC only, with size and terminator bounds.
 * A modern content provider is intentionally not part of this milestone. */
enum CommunityLearnsetResult Pokemon_LoadCommunityLearnsetOf(int species, int form, CommunityLearnset *out);

#endif
