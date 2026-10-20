// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_union_presentation.h"
#include <nitro.h>
#include <nnsys.h>
#include "field/field_system.h"
#include "bg_window.h"
#include "char_transfer.h"
#include "font.h"
#include "heap.h"
#include "map_object.h"
#include "pltt_transfer.h"
#include "sprite.h"
#include "sprite_util.h"
#include "string_gf.h"
#include "text.h"
#include "unk_02012744.h"

#define UNION_LABEL_PALETTE_ID 0x43554e4cU
typedef struct UnionLabel {
    MapObject *object;
    unsigned localId;
    int seen;
    char renderedName[11];
    int width;
    FontOAM *font;
    CharTransferAllocation allocation;
} UnionLabel;
typedef struct UnionLabels {
    FieldSystem *field;
    CommunityUnionRoster roster;
    UnionLabel labels[COMMUNITY_UNION_REMOTE_MAX];
    G2dRenderer renderer;
    SpriteList *sprites;
    UnkStruct_02012744 *fontManager;
    NNSG2dPaletteData paletteData;
    u16 palette[16];
    int paletteLoaded;
} UnionLabels;
static UnionLabels sLabels;

static void DeleteLabel(UnionLabel *label)
{
    if(label->font){sub_02012870(label->font);label->font=NULL;CharTransfer_ClearRange(&label->allocation);}
    memset(label->renderedName,0,sizeof(label->renderedName));
}
static void FreeGraphics(void)
{
    unsigned i;
    for(i=0;i<COMMUNITY_UNION_REMOTE_MAX;i++)DeleteLabel(&sLabels.labels[i]);
    if(sLabels.fontManager){sub_020127BC(sLabels.fontManager);sLabels.fontManager=NULL;}
    if(sLabels.sprites){SpriteList_Delete(sLabels.sprites);sLabels.sprites=NULL;}
    if(sLabels.paletteLoaded){PlttTransfer_ResetTask(UNION_LABEL_PALETTE_ID);sLabels.paletteLoaded=0;}
}
void CommunityUnion_LabelsEnd(FieldSystem *field)
{
    if(sLabels.field && (!field||sLabels.field==field)){FreeGraphics();memset(&sLabels,0,sizeof(sLabels));}
}
void CommunityUnion_LabelsBegin(FieldSystem *field)
{
    CommunityUnionRoster roster;
    unsigned i;
    CommunityUnion_CopyRoster(&roster);
    if(sLabels.field && (sLabels.field!=field||!CommunityUnion_SameGeneration(&sLabels.roster,&roster)))CommunityUnion_LabelsEnd(NULL);
    sLabels.field=field;sLabels.roster=roster;
    for(i=0;i<COMMUNITY_UNION_REMOTE_MAX;i++)sLabels.labels[i].seen=0;
}
int CommunityUnion_BeaconAllowed(const uint8_t mac[6])
{
    return !sLabels.roster.selected || CommunityUnion_FindPeer(&sLabels.roster,mac)>=0;
}
void CommunityUnion_LabelsBind(FieldSystem *field,unsigned localId,MapObject *object,const uint8_t mac[6])
{
    int index;
    UnionLabel *label;
    if(field!=sLabels.field||localId<1||localId>10||!object)return;
    index=CommunityUnion_FindPeer(&sLabels.roster,mac);
    if(index<0)return;
    label=&sLabels.labels[index];
    if(label->object!=object||label->localId!=localId)DeleteLabel(label);
    label->object=object;label->localId=localId;label->seen=1;
}
void CommunityUnion_LabelsFinish(void)
{
    unsigned i;
    for(i=0;i<COMMUNITY_UNION_REMOTE_MAX;i++)if(!sLabels.labels[i].seen){DeleteLabel(&sLabels.labels[i]);sLabels.labels[i].object=NULL;}
}
static int InitGraphics(void)
{
    PlttTransferTaskTemplate palette;
    if(sLabels.sprites)return 1;
    if(PlttTransfer_HasTask(UNION_LABEL_PALETTE_ID))return 0;
    memset(sLabels.palette,0,sizeof(sLabels.palette));sLabels.palette[1]=GX_RGB(31,31,31);sLabels.palette[2]=GX_RGB(0,0,0);
    sLabels.paletteData.fmt=GX_TEXFMT_PLTT16;sLabels.paletteData.bExtendedPlt=FALSE;sLabels.paletteData.szByte=sizeof(sLabels.palette);sLabels.paletteData.pRawData=sLabels.palette;
    memset(&palette,0,sizeof(palette));palette.data=&sLabels.paletteData;palette.vramType=NNS_G2D_VRAM_TYPE_2DMAIN;palette.plttIndex=1;palette.resourceID=UNION_LABEL_PALETTE_ID;
    if(!PlttTransfer_RequestFreeSpace(&palette))return 0;
    sLabels.paletteLoaded=1;
    sLabels.sprites=SpriteList_InitRendering(COMMUNITY_UNION_REMOTE_MAX*4,&sLabels.renderer,HEAP_ID_FIELD1);
    if(!sLabels.sprites){FreeGraphics();return 0;}
    sLabels.fontManager=sub_02012744(COMMUNITY_UNION_REMOTE_MAX,HEAP_ID_FIELD1);
    if(!sLabels.fontManager){FreeGraphics();return 0;}
    return 1;
}
static int CreateLabel(UnionLabel *label,const char *name)
{
    uint16_t codes[11];
    unsigned count=CommunityUnion_NameCodes(name,codes);
    Window window;
    String *string;
    int bytes;
    UnkStruct_020127E8 params;
    if(!count)return 0;
    DeleteLabel(label);
    string=String_Init(11,HEAP_ID_FIELD1);
    if(!string)return 0;
    String_CopyNumChars(string,codes,count+1);
    label->width=(int)Font_CalcStringWidth(FONT_SYSTEM,string,0);
    if(label->width<=0||label->width>128){String_Free(string);return 0;}
    Window_Init(&window);
    Window_AddToTopLeftCorner(sLabels.field->bgConfig,&window,16,2,0,0);
    if(!window.pixels){String_Free(string);return 0;}
    Text_AddPrinterWithParamsAndColor(&window,FONT_SYSTEM,string,0,0,TEXT_SPEED_NO_TRANSFER,TEXT_COLOR(1,2,0),NULL);
    String_Free(string);
    bytes=sub_02012898(&window,NNS_G2D_VRAM_TYPE_2DMAIN,HEAP_ID_FIELD1);
    memset(&label->allocation,0,sizeof(label->allocation));
    if(bytes<=0||!CharTransfer_AllocRange(bytes,TRUE,NNS_G2D_VRAM_TYPE_2DMAIN,&label->allocation)){Window_Remove(&window);return 0;}
    memset(&params,0,sizeof(params));params.unk_00=sLabels.fontManager;params.unk_04=&window;params.unk_08=sLabels.sprites;
    params.unk_0C=PlttTransfer_GetPaletteProxy(UNION_LABEL_PALETTE_ID);params.unk_14=label->allocation.offset;
    params.unk_18=0;params.unk_1C=0;params.unk_20=0;params.unk_24=0;params.unk_28=NNS_G2D_VRAM_TYPE_2DMAIN;params.heapID=HEAP_ID_FIELD1;
    label->font=sub_020127E8(&params);
    Window_Remove(&window);
    if(!label->font){CharTransfer_ClearRange(&label->allocation);return 0;}
    memcpy(label->renderedName,name,strlen(name)+1);
    return 1;
}
void CommunityUnion_LabelsRender(FieldSystem *field)
{
    CommunityUnionRoster current;
    unsigned i;
    if(sLabels.field!=field)return;
    CommunityUnion_CopyRoster(&current);
    if(!CommunityUnion_SameGeneration(&sLabels.roster,&current)){CommunityUnion_LabelsEnd(field);return;}
    sLabels.roster=current;
    for(i=0;i<current.count;i++){
        UnionLabel *label=&sLabels.labels[i];
        VecFx32 position;
        int x,y,visible;
        if(!label->object)continue;
        if(label->object!=MapObjMan_LocalMapObjByIndex(field->mapObjMan,(int)label->localId)){DeleteLabel(label);label->object=NULL;continue;}
        visible=MapObject_CheckStatus(label->object,MAP_OBJ_STATUS_0)&&MapObject_CheckStatus(label->object,MAP_OBJ_STATUS_14)&&!MapObject_IsHidden(label->object);
        if(visible){
            if(!InitGraphics())continue;
            if(!label->font||strcmp(label->renderedName,current.peers[i].name)){if(!CreateLabel(label,current.peers[i].name))continue;}
            position=*MapObject_GetPos(label->object);position.y+=32*FX32_ONE;
            visible=NNS_G3dWorldPosToScrPos(&position,&x,&y)==0;
            visible=visible&&x>=0&&x<256&&y>=0&&y<192;
            if(visible){x-=label->width/2;if(x<0)x=0;if(x>256-label->width)x=256-label->width;if(y>176)y=176;FontOAM_SetXY(label->font,x,y);}
        }
        if(label->font)sub_020129D0(label->font,visible);
    }
    if(sLabels.sprites)SpriteList_Update(sLabels.sprites);
}
