#include "battle/capture_rules.h"

#include "constants/battle.h"
#include "constants/heap.h"
#include "constants/items.h"
#include "constants/species.h"
#include "battle/battle_system.h"
#include "bag.h"
#include "flags.h"
#include "party.h"
#include "pc_boxes.h"

typedef struct CaptureDebitContext {
    BattleSystem *battleSys;
    int item;
} CaptureDebitContext;

static int RemoveOneBall(void *context)
{
    CaptureDebitContext *debit = context;
    return Bag_TryRemoveItem(BattleSystem_GetBag(debit->battleSys), debit->item, 1, HEAP_ID_BATTLE);
}

BOOL CaptureRules_IsOrdinaryBall(int item)
{
    /* This public foundation's ordinary ball catalog is contiguous. */
    return item >= ITEM_MASTER_BALL && item <= ITEM_CHERISH_BALL && item != ITEM_SAFARI_BALL;
}

enum CaptureRuleResult CaptureRules_Check(BattleSystem *battleSys, BattleContext *battleCtx, int actor, int *target)
{
    CaptureRuleSlot slots[MAX_BATTLERS];
    int count = BattleSystem_GetMaxBattlers(battleSys);
    u32 battleType = BattleSystem_GetBattleType(battleSys);
    *target = -1;
    if (actor < 0 || actor >= count || count > MAX_BATTLERS || BattleSystem_GetResultMask(battleSys) != BATTLE_IN_PROGRESS
        || battleCtx->battleMons[actor].curHP <= 0 || (battleCtx->battlersSwitchingMask & FlagIndex(actor))) {
        return CAPTURE_RULE_NO_TARGET;
    }
    for (int i = 0; i < count; i++) {
        slots[i].side = BattleSystem_GetBattlerSide(battleSys, i);
        slots[i].hp = battleCtx->battleMons[i].curHP;
        slots[i].species = battleCtx->battleMons[i].species;
        slots[i].absent = (battleCtx->battlersSwitchingMask & FlagIndex(i)) != 0
            || slots[i].species > NATIONAL_DEX_COUNT;
    }
    enum CaptureRuleResult result = CaptureRules_SelectTarget(battleType, BattleSystem_GetBattlerSide(battleSys, actor), slots, count, target);
    if (result != CAPTURE_RULE_OK) {
        return result;
    }
    /* Safari/Pal Park use their separate capture scripts and counters. */
    if (battleType & (BATTLE_TYPE_SAFARI | BATTLE_TYPE_PAL_PARK)) {
        return CAPTURE_RULE_SPECIAL_MODE;
    }
    if (battleCtx->battleMons[*target].moveEffectsMask & MOVE_EFFECT_SEMI_INVULNERABLE) {
        return CAPTURE_RULE_HIDDEN;
    }
    if (battleCtx->battleMons[*target].statusVolatile & VOLATILE_CONDITION_SUBSTITUTE) {
        return CAPTURE_RULE_SUBSTITUTE;
    }
    if (Party_GetCurrentCount(BattleSystem_GetParty(battleSys, actor)) >= MAX_PARTY_SIZE
        && PCBoxes_FirstEmptyBox(BattleSystem_GetPCBoxes(battleSys)) == MAX_PC_BOXES) {
        return CAPTURE_RULE_NO_ROOM;
    }
    return CAPTURE_RULE_OK;
}

enum CaptureRuleResult CaptureRules_AuthorizeThrow(BattleSystem *battleSys, BattleContext *battleCtx, int actor, int item)
{
    int target;
    battleCtx->captureAuthorized = FALSE;
    enum CaptureRuleResult result = CaptureRules_Check(battleSys, battleCtx, actor, &target);
    if (result != CAPTURE_RULE_OK) {
        return result;
    }
    if (!CaptureRules_IsOrdinaryBall(item)) {
        return CAPTURE_RULE_NO_BALL;
    }
    /* This is the only ordinary throw debit. Failure never schedules animation.
     * Tutorials have a disposable demo Bag populated by FieldBattleDTO_New.
     */
    CaptureDebitContext debit = { battleSys, item };
    result = CaptureRules_CommitDebit(battleCtx->totalTurns, actor, &battleCtx->captureActionTurn,
        &battleCtx->captureActionsUsedMask, RemoveOneBall, &debit);
    if (result != CAPTURE_RULE_OK) {
        return result;
    }
    battleCtx->captureTarget = target;
    battleCtx->captureActor = actor;
    battleCtx->captureBall = item;
    battleCtx->captureAuthorized = TRUE;
    battleCtx->defender = target;
    return CAPTURE_RULE_OK;
}

BOOL CaptureRules_TakeAuthorization(BattleContext *battleCtx, int item, int *target)
{
    BOOL authorized = battleCtx->captureAuthorized;
    battleCtx->captureAuthorized = FALSE;
    if (!authorized || battleCtx->captureBall != item || battleCtx->captureActor != battleCtx->attacker
        || battleCtx->captureActionTurn != battleCtx->totalTurns) {
        return FALSE;
    }
    *target = battleCtx->captureTarget;
    return TRUE;
}
