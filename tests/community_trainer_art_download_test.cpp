// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_trainer_art_download.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <curl/curl.h>
using namespace CommunityOnline;
using CommunityTrainerArt::DownloadResult;
static void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static void Wait(Service& service,const std::function<bool(const Snapshot&)>& predicate){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(std::chrono::steady_clock::now()<end){const auto s=service.Read();Check(s.state!=State::Failed,s.error.c_str());if(predicate(s))return;std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("art fixture wait timeout");}
static bool Has(const Snapshot& s,const std::string& text){for(const auto& m:s.messages)if(m.text==text)return true;return false;}
static void Mode(Service& service,const std::string& mode){std::this_thread::sleep_for(std::chrono::milliseconds(800));Check(service.SendGlobalChat("art fixture "+mode),"mode queue");Wait(service,[&](const Snapshot& s){return Has(s,"art fixture "+mode);});}
static ArtReference Art(Service& service,const std::string& player){for(const auto& m:service.Read().members)if(m.player==player&&m.appearance.hasArt)return m.appearance.art;throw std::runtime_error("missing art reference");}
static DownloadResult Result(Service& service,std::uint64_t id){DownloadResult r;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(std::chrono::steady_clock::now()<end){if(service.ReceiveTrainerArt(r)){Check(r.request.id==id,"wrong request completion");return r;}Check(service.Read().state==State::Ready,"download disrupted lobby");Check(service.ReadLink().state==LinkState::Active,"download disrupted link");std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("art download timeout");}
int main(int argc,char** argv){try{
    if(argc==5&&std::string(argv[4])=="--untrusted-art-tls"){
        Check(curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK,"curl init");CommunityTrainerArt::Downloads downloads;CommunityTrainerArt::Request request;
        request.id=request.connection=request.epoch=1;request.session="12345678-1234-1234-1234-123456789abc";request.player="1";request.art={"fixture","v1","platinum-trainer-209-v1",std::string(64,'a')};
        Check(downloads.Queue(request,argv[1]),"TLS download queue");DownloadResult result;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!downloads.Poll(result)){Check(std::chrono::steady_clock::now()<end,"TLS refusal timeout");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        Check(!result.pack&&result.error=="art_transport_failed","untrusted TLS accepted");std::cout<<"Art worker refused untrusted TLS before HTTP PASS\n";return 0;
    }
    Check(argc==4,"Expected loopback server, pin and disposable profiles");Options a;a.serverUrl=argv[1];a.serverKey=argv[2];a.username="ArtHost";a.profileDirectory=(std::filesystem::path(argv[3])/"host").string();a.pingIntervalMs=a.pongTimeoutMs=1000;
    Options b=a;b.username="ArtGuest";b.profileDirectory=(std::filesystem::path(argv[3])/"guest").string();Service host,guest;Check(host.Connect(a)&&guest.Connect(b),"connect");
    Wait(host,[](const Snapshot& s){return s.state==State::Ready&&s.trainer.available;});Wait(guest,[](const Snapshot& s){return s.state==State::Ready&&s.trainer.available;});
    Check(host.CreateParty(),"create");Wait(host,[](const Snapshot& s){return !s.partyCode.empty();});Check(guest.JoinParty(host.Read().partyCode),"join");Wait(host,[](const Snapshot& s){return s.members.size()==2;});Wait(guest,[](const Snapshot& s){return s.members.size()==2;});
    const auto player=guest.Read().player;Mode(guest,"valid 1");Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==player&&m.appearance.hasArt)return true;return false;});
    Check(!host.RequestTrainerArt(player,Art(host,player)),"pre-active art admitted");
    Check(host.PrepareLink(std::string(64,'a'),std::string(64,'b'))&&guest.PrepareLink(std::string(64,'a'),std::string(64,'b')),"prepare");
    Wait(host,[&](const Snapshot&){return host.ReadLink().state==LinkState::Prepared;});Wait(guest,[&](const Snapshot&){return guest.ReadLink().state==LinkState::Prepared;});Check(host.OpenLink(),"open");Wait(host,[&](const Snapshot&){return host.ReadLink().state==LinkState::Active;});Wait(guest,[&](const Snapshot&){return guest.ReadLink().state==LinkState::Active;});
    auto ref=Art(host,player);auto id=host.RequestTrainerArt(player,ref);Check(id!=0,"valid request");auto result=Result(host,id);Check(result.pack!=nullptr&&result.error.empty(),"valid decode");Check(result.request.player==player&&result.request.connection==host.Read().connections&&result.request.session==host.ReadLink().session&&result.request.epoch==host.ReadLink().epoch,"context binding");
    CommunityTrainerArt::RgbaFrame frame;Check(result.pack->FieldFrame(0,frame)&&frame[0]==23&&frame[3]==255,"decoded native pixels");
    auto bad=ref;bad.hash=std::string(64,'a');Check(!host.RequestTrainerArt(player,bad)&&!host.RequestTrainerArt("999999",ref),"unbound reference admitted");
    Mode(guest,"slow 2");id=host.RequestTrainerArt(player,ref);Check(id!=0,"slow request");Check(host.SendLink(1,{7,8,9}),"link admission during HTTP");LinkPacket packet;Wait(guest,[&](const Snapshot&){return guest.ReceiveLink(packet);});Check(packet.payload==std::vector<std::uint8_t>({7,8,9}),"link packet changed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1400));Check(host.Read().state==State::Ready&&host.Read().connections==1&&host.ReadLink().state==LinkState::Active,"HTTP blocked ping/link");result=Result(host,id);Check(result.pack!=nullptr,"slow result");
    for(const auto& mode:std::vector<std::pair<std::string,std::string>>{{"corrupt 3","art_hash_mismatch"},{"huge 4","art_body_too_large"},{"chunked 5","art_body_too_large"},{"redirect 6","art_http_refused"}}){Mode(guest,mode.first);id=host.RequestTrainerArt(player,ref);Check(id!=0,"negative request");result=Result(host,id);Check(!result.pack&&result.error==mode.second,mode.second.c_str());}
    Mode(guest,"invalid 7");Wait(host,[&](const Snapshot&){return Art(host,player).hash!=ref.hash;});id=host.RequestTrainerArt(player,Art(host,player));Check(id!=0,"invalid semantic request");result=Result(host,id);Check(!result.pack&&result.error=="art_pack_pixels","structural dummy accepted as pixels");
    Mode(guest,"stall 11");Wait(host,[&](const Snapshot&){return Art(host,player).hash==ref.hash;});id=host.RequestTrainerArt(player,ref);Check(id!=0,"stalled request");result=Result(host,id);Check(!result.pack&&result.error=="art_transport_failed","HTTP deadline not enforced");
    Mode(guest,"slow 8");Wait(host,[&](const Snapshot&){return Art(host,player).hash==ref.hash;});std::vector<std::uint64_t> requests;for(unsigned i=0;i<8;i++){id=host.RequestTrainerArt(player,ref);Check(id!=0,"bounded eight jobs");requests.push_back(id);}Check(!host.RequestTrainerArt(player,ref),"ninth job admitted");for(auto request:requests)Check(host.CancelTrainerArt(request),"cancel job");Check(!host.CancelTrainerArt(requests[0]),"duplicate cancellation");
    id=host.RequestTrainerArt(player,ref);Check(id!=0,"request after cancel");Mode(guest,"clear 9");Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==player)return !m.appearance.hasArt;return false;});std::this_thread::sleep_for(std::chrono::milliseconds(1500));Check(!host.ReceiveTrainerArt(result)&&!host.CancelTrainerArt(id),"old art reappeared after profile change");
    Mode(guest,"slow 10");Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==player)return m.appearance.hasArt;return false;});id=host.RequestTrainerArt(player,ref);Check(id!=0,"shutdown pending request");const auto begin=std::chrono::steady_clock::now();host.Disconnect();Check(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(250),"Disconnect blocked on HTTP");Check(!host.ReceiveTrainerArt(result),"shutdown published stale art");guest.Disconnect();
    std::cout<<"Real backend art: signed roster/reference binding, owned public fixed-hash download, SHA/semantic/size/redirect rejection, independent ping/link progress, bounded jobs, art-change and shutdown cancellation PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
