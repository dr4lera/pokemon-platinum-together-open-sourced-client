// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_union_binding.h"
#include <cassert>
#include <cstdio>

using namespace CommunityOnline;
static Snapshot Lobby()
{
    Snapshot s;s.state=State::Ready;s.connections=7;s.player="20";
    s.partyId="12345678-1234-1234-1234-123456789abc";
    s.members={{"10","Same",true,{}},{"20","Same",true,{}},{"30","A.-' Z9",true,{}}};
    return s;
}
static LinkSnapshot Link()
{
    LinkSnapshot l;l.state=LinkState::Active;l.party=Lobby().partyId;
    l.session="abcdef12-abcd-abcd-abcd-abcdef123456";l.epoch=9;l.seat=1;
    l.roster={{0,"10","020000000001"},{1,"20","020000000002"},{2,"30","020000000003"}};
    return l;
}
int main()
{
    const unsigned male[]={3,5,11,31,50,51,62,70},female[]={6,7,13,14,35,37,42,63};
    for(unsigned g=0;g<3;g++)for(unsigned id=0;id<256;id++){
        bool found=false;for(unsigned i=0;i<8;i++)if(g<2&&(g?female[i]:male[i])==id)found=true;
        assert(CommunityUnion_AllowedAppearance(g,id)==found);
        assert(CommunityUnion_SafeAppearance(g,id)==(found?id:g==1?6:3));
    }
    struct Guard{uint16_t a=0x1234,text[11]{},b=0xabcd;} guard;
    assert(CommunityUnion_NameCodes("Az09 .'-B.",guard.text)==10);
    const uint16_t expected[]={CHAR_A,CHAR_z,CHAR_0,CHAR_9,CHAR_SPACE,CHAR_PERIOD,CHAR_SINGLE_QUOTE_CLOSE,CHAR_MINUS,CHAR_B,CHAR_PERIOD,CHAR_EOS};
    assert(!std::memcmp(expected,guard.text,sizeof(expected)));assert(guard.a==0x1234&&guard.b==0xabcd);
    const char* invalid[]={"","  A","A ","A'","A-",".A","12345678901","A\nB","A\xff"};
    for(auto name:invalid){assert(!CommunityUnion_Name(name));assert(!CommunityUnion_NameCodes(name,guard.text));assert(guard.text[0]==CHAR_EOS);}
    assert(!CommunityUnion_Name(nullptr));assert(!CommunityUnion_NameCodes("A",nullptr));
    auto lobby=Lobby();auto link=Link();CommunityUnionRoster result{};
    assert(CommunityUnionBinding::Build(lobby,link,result));assert(result.selected&&result.active&&result.count==2);
    assert(!std::strcmp(result.peers[0].player,"10")&&!std::strcmp(result.peers[1].player,"30"));
    uint8_t mac[]={2,0,0,0,0,3};assert(CommunityUnion_FindPeer(&result,mac)==1);mac[5]=2;assert(CommunityUnion_FindPeer(&result,mac)==-1);
    auto original=result;assert(CommunityUnion_SameGeneration(&result,&original));result.connection++;assert(!CommunityUnion_SameGeneration(&result,&original));
    auto reject=[&](Snapshot s,LinkSnapshot l){assert(!CommunityUnionBinding::Build(s,l,result));assert(result.selected&&!result.active&&result.count==0);};
    auto s=lobby;auto l=link;s.members[2].name=std::string("A\0B",3);reject(s,l);
    s=lobby;s.members[2].name="12345678901";reject(s,l);
    s=lobby;s.members[0].online=false;reject(s,l);
    s=lobby;std::swap(s.members[0],s.members[1]);reject(s,l);
    s=lobby;s.player="10";reject(s,l);
    s=lobby;s.connections=0;reject(s,l);
    s=lobby;s.state=State::Reconnecting;reject(s,l);
    s=lobby;l.cancelling=true;reject(s,l);
    l=link;l.epoch=0;reject(s,l);
    l=link;l.roster[2].mac=l.roster[0].mac;reject(s,l);
    l=link;l.roster[2].mac="030000000003";reject(s,l);
    l=link;l.roster[2].mac="02AA00000003";reject(s,l);
    l=link;l.roster[2].player="030";reject(s,l);
    l=link;l.roster[2].seat=1;reject(s,l);
    l=link;l.party="not-a-party";reject(s,l);
    l=link;l.session[4]='Z';reject(s,l);
    l=link;l.seat=3;reject(s,l);
    l=link;l.roster.push_back({3,"40","020000000004"});reject(s,l);
    s=lobby;l=link;s.members.push_back({"40","Fourth",true,{}});l.roster.push_back({3,"40","020000000004"});
    assert(CommunityUnionBinding::Build(s,l,result)&&result.count==3);
    assert(!std::strcmp(result.peers[2].player,"40"));
    std::puts("Union binding: native16, bounded charcodes, signed MAC/player identity and cancellation PASS");
}
