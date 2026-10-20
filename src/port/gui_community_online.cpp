// SPDX-License-Identifier: GPL-3.0-or-later
#include <nitro.h>
#include <simulator/imgui/imgui.hpp>
#include "port/community_online.h"
#include "port/community_wireless.h"
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <openssl/sha.h>
#include <string>

namespace SIM::GUI {
namespace {
CommunityOnline::Service& OnlineService()
{
    return CommunityWireless::Online();
}
void InitialValue(char* output, size_t capacity, const char* environment)
{
    const char* value = std::getenv(environment);
    if (value) std::snprintf(output, capacity, "%s", value);
}
std::string ServerProfile(const char* server, const char* key)
{
    std::string identity = server;
    while (!identity.empty() && identity.back() == '/') identity.pop_back();
    identity.push_back('\0');
    identity += key;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(identity.data()), identity.size(), hash);
    const char* hex = "0123456789abcdef";
    std::string path = "CommunityOnline/Profiles/";
    for (unsigned char byte : hash) {
        path.push_back(hex[byte >> 4]);
        path.push_back(hex[byte & 15]);
    }
    return path;
}
}

void CommunityOnlineMain(bool* openState)
{
    static char server[512] = "";
    static char pin[128] = "";
    static char name[64] = "";
    static char code[128] = "";
    static char message[513] = "";
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        InitialValue(server, sizeof(server), "PLATINUM_COMMUNITY_SERVER_URL");
        InitialValue(pin, sizeof(pin), "PLATINUM_COMMUNITY_SERVER_KEY");
        InitialValue(name, sizeof(name), "PLATINUM_COMMUNITY_USERNAME");
    }
    auto& service = OnlineService();
    const auto snapshot = service.Read();
    const bool inactive = snapshot.state == CommunityOnline::State::Stopped || snapshot.state == CommunityOnline::State::Failed;
    const bool ready = snapshot.state == CommunityOnline::State::Ready;
    ImGui::Begin("Community servers", openState);
    ImGui::Text("Status: %s", CommunityOnline::StateName(snapshot.state));
    if (!snapshot.error.empty()) ImGui::TextWrapped("%s", snapshot.error.c_str());
    ImGui::BeginDisabled(!inactive);
    ImGui::InputText("Server address", server, sizeof(server));
    ImGui::InputText("Server identity key", pin, sizeof(pin));
    ImGui::InputText("Username", name, sizeof(name));
    if (ImGui::Button("Connect")) {
        CommunityOnline::Options options;
        options.serverUrl = server;
        options.serverKey = pin;
        options.username = name;
        const char* directory = std::getenv("PLATINUM_COMMUNITY_ONLINE_PROFILE");
        options.profileDirectory = directory && *directory ? directory : ServerProfile(server, pin);
        if (CommunityWireless::Install()) service.Connect(std::move(options));
    }
    ImGui::EndDisabled();
    if (!inactive) {
        ImGui::SameLine();
        if (ImGui::Button("Disconnect")) service.Disconnect();
    }
    if (!snapshot.username.empty()) ImGui::Text("Signed in as %s", snapshot.username.c_str());
    const auto wirelessError = CommunityWireless::Status();
    if (!wirelessError.empty()) ImGui::TextWrapped("%s", wirelessError.c_str());
    ImGui::Separator();
    ImGui::BeginDisabled(!ready);
    if (snapshot.partyId.empty()) {
        if (ImGui::Button("Create party")) service.CreateParty();
        ImGui::InputText("Party code", code, sizeof(code));
        if (ImGui::Button("Join party")) service.JoinParty(code);
    } else {
        ImGui::TextWrapped("Party code: %s", snapshot.partyCode.c_str());
        if (ImGui::Button("Copy code")) ImGui::SetClipboardText(snapshot.partyCode.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Leave party")) service.LeaveParty();
        for (const auto& member : snapshot.members) {
            ImGui::BulletText("%s (%s)", member.name.c_str(), member.online ? "online" : "offline");
        }
    }
    const auto link = service.ReadLink();
    ImGui::Separator();
    ImGui::Text("Game link: %s", CommunityOnline::LinkStateName(link.state));
    if (link.cancelling) ImGui::TextUnformatted("Closing game link...");
    if (!link.error.empty()) ImGui::TextWrapped("%s", link.error.c_str());
    if (link.available && !snapshot.partyId.empty()) {
        const bool preparing = link.state == CommunityOnline::LinkState::Idle
            || link.state == CommunityOnline::LinkState::Ended || link.state == CommunityOnline::LinkState::Failed;
        if (preparing && !link.cancelling && ImGui::Button("Prepare game link")) CommunityWireless::Prepare();
        if (link.state == CommunityOnline::LinkState::Prepared && ImGui::Button("Open game link (party leader)")) service.OpenLink();
        if (!preparing) {
            if (ImGui::Button("Close game link")) service.CancelLink();
        }
        if (link.state == CommunityOnline::LinkState::Active)
            ImGui::TextWrapped("Connected. Enter the Pokemon Center Union Room to use native battles or trades.");
    } else if (!link.available) ImGui::TextWrapped("This server has not enabled game links.");
    ImGui::Separator();
    ImGui::TextUnformatted("Global chat");
    ImGui::BeginChild("community-chat", ImVec2(0, 160), true);
    for (const auto& item : snapshot.messages) ImGui::TextWrapped("%s: %s", item.name.c_str(), item.text.c_str());
    ImGui::EndChild();
    const bool enter = ImGui::InputText("Message", message, sizeof(message), ImGuiInputTextFlags_EnterReturnsTrue);
    if ((enter || ImGui::Button("Send")) && service.SendGlobalChat(message)) message[0] = '\0';
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextWrapped("Development client. Game links use the native Union Room; shared-world and Inverna integration is still in progress.");
    ImGui::End();
}
}
