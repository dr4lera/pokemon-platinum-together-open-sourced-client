#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "learnset_test_game.h"
#include "port/community_learnset.h"

static uint16_t packed[COMMUNITY_LEARNSET_LEGACY_CAPACITY + 2];
static uint8_t modern[COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES + 4];
static size_t modernLength;
static size_t words;
static int useExpanded;
static int allocations;
static CommunityLearnsetEntry expanded[] = { { 512, 1 }, { 700, 20 }, { 512, 30 }, { 65534, 40 } };

void *Heap_Alloc(int heapID, size_t bytes)
{
    (void)heapID;
    void *result = malloc(bytes);
    assert(result);
    allocations++;
    return result;
}
void Heap_Free(void *p) { allocations--; free(p); }
uint32_t Pokemon_GetValue(Pokemon *mon, int field, void *out)
{
    (void)out;
    if (field == MON_DATA_SPECIES) return mon->species;
    if (field == MON_DATA_FORM) return mon->form;
    if (field == MON_DATA_LEVEL) return mon->level;
    assert(field >= MON_DATA_MOVE1 && field < MON_DATA_MOVE1 + 4);
    return mon->moves[field - MON_DATA_MOVE1];
}
enum CommunityLearnsetResult Pokemon_LoadCommunityLearnsetOf(int species, int form, CommunityLearnset *out)
{
    assert(species == 133 && form == 0);
    if (useExpanded == 1) return CommunityLearnset_FromEntries(expanded, 4, out);
    if (useExpanded == 2) return CommunityLearnset_DecodeModern(modern, modernLength, out);
    return CommunityLearnset_DecodeLegacy(packed, words, out);
}

static uint16_t Pack(unsigned level, unsigned move) { return (level << 9) | move; }
static void WriteModernWord(size_t index, uint32_t word)
{
    for (size_t i = 0; i < 4; i++) modern[index * 4 + i] = word >> (i * 8);
}
static void MakeModern(size_t count)
{
    for (size_t i = 0; i < sizeof(modern); i++) modern[i] = 0;
    for (size_t i = 0; i < count; i++) WriteModernWord(i, (1u << 16) | (512 + i));
    WriteModernWord(count, UINT32_MAX);
    modernLength = COMMUNITY_LEARNSET_MODERN_STORAGE_BYTES;
}

