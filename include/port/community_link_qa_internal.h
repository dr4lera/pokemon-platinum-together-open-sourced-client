// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_LINK_QA_INTERNAL_H
#define COMMUNITY_LINK_QA_INTERNAL_H
#include <nitro/types.h>
#ifdef __cplusplus
extern "C" {
#endif
/* C game data stays in community_link_qa_game.c; the C++ controller only polls
 * owned Service snapshots and prepares its authenticated transport. */
BOOL CommunityLinkQA_AuthorizeFresh(BOOL fresh);
BOOL CommunityLinkQA_IsHost(void);
BOOL CommunityLinkQA_Update(void);
void CommunityLinkQA_DidEnter(void);
#ifdef __cplusplus
}
#endif
#endif
