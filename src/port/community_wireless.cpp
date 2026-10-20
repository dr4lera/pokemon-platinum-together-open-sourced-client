// SPDX-License-Identifier: GPL-3.0-or-later
#include <nitro.h>
#include <simulator/sim_net.h>
#include <simulator/wmsp_indicate.h>
#include "port/community_wireless.h"
#include "port/community_wireless_core.h"
#include "port/sim_config_prj.h"
extern "C" {
#include "port/community_catalog.h"
#include "port/community_modern_rules.h"
#include "port/community_rules.h"
void WMSP_Wait4Wm9(void);
}
#include <openssl/sha.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <utility>

namespace CommunityWireless {
namespace {
struct Provider {
    std::mutex mutex;
    std::condition_variable changed;
    Core core;
    CommunityOnline::Service online;
    std::string generation, blocked, error;
    std::uint32_t ggid = 0, tgid = 0, nonce = 0;
    Role configuredRole = Role::Off;
    ParentInfo parent;
    std::vector<std::uint8_t> gameInfo;
};
Provider& Context() { static Provider* p = new Provider; return *p; }
Provider& Get(void* p) { return *static_cast<Provider*>(p); }
void Fail(Provider& p, const char* reason)
{
    p.error = reason; p.blocked = p.generation; p.core.End();
    p.online.CancelLink(); p.changed.notify_all();
}
bool Flush(Provider& p)
{
    for (auto& frame : p.core.TakeFrames()) if (!p.online.SendLink(frame.destination,frame.bytes)) {
        Fail(p,"Wireless send could not be queued; the link was closed."); return false;
    }
    return true;
}
int Hex(char c) { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : -1; }
bool Roster(const CommunityOnline::LinkSnapshot& link, std::vector<std::array<std::uint8_t,6>>& result)
{
    if (link.roster.size() < 2 || link.roster.size() > MaxPeers || link.seat >= link.roster.size()) return false;
    for (unsigned i = 0; i < link.roster.size(); ++i) {
        const auto& peer = link.roster[i];
        if (peer.seat != i || peer.mac.size() != 12) return false;
        std::array<std::uint8_t,6> mac{};
        for (unsigned j=0;j<6;++j) {
            int high=Hex(peer.mac[j*2]),low=Hex(peer.mac[j*2+1]);
            if (high < 0 || low < 0) return false;
            mac[j]=static_cast<std::uint8_t>((high<<4)|low);
        }
        result.push_back(mac);
    }
    return true;
}
void Process(void* opaque)
{
    auto& p = Get(opaque);
    const auto link = p.online.ReadLink();
    std::vector<Event> events;
    {
        std::lock_guard<std::mutex> lock(p.mutex);
        const std::string generation = link.session + ":" + std::to_string(link.epoch);
        if (link.state != CommunityOnline::LinkState::Active) {
            if (p.core.Active()) { p.core.End(); p.changed.notify_all(); }
        } else if (generation != p.blocked) {
            if (!p.core.Active() || generation != p.generation) {
                p.core.End(); events=p.core.TakeEvents(); p.generation=generation;
                std::vector<std::array<std::uint8_t,6>> roster;
                if (!Roster(link,roster) || !p.core.Begin(link.seat,roster)) Fail(p,"The server's wireless roster is invalid.");
                else {
                    p.error.clear();
                    p.core.SetInfo(p.ggid,p.tgid,p.gameInfo.data(),static_cast<unsigned>(p.gameInfo.size()),p.parent);
                    // An association/AID cannot survive another session epoch.
                    if (p.configuredRole == Role::Parent) p.core.SetRole(Role::Parent);
                }
            }
            CommunityOnline::LinkPacket packet;
            unsigned handled = 0;
            while (p.core.Active() && handled++ < 64 && p.online.ReceiveLink(packet)) {
                if (packet.session != link.session || packet.epoch != link.epoch) continue;
                if (!p.core.Receive(packet.sender,packet.payload)) { Fail(p,"A wireless message was invalid; the link was closed."); break; }
            }
            if (p.core.Active()) Flush(p);
            p.changed.notify_all();
        }
        auto pending=p.core.TakeEvents();
        for (auto& event:pending) events.push_back(std::move(event));
    }
    // WM queues a pointer into each Event's owned vector. It remains alive until
    // ARM9 clears its callback bit. Never wait while holding the provider lock.
    for (auto& event : events) {
        if (event.kind != EventKind::Disconnect) {
            const auto current=p.online.ReadLink();
            if (current.state!=CommunityOnline::LinkState::Active || current.session!=link.session || current.epoch!=link.epoch) continue;
        }
        SIM_WMSP_Indicate_t indication{};
        indication.cmd = event.kind == EventKind::Join ? SIM_NET_MPPACKETCMD_JOIN_REQ
            : event.kind == EventKind::Disconnect ? SIM_NET_MPPACKETCMD_DISCONNECT : SIM_NET_MPPACKETCMD_DATA;
        indication.aid=event.aid; indication.port=event.port;
        indication.dataLen=static_cast<int>(event.bytes.size()); indication.data=event.bytes.data();
        WMSP_Indicate(&indication); WMSP_Wait4Wm9();
    }
}
void Beacon(void* opaque)
{
    auto& p=Get(opaque); bool parent;
    { std::lock_guard<std::mutex> lock(p.mutex); parent=p.core.Active() && p.core.GetPeer(p.core.LocalSeat()).role==Role::Parent; }
    if (parent) WMSP_IndicateSendBeacon();
}
BOOL StartHost(void*) { return FALSE; }
BOOL StartClient(void*, const char*) { return FALSE; }
int Aid(void* opaque) { auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex); return p.core.Active() ? p.core.GetPeer(p.core.LocalSeat()).aid : 0; }
int Seat(void* opaque) { auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex); return p.core.Active() ? static_cast<int>(p.core.LocalSeat()) : 0; }
SIM_Net_State_t State(void* opaque) {
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex);
    return !p.core.Active() ? SIM_NET_STATE_DISCONNECTED : p.core.LocalSeat()==0 ? SIM_NET_STATE_HOST : SIM_NET_STATE_CLIENT;
}
int Count(void* opaque) { auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex); return static_cast<int>(p.core.Count()); }
const SIM_Net_Player_t* Players(void* opaque)
{
    thread_local SIM_Net_Player_t copy[SIM_NET_MAX_PLAYERS]; std::memset(copy,0,sizeof(copy));
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex);
    for (unsigned i=0;i<p.core.Count();++i) {
        const auto& source=p.core.GetPeer(i); auto& dest=copy[i];
        dest.id=static_cast<int>(i); dest.status=i==0 ? SIM_NET_PLAYERSTATUS_HOST : SIM_NET_PLAYERSTATUS_CLIENT;
        std::memcpy(dest.ndsMacAddr,source.mac.data(),6);
        dest.mpState=static_cast<SIM_Net_MPState_t>(source.role); dest.aid=source.aid;
        dest.ggid=static_cast<int>(source.ggid); dest.tgid=static_cast<int>(source.tgid);
        dest.userGameInfoLength=static_cast<u8>(source.gameInfo.size());
        std::copy(source.gameInfo.begin(),source.gameInfo.end(),dest.userGameInfo);
    }
    return copy;
}
void Info(void* opaque,int ggid,int tgid,const u8* info,int size,const SIM_Net_ParentInfo_t* parent = nullptr)
{
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex);
    if (size < 0 || size > 112 || (size && !info)) { Fail(p,"Wireless game information is invalid."); return; }
    p.ggid=static_cast<std::uint32_t>(ggid); p.tgid=static_cast<std::uint32_t>(tgid);
    p.parent = parent ? ParentInfo{parent->parentMaxSize,parent->childMaxSize,parent->beaconPeriod,parent->channel,parent->maxEntry,parent->attributes} : ParentInfo{};
    p.gameInfo.clear(); if (size) p.gameInfo.assign(info,info+size);
    if (!p.core.SetInfo(p.ggid,p.tgid,info,static_cast<unsigned>(size),p.parent)) Fail(p,"Wireless game information could not be queued.");
    else if (p.core.Active()) Flush(p);
}
void RoleChange(void* opaque,SIM_Net_MPState_t state)
{
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex);
    if (state < SIM_NET_MPSTATE_OFF || state >= SIM_NET_MPSTATE_COUNT) { Fail(p,"Wireless state is invalid."); return; }
    p.configuredRole=static_cast<Role>(state);
    if (!p.core.SetRole(p.configuredRole)) { Fail(p,"Wireless state could not be changed."); return; }
    if (p.core.Active()) Flush(p);
}
BOOL Join(void* opaque,const u8* mac,const u8* ssid)
{
    auto& p=Get(opaque); std::unique_lock<std::mutex> lock(p.mutex);
    if (++p.nonce==0) ++p.nonce;
    const auto generation=p.generation;
    if (!p.core.Join(mac,ssid,p.nonce) || !Flush(p)) return FALSE;
    const bool complete=p.changed.wait_for(lock,std::chrono::seconds(5),[&] {
        return !p.core.Active() || p.generation!=generation || !p.core.Joining();
    });
    if (!complete) { p.core.CancelJoin(); Fail(p,"The wireless join timed out."); return FALSE; }
    return p.core.Active() && p.generation==generation && p.core.GetPeer(p.core.LocalSeat()).role==Role::Child ? TRUE : FALSE;
}
BOOL Disconnect(void* opaque,u16 mask,u16* result)
{
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex); std::uint16_t disconnected=0;
    bool ok=p.core.Disconnect(mask,disconnected); if (ok) ok=Flush(p);
    if (result) *result=disconnected;
    return ok ? TRUE : FALSE;
}
BOOL Send(void* opaque,u16 mask,u8 port,const u8* data,int size)
{
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex);
    if (size < 0 || !p.core.Send(mask,port,data,static_cast<unsigned>(size))) return FALSE;
    return Flush(p) ? TRUE : FALSE;
}
int Scan(void* opaque,WMBssDesc* output)
{
    if (!output) return 0;
    auto& p=Get(opaque); std::lock_guard<std::mutex> lock(p.mutex); unsigned found=0;
    for (unsigned i=0;i<p.core.Count();++i) if (i!=p.core.LocalSeat()) {
        const auto& peer=p.core.GetPeer(i); if (peer.role!=Role::Parent) continue;
        auto& bss=output[found++]; std::memset(&bss,0,sizeof(bss));
        std::memcpy(bss.bssid,peer.mac.data(),6);
        bss.gameInfo.magicNumber=WM_GAMEINFO_MAGIC_NUMBER;
        bss.gameInfo.ver=WM_GAMEINFO_VERSION_NUMBER; bss.gameInfo.platform=0;
        bss.gameInfoLength=static_cast<u16>(WM_GAMEINFO_LENGTH_MIN+peer.gameInfo.size());
        bss.length=static_cast<u16>((offsetof(WMBssDesc,gameInfo)+((bss.gameInfoLength+1u)&~1u))/2u);
        bss.channel=peer.parent.channel;
        bss.beaconPeriod=peer.parent.beaconPeriod;
        bss.gameInfo.gameNameCount_attribute=peer.parent.attributes; bss.gameInfo.ggid=peer.ggid; bss.gameInfo.tgid=static_cast<u16>(peer.tgid);
        bss.gameInfo.parentMaxSize=peer.parent.parentMaxSize; bss.gameInfo.childMaxSize=peer.parent.childMaxSize;
        bss.gameInfo.userGameInfoLength=static_cast<u8>(peer.gameInfo.size());
        if (!peer.gameInfo.empty()) std::memcpy(bss.gameInfo.userGameInfo,peer.gameInfo.data(),peer.gameInfo.size());
    }
    return static_cast<int>(found);
}
std::string Hash(const void* data,std::size_t size)
{
    unsigned char digest[SHA256_DIGEST_LENGTH]; SHA256(static_cast<const unsigned char*>(data),size,digest);
    static const char hex[]="0123456789abcdef"; std::string output;
    for (auto byte:digest) { output.push_back(hex[byte>>4]); output.push_back(hex[byte&15]); }
    return output;
}
}
CommunityOnline::Service& Online() { return Context().online; }
bool RulesLocked()
{
    const auto state=Online().ReadLink().state;
    return state!=CommunityOnline::LinkState::Idle && state!=CommunityOnline::LinkState::Ended && state!=CommunityOnline::LinkState::Failed;
}
bool Install()
{
    static const SIM_Net_Provider_t provider = {2,&Context(),Process,Beacon,StartHost,StartClient,Aid,Seat,State,Count,Players,Info,RoleChange,Join,Disconnect,Send,Scan};
    if (SIM_Net_InstallProvider(&provider)) return true;
    auto& p=Context(); std::lock_guard<std::mutex> lock(p.mutex);
    p.error="Close the local wireless connection before selecting a community server."; return false;
}
bool Prepare()
{
    if (!Install()) return false;
    auto* config=SIM_Config_prj_GetConfig();
    std::string rules="community-wm-v2;cap="+std::to_string(CommunityRules_CurrentLevelCap())
        +";caps="+std::to_string(config->worldLevelCaps ? 1 : 0)
        +";60fps="+std::to_string(config->enable60fps ? 1 : 0)
        +";speed="+std::to_string(config->enable60fpsSpeedFix ? 1 : 0)
        +";walk="+std::to_string(config->walkThroughWalls ? 1 : 0)
        +";encounters="+std::to_string(config->disableRandomEncounters ? 1 : 0)
        +";run="+std::to_string(config->runFromTrainerBattles ? 1 : 0)
        +";obedience=ot-independent;capture=wild-one-debit-v1;modern="+std::to_string(CommunityModernRules_Enabled())
        +";ability0110-v1="+std::to_string(CommunityAbilityUpdates_Enabled());
    const char legacy[]="pokeplatinum-pc-port-35067868-community-fairy-v1";
    std::string catalogHash=Hash(legacy,sizeof(legacy)-1);
    if (CommunityModernRules_Enabled()) {
        const auto* catalog=CommunityCatalog_Runtime();
        std::vector<std::uint8_t> bytes;
        const auto append=[&](const void* data,std::size_t size) { auto* start=static_cast<const std::uint8_t*>(data); bytes.insert(bytes.end(),start,start+size); };
        append(catalog->personal,sizeof(catalog->personal)); append(catalog->learnsets,sizeof(catalog->learnsets));
        append(catalog->supplements,sizeof(catalog->supplements)); append(catalog->types,sizeof(catalog->types)); append(catalog->moves,sizeof(catalog->moves));
        append(catalog->metadata,sizeof(catalog->metadata)); append(catalog->evolutions,sizeof(catalog->evolutions));
        append(catalog->spriteData,sizeof(catalog->spriteData)); append(catalog->spriteHeights,sizeof(catalog->spriteHeights));
        append(catalog->iconPalettes,sizeof(catalog->iconPalettes)); append(catalog->formGraphics,sizeof(catalog->formGraphics));
        append(catalog->fairyMovePalette,sizeof(catalog->fairyMovePalette));
        append(catalog->modernMoves,sizeof(catalog->modernMoves)); append(catalog->statStages,sizeof(catalog->statStages));
        append(catalog->sideEffectScripts,sizeof(catalog->sideEffectScripts)); append(catalog->metronomeExclusions,sizeof(catalog->metronomeExclusions));
        catalogHash=Hash(bytes.data(),bytes.size());
    }
    return Online().PrepareLink(catalogHash,Hash(rules.data(),rules.size()));
}
std::string Status() { auto& p=Context(); std::lock_guard<std::mutex> lock(p.mutex); return p.error; }
}
