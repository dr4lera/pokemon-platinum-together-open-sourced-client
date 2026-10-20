/* SPDX-License-Identifier: GPL-3.0-or-later
 * Actual loader and SDK format declarations; synthetic bytes only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#define SDK_PORT 1
#define SDK_ARM9 1
#define NNS_FROM_TOOL 1
#include <nitro.h>
/* Select the actual palette/common declarations without unrelated SDK graphics
 * loaders. This fixture supplies only platform scalar/GX declarations. */
#include <nnsys/g2d/fmt/g2d_Character_data.h>
#define NNS_G2D_DATA_H_
#define NNS_G2D_LOAD_H_
#define NNS_G2D_DEBUG_FUNC_DECL_BEGIN
#define NNS_G2D_DEBUG_FUNC_DECL_END ;
#define BIN_FILE_VERSION(EXT) NNS_G2dMakeVersionData(NNS_G2D_ ## EXT ## _MAJOR_VER, NNS_G2D_ ## EXT ## _MINOR_VER)

static unsigned live_views, allocations, releases;
static int allocation_failure;
static void * PaletteMalloc(size_t size) { void * p; if (allocation_failure) return NULL; p=malloc(size); if(p){++live_views;++allocations;}return p; }
static void PaletteFree(void * p) { if(p){assert(live_views);--live_views;++releases;}free(p); }
#define malloc PaletteMalloc
#define free PaletteFree
#include "../subprojects/libntrsystem/libraries/g2d/src/load/g2d_NCL_load.c"
#undef malloc
#undef free

_Static_assert(sizeof(WIN_NNSG2dPaletteData)==16,"serialized palette layout");
_Static_assert(sizeof(WIN_NNSG2dPaletteCompressInfo)==8,"serialized compression layout");
_Static_assert(sizeof(void*)==8&&sizeof(NNSG2dPaletteData)==24,"test must use native64-bit palette layout");