int main(void)
{
    CommunityLearnset learnset;
    assert(CommunityLearnset_DecodeLegacy(NULL, 0, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_DecodeLegacy(packed, 1, NULL) == COMMUNITY_LEARNSET_INVALID);
    packed[0] = UINT16_MAX;
    assert(CommunityLearnset_DecodeLegacy(packed, 1, &learnset) == COMMUNITY_LEARNSET_OK && learnset.count == 0);
    packed[0] = Pack(1, 511);
    assert(CommunityLearnset_DecodeLegacy(packed, 1, &learnset) == COMMUNITY_LEARNSET_UNTERMINATED && learnset.count == 0);
    packed[1] = UINT16_MAX;
    packed[2] = 123; /* Archive padding is outside the records. */
    assert(CommunityLearnset_DecodeLegacy(packed, 3, &learnset) == COMMUNITY_LEARNSET_OK);
    assert(learnset.count == 1 && learnset.entries[0].move == 511 && learnset.entries[0].level == 1);
    for (size_t i = 0; i < 20; i++) packed[i] = Pack(i, i + 1);
    packed[20] = UINT16_MAX;
    assert(CommunityLearnset_DecodeLegacy(packed, 21, &learnset) == COMMUNITY_LEARNSET_OK && learnset.count == 20);
    packed[20] = Pack(30, 100);
    packed[21] = UINT16_MAX;
    assert(CommunityLearnset_DecodeLegacy(packed, 22, &learnset) == COMMUNITY_LEARNSET_TOO_LONG && learnset.count == 0);
    assert(CommunityLearnset_DecodeLegacy(packed, 20, &learnset) == COMMUNITY_LEARNSET_UNTERMINATED && learnset.count == 0);

    CommunityLearnsetEntry records[] = { { 700, 20 }, { 512, 20 }, { 700, 20 }, { 400, 1 }, { 65534, 40 } };
    assert(CommunityLearnset_FromEntries(records, 5, &learnset) == COMMUNITY_LEARNSET_OK);
    uint16_t output[23];
    assert(CommunityLearnset_CopyMoveIds(&learnset, output, 23) == 5);
    assert(output[0] == 700 && output[1] == 512 && output[2] == 700 && output[3] == 400 && output[4] == 65534);
    output[2] = 999;
    assert(CommunityLearnset_CopyMoveIds(&learnset, output, 2) == 2 && output[2] == 999);
    size_t cursor = 0;
    uint16_t move = 0;
    assert(CommunityLearnset_NextAtLevel(&learnset, 20, &cursor, &move) && move == 700 && cursor == 1);
    assert(CommunityLearnset_NextAtLevel(&learnset, 20, &cursor, &move) && move == 512 && cursor == 2);
    assert(CommunityLearnset_NextAtLevel(&learnset, 20, &cursor, &move) && move == 700 && cursor == 3);
    assert(!CommunityLearnset_NextAtLevel(&learnset, 20, &cursor, &move) && cursor == 5);
    cursor = 999;
    assert(!CommunityLearnset_NextAtLevel(&learnset, 20, &cursor, &move) && cursor == 999);
    uint16_t known[] = { 512 };
    assert(CommunityLearnset_ReminderMoves(&learnset, 20, known, 1, output, 23) == 2);
    assert(output[0] == 700 && output[1] == 400 && output[2] == UINT16_MAX);
    output[1] = 999;
    assert(CommunityLearnset_ReminderMoves(&learnset, 100, NULL, 0, output, 1) == 0);
    assert(output[0] == UINT16_MAX && output[1] == 999);
    records[0].move = UINT16_MAX;
    assert(CommunityLearnset_FromEntries(records, 5, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    records[0].move = 700;
    records[0].level = 128;
    assert(CommunityLearnset_FromEntries(records, 5, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_FromEntries(NULL, COMMUNITY_LEARNSET_CAPACITY + 1, &learnset) == COMMUNITY_LEARNSET_TOO_LONG);

    /* Distinct modern byte format, bounded independently from legacy input. */
    MakeModern(23);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_OK && learnset.count == 23);
    for (size_t i = 0; i < 23; i++) assert(learnset.entries[i].move == 512 + i && learnset.entries[i].level == 1);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, NULL) == COMMUNITY_LEARNSET_INVALID);
    assert(CommunityLearnset_DecodeModern(NULL, modernLength, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_DecodeModern(modern, 0, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_DecodeModern(modern, 3, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_DecodeModern(modern, sizeof(modern), &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    assert(CommunityLearnset_DecodeModern(modern, 23 * 4, &learnset) == COMMUNITY_LEARNSET_UNTERMINATED && learnset.count == 0);
    WriteModernWord(24, 1); /* Nonzero material after sentinel is invalid padding. */
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    WriteModernWord(24, UINT32_MAX);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    MakeModern(32);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_OK && learnset.count == 32);
    uint16_t breedingMoves[50];
    for (size_t i = 0; i < 50; i++) breedingMoves[i] = 999;
    assert(CommunityLearnset_CopyMoveIds(&learnset, breedingMoves, 50) == 32);
    for (size_t i = 0; i < 32; i++) assert(breedingMoves[i] == 512 + i);
    assert(breedingMoves[32] == 999);
    MakeModern(33);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_TOO_LONG && learnset.count == 0);
    MakeModern(0);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_OK && learnset.count == 0);
    MakeModern(1);
    WriteModernWord(0, (128u << 16) | 512);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    WriteModernWord(0, (1u << 16) | UINT16_MAX);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_INVALID && learnset.count == 0);
    WriteModernWord(0, (91u << 16) | 821);
    assert(CommunityLearnset_DecodeModern(modern, 8, &learnset) == COMMUNITY_LEARNSET_OK);
    assert(learnset.count == 1 && learnset.entries[0].move == 821 && learnset.entries[0].level == 91);
    MakeModern(2);
    WriteModernWord(1, (1u << 16) | 512);
    assert(CommunityLearnset_DecodeModern(modern, modernLength, &learnset) == COMMUNITY_LEARNSET_OK);
    assert(learnset.entries[0].move == 512 && learnset.entries[1].move == 512);

    /* Actual Move Reminder consumer, using a local source-provider double. */
    Pokemon mon = { .species = 133, .form = 0, .level = 20, .moves = { 10, 0, 0, 0 } };
    packed[0] = Pack(1, 10); packed[1] = Pack(5, 20); packed[2] = Pack(5, 20);
    packed[3] = Pack(30, 30); packed[4] = Pack(2, 40); packed[5] = UINT16_MAX;
    words = 6;
    uint16_t *reminder = MoveReminderData_GetMoves(&mon, 0);
    assert(reminder[0] == 20 && reminder[1] == 40 && reminder[2] == UINT16_MAX);
    Heap_Free(reminder);
    for (size_t i = 0; i < 20; i++) packed[i] = Pack(1, i + 1);
    packed[20] = UINT16_MAX; words = 21;
    mon.moves[0] = 0;
    reminder = MoveReminderData_GetMoves(&mon, 0);
    for (size_t i = 0; i < 20; i++) assert(reminder[i] == i + 1);
    assert(reminder[20] == UINT16_MAX);
    Heap_Free(reminder);
    useExpanded = 1;
    mon.moves[0] = 512; mon.level = 40;
    reminder = MoveReminderData_GetMoves(&mon, 0);
    assert(reminder[0] == 700 && reminder[1] == 65534 && reminder[2] == UINT16_MAX);
    Heap_Free(reminder);
    useExpanded = 2;
    MakeModern(23);
    mon.moves[0] = 0; mon.level = 100;
    reminder = MoveReminderData_GetMoves(&mon, 0);
    for (size_t i = 0; i < 23; i++) assert(reminder[i] == 512 + i);
    assert(reminder[23] == UINT16_MAX);
    Heap_Free(reminder);
    MakeModern(32);
    reminder = MoveReminderData_GetMoves(&mon, 0);
    for (size_t i = 0; i < 32; i++) assert(reminder[i] == 512 + i);
    assert(reminder[32] == UINT16_MAX);
    Heap_Free(reminder);
    useExpanded = 0; words = 20; /* No terminator: empty safe result. */
    reminder = MoveReminderData_GetMoves(&mon, 0);
    assert(reminder[0] == UINT16_MAX && !MoveReminderData_HasMoves(reminder));
    Heap_Free(reminder);
    assert(allocations == 0);
    puts("community learnsets: decoder, bounded consumers and actual Move Reminder passed");
    return 0;
}
