// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the actual runtime parser; only ENet, SDL and WM delivery are fixtures.
#include <algorithm>
#include <cassert>
#include <deque>
#include <set>
#include <vector>
#include <simulator/community_scenario.h>
#include "../subprojects/libntr/libraries/sim/src/sim_net.cpp"

static std::set<ENetPacket*> packets;
static std::deque<ENetEvent> events;
static bool sendFails=false, callbackInstalled=true;
static unsigned indications=0, acknowledgements=0, reads=0;
static const unsigned char* borrowed=nullptr;
static int borrowedLength=0;
static std::vector<unsigned char> delivered;
static ENetPeer peers[16]{};
static ENetHost host;

SDL_mutex* SDL_CreateMutex(){static SDL_mutex m;return &m;}
SDL_sem* SDL_CreateSemaphore(unsigned){static SDL_sem s;return &s;}
int SDL_LockMutex(SDL_mutex*){return 0;} int SDL_UnlockMutex(SDL_mutex*){return 0;}
int SDL_SemWaitTimeout(SDL_sem*,unsigned){return 0;} int SDL_SemPost(SDL_sem*){return 0;}
uint64_t SDL_GetTicks64(){static uint64_t t=0;return ++t;} void SDL_Delay(unsigned){}
void* SDL_CreateThread(int(*)(void*),const char*,void*){return nullptr;}
SIM_config_type* SIM_GetConfigPtr(){static SIM_config_type c{};return &c;}
ENetHost* enet_host_create(const ENetAddress*,size_t,size_t,uint32_t,uint32_t){return &host;}
ENetPeer* enet_host_connect(ENetHost*,const ENetAddress*,size_t,uint32_t){return &peers[0];}
void enet_host_destroy(ENetHost*){} int enet_address_set_host(ENetAddress*,const char*){return 0;}
void enet_peer_disconnect(ENetPeer*,uint32_t){}
ENetPacket* enet_packet_create(const void* data,size_t length,unsigned){
    auto* p=new ENetPacket{0,length,new unsigned char[length]};
    if(length) std::memcpy(p->data,data,length);
    packets.insert(p);return p;
}
void enet_packet_destroy(ENetPacket* p){
    assert(packets.erase(p)==1);
    if(borrowed)assert(reinterpret_cast<uintptr_t>(borrowed)<reinterpret_cast<uintptr_t>(p->data)
        || reinterpret_cast<uintptr_t>(borrowed)>=reinterpret_cast<uintptr_t>(p->data)+p->dataLength);
    delete[] p->data;delete p;
}
int enet_peer_send(ENetPeer*,uint8_t,ENetPacket* p){if(sendFails)return -1;++p->referenceCount;return 0;}
void enet_host_broadcast(ENetHost*,uint8_t,ENetPacket* p){enet_packet_destroy(p);}
int enet_host_service(ENetHost*,ENetEvent* out,unsigned){if(events.empty())return 0;*out=events.front();events.pop_front();return 1;}
extern "C" void WMSP_Indicate(SIM_WMSP_Indicate_t* m){
    assert(!borrowed);++indications;borrowed=m->data;
    borrowedLength=m->cmd==SIM_NET_MPPACKETCMD_JOIN_REQ?WM_SIZE_CHILD_SSID:m->dataLen;
}
extern "C" void WMSP_Wait4Wm9(){
    ++acknowledgements;
    if(callbackInstalled&&borrowed&&borrowedLength){delivered.assign(borrowed,borrowed+borrowedLength);++reads;}
    borrowed=nullptr;borrowedLength=0;
}
extern "C" void WMSP_IndicateSendBeacon(){}
static void Reset(){
    assert(!borrowed);for(auto* p:std::vector<ENetPacket*>(packets.begin(),packets.end()))enet_packet_destroy(p);
    events.clear();net={};net.state=SIM_NET_STATE_HOST;net.enetHost=&host;
    net.curPlayer.mpState=SIM_NET_MPSTATE_PARENT;
    net.aidBitmask=2;
    for(int i=0;i<16;++i){net.playerList[i].id=i;peers[i]={};}
    net.playerList[1].status=SIM_NET_PLAYERSTATUS_CLIENT;net.playerList[1].aid=1;net.playerList[1].mpState=SIM_NET_MPSTATE_CHILD;
    net.peerList[1]=&peers[1];peers[1].data=&net.playerList[1];
    sendFails=false;callbackInstalled=true;indications=acknowledgements=reads=0;delivered.clear();
}
template<class T>static ENetPacket* Packet(T data){InitPacketHeader(&data.common.header);return enet_packet_create(&data,sizeof(data),0);}
static void Dispatch(ENetPacket* p,unsigned channel=SIM_NET_CHANNEL_APPDATA){events.push_back({ENET_EVENT_TYPE_RECEIVE,&peers[1],static_cast<uint8_t>(channel),p});SIM_Net_Process();}
static ENetPacket* DataPacket(int length,unsigned aid=1,unsigned port=2){
    std::vector<unsigned char> wire(sizeof(SIM_Net_MpDataPacket_t)+3,0);
    auto* d=reinterpret_cast<SIM_Net_MpDataPacket_t*>(wire.data());InitPacketHeader(&d->common.header);
    d->common.cmd=SIM_NET_MPPACKETCMD_DATA;d->aid=aid;d->port=port;d->dataLen=length;
    d->data[0]=0x31;d->data[1]=0x32;d->data[2]=0x33;
    return enet_packet_create(wire.data(),wire.size(),0);
}
int main(){
    SIM_CommunityScenarioRecord scenario{};
    assert(SIM_CommunityScenario_Parse("36000 fff\r\n",&scenario) && scenario.frame==36000 && scenario.mask==0xfff);
    for(const char* line:{"", "-1 0", "1 -1", "36001 0", "1 1000", "999999999999999999999 0", "1 0 shell", "1", "1 z", "1 0x"}) assert(!SIM_CommunityScenario_Parse(line,&scenario));
    assert(!SIM_Net_ValidPlayerId(-1)&&!SIM_Net_ValidPlayerId(16)&&SIM_Net_ValidPlayerId(15));
    Reset();Dispatch(DataPacket(3));assert(indications==1&&acknowledgements==1&&reads==1&&delivered==std::vector<unsigned char>({0x31,0x32,0x33})&&packets.empty());
    Reset();callbackInstalled=false;Dispatch(DataPacket(3));assert(indications==1&&acknowledgements==1&&reads==0&&packets.empty());
    for(int length:{-1,2,4,513}){Reset();Dispatch(DataPacket(length));assert(indications==0&&packets.empty());}
    Reset();Dispatch(DataPacket(3,16));assert(indications==0&&packets.empty());
    Reset();Dispatch(DataPacket(3,1,16));assert(indications==0&&packets.empty());
    Reset();Dispatch(DataPacket(3,2));assert(indications==0&&packets.empty());
    Reset();net.playerList[1].aid=0;Dispatch(DataPacket(3,0));assert(indications==0&&packets.empty());
    Reset();net.playerList[1].mpState=SIM_NET_MPSTATE_OFF;Dispatch(DataPacket(3));assert(indications==0&&packets.empty());
    Reset();net.aidBitmask=0;Dispatch(DataPacket(3));assert(indications==0&&packets.empty());
    Reset();SIM_Net_MpPacketCommon_t shortData{};InitPacketHeader(&shortData.header);shortData.cmd=SIM_NET_MPPACKETCMD_DATA;Dispatch(enet_packet_create(&shortData,sizeof(shortData),0));assert(indications==0&&packets.empty());
    for(int id:{-1,0,2,16}){Reset();SIM_Net_PlayerInfoPacket_t info{};info.common.cmd=SIM_NET_MGMTPACKETCMD_PLAYER_INFO;info.player.id=id;Dispatch(Packet(info),SIM_NET_CHANNEL_MGMT);assert(net.playerList[1].id==1&&packets.empty());}
    Reset();SIM_Net_PlayerInfoPacket_t info{};info.common.cmd=SIM_NET_MGMTPACKETCMD_PLAYER_INFO;info.player=net.playerList[1];info.player.userGameInfoLength=113;Dispatch(Packet(info),SIM_NET_CHANNEL_MGMT);assert(net.playerList[1].userGameInfoLength==0&&packets.empty());
    Reset();info.player=net.playerList[1];std::fill(std::begin(info.player.userName),std::end(info.player.userName),'x');Dispatch(Packet(info),SIM_NET_CHANNEL_MGMT);assert(net.playerList[1].userName[10]=='\0'&&packets.empty());
    Reset();net.aidBitmask=0;net.playerList[1].aid=0;net.playerList[1].mpState=SIM_NET_MPSTATE_OFF;SIM_Net_JoinReqPacket_t join{};join.common.cmd=SIM_NET_MPPACKETCMD_JOIN_REQ;join.playerId=1;std::fill(std::begin(join.ssid),std::end(join.ssid),0x55);Dispatch(Packet(join));assert(indications==1&&acknowledgements==1&&delivered.size()==24);
    unsigned aid=net.playerList[1].aid;Dispatch(Packet(join));assert(indications==1&&net.playerList[1].aid==aid);
    Reset();join.playerId=2;Dispatch(Packet(join));assert(indications==0&&packets.empty());
    Reset();shortData.cmd=SIM_NET_MPPACKETCMD_JOIN_REQ;Dispatch(enet_packet_create(&shortData,sizeof(shortData),0));assert(indications==0&&packets.empty());
    Reset();net.state=SIM_NET_STATE_CLIENT;net.curPlayer.mpState=SIM_NET_MPSTATE_CHILD;net.curPlayer.aid=1;net.playerList[1].mpState=SIM_NET_MPSTATE_PARENT;net.playerList[1].aid=0;
    SIM_Net_DisconnectPacket_t disconnect{};disconnect.common.cmd=SIM_NET_MPPACKETCMD_DISCONNECT;Dispatch(Packet(disconnect));assert(net.curPlayer.mpState==SIM_NET_MPSTATE_OFF&&net.curPlayer.aid==0&&acknowledgements==1&&packets.empty());
    Reset();net.peerList[1]=nullptr;unsigned char byte=1;assert(!SIM_Net_SendMPData(2,0,&byte,1)&&packets.empty());assert(SIM_Net_DisconnectMP(2,nullptr)&&packets.empty());
    Reset();sendFails=true;assert(!SIM_Net_SendMPData(2,0,&byte,1)&&packets.empty());
    Reset();assert(!SIM_Net_SendMPData(2,16,&byte,1)&&!SIM_Net_SendMPData(2,0,&byte,-1)&&!SIM_Net_SendMPData(2,0,&byte,513)&&packets.empty());
    Reset();net.state=SIM_NET_STATE_CLIENT;net.curPlayer.id=2;net.peerList[0]=&peers[1];peers[1].data=&net.playerList[0];SIM_Net_PlayerListPacket_t list{};list.common.cmd=SIM_NET_MGMTPACKETCMD_PLAYER_LIST;list.numPlayers=2;for(int i=0;i<16;++i)list.playerList[i].id=i;
    Dispatch(Packet(list),SIM_NET_CHANNEL_MGMT);assert(net.playerList[1].id==1&&packets.empty());
    Reset();assert(packets.empty());
    struct ProviderTest { unsigned calls=0; int ggid=-1; SIM_Net_MPState_t role=SIM_NET_MPSTATE_OFF; SIM_Net_Player_t players[16]{}; } fixture;
    SIM_Net_Provider_t provider{1,&fixture,
        [](void* p){++static_cast<ProviderTest*>(p)->calls;},
        [](void* p){++static_cast<ProviderTest*>(p)->calls;},
        [](void*)->BOOL{return TRUE;}, [](void*,const char* address)->BOOL{return std::strcmp(address,"fixture")==0;},
        [](void*){return 7;}, [](void*){return 3;}, [](void*){return SIM_NET_STATE_CLIENT;}, [](void*){return 4;},
        [](void* p)->const SIM_Net_Player_t*{return static_cast<ProviderTest*>(p)->players;},
        [](void* p,int ggid,int,const u8*,int){static_cast<ProviderTest*>(p)->ggid=ggid;},
        [](void* p,SIM_Net_MPState_t role){static_cast<ProviderTest*>(p)->role=role;},
        [](void*,const u8*,const u8*)->BOOL{return TRUE;},
        [](void*,u16 mask,u16* result)->BOOL{if(result)*result=mask;return TRUE;},
        [](void*,u16,u8,const u8*,int)->BOOL{return TRUE;},
        [](void*,WMBssDesc*){return 2;}};
    assert(!SIM_Net_InstallProvider(nullptr) && !SIM_Net_InstallProvider(&provider));
    net.state=SIM_NET_STATE_DISCONNECTED;net.enetHost=nullptr;net.curPlayer.ggid=91;
    provider.version=2;assert(!SIM_Net_InstallProvider(&provider));provider.version=1;
    assert(SIM_Net_InstallProvider(&provider) && fixture.ggid==91 && fixture.role==SIM_NET_MPSTATE_PARENT);
    assert(SIM_Net_InstallProvider(&provider));auto another=provider;assert(!SIM_Net_InstallProvider(&another));
    SIM_Net_Process();SIM_Net_SendBeaconIndication();assert(fixture.calls==2);
    char address[]="fixture";assert(SIM_Net_StartHost()&&SIM_Net_StartClient(address));
    assert(SIM_Net_GetAID()==7 && SIM_Net_GetCurrentPlayerID()==3 && SIM_Net_GetCurrentState()==SIM_NET_STATE_CLIENT && SIM_Net_GetPlayerCount()==4 && SIM_Net_GetPlayerList()==fixture.players);
    SIM_Net_SetGameInfo(42,0,nullptr,0);SIM_Net_SetMPState(SIM_NET_MPSTATE_CHILD);
    assert(fixture.ggid==42 && fixture.role==SIM_NET_MPSTATE_CHILD && SIM_Net_JoinMP(nullptr,nullptr));
    u16 result=0;assert(SIM_Net_DisconnectMP(17,&result)&&result==17&&SIM_Net_SendMPData(1,0,nullptr,0));
    assert(SIM_Net_Scan(nullptr)==2);
    std::puts("Actual wireless parser and provider routing: bounds, peer binding, callback/ack/release ownership, duplicate joins, AID transitions, send cleanup, explicit one-way provider installation and all WM dispatch paths passed.");
}
