#ifndef COMMUNITY_CAPTURE_RULES_H
#define COMMUNITY_CAPTURE_RULES_H

#include <nitro.h>
#include "struct_decls/battle_system.h"
#include "battle/battle_context.h"
#include "battle/capture_rules_core.h"

BOOL CaptureRules_IsOrdinaryBall(int item);
enum CaptureRuleResult CaptureRules_Check(BattleSystem *battleSys, BattleContext *battleCtx, int actor, int *target);
enum CaptureRuleResult CaptureRules_AuthorizeThrow(BattleSystem *battleSys, BattleContext *battleCtx, int actor, int item);
BOOL CaptureRules_TakeAuthorization(BattleContext *battleCtx, int item, int *target);

#endif
