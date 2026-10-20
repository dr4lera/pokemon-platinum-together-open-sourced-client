"""Actual graphics consumers and strict private archive merger; no game assets required."""
import argparse
from dataclasses import replace
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/community/prepare-species-assets.py"
spec = importlib.util.spec_from_file_location("community_species_assets", TOOL)
assets = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = assets
spec.loader.exec_module(assets)
assets.SOURCE_ROOT = ROOT
NAMES = struct.pack("<4sIIHH", b"BTNF", 16, 4, 0, 1)


def character(icon=False):
    size = 1024 if icon else 6400
    fields = (65535, 65535, 3, 16, 0, size, 24) if icon else (10, 20, 3, 0, 1, size, 24)
    return (struct.pack("<4sHHIHH4sI", b"RGCN", 0xfeff, 0x101 if icon else 0x100, 48 + size, 16, 1, b"RAHC", 32 + size)
            + struct.pack("<HHIIIII", *fields) + bytes(size))


def palette():
    return (struct.pack("<4sHHIHH4sI", b"RLCN", 0xfeff, 0x100, 72, 16, 1, b"TTLP", 56)
            + struct.pack("<IIII", 0x000a0004, 0, 32, 16) + bytes(32))


def extract(source, name):
    match = re.search(r"(?m)^(?:static (?:inline )?)?(?:const u8|void|BOOL|u8|u16|u32|int) " + name + r"\([^;{}]*\)\s*\{", source)
    assert match, name
    depth, end = 1, match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def run_consumers():
    pokemon = (ROOT / "src/pokemon.c").read_text()
    icons = (ROOT / "src/pokemon_icon.c").read_text()
    functions = "\n".join(extract(pokemon, name) for name in (
        "Pokemon_SanitizeFormId", "BuildCommunityPokemonSpriteTemplate", "BuildPokemonSpriteTemplate", "BuildPokemonSpriteTemplateDP",
        "LoadPokemonSpriteYOffset", "LoadPokemonDPSpriteHeight", "Pokemon_Face", "LoadPokemonSpriteData",
        "PokemonSprite_LoadAnimFrames", "PokemonSprite_LoadAnim", "PokemonSprite_LoadCryDelay",
        "PokemonSprite_LoadYOffset", "PokemonSprite_LoadXOffsetShadow", "PokemonSprite_LoadShadowSize"))
    functions += "\n" + "\n".join(extract(icons, name) for name in ("IconTilesIndex", "PokeIconSpriteIndex", "PokeIconPaletteIndex"))
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "port/community_catalog.h"
#define CommunityCatalog_Runtime CommunityCatalog_DefaultRuntime
#include "CATALOG_SOURCE"
#undef CommunityCatalog_Runtime
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;
enum { FALSE, TRUE, FACE_FRONT=2, FACE_BACK=0 };
#include "constants/species.h"
#include "constants/forms.h"
#include "constants/pokemon.h"
#include "constants/narc.h"
#include "generated/genders.h"
#include "struct_defs/species_sprite_data.h"
#include "res/pokemon/pl_poke_icon.naix"
#include "res/pokemon/species_icon_palettes.h"
typedef struct PokemonSpriteTemplate { u16 narcID,character,palette,spindaSpots; u8 dummy; u32 personality; } PokemonSpriteTemplate;
typedef struct PokemonAnimTemplate { u16 animation,startDelay; u8 flipSprite; } PokemonAnimTemplate;
typedef struct PokemonAnimManager { int unused; } PokemonAnimManager;
typedef struct PokemonSprite { int unused; } PokemonSprite;
typedef struct NARC { int unused; } NARC;
static CommunityCatalog catalog;
static int enabled=1,missingCatalog,narcReads,lastNarc,lastMember,lastOffset,animFrame;
static NARC narc;
static SpeciesSpriteData legacy;
static PokemonAnimTemplate lastAnim;
int CommunityModernRules_Enabled(void) { return enabled; }
const CommunityCatalog *CommunityCatalog_Runtime(void) { return missingCatalog ? NULL : &catalog; }
#define MI_CpuCopy8(src,dst,size) memcpy(dst,src,size)
void NARC_ReadFromMember(NARC *arc,int member,int offset,int length,void *out) {
    assert(arc==&narc && member==0 && length==89 && offset>=0 && offset<494*89);
    narcReads++; lastOffset=offset; memcpy(out,&legacy,89);
}
void NARC_ReadWholeMemberByIndexPair(void *out,int arc,int member) {
    narcReads++; lastNarc=arc; lastMember=member; *(u8*)out=7;
}
void PokemonAnimManager_InitAnim(PokemonAnimManager *manager,PokemonSprite *sprite,PokemonAnimTemplate *t,int frame) {
    (void)manager; (void)sprite; lastAnim=*t; animFrame=frame;
}
_Static_assert(sizeof(SpeciesSpriteData)==89,"sprite data serialized size");
FUNCTIONS
int main(void) {
    catalog.valid=1;
    for(unsigned i=0;i<65;i++) {
        catalog.spriteHeights[i][0]=(u8)i; catalog.spriteHeights[i][1]=(u8)(i+10);
        catalog.iconPalettes[i]=(u8)(i%3);
        SpeciesSpriteData data={0};
        data.faceAnims[0].cryDelay=12; data.faceAnims[0].animation=84; data.faceAnims[0].startDelay=20;
        data.faceAnims[0].frames[0]=(SpriteAnimFrame){0,3,-2,2};
        data.faceAnims[1].cryDelay=6; data.faceAnims[1].animation=17; data.faceAnims[1].startDelay=9;
        data.faceAnims[1].frames[0]=(SpriteAnimFrame){-1,2,1,-3};
        data.yOffset=-1; data.xOffsetShadow=-2; data.shadowSize=3;
        memcpy(catalog.spriteData[i],&data,89);
    }
    for(unsigned i=0;i<4;i++) {
        catalog.formGraphics[i][0]=(u8)i; catalog.formGraphics[i][1]=2;
        catalog.formGraphics[i][2]=(u8)(i%2+1);
        catalog.formGraphics[i][3]=(u8)(70+i); catalog.formGraphics[i][4]=(u8)(80+i);
    }
    for(unsigned i=0;i<65;i++) for(unsigned gender=0;gender<3;gender++) for(unsigned face=0;face<=2;face+=2) for(unsigned shiny=0;shiny<2;shiny++) {
        PokemonSpriteTemplate pt={0},dp={0};
        BuildPokemonSpriteTemplate(&pt,512+i,gender,face,shiny,0,0x12345678);
        BuildPokemonSpriteTemplateDP(&dp,512+i,gender,face,shiny,0,0x12345678);
        assert(pt.narcID==NARC_INDEX_POKETOOL__POKEGRA__PL_POKEGRA && memcmp(&pt,&dp,sizeof(pt))==0);
        assert(pt.character==(512+i)*6+face+(gender!=GENDER_FEMALE));
        assert(pt.palette==(512+i)*6+4+shiny && !pt.spindaSpots && !pt.dummy && !pt.personality);
        assert(LoadPokemonSpriteYOffset(512+i,gender,face,0,0)==(face==2 ? i:i+10));
        assert(LoadPokemonDPSpriteHeight(512+i,gender,face,0,0)==(face==2 ? i:i+10));
        assert(PokeIconSpriteIndex(512+i,FALSE,0)==547+i && PokeIconSpriteIndex(512+i,FALSE,255)==547+i);
        assert(PokeIconPaletteIndex(512+i,255,FALSE)==i%3);
    }
    for(unsigned i=0;i<4;i++) {
        PokemonSpriteTemplate t={0};
        BuildPokemonSpriteTemplate(&t,512+i,GENDER_FEMALE,FACE_FRONT,1,i%2+1,0);
        assert(t.character==(577+i)*6+2 && t.palette==(577+i)*6+5);
        assert(LoadPokemonSpriteYOffset(512+i,0,2,i%2+1,0)==70+i);
        assert(LoadPokemonDPSpriteHeight(512+i,0,0,i%2+1,0)==80+i);
        BuildPokemonSpriteTemplate(&t,512+i,0,0,0,255,0);
        assert(t.character==(512+i)*6+1);
    }
    for(unsigned species=512;species<=576;species++) {
        u8 cry,shadow; s8 y,x; SpriteAnimFrame frames[10];
        PokemonSprite_LoadAnimFrames(&narc,frames,species,1);
        assert(frames[0].spriteFrame==0 && frames[0].xOffset==-2 && frames[0].yOffset==2);
        PokemonSprite_LoadAnimFrames(&narc,frames,species,0);
        assert(frames[0].spriteFrame==-1 && frames[0].frameDelay==2 && frames[0].yOffset==-3);
        PokemonSprite_LoadAnim(&narc,NULL,NULL,species,FACE_FRONT,1,3);
        assert(lastAnim.animation==84 && lastAnim.startDelay==20 && lastAnim.flipSprite==1 && animFrame==3);
        PokemonSprite_LoadAnim(&narc,NULL,NULL,species,FACE_BACK,0,2);
        assert(lastAnim.animation==17 && lastAnim.startDelay==9 && !lastAnim.flipSprite && animFrame==2);
        PokemonSprite_LoadCryDelay(&narc,&cry,species,1); assert(cry==12);
        PokemonSprite_LoadCryDelay(&narc,&cry,species,0); assert(cry==6);
        PokemonSprite_LoadYOffset(&narc,&y,species); assert(y==-1);
        PokemonSprite_LoadXOffsetShadow(&narc,&x,species); assert(x==-2);
        PokemonSprite_LoadShadowSize(&narc,&shadow,species); assert(shadow==3);
    }
    assert(narcReads==0);
    assert(PokeIconSpriteIndex(541,TRUE,2)==icon_00000_NCGR+ICON_EGG);
    assert(PokeIconPaletteIndex(541,2,TRUE)==sPokemonIconPaletteIndex[ICON_EGG]);
    assert(PokeIconSpriteIndex(SPECIES_MANAPHY,TRUE,0)==icon_00000_NCGR+ICON_MANAPHY_EGG);
    assert(PokeIconSpriteIndex(577,FALSE,0)==icon_00000_NCGR);
    PokemonSpriteTemplate t={0};
    BuildPokemonSpriteTemplate(&t,SPECIES_SPINDA,0,FACE_FRONT,0,0,0x12345678);
    assert(t.spindaSpots==SPECIES_SPINDA && t.personality==0x12345678);
    BuildPokemonSpriteTemplateDP(&t,SPECIES_CLEFAIRY,GENDER_FEMALE,FACE_FRONT,1,0,0);
    assert(t.narcID==NARC_INDEX_POKETOOL__POKEGRA__POKEGRA && t.palette==35*6+5);
    BuildPokemonSpriteTemplate(&t,SPECIES_EGG,0,FACE_FRONT,0,1,0);
    assert(t.narcID==NARC_INDEX_POKETOOL__POKEGRA__PL_OTHERPOKE && t.character==133 && t.palette==227);
    assert(LoadPokemonSpriteYOffset(SPECIES_BURMY,0,FACE_FRONT,2,0)==7 && lastMember==77 && lastNarc==NARC_INDEX_POKETOOL__POKEGRA__HEIGHT_O);
    assert(LoadPokemonDPSpriteHeight(SPECIES_CLEFAIRY,1,FACE_BACK,0,0)==7 && lastNarc==NARC_INDEX_POKETOOL__POKEGRA__DP_HEIGHT);
    legacy.faceAnims[0].cryDelay=4;
    for(int mode=0;mode<3;mode++) {
        enabled=mode!=0; missingCatalog=mode==1; catalog.valid=mode!=2;
        int before=narcReads; u8 cry=0;
        PokemonSprite_LoadCryDelay(&narc,&cry,SPECIES_CLEFAIRY,1);
        assert(cry==4 && narcReads==before+1 && lastOffset==35*89);
        assert(PokeIconSpriteIndex(512,FALSE,0)==icon_00000_NCGR);
        assert(PokeIconPaletteIndex(512,0,FALSE)==sPokemonIconPaletteIndex[0]);
    }
    puts("Actual sprite/icon consumers:65 IDs,4 graphics forms,PT/DP/front/back/gender/shiny,animation signed fields,egg/Spinda and catalog fallback passed");
    return 0;
}
'''
    harness = harness.replace("CATALOG_SOURCE", (ROOT / "src/port/community_catalog.c").as_posix()).replace("FUNCTIONS", functions)
    compiler = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
    build = Path(os.environ.get("COMMUNITY_TEST_BUILD", ROOT / "build_win64"))
    scratch = ROOT / ".build-tools"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="species-graphics-tests-", dir=scratch) as directory:
        temp = Path(directory)
        unit = temp / "graphics.c"
        unit.write_text(harness)
        executable = temp / ("graphics.exe" if os.name == "nt" else "graphics")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-ignored-qualifiers",
                        "-I" + str(ROOT / "include"), "-I" + str(build), str(unit),
                        str(ROOT / "src/port/community_learnset.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


class ArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.real_pins = assets.ASSETS
        cls.real_animation_hash = assets.ANIMATION_SHA256
        cls.fixtures = []
        for pin in cls.real_pins:
            base = assets.Narc(tuple(b"BASE" + struct.pack("<I", i) for i in range(pin.base_count)), NAMES)
            if pin.path == cls.real_pins[0].path:
                extension = (b"",) * 108 + tuple(character() if i % 6 < 4 else palette() for i in range(69 * 6))
            else:
                extension = (character(True),) * 65
            ref = assets.Narc(base.members + extension, NAMES)
            old, new = assets.encode_narc(base), assets.encode_narc(ref)
            fixture_pin = replace(pin, base_sha256=assets.digest(old), reference_sha256=assets.digest(new))
            cls.fixtures.append((fixture_pin, old, new))
        cls.animation = assets.encode_narc(assets.Narc((struct.pack("<I", 0),) * 143, NAMES))

    def setUp(self):
        assets.ASSETS = tuple(row[0] for row in self.fixtures)

    def tearDown(self):
        assets.ASSETS = self.real_pins
        assets.ANIMATION_SHA256 = self.real_animation_hash

    def test_prefix_append_and_framing(self):
        for pin, old, new in self.fixtures:
            merged = assets.merge(pin, old, new)
            before, after = assets.parse_narc(old), assets.parse_narc(merged)
            self.assertEqual(after.members[:pin.base_count], before.members)
            self.assertEqual(len(after.members), pin.reference_count)
        pin, old, new = self.fixtures[0]
        ref = assets.parse_narc(new)
        for index, member in ((0, b"OTHER"), (2964, b"NONEMPTY"), (3072, character()[:-1]), (3076, palette()[:-1])):
            changed = list(ref.members)
            changed[index] = member
            damaged = assets.encode_narc(assets.Narc(tuple(changed), ref.names))
            with self.assertRaises(assets.AssetError):
                assets.merge(replace(pin, reference_sha256=assets.digest(damaged)), old, damaged)

    def test_narc_bounds_and_resource_refusal(self):
        pin, old, new = self.fixtures[0]
        for data in (new[:-1], new + b"\0", b"BAD!" + new[4:]):
            with self.assertRaises(assets.AssetError):
                assets.parse_narc(data)
        # Duplicate nonempty member bounds, outside-image end, wrong count.
        for offset, value in ((36, old[28:36]), (32, struct.pack("<I", len(old))), (24, struct.pack("<H", 4097))):
            damaged = bytearray(old)
            damaged[offset:offset + len(value)] = value
            with self.assertRaises(assets.AssetError):
                assets.parse_narc(damaged)
        empty = assets.Narc((b"", b"", b"DATA", b""), NAMES)
        self.assertEqual(assets.parse_narc(assets.encode_narc(empty)), empty)
        for resource, validator, offset in ((character(), assets.validate_character, 40),
            (palette(), assets.validate_palette, 36), (character(True), lambda d: assets.validate_character(d, True), 32)):
            damaged = bytearray(resource)
            damaged[offset:offset + 4] = struct.pack("<I", 0xffffffff)
            with self.assertRaises(assets.AssetError):
                validator(damaged)
        with self.assertRaises(assets.AssetError):
            assets.merge(pin, old, new[:-1] + bytes([new[-1] ^ 1]))
        # The shipped policy has no hash override or acceptance for fixtures.
        with self.assertRaises(assets.AssetError):
            assets.merge(self.real_pins[0], old, new)

    def test_output_preflight_and_immutability(self):
        assets.ANIMATION_SHA256 = assets.digest(self.animation)
        with tempfile.TemporaryDirectory(prefix="species-assets-tests-", dir=ROOT.parent) as directory:
            root = Path(directory).resolve()
            self.assertNotIn(ROOT, root.parents)
            base, ref, output = root / "base", root / "ref", root / "output"
            for pin, old, new in self.fixtures:
                for path, data in ((base / pin.path, old), (ref / "patch" / pin.path, new)):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
            dep = base / assets.ANIMATION_PATH
            dep.parent.mkdir(parents=True, exist_ok=True)
            dep.write_bytes(self.animation)
            for invalid in (base / "out", ref / "out", ROOT / ".build-tools/species-assets-refused"):
                with self.assertRaises(assets.AssetError):
                    assets.prepare(invalid, base, ref)
                self.assertFalse(invalid.exists())
            dep.write_bytes(b"wrong dependency")
            with self.assertRaises(assets.AssetError):
                assets.prepare(output, base, ref)
            self.assertFalse(output.exists())
            dep.write_bytes(self.animation)
            manifest = assets.prepare(output, base, ref)
            self.assertTrue(manifest["private"])
            self.assertFalse(manifest["runtime_verified"])
            for pin, old, new in self.fixtures:
                self.assertEqual((base / pin.path).read_bytes(), old)
                self.assertEqual((ref / "patch" / pin.path).read_bytes(), new)
                merged = assets.parse_narc((output / pin.path).read_bytes())
                self.assertEqual(merged.members[:pin.base_count], assets.parse_narc(old).members)
            self.assertNotIn(str(root), json.dumps(manifest))
            with self.assertRaises(assets.AssetError):
                assets.prepare(output, base, ref)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archives-only", action="store_true")
    parser.add_argument("--reference-root", type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ArchiveTests))
    if not result.wasSuccessful():
        raise SystemExit(1)
    if args.reference_root:
        root = args.reference_root.resolve()
        for pin in assets.ASSETS:
            data = assets.merge(pin, assets.read_resource(root, pin.path), assets.read_resource(root, "patch/" + pin.path))
            assert assets.digest(data) == pin.reference_sha256
        print("Pinned reference archives: exact original prefixes and every appended resource validated in memory; no assets written")
    if not args.archives_only:
        run_consumers()


if __name__ == "__main__":
    main()
