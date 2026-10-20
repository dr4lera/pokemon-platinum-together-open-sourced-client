#ifndef COMMUNITY_FORM_RULES_H
#define COMMUNITY_FORM_RULES_H

#include <stdint.h>

enum {
    COMMUNITY_FORM_PRE_BOND_SPECIES = 540,
    COMMUNITY_FORM_BOND_SPECIES = 541,
    COMMUNITY_FORM_STANCE_SPECIES = 547,
    COMMUNITY_FORM_SCHOOL_SPECIES = 561,
    COMMUNITY_FORM_DISGUISE_SPECIES = 562,
    COMMUNITY_FORM_BOND_ABILITY = 143
};

static inline int CommunityForm_EvolvesWithBond(unsigned species, unsigned target, unsigned friendship)
{
    return species == COMMUNITY_FORM_PRE_BOND_SPECIES && target == COMMUNITY_FORM_BOND_SPECIES && friendship > 219;
}

static inline int CommunityForm_BondAbilitySlots(unsigned species, unsigned form, uint8_t abilities[2])
{
    if (species != COMMUNITY_FORM_BOND_SPECIES || form != 1 || !abilities) {
        return 0;
    }
    abilities[0] = COMMUNITY_FORM_BOND_ABILITY;
    abilities[1] = 0;
    return 1;
}

struct Pokemon;
void Pokemon_ApplyCommunityEvolutionForm(struct Pokemon *mon, unsigned targetSpecies);
int Pokemon_CalcCommunityFormStats(struct Pokemon *mon, unsigned species, unsigned form, uint16_t out[5]);

#endif
