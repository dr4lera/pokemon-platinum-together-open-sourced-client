// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_LINK_QA_H
#define COMMUNITY_LINK_QA_H
#include <nitro/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "field/field_system_decl.h"
#include "savedata.h"
/* Explicit disposable QA only. Preflight MUST precede SaveData_Init(). A
 * requested but invalid scene terminates before the backup API can open saves. */
void CommunityLinkQA_Preflight(void);
BOOL CommunityLinkQA_TryStart(SaveData *saveData);
/* Game/field thread, before normal input processing; never a network callback. */
void CommunityLinkQA_Tick(FieldSystem *fieldSystem);
#ifdef __cplusplus
}
#endif
#endif
