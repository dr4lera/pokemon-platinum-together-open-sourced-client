// SPDX-License-Identifier: GPL-3.0-or-later
#include <simulator/sim_net.h>
#include "port/community_union_binding.h"
#include "port/community_wireless.h"
extern "C" void CommunityUnion_CopyRoster(CommunityUnionRoster *output)
{
    if(!output)return;
    *output={};
    if(!SIM_Net_HasProvider())return;
    output->selected=1;
    auto& service=CommunityWireless::Online();
    const auto before=service.ReadLink();
    const auto lobby=service.Read();
    const auto after=service.ReadLink();
    if(before.state!=CommunityOnline::LinkState::Active||before.cancelling||after.state!=before.state||after.cancelling
        ||before.session!=after.session||before.epoch!=after.epoch)return;
    CommunityUnionRoster roster{};
    if(CommunityUnionBinding::Build(lobby,after,roster))*output=roster;
}
