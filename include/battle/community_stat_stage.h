#ifndef COMMUNITY_STAT_STAGE_H
#define COMMUNITY_STAT_STAGE_H

#include <stdint.h>

#define COMMUNITY_STAT_QUEUE_CAPACITY 24

/* Battle-local state only; never serialized into a Pokemon or save. */
typedef struct CommunityStatStageEntry {
    uint8_t battler;
    uint8_t stat;
    int8_t change;
} CommunityStatStageEntry;

typedef struct CommunityStatStageQueue {
    CommunityStatStageEntry entries[COMMUNITY_STAT_QUEUE_CAPACITY];
    uint8_t count;
    uint8_t cursor;
    int8_t previousDirection;
    uint8_t previousBattler;
    int8_t stageChange;
    uint16_t overrideMove;
    uint16_t sourceMove;
} CommunityStatStageQueue;

#endif
