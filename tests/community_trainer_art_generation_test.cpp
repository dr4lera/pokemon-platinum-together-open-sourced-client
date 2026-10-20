// SPDX-License-Identifier: GPL-3.0-or-later
// Actual Service + content worker publication logic; no network/profile/save.
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#define private public
#include "port/community_online.h"
#include "port/community_trainer_art_download.h"
#undef private
#include "../src/port/community_trainer_art_download.cpp"
#include "../src/port/community_online.cpp"
#include <cassert>
#include <cstdio>
using namespace CommunityOnline;
using CommunityTrainerArt::DownloadResult;
int main()
{
    Service service;auto& i=*service.impl;
    const ArtReference art{"synthetic","v1","platinum-trainer-209-v1",std::string(64,'a')};
    auto setup=[&]{
        i.ClearArtLocked();i.stop=false;i.linkStop=false;i.linkCancelPending=false;
        i.snapshot=Snapshot{};i.snapshot.state=State::Ready;i.snapshot.connections=3;i.snapshot.player="10";i.snapshot.partyId="12345678-1234-1234-1234-123456789abc";
        i.snapshot.members={{"10","Host",true,{}},{"20","Peer",true,{}}};i.snapshot.members[1].appearance.hasArt=true;i.snapshot.members[1].appearance.art=art;
        i.link=LinkSnapshot{};i.link.state=LinkState::Active;i.link.session="abcdef12-abcd-abcd-abcd-abcdef123456";i.link.party=i.snapshot.partyId;i.link.epoch=9;i.link.expiresAtMs=Now()+600000;i.link.roster={{0,"10","020000000001"},{1,"20","020000000002"}};
    };
    std::uint64_t next=0;
    auto inject=[&]{
        auto job=std::make_shared<CommunityTrainerArt::Downloads::Impl::Job>();job->request.id=++next;job->request.connection=i.snapshot.connections;job->request.epoch=i.link.epoch;job->request.session=i.link.session;job->request.player="20";job->request.art=art;
        job->result.request=job->request;job->result.pack=std::make_shared<CommunityTrainerArt::Pack>();
        i.artRequests.emplace(next,job->request);auto& d=*i.artDownloads.impl;{std::lock_guard<std::mutex> lock(d.mutex);d.jobs.emplace(next,job);d.completed.push_back(job);}return job;
    };
    DownloadResult result;setup();auto job=inject();assert(service.ReceiveTrainerArt(result)&&result.request.id==job->request.id&&result.pack);assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.snapshot.connections++;assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.link.epoch++;assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.link.session[0]='0';assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.snapshot.members[1].appearance.art.hash[0]='b';assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.snapshot.members[1].online=false;assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.link.expiresAtMs=Now()-1;assert(!service.ReceiveTrainerArt(result));
    setup();inject();i.linkCancelPending=true;assert(!service.ReceiveTrainerArt(result));
    setup();job=inject();assert(service.CancelTrainerArt(job->request.id)&&job->cancelled&&!service.ReceiveTrainerArt(result));
    setup();job=inject();i.EndLink("fixture");assert(job->cancelled&&!service.ReceiveTrainerArt(result)&&i.artRequests.empty());
    setup();job=inject();i.snapshot.members[1].appearance.hasArt=false;i.PruneArtLocked();assert(job->cancelled&&i.artRequests.empty()&&!service.ReceiveTrainerArt(result));
    setup();job=inject();bool published=true;std::unique_lock<std::mutex> lock(i.mutex);
    std::thread receiver([&]{DownloadResult candidate;published=service.ReceiveTrainerArt(candidate);});
    for(unsigned n=0;n<1000;n++){bool empty;{std::lock_guard<std::mutex> inner(i.artDownloads.impl->mutex);empty=i.artDownloads.impl->completed.empty();}if(empty)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    i.stop=true;i.snapshot.state=State::Stopping;lock.unlock();receiver.join();assert(!published);i.ClearArtLocked();
    setup();job=inject();service.Disconnect();assert(job->cancelled&&i.artRequests.empty()&&!service.ReceiveTrainerArt(result));
    // A late parsed party event must not republish membership after Disconnect.
    const auto party=Parse(R"({"party":{"id":"12345678-1234-1234-1234-123456789abc","code":"ABCDEF","members":[{"player":"10","name":"Changed","online":true,"look":0,"outfit":0,"art":null}]}})");
    i.Party(party);assert(i.snapshot.members.empty());
    puts("Actual art publication: generation/session/epoch/ref/drop/expiry/cancel gates, stop during publication, and late party shutdown PASS");
}
