// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the actual provider in this TU to exercise its private callbacks with
// two real Services. Only the SDK callback recipient/config/catalog are stubs.
#define SDL_MAIN_HANDLED
#include "../src/port/community_wireless.cpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Indication {int cmd=0,aid=0,port=0;std::vector<u8> bytes;};
struct Capture {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Indication> completed;
    SIM_WMSP_Indicate_t pending{};
    bool pendingValid=false,holdData=false,ack=false,stopping=false;
    unsigned waits=0,beacons=0;
    void Release(){std::lock_guard<std::mutex> lock(mutex);stopping=true;ack=true;changed.notify_all();}
};
thread_local Capture* recipient=nullptr;
const SIM_Net_Provider_t* installed=nullptr;
SIM_Config_prj_type config{};
template<class Condition> void Wait(Condition condition,const char* reason){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(std::chrono::steady_clock::now()<deadline){if(condition())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error(reason);}
bool Completed(Capture& capture,int command,int aid,int port,const std::vector<u8>& bytes){std::lock_guard<std::mutex> lock(capture.mutex);return std::any_of(capture.completed.begin(),capture.completed.end(),[&](const auto& item){return item.cmd==command&&item.aid==aid&&item.port==port&&item.bytes==bytes;});}
class Pumps {
    std::atomic<bool> stop{false};
    std::atomic<bool> pauseSecond{false},secondPaused{false};
    std::thread first,second;
    Capture& a;Capture& b;
    void Run(CommunityWireless::Provider& p,Capture& c,bool isSecond){recipient=&c;while(!stop){if(isSecond&&pauseSecond){secondPaused=true;while(pauseSecond&&!stop)std::this_thread::sleep_for(std::chrono::milliseconds(1));secondPaused=false;}if(stop)break;CommunityWireless::Process(&p);std::this_thread::sleep_for(std::chrono::milliseconds(2));}recipient=nullptr;}
public:
    Pumps(CommunityWireless::Provider& p,CommunityWireless::Provider& q,Capture& x,Capture& y):a(x),b(y){first=std::thread([&]{Run(p,x,false);});second=std::thread([&]{Run(q,y,true);});}
    void PauseSecond(bool enabled){pauseSecond=enabled;if(enabled)Wait([&]{return secondPaused.load();},"provider pump did not pause");}
    ~Pumps(){stop=true;a.Release();b.Release();first.join();second.join();}
};
void Active(CommunityWireless::Provider& p){Wait([&]{auto s=p.online.ReadLink();if(s.state==CommunityOnline::LinkState::Failed)throw std::runtime_error(s.error);return s.state==CommunityOnline::LinkState::Active;},"link did not become active");}
void Prepare(CommunityWireless::Provider& p,CommunityWireless::Provider& q){
    const auto catalog=CommunityWireless::Hash("provider-fixture-catalog-v1",27),rules=CommunityWireless::Hash("provider-fixture-rules-v1",25);
    bool first=false,second=false;Wait([&]{if(!first)first=p.online.PrepareLink(catalog,rules);if(!second)second=q.online.PrepareLink(catalog,rules);return first&&second;},"prepare remained busy");
    Wait([&]{return p.online.ReadLink().state==CommunityOnline::LinkState::Prepared&&q.online.ReadLink().state==CommunityOnline::LinkState::Prepared;},"prepare acknowledgment absent");Require(p.online.OpenLink(),"leader link open refused");Active(p);Active(q);
}
}

extern "C" BOOL SIM_Net_InstallProvider(const SIM_Net_Provider_t* provider){installed=provider;return TRUE;}
extern "C" SIM_Config_prj_type* SIM_Config_prj_GetConfig(){return &config;}
extern "C" int CommunityRules_CurrentLevelCap(){return 50;}
extern "C" int CommunityModernRules_Enabled(){return 0;}
extern "C" const CommunityCatalog* CommunityCatalog_Runtime(){static const CommunityCatalog catalog{};return &catalog;}
extern "C" void WMSP_Indicate(SIM_WMSP_Indicate_t* indication){
    Require(recipient&&indication&&indication->dataLen>=0,"invalid callback recipient");std::lock_guard<std::mutex> lock(recipient->mutex);Require(!recipient->pendingValid,"callback replaced before acknowledgment");recipient->pending=*indication;recipient->pendingValid=true;recipient->ack=false;recipient->changed.notify_all();
}
extern "C" void WMSP_Wait4Wm9(){
    Require(recipient,"missing acknowledgment context");std::unique_lock<std::mutex> lock(recipient->mutex);Require(recipient->pendingValid,"wait without callback");recipient->waits++;
    if(recipient->holdData&&recipient->pending.cmd==SIM_NET_MPPACKETCMD_DATA)recipient->changed.wait(lock,[&]{return recipient->ack||recipient->stopping;});
    const auto& pending=recipient->pending;Indication copy{static_cast<int>(pending.cmd),pending.aid,pending.port,{}};if(pending.dataLen)copy.bytes.assign(pending.data,pending.data+pending.dataLen);recipient->completed.push_back(std::move(copy));recipient->pendingValid=false;recipient->changed.notify_all();
}
extern "C" void WMSP_IndicateSendBeacon(){Require(recipient,"missing beacon context");std::lock_guard<std::mutex> lock(recipient->mutex);recipient->beacons++;}

