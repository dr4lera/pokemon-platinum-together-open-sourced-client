// SPDX-License-Identifier: GPL-3.0-or-later
#include <nitro.h>
#include "port/community_link_qa.h"
#include "port/community_link_qa_internal.h"
#include "port/community_link_qa_guard.h"
#include "port/community_wireless.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
using Clock = std::chrono::steady_clock;
CommunityLinkQAGuard::Directory sDirectory;
bool sRequested = false, sAuthorized = false, sStarted = false, sConnected = false;
bool sPartyRequested = false, sPrepared = false, sAnnounced = false, sOpened = false, sEntered = false;
Clock::time_point sDeadline;
const char* Environment(const char* name) { const auto* value = std::getenv(name); return value ? value : ""; }
[[noreturn]] void Refuse(const std::string& reason)
{
    std::fprintf(stderr, "Community link QA refused: %s\n", reason.c_str());
    std::fflush(stderr);
    std::exit(2);
}
void Log(const char* text)
{
    std::fprintf(stderr, "Community link QA: %s\n", text);
    std::fflush(stderr);
}
std::string PreparedMarker(const CommunityOnline::LinkSnapshot& link)
{
    return "community-link-qa-v1\nnonce=" + sDirectory.nonce + "\nbuild=" + link.build + "\ncatalog=" + link.catalog + "\nrules=" + link.rules + "\n";
}
}
extern "C" void CommunityLinkQA_Preflight(void)
{
    const auto* scene = Environment("PLATINUM_COMMUNITY_LINK_QA");
    if (!*scene) return;
    sRequested = true;
    if (std::strcmp(scene, "union-room") || std::strcmp(Environment("PLATINUM_COMMUNITY_QA_CONFIRM_FRESH"), "1")
        || *Environment("PLATINUM_COMMUNITY_QA_SCENE")) Refuse("requires union-room, fresh confirmation, and no other QA scene");
    std::string error;
    if (!CommunityLinkQAGuard::Validate(Environment("PLATINUM_COMMUNITY_LINK_QA_DIRECTORY"), Environment("PLATINUM_COMMUNITY_LINK_QA_ROLE"), sDirectory, error)) Refuse(error);
    if (!*Environment("PLATINUM_COMMUNITY_SERVER_URL") || !*Environment("PLATINUM_COMMUNITY_SERVER_KEY")) Refuse("explicit server URL and pinned server key required");
    sAuthorized = true;
    Log("fresh directory verified before save access; native saving is confined to this disposable working directory");
}
extern "C" BOOL CommunityLinkQA_AuthorizeFresh(BOOL fresh)
{
    if (!sRequested && !*Environment("PLATINUM_COMMUNITY_LINK_QA")) return FALSE;
    if (!sAuthorized || sStarted || !fresh) Refuse("startup preflight and an unused fresh save are required");
    sStarted = true;
    sDeadline = Clock::now() + std::chrono::seconds(120);
    Log("fresh game state authorized; the field bridge will seed two level-10 Pokémon without accepting an activity");
    return TRUE;
}
extern "C" BOOL CommunityLinkQA_IsHost(void) { return sDirectory.role == "host"; }
extern "C" void CommunityLinkQA_DidEnter(void)
{
    sEntered = true;
    Log("authenticated two-peer link is Active; entering the normal Union Room warp and CommManager state machines");
}
extern "C" BOOL CommunityLinkQA_Update(void)
{
    if (!sStarted || sEntered) return FALSE;
    auto& online = CommunityWireless::Online();
    if (Clock::now() > sDeadline) Refuse("online/party/link setup exceeded 120 seconds");
    if (!sConnected) {
        CommunityOnline::Options options;
        options.serverUrl = Environment("PLATINUM_COMMUNITY_SERVER_URL");
        options.serverKey = Environment("PLATINUM_COMMUNITY_SERVER_KEY");
        const auto encodedDirectory = (sDirectory.path / "CommunityOnline" / "QAAccount").u8string();
        options.profileDirectory.assign(reinterpret_cast<const char*>(encodedDirectory.data()), encodedDirectory.size());
        options.username = std::string(sDirectory.role == "host" ? "QAH" : "QAG") + sDirectory.nonce.substr(0, 12);
        options.reconnect = false;
        if (!CommunityWireless::Install() || !online.Connect(std::move(options))) Refuse("could not install/connect the actual community provider");
        sConnected = true;
        Log("connecting a new isolated signed account");
        return FALSE;
    }
    const auto lobby = online.Read();
    if (lobby.state == CommunityOnline::State::Failed || lobby.state == CommunityOnline::State::Stopped) Refuse("online connection failed: " + lobby.error);
    if (lobby.state != CommunityOnline::State::Ready) return FALSE;
    if (!sPartyRequested) {
        if (sDirectory.role == "host") {
            if (!online.CreateParty()) Refuse("party creation could not be queued");
        } else {
            const auto code = CommunityLinkQAGuard::ReadSmall(sDirectory.partyFile, 256);
            const auto prefix = "community-link-qa-v1\nnonce=" + sDirectory.nonce + "\ncode=";
            if (code.empty()) return FALSE;
            if (code.compare(0, prefix.size(), prefix) || code.back() != '\n') Refuse("party coordination record mismatch");
            const auto partyCode = code.substr(prefix.size(), code.size() - prefix.size() - 1);
            if (!online.JoinParty(partyCode)) Refuse("party join could not be queued");
        }
        sPartyRequested = true;
        return FALSE;
    }
    if (lobby.partyId.empty()) { if (!lobby.error.empty()) Refuse("party setup failed: " + lobby.error); return FALSE; }
    if (sDirectory.role == "host" && !sAnnounced) {
        const auto record = "community-link-qa-v1\nnonce=" + sDirectory.nonce + "\ncode=" + lobby.partyCode + "\n";
        if (!CommunityLinkQAGuard::WriteNew(sDirectory.partyFile, record)) Refuse("could not publish fresh private party coordination");
        sAnnounced = true;
    }
    if (lobby.members.size() != 2 || !lobby.members[0].online || !lobby.members[1].online) return FALSE;
    if (!sPrepared) {
        if (!CommunityWireless::Prepare()) Refuse("real compatibility preparation could not be queued");
        sPrepared = true;
        return FALSE;
    }
    const auto link = online.ReadLink();
    if (link.state == CommunityOnline::LinkState::Failed || link.state == CommunityOnline::LinkState::Ended) Refuse("link setup ended: " + link.error);
    if (link.state == CommunityOnline::LinkState::Prepared && !sOpened) {
        if (sDirectory.role == "guest") {
            if (!CommunityLinkQAGuard::WriteNew(sDirectory.guestPreparedFile, PreparedMarker(link))) Refuse("could not record the guest prepare acknowledgment");
            sOpened = true;
        } else {
            const auto guest = CommunityLinkQAGuard::ReadSmall(sDirectory.guestPreparedFile, 512);
            if (guest.empty()) return FALSE;
            if (guest != PreparedMarker(link)) Refuse("guest compatibility acknowledgment differs");
            if (!online.OpenLink()) Refuse("real link allocation could not be queued");
            sOpened = true;
        }
    }
    return link.state == CommunityOnline::LinkState::Active;
}
