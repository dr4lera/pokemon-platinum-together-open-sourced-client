#!/usr/bin/env python3
"""Actual bounded lighting parser plus existing legacy reader regression.

An optional private NARC is read in place for validation; no assets are copied.
"""
import argparse
import os
from pathlib import Path
import re
import json
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "C:/msys64/ucrt64/bin/gcc.exe" if os.name == "nt" else "cc"))
parser.add_argument("--narc", type=Path)
parser.add_argument("--engine-build", type=Path, help="Configured native build used only for its public compile arguments")
args = parser.parse_args()

def function(source, name):
    match = re.search(r"(?:static )?(?:char \*|int )" + re.escape(name) + r"\([^;]+?\)\s*\{", source)
    if not match:
        raise RuntimeError("Public source function missing: " + name)
    start, end, depth = match.start(), match.end(), 1
    while depth:
        if source[end] == "{": depth += 1
        elif source[end] == "}": depth -= 1
        end += 1
    return source[start:end]

ascii_source = (root / "src/ascii_util.c").read_text()
light_source = (root / "src/overlay005/area_light.c").read_text()
legacy = "\n".join(function(ascii_source, name) for name in ("Ascii_Length", "Ascii_CopyToTerminator", "Ascii_ConvertToInt"))
legacy += "\n" + "\n".join(function(light_source, name) for name in ("AreaLightTemplate_ParseLightAttrs", "AreaLightTemplate_ParseColor"))
parent = root / ".build-tools"
parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-area-light-", dir=parent) as temporary:
    scratch = Path(temporary)
    source = scratch / "light.c"
    source.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "port/community_area_light.h"
