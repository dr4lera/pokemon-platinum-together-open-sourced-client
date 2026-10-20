#ifndef COMMUNITY_BATTLE_FORMS_H
#define COMMUNITY_BATTLE_FORMS_H

#include <stdint.h>
#include "struct_decls/battle_system.h"

struct BattleContext;
struct Pokemon;

/* Lifetime is the entire battle, not one action or one active party slot. */
typedef struct CommunityBattleForms {
    uint32_t brokenDisguises[24];
    uint32_t bondedPokemon[24];
    uint8_t brokenCount;
    uint8_t bondedCount;
    uint8_t schoolingPending;
    uint8_t knockoutPending;
} CommunityBattleForms;

void CommunityForms_PartyLoaded(BattleSystem *, struct BattleContext *, int battler, struct Pokemon *);
int CommunityForms_CheckSchooling(BattleSystem *, struct BattleContext *, int pendingOnly);
int CommunityForms_BeforeMove(BattleSystem *, struct BattleContext *);
int CommunityForms_InterceptDamage(BattleSystem *, struct BattleContext *);
void CommunityForms_MarkKnockout(BattleSystem *, struct BattleContext *);
int CommunityForms_AfterMove(BattleSystem *, struct BattleContext *);
int CommunityForms_BondShuriken(struct BattleContext *, unsigned move, int battler);
int CommunityForms_AdjustPower(struct BattleContext *, unsigned move, int power, int battler);

#endif
