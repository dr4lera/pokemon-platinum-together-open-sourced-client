// SPDX-License-Identifier: GPL-3.0-or-later
// Include the actual implementation in this isolated TU to exercise the
// receive-handler/Disconnect synchronization deterministically, without adding
// product test hooks or opening sockets/profiles.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "port/community_online.h"
#undef private
#include "../src/port/community_online.cpp"
#include <iostream>
using namespace CommunityOnline;
static void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    Service service;auto& impl=*service.impl;
    const auto trainer=Parse(R"({"t":"trainer","applied":true,"trainer":{"name":"Fixture","look":42,"outfit":65535,"tutorial_done":true,"revision":2}})");
    // A receive thread that has parsed the response but has not acquired the
    // publication lock must observe a concurrent stop and keep cleared state.
    std::unique_lock<std::mutex> lock(impl.mutex);
    impl.snapshot.state=State::Ready;impl.snapshot.trainer.updating=true;
    std::thread receive([&]{impl.TrainerMessage(trainer);});
    impl.stop=true;impl.snapshot.state=State::Stopping;impl.ClearPresentationLocked();
    lock.unlock();receive.join();
    Check(!service.Read().trainer.available&&!service.Read().trainer.updating,"stopped handler republished trainer");
    // The earlier stop=true / EndLink / snapshot-clear interval also must not
    // accept city events, even if the previous state still says Ready.
    impl.snapshot.state=State::Ready;impl.snapshot.city.state=CityState::Entering;
    auto joined=Parse(R"({"t":"city.joined","district":"plaza","instance":"private-plaza","cap":64,"slot":0,"players":[{"player":"2","name":"Peer","slot":1,"status":"active","look":0,"outfit":0,"art":null}]})");
    impl.CityMessage(joined,"city.joined");Check(service.Read().city.state==CityState::Entering&&service.Read().city.players.empty(),"stopped city join was published");
    impl.snapshot.city.state=CityState::Present;auto added=Parse(R"({"t":"city.add","player":"2","name":"Peer","slot":1,"status":"away","look":255,"outfit":65535,"art":null})");
    impl.CityMessage(added,"city.add");Check(service.Read().city.players.empty(),"stopped city add was published");
    impl.stop=false;impl.ClearPresentationLocked();
    auto absent=Parse(R"({"t":"trainer","applied":false,"trainer":null})");impl.TrainerMessage(absent);Check(impl.snapshot.trainer.loaded&&!impl.snapshot.trainer.available,"null trainer was not distinguished from loading");
    Trainer create;create.name="NewProfile";Check(service.UpdateTrainer(create),"revision-zero creation refused for confirmed absence");Check(Number(Parse(impl.commands.back()),"revision")==0,"create revision changed");
    impl.TrainerMessage(trainer);Check(!service.UpdateTrainer(create),"revision-zero overwrite admitted for existing profile");impl.ClearPresentationLocked();impl.commands.clear();
    impl.snapshot.city.state=CityState::Leaving;impl.requestCityLeave=true;
    impl.PresentationErrorLocked("city_full");Check(impl.snapshot.city.state==CityState::Leaving&&impl.requestCityLeave&&impl.snapshot.city.error=="city_full","failed enter released pending leave gate");
    Check(!service.EnterCity(),"entry overtook old leave acknowledgement");
    auto left=Parse(R"({"t":"city.left","reason":"left"})");impl.CityMessage(left,"city.left");Check(service.EnterCity(),"acknowledged cancel did not allow fresh enter");
    impl.PresentationErrorLocked("city_full");Check(impl.snapshot.city.state==CityState::Absent&&!impl.snapshot.city.error.empty(),"failed ordinary enter remained pending");
    impl.snapshot.city.state=CityState::Present;impl.PresentationErrorLocked("trainer_required");Check(impl.snapshot.city.state==CityState::Present,"unrelated party error removed city presence");
    impl.snapshot.city.state=CityState::Leaving;impl.requestCityLeave=true;impl.PresentationErrorLocked("inverna_server_required");Check(impl.snapshot.city.state==CityState::Absent&&!impl.requestCityLeave,"unsupported city server left cancellation pending");
    std::cout<<"Actual presentation handlers passed deterministic stop/publication and enter-error/cancel ordering tests.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
