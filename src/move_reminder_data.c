#include "move_reminder_data.h"

#include <nitro.h>
#include <string.h>

#include "heap.h"
#include "pokemon.h"
#include "port/community_learnset.h"

#define MAX_NUMBER_REMINDER_MOVES 22

MoveReminderData *MoveReminderData_Alloc(enum HeapID heapID)
{
    MoveReminderData *data = Heap_Alloc(heapID, sizeof(MoveReminderData));
    memset(data, 0, sizeof(MoveReminderData));
    return data;
}

void MoveReminderData_Free(MoveReminderData *data)
{
    Heap_Free(data);
}

u16 *MoveReminderData_GetMoves(Pokemon *mon, enum HeapID heapID)
{
    u8 i;

    u16 species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    u8 form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    u8 level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);

    u16 currentMoves[LEARNED_MOVES_MAX];
    for (i = 0; i < LEARNED_MOVES_MAX; i++) {
        currentMoves[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1 + i, NULL);
    }

    CommunityLearnset *learnset = Heap_Alloc(heapID, sizeof(CommunityLearnset));
    u16 *reminderMoves = Heap_Alloc(heapID, MAX_NUMBER_REMINDER_MOVES * sizeof(u16));

    Pokemon_LoadCommunityLearnsetOf(species, form, learnset);
    CommunityLearnset_ReminderMoves(learnset, level, currentMoves, LEARNED_MOVES_MAX, reminderMoves, MAX_NUMBER_REMINDER_MOVES);
    Heap_Free(learnset);

    return reminderMoves;
}

BOOL MoveReminderData_HasMoves(u16 *moves)
{
    return moves[0] != LEVEL_UP_MOVESET_TERMINATOR;
}
