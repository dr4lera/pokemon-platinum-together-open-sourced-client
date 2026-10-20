#ifndef COMMUNITY_MODERN_DEX_H
#define COMMUNITY_MODERN_DEX_H

#include <stddef.h>
#include <stdint.h>

#define COMMUNITY_MODERN_DEX_FIRST 512u
#define COMMUNITY_MODERN_DEX_COUNT 65u
#define COMMUNITY_MODERN_DEX_CARRIER_BYTES 496u
#define COMMUNITY_MODERN_DEX_RECORD_BYTES 62u

enum CommunityModernDexStatus {
    COMMUNITY_MODERN_DEX_INVALID = -1,
    COMMUNITY_MODERN_DEX_LEGACY = 0,
    COMMUNITY_MODERN_DEX_VALID = 1
};

typedef struct CommunityModernDexEntry {
    uint8_t seen;
    uint8_t caught;
    /* Observed male/female mask; zero does not infer genderless. */
    uint8_t genders;
} CommunityModernDexEntry;

int CommunityModernDex_IsSpecies(unsigned species);
enum CommunityModernDexStatus CommunityModernDex_Read(const uint8_t *carrier,
    size_t length, unsigned species, CommunityModernDexEntry *entry);
/* Native gender 0=male, 1=female, 2=genderless. No input is modified on failure. */
int CommunityModernDex_Record(uint8_t *carrier, size_t length,
    unsigned species, int caught, unsigned gender);

#endif