typedef struct Guard { void * allocation; size_t mapping; u8 * bytes; size_t size; } Guard;
static Guard Guarded(size_t size) {
    Guard g;size_t page,committed;
#ifdef _WIN32
    SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;
#else
    page=(size_t)sysconf(_SC_PAGESIZE);
#endif
    committed=((size+page-1)/page)*page;g.mapping=committed+page;g.size=size;
#ifdef _WIN32
    g.allocation=VirtualAlloc(NULL,g.mapping,MEM_RESERVE,PAGE_NOACCESS);assert(g.allocation);assert(VirtualAlloc(g.allocation,committed,MEM_COMMIT,PAGE_READWRITE));
#else
    g.allocation=mmap(NULL,g.mapping,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(g.allocation!=MAP_FAILED);assert(mprotect(g.allocation,committed,PROT_READ|PROT_WRITE)==0);
#endif
    g.bytes=(u8*)g.allocation+committed-size;return g;
}
static void Unguard(Guard g) {
#ifdef _WIN32
    assert(VirtualFree(g.allocation,0,MEM_RELEASE));
#else
    assert(munmap(g.allocation,g.mapping)==0);
#endif
}
static void U16(u8 * p,u16 value){p[0]=(u8)value;p[1]=(u8)(value>>8);}
static void U32(u8 * p,u32 value){p[0]=(u8)value;p[1]=(u8)(value>>8);p[2]=(u8)(value>>16);p[3]=(u8)(value>>24);}
static void Palette(u8 * bytes,u32 colors) {
    u32 size=40+colors,i;memset(bytes,0,size);U32(bytes,NNS_G2D_BINFILE_SIG_PALETTEDATA);U16(bytes+4,NNS_G2D_LITTLEENDIAN_BITMARK);U16(bytes+6,BIN_FILE_VERSION(NCLR));U32(bytes+8,size);U16(bytes+12,16);U16(bytes+14,1);
    U32(bytes+16,NNS_G2D_BINBLK_SIG_PALETTEDATA);U32(bytes+20,size-16);U32(bytes+24,colors>32?GX_TEXFMT_PLTT256:GX_TEXFMT_PLTT16);U32(bytes+28,colors>512);U32(bytes+32,colors);U32(bytes+36,16);for(i=0;i<colors;i++)bytes[40+i]=(u8)(i*3+1);
}
static void Refused(u8 * file,u32 available){NNSG2dPaletteData * out=(void*)(uintptr_t)1;unsigned before=live_views;assert(!NNS_G2dGetUnpackedPaletteDataSized(file,available,&out)&&out==NULL&&live_views==before);}

int main(void) {
    Guard g=Guarded(72);u8 original[72];NNSG2dPaletteData * first,*again;unsigned i;
    Palette(g.bytes,32);memcpy(original,g.bytes,72);
    assert(NNS_G2dGetUnpackedPaletteData(g.bytes,&first));assert(first->fmt==GX_TEXFMT_PLTT16&&first->szByte==32&&!first->bExtendedPlt&&first->pRawData==g.bytes+40);assert((uintptr_t)first<(uintptr_t)g.bytes||(uintptr_t)first>=(uintptr_t)g.bytes+72);assert(memcmp(g.bytes,original,72)==0);
    for(i=0;i<130;i++){assert(NNS_G2dGetUnpackedPaletteDataSized(g.bytes,72,&again)&&again==first);assert(memcmp(first->pRawData,original+40,32)==0);}
    assert(live_views==1&&allocations==1);assert(NNS_G2dUnpackNCL((void*)(g.bytes+24))==g.bytes+40);NNS_G2dPrintPaletteData(first);assert(memcmp(g.bytes,original,72)==0);
    /* More than the removed ring's64 slots cannot overwrite a live view. */
    {
        u8 * files[130];NNSG2dPaletteData * views[130];
        for(i=0;i<130;i++){files[i]=malloc(72);assert(files[i]);Palette(files[i],32);assert(NNS_G2dGetUnpackedPaletteDataSized(files[i],72,&views[i]));assert(views[i]!=first);}
        for(i=0;i<130;i++){assert(views[i]->pRawData==files[i]+40&&views[i]->szByte==32);WIN_CheckAndFreePaletteData(files[i],files[i]+72);free(files[i]);}
        assert(live_views==1&&first->pRawData==g.bytes+40&&memcmp(first->pRawData,original+40,32)==0);
    }
    /* Sized APIs reject actual truncation before accessing inaccessible bytes. */
    for(i=0;i<72;i++)Refused(g.bytes,i);
    {Guard header=Guarded(16);memcpy(header.bytes,original,16);Refused(header.bytes,16);Unguard(header);}
    WIN_CheckAndFreePaletteData(g.bytes,g.bytes+72);assert(!live_views);
    /* nitrogfx compNum10 occupies the reserved upper format halfword in
     * Pokemon palettes. Normalize only that reviewed encoding, not any flags. */
    memcpy(g.bytes,original,72);U32(g.bytes+24,0x000a0004);
    assert(NNS_G2dGetUnpackedPaletteDataSized(g.bytes,72,&again)&&again->fmt==GX_TEXFMT_PLTT256&&again->szByte==32&&again->pRawData==g.bytes+40);
    assert(NNS_G2dUnpackNCL((void*)(g.bytes+24))==g.bytes+40);WIN_CheckAndFreePaletteData(g.bytes,g.bytes+72);assert(!live_views);
    U32(g.bytes+24,0x000b0004);Refused(g.bytes,72);U32(g.bytes+24,0x000a0003);Refused(g.bytes,72);
    {
        static const struct {unsigned offset;u32 value;} cases[]={
            {8,15},{8,73},{12,15},{12,100},{14,100},{20,0},{20,7},{20,23},{20,57},{20,UINT32_MAX},
            {24,2},{32,0},{32,33},{32,34},{32,UINT32_MAX},{36,0},{36,15},{36,17},{36,48},{36,UINT32_MAX}
        };
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){memcpy(g.bytes,original,72);U32(g.bytes+cases[i].offset,cases[i].value);Refused(g.bytes,72);}
        memcpy(g.bytes,original,72);U16(g.bytes+4,NNS_G2D_BIGENDIAN_BITMARK);Refused(g.bytes,72);memcpy(g.bytes,original,72);U16(g.bytes+6,0);Refused(g.bytes,72);
        memcpy(g.bytes,original,72);U32(g.bytes+16,0);Refused(g.bytes,72);
    }
    memcpy(g.bytes,original,72);allocation_failure=1;Refused(g.bytes,72);allocation_failure=0;
    assert(!NNS_G2dGetUnpackedPaletteDataSized(g.bytes,72,NULL));assert(!NNS_G2dGetUnpackedPaletteDataSized(NULL,72,&again)&&!again);assert(!NNS_G2dGetUnpackedPaletteData(NULL,&again)&&!again);
    {Guard large=Guarded(8232);Palette(large.bytes,8192);assert(NNS_G2dGetUnpackedPaletteDataSized(large.bytes,8232,&again)&&again->szByte==8192&&again->bExtendedPlt&&again->pRawData==large.bytes+40);assert(((u8*)again->pRawData)[8191]==(u8)(8191*3+1));WIN_CheckAndFreePaletteData(large.bytes,large.bytes+8232);Unguard(large);}
    {
        /* Sparse16-color palettes occupy96 file bytes but reserve416 VRAM
         * bytes. The full158-byte file ends at an inaccessible guard page. */
        Guard c=Guarded(158);NNSG2dPaletteCompressInfo * compression,*same;Palette(c.bytes,96);memset(c.bytes+136,0,22);U32(c.bytes+8,158);U16(c.bytes+14,2);U32(c.bytes+24,GX_TEXFMT_PLTT16);U32(c.bytes+32,416);U32(c.bytes+136,NNS_G2D_BINBLK_SIG_PALETTECOMPINFO);U32(c.bytes+140,22);U16(c.bytes+144,3);U16(c.bytes+146,0xbeef);U32(c.bytes+148,8);U16(c.bytes+152,0);U16(c.bytes+154,7);U16(c.bytes+156,12);
        assert(NNS_G2dGetUnpackedPaletteDataSized(c.bytes,158,&again)&&again->szByte==416&&again->pRawData==c.bytes+40);assert(NNS_G2dGetUnpackedPaletteCompressInfo(c.bytes,&compression));assert(compression->numPalette==3&&compression->pad16==0xbeef&&compression->pPlttIdxTbl==c.bytes+152&&((u16*)compression->pPlttIdxTbl)[2]==12);
        assert(NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,158,&same)&&same==compression);assert(NNSi_G2dUnpackNCLCmpInfo((void*)(c.bytes+144))==compression);assert(live_views==2);assert(WIN_PaletteStoredBytes(again)==96);NNS_G2dPrintPaletteData(again);
        /* Exercise the same packed-source/sparse-destination traversal used
         * by LoadPaletteEx, without reading or copying expanded-size bytes. */
        {u8 vram[416]={0};for(i=0;i<compression->numPalette;i++){u16 slot=((u16*)compression->pPlttIdxTbl)[i];memcpy(vram+slot*32,(u8*)again->pRawData+i*32,32);}assert(memcmp(vram+12*32,c.bytes+40+64,32)==0);}
        WIN_CheckAndFreePaletteData(c.bytes,c.bytes+158);assert(!live_views);
        for(i=0;i<158;i++)assert(!NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,i,&same)&&!same);
        U32(c.bytes+148,UINT32_MAX);assert(!NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,158,&same)&&!same);Refused(c.bytes,158);U32(c.bytes+148,8);
        U16(c.bytes+144,4);assert(!NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,158,&same)&&!same);Refused(c.bytes,158);U16(c.bytes+144,3);
        U16(c.bytes+156,13);assert(!NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,158,&same)&&!same);Refused(c.bytes,158);U16(c.bytes+156,12);
        U32(c.bytes+140,23);Refused(c.bytes,158);Unguard(c);
    }
    {
        Guard c=Guarded(1084);NNSG2dPaletteCompressInfo * info;Palette(c.bytes,1024);memset(c.bytes+1064,0,20);U32(c.bytes+8,1084);U16(c.bytes+14,2);U32(c.bytes+32,2048);U32(c.bytes+1064,NNS_G2D_BINBLK_SIG_PALETTECOMPINFO);U32(c.bytes+1068,20);U16(c.bytes+1072,2);U32(c.bytes+1076,8);U16(c.bytes+1080,0);U16(c.bytes+1082,3);
        assert(NNS_G2dGetUnpackedPaletteDataSized(c.bytes,1084,&again)&&again->fmt==GX_TEXFMT_PLTT256&&again->szByte==2048&&WIN_PaletteStoredBytes(again)==1024);assert(NNS_G2dGetUnpackedPaletteCompressInfoSized(c.bytes,1084,&info)&&info->numPalette==2);WIN_CheckAndFreePaletteData(c.bytes,c.bytes+1084);U16(c.bytes+1082,4);Refused(c.bytes,1084);Unguard(c);
    }
    assert(!live_views&&allocations==releases);Unguard(g);
    puts("Actual NCLR loader: guarded72-byte and sparse158-byte inputs, native64-bit views, packed16/256-color palettes with expanded VRAM sizes, repeated/many-live views, cleanup and sized malformed/truncated refusal passed. Legacy unsized APIs still require a complete declared allocation.");return 0;
}
