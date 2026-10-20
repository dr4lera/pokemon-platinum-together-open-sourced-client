#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "generated/pokemon_types.h"
#include "generated/moves.h"
#include "generated/pokemon_contest_types.h"
#include "res/graphics/battle/sprites.naix"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define TYPE_MULTI_IMMUNE 0
#define TYPE_MULTI_NOT_VERY_EFF 5
#define TYPE_MULTI_SUPER_EFF 20
#define ALIGN_4
#define COMMUNITY_FAIRY_TYPE_ICON_INDEX 343
#define LEARNED_MOVES_MAX 4
#define NUM_VALID_MOVES (MAX_MOVES - 1)
static int modern;
int CommunityModernRules_Enabled(void) { return modern; }
typedef struct BattleSystem { unsigned random; } BattleSystem;
typedef struct BattleContext { struct { u16 moves[4]; } mon; int attacker, msgMoveTemp; } BattleContext;
#define ATTACKING_MON (battleCtx->mon)
static unsigned BattleSystem_RandNext(BattleSystem *system) { return system->random; }
static void BattleScript_Iter(BattleContext *context, int amount) { (void)context; (void)amount; }
static int Move_CanBeMetronomed(BattleSystem *system, BattleContext *context, int attacker, int move)
{ (void)system; (void)context; (void)attacker; (void)move; return TRUE; }
#include "actual_fairy_engine.inc"

int main(void)
{
    BattleSystem system = {467};
    BattleContext context = {0};
    u8 attacking, defending, multiplier;
    assert(TYPE_FAIRY == 18 && NUM_POKEMON_TYPES == 19);
    assert(MOVE_FAIRY_WIND == 468 && MOVE_MOONBLAST == 473 && MAX_MOVES == 474);
    modern = 0;
    assert(TypeMatchupCount() == 112);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_GHOST, TYPE_STEEL, TYPE_STEEL) == 20);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_DARK, TYPE_STEEL, TYPE_STEEL) == 20);
    BtlCmd_Metronome(&system, &context);
    assert(context.msgMoveTemp == 1);
    modern = 1;
    assert(TypeMatchupCount() == 122);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_GHOST, TYPE_STEEL, TYPE_STEEL) == 40);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_DARK, TYPE_STEEL, TYPE_STEEL) == 40);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_FAIRY, TYPE_DRAGON, TYPE_DRAGON) == 80);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_DRAGON, TYPE_FAIRY, TYPE_FAIRY) == 0);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_FAIRY, TYPE_FIRE, TYPE_STEEL) == 10);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_POISON, TYPE_FAIRY, TYPE_FAIRY) == 80);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_FIGHTING, TYPE_FAIRY, TYPE_FAIRY) == 20);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_BUG, TYPE_FAIRY, TYPE_FAIRY) == 20);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_DARK, TYPE_FAIRY, TYPE_FAIRY) == 20);
    assert(BattleSystem_TypeMatchupMultiplier(TYPE_NORMAL, TYPE_GHOST, TYPE_GHOST) == 0);
    assert(BattleSystem_TypeMatchup(&system, 118, &attacking, &defending, &multiplier));
    assert(attacking == 0xfe && defending == 0xfe && multiplier == 0);
    assert(BattleSystem_TypeMatchup(&system, 121, &attacking, &defending, &multiplier));
    assert(attacking == 0xff && defending == 0xff);
    assert(!BattleSystem_TypeMatchup(&system, 122, &attacking, &defending, &multiplier));
    BtlCmd_Metronome(&system, &context);
    assert(context.msgMoveTemp == MOVE_FAIRY_WIND);
    assert(NELEMS(sSoundMoves) == 13 && sSoundMoves[12] == MOVE_DISARMING_VOICE);
    assert(sMoveTypeIconIndex[TYPE_FAIRY] == 343 && sMoveTypeIconPaletteIndex[TYPE_FAIRY] == 1);
    assert(sMoveTypeIconIndex[NUM_POKEMON_TYPES + CONTEST_TYPE_COOL] == type_icons_cool_NCGR_lz);
    assert(NELEMS(sMoveTypeIconIndex) == 24);
    puts("Actual Fairy chart, gate, Metronome, sound list and icon mappings passed");
    return 0;
}
