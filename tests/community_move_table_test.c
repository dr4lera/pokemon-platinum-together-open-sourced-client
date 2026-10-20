// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "move_table.h"
#include "port/community_catalog.h"
#include "port/community_modern_rules.h"
#include "constants/moves.h"
#include "overlay011/move_palettes.h"
static MoveTable legacy[471];
static CommunityCatalog catalog;
static int available, bulkReads, singleReads;
static u16 loadedPalette[16];
void PaletteData_LoadBuffer(PaletteData *data,const void *bytes,enum PaletteBufferID id,int destination,unsigned size)
{
    (void)data;assert(bytes && id==PLTTBUF_SUB_BG && destination==128 && size==32);
    memcpy(loadedPalette,bytes,32);
}
const CommunityCatalog *CommunityCatalog_Runtime(void) { return available ? &catalog : NULL; }
void NARC_ReadFromMemberByIndexPair(void *out,int archive,int member,int offset,unsigned bytes)
{
    assert(archive==1 && member==0 && offset==0 && bytes==468*sizeof(MoveTable));
    memcpy(out,legacy,bytes); ++bulkReads;
}
void NARC_ReadWholeMemberByIndexPair(void *out,int archive,int member)
{
    assert(archive==1 && member>=0 && member<468); memcpy(out,&legacy[member],sizeof(MoveTable)); ++singleReads;
}
int main(int argc,char **argv)
{
    assert(argc==4); available=atoi(argv[1]); int modern=atoi(argv[2]); int updates=atoi(argv[3]);
    _Static_assert(sizeof(MoveTable)==16,"Catalog move record and engine structure differ");
    catalog.valid=1;
    for(unsigned i=0;i<16;i++){catalog.fairyMovePalette[i*2]=(u8)i;catalog.fairyMovePalette[i*2+1]=0;}
    for(unsigned i=0;i<471;i++) { legacy[i].effect=i; legacy[i].type=0; legacy[i].power=40; legacy[i].accuracy=100; legacy[i].pp=20; }
    for(unsigned i=0;i<COMMUNITY_MOVE_OVERRIDE_COUNT;i++) {
        catalog.moves[i][0]=i+1; catalog.moves[i][2]=255;
        memset(catalog.moves[i]+4,255,6);
    }
    catalog.moves[0][0]=204; catalog.moves[0][2]=18;
    catalog.moves[0][4]=50;catalog.moves[0][5]=0;
    catalog.moves[0][6]=90;catalog.moves[0][7]=0;
    catalog.moves[0][8]=15;catalog.moves[0][9]=0;
    assert(CommunityModernRules_Enabled()==modern);
    assert(CommunityAbilityUpdates_Enabled()==updates);
    const u16 *none=GetMoveTypePalette(-1);
    assert(none && GetMoveTypePalette(-2)==none && GetMoveTypePalette(19)==none && GetMoveTypePalette(65535)==none);
    for(unsigned type=0;type<18;type++)assert(GetMoveTypePalette(type));
    const u16 *fairy=GetMoveTypePalette(18);assert(fairy);
    if(modern)for(unsigned i=0;i<16;i++)assert(fairy[i]==i);
    else assert(fairy==none);
    LoadMoveSelectPltt(NULL,TYPE_FAIRY,0,PLTTBUF_SUB_BG,8);assert(memcmp(loadedPalette,fairy,32)==0);
    struct { unsigned before; MoveTable moves[MAX_MOVES]; unsigned after; } buffer;
    memset(&buffer,0xa5,sizeof(buffer)); buffer.before=0x12345678;buffer.after=0x87654321;
    MoveTable_Load(buffer.moves);
    assert(bulkReads==1 && buffer.before==0x12345678 && buffer.after==0x87654321);
    assert(buffer.moves[467].effect==467 && buffer.moves[467].pp==20);
    assert(MoveTable_LoadParam(467,MOVEATTRIBUTE_PP)==20);
    assert(MoveTable_LoadParam(-1,MOVEATTRIBUTE_PP)==0 && MoveTable_LoadParam(MAX_MOVES,MOVEATTRIBUTE_PP)==0);
    assert(MoveTable_CalcMaxPP(1,255)==32);
    if(modern) {
        assert(buffer.moves[204].type==18 && buffer.moves[204].power==50 && buffer.moves[204].accuracy==90 && buffer.moves[204].pp==15);
        assert(MoveTable_LoadParam(204,MOVEATTRIBUTE_TYPE)==18 && MoveTable_LoadParam(204,MOVEATTRIBUTE_POWER)==50 && MoveTable_LoadParam(204,MOVEATTRIBUTE_ACCURACY)==90 && MoveTable_LoadParam(204,MOVEATTRIBUTE_PP)==15);
        const unsigned pp[]={30,15,10,10,10,15},power[]={40,40,50,90,80,95},effect[]={0,17,3,68,0,71};
        for(unsigned i=0;i<6;i++) {
            assert(buffer.moves[468+i].type==18 && buffer.moves[468+i].pp==pp[i] && buffer.moves[468+i].power==power[i] && buffer.moves[468+i].effect==effect[i]);
            assert(MoveTable_LoadParam(468+i,MOVEATTRIBUTE_PP)==pp[i] && MoveTable_LoadParam(468+i,MOVEATTRIBUTE_POWER)==power[i]);
        }
        assert(buffer.moves[MOVE_DISARMING_VOICE].range==4 && buffer.moves[MOVE_DAZZLING_GLEAM].range==4);
    } else {
        assert(buffer.moves[204].type==0 && buffer.moves[204].power==40 && MoveTable_LoadParam(204,MOVEATTRIBUTE_PP)==20);
        const MoveTable zero={0};
        for(unsigned i=468;i<MAX_MOVES;i++) assert(memcmp(&buffer.moves[i],&zero,sizeof(zero))==0 && MoveTable_LoadParam(i,MOVEATTRIBUTE_PP)==0);
    }
    // The actual gate is immutable even if the provider/environment changes.
    available=!available;
#ifdef _WIN32
    _putenv_s("PLATINUM_COMMUNITY_MODERN_RULES",modern ? "0" : "1");
    _putenv_s("PLATINUM_COMMUNITY_0110_ABILITIES",updates ? "0" : "1");
#else
    setenv("PLATINUM_COMMUNITY_MODERN_RULES",modern ? "0" : "1",1);
    setenv("PLATINUM_COMMUNITY_0110_ABILITIES",updates ? "0" : "1",1);
#endif
    assert(CommunityModernRules_Enabled()==modern && CommunityAbilityUpdates_Enabled()==updates && singleReads>0);
    printf("Actual move table: modern=%d, bounded legacy bulk/single reads, six Fairy records, cached/single overrides and immutable gate passed.\n",modern);
}
