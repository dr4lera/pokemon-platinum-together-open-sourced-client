// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_QA_H
#define COMMUNITY_QA_H
#include "savedata.h"
/* Explicit disposable fresh-profile scenes, never ordinary startup or saves. */
BOOL CommunityQA_TryStart(SaveData *saveData);
#endif
