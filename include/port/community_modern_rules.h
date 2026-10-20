#ifndef COMMUNITY_MODERN_RULES_H
#define COMMUNITY_MODERN_RULES_H

/* Implemented by the validated runtime catalog selection, not by battle code.
 * Must remain stable while a battle is in progress. No default-on fallback. */
int CommunityModernRules_Enabled(void);
/* Separate opt-in for verified 0.1.10 ability rules, not a catalog version.
 * Captured with the modern gate and included in peer compatibility. */
int CommunityAbilityUpdates_Enabled(void);

#endif
