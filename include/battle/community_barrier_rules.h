#ifndef COMMUNITY_BARRIER_RULES_H
#define COMMUNITY_BARRIER_RULES_H

#include "constants/moves.h"
#include "generated/abilities.h"
#include "generated/move_flags.h"
#include "port/community_modern_rules.h"

/* Callers provide effective abilities, so normal suppression remains shared
 * with the engine. These predicates never mutate shields or status fields. */
static inline int CommunityAbility_Infiltrates(unsigned ability)
{
    return CommunityAbilityUpdates_Enabled() && ability == ABILITY_INFILTRATOR;
}

static inline int CommunityAbility_BypassesProtect(unsigned ability, unsigned moveFlags, int priorityBlocked)
{
    return CommunityAbilityUpdates_Enabled() && !priorityBlocked
        && ability == ABILITY_UNSEEN_FIST && (moveFlags & MOVE_FLAG_MAKES_CONTACT);
}

struct BattleContext;
int CommunityAbility_BattlerInfiltrates(struct BattleContext *battleCtx, int battler);
int CommunityAbility_BattlerBypassesProtect(struct BattleContext *battleCtx, int battler, unsigned move, int priorityBlocked);

#endif
