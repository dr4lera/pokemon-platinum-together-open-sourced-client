// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_wireless_core.h"
#include <cassert>
#include <cstdio>
#include <deque>
#include <utility>
using namespace CommunityWireless;
static const std::vector<std::array<std::uint8_t,6>> roster = {
    {2,0,0,0,0,1},{2,0,0,0,0,2},{2,0,0,0,0,3},{2,0,0,0,0,4}
};
static Core clients[4];
static void Pump()
{
    for (unsigned rounds=0; rounds<32; ++rounds) {
        bool sent=false;
        for (unsigned i=0; i<4; ++i) for (auto& frame: clients[i].TakeFrames()) {
            assert(clients[frame.destination].Receive(i,frame.bytes)); sent=true;
        }
        if (!sent) return;
    }
    assert(false && "Unbounded control loop");
}
static void Setup()
{
    for (unsigned i=0;i<4;++i) { clients[i].End(); clients[i].TakeEvents(); assert(clients[i].Begin(i,roster)); }
    Pump();
}
int main()
{
    Core invalid;
    assert(!invalid.Begin(4,roster));
    auto bad=roster; bad[1]=bad[0]; assert(!invalid.Begin(0,bad));
    bad=roster; bad[0][0]=1; assert(!invalid.Begin(0,bad));
    Setup();
    const std::uint8_t info[]{0x12,0x34,0x56};
    assert(clients[2].SetInfo(0x12345678,0xabcdef01,info,sizeof(info)));
    assert(clients[2].SetRole(Role::Parent)); Pump();
    assert(clients[0].GetPeer(2).ggid==0x12345678 && clients[0].GetPeer(2).tgid==0xabcdef01);
    assert(clients[0].GetPeer(2).gameInfo==std::vector<std::uint8_t>({0x12,0x34,0x56}));
    std::uint8_t ssid[24]; for(unsigned i=0;i<24;++i)ssid[i]=static_cast<std::uint8_t>(i+10);
    // Seat2 is the DS parent, seat0 is assigned AID1. The leader is unrelated.
    assert(clients[0].Join(roster[2].data(),ssid,41));
    auto request=clients[0].TakeFrames(); assert(request.size()==1);
    assert(clients[2].Receive(0,request[0].bytes));
    auto joined=clients[2].TakeEvents(); assert(joined.size()==1 && joined[0].kind==EventKind::Join && joined[0].aid==1);
    assert(joined[0].bytes==std::vector<std::uint8_t>(ssid,ssid+24));
    assert(clients[2].Receive(0,request[0].bytes)); assert(clients[2].TakeEvents().empty());
    Pump(); assert(!clients[0].Joining() && clients[0].GetPeer(0).aid==1);
    assert(clients[1].Join(roster[2].data(),ssid,42)); Pump();
    assert(clients[1].GetPeer(1).aid==2);
    clients[2].TakeEvents();
    const std::uint8_t data[]{0,1,2,3,0xff};
    assert(clients[2].Send(0xffff,15,data,sizeof(data))); Pump();
    for(unsigned i:{0u,1u}) {
        auto events=clients[i].TakeEvents(); assert(events.size()==1 && events[0].kind==EventKind::Data && events[0].aid==0 && events[0].port==15);
        assert(events[0].bytes==std::vector<std::uint8_t>(data,data+sizeof(data)));
    }
    assert(clients[3].TakeEvents().empty());
    assert(clients[0].Send(1,7,data,sizeof(data))); Pump();
    auto received=clients[2].TakeEvents(); assert(received.size()==1 && received[0].aid==1 && received[0].port==7);
    assert(!clients[0].Send(4,7,data,sizeof(data))); // No child-to-child shortcut.
    assert(!clients[0].Send(1,16,data,sizeof(data)));
    assert(!clients[0].Send(1,0,nullptr,1));
    std::vector<std::uint8_t> oversized(513); assert(!clients[0].Send(1,0,oversized.data(),513));
    assert(clients[0].Send(1,0,nullptr,0)); Pump(); assert(clients[2].TakeEvents()[0].bytes.empty());
    assert(clients[0].Send(1,3,data,sizeof(data)));
    auto frame=clients[0].TakeFrames()[0].bytes;
    assert(!clients[2].Receive(2,frame));
    assert(!clients[2].Receive(3,frame)); // Sender seat authenticated by server; no joined AID.
    auto forged=frame; forged[5]=2; assert(!clients[2].Receive(0,forged));
    for (unsigned size=0;size<9;++size) assert(!clients[2].Receive(0,{frame.begin(),frame.begin()+size}));
    forged=frame; forged[8]++; assert(!clients[2].Receive(0,forged));
    forged=frame; forged[6]=16; assert(!clients[2].Receive(0,forged));
    forged=frame; forged[3]=1; assert(!clients[2].Receive(0,forged));
    assert(clients[2].TakeEvents().empty());
    std::uint16_t disconnected=0;
    assert(clients[0].Disconnect(1,disconnected) && disconnected==1); Pump();
    auto lost=clients[2].TakeEvents(); assert(lost.size()==1 && lost[0].kind==EventKind::Disconnect && lost[0].aid==1);
    assert(clients[0].GetPeer(0).role==Role::Off);
    assert(clients[3].Join(roster[2].data(),ssid,43)); Pump();
    assert(clients[3].GetPeer(3).aid==1); // Freed AID reused independently of seat.
    clients[2].TakeEvents();
    assert(clients[2].Disconnect(1u<<2,disconnected) && disconnected==(1u<<2)); Pump();
    lost=clients[1].TakeEvents(); assert(lost.size()==1 && lost[0].kind==EventKind::Disconnect && clients[1].GetPeer(1).role==Role::Off);
    assert(clients[1].GetPeer(2).role==Role::Parent);
    assert(clients[1].Join(roster[2].data(),ssid,44)); Pump();
    assert(clients[1].GetPeer(1).aid==2); clients[2].TakeEvents();
    clients[3].End(); lost=clients[3].TakeEvents(); assert(lost.size()==1 && lost[0].kind==EventKind::Disconnect);
    assert(clients[3].TakeFrames().empty()); assert(!clients[3].Receive(2,frame));
    // Queues are bounded and ending a generation drops all unsent payloads.
    for(unsigned i=0;i<128;++i)assert(clients[2].Send(2,0,data,sizeof(data)));
    assert(!clients[2].Send(2,0,data,sizeof(data)));
    assert(!clients[2].SetInfo(9,9,info,sizeof(info)) && clients[2].GetPeer(2).ggid==0x12345678);
    assert(!clients[2].SetRole(Role::Off) && clients[2].GetPeer(2).role==Role::Parent);
    assert(!clients[2].Disconnect(2,disconnected) && disconnected==0 && clients[2].GetPeer(3).role==Role::Child);
    clients[2].End(); assert(clients[2].TakeFrames().empty());
    assert(!clients[2].SetRole(Role::Child));
    // Real parent limits and beacon metadata survive fixed-width v2 encoding.
    Setup();
    const ParentInfo parent{512,128,120,11,1,5};
    std::vector<std::uint8_t> maximumInfo(112);
    for(unsigned i=0;i<maximumInfo.size();i++)maximumInfo[i]=static_cast<std::uint8_t>(i*3+1);
    assert(clients[2].SetInfo(0x1234,0x5678,maximumInfo.data(),112,parent));
    assert(clients[2].SetRole(Role::Parent));Pump();
    const auto& advertised=clients[0].GetPeer(2);
    assert(advertised.parent.parentMaxSize==512&&advertised.parent.childMaxSize==128&&advertised.parent.channel==11&&advertised.parent.beaconPeriod==120&&advertised.parent.maxEntry==1&&advertised.parent.attributes==5&&advertised.gameInfo==maximumInfo);
    auto invalidParent=parent;invalidParent.parentMaxSize=513;assert(!clients[2].SetInfo(9,9,info,sizeof(info),invalidParent));
    invalidParent=parent;invalidParent.maxEntry=16;assert(!clients[2].SetInfo(9,9,info,sizeof(info),invalidParent));
    invalidParent=parent;invalidParent.channel=15;assert(!clients[2].SetInfo(9,9,info,sizeof(info),invalidParent));
    assert(clients[0].Join(roster[2].data(),ssid,100));Pump();clients[2].TakeEvents();
    assert(clients[1].Join(roster[2].data(),ssid,101));Pump();
    assert(!clients[1].Joining()&&clients[1].GetPeer(1).role==Role::Off&&clients[1].Active()&&clients[2].Active()&&clients[2].TakeEvents().empty());
    assert(clients[0].Disconnect(1,disconnected));Pump();clients[2].TakeEvents();
    assert(clients[1].Join(roster[2].data(),ssid,102));Pump();assert(clients[1].GetPeer(1).aid==1);
    std::puts("WM semantic core: authenticated seats, independent AIDs, four-peer handshake, duplicate joins, parent/child routing, malformed input, disconnect/reuse, bounded queues and generation teardown passed.");
}
