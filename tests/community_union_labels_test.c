// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the actual controller with actual native headers and disposable GFX endpoints.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/port/community_union_labels.c"

struct FontOAM {int drawn,x,y;};
typedef struct TestActor {VecFx32 position;int hidden,active,drawable;} TestActor;
static TestActor actors[12];
static MapObject *slots[12];
static CommunityUnionRoster snapshot;
static int fonts,managers,sprites,palettes,ranges,windows,updates;
static int failAllocation,failFont,failSprite,failManager,failPalette,foreignPalette;
static int projectResult,projectX=100,projectY=100;
static unsigned lastStringCount;
static NNSG2dImagePaletteProxy proxy;
static int managerToken,spriteToken;

void CommunityUnion_CopyRoster(CommunityUnionRoster *out){*out=snapshot;}
MapObject *MapObjMan_LocalMapObjByIndex(const MapObjectManager *manager,int index){(void)manager;assert(index>=0&&index<12);return slots[index];}
u32 MapObject_CheckStatus(const MapObject *object,u32 flag){const TestActor *a=(const TestActor*)object;return flag==MAP_OBJ_STATUS_0?a->active:a->drawable;}
int MapObject_IsHidden(const MapObject *object){return ((const TestActor*)object)->hidden;}
const VecFx32 *MapObject_GetPos(const MapObject *object){return &((const TestActor*)object)->position;}
BOOL PlttTransfer_HasTask(int resource){assert(resource==UNION_LABEL_PALETTE_ID);return palettes||foreignPalette;}
BOOL PlttTransfer_RequestFreeSpace(const PlttTransferTaskTemplate *p){assert(!palettes&&p->data->szByte==32&&p->plttIndex==1);if(failPalette)return FALSE;palettes++;return TRUE;}
void PlttTransfer_ResetTask(int resource){assert(resource==UNION_LABEL_PALETTE_ID&&palettes==1&&!fonts&&!sprites&&!managers);palettes--;}
NNSG2dImagePaletteProxy *PlttTransfer_GetPaletteProxy(int resource){assert(resource==UNION_LABEL_PALETTE_ID&&palettes);return &proxy;}
SpriteList *SpriteList_InitRendering(int capacity,G2dRenderer *renderer,enum HeapID heap){(void)renderer;(void)heap;assert(capacity==12&&!sprites);if(failSprite)return NULL;sprites++;return (SpriteList*)&spriteToken;}
BOOL SpriteList_Delete(SpriteList *list){assert(list==(SpriteList*)&spriteToken&&sprites==1&&!fonts&&!managers);sprites--;return TRUE;}
void SpriteList_Update(const SpriteList *list){assert(list==(SpriteList*)&spriteToken);updates++;}
UnkStruct_02012744 *sub_02012744(int capacity,enum HeapID heap){(void)heap;assert(capacity==3&&!managers);if(failManager)return NULL;managers++;return (UnkStruct_02012744*)&managerToken;}
void sub_020127BC(UnkStruct_02012744 *p){assert(p==(UnkStruct_02012744*)&managerToken&&managers==1&&!fonts);managers--;}
FontOAM *sub_020127E8(const UnkStruct_020127E8 *p){assert(managers&&sprites&&ranges&&p->unk_0C==&proxy&&p->unk_04->width==16&&p->unk_04->height==2);if(failFont)return NULL;fonts++;return calloc(1,sizeof(FontOAM));}
void sub_02012870(FontOAM *font){assert(fonts>0&&ranges>0);fonts--;free(font);}
int sub_02012898(const Window *window,int type,enum HeapID heap){(void)type;(void)heap;assert(window->pixels);return 1024;}
void FontOAM_SetXY(FontOAM *font,int x,int y){font->x=x;font->y=y;}
void sub_020129D0(FontOAM *font,BOOL visible){font->drawn=visible;}
BOOL CharTransfer_AllocRange(int size,BOOL end,NNS_G2D_VRAM_TYPE type,CharTransferAllocation *allocation){(void)type;assert(size==1024&&end);if(failAllocation)return FALSE;ranges++;allocation->size=size;allocation->offset=32;return TRUE;}
void CharTransfer_ClearRange(CharTransferAllocation *allocation){assert(ranges>0&&allocation->size==1024);ranges--;memset(allocation,0,sizeof(*allocation));}
String *String_Init(u32 size,u32 heap){(void)heap;String *s=calloc(1,sizeof(String)+(size-1)*sizeof(charcode_t));s->maxSize=size;return s;}
void String_CopyNumChars(String *s,const charcode_t *text,u32 count){assert(count<=s->maxSize&&count>1&&text[count-1]==CHAR_EOS);lastStringCount=count;memcpy(s->data,text,count*sizeof(charcode_t));s->size=count-1;}
void String_Free(String *s){free(s);}
u32 Font_CalcStringWidth(enum Font font,const String *s,u32 spacing){(void)font;(void)spacing;return s->size*8;}
void Window_Init(Window *w){memset(w,0,sizeof(*w));}
void Window_AddToTopLeftCorner(BgConfig *bg,Window *w,u8 width,u8 height,u16 tile,u8 fill){(void)bg;(void)tile;(void)fill;w->width=width;w->height=height;w->pixels=calloc(1,1024);windows++;}
void Window_Remove(Window *w){assert(w->pixels&&windows>0);free(w->pixels);w->pixels=NULL;windows--;}
u8 Text_AddPrinterWithParamsAndColor(Window *w,u32 font,const String *s,u32 x,u32 y,u32 speed,TextColor color,TextPrinterCallback callback){(void)font;(void)s;(void)x;(void)y;(void)callback;assert(w->pixels&&speed==TEXT_SPEED_NO_TRANSFER&&color==TEXT_COLOR(1,2,0));return 0;}
int NNS_G3dWorldPosToScrPos(const VecFx32 *position,int *x,int *y){assert(position->y==32*FX32_ONE);*x=projectX;*y=projectY;return projectResult;}

