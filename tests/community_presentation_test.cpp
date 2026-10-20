// SPDX-License-Identifier: GPL-3.0-or-later
// The actual Service and backend; all accounts and files are disposable.
#include "port/community_online.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace CommunityOnline;
static const char* stage="connect";
static void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static void Wait(Service& service,const std::function<bool(const Snapshot&)>& condition){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(std::chrono::steady_clock::now()<until){auto s=service.Read();if(s.state==State::Failed)throw std::runtime_error(s.error);if(condition(s))return;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    auto s=service.Read();throw std::runtime_error(std::string("presentation fixture timed out at ")+stage+" state="+StateName(s.state)+" trainer="+s.trainer.value.name+" revision="+std::to_string(s.trainer.value.revision)+" city="+CityStateName(s.city.state)+" peers="+std::to_string(s.city.players.size()));
}
static bool Has(const Snapshot& s,const std::string& text){for(const auto& m:s.messages)if(m.text==text)return true;return false;}
static const CityPlayer& Peer(const Snapshot& s,const std::string& player){for(const auto& p:s.city.players)if(p.player==player)return p;throw std::runtime_error("city peer missing");}
int main(int argc,char** argv){try{
    Require(argc==4,"Expected loopback URL, server pin and new private profile root");
    Options a;a.serverUrl=argv[1];a.serverKey=argv[2];a.profileDirectory=(std::filesystem::path(argv[3])/"host").string();a.username="ProfileHost";
    Options b=a;b.profileDirectory=(std::filesystem::path(argv[3])/"guest").string();b.username="ProfilePal";
    Service host,guest;Require(host.Connect(a)&&guest.Connect(b),"start failed");
    Wait(host,[](const Snapshot& s){return s.state==State::Ready&&s.trainer.available;});Wait(guest,[](const Snapshot& s){return s.state==State::Ready&&s.trainer.available;});
    stage="initial profile";
    auto original=host.Read().trainer.value;Require(original.revision==1,"unexpected initial profile revision");
    auto invalid=original;invalid.name="bad\nname";Require(!host.UpdateTrainer(invalid),"control name admitted");invalid.name="12345678901";Require(!host.UpdateTrainer(invalid),"over-limit name admitted");invalid.name="bad_";Require(!host.UpdateTrainer(invalid),"unsupported name admitted");invalid.name="Valid";invalid.revision=0;Require(!host.UpdateTrainer(invalid),"unversioned update admitted");
    stage="profile save";
    original.name="Own Host";original.look=255;original.outfit=65535;original.tutorialDone=true;
    Require(host.UpdateTrainer(original),"profile save rejected");Require(!host.UpdateTrainer(original)&&!host.RefreshTrainer(),"parallel profile request admitted");
    Wait(host,[&](const Snapshot& s){return !s.trainer.updating&&s.trainer.value.revision==original.revision+1;});
    Require(host.Read().trainer.value.look==255&&host.Read().trainer.value.outfit==65535&&host.Read().trainer.value.tutorialDone&&!host.Read().trainer.conflict,"profile fields changed");
    stage="conflict";
    auto stale=host.Read().trainer.value;Require(host.SendGlobalChat("fixture profile conflict"),"conflict trigger refused");Wait(host,[](const Snapshot& s){return Has(s,"fixture profile conflict");});
    stale.name="Stale";Require(host.UpdateTrainer(stale),"stale request not queued");Wait(host,[](const Snapshot& s){return s.trainer.conflict&&!s.trainer.updating;});
    Require(host.Read().trainer.value.name=="OtherHost"&&host.Read().trainer.value.revision==stale.revision+1,"conflict did not return authoritative profile");
    stage="refresh";
    Require(host.RefreshTrainer(),"refresh rejected");Wait(host,[](const Snapshot& s){return !s.trainer.conflict;});
    stage="party";
    Require(host.CreateParty(),"party create failed");Wait(host,[](const Snapshot& s){return !s.partyCode.empty();});Require(guest.JoinParty(host.Read().partyCode),"join failed");Wait(host,[](const Snapshot& s){return s.members.size()==2;});
    stage="city join";
    Require(host.EnterCity()&&guest.EnterCity(),"city enter rejected");Wait(host,[](const Snapshot& s){return s.city.state==CityState::Present&&s.city.players.size()==1;});Wait(guest,[](const Snapshot& s){return s.city.state==CityState::Present&&s.city.players.size()==1;});
    const auto hostId=host.Read().player,guestId=guest.Read().player;const auto slot=Peer(host.Read(),guestId).slot;
    Require(host.Read().city.generation==host.Read().connections&&!host.EnterCity(),"city generation/duplicate entry wrong");
    stage="city status";
    Require(guest.SetCityStatus("away")&&!guest.SetCityStatus("spoof"),"status validation failed");Wait(host,[&](const Snapshot& s){return Peer(s,guestId).status=="away";});
    stage="appearance refresh";
    auto changed=guest.Read().trainer.value;changed.name="New Pal";changed.look=42;changed.outfit=4096;Require(guest.UpdateTrainer(changed),"guest profile update rejected");
    Wait(host,[&](const Snapshot& s){return Peer(s,guestId).name=="New Pal";});const auto updated=Peer(host.Read(),guestId);
    Require(updated.slot==slot&&updated.status=="away"&&updated.appearance.look==42&&updated.appearance.outfit==4096&&host.Read().city.players.size()==1,"same-slot refresh lost identity/status/appearance");
    Wait(host,[&](const Snapshot& s){for(const auto& m:s.members)if(m.player==guestId)return m.name=="New Pal"&&m.appearance.look==42&&m.appearance.outfit==4096;return false;});
    stage="art and opaque pose";
    Require(guest.SendGlobalChat("fixture selected art and pose"),"art trigger refused");Wait(host,[&](const Snapshot& s){return Peer(s,guestId).appearance.hasArt&&Has(s,"fixture selected art and pose");});
    Require(Peer(host.Read(),guestId).appearance.art.creation=="fixture-art"&&Peer(host.Read(),guestId).slot==slot,"art refresh changed actor");
    // The backend allows two authenticated sockets for one player. Identity is
    // (connection generation, slot), so their presence must not collapse.
    stage="same-player distinct slots";
    Options duplicate=b;duplicate.profileDirectory=(std::filesystem::path(argv[3])/"mirror").string();std::filesystem::create_directories(duplicate.profileDirectory);
    std::filesystem::copy_file(std::filesystem::path(b.profileDirectory)/"community-online.json",std::filesystem::path(duplicate.profileDirectory)/"community-online.json");
    Service mirror;Require(mirror.Connect(duplicate),"mirror connection refused");Wait(mirror,[&](const Snapshot& s){return s.state==State::Ready&&s.player==guestId&&s.trainer.available;});Require(mirror.EnterCity(),"mirror entry refused");
    Wait(host,[](const Snapshot& s){return s.city.players.size()==2;});auto duplicates=host.Read().city.players;Require(duplicates[0].player==guestId&&duplicates[1].player==guestId&&duplicates[0].slot!=duplicates[1].slot,"same-player sockets collapsed");
    Require(mirror.SetCityStatus("menu"),"mirror status refused");Wait(host,[](const Snapshot& s){unsigned away=0,menu=0;for(const auto& p:s.city.players){away+=p.status=="away";menu+=p.status=="menu";}return away==1&&menu==1;});
    Require(mirror.LeaveCity(),"mirror leave refused");Wait(host,[](const Snapshot& s){return s.city.players.size()==1;});mirror.Disconnect();
    stage="party plus leave";
    // Party traffic does not gate city exit or alter the caller's game location.
    Require(host.CreateParty(),"party action failed");unsigned admitted=0;for(unsigned i=0;i<96;i++)admitted+=host.SetCityStatus("menu");Require(admitted>0,"status queue probe admitted nothing");
    Require(host.LeaveCity(),"leave during queued activity failed");Require(host.Read().city.players.empty()&&!host.EnterCity()&&!host.SetCityStatus("active"),"leaving exposed stale actors or allowed reentry/status");Wait(host,[](const Snapshot& s){return s.city.state==CityState::Absent;});Wait(guest,[](const Snapshot& s){return s.city.players.empty();});
    stage="cancel entry";
    Require(host.EnterCity()&&host.LeaveCity(),"pending enter cancellation rejected");Require(!host.EnterCity()&&host.Read().city.state==CityState::Leaving,"cancel gate absent");Wait(host,[](const Snapshot& s){return s.city.state==CityState::Absent;});
    stage="reentry";
    Require(host.EnterCity(),"fresh enter refused");Wait(host,[](const Snapshot& s){return s.city.state==CityState::Present&&s.city.players.size()==1;});
    stage="reconnect";
    std::this_thread::sleep_for(std::chrono::milliseconds(800)); // Respect actual backend chat cooldown.
    const auto generation=host.Read().city.generation;Require(host.SendGlobalChat("fixture reconnect city"),"reconnect trigger refused");Wait(host,[](const Snapshot& s){return s.state==State::Reconnecting;});Require(host.Read().city.state==CityState::Absent&&host.Read().city.players.empty()&&!host.Read().trainer.available,"disconnected presentation retained");
    Wait(host,[&](const Snapshot& s){return s.state==State::Ready&&s.connections>generation&&s.trainer.available;});Require(host.Read().city.state==CityState::Absent,"city reentered without explicit consent");Require(host.EnterCity(),"new generation enter refused");Wait(host,[&](const Snapshot& s){return s.city.state==CityState::Present&&s.city.generation>generation;});
    stage="confirmed absent trainer creation";
    Require(host.SendGlobalChat("fixture remove trainer"),"absent profile trigger refused");Wait(host,[](const Snapshot& s){return Has(s,"fixture remove trainer");});Require(host.RefreshTrainer(),"absent profile refresh refused");Wait(host,[](const Snapshot& s){return s.trainer.loaded&&!s.trainer.available;});
    Trainer creation;creation.name="Restored";creation.look=12;creation.outfit=1234;Require(host.UpdateTrainer(creation),"confirmed absent trainer creation refused");Wait(host,[](const Snapshot& s){return s.trainer.available&&!s.trainer.updating&&s.trainer.value.revision==1;});Require(host.Read().trainer.value.name=="Restored"&&!host.Read().trainer.conflict,"created trainer fields changed");
    stage="malformed appearance";
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    Require(host.SendGlobalChat("fixture invalid appearance"),"malformed event trigger refused");
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(host.Read().state!=State::Failed&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(host.Read().state==State::Failed&&host.Read().error=="bad_appearance"&&host.Read().city.players.empty()&&host.Read().city.state==CityState::Absent,"malformed appearance was retained");
    host.Disconnect();guest.Disconnect();Require(guest.Read().city.state==CityState::Absent&&guest.Read().city.players.empty(),"explicit disconnect retained presence");
    std::cout<<"Actual native presentation Service passed profile validation/revision conflict, party appearance, city lifecycle/cancellation/generation, authenticated art metadata, opaque fragmented pose and malformed-event cleanup.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
