// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_ONLINE_H
#define COMMUNITY_ONLINE_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CommunityOnline {
enum class State { Stopped, Connecting, Authenticating, Ready, Reconnecting, Stopping, Failed };
struct Options {
    std::string serverUrl;
    std::string serverKey; // Explicitly trusted raw Ed25519 public key, canonical base64.
    std::string profileDirectory; // Dedicated own-client directory; no game/save paths.
    std::string username; // Required only when creating this adapter's account.
    std::string build = "community-client-online-v1";
    bool reconnect = true;
    std::uint32_t pingIntervalMs = 20000;
    std::uint32_t pongTimeoutMs = 10000;
};
struct Member { std::string player, name; bool online = false; };
struct Message { std::uint64_t id = 0; std::string from, name, text; };
enum class LinkState { Idle, Preparing, Prepared, Opening, Connecting, Authenticating, Waiting, Active, Ended, Failed };
struct LinkPeer { std::uint8_t seat = 255; std::string player, mac; };
struct LinkSnapshot {
    LinkState state = LinkState::Idle;
    bool available = false;
    bool cancelling = false; // Local traffic stopped; awaiting server consent withdrawal ack.
    std::string error, session, party, build, catalog, rules, rosterHash, requestId;
    std::uint64_t epoch = 0, expiresAtMs = 0;
    std::uint8_t seat = 255;
    std::vector<LinkPeer> roster;
};
struct LinkPacket {
    std::string session;
    std::uint64_t epoch = 0;
    std::uint8_t sender = 255;
    std::vector<std::uint8_t> payload;
};
struct Snapshot {
    State state = State::Stopped;
    std::uint64_t connections = 0; // Successful lobby authentications this Connect lifetime.
    std::string error, player, username, partyId, partyCode;
    std::vector<Member> members;
    std::vector<Message> messages;
};
// All network and key/file operations run on a private worker. Read() copies a
// bounded snapshot; queue methods never access the game/save/renderer.
class Service {
public:
    Service();
    ~Service();
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    bool Connect(Options options); // False while an earlier worker is still active.
    void Disconnect();
    Snapshot Read() const;
    bool CreateParty();
    bool JoinParty(const std::string& code);
    bool LeaveParty();
    bool SendGlobalChat(const std::string& text);
    LinkSnapshot ReadLink() const; // No account keys or single-use tickets.
    bool PrepareLink(const std::string& catalogHash, const std::string& rulesHash);
    bool OpenLink(const std::string& requestId = "");
    bool CancelLink();
    bool SendLink(std::uint8_t destination, const std::vector<std::uint8_t>& payload);
    bool ReceiveLink(LinkPacket& packet); // Transfers one owned packet; no game pointers.
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
const char* StateName(State state);
const char* LinkStateName(LinkState state);
}
#endif
