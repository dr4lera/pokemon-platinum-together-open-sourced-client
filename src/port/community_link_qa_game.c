// SPDX-License-Identifier: GPL-3.0-or-later
#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include "port/community_link_qa.h"
#include "port/community_link_qa_internal.h"
#include "appearance.h"
#include "constants/charcode.h"
#include "constants/game_options.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "field_map_change.h"
#include "field_overworld_state.h"
#include "field_system.h"
#include "field_task.h"
#include "game_options.h"
#include "location.h"
#include "main.h"
#include "party.h"
#include "pokedex.h"
#include "pokemon.h"
#include "save_player.h"
#include "system_data.h"
#include "system_flags.h"
#include "trainer_info.h"
#include "constdata/const_020EA10C.h"

static SaveData *sSave;
static void AddMon(Party *party, int species, u32 trainerID, u32 personality)
{
    Pokemon mon;
    Pokemon_InitWith(&mon, species, 10, 20, TRUE, personality, OTID_SET, trainerID);
    Pokemon_SetMoveSlot(&mon, MOVE_TACKLE, 0);
    Pokemon_SetMoveSlot(&mon, MOVE_GROWL, 1);
    Pokemon_SetMoveSlot(&mon, MOVE_SPLASH, 2);
    Pokemon_SetMoveSlot(&mon, MOVE_FALSE_SWIPE, 3);
    if (!Party_AddPokemon(party, &mon)) {
        fprintf(stderr, "Community link QA refused: could not seed disposable party\n");
        fflush(stderr);
        exit(2);
    }
    Pokedex_Capture(SaveData_GetPokedex(sSave), &mon);
}
static BOOL EnterUnion(FieldTask *task)
{
    int *state = FieldTask_GetState(task);
    if (*state == 0) {
        *state = 1;
        SystemFlag_SetCommunicationClubAccessible(SaveData_GetVarsFlags(sSave));
        /* This pushes the normal map-change child task. Return FALSE so the
         * task runner does not pop that newly pushed task before it executes. */
        sub_02054708(task);
        return FALSE;
    }
    fprintf(stderr, "Community link QA: native Union Room transition complete; native activity menus are ready\n");
    fflush(stderr);
    return TRUE;
}
BOOL CommunityLinkQA_TryStart(SaveData *save)
{
    BOOL fresh = save && !SaveData_DataExists(save) && SaveData_IsNewGameData(save) && SaveData_LoadCheckStatus(save) == 0;
    if (!CommunityLinkQA_AuthorizeFresh(fresh)) return FALSE;
    sSave = save;
    BOOL host = CommunityLinkQA_IsHost();
    u32 id = host ? 0x51414831 : 0x51414732;
    TrainerInfo *trainer = SaveData_GetTrainerInfo(save);
    const charcode_t hostName[] = {CHAR_Q, CHAR_A, CHAR_H, CHAR_O, CHAR_S, CHAR_T, CHAR_EOS};
    const charcode_t guestName[] = {CHAR_Q, CHAR_A, CHAR_G, CHAR_U, CHAR_E, CHAR_S, CHAR_T, CHAR_EOS};
    TrainerInfo_SetName(trainer, host ? hostName : guestName);
    TrainerInfo_SetID(trainer, id);
    TrainerInfo_SetGender(trainer, host ? 0 : 1);
    TrainerInfo_SetAppearance(trainer, Appearance_CalculateFromTrainerInfo(id, TrainerInfo_Gender(trainer), 0));
    SystemData_Init(SaveData_GetSystemData(save));
    GameTime_Clear(SaveData_GetGameTime(save));
    Options_SetTextSpeed(SaveData_GetOptions(save), OPTIONS_TEXT_SPEED_FAST);
    SystemFlag_SetBagAcquired(SaveData_GetVarsFlags(save));
    Party *party = SaveData_GetParty(save);
    Party_Init(party);
    AddMon(party, host ? SPECIES_BIDOOF : SPECIES_SHINX, id, 0x13572468);
    AddMon(party, host ? SPECIES_STARLY : SPECIES_PSYDUCK, id, 0x24681357);
    FieldOverworldState *overworld = SaveData_GetFieldOverworldState(save);
    Location_Set(FieldOverworldState_GetPlayerLocation(overworld), MAP_HEADER_JUBILIFE_CITY_POKECENTER_2F, -1, 8, 6, 0);
    EnqueueApplication(FS_OVERLAY_ID_NONE, &gFieldSystemNewGameTemplate);
    return TRUE;
}
void CommunityLinkQA_Tick(FieldSystem *field)
{
    if (!sSave || !field || FieldSystem_GetSaveData(field) != sSave || !CommunityLinkQA_Update()) return;
    if (!FieldSystem_IsRunningFieldMap(field) || FieldSystem_IsRunningTask(field) || FieldSystem_HasChildProcess(field)) return;
    CommunityLinkQA_DidEnter();
    FieldSystem_CreateTask(field, EnterUnion, NULL);
}