typedef uint16_t u16;typedef uint32_t u32;typedef int32_t s32;typedef uint16_t GXRgb;
typedef struct {int16_t x,y,z;} VecFx16;
#define MAX_STRING_COPY_LEN 256
#define SCRATCH_BUFFER_SIZE 256
#define GX_COLOR_DIMS 3
#define GX_VEC_FX_DIMS 3
#define FX16_ONE 4096
#define INVALID_LIGHT_COLOR 0xffff
#define TRUE 1
#define GX_RGB(r,g,b) ((GXRgb)((r)|((g)<<5)|((b)<<10)))
''' + legacy + r'''
static const char *lines[]={"900,","1,23,23,25,-2043,-3548,110,","0,0,0,0,0,0,0,","1,4,4,16,0,0,4096,","0,0,0,0,0,0,0,","16,16,16,","14,14,14,","20,20,20,","16,16,16,","","EOF"};
static size_t Fixture(char *out,const char *ending) {
    size_t used=0;
    for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);++i) {size_t n=strlen(lines[i]);memcpy(out+used,lines[i],n);used+=n;n=strlen(ending);memcpy(out+used,ending,n);used+=n;}
    out[used]=0;return used;
}
static unsigned U16(const unsigned char *p){return p[0]|(p[1]<<8);}
static unsigned U32(const unsigned char *p){return U16(p)|(U16(p+2)<<16);}
int main(int argc,char **argv) {
    char text[4096],other[4096];size_t length,count=99;
    CommunityAreaLightRecord expected[2],actual[2],sentinel[2];
    memset(sentinel,0xa5,sizeof(sentinel));
    const char *endings[]={"\r","\n","\r\n","\r\r\n"};
    for(unsigned i=0;i<4;++i) {
        length=Fixture(text,endings[i]);count=99;
        assert(CommunityAreaLight_Parse(text,length,actual,2,&count)&&count==1);
        if(!i)memcpy(expected,actual,sizeof(actual));else assert(!memcmp(expected,actual,sizeof(actual[0])));
        assert(actual[0].endTime==900&&actual[0].validLightsMask==5);
        assert(actual[0].lightColors[0]==GX_RGB(23,23,25)&&actual[0].lightColors[2]==GX_RGB(4,4,16));
        assert(actual[0].lightVectors[0][0]==-2043&&actual[0].lightVectors[0][1]==-3548&&actual[0].lightVectors[2][2]==4096);
        assert(actual[0].materialColors[0]==GX_RGB(16,16,16)&&actual[0].materialColors[1]==GX_RGB(14,14,14));
        assert(actual[0].materialColors[2]==GX_RGB(20,20,20)&&actual[0].materialColors[3]==GX_RGB(16,16,16));
    }
    // Run the actual retained legacy functions on the same two newline forms.
    // Zero-initialize locals for deterministic examination of its empty-field
    // behavior; this still executes the existing public reader functions.
    for(unsigned i=2;i<4;++i) {
        length=Fixture(text,endings[i]);char header[256];char *next=Ascii_CopyToTerminator(text,header,'\r');
        GXRgb lights[4],colors[4];VecFx16 vectors[4];
        for(unsigned light=0;light<4;++light)next=AreaLightTemplate_ParseLightAttrs(next,&lights[light],&vectors[light]);
        for(unsigned color=0;color<4;++color)next=AreaLightTemplate_ParseColor(next,&colors[color]);
        (void)next;
        if(i==2)assert(colors[0]==GX_RGB(16,16,16)&&colors[3]==GX_RGB(16,16,16));
        else {assert(colors[0]==0&&colors[3]!=GX_RGB(16,16,16));puts("Existing legacy reader reproduced dark/shifted material colors on CRCRLF; bounded desktop parser preserves expected colors.");}
    }
    length=Fixture(text,"\r\r\n");
    memcpy(other,text,length);char *badColor=strstr(other,"23,23,25");assert(badColor);badColor[0]='9';
    memcpy(actual,sentinel,sizeof(actual));count=99;assert(!CommunityAreaLight_Parse(other,length,actual,2,&count));assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));
    for(size_t n=0;n<length-3;++n) {
        memcpy(actual,sentinel,sizeof(actual));count=99;
        if(!CommunityAreaLight_Parse(text,n,actual,2,&count))assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));
    }
    const char *invalid[]={"", "EOF\n", "900,\nEOF\n", "900,\n1,32,0,0,0,0,0,\nEOF\n", "900,\n1,,0,0,0,0,0,\nEOF\n"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {memcpy(actual,sentinel,sizeof(actual));count=99;assert(!CommunityAreaLight_Parse(invalid[i],strlen(invalid[i]),actual,2,&count));assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));}
    length=Fixture(text,"\n");memcpy(other,text,length);memcpy(other+length,"junk\n",5);
    memcpy(actual,sentinel,sizeof(actual));count=99;assert(!CommunityAreaLight_Parse(other,length+5,actual,2,&count));assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));
    assert(!CommunityAreaLight_Parse(text,length,actual,0,&count));
    // A malformed second record cannot publish an otherwise valid first one.
    char *eof=strstr(text,"EOF");assert(eof);strcpy(eof,"1000,\n1,0,0,0,0,0,0,\nEOF\n");
    memcpy(actual,sentinel,sizeof(actual));count=99;assert(!CommunityAreaLight_Parse(text,strlen(text),actual,2,&count));assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));
    // Exact record cap and stable source order use complete valid records.
    length=Fixture(text,"\n");eof=strstr(text,"EOF");assert(eof);
    char *many=malloc(131073);assert(many);size_t used=0,body=(size_t)(eof-text)-5;
    for(unsigned i=0;i<257;++i){used+=(size_t)sprintf(many+used,"%u,\n",900+i);memcpy(many+used,text+5,body);used+=body;memcpy(many+used,"EOF",3);if(i==255){count=0;assert(CommunityAreaLight_Parse(many,used+3,NULL,0,&count)&&count==256);}}
    count=99;memcpy(actual,sentinel,sizeof(actual));assert(!CommunityAreaLight_Parse(many,used+3,actual,2,&count));assert(count==99&&!memcmp(actual,sentinel,sizeof(actual)));
    assert(!CommunityAreaLight_Parse(many,131073,NULL,0,&count));free(many);
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);long bytes=ftell(file);assert(bytes>0&&bytes<1048576);rewind(file);
        unsigned char *narc=malloc((size_t)bytes);assert(narc);assert(fread(narc,1,(size_t)bytes,file)==(size_t)bytes);fclose(file);
        assert(!memcmp(narc,"NARC",4));unsigned fat=16,gmif=0;
        for(unsigned block=0;block<U16(narc+14);++block){assert(fat+8<=(unsigned)bytes);unsigned size=U32(narc+fat+4);assert(size>=8&&size<=(unsigned)bytes-fat);if(!memcmp(narc+fat,"GMIF",4))gmif=fat+8;fat+=size;}
        assert(gmif);fat=16;assert(!memcmp(narc+fat,"BTAF",4));
        for(unsigned member=0;member<U16(narc+fat+8);++member){unsigned start=U32(narc+fat+12+member*8),end=U32(narc+fat+16+member*8);assert(end>=start&&end<=(unsigned)bytes-gmif);count=0;assert(CommunityAreaLight_Parse(narc+gmif+start,end-start,NULL,0,&count));printf("Private existing lighting member%u validates %zu complete records.\n",member,count);}
        free(narc);
    }
    puts("Actual desktop lighting parser: all newline variants are byte-identical; bounds, exact colors/vectors, incomplete/invalid records and no partial caller mutation passed.");
}
''', encoding="utf-8")
    executable = scratch / ("light.exe" if os.name == "nt" else "light")
    subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-ftrivial-auto-var-init=zero", "-I" + str(root / "include"), str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)] + ([str(args.narc.resolve())] if args.narc else []), check=True)
    if args.engine_build:
        engine = scratch / "loader.c"
        engine.write_text('#include "' + (root / "src/overlay005/area_light.c").as_posix() + '"\n' + r'''
