// SPDX-License-Identifier: GPL-3.0-or-later
#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port/community_qa.h"
#include "port/community_modern_rules.h"
#include "port/community_rules.h"
#include "constants/charcode.h"
#include "constants/game_options.h"
#include "constants/heap.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constdata/const_020EA358.h"
#include "field_battle_data_transfer.h"
#include "game_options.h"
#include "heap.h"
#include "location.h"
#include "main.h"
#include "overlay_manager.h"
#include "party.h"
#include "pokedex.h"
#include "pokemon.h"
#include "save_player.h"
#include "trainer_info.h"
#include "trainer_data.h"
#include "generated/trainers.h"

typedef struct CommunityQAScene {
    ApplicationManager *battle;
    FieldBattleDTO *dto;
    BagCursor *cursor;
    u8 subscreen;
    u32 initialExp;
} CommunityQAScene;
static int sScene;

static void AddMon(Party *party, int species, int level, u32 ot, int fairyMove)
{
    Pokemon mon;
    Pokemon_InitWith(&mon,species,level,20,TRUE,0x13572468,OTID_SET,ot);
    Pokemon_SetMoveSlot(&mon,fairyMove ? MOVE_MOONBLAST : MOVE_TACKLE,0);
    Pokemon_SetMoveSlot(&mon,MOVE_SPLASH,1);
    Pokemon_SetMoveSlot(&mon,MOVE_THUNDER_WAVE,2);
    Pokemon_SetMoveSlot(&mon,MOVE_FALSE_SWIPE,3);
    Party_AddPokemon(party,&mon);
}
static BOOL Init(ApplicationManager *manager,int *state)
{
    (void)state;
    SaveData *save=((ApplicationArgs*)ApplicationManager_Args(manager))->saveData;
    CommunityQAScene *scene=ApplicationManager_NewData(manager,sizeof(*scene),HEAP_ID_APPLICATION);
    memset(scene,0,sizeof(*scene));
    // No erase/load/save API is used. The startup guard requires no stored game.
    InitPlayerStartLocation(save);
    TrainerInfo *trainer=SaveData_GetTrainerInfo(save);
    const charcode_t name[]={CHAR_Q,CHAR_A,CHAR_EOS};
    TrainerInfo_SetName(trainer,name); TrainerInfo_SetID(trainer,0x12345678); TrainerInfo_SetGender(trainer,0);
    Options_SetTextSpeed(SaveData_GetOptions(save),OPTIONS_TEXT_SPEED_FAST);
    Options_SetBattleScene(SaveData_GetOptions(save),OPTIONS_BATTLE_SCENE_ON);
    Party *party=SaveData_GetParty(save); Party_Init(party);
    AddMon(party,SPECIES_CLEFAIRY,14,0x87654321,CommunityModernRules_Enabled());
    if (sScene==2) AddMon(party,SPECIES_PIKACHU,14,0x87654321,0);
    Bag *bag=SaveData_GetBag(save);
    Bag_TryAddItem(bag,ITEM_POKE_BALL,3,HEAP_ID_APPLICATION);
    Bag_TryAddItem(bag,ITEM_MASTER_BALL,1,HEAP_ID_APPLICATION);
    u32 type=sScene==3 ? BATTLE_TYPE_TRAINER : BATTLE_TYPE_WILD_MON;
    if (sScene==2) type|=BATTLE_TYPE_DOUBLES;
    scene->dto=FieldBattleDTO_New(HEAP_ID_APPLICATION,type);
    scene->cursor=BagCursor_New(HEAP_ID_APPLICATION);
    FieldBattleDTO_InitFromGameState(scene->dto,NULL,save,MAP_HEADER_TWINLEAF_TOWN,NULL,scene->cursor,&scene->subscreen);
    FieldBattleDTO_CopyPlayerInfoToTrainerData(scene->dto);
    scene->dto->terrain=TERRAIN_PLAIN; scene->dto->background=BACKGROUND_PLAIN; scene->dto->seed=0x01234567;
    AddMon(scene->dto->parties[BATTLER_ENEMY_1],SPECIES_BIDOOF,12,0x11223344,0);
    if (sScene==2) AddMon(scene->dto->parties[BATTLER_ENEMY_1],SPECIES_STARLY,12,0x11223344,0);
    if (sScene==3) {
        scene->dto->trainerIDs[BATTLER_ENEMY_1]=TRAINER_YOUNGSTER_TRISTAN;
        Trainer_Load(TRAINER_YOUNGSTER_TRISTAN,&scene->dto->trainer[BATTLER_ENEMY_1]);
        TrainerInfo_SetName(scene->dto->trainerInfo[BATTLER_ENEMY_1],name);
    }
    // Establish the caught/merely-seen distinction through the real dex API.
    Pokedex_Capture(scene->dto->pokedex,Party_GetPokemonBySlotIndex(scene->dto->parties[BATTLER_ENEMY_1],0));
    if (sScene==2) Pokedex_Encounter(scene->dto->pokedex,Party_GetPokemonBySlotIndex(scene->dto->parties[BATTLER_ENEMY_1],1));
    scene->initialExp=Pokemon_GetValue(Party_GetPokemonBySlotIndex(scene->dto->parties[0],0),MON_DATA_EXPERIENCE,NULL);
    scene->battle=ApplicationManager_New(&gBattleApplicationTemplate,scene->dto,HEAP_ID_APPLICATION);
    fprintf(stderr,"Community QA battle started: scene=%d cap=%d foreignOT=1 modern=%d caughtBidoof=%d seenOnlyStarly=%d exp=%lu\n",sScene,CommunityRules_CurrentLevelCap(),CommunityModernRules_Enabled(),Pokedex_HasCaughtSpecies(scene->dto->pokedex,SPECIES_BIDOOF),sScene==2 ? !Pokedex_HasCaughtSpecies(scene->dto->pokedex,SPECIES_STARLY) : 0,(unsigned long)scene->initialExp);
    fflush(stderr);
    return TRUE;
}
static BOOL Main(ApplicationManager *manager,int *state)
{
    (void)state;
    CommunityQAScene *scene=ApplicationManager_Data(manager);
    return ApplicationManager_Exec(scene->battle);
}
static BOOL Exit(ApplicationManager *manager,int *state)
{
    (void)state;
    CommunityQAScene *scene=ApplicationManager_Data(manager);
    Pokemon *mon=Party_GetPokemonBySlotIndex(scene->dto->parties[0],0);
    fprintf(stderr,"Community QA battle ended: result=%d turns=%d expBefore=%lu expAfter=%lu pokeBalls=%u masterBalls=%u throws=%u\n",scene->dto->resultMask,scene->dto->totalTurnsElapsed,(unsigned long)scene->initialExp,(unsigned long)Pokemon_GetValue(mon,MON_DATA_EXPERIENCE,NULL),Bag_GetItemQuantity(scene->dto->bag,ITEM_POKE_BALL,HEAP_ID_APPLICATION),Bag_GetItemQuantity(scene->dto->bag,ITEM_MASTER_BALL,HEAP_ID_APPLICATION),scene->dto->captureAttempt->ballsThrown);
    fflush(stderr);
    ApplicationManager_Free(scene->battle); Heap_Free(scene->cursor);
    FieldBattleDTO_Free(scene->dto); ApplicationManager_FreeData(manager);
    fprintf(stderr,"Community QA scene complete; no game was saved.\n"); fflush(stderr);
    exit(0);
    return TRUE;
}
static const ApplicationManagerTemplate sTemplate={Init,Main,Exit,FS_OVERLAY_ID_NONE};
BOOL CommunityQA_TryStart(SaveData *saveData)
{
    const char *name=getenv("PLATINUM_COMMUNITY_QA_SCENE");
    if (name==NULL || *name=='\0') return FALSE;
    const char *confirm=getenv("PLATINUM_COMMUNITY_QA_CONFIRM_FRESH");
    if (confirm==NULL || strcmp(confirm,"1") || SaveData_DataExists(saveData)
        || !SaveData_IsNewGameData(saveData) || SaveData_LoadCheckStatus(saveData)!=0) {
        fprintf(stderr,"Community QA scene refused: requires explicit confirmation and a fresh profile without stored game data.\n"); fflush(stderr); return FALSE;
    }
    if (!strcmp(name,"wild-fairy")) {
        if (!CommunityModernRules_Enabled() || CommunityRules_CurrentLevelCap()!=14) {
            fprintf(stderr,"Community QA Fairy scene requires the validated modern gate and initial world cap14.\n"); fflush(stderr); return FALSE;
        }
        sScene=1;
    }
    else if (!strcmp(name,"double-balls")) sScene=2;
    else if (!strcmp(name,"trainer-balls")) sScene=3;
    else { fprintf(stderr,"Unknown community QA scene.\n"); fflush(stderr); return FALSE; }
    EnqueueApplication(FS_OVERLAY_ID_NONE,&sTemplate); return TRUE;
}
