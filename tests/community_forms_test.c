#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "battle/battle_context.h"
#include "battle/battle_lib.h"
#include "battle/battle_system.h"
#include "battle/battle_controller_player.h"
#include "battle/common.h"
#include "battle/community_ability_rules.h"
#include "battle/community_barrier_rules.h"
#include "battle/community_forms.h"
#include "constants/battle/battle_controller.h"
#include "constants/items.h"
#include "generated/game_records.h"
#include "generated/battle_subscripts.h"
#include "generated/natures.h"
#include "pokemon.h"
#include "port/community_form_rules.h"
#include "port/community_catalog.h"
#include "res/battle/scripts/sub_seq.naix"
#undef main
#undef GF_ASSERT
#define GF_ASSERT(value) assert(value)

struct BattleSystem { int battlers; };
static Pokemon party[4];
static unsigned values[4][200];
static u8 nature[4], base[5] = { 40, 60, 80, 100, 120 };
static CommunityCatalog catalog;
static int modern = 1, loaded = -1, reads;
static int battleOver, payout, replacing, disrupted, obedienceChecks, ppChecks, setups;
int CommunityModernRules_Enabled(void) { return modern; }
int CommunityAbilityUpdates_Enabled(void) { return 0; }
const CommunityCatalog *CommunityCatalog_Runtime(void) { return &catalog; }
int CommunityCatalog_Personal(const CommunityCatalog *c, unsigned species, uint8_t out[44])
{ if (!c->valid || species < 512 || species > 576) return 0; memset(out, 0, 44); return 1; }
int CommunityCatalog_FormStats(const CommunityCatalog *c, unsigned species, unsigned form, uint8_t out[5])
{
    if (!c->valid || form != 1 || species != COMMUNITY_FORM_SCHOOL_SPECIES) {
        if (!c->valid || !((form == 1 && species == COMMUNITY_FORM_STANCE_SPECIES) || (form == 2 && species == COMMUNITY_FORM_BOND_SPECIES))) return 0;
    }
    for (unsigned i = 0; i < 5; i++) out[i] = base[i] + 20;
    return 1;
}
u32 SpeciesData_GetSpeciesValue(int species, enum SpeciesDataParam param)
{ (void)species; assert(param >= SPECIES_DATA_BASE_ATK && param <= SPECIES_DATA_BASE_SP_DEF); return base[param - SPECIES_DATA_BASE_ATK]; }
u32 Pokemon_GetValue(Pokemon *mon, enum PokemonDataParam param, void *out)
{ (void)out; assert(mon >= party && mon < party + 4 && param < 200); reads++; return values[mon - party][param]; }
void Pokemon_SetValue(Pokemon *mon, enum PokemonDataParam param, const void *value)
{ assert(param == MON_DATA_FORM); values[mon - party][param] = *(const int *)value; }
u8 Pokemon_GetNature(Pokemon *mon) { return nature[mon-party]; }
Pokemon *BattleSystem_GetPartyPokemon(BattleSystem *sys, int battler, int slot)
{ (void)sys; (void)battler; assert(slot >= 0 && slot < 4); return &party[slot]; }
int BattleSystem_GetMaxBattlers(BattleSystem *sys) { return sys->battlers; }
u8 BattleSystem_GetBattlerSide(BattleSystem *sys, int battler) { (void)sys; return battler & 1; }
static u32 FlagIndex(int bit) { return 1U << bit; }
void BattleSystem_LoadScript(BattleContext *ctx, enum NarcID narc, int member)
{ (void)ctx; (void)narc; loaded=member; }
void BattleController_EmitIncrementRecord(BattleSystem *sys, int battler, int type, int record)
{ (void)sys; (void)battler; (void)type; (void)record; }
s32 Battler_HeldItemEffect(BattleContext *ctx, int battler) { (void)ctx; (void)battler; return 0; }
s32 Battler_HeldItemPower(BattleContext *ctx, int battler, enum HeldItemPowerOp op)
{ (void)ctx; (void)battler; (void)op; return 0; }
u16 BattleSystem_RandNext(BattleSystem *sys) { (void)sys; return 0; }
int CommunityAbility_BattlerInfiltrates(BattleContext *ctx, int battler) { (void)ctx; (void)battler; return 0; }
#undef MI_CpuClearFast
#define MI_CpuClearFast(dest, size) memset(dest, 0, size)
static BOOL BattleControllerPlayer_AnyExpPayout(BattleContext *ctx, int a, int b)
{ (void)ctx; (void)a; (void)b; return payout; }
static BOOL BattleControllerPlayer_CheckBattleOver(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; return battleOver; }
static BOOL BattleControllerPlayer_ReplaceFainted(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; return replacing; }
void BattleSystem_SetupNextTurn(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; setups++; }
BOOL BattleSystem_RecoverStatusByAbility(BattleSystem *sys, BattleContext *ctx, int battler, int skipLoad)
{ (void)sys; (void)ctx; (void)battler; (void)skipLoad; return 0; }
u32 BattleSystem_GetBattleType(BattleSystem *sys) { (void)sys; return 0; }
int BattleSystem_TriggerEffectOnSwitch(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; return 0; }
static BOOL BattleControllerPlayer_ToggleSemiInvulnMons(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; return 0; }
static void BattleControllerPlayer_ClearFlags(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
void BattleSystem_SortMonActionOrder(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
void BattleSystem_SortMonSpeedOrder(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
static void BattleControllerPlayer_LoadQuickClawCheck(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
static BOOL BattleControllerPlayer_CheckStatusDisruption(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; return disrupted; }
static int BattleControllerPlayer_CheckObedience(BattleSystem *sys, BattleContext *ctx, int *next)
{ (void)sys; (void)ctx; (void)next; obedienceChecks++; return 0; }
static BOOL BattleControllerPlayer_DecrementPP(BattleSystem *sys, BattleContext *ctx)
{ (void)sys; (void)ctx; ppChecks++; return 0; }
static BOOL BattleControllerPlayer_HasNoTarget(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; return 0; }
static int BattleControllerPlayer_MoveStolen(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; return 0; }
void BattleSystem_CheckRedirectionAbilities(BattleSystem *sys, BattleContext *ctx, int battler, u16 move)
{ (void)sys; (void)ctx; (void)battler; (void)move; }
void BattleSystem_UpdateLastResort(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
void BattleSystem_UpdateMetronomeCount(BattleSystem *sys, BattleContext *ctx) { (void)sys; (void)ctx; }
#include "actual_forms.inc"

static void prepare(BattleContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx)); memset(values, 0, sizeof(values)); memset(nature, 0, sizeof(nature));
    modern=1; catalog.valid=1; loaded=-1;
    battleOver=payout=replacing=disrupted=obedienceChecks=ppChecks=setups=0;
    ctx->attacker=0; ctx->defender=1; ctx->moveCur=MOVE_TACKLE;
    for (int i=0; i<4; i++) {
        ctx->monSpeedOrder[i]=i; ctx->selectedPartySlot[i]=i;
        ctx->battleMons[i].curHP=ctx->battleMons[i].maxHP=100;
        ctx->battleMons[i].level=20; ctx->battleMons[i].personality=100+i;
        ctx->battleMons[i].attack=99; ctx->battleMons[i].defense=88;
        values[i][MON_DATA_LEVEL]=50;
    }
}
int main(void)
{
    struct BattleSystem sys={4}; BattleContext ctx;
    prepare(&ctx);
    uint8_t slots[2]={7,8};
    assert(!CommunityForm_BondAbilitySlots(541,0,slots) && slots[0]==7 && slots[1]==8);
    assert(CommunityForm_BondAbilitySlots(541,1,slots) && slots[0]==143 && slots[1]==0);
    assert(!CommunityForm_BondAbilitySlots(541,2,slots));
    values[0][MON_DATA_SPECIES]=540; values[0][MON_DATA_FRIENDSHIP]=219;
    Pokemon_ApplyCommunityEvolutionForm(&party[0],541); assert(values[0][MON_DATA_FORM]==0);
    values[0][MON_DATA_FRIENDSHIP]=220;
    Pokemon_ApplyCommunityEvolutionForm(&party[0],541); assert(values[0][MON_DATA_FORM]==1);
    values[0][MON_DATA_FORM]=0; modern=0;
    Pokemon_ApplyCommunityEvolutionForm(&party[0],541); assert(values[0][MON_DATA_FORM]==0); modern=1;
    uint16_t stats[5]={777,777,777,777,777};
    nature[0]=NATURE_LONELY; values[0][MON_DATA_ATK_IV]=31; values[0][MON_DATA_ATK_EV]=252;
    assert(Pokemon_CalcCommunityFormStats(&party[0],561,1,stats));
    assert(stats[0]==123 && stats[1]==76 && stats[2]==105 && stats[3]==125 && stats[4]==145);
    uint16_t preserved[5]; memcpy(preserved,stats,sizeof(stats));
    assert(!Pokemon_CalcCommunityFormStats(&party[0],562,1,stats) && !memcmp(stats,preserved,sizeof(stats)));
    catalog.valid=0; assert(!Pokemon_CalcCommunityFormStats(&party[0],561,0,stats)); catalog.valid=1;

    prepare(&ctx); BattleMon *mon=&ctx.battleMons[0]; mon->species=561; mon->ability=ABILITY_SCHOOLING;
    mon->moveEffectsMask=MOVE_EFFECT_POWER_TRICK; mon->level=19;
    CommunityForms_PartyLoaded(&sys,&ctx,0,&party[0]); assert(ctx.communityForms.schoolingPending==1);
    assert(!CommunityForms_CheckSchooling(&sys,&ctx,1) && !ctx.communityForms.schoolingPending);
    mon->level=20; mon->curHP=25;
    assert(!CommunityForms_CheckSchooling(&sys,&ctx,0));
    mon->curHP=26; assert(CommunityForms_CheckSchooling(&sys,&ctx,0)==BATTLE_SUBSCRIPT_COMMUNITY_SCHOOL_FORM);
    assert(mon->formNum==1 && mon->attack==85 && mon->defense==65 && mon->curHP==26 && mon->maxHP==100 && mon->ability==ABILITY_SCHOOLING);
    mon->curHP=25; assert(CommunityForms_CheckSchooling(&sys,&ctx,0)==BATTLE_SUBSCRIPT_COMMUNITY_SCHOOL_SOLO && mon->formNum==0);
    mon->moveEffectsMask=MOVE_EFFECT_ABILITY_SUPPRESSED; mon->curHP=100;
    assert(!CommunityForms_CheckSchooling(&sys,&ctx,0)); mon->moveEffectsMask=0; mon->statusVolatile=VOLATILE_CONDITION_TRANSFORM;
    assert(!CommunityForms_CheckSchooling(&sys,&ctx,0));

    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=547; mon->ability=ABILITY_STANCE_CHANGE;
    ctx.aiContext.moveTable[MOVE_TACKLE].class=CLASS_PHYSICAL;
    assert(CommunityForms_BeforeMove(&sys,&ctx)==BATTLE_SUBSCRIPT_COMMUNITY_STANCE_ATTACK && mon->formNum==1);
    assert(!CommunityForms_BeforeMove(&sys,&ctx));
    ctx.moveCur=589; assert(CommunityForms_BeforeMove(&sys,&ctx)==BATTLE_SUBSCRIPT_COMMUNITY_STANCE_DEFENSE && mon->formNum==0);
    ctx.moveCur=MOVE_GROWL; ctx.aiContext.moveTable[MOVE_GROWL].class=CLASS_STATUS; assert(!CommunityForms_BeforeMove(&sys,&ctx));

    prepare(&ctx); mon=&ctx.battleMons[1]; mon->species=562; mon->ability=ABILITY_DISGUISE; ctx.damage=-100;
    BattleControllerPlayer_UpdateHP(&sys,&ctx);
    assert(loaded==BATTLE_SUBSCRIPT_COMMUNITY_DISGUISE_BREAK && ctx.commandNext==BATTLE_CONTROL_AFTER_MOVE_MESSAGE);
    assert(mon->formNum==1 && mon->attack==99 && ctx.damage==0 && ctx.hpCalcTemp==-12 && ctx.hitDamage==0);
    assert(ctx.communityForms.brokenCount==1 && !ctx.communityForms.knockoutPending && mon->curHP==100);
    mon->formNum=0; CommunityForms_PartyLoaded(&sys,&ctx,1,&party[1]); assert(mon->formNum==1);
    prepare(&ctx); mon=&ctx.battleMons[1]; mon->species=562; mon->ability=ABILITY_DISGUISE; ctx.damage=-100;
    mon->statusVolatile=VOLATILE_CONDITION_SUBSTITUTE; mon->moveEffectsData.substituteHP=10;
    BattleControllerPlayer_UpdateHP(&sys,&ctx); assert(mon->formNum==0 && !ctx.communityForms.brokenCount && loaded==subscript_hit_substitute);
    mon->statusVolatile=0; ctx.battleMons[0].ability=ABILITY_MOLD_BREAKER; ctx.damage=-100;
    BattleControllerPlayer_UpdateHP(&sys,&ctx); assert(mon->formNum==0 && ctx.communityForms.knockoutPending==1);

    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=541; mon->ability=ABILITY_BATTLE_BOND; mon->formNum=1;
    ctx.damage=-100; BattleControllerPlayer_UpdateHP(&sys,&ctx); assert(ctx.communityForms.knockoutPending==1);
    assert(CommunityForms_AfterMove(&sys,&ctx)==BATTLE_SUBSCRIPT_COMMUNITY_BATTLE_BOND && mon->formNum==2 && ctx.communityForms.bondedCount==1);
    assert(!CommunityForms_AfterMove(&sys,&ctx) && !ctx.communityForms.knockoutPending);
    mon->formNum=1; CommunityForms_PartyLoaded(&sys,&ctx,0,&party[0]); assert(mon->formNum==2);
    ctx.defender=2; CommunityForms_MarkKnockout(&sys,&ctx); assert(!ctx.communityForms.knockoutPending);
    for(int i=0;i<40;i++) CommunityForms_Remember(ctx.communityForms.bondedPokemon,&ctx.communityForms.bondedCount,i);
    assert(ctx.communityForms.bondedCount==24 && !CommunityForms_Remembered(ctx.communityForms.bondedPokemon,24,999));
    modern=0; mon->formNum=1; ctx.communityForms.knockoutPending=1;
    assert(!CommunityForms_AfterMove(&sys,&ctx) && mon->formNum==1 && !ctx.communityForms.knockoutPending);

    /* Exercise orchestration, including one-shot return states and final KO. */
    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=547; mon->ability=ABILITY_STANCE_CHANGE;
    ctx.beforeMoveCheckState=BEFORE_MOVE_STATE_STATUS_DISRUPTION;
    disrupted=1; BattleControllerPlayer_BeforeMove(&sys,&ctx); assert(mon->formNum==0 && ctx.beforeMoveCheckState==BEFORE_MOVE_STATE_STATUS_DISRUPTION);
    disrupted=0; BattleControllerPlayer_BeforeMove(&sys,&ctx);
    assert(mon->formNum==1 && ctx.commandNext==BATTLE_CONTROL_BEFORE_MOVE && ctx.beforeMoveCheckState==BEFORE_MOVE_STATE_CHECK_OBEDIENCE && !obedienceChecks && !ppChecks);
    BattleControllerPlayer_BeforeMove(&sys,&ctx); assert(obedienceChecks==1 && ppChecks==1 && ctx.commandNext==BATTLE_CONTROL_TRY_MOVE);

    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=561; mon->ability=ABILITY_SCHOOLING;
    ctx.command=BATTLE_CONTROL_TURN_END;
    replacing=1; BattleControllerPlayer_TurnEnd(&sys,&ctx); assert(mon->formNum==0 && !setups);
    replacing=0; BattleControllerPlayer_TurnEnd(&sys,&ctx);
    assert(mon->formNum==1 && ctx.commandNext==BATTLE_CONTROL_TURN_END && !ctx.totalTurns && !setups);
    BattleControllerPlayer_TurnEnd(&sys,&ctx); assert(ctx.totalTurns==1 && setups==1);

    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=541; mon->ability=ABILITY_BATTLE_BOND; mon->formNum=1;
    ctx.command=BATTLE_CONTROL_MOVE_END; ctx.communityForms.knockoutPending=1; battleOver=1;
    BattleControllerPlayer_MoveEnd(&sys,&ctx); assert(mon->formNum==1 && ctx.communityForms.knockoutPending==1);
    battleOver=0; BattleControllerPlayer_MoveEnd(&sys,&ctx);
    assert(mon->formNum==2 && ctx.commandNext==BATTLE_CONTROL_MOVE_END && ctx.turnOrderCounter==0);
    BattleControllerPlayer_MoveEnd(&sys,&ctx);
    assert(ctx.turnOrderCounter==1 && ctx.communityForms.bondedCount==1 && ctx.communityForms.bondedPokemon[0]==100);
    BattleContext_Init(&ctx); assert(ctx.communityForms.bondedCount==1 && ctx.communityForms.bondedPokemon[0]==100);

    prepare(&ctx); mon=&ctx.battleMons[0]; mon->species=541; mon->formNum=2;
    assert(CommunityForms_AdjustPower(&ctx,595,15,0)==20);
    assert(CommunityForms_AdjustPower(&ctx,MOVE_TACKLE,40,0)==40);
    mon->statusVolatile=VOLATILE_CONDITION_TRANSFORM; assert(CommunityForms_AdjustPower(&ctx,595,15,0)==15);
    mon->statusVolatile=0; modern=0; assert(CommunityForms_AdjustPower(&ctx,595,15,0)==15); modern=1;
    assert(!CommunityForms_BondShuriken(&ctx,595,-1) && !CommunityForms_BondShuriken(&ctx,595,4));
    ctx.moveCur=595; mon->ability=ABILITY_SKILL_LINK;
    ctx.battleScript[1]=0; ctx.battleScript[2]=7;
    BtlCmd_SetMultiHit(&sys,&ctx); assert(ctx.multiHitNumHits==3 && ctx.multiHitCounter==3 && ctx.multiHitAccuracyCheck==7);
    ctx.scriptCursor=0; ctx.battleScript[1]=2; BtlCmd_SetMultiHit(&sys,&ctx); assert(ctx.multiHitNumHits==3);
    ctx.multiHitNumHits=0; ctx.scriptCursor=0; BtlCmd_SetMultiHit(&sys,&ctx); assert(ctx.multiHitNumHits==2);
    ctx.multiHitNumHits=0; ctx.scriptCursor=0; ctx.battleScript[1]=0; modern=0;
    BtlCmd_SetMultiHit(&sys,&ctx); assert(ctx.multiHitNumHits==5);
    puts("Actual form stat, evolution, phase helper and damage-routing consumers passed");
    return 0;
}
