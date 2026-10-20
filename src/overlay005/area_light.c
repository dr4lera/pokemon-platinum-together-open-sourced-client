#include "overlay005/area_light.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"
#include "constants/heap.h"
#include "constants/narc.h"

#include "overlay005/model_attributes.h"

#include "ascii_util.h"
#include "graphics.h"
#include "heap.h"
#include "rtc.h"
#ifdef SDK_PORT
#include <stdio.h>
#include "narc.h"
#include "port/community_area_light.h"
#endif

#define AREA_LIGHT_FILE_COUNT 4
#define SCRATCH_BUFFER_SIZE   256
#define INVALID_LIGHT_COLOR   0xFFFF

static void AreaLightManager_ApplyActiveTemplateToAreaModelAttrs(AreaLightManager *areaLightMan);
static u32 AreaLightTemplate_New(u32 archiveID, AreaLightTemplate **templates);
static void AreaLightTemplate_Free(AreaLightTemplate **template);
#ifndef SDK_PORT
static char *AreaLightTemplate_ParseLightAttrs(char *fileIter, GXRgb *lightColor, VecFx16 *lightVector);
static char *AreaLightTemplate_ParseColor(char *fileIter, GXRgb *color);
#endif

AreaLightManager *AreaLightManager_New(ModelAttributes *areaModelAttrs, const u8 archiveID)
{
    GF_ASSERT(archiveID < AREA_LIGHT_FILE_COUNT);

    AreaLightManager *areaLightMan = Heap_Alloc(HEAP_ID_FIELD1, sizeof(AreaLightManager));

    areaLightMan->areaModelAttrs = areaModelAttrs;
    areaLightMan->templateCount = AreaLightTemplate_New(archiveID, &areaLightMan->templates);
    areaLightMan->activeTemplateIndex = 0;

    int currentTime = GetSecondsSinceMidnight() / 2;

    for (int i = 0; i < areaLightMan->templateCount; i++) {
        if (areaLightMan->templates[i].endTime > currentTime) {
            areaLightMan->activeTemplateIndex = i;
            break;
        }
    }

    areaLightMan->applyToAreaModelAttrs = TRUE;
    AreaLightManager_ApplyActiveTemplateToAreaModelAttrs(areaLightMan);

    return areaLightMan;
}

void AreaLightManager_Free(AreaLightManager **areaLightMan)
{
    GF_ASSERT(areaLightMan);

    AreaLightTemplate_Free(&(*areaLightMan)->templates);
    Heap_FreeExplicit(HEAP_ID_FIELD1, *areaLightMan);

    *areaLightMan = NULL;
}

void AreaLightManager_UpdateActiveTemplate(AreaLightManager *areaLightMan)
{
    GF_ASSERT(areaLightMan);

    int currentTime = GetSecondsSinceMidnight() / 2;

    if (areaLightMan->templateCount > 1) {
        int previousEndTime = areaLightMan->activeTemplateIndex - 1 >= 0
            ? areaLightMan->templates[areaLightMan->activeTemplateIndex - 1].endTime
            : 0;

        int activeEndTime = areaLightMan->templates[areaLightMan->activeTemplateIndex].endTime;

        if (currentTime >= activeEndTime || currentTime < previousEndTime) {
            areaLightMan->activeTemplateIndex++;

            if (areaLightMan->activeTemplateIndex >= areaLightMan->templateCount) {
                areaLightMan->activeTemplateIndex = 0;
            }

            if (areaLightMan->applyToAreaModelAttrs) {
                AreaLightManager_ApplyActiveTemplateToAreaModelAttrs(areaLightMan);
            }
        }
    }
}

void AreaLightTemplate_ApplyToModelAttributes(const AreaLightTemplate *template, ModelAttributes *modelAttrs)
{
    for (int i = 0; i < GX_LIGHTS_COUNT; i++) {
        int currentLightMask = (1 << i);

        if ((template->validLightsMask & currentLightMask) != 0) {
            ModelAttributes_SetLightVector(modelAttrs, i, template->lightVectors[i].x, template->lightVectors[i].y, template->lightVectors[i].z);
            ModelAttributes_SetLightColor(modelAttrs, i, template->lightColors[i]);
        } else {
            ModelAttributes_SetLightVector(modelAttrs, i, 0, 0, 0);
            ModelAttributes_SetLightColor(modelAttrs, i, GX_RGB(0, 0, 0));
        }
    }

    ModelAttributes_SetDiffuseReflection(modelAttrs, template->diffuseReflectColor, FALSE, FALSE);
    ModelAttributes_SetAmbientReflection(modelAttrs, template->ambientReflectColor, TRUE);
    ModelAttributes_SetSpecularReflection(modelAttrs, template->specularReflectColor, FALSE, FALSE);
    ModelAttributes_SetEmissionColor(modelAttrs, template->emissionColor, TRUE);
}

