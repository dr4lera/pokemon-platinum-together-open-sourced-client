#ifndef COMMUNITY_RULES_H
#define COMMUNITY_RULES_H

#include <stdint.h>

struct Pokemon;

/* Portable rule arithmetic; ownership is deliberately not an input. */
int CommunityRules_LevelCapForBadges(int badges, int enabled, int gameCompleted);
uint32_t CommunityRules_ClampExperience(uint32_t current, uint32_t requested, uint32_t ceiling);

/* Local client world rule until world selection and peer progression exist. */
int CommunityRules_CurrentLevelCap(void);
uint32_t CommunityRules_ClampPokemonExperience(struct Pokemon *mon, uint32_t requested);
int CommunityRules_PokemonAtLevelCap(struct Pokemon *mon);

#endif
