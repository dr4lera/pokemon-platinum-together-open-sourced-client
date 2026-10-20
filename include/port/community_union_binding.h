// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_UNION_BINDING_H
#define COMMUNITY_UNION_BINDING_H
#include "port/community_union_presentation.h"
#include "port/community_online.h"
#include <cstring>
#include <set>
namespace CommunityUnionBinding {
inline int Hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
inline bool Player(const std::string& p){return !p.empty()&&p.size()<=20&&p[0]>='1'&&p[0]<='9'&&p.find_first_not_of("0123456789")==std::string::npos;}
inline bool Session(const std::string& s){if(s.size()!=36)return false;for(unsigned i=0;i<36;i++){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(Hex(s[i])<0)return false;}return true;}
inline bool Build(const CommunityOnline::Snapshot& lobby,const CommunityOnline::LinkSnapshot& link,CommunityUnionRoster& out)
{
    out={};out.selected=1;
    if(lobby.state!=CommunityOnline::State::Ready||!lobby.connections||link.state!=CommunityOnline::LinkState::Active||link.cancelling
        ||!link.epoch||!Session(link.session)||!Session(link.party)||link.party!=lobby.partyId||link.roster.size()<2||link.roster.size()>4
        ||link.seat>=link.roster.size()||lobby.members.size()!=link.roster.size())return false;
    CommunityUnionRoster result{};result.selected=1;
    std::set<std::string> players,macs;
    for(unsigned i=0;i<link.roster.size();i++){
        const auto& peer=link.roster[i];const auto& member=lobby.members[i];
        if(peer.seat!=i||peer.player!=member.player||!Player(peer.player)||!member.online||!CommunityUnion_Name(member.name.c_str())
            ||member.name.size()>10||member.name.find('\0')!=std::string::npos||peer.mac.size()!=12||!players.insert(peer.player).second||!macs.insert(peer.mac).second)return false;
        uint8_t address[6];for(unsigned j=0;j<6;j++){int hi=Hex(peer.mac[j*2]),lo=Hex(peer.mac[j*2+1]);if(hi<0||lo<0)return false;address[j]=(uint8_t)((hi<<4)|lo);}
        if((address[0]&3)!=2)return false; // Exact roster MAC is local-unicast.
        if(i==link.seat){if(peer.player!=lobby.player)return false;continue;}
        auto& target=result.peers[result.count++];std::memcpy(target.mac,address,6);std::memcpy(target.player,peer.player.data(),peer.player.size());std::memcpy(target.name,member.name.data(),member.name.size());
    }
    result.active=1;result.connection=lobby.connections;result.epoch=link.epoch;std::memcpy(result.session,link.session.data(),36);out=result;return true;
}
}
#endif