static void AreaLightManager_ApplyActiveTemplateToAreaModelAttrs(AreaLightManager *areaLightMan)
{
    AreaLightTemplate *activeTemplate = &areaLightMan->templates[areaLightMan->activeTemplateIndex];
    AreaLightTemplate_ApplyToModelAttributes(activeTemplate, areaLightMan->areaModelAttrs);
}

void AreaLight_UseGlobalModelAttributes(NNSG3dResMdl *model)
{
    NNS_G3dMdlUseGlbDiff(model);
    NNS_G3dMdlUseGlbAmb(model);
    NNS_G3dMdlUseGlbSpec(model);
    NNS_G3dMdlUseGlbEmi(model);
}

static u32 AreaLightTemplate_New(u32 archiveID, AreaLightTemplate **templates)
{
#ifdef SDK_PORT
    /* The desktop asset path may preserve CRCRLF after text conversion. Read
     * the exact member extent and validate the entire CSV before publishing
     * any template. Keep the original ARM reader below unchanged. */
    NARC *narc = NARC_ctor(NARC_INDEX_DATA__AREALIGHT, HEAP_ID_FIELD1);
    void *buffer = NULL;
    CommunityAreaLightRecord *records = NULL;
    size_t count = 0;
    BOOL valid = FALSE;
    *templates = NULL;
    if (narc && archiveID < NARC_GetFileCount(narc)) {
        u32 length = NARC_GetMemberSize(narc, archiveID);
        if (length > 0 && length <= 131072) {
            buffer = Heap_Alloc(HEAP_ID_FIELD1, length);
            if (buffer) {
                NARC_ReadFromMember(narc, archiveID, 0, length, buffer);
                if (CommunityAreaLight_Parse(buffer, length, NULL, 0, &count)) {
                    records = Heap_Alloc(HEAP_ID_FIELD1, count * sizeof(*records));
                    if (records) valid = CommunityAreaLight_Parse(buffer, length, records, count, &count);
                }
            }
        }
    }
    if (narc) NARC_dtor(narc);
    if (buffer) Heap_Free(buffer);
    if (valid) {
        *templates = Heap_Alloc(HEAP_ID_FIELD1, count * sizeof(**templates));
        if (*templates) {
            MI_CpuClear8(*templates, count * sizeof(**templates));
            for (size_t i = 0; i < count; ++i) {
                AreaLightTemplate *output = &(*templates)[i];
                const CommunityAreaLightRecord *input = &records[i];
                output->endTime = input->endTime;
                output->validLightsMask = input->validLightsMask;
                for (int light = 0; light < GX_LIGHTS_COUNT; ++light) {
                    output->lightColors[light] = input->lightColors[light];
                    output->lightVectors[light].x = input->lightVectors[light][0];
                    output->lightVectors[light].y = input->lightVectors[light][1];
                    output->lightVectors[light].z = input->lightVectors[light][2];
                }
                output->diffuseReflectColor = input->materialColors[0];
                output->ambientReflectColor = input->materialColors[1];
                output->specularReflectColor = input->materialColors[2];
                output->emissionColor = input->materialColors[3];
            }
        } else valid = FALSE;
    }
    if (records) Heap_Free(records);
    if (valid) return count;
    /* A malformed desktop resource must not publish partial/black colors.
     * This explicit neutral fallback is a port stability deviation. */
    fprintf(stderr, "Community area lighting: invalid/unavailable member %u; using initialized neutral fallback\n", (unsigned)archiveID);
    fflush(stderr);
    *templates = Heap_Alloc(HEAP_ID_FIELD1, sizeof(**templates));
    GF_ASSERT(*templates != NULL);
    if (*templates == NULL) return 0;
    MI_CpuClear8(*templates, sizeof(**templates));
    (*templates)->endTime = 43200;
    (*templates)->validLightsMask = 1;
    (*templates)->lightColors[0] = GX_RGB(31, 31, 31);
    (*templates)->lightVectors[0].z = -FX16_ONE;
    (*templates)->diffuseReflectColor = GX_RGB(16, 16, 16);
    (*templates)->ambientReflectColor = GX_RGB(16, 16, 16);
    (*templates)->emissionColor = GX_RGB(16, 16, 16);
    return 1;
#else
    int i, j;
    char lineBuffer[SCRATCH_BUFFER_SIZE];
    char endTimeBuffer[SCRATCH_BUFFER_SIZE];

    void *fileBuffer = LoadMemberFromNARC(NARC_INDEX_DATA__AREALIGHT, archiveID, FALSE, HEAP_ID_FIELD1, FALSE);
    void *fileIter = fileBuffer;
    int templateCount = 0;

    do {
        fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');

        if (!(lineBuffer[0] == 'E' && lineBuffer[1] == 'O' && lineBuffer[2] == 'F')) {
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
            templateCount++;
        }
    } while (!(lineBuffer[0] == 'E' && lineBuffer[1] == 'O' && lineBuffer[2] == 'F'));

    *templates = Heap_Alloc(HEAP_ID_FIELD1, sizeof(AreaLightTemplate) * templateCount);
    MI_CpuClear8(*templates, sizeof(AreaLightTemplate) * templateCount);
    fileIter = fileBuffer;

    for (i = 0; i < templateCount; i++) {
        AreaLightTemplate *template = &(*templates)[i];
        fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
        char *iter = lineBuffer;
        iter = Ascii_CopyToTerminator(iter, endTimeBuffer, ',');
        template->endTime = Ascii_ConvertToInt(endTimeBuffer);

        for (j = 0; j < GX_LIGHTS_COUNT; j++) {
            fileIter = AreaLightTemplate_ParseLightAttrs(fileIter, &template->lightColors[j], &template->lightVectors[j]);

            if (template->lightColors[j] != INVALID_LIGHT_COLOR) {
                template->validLightsMask |= 1 << j;
            } else {
                template->lightColors[j] = GX_RGB(0, 0, 0);
            }
        }

        fileIter = AreaLightTemplate_ParseColor(fileIter, &template->diffuseReflectColor);
        fileIter = AreaLightTemplate_ParseColor(fileIter, &template->ambientReflectColor);
        fileIter = AreaLightTemplate_ParseColor(fileIter, &template->specularReflectColor);
        fileIter = AreaLightTemplate_ParseColor(fileIter, &template->emissionColor);
        fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
    }

    Heap_FreeExplicit(HEAP_ID_FIELD1, fileBuffer);

    return templateCount;
#endif
}

