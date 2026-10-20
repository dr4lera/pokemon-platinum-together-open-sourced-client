"""Execute actual evolution consumers with synthetic state and no game assets."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_community_species_loaders import function

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
    source = (ROOT / "src/pokemon.c").read_text()
    rules = re.search(r"enum \{\s*COMMUNITY_EVO_LEVEL_DAY.*?\};", source, re.S).group()
    functions = "\n".join(function(source, name) for name in (
        "Pokemon_HasKnownFairyMove", "Pokemon_EvolutionHasRain", "LoadSpeciesEvolutions", "Pokemon_GetEvolutionTargetSpecies"))
    methods = ",".join((ROOT / "generated/evolution_methods.txt").read_text().split())
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port/community_catalog.h"
#define CommunityCatalog_Runtime CommunityCatalog_DefaultRuntime
#include "CATALOG_SOURCE"
#undef CommunityCatalog_Runtime
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;
enum { FALSE, TRUE };
enum { EVO_METHODS };
enum { EVO_CLASS_BY_LEVEL, EVO_CLASS_BY_TRADE, EVO_CLASS_UNUSED_02, EVO_CLASS_BY_ITEM };
enum { MON_DATA_SPECIES, MON_DATA_HELD_ITEM, MON_DATA_PERSONALITY, MON_DATA_BEAUTY,
    MON_DATA_LEVEL, MON_DATA_FRIENDSHIP, MON_DATA_ATK, MON_DATA_DEF, MON_DATA_GENDER,
    MON_DATA_MOVE1, MON_DATA_MOVE2, MON_DATA_MOVE3, MON_DATA_MOVE4 };
enum { SPECIES_NONE=0, SPECIES_KADABRA=64, SPECIES_EEVEE=133,
    MOVE_NONE=0, MAX_MOVES=474, TYPE_FAIRY=18, LEARNED_MOVES_MAX=4,
    EVOLVE_FRIENDSHIP_THRESHOLD=220, HOLD_EFFECT_NO_EVOLVE=64,
    OVERWORLD_WEATHER_RAINING=2, OVERWORLD_WEATHER_THUNDERSTORM=4,
    ITEM_PARAM_HOLD_EFFECT=1, HEAP_ID_SYSTEM=0, MOVEATTRIBUTE_TYPE=4,
    GENDER_MALE=0, GENDER_FEMALE=1, MAX_EVOLUTIONS=7,
    NARC_INDEX_POKETOOL__PERSONAL__EVO=34 };
typedef struct Pokemon { u32 values[13]; } Pokemon;
typedef struct Party { int unused; } Party;
typedef struct SpeciesEvolution { u16 method,param,targetSpecies; } SpeciesEvolution;
typedef struct SaveData { int unused; } SaveData;
typedef struct FieldOverworldState { u16 weather; } FieldOverworldState;
static CommunityCatalog catalog;
static SpeciesEvolution legacy[7];
static SaveData save;
static FieldOverworldState field;
static int enabled,night,missingSave,missingField,narcReads,allocs,frees,weatherReads,moveReads;
static u8 moveTypes[MAX_MOVES];
int CommunityModernRules_Enabled(void) { return enabled; }
const CommunityCatalog *CommunityCatalog_Runtime(void) { return &catalog; }
u32 Pokemon_GetValue(Pokemon *mon,int param,void *out) { (void)out; assert(param<13); return mon->values[param]; }
u32 MoveTable_LoadParam(int move,int param) { assert(move>0 && move<MAX_MOVES && param==MOVEATTRIBUTE_TYPE); moveReads++; return moveTypes[move]; }
u8 Item_LoadParam(u16 item,int param,int heap) { (void)param; (void)heap; return item==229 ? HOLD_EFFECT_NO_EVOLVE : 0; }
void *Heap_Alloc(int heap,size_t bytes) { (void)heap; assert(bytes==42); allocs++; return malloc(bytes); }
void Heap_Free(void *ptr) { frees++; free(ptr); }
int IsNight(void) { return night; }
BOOL Pokemon_HasMove(Pokemon *mon,u16 move) { for(int i=0;i<4;i++) if(mon->values[MON_DATA_MOVE1+i]==move) return TRUE; return FALSE; }
BOOL Party_HasSpecies(Party *party,u16 species) { (void)party; (void)species; return FALSE; }
SaveData *SaveData_Ptr(void) { return missingSave ? NULL : &save; }
FieldOverworldState *SaveData_GetFieldOverworldState(SaveData *ptr) { assert(ptr==&save); return missingField ? NULL : &field; }
u16 FieldOverworldState_GetWeather(FieldOverworldState *ptr) { assert(ptr==&field); weatherReads++; return ptr->weather; }
void NARC_ReadWholeMemberByIndexPair(void *out,int archive,int species) {
    assert(archive==NARC_INDEX_POKETOOL__PERSONAL__EVO && species>=0 && species<508);
    narcReads++; memcpy(out,legacy,sizeof(legacy));
}
RULES
FUNCTIONS
static void modernRule(int method,int level,int target) {
    memset(catalog.evolutions,0,sizeof(catalog.evolutions));
    catalog.evolutions[0][0]=(u8)method; catalog.evolutions[0][2]=(u8)level;
    catalog.evolutions[0][4]=(u8)target; catalog.evolutions[0][5]=(u8)(target>>8);
}
static int evolve(Pokemon *mon,int mode,int param,int *type) {
    int beforeAlloc=allocs,beforeFree=frees;
    int result=Pokemon_GetEvolutionTargetSpecies(NULL,mon,(u8)mode,(u16)param,type);
    assert(allocs-beforeAlloc==frees-beforeFree);
    return result;
}
int main(void) {
    Pokemon mon={0}; int type=-1;
    enabled=1; catalog.valid=1; mon.values[MON_DATA_SPECIES]=512;
    for(unsigned i=0;i<65;i++) {
        SpeciesEvolution rows[7]; memset(rows,255,sizeof(rows));
        LoadSpeciesEvolutions(512+i,rows);
        assert(memcmp(rows,catalog.evolutions[i],42)==0 && narcReads==0);
    }
    modernRule(EVO_LEVEL,16,513);
    mon.values[MON_DATA_LEVEL]=15; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0);
    mon.values[MON_DATA_LEVEL]=16; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==513 && type==EVO_LEVEL);
    catalog.evolutions[0][6]=EVO_LEVEL; catalog.evolutions[0][8]=1;
    catalog.evolutions[0][10]=2; catalog.evolutions[0][11]=2;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==513);
    assert(narcReads==0);
    modernRule(COMMUNITY_EVO_LEVEL_DAY,39,513);
    for(night=0;night<=1;night++) for(unsigned level=38;level<=39;level++) {
        mon.values[MON_DATA_LEVEL]=level;
        assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==(!night && level==39 ? 513:0));
    }
    assert(evolve(&mon,EVO_CLASS_BY_TRADE,0,&type)==0 && evolve(&mon,EVO_CLASS_BY_ITEM,39,&type)==0);
    modernRule(COMMUNITY_EVO_LEVEL_RAIN,50,513);
    field.weather=2; mon.values[MON_DATA_LEVEL]=49; int before=weatherReads;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0 && weatherReads==before);
    mon.values[MON_DATA_LEVEL]=50;
    for(unsigned weather=0;weather<=16;weather++) {
        field.weather=(u16)weather;
        assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==(weather>=2 && weather<=4 ? 513:0));
    }
    field.weather=32; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0);
    field.weather=2; missingSave=1; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0);
    missingSave=0; missingField=1; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0); missingField=0;
    assert(narcReads==0);
    mon.values[MON_DATA_HELD_ITEM]=229; before=allocs;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0 && allocs==before);
    mon.values[MON_DATA_HELD_ITEM]=0;
    modernRule(EVO_USE_ITEM,80,513);
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0);
    assert(evolve(&mon,EVO_CLASS_BY_ITEM,80,&type)==513 && type==EVO_NONE);
    enabled=0; mon.values[MON_DATA_SPECIES]=35; mon.values[MON_DATA_LEVEL]=20;
    legacy[0]=(SpeciesEvolution){EVO_LEVEL,20,36};
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==36 && narcReads==1);
    legacy[0]=(SpeciesEvolution){64,1,36};
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0 && narcReads==2);
    enabled=1; memset(legacy,0,sizeof(legacy));
    legacy[0]=(SpeciesEvolution){EVO_LEVEL_HAPPINESS_DAY,0,196};
    legacy[1]=(SpeciesEvolution){EVO_LEVEL_HAPPINESS_NIGHT,0,197};
    legacy[6]=(SpeciesEvolution){EVO_USE_ITEM,80,134};
    mon.values[MON_DATA_SPECIES]=SPECIES_EEVEE; mon.values[MON_DATA_FRIENDSHIP]=159;
    mon.values[MON_DATA_MOVE1]=468; moveTypes[468]=TYPE_FAIRY;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0);
    mon.values[MON_DATA_FRIENDSHIP]=160;
    for(night=0;night<=1;night++) assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==552 && type==66);
    mon.values[MON_DATA_FRIENDSHIP]=255; night=0;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==552);
    assert(evolve(&mon,EVO_CLASS_BY_ITEM,80,&type)==134 && type==EVO_NONE);
    assert(evolve(&mon,EVO_CLASS_BY_TRADE,0,&type)==0);
    mon.values[MON_DATA_HELD_ITEM]=229; before=moveReads;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==0 && moveReads==before);
    assert(evolve(&mon,EVO_CLASS_BY_ITEM,80,&type)==134); mon.values[MON_DATA_HELD_ITEM]=0;
    enabled=0; night=0; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==196);
    night=1; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==197);
    enabled=1; mon.values[MON_DATA_MOVE1]=1;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==197);
    mon.values[MON_DATA_MOVE1]=0; mon.values[MON_DATA_MOVE4]=204; moveTypes[204]=TYPE_FAIRY;
    assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,NULL)==552);
    mon.values[MON_DATA_MOVE4]=512; assert(evolve(&mon,EVO_CLASS_BY_LEVEL,0,&type)==197);
    assert(allocs==frees);
    puts("Actual evolution consumers: modern archive bypass, day/rain boundaries, priority Fairy friendship, seven legacy slots, Everstone, items and disabled gate passed");
    return 0;
}
'''
    harness = harness.replace("CATALOG_SOURCE", (ROOT / "src/port/community_catalog.c").as_posix()).replace("EVO_METHODS", methods).replace("RULES", rules).replace("FUNCTIONS", functions)
    scratch = ROOT / ".build-tools"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="evolution-tests-", dir=scratch) as directory:
        temp = Path(directory)
        unit = temp / "evolutions.c"
        unit.write_text(harness)
        executable = temp / ("evolutions.exe" if os.name == "nt" else "evolutions")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "include"), str(unit),
                        str(ROOT / "src/port/community_learnset.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
