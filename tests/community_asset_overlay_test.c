#include "community_asset_overlay.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* The runner compiles the actual FS_OpenFile function with minimal SDK doubles. */
typedef int BOOL;
typedef unsigned int u32;
typedef int FSFileID;
typedef struct FSFile {
    FILE *pcFilePtr;
    unsigned stat;
    struct { struct { u32 top, bottom; } file; } prop;
} FSFile;
#define TRUE 1
#define FALSE 0
#define FS_FILE_STATUS_IS_FILE 1
#define FS_FILE_STATUS_IS_DIR 2
#define SDL_MESSAGEBOX_INFORMATION 0
#define SDK_VERSION_MAJOR 4
#define SDK_PORT 1
#ifdef _WIN32
#define SDK_BUILD_WIN64 1
#else
#define SDK_BUILD_LINUX 1
#endif
static int extractionCalls;
static void SDL_ShowSimpleMessageBox(int kind, const char *title, const char *message, void *window)
{
    (void)kind; (void)title; (void)message; (void)window;
}
static void SIM_ShowRomExtractionDialog(void)
{
    FILE *file;
    extractionCalls++;
    assert(extractionCalls == 1);
    file = fopen("extraction_test.narc", "wb");
    assert(file != NULL);
    fputs("extracted", file);
    fclose(file);
}
#include "actual_fs_open.inc"

static void Expect(const char *resource, const char *root, const char *expected)
{
    char contents[64] = {0};
    FILE *file = CommunityAsset_Open(resource, root);
    assert(file != NULL);
    assert(fread(contents, 1, sizeof(contents) - 1, file) == strlen(expected));
    assert(strcmp(contents, expected) == 0);
    fclose(file);
}

int main(int argc, char **argv)
{
    char normalized[COMMUNITY_ASSET_PATH_LIMIT];
    char tiny[3];
    char boundary[COMMUNITY_ASSET_PATH_LIMIT + 1];
    char missingRoot[4096];
    const char *invalid[] = {NULL, "", "/msgdata/pl_msg.narc", "//server/file", "C:/file",
        "C:file", "\\\\server\\file", "rom://msgdata/pl_msg.narc", "rom:/../file", "../file",
        "a/../file", "a/./file", "a//file", "a/", "a\\file", "http:foo", "a:b", "a./b", "a.",
        "a/..", "a/.", "a\nfile", "a file"};
    size_t i;
    FSFile file = {0};
    assert(argc == 2);
    assert(CommunityAsset_Normalize("rom:/MSGDATA/pl_msg.narc", normalized, sizeof(normalized)));
    assert(strcmp(normalized, "MSGDATA/pl_msg.narc") == 0);
    assert(CommunityAsset_Normalize("rom:msgdata/pl_msg.narc", normalized, sizeof(normalized)));
    assert(!CommunityAsset_Normalize("abc", tiny, sizeof(tiny)));
    memset(boundary, 'a', sizeof(boundary));
    boundary[COMMUNITY_ASSET_PATH_LIMIT - 1] = '\0';
    assert(CommunityAsset_Normalize(boundary, normalized, sizeof(normalized)));
    boundary[COMMUNITY_ASSET_PATH_LIMIT - 1] = 'a';
    boundary[COMMUNITY_ASSET_PATH_LIMIT] = '\0';
    assert(!CommunityAsset_Normalize(boundary, normalized, sizeof(normalized)));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(!CommunityAsset_Normalize(invalid[i], normalized, sizeof(normalized)));
        assert(normalized[0] == '\0');
        assert(CommunityAsset_Open(invalid[i], argv[1]) == NULL);
    }
    memset(normalized, 'a', sizeof(normalized));
    normalized[sizeof(normalized) - 1] = '\0';
    assert(!CommunityAsset_Normalize(normalized, tiny, sizeof(tiny)));
    assert(CommunityAsset_IsOverlayResource("rom:/POKETOOL/PERSONAL/pl_personal.narc"));
    assert(CommunityAsset_IsOverlayResource("poketool/personal/wotbl.narc"));
    assert(CommunityAsset_IsOverlayResource("msgdata/pl_msg.narc"));
    assert(CommunityAsset_IsOverlayResource("battle/graphic/pl_batt_obj.narc"));
    assert(!CommunityAsset_IsOverlayResource("battle/skill/sub_seq.narc"));
    assert(!CommunityAsset_IsOverlayResource("wazaeffect/we.arc"));
    assert(!CommunityAsset_IsOverlayResource("patch/msgdata/pl_msg.narc"));
    assert(CommunityAsset_Open("PATCH/battle/skill/sub_seq.narc", argv[1]) == NULL);
    Expect("rom:/poketool/personal/pl_personal.narc", argv[1], "patch");
    Expect("MSGDATA/PL_MSG.NARC", argv[1], "patch-lowercase");
    Expect("poketool/personal/wotbl.narc", argv[1], "runtime-base");
    Expect("battle/skill/sub_seq.narc", argv[1], "runtime-unpatched");
    Expect("wazaeffect/we.arc", argv[1], "cwd-unpatched");
    Expect("battle/graphic/pl_batt_obj.narc", argv[1], "cwd-base");
    Expect("LOCAL/LOWER.NARC", NULL, "cwd-lowercase");
    Expect("poketool/personal/pl_personal.narc", "relative-root", "cwd-personal");
    assert(snprintf(missingRoot, sizeof(missingRoot), "%s-missing", argv[1]) > 0);
    Expect("poketool/personal/pl_personal.narc", missingRoot, "cwd-personal");
    Expect("poketool/personal/pl_personal.narc", NULL, "cwd-personal");
    /* Symlink/junction escape fixtures are optional on hosts without privileges. */
    if (getenv("OVERLAY_TEST_LINKS")) {
        Expect("msgdata/pl_msg.narc", getenv("OVERLAY_TEST_ESCAPE_ROOT"), "cwd-msg");
        Expect("poketool/personal/wotbl.narc", getenv("OVERLAY_TEST_ESCAPE_ROOT"), "cwd-learnset");
    }
    file.stat = FS_FILE_STATUS_IS_DIR | 8;
    assert(FS_OpenFile(&file, "rom:/poketool/personal/pl_personal.narc"));
    assert(file.pcFilePtr != NULL && file.stat == (FS_FILE_STATUS_IS_FILE | 8));
    assert(file.prop.file.top == 0 && file.prop.file.bottom == 5);
    assert(fgetc(file.pcFilePtr) == 'p');
    fclose(file.pcFilePtr);
    assert(!FS_OpenFile(&file, "../file"));
    assert(file.pcFilePtr == NULL);
    assert(extractionCalls == 0);
    assert(FS_OpenFile(&file, "rom:/extraction_test.narc"));
    assert(extractionCalls == 1 && file.prop.file.bottom == 9);
    fclose(file.pcFilePtr);
    puts("Community asset overlay and actual FS_OpenFile integration passed");
    return 0;
}
