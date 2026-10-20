"""Check actual personal/learnset loader ordering without game assets or UI."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"(?m)^(?:static )?(?:void|BOOL|u16|enum CommunityLearnsetResult) " + name + r"\([^;{}]*\)\s*\{", source)
    assert match, name
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def main():
    compiler = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
    source = (ROOT / "src/pokemon.c").read_text()
    functions = "\n".join(function(source, name) for name in (
        "SpeciesData_LoadSpecies", "SpeciesData_LoadForm", "Pokemon_LoadCommunityLearnsetOf"))
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port/community_catalog.h"
#include "port/community_form_rules.h"
#define CommunityCatalog_Runtime CommunityCatalog_DefaultRuntime
#include "CATALOG_SOURCE"
#undef CommunityCatalog_Runtime
typedef uint32_t u32;
typedef struct SpeciesData { uint8_t base[6], types[2], rest[14], abilities[2], tail[20]; } SpeciesData;
typedef struct SpeciesLearnset { uint16_t entries[21]; } SpeciesLearnset;
typedef struct NARC { int marker; } NARC;
enum { NARC_INDEX_POKETOOL__PERSONAL__PL_PERSONAL, NARC_INDEX_POKETOOL__PERSONAL__WOTBL, HEAP_ID_SYSTEM };
static CommunityCatalog catalog;
static int enabled, wholeReads, opens, closes, lastMember;
static NARC archive;
int CommunityModernRules_Enabled(void) { return enabled; }
const CommunityCatalog *CommunityCatalog_Runtime(void) { return &catalog; }
int Pokemon_GetFormNarcIndex(int species, int form) { return species == 386 && form == 1 ? 496 : species; }
void NARC_ReadWholeMemberByIndexPair(void *out, int unused, int species) {
    (void)unused; wholeReads++; lastMember=species; memset(out, 9, 44);
}
NARC *NARC_ctor(int unused, int heap) { (void)unused; (void)heap; opens++; return &archive; }
void NARC_dtor(NARC *narc) { assert(narc == &archive); closes++; }
int NARC_GetFileCount(NARC *narc) { (void)narc; return 508; }
u32 NARC_GetMemberSize(NARC *narc, int member) { (void)narc; lastMember=member; return 4; }
void NARC_ReadFromMember(NARC *narc, int member, int offset, u32 bytes, void *out) {
    const uint16_t legacy[2] = { (20 << 9) | 3, UINT16_MAX };
    (void)narc; assert(offset == 0 && bytes == 4); lastMember=member; memcpy(out, legacy, bytes);
}
FUNCTIONS
int main(void) {
    SpeciesData personal;
    CommunityLearnset moves;
    catalog.valid=1; enabled=1;
    for (unsigned i=0;i<65;i++) {
        catalog.personal[i][0]=(uint8_t)(i+1);
        catalog.learnsets[i][0]=88; catalog.learnsets[i][1]=2;
        catalog.learnsets[i][2]=20;
        memset(catalog.learnsets[i]+4,255,4);
        SpeciesData_LoadSpecies(512+i,&personal);
        assert(personal.base[0] == i+1 && wholeReads == 0);
        SpeciesData_LoadForm(512+i,0,&personal);
        assert(personal.base[0] == i+1 && wholeReads == 0);
        assert(Pokemon_LoadCommunityLearnsetOf(512+i,0,&moves) == COMMUNITY_LEARNSET_OK);
        assert(moves.count == 1 && moves.entries[0].move == 600 && moves.entries[0].level == 20);
        assert(opens == 0 && closes == 0);
    }
    SpeciesData_LoadForm(386,1,&personal);
    assert(wholeReads == 1 && lastMember == 496);
    catalog.personal[541-512][22]=67; catalog.personal[541-512][23]=68;
    SpeciesData_LoadForm(541,1,&personal);
    assert(personal.abilities[0]==143 && personal.abilities[1]==0 && wholeReads==1);
    SpeciesData_LoadForm(541,2,&personal);
    assert(personal.abilities[0]==67 && personal.abilities[1]==68 && wholeReads==1);
    assert(Pokemon_LoadCommunityLearnsetOf(35,0,&moves) == COMMUNITY_LEARNSET_OK);
    assert(moves.count == 1 && moves.entries[0].move == 3 && moves.entries[0].level == 20);
    assert(opens == 1 && closes == 1);
    assert(Pokemon_LoadCommunityLearnsetOf(577,0,&moves) == COMMUNITY_LEARNSET_INVALID && moves.count == 0);
    assert(opens == 2 && closes == 2);
    enabled=0;
    SpeciesData_LoadSpecies(35,&personal);
    assert(wholeReads == 2 && lastMember == 35 && personal.base[0] == 9);
    assert(Pokemon_LoadCommunityLearnsetOf(512,0,&moves) == COMMUNITY_LEARNSET_INVALID && moves.count == 0);
    assert(opens == 3 && closes == 3);
    assert(Pokemon_LoadCommunityLearnsetOf(35,0,NULL) == COMMUNITY_LEARNSET_INVALID && opens == 3);
    puts("Actual personal/learnset consumers bypass legacy archives for gated modern IDs; legacy/forms, disabled gate and NARC lifetime preserved");
    return 0;
}
'''
    harness = harness.replace("CATALOG_SOURCE", (ROOT / "src/port/community_catalog.c").as_posix()).replace("FUNCTIONS", functions)
    scratch = ROOT / ".build-tools"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="species-loader-tests-", dir=scratch) as directory:
        temp = Path(directory)
        unit = temp / "loaders.c"
        unit.write_text(harness)
        executable = temp / ("loaders.exe" if os.name == "nt" else "loaders")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "include"),
                        str(unit), str(ROOT / "src/port/community_learnset.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