static void Clean(void){assert(!fonts&&!managers&&!sprites&&!palettes&&!ranges&&!windows);}
static void Bind(FieldSystem *field,unsigned peer,unsigned slot){CommunityUnion_LabelsBind(field,slot,slots[slot],snapshot.peers[peer].mac);}
static void Frame(FieldSystem *field){CommunityUnion_LabelsBegin(field);Bind(field,0,1);CommunityUnion_LabelsFinish();CommunityUnion_LabelsRender(field);}
int main(void)
{
    FieldSystem field={0},other={0};unsigned i;
    snapshot.selected=1;snapshot.active=1;snapshot.connection=4;snapshot.epoch=8;snapshot.count=2;
    strcpy(snapshot.session,"12345678-1234-1234-1234-123456789abc");
    strcpy(snapshot.peers[0].name,"1234567890");strcpy(snapshot.peers[0].player,"100");snapshot.peers[0].mac[0]=2;snapshot.peers[0].mac[5]=1;
    strcpy(snapshot.peers[1].name,"Other");strcpy(snapshot.peers[1].player,"200");snapshot.peers[1].mac[0]=2;snapshot.peers[1].mac[5]=2;
    for(i=0;i<12;i++){slots[i]=(MapObject*)&actors[i];actors[i].active=actors[i].drawable=1;}
    Frame(&field);assert(fonts==1&&ranges==1&&lastStringCount==11&&sLabels.labels[0].font->drawn);assert(sLabels.labels[0].font->x==60);
    assert(CommunityUnion_BeaconAllowed(snapshot.peers[0].mac));{u8 unknown[6]={2,0,0,0,0,9};assert(!CommunityUnion_BeaconAllowed(unknown));}
    actors[1].hidden=1;CommunityUnion_LabelsRender(&field);assert(!sLabels.labels[0].font->drawn);actors[1].hidden=0;
    projectResult=-1;CommunityUnion_LabelsRender(&field);assert(!sLabels.labels[0].font->drawn);projectResult=0;
    projectX=2;projectY=190;CommunityUnion_LabelsRender(&field);assert(sLabels.labels[0].font->x==0&&sLabels.labels[0].font->y==176);projectX=100;projectY=100;
    strcpy(snapshot.peers[0].name,"A.");CommunityUnion_LabelsRender(&field);assert(fonts==1&&ranges==1&&!strcmp(sLabels.labels[0].renderedName,"A."));
    slots[1]=(MapObject*)&actors[11];CommunityUnion_LabelsRender(&field);assert(!fonts&&!ranges&&!sLabels.labels[0].object);
    Frame(&field);assert(fonts==1);CommunityUnion_LabelsBegin(&field);CommunityUnion_LabelsFinish();assert(!fonts&&!ranges);
    Frame(&field);CommunityUnion_LabelsBegin(&field);Bind(&field,1,2);CommunityUnion_LabelsFinish();CommunityUnion_LabelsRender(&field);assert(!sLabels.labels[0].object&&sLabels.labels[1].font&&fonts==1);
    snapshot.active=0;CommunityUnion_LabelsRender(&field);Clean();assert(!sLabels.field);
    snapshot.active=1;Frame(&field);snapshot.epoch++;CommunityUnion_LabelsRender(&field);Clean();
    Frame(&field);CommunityUnion_LabelsEnd(&other);assert(fonts==1);CommunityUnion_LabelsEnd(&field);Clean();CommunityUnion_LabelsEnd(&field);Clean();
    Frame(&field);CommunityUnion_LabelsBegin(&other);Clean();assert(sLabels.field==&other);CommunityUnion_LabelsEnd(NULL);
    failAllocation=1;Frame(&field);assert(!fonts&&!ranges&&!windows);failAllocation=0;Frame(&field);assert(fonts==1);CommunityUnion_LabelsEnd(&field);Clean();
    failFont=1;Frame(&field);assert(!fonts&&!ranges&&!windows);CommunityUnion_LabelsEnd(&field);Clean();failFont=0;
    failSprite=1;Frame(&field);Clean();CommunityUnion_LabelsEnd(&field);failSprite=0;
    failManager=1;Frame(&field);Clean();CommunityUnion_LabelsEnd(&field);failManager=0;
    failPalette=1;Frame(&field);Clean();CommunityUnion_LabelsEnd(&field);failPalette=0;
    foreignPalette=1;Frame(&field);Clean();CommunityUnion_LabelsEnd(&field);foreignPalette=0;
    snapshot.count=3;strcpy(snapshot.peers[2].name,"Third");snapshot.peers[2].mac[0]=2;snapshot.peers[2].mac[5]=3;
    CommunityUnion_LabelsBegin(&field);Bind(&field,0,1);Bind(&field,1,2);Bind(&field,2,3);CommunityUnion_LabelsFinish();CommunityUnion_LabelsRender(&field);assert(fonts==3&&ranges==3);
    CommunityUnion_LabelsEnd(&field);Clean();Frame(&field);assert(fonts==1);snapshot.connection++;CommunityUnion_LabelsRender(&field);Clean();
    Frame(&field);snapshot.session[0]='a';CommunityUnion_LabelsRender(&field);Clean();
    snapshot.selected=0;snapshot.active=0;CommunityUnion_LabelsBegin(&field);assert(CommunityUnion_BeaconAllowed(NULL));CommunityUnion_LabelsEnd(&field);Clean();
    assert(updates>5);puts("Actual Union labels: identity, clipping, refresh, slot reuse, cancellation, teardown and allocation refusal PASS");return 0;
}
