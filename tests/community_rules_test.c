#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rule_test_game.h"
#include "port/community_rules.h"

static SaveData save;
static SIM_Config_prj_type config;
SaveData *SaveData_Ptr(void) { return &save; }
TrainerInfo *SaveData_GetTrainerInfo(SaveData *s) { return &s->trainer; }
int TrainerInfo_BadgeCount(TrainerInfo *t) { return t->badges; }
VarsFlags *SaveData_GetVarsFlags(SaveData *s) { return &s->flags; }
int SystemFlag_CheckGameCompleted(VarsFlags *v) { return v->completed; }
SIM_Config_prj_type *SIM_Config_prj_GetConfig(void) { return &config; }
uint32_t Pokemon_GetValue(Pokemon *p, int field, void *out)
{
    (void)out;
    if (field == MON_DATA_EXPERIENCE) return p->exp;
    if (field == MON_DATA_SPECIES) return p->species;
    assert(field == MON_DATA_LEVEL);
    return p->level;
}
uint32_t Pokemon_GetSpeciesBaseExpAt(int species, int level)
{
    (void)species;
    /* Test growth curve, not a claim about actual species data. */
    return level * level * level;
}

int main(void)
{
    const int expected[] = { 14, 22, 26, 32, 37, 41, 44, 50, 62 };
    config.worldLevelCaps = 1;
    for (int badges = 0; badges <= 8; badges++) {
        save.trainer.badges = badges;
        assert(CommunityRules_CurrentLevelCap() == expected[badges]);
        assert(CommunityRules_LevelCapForBadges(badges, 0, 0) == 100);
        assert(CommunityRules_LevelCapForBadges(badges, 1, 1) == 100);
    }
    assert(CommunityRules_LevelCapForBadges(-1, 1, 0) == 14);
    assert(CommunityRules_LevelCapForBadges(99, 1, 0) == 62);

    const uint32_t samples[] = { 0, 1, 99, 100, 101, UINT32_MAX - 1, UINT32_MAX };
    for (unsigned c = 0; c < sizeof(samples) / sizeof(*samples); c++) {
        for (unsigned r = 0; r < sizeof(samples) / sizeof(*samples); r++) {
            for (unsigned l = 0; l < sizeof(samples) / sizeof(*samples); l++) {
                uint32_t current = samples[c], requested = samples[r], ceiling = samples[l];
                uint64_t available = current >= ceiling ? 0 : (uint64_t)ceiling - current;
                uint32_t want = requested < available ? requested : available;
                assert(CommunityRules_ClampExperience(current, requested, ceiling) == want);
            }
        }
    }

    save.trainer.badges = 0;
    Pokemon local = { .species = 1, .level = 13, .exp = 2734, .ot = 123 };
    Pokemon foreign = local;
    foreign.ot = 987;
    assert(CommunityRules_ClampPokemonExperience(&local, 500) == 10);
    assert(CommunityRules_ClampPokemonExperience(&foreign, 500) == 10);
    assert(local.ot == 123 && foreign.ot == 987);
    assert(local.exp == 2734 && foreign.exp == 2734);
    local.level = 14;
    local.exp = 2744;
    assert(CommunityRules_PokemonAtLevelCap(&local));
    assert(CommunityRules_ClampPokemonExperience(&local, UINT32_MAX) == 0);
    local.level = 45;
    local.exp = 91125;
    assert(CommunityRules_PokemonAtLevelCap(&local));
    assert(CommunityRules_ClampPokemonExperience(&local, 100) == 0);
    assert(local.level == 45 && local.exp == 91125);
    save.trainer.badges = 1;
    assert(!CommunityRules_PokemonAtLevelCap(&foreign));
    assert(CommunityRules_ClampPokemonExperience(&foreign, 500) == 500);
    config.worldLevelCaps = 0;
    assert(!CommunityRules_PokemonAtLevelCap(&local));
    assert(CommunityRules_ClampPokemonExperience(&local, 500) == 500);
    config.worldLevelCaps = 1;
    save.flags.completed = 1;
    assert(CommunityRules_CurrentLevelCap() == 100);
    assert(CommunityRules_ClampPokemonExperience(&local, 500) == 500);
    puts("community level-cap rules: passed");
    return 0;
}
