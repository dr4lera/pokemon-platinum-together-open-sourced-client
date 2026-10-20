// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_WIRELESS_H
#define COMMUNITY_WIRELESS_H
#include "port/community_online.h"
#include <string>
namespace CommunityWireless {
CommunityOnline::Service& Online();
bool Install();
bool Prepare(); // Game/render thread only: freezes local compatibility values.
bool RulesLocked();
std::string Status();
}
#endif