#include <assert.h>
#include <stdlib.h>
static const char validText[]="900,\r\r\n1,23,23,25,-2043,-3548,110,\r\r\n0,0,0,0,0,0,0,\r\r\n1,4,4,16,0,0,4096,\r\r\n0,0,0,0,0,0,0,\r\r\n16,16,16,\r\r\n14,14,14,\r\r\n20,20,20,\r\r\n16,16,16,\r\r\n\r\r\nEOF";
static const char *input=validText;
static int live,allocated,failAt,openNarc,missing,readCount;
static u32 lengthOverride;
static NARC dummy;
void ErrorHandling_AssertFail(void){assert(!"unexpected engine assert");}
void MI_CpuFill8(void *p,u8 value,u32 size){memset(p,value,size);}
int GetSecondsSinceMidnight(void){assert(!"clock outside loader fixture");return 0;}
void NNSi_G3dModifyMatFlag(NNSG3dResMdl *m,BOOL enabled,NNSG3dMatFlag flag){(void)m;(void)enabled;(void)flag;assert(!"render outside loader fixture");}
void ModelAttributes_SetLightVector(ModelAttributes *m,int i,fx16 x,fx16 y,fx16 z){(void)m;(void)i;(void)x;(void)y;(void)z;assert(!"render outside loader fixture");}
void ModelAttributes_SetLightColor(ModelAttributes *m,int i,GXRgb c){(void)m;(void)i;(void)c;assert(!"render outside loader fixture");}
void ModelAttributes_SetDiffuseReflection(ModelAttributes *m,GXRgb c,BOOL v,BOOL a){(void)m;(void)c;(void)v;(void)a;assert(!"render outside loader fixture");}
void ModelAttributes_SetAmbientReflection(ModelAttributes *m,GXRgb c,BOOL a){(void)m;(void)c;(void)a;assert(!"render outside loader fixture");}
void ModelAttributes_SetSpecularReflection(ModelAttributes *m,GXRgb c,BOOL s,BOOL a){(void)m;(void)c;(void)s;(void)a;assert(!"render outside loader fixture");}
void ModelAttributes_SetEmissionColor(ModelAttributes *m,GXRgb c,BOOL a){(void)m;(void)c;(void)a;assert(!"render outside loader fixture");}
void *Heap_Alloc(u32 id,u32 size){(void)id;if(++allocated==failAt)return NULL;void *p=malloc(size);assert(p);++live;return p;}
void Heap_Free(void *p){assert(p&&live>0);--live;free(p);}
void Heap_FreeExplicit(u32 id,void *p){(void)id;Heap_Free(p);}
NARC *NARC_ctor(enum NarcID id,enum HeapID heap){(void)id;(void)heap;if(missing)return NULL;++openNarc;return &dummy;}
void NARC_dtor(NARC *n){assert(n==&dummy&&openNarc==1);--openNarc;}
u16 NARC_GetFileCount(NARC *n){assert(n==&dummy);return 4;}
u32 NARC_GetMemberSize(NARC *n,u32 i){assert(n==&dummy&&i<4);return lengthOverride ? lengthOverride : strlen(input);}
void NARC_ReadFromMember(NARC *n,u32 i,u32 offset,u32 bytes,void *out){assert(n==&dummy&&i<4&&offset==0&&bytes==strlen(input));++readCount;memcpy(out,input,bytes);}
static void Check(int fallback){AreaLightTemplate *templates=(void*)1;u32 count=AreaLightTemplate_New(1,&templates);assert(count==1&&templates&&openNarc==0&&live==1);if(fallback){assert(templates[0].endTime==43200&&templates[0].validLightsMask==1);assert(templates[0].lightColors[0]==GX_RGB(31,31,31)&&templates[0].lightVectors[0].z==-4096);assert(templates[0].ambientReflectColor==GX_RGB(16,16,16)&&templates[0].emissionColor==GX_RGB(16,16,16));for(int i=1;i<4;++i)assert(!templates[0].lightColors[i]&&!templates[0].lightVectors[i].x&&!templates[0].lightVectors[i].y&&!templates[0].lightVectors[i].z);}else{assert(templates[0].endTime==900&&templates[0].validLightsMask==5&&templates[0].lightColors[0]==GX_RGB(23,23,25));assert(templates[0].lightVectors[0].x==-2043&&templates[0].ambientReflectColor==GX_RGB(14,14,14)&&templates[0].emissionColor==GX_RGB(16,16,16));}Heap_Free(templates);assert(!live);allocated=failAt=readCount=0;}
#undef main
int main(void){Check(0);input="900,\n1,23,23,25,0,0,0,\nEOF";Check(1);input=validText;missing=1;Check(1);missing=0;lengthOverride=131073;Check(1);assert(!readCount);lengthOverride=0;failAt=2;Check(1);failAt=3;Check(1);puts("Actual AreaLightTemplate_New loader: CRCRLF colors, owned NARC/read extents, complete neutral fallback, unavailable/oversized/malformed and allocation cleanup passed.");}
''', encoding="utf-8")
        commands = json.loads((args.engine_build.resolve() / "compile_commands.json").read_text())
        entry = next(item for item in commands if item["file"].endswith("/area_light.c"))
        compile_args = shlex.split(entry["command"])
        compile_args[0] = args.cc
        compile_args = compile_args[:compile_args.index("-MD")]
        loader_executable = scratch / ("loader.exe" if os.name == "nt" else "loader")
        subprocess.run(compile_args + ["-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", str(engine), "-o", str(loader_executable)], cwd=entry["directory"], check=True)
        subprocess.run([str(loader_executable)], check=True)
