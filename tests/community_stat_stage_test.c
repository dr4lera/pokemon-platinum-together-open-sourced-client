#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "battle/battle_context.h"
#include "battle/common.h"
#include "constants/battle/battle_script.h"
#include "constants/items.h"
#include "generated/abilities.h"
#include "port/community_catalog.h"
#include "res/text/bank/battle_strings.h"
#include "data/move_side_effect_subscripts.h"
#undef main

struct BattleSystem { int battlers; int partner; };
static int modern, catalogValid = 1, pops, abilityUpdates;
static u8 definition[12];
static CommunityCatalog catalog;
int CommunityModernRules_Enabled(void) { return modern; }
int CommunityAbilityUpdates_Enabled(void) { return abilityUpdates; }
const CommunityCatalog *CommunityCatalog_Runtime(void) { return &catalog; }
int CommunityCatalog_StatStage(const CommunityCatalog *data, unsigned move, uint8_t out[12])
{
    (void)data;
    if (!catalogValid || move != (unsigned)(definition[0] | definition[1] << 8)) return 0;
    memcpy(out, definition, 12); return 1;
}
int CommunityCatalog_SideEffectScript(const CommunityCatalog *data, unsigned pointer, unsigned *out)
{
    (void)data;
    if (!catalogValid || pointer < 145 || pointer > 177) return 0;
    *out = 297 + pointer - 145; return 1;
}
static int BattleSystem_GetMaxBattlers(BattleSystem *system) { return system->battlers; }
static int BattleSystem_GetPartner(BattleSystem *system, int battler) { (void)battler; return system->partner; }
static int BattleSystem_GetBattlerSide(BattleSystem *system, int battler) { (void)system; return battler & 1; }
static BOOL BattleSystem_PopScript(BattleContext *context) { (void)context; pops++; return TRUE; }
static int BattleSystem_NicknameTag(BattleContext *context, int battler) { (void)context; return battler + 256; }
static u8 Battler_Ability(BattleContext *context, int battler) { return context->battleMons[battler].ability; }
static s32 Battler_HeldItemEffect(BattleContext *context, int battler) { return context->battleMons[battler].heldItem; }
static BOOL Battler_IgnorableAbility(BattleContext *context, int attacker, int defender, int ability)
{ (void)attacker; return Battler_Ability(context, defender) == ability; }
static int BattleMon_Get(BattleContext *context, int battler, enum BattleMonParam param, void *out)
{ (void)out; return param == BATTLEMON_TYPE_1 ? context->battleMons[battler].type1 : context->battleMons[battler].type2; }
static BOOL BtlCmd_End(BattleSystem *, BattleContext *);
static int BattleScript_Read(BattleContext *);
static void BattleScript_Iter(BattleContext *, int);
#undef MI_CpuClearFast
#define MI_CpuClearFast(dest, size) memset(dest, 0, size)
#include "actual_stat_stage.inc"

