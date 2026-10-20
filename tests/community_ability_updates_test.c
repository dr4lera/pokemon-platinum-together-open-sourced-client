#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "battle/battle_context.h"
#include "battle/battle_lib.h"
#include "battle/battle_controller_player.h"
#include "battle/battle_system.h"
#include "battle/common.h"
#include "battle/community_ability_rules.h"
#include "battle/message_defs.h"
#include "constants/battle/battle_script.h"
#include "constants/items.h"
#include "constants/species.h"
#include "struct_defs/battler_data.h"
#include "struct_defs/fraction.h"
#include "data/hit_rate_stages.h"
#undef main
#undef GF_ASSERT
#define GF_ASSERT(value) assert(value)

struct BattleSystem { int battlers; u32 type; u16 random; };
static int modern, updates;
int CommunityModernRules_Enabled(void) { return modern; }
int CommunityAbilityUpdates_Enabled(void) { return modern && updates; }
int BattleSystem_GetMaxBattlers(BattleSystem *system) { return system->battlers; }
u8 BattleSystem_GetBattlerSide(BattleSystem *system, int battler) { (void)system; return battler & 1; }
u32 BattleSystem_GetBattleType(BattleSystem *system) { return system->type; }
u16 BattleSystem_RandNext(BattleSystem *system) { return system->random; }
static u32 FlagIndex(int index) { return 1U << index; }
s32 Battler_HeldItemEffect(BattleContext *context, int battler) { (void)context; (void)battler; return 0; }
s32 Battler_HeldItemPower(BattleContext *context, int battler, enum HeldItemPowerOp op)
{ (void)context; (void)battler; (void)op; return 0; }
u16 Battler_HeldItem(BattleContext *context, int battler) { return context->battleMons[battler].heldItem; }
s32 BattleSystem_GetItemData(BattleContext *context, u16 item, enum ItemDataParam param)
{ (void)context; (void)item; (void)param; return 0; }
int BattleContext_Get(BattleSystem *system, BattleContext *context, enum BattleContextParam param, int battler)
{ (void)system; (void)param; (void)battler; return context->totalTurns; }
BOOL BattleSystem_AnyBattlersWithMoveEffect(BattleSystem *system, BattleContext *context, int mask)
{ for (int i=0;i<system->battlers;i++) if (context->battleMons[i].moveEffectsMask & mask) return TRUE; return FALSE; }
u8 BattleSystem_CountAliveBattlers(BattleSystem *system, BattleContext *context, BOOL sameSide, int defender)
{ unsigned count=0; for(int i=0;i<system->battlers;i++) if (context->battleMons[i].curHP && (((i^defender)&1)==0)==!!sameSide) count++; return count; }
BOOL Battler_MovedThisTurn(BattleContext *context, int battler) { (void)context; (void)battler; return FALSE; }
int BattleMon_Get(BattleContext *context, int battler, enum BattleMonParam param, void *out)
{
    (void)out;
    BattleMon *mon = &context->battleMons[battler];
    switch (param) {
    case BATTLEMON_SPECIES: return mon->species;
    case BATTLEMON_ATTACK: return mon->attack;
    case BATTLEMON_DEFENSE: return mon->defense;
    case BATTLEMON_SP_ATTACK: return mon->spAttack;
    case BATTLEMON_SP_DEFENSE: return mon->spDefense;
    case BATTLEMON_CUR_HP: return mon->curHP;
    case BATTLEMON_MAX_HP: return mon->maxHP;
    case BATTLEMON_LEVEL: return mon->level;
    case BATTLEMON_TYPE_1: return mon->type1;
    case BATTLEMON_TYPE_2: return mon->type2;
    case BATTLEMON_STATUS: return mon->status;
    case BATTLEMON_VOLATILE_STATUS: return mon->statusVolatile;
    case BATTLEMON_ATTACK_STAGE: return mon->statBoosts[BATTLE_STAT_ATTACK];
    case BATTLEMON_DEFENSE_STAGE: return mon->statBoosts[BATTLE_STAT_DEFENSE];
    case BATTLEMON_SP_ATTACK_STAGE: return mon->statBoosts[BATTLE_STAT_SP_ATTACK];
    case BATTLEMON_SP_DEFENSE_STAGE: return mon->statBoosts[BATTLE_STAT_SP_DEFENSE];
    case BATTLEMON_MOVE_1: case BATTLEMON_MOVE_2: case BATTLEMON_MOVE_3: case BATTLEMON_MOVE_4:
        return mon->moves[param-BATTLEMON_MOVE_1];
    default: return 0;
    }
}
static Pokemon party[2];
static unsigned partyAbilities[2], partyStatuses[2];
int BattleSystem_GetPartyCount(BattleSystem *system, int battler) { (void)system; (void)battler; return 2; }
Pokemon *BattleSystem_GetPartyPokemon(BattleSystem *system, int battler, int slot)
{ (void)system; (void)battler; return &party[slot]; }
u32 Pokemon_GetValue(Pokemon *mon, enum PokemonDataParam param, void *out)
{ (void)out; assert(param==MON_DATA_ABILITY); return partyAbilities[mon-party]; }
void Pokemon_SetValue(Pokemon *mon, enum PokemonDataParam param, const void *value)
{ assert(param==MON_DATA_STATUS); partyStatuses[mon-party]=*(const u32*)value; }
void BattleController_EmitClearCommand(BattleSystem *system, int battler, int command)
{ (void)system; (void)battler; (void)command; }
static void ZeroDataBuffer(BattlerData *data) { memset(data->data,0,sizeof(data->data)); }
#include "actual_ability_updates.inc"

