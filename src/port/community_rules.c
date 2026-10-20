#include "port/community_rules.h"

int CommunityRules_LevelCapForBadges(int badges, int enabled, int gameCompleted)
{
    static const uint8_t caps[] = { 14, 22, 26, 32, 37, 41, 44, 50, 62 };

    if (!enabled || gameCompleted) {
        return 100;
    }
    if (badges < 0) {
        badges = 0;
    } else if (badges > 8) {
        badges = 8;
    }
    return caps[badges];
}

uint32_t CommunityRules_ClampExperience(uint32_t current, uint32_t requested, uint32_t ceiling)
{
    if (current >= ceiling) {
        return 0;
    }
    /* Subtract before comparing: even an oversized award cannot wrap. */
    uint32_t remaining = ceiling - current;
    return requested < remaining ? requested : remaining;
}
