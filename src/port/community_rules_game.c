#include <nitro.h>

#include "port/community_rules.h"
#include "port/sim_config_prj.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "system_flags.h"
#include "trainer_info.h"
#include "vars_flags.h"

int CommunityRules_CurrentLevelCap(void)
{
    SaveData *saveData = SaveData_Ptr();
    if (saveData == NULL) {
        return CommunityRules_LevelCapForBadges(0, SIM_Config_prj_GetConfig()->worldLevelCaps, 0);
    }
    return CommunityRules_LevelCapForBadges(
        TrainerInfo_BadgeCount(SaveData_GetTrainerInfo(saveData)),
        SIM_Config_prj_GetConfig()->worldLevelCaps,
        SystemFlag_CheckGameCompleted(SaveData_GetVarsFlags(saveData)));
}

uint32_t CommunityRules_ClampPokemonExperience(Pokemon *mon, uint32_t requested)
{
    int cap = CommunityRules_CurrentLevelCap();
    if (cap >= 100) {
        return requested;
    }

    uint32_t current = Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL);
    uint32_t ceiling = Pokemon_GetSpeciesBaseExpAt(Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), cap);
    return CommunityRules_ClampExperience(current, requested, ceiling);
}

int CommunityRules_PokemonAtLevelCap(Pokemon *mon)
{
    int cap = CommunityRules_CurrentLevelCap();
    return cap < 100 && Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL) >= (uint32_t)cap;
}
