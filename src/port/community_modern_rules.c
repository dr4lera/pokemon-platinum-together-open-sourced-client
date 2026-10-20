#include "port/community_modern_rules.h"
#ifdef PLATINUM_COMMUNITY_CLIENT
#include "port/community_catalog.h"
#include <stdlib.h>
#include <string.h>
static int sInitialized;
static int sModernEnabled;
static int sAbilityUpdates;
static void ResolveRules(void)
{
    if (!sInitialized) {
        sInitialized = 1;
        const char *choice = getenv("PLATINUM_COMMUNITY_MODERN_RULES");
        const char *updates = getenv("PLATINUM_COMMUNITY_0110_ABILITIES");
        sModernEnabled = choice != NULL && strcmp(choice, "1") == 0 && CommunityCatalog_Runtime() != NULL;
        sAbilityUpdates = sModernEnabled && updates != NULL && strcmp(updates, "1") == 0;
    }
}
#endif

int CommunityModernRules_Enabled(void)
{
#ifdef PLATINUM_COMMUNITY_CLIENT
    /* Resolve once on the game thread; changing files/environment cannot switch
     * type charts or move properties halfway through an active battle. */
    ResolveRules();
    return sModernEnabled;
#else
    return 0;
#endif
}

int CommunityAbilityUpdates_Enabled(void)
{
#ifdef PLATINUM_COMMUNITY_CLIENT
    ResolveRules();
    return sAbilityUpdates;
#else
    return 0;
#endif
}
