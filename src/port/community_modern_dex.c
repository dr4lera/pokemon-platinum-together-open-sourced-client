#include "port/community_modern_dex.h"

#include <string.h>

/* CDX1 uses bit7 only; native language flags in bits0..6 remain opaque. */
static uint32_t Checksum(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
        }
    }
    return ~crc;
}

static unsigned Bit(const uint8_t *record, unsigned offset, unsigned index)
{
    return (record[offset + index / 8] >> (index % 8)) & 1;
}

static enum CommunityModernDexStatus Decode(const uint8_t *carrier, size_t length,
    uint8_t record[COMMUNITY_MODERN_DEX_RECORD_BYTES])
{
    if (carrier == NULL || length != COMMUNITY_MODERN_DEX_CARRIER_BYTES) {
        return COMMUNITY_MODERN_DEX_INVALID;
    }
    memset(record, 0, COMMUNITY_MODERN_DEX_RECORD_BYTES);
    unsigned nonzero = 0;
    for (unsigned i = 0; i < COMMUNITY_MODERN_DEX_CARRIER_BYTES; ++i) {
        unsigned bit = carrier[i] >> 7;
        record[i / 8] |= (uint8_t)(bit << (i % 8));
        nonzero |= bit;
    }
    if (!nonzero) {
        return COMMUNITY_MODERN_DEX_LEGACY;
    }
    if (memcmp(record, "CDX1", 4) || record[4] != 1 || record[5] != 65
        || record[6] != 0 || record[7] != 2
        || (record[16] & 0xfe) || (record[25] & 0xfe) || (record[42] & 0xfc)) {
        return COMMUNITY_MODERN_DEX_INVALID;
    }
    for (unsigned i = 43; i < 58; ++i) {
        if (record[i]) {
            return COMMUNITY_MODERN_DEX_INVALID;
        }
    }
    uint32_t crc = Checksum(record, 58);
    for (unsigned i = 0; i < 4; ++i) {
        if (record[58 + i] != (uint8_t)(crc >> (i * 8))) {
            return COMMUNITY_MODERN_DEX_INVALID;
        }
    }
    for (unsigned i = 0; i < COMMUNITY_MODERN_DEX_COUNT; ++i) {
        unsigned seen = Bit(record, 8, i);
        unsigned genders = (record[26 + i / 4] >> ((i % 4) * 2)) & 3;
        if ((!seen && Bit(record, 17, i)) || (!seen && genders)) {
            return COMMUNITY_MODERN_DEX_INVALID;
        }
    }
    return COMMUNITY_MODERN_DEX_VALID;
}

int CommunityModernDex_IsSpecies(unsigned species)
{
    return species >= COMMUNITY_MODERN_DEX_FIRST
        && species - COMMUNITY_MODERN_DEX_FIRST < COMMUNITY_MODERN_DEX_COUNT;
}

enum CommunityModernDexStatus CommunityModernDex_Read(const uint8_t *carrier,
    size_t length, unsigned species, CommunityModernDexEntry *entry)
{
    if (!CommunityModernDex_IsSpecies(species) || entry == NULL) {
        return COMMUNITY_MODERN_DEX_INVALID;
    }
    uint8_t record[COMMUNITY_MODERN_DEX_RECORD_BYTES];
    enum CommunityModernDexStatus status = Decode(carrier, length, record);
    if (status == COMMUNITY_MODERN_DEX_INVALID) {
        return status;
    }
    unsigned index = species - COMMUNITY_MODERN_DEX_FIRST;
    CommunityModernDexEntry result = { 0, 0, 0 };
    if (status == COMMUNITY_MODERN_DEX_VALID) {
        result.seen = (uint8_t)Bit(record, 8, index);
        result.caught = (uint8_t)Bit(record, 17, index);
        result.genders = (record[26 + index / 4] >> ((index % 4) * 2)) & 3;
    }
    *entry = result;
    return status;
}

int CommunityModernDex_Record(uint8_t *carrier, size_t length,
    unsigned species, int caught, unsigned gender)
{
    if (!CommunityModernDex_IsSpecies(species) || (caught != 0 && caught != 1) || gender > 2) {
        return 0;
    }
    uint8_t record[COMMUNITY_MODERN_DEX_RECORD_BYTES];
    enum CommunityModernDexStatus status = Decode(carrier, length, record);
    if (status == COMMUNITY_MODERN_DEX_INVALID) {
        return 0;
    }
    if (status == COMMUNITY_MODERN_DEX_LEGACY) {
        memcpy(record, "CDX1", 4);
        record[4] = 1;
        record[5] = 65;
        record[7] = 2;
    }
    unsigned index = species - COMMUNITY_MODERN_DEX_FIRST;
    record[8 + index / 8] |= (uint8_t)(1u << (index % 8));
    if (caught) {
        record[17 + index / 8] |= (uint8_t)(1u << (index % 8));
    }
    if (gender < 2) {
        record[26 + index / 4] |= (uint8_t)(1u << ((index % 4) * 2 + gender));
    }
    uint32_t crc = Checksum(record, 58);
    for (unsigned i = 0; i < 4; ++i) {
        record[58 + i] = (uint8_t)(crc >> (i * 8));
    }
    for (unsigned i = 0; i < COMMUNITY_MODERN_DEX_CARRIER_BYTES; ++i) {
        carrier[i] = (carrier[i] & 0x7f) | (uint8_t)(((record[i / 8] >> (i % 8)) & 1) << 7);
    }
    return 1;
}