int main(int argc,char** argv){try{
    Require(argc==4,"Expected loopback origin, pin and fresh profile root");
    CommunityWireless::Provider p,q;Capture a,b;
    CommunityOnline::Options options;options.serverUrl=argv[1];options.serverKey=argv[2];options.profileDirectory=std::string(argv[3])+"/leader";options.username="WMLeader";options.build="community-client-provider-fixture-v1";options.pingIntervalMs=1000;options.pongTimeoutMs=1000;
    Require(p.online.Connect(options),"first adapter refused connect");options.profileDirectory=std::string(argv[3])+"/guest";options.username="WMGuest";Require(q.online.Connect(options),"second adapter refused connect");
    Wait([&]{auto x=p.online.Read(),y=q.online.Read();if(x.state==CommunityOnline::State::Failed||y.state==CommunityOnline::State::Failed)throw std::runtime_error(x.error+" "+y.error);return x.state==CommunityOnline::State::Ready&&y.state==CommunityOnline::State::Ready;},"accounts did not authenticate");
    Require(p.online.CreateParty(),"party create refused");Wait([&]{return !p.online.Read().partyCode.empty();},"party missing");Require(q.online.JoinParty(p.online.Read().partyCode),"party join refused");Wait([&]{return p.online.Read().members.size()==2&&q.online.Read().members.size()==2;},"party roster missing");
    Require(CommunityWireless::Install()&&installed&&installed->version==1&&installed->process==CommunityWireless::Process,"actual provider table not registered");
    const std::vector<u8> gameInfo{3,1,4,1,5,9};CommunityWireless::Info(&q,0x12345678,0x2345,gameInfo.data(),static_cast<int>(gameInfo.size()));CommunityWireless::RoleChange(&q,SIM_NET_MPSTATE_PARENT);
    Pumps pump(p,q,a,b);Prepare(p,q);
    WMBssDesc scan[4]{};Wait([&]{return CommunityWireless::Scan(&p,scan)==1;},"parent not discovered");
    Require(CommunityWireless::Seat(&p)==0&&CommunityWireless::Seat(&q)==1&&CommunityWireless::Aid(&q)==0,"routing seat incorrectly selected DS parent");Require(scan[0].gameInfo.ggid==0x12345678&&scan[0].gameInfo.tgid==0x2345&&scan[0].gameInfo.userGameInfoLength==gameInfo.size()&&std::equal(gameInfo.begin(),gameInfo.end(),scan[0].gameInfo.userGameInfo),"scan metadata changed");
    std::array<u8,6> oldMac{};std::copy(scan[0].bssid,scan[0].bssid+6,oldMac.begin());recipient=&b;CommunityWireless::Beacon(&q);recipient=nullptr;Require(b.beacons>0,"parent beacon callback missing");
    std::array<u8,24> ssid{};for(unsigned i=0;i<ssid.size();i++)ssid[i]=static_cast<u8>(i+1);Require(CommunityWireless::Join(&p,scan[0].bssid,ssid.data()),"actual WM join failed");CommunityWireless::RoleChange(&p,SIM_NET_MPSTATE_CHILD);
    Require(CommunityWireless::Aid(&p)==1&&CommunityWireless::Aid(&q)==0,"parent did not assign child AID");Wait([&]{return Completed(b,SIM_NET_MPPACKETCMD_JOIN_REQ,1,0,{ssid.begin(),ssid.end()});},"parent join callback missing");
    const auto* parentPlayers=CommunityWireless::Players(&q);Require(parentPlayers[0].aid==1&&parentPlayers[0].mpState==SIM_NET_MPSTATE_CHILD&&parentPlayers[1].aid==0,"peer/AID table incorrect");
    std::vector<u8> payload(512);for(unsigned i=0;i<payload.size();i++)payload[i]=static_cast<u8>(i*3);const auto expected=payload;
    {std::lock_guard<std::mutex> lock(b.mutex);b.holdData=true;}
    Require(CommunityWireless::Send(&p,1,7,payload.data(),static_cast<int>(payload.size())),"child MP data refused");std::fill(payload.begin(),payload.end(),0);
    Wait([&]{std::lock_guard<std::mutex> lock(b.mutex);return b.pendingValid&&b.pending.cmd==SIM_NET_MPPACKETCMD_DATA;},"data callback not held");
    Require(CommunityWireless::Aid(&q)==0&&CommunityWireless::Count(&q)==2,"callback wait held provider mutex");
    {std::unique_lock<std::mutex> lock(b.mutex);Require(b.pending.aid==1&&b.pending.port==7&&b.pending.dataLen==512&&std::equal(expected.begin(),expected.end(),b.pending.data),"owned callback data changed before ack");lock.unlock();std::this_thread::sleep_for(std::chrono::milliseconds(25));lock.lock();Require(std::equal(expected.begin(),expected.end(),b.pending.data),"callback storage expired while ARM9 acknowledgment delayed");b.ack=true;b.holdData=false;b.changed.notify_all();}
    Wait([&]{return Completed(b,SIM_NET_MPPACKETCMD_DATA,1,7,expected);},"acknowledged child data missing");const std::vector<u8> reply{8,6,7,5,3,0,9};Require(CommunityWireless::Send(&q,2,9,reply.data(),static_cast<int>(reply.size())),"parent MP data refused");Wait([&]{return Completed(a,SIM_NET_MPPACKETCMD_DATA,0,9,reply);},"parent data callback missing");
    u16 disconnected=0;Require(CommunityWireless::Disconnect(&p,1,&disconnected)&&disconnected==1,"child disconnect failed");Wait([&]{return Completed(b,SIM_NET_MPPACKETCMD_DISCONNECT,1,0,{});},"native disconnect callback missing");Wait([&]{return CommunityWireless::Scan(&p,scan)==1;},"disconnect removed still-advertising parent");Require(CommunityWireless::Join(&p,scan[0].bssid,ssid.data()),"rejoin to existing parent failed");
    const auto previous=p.online.ReadLink();
    // Accumulate two data events in one Process batch. The first callback owns
    // its bytes until ack; cancellation must discard the unindicated second one.
    pump.PauseSecond(true);{std::lock_guard<std::mutex> lock(b.mutex);b.holdData=true;}
    const std::vector<u8> inFlight{0xA1},stale{0xA2};Require(CommunityWireless::Send(&p,1,12,inFlight.data(),1)&&CommunityWireless::Send(&p,1,13,stale.data(),1),"teardown batch enqueue failed");std::this_thread::sleep_for(std::chrono::milliseconds(80));pump.PauseSecond(false);
    Wait([&]{std::lock_guard<std::mutex> lock(b.mutex);return b.pendingValid&&b.pending.cmd==SIM_NET_MPPACKETCMD_DATA&&b.pending.port==12;},"teardown batch did not hold first callback");
    Require(q.online.CancelLink(),"link cancel failed");Wait([&]{return p.online.ReadLink().state==CommunityOnline::LinkState::Ended;},"remote link cancellation missing");
    {std::lock_guard<std::mutex> lock(b.mutex);b.ack=true;b.holdData=false;b.changed.notify_all();}
    Wait([&]{return CommunityWireless::Count(&p)==0&&CommunityWireless::Count(&q)==0;},"teardown did not cancel wireless state");Require(!Completed(b,SIM_NET_MPPACKETCMD_DATA,1,13,stale),"queued old-epoch callback delivered after link cancellation");Wait([&]{return Completed(a,SIM_NET_MPPACKETCMD_DISCONNECT,0,0,{});},"teardown child callback absent");
    Prepare(p,q);Require(p.online.ReadLink().session!=previous.session&&p.online.ReadLink().epoch>previous.epoch,"fresh link reused session");Wait([&]{return CommunityWireless::Scan(&p,scan)==1;},"fresh parent not discovered");Require(CommunityWireless::Aid(&p)==0&&CommunityWireless::Players(&p)[0].mpState==SIM_NET_MPSTATE_OFF,"old association survived epoch");Require(!std::equal(oldMac.begin(),oldMac.end(),scan[0].bssid)&&!CommunityWireless::Join(&p,oldMac.data(),ssid.data()),"old epoch MAC accepted");Require(CommunityWireless::Join(&p,scan[0].bssid,ssid.data()),"fresh epoch WM join failed");
    const std::vector<u8> fresh{0xEE,0xCC,0x11};Require(CommunityWireless::Send(&q,2,4,fresh.data(),static_cast<int>(fresh.size())),"fresh parent send failed");Wait([&]{return Completed(a,SIM_NET_MPPACKETCMD_DATA,0,4,fresh);},"fresh epoch data callback absent");
    p.online.Disconnect();Wait([&]{return CommunityWireless::Count(&p)==0&&CommunityWireless::Count(&q)==0;},"lobby disconnect retained wireless state");q.online.Disconnect();
    {std::scoped_lock lock(a.mutex,b.mutex);Require(a.waits>0&&b.waits>0,"callback acknowledgment path untested");}std::cout<<"Actual provider: authenticated discovery, seat-independent parent/AID handshake, bidirectional MP data, owned callback bytes through delayed acknowledgment, disconnect/rejoin and fresh epoch passed. SDK/ARM9 callback recipient is a stub; gameplay and saves were not exercised.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
