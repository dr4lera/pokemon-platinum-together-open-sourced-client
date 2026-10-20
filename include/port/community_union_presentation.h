// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_UNION_PRESENTATION_H
#define COMMUNITY_UNION_PRESENTATION_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "constants/charcode.h"
#define COMMUNITY_UNION_REMOTE_MAX 3
typedef struct CommunityUnionPeer {
    uint8_t mac[6];
    char player[21], name[11];
} CommunityUnionPeer;
typedef struct CommunityUnionRoster {
    int selected, active;
    uint64_t connection, epoch;
    char session[37];
    unsigned count;
    CommunityUnionPeer peers[COMMUNITY_UNION_REMOTE_MAX];
} CommunityUnionRoster;
static inline int CommunityUnion_AllowedAppearance(unsigned gender, unsigned graphics)
{
    static const unsigned male[8] = {3,5,11,31,50,51,62,70};
    static const unsigned female[8] = {6,7,13,14,35,37,42,63};
    unsigned i;
    if (gender > 1) return 0;
    for (i=0;i<8;i++) if ((gender ? female[i] : male[i]) == graphics) return 1;
    return 0;
}
static inline unsigned CommunityUnion_SafeAppearance(unsigned gender, unsigned graphics)
{
    return CommunityUnion_AllowedAppearance(gender, graphics) ? graphics : gender==1 ? 6 : 3;
}
static inline int CommunityUnion_Name(const char *name)
{
    size_t i, length=0;
    if (!name) return 0;
    while (length<11 && name[length]) length++;
    if (!length || length>10) return 0;
    for (i=0;i<length;i++) {
        unsigned c=(unsigned char)name[i];
        int alnum=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');
        if (!alnum && (i==0 || (i==length-1&&c!='.') || (c!=' '&&c!='.'&&c!='\''&&c!='-'))) return 0;
    }
    return 1;
}
static inline unsigned CommunityUnion_NameCodes(const char *name, uint16_t output[11])
{
    unsigned i;
    if (!output) return 0;
    memset(output,0,11*sizeof(*output));
    output[0]=CHAR_EOS;
    if (!CommunityUnion_Name(name)) return 0;
    for (i=0;name[i];i++) {
        unsigned c=(unsigned char)name[i];
        if(c>='A'&&c<='Z')output[i]=(uint16_t)(CHAR_A+c-'A');
        else if(c>='a'&&c<='z')output[i]=(uint16_t)(CHAR_a+c-'a');
        else if(c>='0'&&c<='9')output[i]=(uint16_t)(CHAR_0+c-'0');
        else output[i]=(uint16_t)(c==' ' ? CHAR_SPACE : c=='.' ? CHAR_PERIOD
            : c=='\'' ? CHAR_SINGLE_QUOTE_CLOSE : CHAR_MINUS);
    }
    output[i]=CHAR_EOS;
    return i;
}
static inline int CommunityUnion_SameGeneration(const CommunityUnionRoster *a,const CommunityUnionRoster *b)
{
    return a->active && b->active && a->connection==b->connection && a->epoch==b->epoch
        && memcmp(a->session,b->session,sizeof(a->session))==0;
}
static inline int CommunityUnion_FindPeer(const CommunityUnionRoster *roster,const uint8_t mac[6])
{
    unsigned i;
    if (!roster->active || roster->count>COMMUNITY_UNION_REMOTE_MAX || !mac) return -1;
    for(i=0;i<roster->count;i++) if(!memcmp(roster->peers[i].mac,mac,6)) return (int)i;
    return -1;
}
#ifdef __cplusplus
extern "C" {
#endif
void CommunityUnion_CopyRoster(CommunityUnionRoster *output);
struct FieldSystem_t;
struct MapObject;
void CommunityUnion_LabelsBegin(struct FieldSystem_t *field);
int CommunityUnion_BeaconAllowed(const uint8_t mac[6]);
void CommunityUnion_LabelsBind(struct FieldSystem_t *field,unsigned localId,struct MapObject *object,const uint8_t mac[6]);
void CommunityUnion_LabelsFinish(void);
void CommunityUnion_LabelsRender(struct FieldSystem_t *field);
void CommunityUnion_LabelsEnd(struct FieldSystem_t *field);
#ifdef __cplusplus
}
#endif
#endif
