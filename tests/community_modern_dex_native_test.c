/* Actual public Pokédex/catch code; Pokémon/catalogue and unrelated save owners
 * are isolated fixtures. No installed profile or runtime assets are accessed. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port/community_modern_dex.h"
#include "port/community_catalog.h"
#include "port/community_modern_rules.h"
#include "game_records.h"
#include "poketch.h"
#include "struct_defs/battle_system.h"
#include "battle/battle_system.h"
#include "generated/trainer_score_events.h"
#include POKEDEX_SOURCE
#include POKEDEX_LANGUAGE_SOURCE
#include CATCH_RECORDS_SOURCE
#include SDK_CRC_SOURCE
#include NATIVE_CHECKSUM_SOURCE

_Static_assert(sizeof(Pokedex)==804,"Native Pokédex save layout changed");
_Static_assert(offsetof(Pokedex,recordedLanguages)==296,"Native language offset changed");
_Static_assert(offsetof(Pokedex,rotomFormsSeen)==796,"Native trailing field offset changed");
_Static_assert(sizeof(((Pokedex *)0)->recordedLanguages)==496,"Native carrier extent changed");
static Pokedex dex;
static CommunityCatalog catalog;
static int enabled=1,haveCatalog=1,genderCalls,score,history;
static unsigned species=512,egg,gender=1,language=2;
static MATHCRC16Table table;
u32 Pokemon_GetValue(Pokemon *mon,enum PokemonDataParam param,void *destination)
{
    (void)mon;(void)destination;
    switch(param) {
    case MON_DATA_SPECIES:return species;
    case MON_DATA_IS_EGG:return egg;
    case MON_DATA_LANGUAGE:return language;
    case MON_DATA_PERSONALITY:return 1234567;
    default:return 0;
    }
}
u8 Pokemon_GetGender(Pokemon *mon) {(void)mon;++genderCalls;return gender;}
u8 Pokemon_GetForm(Pokemon *mon) {(void)mon;return 0;}
u16 Pokemon_SinnohDexNumber(u16 input) {(void)input;assert(!"Local dex outside fixture scope");return 0;}
u32 SpeciesData_GetSpeciesValue(int input,enum SpeciesDataParam param) {(void)input;(void)param;assert(!"Gender display outside fixture scope");return 0;}
void *Heap_Alloc(u32 heap,u32 size) {(void)heap;return malloc(size);}
int CommunityModernRules_Enabled(void) {return enabled;}
const CommunityCatalog *CommunityCatalog_Runtime(void) {return haveCatalog ? &catalog : NULL;}
void MI_CpuCopy8(const void *source,void *destination,u32 size) {memcpy(destination,source,size);}
void ErrorHandling_AssertFail(void) {assert(!"Unexpected native assertion");}
void *SaveData_SaveTable(SaveData *save,int slot) {(void)save;assert(slot==SAVE_TABLE_ENTRY_POKEDEX);return &dex;}
GameRecords *SaveData_GetGameRecords(SaveData *save) {(void)save;return (GameRecords *)(void *)&score;}
Poketch *SaveData_GetPoketch(SaveData *save) {(void)save;return (Poketch *)(void *)&history;}
void Poketch_PokemonHistoryEnqueue(Poketch *p,const BoxPokemon *mon) {(void)mon;assert(p==(void *)&history);++history;}
void GameRecords_IncrementTrainerScore(GameRecords *records,int event) {assert(records==(void *)&score&&event==TRAINER_SCORE_EVENT_CAUGHT_SPECIES);++score;}
u16 CalcCRC16Checksum(const void *bytes,u32 size) {return MATH_CalcCRC16CCITT(&table,bytes,size);}
static void Preserved(const Pokedex *before,const Pokedex *after)
{
    const unsigned char *a=(const void *)before,*b=(const void *)after;
    for(unsigned i=0;i<sizeof(*before);++i) {
        unsigned mask=i>=296&&i<792 ? 127 : 255;
        assert((a[i]&mask)==(b[i]&mask));
    }
}
#undef main
int main(int argc,char **argv)
{
    assert(argc==2);catalog.valid=1;MATH_CRC16CCITTInitTable(&table);
    Pokemon mon,identity;memset(&mon,0xa5,sizeof(mon));memcpy(&identity,&mon,sizeof(mon));
    memset(&dex,0xa5,sizeof(dex));Pokedex_Init(&dex);
    for(unsigned i=0;i<496;++i) assert(!dex.recordedLanguages[i]);
    for(unsigned i=0;i<496;++i) dex.recordedLanguages[i]=(unsigned char)((i*19+67)&127);
    Pokedex before=dex,copy;
    assert(!Pokedex_HasCaughtSpecies(&dex,512)&&!Pokedex_HasSeenSpecies(&dex,512));
    assert(!memcmp(&before,&dex,sizeof(dex)));
    Pokedex_Encounter(&dex,&mon);
    assert(Pokedex_HasSeenSpecies(&dex,512)&&!Pokedex_HasCaughtSpecies(&dex,512));
    Preserved(&before,&dex);
    SaveData_UpdateCatchRecords((SaveData *)(void *)&dex,&mon);
    assert(Pokedex_HasCaughtSpecies(&dex,512)&&score==1&&history==1);
    SaveData_UpdateCatchRecords((SaveData *)(void *)&dex,&mon);
    assert(score==1&&history==2);Preserved(&before,&dex);
    assert(!memcmp(&mon,&identity,sizeof(mon)));
    Pokedex_Copy(&dex,&copy);assert(!memcmp(&copy,&dex,sizeof(dex)));
    BattleSystem battle;memset(&battle,0,sizeof(battle));battle.pokedex=&copy;
    assert(BattleSystem_HasCaughtSpecies(&battle,512));
    assert(!BattleSystem_HasCaughtSpecies(&battle,513));
    /* Catalogue/mode changes cannot erase readable history or permit new writes. */
    const int modes[][3]={{0,1,1},{1,0,1},{1,1,0}};
    species=513;
    for(unsigned i=0;i<3;++i) {
        enabled=modes[i][0];haveCatalog=modes[i][1];catalog.valid=modes[i][2];before=dex;
        int calls=genderCalls;Pokedex_Encounter(&dex,&mon);Pokedex_Capture(&dex,&mon);
        assert(genderCalls==calls&&!memcmp(&dex,&before,sizeof(dex)));
        assert(Pokedex_HasCaughtSpecies(&dex,512)&&!Pokedex_HasSeenSpecies(&dex,513));
    }
    enabled=haveCatalog=catalog.valid=1;
    egg=1;SaveData_UpdateCatchRecords((SaveData *)(void *)&dex,&mon);
    assert(!Pokedex_HasCaughtSpecies(&dex,513));egg=0;
    /* Final modern ID exercises the last payload bit, independently from base counts. */
    species=576;Pokedex_Capture(&dex,&mon);assert(Pokedex_HasCaughtSpecies(&dex,576));
    assert(Pokedex_CountCaught_National(&dex)==0);
    before=dex;
    /* Serialize exactly the unchanged native page plus its native CRC16. */
    u8 page[sizeof(Pokedex)+2],loaded[sizeof(page)];memcpy(page,&dex,sizeof(dex));
    u16 checksum=SaveData_CalculateChecksum(NULL,page,sizeof(Pokedex));
    page[sizeof(Pokedex)]=(u8)checksum;page[sizeof(Pokedex)+1]=(u8)(checksum>>8);
    FILE *file=fopen(argv[1],"wbx");assert(file);assert(fwrite(page,1,sizeof(page),file)==sizeof(page));assert(!fclose(file));
    file=fopen(argv[1],"rb");assert(file);assert(fread(loaded,1,sizeof(loaded),file)==sizeof(loaded));assert(fgetc(file)==EOF);fclose(file);
    assert(!memcmp(page,loaded,sizeof(page)));
    assert(SaveData_CalculateChecksum(NULL,loaded,sizeof(Pokedex))==(u16)(loaded[sizeof(Pokedex)]|(loaded[sizeof(Pokedex)+1]<<8)));
    memcpy(&copy,loaded,sizeof(copy));assert(Pokedex_HasCaughtSpecies(&copy,576));
    /* Actual ordinary language updates must not invalidate CDX1's high bits. */
    species=25;language=7;dex.recordedLanguages[25]&=(u8)~(1u<<5);
    u8 high[496];for(unsigned i=0;i<496;++i) high[i]=dex.recordedLanguages[i]&128;
    Pokedex_Capture(&dex,&mon);
    for(unsigned i=0;i<496;++i) assert((dex.recordedLanguages[i]&128)==high[i]);
    assert(Pokedex_HasCaughtSpecies(&dex,512)&&Pokedex_HasCaughtSpecies(&dex,576));
    assert(Pokedex_IsLanguageObtained(&dex,25,7));
    for(unsigned lang=0;lang<8;++lang) {
        unsigned index=PokedexLanguage_LanguageToIndex(lang);
        assert(index<=6);
        assert(Pokedex_IsLanguageObtained(&dex,25,lang)==(dex.recordedLanguages[25]&(1u<<index)));
    }
    /* Corruption invalidates both native CRC and CDX1; no implicit repair. */
    loaded[296+320]^=128;assert(SaveData_CalculateChecksum(NULL,loaded,sizeof(Pokedex))!=checksum);
    memcpy(&copy,loaded,sizeof(copy));assert(!Pokedex_HasSeenSpecies(&copy,512)&&!Pokedex_HasCaughtSpecies(&copy,576));
    species=576;before=copy;Pokedex_Capture(&copy,&mon);assert(!memcmp(&before,&copy,sizeof(copy)));
    Pokedex_Init(&copy);assert(!Pokedex_HasSeenSpecies(&copy,576));
    return 0;
}
