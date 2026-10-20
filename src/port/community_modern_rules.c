#include "port/community_modern_rules.h"
#ifdef PLATINUM_COMMUNITY_CLIENT
#include "port/community_catalog.h"
#include <stdlib.h>
#include <string.h>
#endif

int CommunityModernRules_Enabled(void)
{
#ifdef PLATINUM_COMMUNITY_CLIENT
    /* Resolve once on the game thread; changing files/environment cannot switch
     * type charts or move properties halfway through an active battle. */
    static int initialized;
    static int enabled;
    if (!initialized) {
        initialized = 1;
        const char *choice = getenv("PLATINUM_COMMUNITY_MODERN_RULES");
        enabled = choice != NULL && strcmp(choice, "1") == 0 && CommunityCatalog_Runtime() != NULL;
    }
    return enabled;
#else
    return 0;
#endif
}
