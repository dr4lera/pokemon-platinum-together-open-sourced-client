// SPDX-License-Identifier: GPL-3.0-or-later
// The runner extracts the actual room functions, retaining real SDK/engine types.
#include <assert.h>
#include <stdio.h>
#include "port/community_union_presentation.h"
#include "struct_defs/struct_0203330C.h"
#include "struct_defs/struct_0205B4F8.h"
#include "struct_defs/struct_0205C22C.h"
#include "trainer_info.h"
#include "map_object.h"
#include "unk_0205B33C.h"
#include "unk_020655F4.h"
static const u8 Unk_020ED570[10]={10,14,18,22,26,30,34,38,42,46};
static void sub_0205C444(UnkStruct_0205C680 states[],int index,int state);
static WMBssDesc beacon;
static int allowed=1,animationReady=1,refreshes,graphics=-1,binds,available=1;
static char objectTokens[51];
WMBssDesc *sub_0205B774(UnkStruct_0205B43C *room,int index){(void)room;return index==0&&available?&beacon:NULL;}
int CommunityUnion_BeaconAllowed(const uint8_t mac[6]){assert(!memcmp(mac,beacon.bssid,6));return allowed;}
void CommunityUnion_LabelsBind(struct FieldSystem_t *field,unsigned localId,MapObject *object,const uint8_t mac[6]){(void)field;(void)mac;assert(localId==1&&object==(MapObject*)&objectTokens[1]);binds++;}
MapObject *MapObjMan_LocalMapObjByIndex(const MapObjectManager *manager,int index){(void)manager;assert(index>=1&&index<=50);return (MapObject*)&objectTokens[index];}
BOOL LocalMapObj_IsAnimationSet(const MapObject *object){(void)object;return animationReady;}
void sub_02061AD4(MapObject *object,int id){assert(object==(MapObject*)&objectTokens[1]);refreshes++;graphics=id;}
u32 TrainerInfo_Gender(const TrainerInfo *trainer){return trainer->gender;}
u8 TrainerInfo_Appearance(const TrainerInfo *trainer){return trainer->appearance;}
u32 TrainerInfo_ID(const TrainerInfo *trainer){return trainer->id;}
int PalPad_TrainerIsFriend(PalPad *pad,u32 id){(void)pad;(void)id;return 0;}
#include "union_room_under_test.inc"
int main(void)
{
    UnkStruct_0205C22C room={0};UnkStruct_0203330C profile={0};TrainerInfo trainer={0};
    UnkStruct_0205B4F8 *companions=(UnkStruct_0205B4F8*)profile.unk_30;
    unsigned i;const unsigned male[]={3,5,11,31,50,51,62,70},female[]={6,7,13,14,35,37,42,63};
    beacon.bssid[0]=2;beacon.bssid[5]=1;beacon.gameInfo.userGameInfoLength=sizeof(profile);profile.unk_00=123;
    for(i=0;i<16;i++){
        memset(&room,0,sizeof(room));trainer.gender=i>=8;trainer.appearance=i>=8?female[i-8]:male[i];trainer.id=999;
        memcpy(profile.unk_10,&trainer,sizeof(trainer));memcpy(beacon.gameInfo.userGameInfo,&profile,sizeof(profile));
        sub_0205C44C(&room,NULL,NULL,NULL);assert(room.unk_0C[0].unk_08==trainer.appearance&&room.unk_0C[0].unk_00==1);
    }
    room.unk_0C[0].unk_01=2;room.unk_0C[0].unk_08=6;room.unk_0C[0].unk_0C=profile.unk_00;
    trainer.gender=0;trainer.appearance=70;memcpy(profile.unk_10,&trainer,sizeof(trainer));memcpy(beacon.gameInfo.userGameInfo,&profile,sizeof(profile));
    animationReady=0;sub_0205C44C(&room,NULL,NULL,NULL);assert(room.unk_0C[0].unk_08==6&&!refreshes);
    animationReady=1;sub_0205C44C(&room,NULL,NULL,NULL);assert(room.unk_0C[0].unk_08==70&&refreshes==1&&graphics==70);
    sub_0205C44C(&room,NULL,NULL,NULL);assert(refreshes==1);
    trainer.appearance=255;trainer.gender=255;memcpy(profile.unk_10,&trainer,sizeof(trainer));memcpy(beacon.gameInfo.userGameInfo,&profile,sizeof(profile));
    sub_0205C44C(&room,NULL,NULL,NULL);assert(graphics==3&&refreshes==2);
    allowed=0;binds=0;sub_0205C44C(&room,NULL,NULL,NULL);assert(!binds&&room.unk_0C[0].unk_00==3);allowed=1;
    beacon.gameInfo.userGameInfoLength=sizeof(profile)-1;binds=0;sub_0205C44C(&room,NULL,NULL,NULL);assert(!binds);beacon.gameInfo.userGameInfoLength=sizeof(profile);
    memset(&room,0,sizeof(room));room.unk_0C[0].unk_0C=profile.unk_00;
    companions->unk_18[0]=(1<<7)|63;companions->unk_18[1]=127;
    memcpy(beacon.gameInfo.userGameInfo,&profile,sizeof(profile));
    assert(sub_0205C340(&room,0,&beacon,NULL));assert(room.unk_0C[10].unk_08==63&&room.unk_0C[11].unk_08==3);
    puts("Actual native room hooks: all16 choices, busy refresh, invalid appearance, short/unbound beacon and companions PASS");return 0;
}
