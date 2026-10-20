#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constants/battle/battle_anim.h"

typedef int BOOL;
typedef int16_t s16;
typedef int32_t s32;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t fx32;
#define TRUE 1
#define FALSE 0
#define GF_ASSERT(value) assert(value)
#define FX32_SHIFT 12
#define FX32_ONE (1 << FX32_SHIFT)
typedef struct { fx32 x, y, z; } VecFx32;
typedef struct { s16 x, y; } Point2D;
typedef struct SPLResourceHeader { VecFx32 emitterBasePos; } SPLResourceHeader;
typedef struct SPLResource { SPLResourceHeader *header; } SPLResource;
typedef struct SPLParticle { struct SPLParticle *next; unsigned age, lifeTime; } SPLParticle;
typedef struct SPLEmitter {
    struct { SPLParticle *first; } particles;
    SPLResource *resource;
    VecFx32 position;
} SPLEmitter;
typedef struct ParticleSystem { int unused; } ParticleSystem;
typedef struct SysTask { int unused; } SysTask;
typedef struct BattleAnimSystem {
    int vars[10], attacker, defender, direction;
    VecFx32 world[2];
    SPLEmitter *emitter;
    ParticleSystem particles;
} BattleAnimSystem;
typedef struct BattleAnimScriptFuncCommon { BattleAnimSystem *battleAnimSys; } BattleAnimScriptFuncCommon;
typedef struct BattleAnimSpriteInfo { int unused; } BattleAnimSpriteInfo;
static void *taskContext;
static int ended, deleted, freed;

/* Chosen endpoint deltas divide exactly; this test double performs only the
 * fixed-point divide, while the actual engine lerp functions execute below. */
static fx32 FX_Div(fx32 numerator, fx32 denominator)
{ assert(denominator); return (fx32)(((int64_t)numerator * FX32_ONE) / denominator); }
static int BattleAnimSystem_GetScriptVar(BattleAnimSystem *system, int var) { return system->vars[var]; }
static SPLEmitter *BattleAnimSystem_GetEmitter(BattleAnimSystem *system, int id)
{ assert(id == 1); return system->emitter; }
static ParticleSystem *BattleAnimSystem_GetCurrentParticleSystem(BattleAnimSystem *system) { return &system->particles; }
static int BattleAnimSystem_GetAttacker(BattleAnimSystem *system) { return system->attacker; }
static int BattleAnimSystem_GetDefender(BattleAnimSystem *system) { return system->defender; }
static void *BattleAnimUtil_Alloc(BattleAnimSystem *system, size_t bytes)
{ (void)system; void *result = calloc(1, bytes); assert(result); return result; }
static void BattleAnimSystem_GetCommonData(BattleAnimSystem *system, BattleAnimScriptFuncCommon *common)
{ common->battleAnimSys = system; }
static int BattleAnimUtil_GetTransformDirectionX(BattleAnimSystem *system, int attacker)
{ assert(attacker == system->attacker); return system->direction; }
static void BattleAnimUtil_GetBattlerWorldPos_Normal(BattleAnimSystem *system, int battler, VecFx32 *position)
{ assert(battler == 0 || battler == 1); *position = system->world[battler]; }
static void BattleAnimSystem_StartAnimTask(BattleAnimSystem *system, void (*func)(SysTask *, void *), void *context)
{ (void)system; (void)func; taskContext = context; }
static void ParticleSystem_DeleteEmitter(ParticleSystem *system, SPLEmitter *emitter)
{ (void)system; (void)emitter; deleted++; }
static void BattleAnimSystem_EndAnimTask(BattleAnimSystem *system, SysTask *task)
{ (void)system; (void)task; ended++; }
static void BattleAnimUtil_Free(void *context) { freed++; free(context); }
static int CalcSineDegrees_Wraparound(int angle) { (void)angle; assert(0); return 0; }
#include "actual_emitter_mode.inc"

