#ifndef COMMUNITY_ABILITY_RULES_H
#define COMMUNITY_ABILITY_RULES_H

#include "generated/abilities.h"
#include "port/community_modern_rules.h"
#include "struct_decls/battle_system.h"

struct BattleContext;

/* The legacy ability is unconditional. The two aliases are a separate,
 * explicitly selected 0.1.10 rule update, not a catalog-version inference. */
static inline int CommunityAbility_IgnoresDefenses(unsigned ability)
{
    return ability == ABILITY_MOLD_BREAKER
        || (CommunityAbilityUpdates_Enabled()
            && (ability == ABILITY_TURBOBLAZE || ability == ABILITY_TERAVOLT));
}

int CommunityAbility_AdjustAccuracy(BattleSystem *battleSys, struct BattleContext *battleCtx, int attacker, int accuracy);

#endif