static void prepare(BattleContext *context)
{
    memset(context,0,sizeof(*context)); modern=1; updates=0;
    context->powerMul=10; context->totalTurns=10;
    for (int i=0;i<4;i++) {
        BattleMon *mon=&context->battleMons[i];
        mon->curHP=mon->maxHP=100; mon->level=50;
        mon->attack=mon->defense=mon->spAttack=mon->spDefense=100;
        mon->speed=100; mon->type1=mon->type2=TYPE_NORMAL;
        for(int j=0;j<NUM_BOOSTABLE_STATS;j++) mon->statBoosts[j]=6;
        context->battlerActions[i][BATTLE_ACTION_SELECTED_COMMAND]=PLAYER_INPUT_FIGHT;
        mon->moves[0]=MOVE_GROWL;
    }
    context->aiContext.moveTable[MOVE_BITE].power=60;
    context->aiContext.moveTable[MOVE_BITE].type=TYPE_DARK;
    context->aiContext.moveTable[MOVE_BITE].class=CLASS_PHYSICAL;
    context->aiContext.moveTable[MOVE_BITE].accuracy=80;
    context->aiContext.moveTable[MOVE_GROWL].class=CLASS_STATUS;
}
static int damage(BattleSystem *system, BattleContext *context, unsigned power)
{ return BattleSystem_CalcMoveDamage(system,context,MOVE_BITE,0,0,power,TYPE_DARK,0,1,1); }
int main(void)
{
    BattleSystem system={4,0,84}; BattleContext context;
    prepare(&context);
    assert(ABILITY_UNNERVE==124 && ABILITY_TURBOBLAZE==133 && ABILITY_TERAVOLT==134 && ABILITY_BATTLE_BOND==143);
    assert(CommunityAbility_IgnoresDefenses(ABILITY_MOLD_BREAKER));
    assert(!CommunityAbility_IgnoresDefenses(ABILITY_TURBOBLAZE)); updates=1;
    assert(CommunityAbility_IgnoresDefenses(ABILITY_TURBOBLAZE) && CommunityAbility_IgnoresDefenses(ABILITY_TERAVOLT));
    context.battleMons[0].ability=ABILITY_TURBOBLAZE; context.battleMons[1].ability=ABILITY_CLEAR_BODY;
    assert(!Battler_IgnorableAbility(&context,0,1,ABILITY_CLEAR_BODY));
    assert(context.selfTurnFlags[0].moldBreakerActivated && (context.battleStatusMask & SYSCTL_APPLY_MOLD_BREAKER));
    context.battleMons[0].moveEffectsMask=MOVE_EFFECT_ABILITY_SUPPRESSED;
    assert(Battler_IgnorableAbility(&context,0,1,ABILITY_CLEAR_BODY));
    context.battleMons[0].moveEffectsMask=0; updates=0;
    assert(Battler_IgnorableAbility(&context,0,1,ABILITY_CLEAR_BODY));

    prepare(&context); int ordinary=damage(&system,&context,60);
    context.battleMons[0].ability=ABILITY_STRONG_JAW;
    assert(damage(&system,&context,60)==ordinary); updates=1;
    int jaw=damage(&system,&context,60); assert(jaw>ordinary);
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_BITE,TYPE_DARK,61,0)==91);
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_NORMAL,61,0)==61);
    unsigned bites[]={44,158,242,305,422,423,424,660,692,701};
    for(unsigned i=0;i<sizeof(bites)/sizeof(*bites);i++) assert(CommunityAbility_IsBiteMove(bites[i]));
    assert(!CommunityAbility_IsBiteMove(421) && !CommunityAbility_IsBiteMove(425));
    context.battleMons[2].ability=ABILITY_DARK_AURA;
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_BITE,TYPE_DARK,61,0)==121);
    int combined=damage(&system,&context,60);
    context.battleMons[0].ability=ABILITY_NONE; updates=0;
    assert(combined==damage(&system,&context,120));
    updates=1; assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_DARK,61,0)==81);
    context.battleMons[3].ability=ABILITY_DARK_AURA;
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_DARK,61,0)==81);
    context.battleMons[2].curHP=0; context.battleMons[3].moveEffectsMask=MOVE_EFFECT_ABILITY_SUPPRESSED;
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_DARK,61,0)==61);
    context.battleMons[3].moveEffectsMask=0; context.battleMons[3].ability=ABILITY_FAIRY_AURA;
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_DARK,61,0)==61);
    assert(CommunityAbility_AdjustPower(&system,&context,MOVE_TACKLE,TYPE_FAIRY,61,0)==81);

    prepare(&context); context.battleMons[0].ability=ABILITY_TECHNICIAN; context.battleMons[2].ability=ABILITY_DARK_AURA;
    updates=1; int ordered=damage(&system,&context,45);
    context.battleMons[0].ability=ABILITY_NONE; updates=0;
    assert(ordered==damage(&system,&context,90));

    prepare(&context); context.battleMons[0].ability=ABILITY_VICTORY_STAR;
    assert(CommunityAbility_AdjustAccuracy(&system,&context,0,97)==97); updates=1;
    assert(CommunityAbility_AdjustAccuracy(&system,&context,0,97)==106);
    context.battleMons[2].ability=ABILITY_VICTORY_STAR;
    assert(CommunityAbility_AdjustAccuracy(&system,&context,0,97)==116);
    context.battleMons[2].curHP=0;
    assert(CommunityAbility_AdjustAccuracy(&system,&context,0,97)==106);
    context.battleMons[0].ability=ABILITY_NONE; context.battleMons[1].ability=ABILITY_VICTORY_STAR;
    assert(CommunityAbility_AdjustAccuracy(&system,&context,0,97)==97);
    context.battleMons[2].curHP=100;
    BattleControllerPlayer_CheckMoveHitAccuracy(&system,&context,0,1,MOVE_BITE);
    assert(!(context.moveStatusFlags & MOVE_STATUS_MISSED));
    updates=0; BattleControllerPlayer_CheckMoveHitAccuracy(&system,&context,0,1,MOVE_BITE);
    assert(context.moveStatusFlags & MOVE_STATUS_MISSED);

    prepare(&context); context.battleMons[0].speed=10; context.battleMons[1].speed=100;
    context.battleMons[0].ability=ABILITY_PRANKSTER;
    assert(BattleSystem_CompareBattlerSpeed(&system,&context,0,1,FALSE)==COMPARE_SPEED_SLOWER);
    updates=1; assert(BattleSystem_CompareBattlerSpeed(&system,&context,0,1,FALSE)==COMPARE_SPEED_FASTER);
    context.battleMons[0].moveEffectsMask=MOVE_EFFECT_ABILITY_SUPPRESSED;
    assert(BattleSystem_CompareBattlerSpeed(&system,&context,0,1,FALSE)==COMPARE_SPEED_SLOWER);
    context.battleMons[0].moveEffectsMask=0; context.battleMons[0].moves[0]=MOVE_BITE;
    assert(BattleSystem_CompareBattlerSpeed(&system,&context,0,1,FALSE)==COMPARE_SPEED_SLOWER);
    assert(CommunityAbility_MovePriority(&context,0,MOVE_NONE,-2)==-2);

    BattlerData data={0}; RefreshPartyStatusMessage *message=(RefreshPartyStatusMessage*)data.data;
    partyAbilities[0]=ABILITY_SOUNDPROOF; partyAbilities[1]=ABILITY_NONE;
    partyStatuses[0]=partyStatuses[1]=MON_CONDITION_BURN;
    message->ability=ABILITY_TURBOBLAZE; message->move=MOVE_HEAL_BELL; updates=0;
    BtlIOCmd_RefreshPartyStatus(&system,&data);
    assert(partyStatuses[0]==MON_CONDITION_BURN && partyStatuses[1]==0);
    partyStatuses[0]=partyStatuses[1]=MON_CONDITION_BURN;
    message->ability=ABILITY_TURBOBLAZE; message->move=MOVE_HEAL_BELL; updates=1;
    BtlIOCmd_RefreshPartyStatus(&system,&data); assert(partyStatuses[0]==0 && partyStatuses[1]==0);
    modern=0; assert(!CommunityAbility_IgnoresDefenses(ABILITY_TURBOBLAZE));
    puts("Actual 0.1.10 ability damage/accuracy/order/defense bypass/party-refresh fixtures passed");
    return 0;
}