static void Position(SPLEmitter *emitter, int x, int y, int z)
{
    assert(emitter->position.x == x * BATTLE_PARTICLE_PIXEL_FACTOR + emitter->resource->header->emitterBasePos.x);
    assert(emitter->position.y == y * BATTLE_PARTICLE_PIXEL_FACTOR + emitter->resource->header->emitterBasePos.y);
    assert(emitter->position.z == z);
}

static void Case(int mode, int direction, int baseX, int baseY, int fractional)
{
    SPLResourceHeader header = {{baseX * 172, baseY * 172, 901}};
    SPLResource resource = {&header};
    SPLParticle particle = {NULL, 0, 100};
    SPLEmitter emitter = {{&particle}, &resource, {0, 0, 12345}};
    BattleAnimSystem system = {0};
    SysTask task = {0};
    system.attacker = direction > 0 ? 0 : 1;
    system.defender = 1 - system.attacker;
    system.direction = direction;
    system.world[0] = (VecFx32){10 * 172, 20 * 172, 50};
    system.world[1] = (VecFx32){110 * 172, 60 * 172, -50};
    system.emitter = &emitter;
    system.vars[0] = 1;
    system.vars[1] = 10;
    system.vars[2] = -20;
    system.vars[3] = 36;
    system.vars[4] = 10;
    system.vars[6] = mode;
    emitter.position.x = (-40 * 172 - fractional) + header.emitterBasePos.x;
    emitter.position.y = (100 * 172 + fractional) + header.emitterBasePos.y;
    int start = mode == 1 ? system.defender : system.attacker;
    int end = mode == 1 ? system.attacker : system.defender;
    int sx = mode == 2 ? -40 : system.world[start].x / 172;
    int sy = mode == 2 ? 100 : system.world[start].y / 172;
    int ex = system.world[end].x / 172 + 10 * direction;
    int ey = system.world[end].y / 172 - 20 * direction;
    ended = deleted = freed = 0;
    taskContext = NULL;
    BattleAnimScriptFunc_MoveEmitterA2BLinear(&system);
    EmitterAnimationContext *ctx = taskContext;
    assert(ctx && ctx->startBattler == start && ctx->endBattler == end);
    assert(ctx->timer == 0 && ctx->startDelay == 36 && ctx->frames == 10);
    assert(ctx->maxFrames == EMITTER_ANIMATION_DEFAULT_FRAMES && ctx->skipFrames == 0);
    Position(&emitter, sx, sy, 12345); // frame 0, origin added exactly once
    for (int i = 0; i < 36; i++) {
        BattleAnimTask_MoveEmitterA2BLinear(&task, ctx);
        Position(&emitter, sx, sy, 12345);
    }
    for (int i = 1; i <= 10; i++) {
        BattleAnimTask_MoveEmitterA2BLinear(&task, ctx);
        if (i == 5) Position(&emitter, (sx + ex) / 2, (sy + ey) / 2, 12345);
        if (i == 10) Position(&emitter, ex, ey, 12345);
    }
    assert(!ended && !deleted && !freed);
    BattleAnimTask_MoveEmitterA2BLinear(&task, ctx);
    Position(&emitter, ex, ey, 12345);
    assert(!ended && !deleted && !freed); // active particles keep the task alive
    particle.age = particle.lifeTime;
    BattleAnimTask_MoveEmitterA2BLinear(&task, ctx);
    assert(ended == 1 && deleted == 1 && freed == 1);
}

int main(void)
{
    for (int mode = 0; mode <= 2; mode++) {
        for (int direction = -1; direction <= 1; direction += 2) {
            Case(mode, direction, 7, -3, 0);
            Case(mode, direction, -5, 9, 0);
            Case(mode, direction, 7, -3, 86);
        }
    }
    puts("Actual emitter modes 0/1/2, both perspectives, origin offsets, lerp and lifecycle passed");
    return 0;
}