static void AreaLightTemplate_Free(AreaLightTemplate **template)
{
    Heap_FreeExplicit(HEAP_ID_FIELD1, *template);
    *template = NULL;
}

#ifndef SDK_PORT
static char *AreaLightTemplate_ParseLightAttrs(char *fileIter, GXRgb *lightColor, VecFx16 *lightVector)
{
    char lineBuffer[SCRATCH_BUFFER_SIZE];
    char partBuffer[SCRATCH_BUFFER_SIZE];

    fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');

    char *iter = lineBuffer;
    iter = Ascii_CopyToTerminator(iter, partBuffer, ',');
    u32 lightValid = Ascii_ConvertToInt(partBuffer);

    if (lightValid == TRUE) {
        int i;
        u16 lightColorParts[GX_COLOR_DIMS];
        s32 lightVectorParts[GX_VEC_FX_DIMS];

        for (i = 0; i < GX_COLOR_DIMS; i++) {
            iter = Ascii_CopyToTerminator(iter, partBuffer, ',');
            lightColorParts[i] = Ascii_ConvertToInt(partBuffer);
        }

        *lightColor = GX_RGB(lightColorParts[0], lightColorParts[1], lightColorParts[2]);

        for (i = 0; i < GX_VEC_FX_DIMS; i++) {
            iter = Ascii_CopyToTerminator(iter, partBuffer, ',');
            lightVectorParts[i] = Ascii_ConvertToInt(partBuffer);
        }

        lightVector->x = lightVectorParts[0];
        lightVector->y = lightVectorParts[1];
        lightVector->z = lightVectorParts[2];

        if (lightVector->x > FX16_ONE) {
            lightVector->x = FX16_ONE;
        }

        if (lightVector->x < -FX16_ONE) {
            lightVector->x = -FX16_ONE;
        }

        if (lightVector->y > FX16_ONE) {
            lightVector->y = FX16_ONE;
        }

        if (lightVector->y < -FX16_ONE) {
            lightVector->y = -FX16_ONE;
        }

        if (lightVector->z > FX16_ONE) {
            lightVector->z = FX16_ONE;
        }

        if (lightVector->z < -FX16_ONE) {
            lightVector->z = -FX16_ONE;
        }
    } else {
        *lightColor = INVALID_LIGHT_COLOR;
    }

    return fileIter;
}

static char *AreaLightTemplate_ParseColor(char *fileIter, GXRgb *color)
{
    char lineBuffer[SCRATCH_BUFFER_SIZE];
    char partBuffer[SCRATCH_BUFFER_SIZE];
    u16 colorParts[GX_COLOR_DIMS];

    fileIter = Ascii_CopyToTerminator(fileIter, lineBuffer, '\r');
    char *iter = lineBuffer;

    for (int i = 0; i < GX_COLOR_DIMS; i++) {
        iter = Ascii_CopyToTerminator(iter, partBuffer, ',');
        colorParts[i] = Ascii_ConvertToInt(partBuffer);
    }

    *color = GX_RGB(colorParts[0], colorParts[1], colorParts[2]);

    return fileIter;
}
#endif
