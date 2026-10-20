// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_WIRELESS_CORE_H
#define COMMUNITY_WIRELESS_CORE_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CommunityWireless {
constexpr unsigned MaxPeers = 4;
constexpr unsigned MaxData = 512;
enum class Role : std::uint8_t { Off, Parent, Child };
struct ParentInfo {
    std::uint16_t parentMaxSize = 0, childMaxSize = 0, beaconPeriod = 0, channel = 0, maxEntry = 15;
    std::uint8_t attributes = 1;
};
struct Peer {
    std::array<std::uint8_t, 6> mac{};
    Role role = Role::Off;
    std::uint8_t aid = 0;
    std::uint32_t ggid = 0, tgid = 0;
    ParentInfo parent;
    std::vector<std::uint8_t> gameInfo;
};
struct Frame { std::uint8_t destination; std::vector<std::uint8_t> bytes; };
enum class EventKind { Join, Disconnect, Data };
struct Event {
    EventKind kind;
    std::uint8_t aid = 0, port = 0;
    std::vector<std::uint8_t> bytes;
};
// A deterministic WM state machine. The owner serializes calls and delivers
// Events outside its lock, retaining bytes until the ARM9 callback acknowledges.
// Server seats identify authenticated peers; AIDs are allocated by a DS parent.
class Core {
public:
    bool Begin(unsigned localSeat, const std::vector<std::array<std::uint8_t, 6>>& roster);
    void End();
    bool SetInfo(std::uint32_t ggid, std::uint32_t tgid, const std::uint8_t* bytes, unsigned size, ParentInfo parent = {});
    bool SetRole(Role role);
    bool Join(const std::uint8_t mac[6], const std::uint8_t ssid[24], std::uint32_t nonce);
    void CancelJoin();
    bool Disconnect(std::uint16_t aidMask, std::uint16_t& disconnected);
    bool Send(std::uint16_t aidMask, unsigned port, const std::uint8_t* bytes, unsigned size);
    bool Receive(unsigned sender, const std::vector<std::uint8_t>& bytes);
    bool Active() const { return active; }
    bool Joining() const { return joinParent < MaxPeers && peers[seat].role != Role::Child; }
    unsigned LocalSeat() const { return seat; }
    unsigned Count() const { return count; }
    const Peer& GetPeer(unsigned index) const { return peers.at(index); }
    std::vector<Frame> TakeFrames();
    std::vector<Event> TakeEvents();
private:
    bool active = false;
    unsigned seat = 0, count = 0, joinParent = MaxPeers;
    std::uint32_t joinNonce = 0;
    std::uint16_t allocated = 1;
    std::array<Peer, MaxPeers> peers{};
    std::array<std::uint32_t, MaxPeers> acceptedNonces{};
    std::vector<Frame> frames;
    std::vector<Event> events;
    bool Advertise();
    void Lost(unsigned index);
    bool Queue(unsigned destination, std::vector<std::uint8_t> bytes);
};
}
#endif