static void prepare(BattleContext *context, unsigned move, unsigned target, unsigned predicate)
{
    memset(context, 0, sizeof(*context)); memset(definition, 0, sizeof(definition));
    definition[0] = move; definition[1] = move >> 8; definition[2] = target; definition[3] = predicate;
    context->moveCur = move; context->attacker = 0; context->defender = 1;
    context->sideEffectMon = 1; context->sideEffectType = SIDE_EFFECT_TYPE_DIRECT;
    for (int i = 0; i < 4; i++) {
        context->battleMons[i].curHP = 100;
        context->monSpeedOrder[i] = 3 - i;
        for (int j = 0; j < NUM_BOOSTABLE_STATS; j++) context->battleMons[i].statBoosts[j] = 6;
    }
    modern = catalogValid = 1;
}
static void action(BattleSystem *system, BattleContext *context, int selector, int failure)
{
    context->scriptCursor = 0;
    context->battleScript[0] = 223; context->battleScript[1] = selector;
    context->battleScript[2] = 0; context->battleScript[3] = failure;
    BtlCmd_CommunityAction(system, context);
}
static void apply(BattleSystem *system, BattleContext *context)
{
    context->scriptCursor = 0;
    memset(context->battleScript, 0, 4 * sizeof(int));
    BtlCmd_ChangeStatStage(system, context);
}
int main(void)
{
    BattleContext context;
    BattleSystem system = {4, 2};
    prepare(&context, 474, 3, 0); definition[4] = 1; definition[5] = 2;
    action(&system, &context, 0, 9);
    assert(context.scriptCursor == 4 && context.communityStatStages.count == 4);
    assert(context.communityStatStages.entries[0].battler == 0 && context.communityStatStages.entries[2].battler == 2);
    context.battleStatusMask2 |= SYSCTL_UPDATE_STAT_STAGES; context.battleStatusMask |= SYSCTL_TURN_OFF_MESSAGES;
    assert(CommunityStatStage_Next(&context));
    assert(context.sideEffectMon == 0 && context.sideEffectParam == MOVE_SUBSCRIPT_PTR_ATTACK_UP_1_STAGE);
    assert(!(context.battleStatusMask2 & SYSCTL_UPDATE_STAT_STAGES) && !(context.battleStatusMask & SYSCTL_TURN_OFF_MESSAGES));
    apply(&system, &context); assert(context.battleMons[0].statBoosts[BATTLE_STAT_ATTACK] == 7);
    context.battleStatusMask2 |= SYSCTL_UPDATE_STAT_STAGES; context.battleStatusMask |= SYSCTL_TURN_OFF_MESSAGES;
    assert(CommunityStatStage_Next(&context));
    assert(context.battleStatusMask2 & SYSCTL_UPDATE_STAT_STAGES);
    assert(context.battleStatusMask & SYSCTL_TURN_OFF_MESSAGES);
    assert(CommunityStatStage_Next(&context)); assert(context.sideEffectMon == 2);
    assert(!(context.battleStatusMask2 & SYSCTL_UPDATE_STAT_STAGES));
    while (CommunityStatStage_Next(&context)) {}
    assert(context.communityStatStages.stageChange == 0);
    assert(CommunityStatStage_Build(&system, &context));
    context.moveCur++;
    assert(!CommunityStatStage_Next(&context) && context.communityStatStages.count == 0);
    context.moveCur--;
    assert(CommunityStatStage_Build(&system, &context));
    BattleContext_Init(&context);
    assert(!CommunityStatStage_Next(&context));
    assert(context.communityStatStages.overrideMove == 0 && context.communityStatStages.stageChange == 0);

    prepare(&context, 474, 1, 0); definition[4] = 3;
    context.sideEffectMon = 2; context.defender = 1;
    assert(CommunityStatStage_Build(&system, &context)); assert(CommunityStatStage_Next(&context));
    assert(context.sideEffectMon == 2 && context.communityStatStages.stageChange == 3);
    context.battleMons[2].statBoosts[BATTLE_STAT_ATTACK] = 10;
    apply(&system, &context); assert(context.battleMons[2].statBoosts[BATTLE_STAT_ATTACK] == 12);
    assert(context.communityStatStages.stageChange == 0);

    prepare(&context, 474, 4, 0); memset(definition + 4, 1, 7);
    assert(CommunityStatStage_Build(&system, &context)); assert(context.communityStatStages.count == 24);
    assert(context.communityStatStages.entries[0].battler == 3 && context.communityStatStages.entries[7].battler == 2);
    context.battleMons[3].curHP = 0;
    assert(CommunityStatStage_Build(&system, &context)); assert(context.communityStatStages.count == 21);
    assert(context.communityStatStages.entries[0].battler == 2);

    prepare(&context, 474, 2, 0); definition[4] = 1; system.partner = 0;
    assert(!CommunityStatStage_Build(&system, &context)); assert(context.moveStatusFlags & MOVE_STATUS_FAILED);
    system.partner = 2; context.battleMons[2].curHP = 0;
    assert(!CommunityStatStage_Build(&system, &context));
    prepare(&context, 474, 0, 0); definition[4] = 1;
    context.battleMons[0].statBoosts[BATTLE_STAT_ATTACK] = MAX_STAT_STAGE;
    assert(!CommunityStatStage_Build(&system, &context)); assert(context.communityStatStages.count == 0);
    assert(context.msgBuffer.id == BattleStrings_Text_PokemonsStatsWontGoAnyHigher_Ally);

    prepare(&context, 474, 0, 3); definition[4] = -1;
    context.battleMons[0].status = MON_CONDITION_BURN;
    assert(!CommunityStatStage_Build(&system, &context));
    context.battleMons[0].status = MON_CONDITION_TOXIC_COUNTER_0;
    assert(CommunityStatStage_Build(&system, &context));
    definition[3] = 2; context.battleMons[0].ability = ABILITY_PLUS;
    assert(CommunityStatStage_Build(&system, &context));
    context.battleMons[0].ability = ABILITY_NONE;
    assert(!CommunityStatStage_Build(&system, &context));
    definition[3] = 1; context.battleMons[0].type1 = TYPE_GRASS; context.battleMons[0].type2 = TYPE_FLYING;
    assert(!CommunityStatStage_Build(&system, &context));
    context.battleMons[0].moveEffectsMask = MOVE_EFFECT_INGRAIN;
    assert(CommunityStatStage_Build(&system, &context));

    prepare(&context, 474, 1, 0); definition[4] = -2;
    assert(CommunityStatStage_Build(&system, &context)); assert(CommunityStatStage_Next(&context));
    context.battleMons[1].ability = ABILITY_CLEAR_BODY; apply(&system, &context);
    assert(context.battleMons[1].statBoosts[BATTLE_STAT_ATTACK] == 6);
    context.battleMons[1].ability = ABILITY_NONE; context.sideConditions[1].mistTurns = 1;
    apply(&system, &context); assert(context.battleMons[1].statBoosts[BATTLE_STAT_ATTACK] == 6);
    context.sideConditions[1].mistTurns = 0; context.battleMons[1].statusVolatile = VOLATILE_CONDITION_SUBSTITUTE;
    apply(&system, &context); assert(context.battleMons[1].statBoosts[BATTLE_STAT_ATTACK] == 6);
    context.battleMons[1].statusVolatile = 0; context.battleMons[1].statBoosts[BATTLE_STAT_ATTACK] = 1;
    apply(&system, &context); assert(context.battleMons[1].statBoosts[BATTLE_STAT_ATTACK] == 0);

    prepare(&context, 474, 1, 0); definition[5] = -1;
    assert(CommunityStatStage_Build(&system, &context)); assert(CommunityStatStage_Next(&context));
    context.battleMons[1].ability = ABILITY_BIG_PECKS;
    abilityUpdates = 1; apply(&system, &context);
    assert(context.battleMons[1].statBoosts[BATTLE_STAT_DEFENSE] == 6);
    abilityUpdates = 0; apply(&system, &context);
    assert(context.battleMons[1].statBoosts[BATTLE_STAT_DEFENSE] == 5);

    prepare(&context, 474, 0, 0); definition[4] = 1;
    action(&system, &context, 5, 9); assert(context.scriptCursor == 4);
    context.moveCur = 475; action(&system, &context, 5, -4); assert(context.scriptCursor == 0);
    action(&system, &context, 3, 2); assert(context.scriptCursor == 6 && (context.moveStatusFlags & MOVE_STATUS_FAILED));
    action(&system, &context, 2, 2); assert(context.moveStatusFlags & MOVE_STATUS_FAILED);
    action(&system, &context, 3, INT_MAX); assert(context.battleProgressFlag && pops == 1);
    context.scriptCursor = BATTLE_SCRIPT_SIZE_MAX - 3;
    BtlCmd_CommunityAction(&system, &context); assert(pops == 2);
    modern = 0; action(&system, &context, 0, 2); assert(context.scriptCursor == 6);
    modern = 1; catalogValid = 0; action(&system, &context, 0, 2); assert(context.scriptCursor == 6);

    prepare(&context, 727, 0, 0); definition[4] = 1; context.multiHitCounter = 2;
    assert(!CommunityStatStage_Build(&system, &context));
    context.battleMons[1].curHP = 0; assert(CommunityStatStage_Build(&system, &context));
    assert(MapSideEffectToSubscript(&context, SIDE_EFFECT_TYPE_DIRECT, MOVE_SUBSCRIPT_PTR_SLEEP) == subscript_fall_asleep);
    assert(MapSideEffectToSubscript(&context, SIDE_EFFECT_TYPE_DIRECT, 145) == 297);
    assert(MapSideEffectToSubscript(&context, SIDE_EFFECT_TYPE_DIRECT, 177) == 329);
    assert(MapSideEffectToSubscript(&context, SIDE_EFFECT_TYPE_DIRECT, 178) == subscript_but_it_failed);
    modern = 0; assert(MapSideEffectToSubscript(&context, SIDE_EFFECT_TYPE_DIRECT, 145) == subscript_but_it_failed);
    puts("Actual modern opcode/stat queue/shared stat consumer/routing fixtures passed");
    return 0;
}
