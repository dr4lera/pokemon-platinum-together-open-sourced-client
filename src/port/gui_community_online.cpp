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
    ImGui::TextUnformatted("Server trainer profile");
    if (snapshot.trainer.loaded) {
        static char trainerName[11] = "";
        static int look = 0, outfit = 0;
        static std::string profilePlayer;
        static std::uint64_t revision = 0, connection = 0;
        static bool dirty = false, saveQueued = false, saveRefused = false;
        const auto load = [&] {
            std::snprintf(trainerName, sizeof(trainerName), "%s", snapshot.trainer.value.name.c_str());
            look = snapshot.trainer.value.look; outfit = snapshot.trainer.value.outfit;
            revision = snapshot.trainer.value.revision; profilePlayer = snapshot.player;
            connection = snapshot.connections; dirty = false; saveQueued = false; saveRefused = false;
        };
        if (profilePlayer != snapshot.player || connection != snapshot.connections || (!dirty && revision != snapshot.trainer.value.revision)) load();
        if (saveQueued && !snapshot.trainer.updating && !snapshot.trainer.conflict && snapshot.trainer.value.revision > revision) load();
        ImGui::BeginDisabled(snapshot.trainer.updating);
        dirty |= ImGui::InputText("Trainer name (10 ASCII bytes)", trainerName, sizeof(trainerName));
        dirty |= ImGui::InputInt("Server look ID (0-255)", &look);
        dirty |= ImGui::InputInt("Server outfit ID (0-65535)", &outfit);
        const bool valid = trainerName[0] && look >= 0 && look <= 255 && outfit >= 0 && outfit <= 65535;
        ImGui::BeginDisabled(!valid || !dirty);
        if (ImGui::Button("Save trainer profile")) {
            auto value = snapshot.trainer.value;
            value.name = trainerName; value.look = static_cast<std::uint8_t>(look);
            value.outfit = static_cast<std::uint16_t>(outfit); value.revision = revision;
            saveQueued = service.UpdateTrainer(value);
            saveRefused = !saveQueued;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Use latest profile")) { load(); service.RefreshTrainer(); }
        ImGui::EndDisabled();
        if (snapshot.trainer.updating) ImGui::TextUnformatted("Saving profile...");
        if (snapshot.trainer.conflict) ImGui::TextWrapped("Profile changed elsewhere. Use latest profile before editing again.");
        if (saveRefused) ImGui::TextWrapped("Profile could not be queued. Check the name and ID ranges, then try again.");
        ImGui::TextWrapped("Names start with a letter or number and use letters, numbers, spaces, periods, apostrophes or hyphens. End with a letter, number or period.");
        ImGui::TextWrapped("Look and outfit IDs are saved on this server. Their character sprites and custom art are not rendered by this client yet.");
    } else ImGui::TextUnformatted("Waiting for trainer profile...");
    ImGui::Separator();
    ImGui::Text("City presence: %s", CommunityOnline::CityStateName(snapshot.city.state));
    if (!snapshot.city.error.empty()) ImGui::TextWrapped("%s", snapshot.city.error.c_str());
    if (snapshot.city.state == CommunityOnline::CityState::Absent) {
        if (ImGui::Button("Join plaza presence")) service.EnterCity();
    } else {
        // Leave remains independent of invitations and game-link preparation.
        if (ImGui::Button("Leave plaza presence")) service.LeaveCity();
        if (snapshot.city.state == CommunityOnline::CityState::Present) {
            ImGui::Text("Your server slot: %u", static_cast<unsigned>(snapshot.city.slot));
            if (ImGui::Button("Active")) service.SetCityStatus("active");
            ImGui::SameLine(); if (ImGui::Button("Away")) service.SetCityStatus("away");
            for (const auto& player : snapshot.city.players) {
                ImGui::BulletText("%s [%s] - slot %u, look %u, outfit %u", player.name.c_str(), player.status.c_str(), static_cast<unsigned>(player.slot), static_cast<unsigned>(player.appearance.look), static_cast<unsigned>(player.appearance.outfit));
                if (player.appearance.hasArt) ImGui::TextWrapped("Custom art %s / %s (rendering unsupported)", player.appearance.art.creation.c_str(), player.appearance.art.version.c_str());
            }
        }
    }
    ImGui::TextWrapped("This panel lists authenticated plaza presence. It does not move your character or enter a city map.");
    ImGui::Separator();
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
            ImGui::Text("Server appearance: look %u, outfit %u", static_cast<unsigned>(member.appearance.look), static_cast<unsigned>(member.appearance.outfit));
            if (member.appearance.hasArt) ImGui::TextWrapped("Custom art %s / %s (rendering unsupported)", member.appearance.art.creation.c_str(), member.appearance.art.version.c_str());
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
