#ifndef COMMUNITY_CAPTURE_RULES_CORE_H
#define COMMUNITY_CAPTURE_RULES_CORE_H

/* Pure capture policy; independent of the renderer and communication layer. */
typedef struct CaptureRuleSlot {
    int side;
    int hp;
    int species;
    int absent;
} CaptureRuleSlot;

enum CaptureRuleResult {
    CAPTURE_RULE_OK,
    CAPTURE_RULE_TRAINER,
    CAPTURE_RULE_NO_TARGET,
    CAPTURE_RULE_MULTIPLE_TARGETS,
    CAPTURE_RULE_HIDDEN,
    CAPTURE_RULE_SUBSTITUTE,
    CAPTURE_RULE_NO_ROOM,
    CAPTURE_RULE_NO_BALL,
    CAPTURE_RULE_SPECIAL_MODE,
    CAPTURE_RULE_ALREADY_USED
};

static inline enum CaptureRuleResult CaptureRules_SelectTarget(unsigned battleType, int actorSide, const CaptureRuleSlot *slots, int count, int *target)
{
    int found = -1;
    *target = -1;
    /* Trainer ownership is authoritative even when other mode bits are set. */
    if (battleType & 1u) {
        return CAPTURE_RULE_TRAINER;
    }
    if (actorSide != 0 || count < 1 || count > 4) {
        return CAPTURE_RULE_NO_TARGET;
    }
    for (int i = 0; i < count; i++) {
        if (slots[i].side == actorSide || slots[i].hp <= 0 || slots[i].species <= 0 || slots[i].absent) {
            continue;
        }
        if (found >= 0) {
            return CAPTURE_RULE_MULTIPLE_TARGETS;
        }
        found = i;
    }
    if (found < 0) {
        return CAPTURE_RULE_NO_TARGET;
    }
    *target = found;
    return CAPTURE_RULE_OK;
}

/* The callback must remove exactly one item or leave inventory unchanged. */
static inline enum CaptureRuleResult CaptureRules_CommitDebit(int turn, int actor, int *lastTurn, unsigned *usedMask, int (*removeOne)(void *), void *inventory)
{
    if (actor < 0 || actor >= 4) {
        return CAPTURE_RULE_NO_TARGET;
    }
    if (*lastTurn != turn) {
        *lastTurn = turn;
        *usedMask = 0;
    }
    unsigned actorBit = 1u << actor;
    if (*usedMask & actorBit) {
        return CAPTURE_RULE_ALREADY_USED;
    }
    if (!removeOne(inventory)) {
        return CAPTURE_RULE_NO_BALL;
    }
    *usedMask |= actorBit;
    return CAPTURE_RULE_OK;
}

#endif
