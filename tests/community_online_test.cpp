// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises the actual native adapter, never a handshake/protocol replacement.
#include "port/community_online.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace CommunityOnline;
static void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static void Wait(Service& service,const std::function<bool(const Snapshot&)>& condition){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(25);while(std::chrono::steady_clock::now()<end){auto s=service.Read();if(s.state==State::Failed)throw std::runtime_error("adapter: "+s.error);if(condition(s))return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("native adapter condition timed out");}
static bool Has(const Snapshot& s,const std::string& text){for(const auto& m:s.messages)if(m.text==text)return true;return false;}
static void WaitLink(Service& service,const std::function<bool(const LinkSnapshot&)>& condition){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(std::chrono::steady_clock::now()<end){auto s=service.ReadLink();if(s.state==LinkState::Failed)throw std::runtime_error("link: "+s.error);if(condition(s))return;std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("native link condition timed out");}
static LinkPacket Packet(Service& service){LinkPacket packet;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<end){if(service.ReceiveLink(packet))return packet;Require(service.ReadLink().state==LinkState::Active,"link ended before packet");std::this_thread::sleep_for(std::chrono::milliseconds(5));}throw std::runtime_error("native link packet timed out");}
static void Failed(Service& service,const std::string& code){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(std::chrono::steady_clock::now()<end){auto s=service.Read();if(s.state==State::Failed){Require(s.error==code,"unexpected failure code");return;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("expected rejection timed out");}
int main(int argc,char** argv){try{
    Require(argc==4||argc==5,"Expected loopback URL, trusted public key and new private profile root");
    if(argc==5){Require(std::string(argv[4])=="--reject-untrusted-tls","Unknown fixture mode");Options tls;tls.serverUrl=argv[1];tls.serverKey=argv[2];tls.profileDirectory=argv[3];tls.username="TlsFixture";tls.reconnect=false;Service service;Require(service.Connect(tls),"TLS negative test did not start");Failed(service,"http_transport_failed");Require(!std::filesystem::exists(std::filesystem::path(tls.profileDirectory)/"community-online.json"),"Untrusted TLS generated a profile");std::cout<<"Native adapter refused untrusted TLS before profile/account creation.\n";return 0;}
    Options a;a.serverUrl=argv[1];a.serverKey=argv[2];a.profileDirectory=(std::filesystem::path(argv[3])/"host").string();a.username="OwnHost";a.pingIntervalMs=1000;a.pongTimeoutMs=1000;
    Options b=a;b.profileDirectory=(std::filesystem::path(argv[3])/"guest").string();b.username="OwnGuest";
    Service host,guest;host.Connect(a);guest.Connect(b);
    Wait(host,[](const Snapshot& s){return s.state==State::Ready;});Wait(guest,[](const Snapshot& s){return s.state==State::Ready;});
    auto firstPlayer=guest.Read().player;Require(!firstPlayer.empty()&&firstPlayer!=host.Read().player,"distinct signed player identities required");
    Require(host.CreateParty(),"create party rejected");Wait(host,[](const Snapshot& s){return s.partyCode.size()==6;});
    Require(!guest.JoinParty("BAD!"),"invalid party code accepted");Require(guest.JoinParty(host.Read().partyCode),"join party rejected");
    Wait(host,[](const Snapshot& s){return s.members.size()==2;});Wait(guest,[&](const Snapshot& s){return s.members.size()==2&&s.partyId==host.Read().partyId;});
    const auto prepare=[&](bool open=true){Require(host.PrepareLink(std::string(64,'a'),std::string(64,'b')),"host prepare rejected");Require(guest.PrepareLink(std::string(64,'a'),std::string(64,'b')),"guest prepare rejected");WaitLink(host,[](const LinkSnapshot& s){return s.state==LinkState::Prepared;});WaitLink(guest,[](const LinkSnapshot& s){return s.state==LinkState::Prepared;});if(open){Require(host.OpenLink(),"open link rejected");WaitLink(host,[](const LinkSnapshot& s){return s.state==LinkState::Active;});WaitLink(guest,[](const LinkSnapshot& s){return s.state==LinkState::Active;});}};
    Require(host.ReadLink().available&&guest.ReadLink().available,"link feature absent");Require(!host.PrepareLink("bad",std::string(64,'a')),"invalid fingerprint accepted");Require(!host.SendLink(255,{1}),"pre-active send accepted");
    prepare(false);Require(guest.CancelLink()&&guest.ReadLink().cancelling,"prepared cancellation did not await acknowledgment");Require(!guest.PrepareLink(std::string(64,'a'),std::string(64,'b')),"prepare overtook cancellation acknowledgment");Wait(guest,[&](const Snapshot&){return !guest.ReadLink().cancelling;});
    Require(host.OpenLink(),"leader negative open did not queue");Wait(host,[&](const Snapshot&){return host.ReadLink().state==LinkState::Failed;});Require(host.ReadLink().error=="link_incompatible_member"&&guest.ReadLink().state==LinkState::Ended,"canceled guest consent remained on backend");
    // A queued prepare is acknowledged before its following cancellation. A
    // fresh fingerprint must wait until that cancellation's own acknowledgment.
    Require(guest.PrepareLink(std::string(64,'c'),std::string(64,'d'))&&guest.CancelLink(),"pending prepare cancellation failed");Require(!guest.PrepareLink(std::string(64,'a'),std::string(64,'b')),"pending-prepare cancellation gate absent");Wait(guest,[&](const Snapshot&){return !guest.ReadLink().cancelling;});
    Require(host.CancelLink(),"failed allocation cleanup refused");Wait(host,[&](const Snapshot&){return !host.ReadLink().cancelling;});prepare();
    auto link=host.ReadLink(),other=guest.ReadLink();Require(link.session==other.session&&link.epoch==other.epoch&&link.seat==0&&other.seat==1&&link.roster.size()==2&&link.roster[1].mac.size()==12,"link roster mismatch");
    Require(host.SendLink(255,{0,1,2,3}),"link broadcast rejected");auto received=Packet(guest);Require(received.sender==0&&received.session==link.session&&received.epoch==link.epoch&&received.payload==std::vector<std::uint8_t>({0,1,2,3}),"authoritative broadcast sender mismatch");
    Require(guest.SendLink(0,{9,8,7}),"link direct send rejected");received=Packet(host);Require(received.sender==1&&received.payload==std::vector<std::uint8_t>({9,8,7}),"authoritative direct sender mismatch");
    std::vector<std::uint8_t> large(8192);for(std::size_t i=0;i<large.size();i++)large[i]=static_cast<std::uint8_t>(i);Require(host.SendLink(1,large),"maximum payload rejected");Require(Packet(guest).payload==large,"large fragmented binary mismatch");
    unsigned queued=0,rejected=0;for(unsigned i=0;i<128;i++){if(host.SendLink(1,large))queued++;else rejected++;}Require(queued>0&&rejected>0&&queued<128,"outbound byte bound was not enforced");for(unsigned i=0;i<queued;i++)Require(Packet(guest).payload==large,"bounded FIFO payload changed");
    for(std::uint8_t i=0;i<20;i++)Require(guest.SendLink(0,{i}),"ordered enqueue rejected");
    for(std::uint8_t i=0;i<20;i++)Require(Packet(host).payload==std::vector<std::uint8_t>({i}),"binary FIFO order changed");
    Require(!host.SendLink(0,{1})&&!host.SendLink(3,{1})&&!host.SendLink(1,{})&&!host.SendLink(1,std::vector<std::uint8_t>(8193)),"invalid binary accepted");
    Require(guest.CancelLink(),"cancel link rejected");WaitLink(host,[](const LinkSnapshot& s){return s.state==LinkState::Ended;});Require(!host.SendLink(1,{1})&&!guest.ReceiveLink(received),"ended session retained frames");
    // Both workers must release the previous sockets before explicit new setup.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));prepare(false);Require(host.OpenLink()&&host.CancelLink(),"immediate open cancellation rejected");WaitLink(guest,[](const LinkSnapshot& s){return s.state==LinkState::Ended;});Require(host.ReadLink().state==LinkState::Ended,"canceled offer reopened locally");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));prepare();Require(host.ReadLink().session!=link.session&&host.ReadLink().epoch>link.epoch,"new link reused generation");
    Require(host.SendGlobalChat("own-client host native message"),"host send rejected");Wait(guest,[](const Snapshot& s){return Has(s,"own-client host native message");});
    Require(guest.SendGlobalChat("own-client guest native reply"),"guest send rejected");Wait(host,[](const Snapshot& s){return Has(s,"own-client guest native reply");});
    Wait(host,[](const Snapshot& s){return s.state==State::Ready&&s.connections>=2&&s.members.size()==2;});
    WaitLink(guest,[](const LinkSnapshot& s){return s.state==LinkState::Ended;});Require(host.ReadLink().state==LinkState::Ended&&!host.SendLink(1,{1})&&!guest.ReceiveLink(received),"lobby reconnect resumed old link");
    Require(host.SendGlobalChat("fixture liveness probe"),"liveness probe rejected");
    Wait(host,[](const Snapshot& s){return s.state==State::Reconnecting&&s.error=="lobby_pong_timeout";});
    Wait(host,[](const Snapshot& s){return s.state==State::Ready&&s.connections>=3&&s.members.size()==2;});
    Require(!host.SendGlobalChat(std::string(481,'x'))&&!host.SendGlobalChat("bad\ncontrol"),"invalid chat accepted");
    Require(!host.SendGlobalChat(std::string("\xC3\x28",2)),"invalid UTF-8 chat accepted");
    Service locked;locked.Connect(a);Failed(locked,"profile_in_use");locked.Disconnect();
    guest.Disconnect();Wait(guest,[](const Snapshot& s){return s.state==State::Stopped;});Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==firstPlayer)return !m.online;return false;});
    guest.Connect(b);Wait(guest,[&](const Snapshot& s){return s.state==State::Ready&&s.player==firstPlayer&&s.members.size()==2;});
    Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==firstPlayer)return m.online;return false;});
    Require(guest.LeaveParty(),"leave party rejected");Wait(guest,[](const Snapshot& s){return s.partyId.empty();});Wait(host,[](const Snapshot& s){return s.members.size()==1;});
    std::string unicode;for(unsigned i=0;i<160;++i)unicode+="水";
    Require(unicode.size()==480&&guest.SendGlobalChat(unicode),"valid 480-byte UTF-8 chat rejected");Wait(host,[&](const Snapshot& s){return Has(s,unicode);});
    Require(!guest.SendGlobalChat(unicode+"水"),"over-limit UTF-8 chat accepted");
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    Require(guest.SendGlobalChat("fixture deep JSON probe"),"deep JSON probe rejected");Failed(guest,"json_too_deep");
    Options wrong=a;wrong.serverKey="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";wrong.profileDirectory=(std::filesystem::path(argv[3])/"wrong-pin").string();
    Service refused;refused.Connect(wrong);Failed(refused,"server_identity_changed");refused.Disconnect();
    Require(!std::filesystem::exists(std::filesystem::path(wrong.profileDirectory)/"community-online.json"),"wrong server created a key profile");
    for(const auto& settings:{a,b}){std::ifstream file(std::filesystem::path(settings.profileDirectory)/"community-online.json");std::string profile((std::istreambuf_iterator<char>(file)),{});Require(profile.find("seedProtected")!=std::string::npos&&profile.find("\"seed\":")==std::string::npos&&profile.find("\"token\":")==std::string::npos,"profile secret/token storage regression");}
    guest.Disconnect();host.Disconnect();Wait(host,[](const Snapshot& s){return s.state==State::Stopped;});Wait(guest,[](const Snapshot& s){return s.state==State::Stopped;});
    Options damaged=a;damaged.profileDirectory=(std::filesystem::path(argv[3])/"damaged").string();std::filesystem::create_directory(damaged.profileDirectory);
    std::ifstream source(std::filesystem::path(a.profileDirectory)/"community-online.json");std::string saved((std::istreambuf_iterator<char>(source)),{});const std::string marker="\"seedProtected\":\"";auto position=saved.find(marker);Require(position!=std::string::npos,"missing protected seed");position+=marker.size();saved[position]=saved[position]=='A'?'B':'A';
    {std::ofstream file(std::filesystem::path(damaged.profileDirectory)/"community-online.json");file<<saved;}
    Service corrupt;corrupt.Connect(damaged);Failed(corrupt,"profile_protection_failed");corrupt.Disconnect();
    Options pending=a;pending.reconnect=false;pending.username="FixtureOwner";pending.profileDirectory=(std::filesystem::path(argv[3])/"pending-name").string();
    Service recovery;Require(recovery.Connect(pending),"pending registration did not start");Failed(recovery,"username_taken");
    auto publicKey=[&](){std::ifstream file(std::filesystem::path(pending.profileDirectory)/"community-online.json");std::string text((std::istreambuf_iterator<char>(file)),{});const std::string marker="\"publicKey\":\"";auto p=text.find(marker);Require(p!=std::string::npos,"missing staged public key");p+=marker.size();return text.substr(p,text.find('"',p)-p);};
    auto staged=publicKey();pending.username="OwnRecovered";
    bool started=false;for(unsigned i=0;i<100&&!started;++i){started=recovery.Connect(pending);if(!started)std::this_thread::sleep_for(std::chrono::milliseconds(10));}Require(started,"pending recovery worker busy");
    Wait(recovery,[](const Snapshot& s){return s.state==State::Ready&&s.username=="OwnRecovered";});Require(publicKey()==staged,"pending recovery regenerated the account key");recovery.Disconnect();
    std::cout<<"Native adapter: signed auth/renewal, party/chat, signed link lifecycle and binary routing, reconnect and pong timeout, UTF-8 limits, deep-JSON refusal, protected profiles, pin/secret/lock refusal and same-key pending-name recovery passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
