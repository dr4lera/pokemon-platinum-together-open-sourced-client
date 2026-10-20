#include <assert.h>
#include <stdio.h>

#include "port/sim_config_prj.h"

int main(void)
{
    SIM_Config_prj_type config = { 0 };
    SIM_Config_prj_LoadDefaults(&config);
    assert(config.worldLevelCaps);
    FILE *file = fopen("sim_config_prj.ini", "w");
    assert(file);
    fputs("[General]\n60Fps=false\n", file);
    fclose(file);
    assert(SIM_Config_prj_LoadConfigFile(&config));
    assert(config.worldLevelCaps); /* Existing configs keep the new default. */
    config.worldLevelCaps = 0;
    SIM_Config_prj_SaveConfigFile(&config);
    SIM_Config_prj_LoadDefaults(&config);
    assert(config.worldLevelCaps);
    assert(SIM_Config_prj_LoadConfigFile(&config));
    assert(!config.worldLevelCaps);
    config.worldLevelCaps = 1;
    SIM_Config_prj_SaveConfigFile(&config);
    config.worldLevelCaps = 0;
    assert(SIM_Config_prj_LoadConfigFile(&config));
    assert(config.worldLevelCaps);
    puts("community level-cap configuration: passed");
    return 0;
}
