// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_wireless_core.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace CommunityWireless {
namespace {
enum Kind : std::uint8_t { Info = 1, JoinRequest, JoinResponse, DisconnectPeer, Data };
std::vector<std::uint8_t> Packet(Kind kind) { return {'C','W','M',2,kind}; }
void U16(std::vector<std::uint8_t>& out, std::uint16_t n) { out.push_back(n >> 8); out.push_back(n & 255); }
std::uint16_t U16(const std::vector<std::uint8_t>& in, unsigned pos) { return (std::uint16_t(in[pos]) << 8) | in[pos+1]; }
bool ValidParent(const ParentInfo& p) { return p.parentMaxSize <= MaxData && p.childMaxSize <= MaxData && p.channel <= 14 && p.maxEntry <= 15; }
void U32(std::vector<std::uint8_t>& out, std::uint32_t n) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(n >> shift));
}
std::uint32_t U32(const std::vector<std::uint8_t>& in, unsigned pos) {
    return (std::uint32_t(in[pos]) << 24) | (std::uint32_t(in[pos+1]) << 16)
        | (std::uint32_t(in[pos+2]) << 8) | in[pos+3];
}
}
bool Core::Begin(unsigned localSeat, const std::vector<std::array<std::uint8_t, 6>>& roster)
{
    if (active || roster.size() < 2 || roster.size() > MaxPeers || localSeat >= roster.size()) return false;
    for (unsigned i = 0; i < roster.size(); ++i) {
        if ((roster[i][0] & 3) != 2) return false;
        for (unsigned j = 0; j < i; ++j) if (roster[i] == roster[j]) return false;
    }
    End(); seat = localSeat; count = static_cast<unsigned>(roster.size()); active = true;
    for (unsigned i = 0; i < count; ++i) peers[i].mac = roster[i];
    Advertise(); return true;
}
void Core::Lost(unsigned index)
{
    auto& p = peers[index];
    if (peers[seat].role == Role::Parent && p.role == Role::Child && p.aid > 0) {
        events.push_back({EventKind::Disconnect, p.aid, 0, {}});
        allocated &= ~(1u << p.aid);
    } else if (index == joinParent && peers[seat].role == Role::Child) {
        events.push_back({EventKind::Disconnect, 0, 0, {}});
        peers[seat].role = Role::Off; peers[seat].aid = 0; joinParent = MaxPeers;
    }
    p.role = Role::Off; p.aid = 0; acceptedNonces[index] = 0;
}
void Core::End()
{
    frames.clear(); events.clear();
    if (active) for (unsigned i = 0; i < count; ++i) if (i != seat) Lost(i);
    peers = {}; acceptedNonces = {}; active = false; count = 0; joinParent = MaxPeers; allocated = 1; joinNonce = 0;
}
bool Core::Queue(unsigned destination, std::vector<std::uint8_t> bytes)
{
    if (!active || destination >= count || destination == seat || frames.size() >= 128) return false;
    frames.push_back({static_cast<std::uint8_t>(destination), std::move(bytes)}); return true;
}
bool Core::Advertise()
{
    if (!active) return true;
    if (frames.size() + count - 1 > 128) return false;
    const auto& p = peers[seat];
    auto b = Packet(Info); b.push_back(static_cast<std::uint8_t>(p.role)); b.push_back(p.aid);
    U32(b, p.ggid); U32(b, p.tgid);
    U16(b,p.parent.parentMaxSize); U16(b,p.parent.childMaxSize); U16(b,p.parent.beaconPeriod);
    U16(b,p.parent.channel); U16(b,p.parent.maxEntry); b.push_back(p.parent.attributes);
    b.push_back(static_cast<std::uint8_t>(p.gameInfo.size()));
    b.insert(b.end(), p.gameInfo.begin(), p.gameInfo.end());
    for (unsigned i = 0; i < count; ++i) if (i != seat) Queue(i, b);
    return true;
}
bool Core::SetInfo(std::uint32_t ggid, std::uint32_t tgid, const std::uint8_t* bytes, unsigned size, ParentInfo parent)
{
    if (!ValidParent(parent) || size > 112 || (size && !bytes) || (active && frames.size() + count - 1 > 128)) return false;
    auto& p = peers[seat]; p.ggid = ggid; p.tgid = tgid; p.parent = parent; p.gameInfo.clear();
    if (size) p.gameInfo.assign(bytes, bytes + size);
    Advertise(); return true;
}
bool Core::SetRole(Role role)
{
    if (role != Role::Off && role != Role::Parent && role != Role::Child) return false;
    auto& p = peers[seat];
    // WM StartMP follows a successful child join. It cannot invent an AID.
    if (role == Role::Child && (p.role != Role::Child || joinParent >= count)) return false;
    if (role == p.role) return true;
    // Child Disconnect advertises Off before this transition's final role.
    unsigned departures = p.role == Role::Child ? count : 0;
    if (p.role == Role::Parent) for (unsigned i = 0; i < count; ++i)
        if (i != seat && peers[i].role == Role::Child && peers[i].aid > 0) ++departures;
    if (active && frames.size() + departures + count - 1 > 128) return false;
    if (active && p.role != Role::Off) { std::uint16_t ignored; Disconnect(0xffff, ignored); }
    p.role = role; p.aid = 0; allocated = 1; Advertise(); return true;
}
bool Core::Join(const std::uint8_t mac[6], const std::uint8_t ssid[24], std::uint32_t nonce)
{
    if (!active || !mac || !ssid || !nonce || peers[seat].role != Role::Off || Joining()) return false;
    for (unsigned i = 0; i < count; ++i) if (i != seat && peers[i].role == Role::Parent && std::memcmp(mac, peers[i].mac.data(), 6) == 0) {
        auto b = Packet(JoinRequest); U32(b, nonce); b.insert(b.end(), ssid, ssid+24);
        if (!Queue(i, std::move(b))) return false;
        joinParent = i; joinNonce = nonce; return true;
    }
    return false;
}
void Core::CancelJoin() { if (peers[seat].role != Role::Child) { joinParent = MaxPeers; joinNonce = 0; } }
bool Core::Disconnect(std::uint16_t mask, std::uint16_t& disconnected)
{
    disconnected = 0;
    if (!active) return false;
    const auto role = peers[seat].role;
    if (role == Role::Child && joinParent < count && (mask & 1)) {
        if (frames.size() + count > 128) return false;
        auto b = Packet(DisconnectPeer); b.push_back(peers[seat].aid);
        if (!Queue(joinParent, std::move(b))) return false;
        disconnected = 1; peers[seat].role = Role::Off; peers[seat].aid = 0; joinParent = MaxPeers; Advertise(); return true;
    }
    if (role == Role::Parent) {
        unsigned recipients = 0;
        for (unsigned i = 0; i < count; ++i) if (i != seat && peers[i].role == Role::Child && peers[i].aid > 0 && (mask & (1u << peers[i].aid))) ++recipients;
        if (frames.size() + recipients > 128) return false;
        for (unsigned i = 0; i < count; ++i) if (i != seat && peers[i].role == Role::Child && peers[i].aid > 0 && (mask & (1u << peers[i].aid))) {
            auto b = Packet(DisconnectPeer); b.push_back(0);
            if (!Queue(i, std::move(b))) return false;
            disconnected |= 1u << peers[i].aid; allocated &= ~(1u << peers[i].aid);
            peers[i].role = Role::Off; peers[i].aid = 0; acceptedNonces[i] = 0;
        }
    }
    return true;
}
bool Core::Send(std::uint16_t mask, unsigned port, const std::uint8_t* bytes, unsigned size)
{
    if (!active || port >= 16 || size > MaxData || (size && !bytes)) return false;
    const auto& local = peers[seat];
    std::vector<unsigned> recipients;
    for (unsigned i = 0; i < count; ++i) if (i != seat) {
        const auto& p = peers[i];
        if (((local.role == Role::Parent && p.role == Role::Child && p.aid > 0)
            || (local.role == Role::Child && i == joinParent && p.role == Role::Parent)) && (mask & (1u << p.aid))) recipients.push_back(i);
    }
    if (recipients.empty() || frames.size() + recipients.size() > 128) return false;
    auto b = Packet(Data); b.push_back(local.aid); b.push_back(static_cast<std::uint8_t>(port));
    b.push_back(static_cast<std::uint8_t>(size >> 8)); b.push_back(static_cast<std::uint8_t>(size));
    if (size) b.insert(b.end(), bytes, bytes + size);
    for (auto i : recipients) Queue(i, b);
    return true;
}
bool Core::Receive(unsigned sender, const std::vector<std::uint8_t>& b)
{
    if (!active || sender >= count || sender == seat || b.size() < 5 || b.size() > MaxData + 16
        || b[0] != 'C' || b[1] != 'W' || b[2] != 'M' || b[3] != 2 || events.size() >= 128) return false;
    auto& remote = peers[sender]; auto& local = peers[seat];
    switch (b[4]) {
    case Info: {
        if (b.size() < 27 || b[5] > 2 || b[6] >= 16 || b[26] > 112 || b.size() != 27u + b[26]) return false;
        const ParentInfo parent{U16(b,15),U16(b,17),U16(b,19),U16(b,21),U16(b,23),b[25]};
        if (!ValidParent(parent)) return false;
        Role role = static_cast<Role>(b[5]);
        if (role != Role::Child && b[6] != 0) return false;
        // A child cannot assign itself an AID. Parent/child associations are
        // established exclusively by the request/response handshake below.
        if (role == Role::Child && remote.role == Role::Child && remote.aid != b[6]) return false;
        if (role == Role::Off && remote.role != Role::Off) Lost(sender);
        if (role == Role::Parent) {
            if (remote.role == Role::Child) return false;
            remote.role = role; remote.aid = 0;
        }
        remote.ggid = U32(b,7); remote.tgid = U32(b,11); remote.parent = parent;
        remote.gameInfo.assign(b.begin()+27,b.end()); return true;
    }
    case JoinRequest: {
        if (b.size() != 33 || local.role != Role::Parent || !U32(b,5)) return false;
        const auto nonce = U32(b,5);
        const bool duplicate = remote.role == Role::Child && remote.aid > 0 && acceptedNonces[sender] == nonce;
        if (remote.role == Role::Child && !duplicate) return false;
        unsigned occupied = 0;
        for (unsigned i = 1; i < 16; i++) if (allocated & (1u << i)) ++occupied;
        if (!duplicate && occupied >= local.parent.maxEntry) {
            auto response = Packet(JoinResponse); U32(response,nonce); response.push_back(0);
            return Queue(sender,std::move(response));
        }
        unsigned aid = duplicate ? remote.aid : 1;
        if (!duplicate) while (aid < 16 && (allocated & (1u << aid))) ++aid;
        if (aid >= 16) return false;
        auto response = Packet(JoinResponse); U32(response,nonce); response.push_back(static_cast<std::uint8_t>(aid));
        if (!Queue(sender,std::move(response))) return false;
        if (!duplicate) {
            allocated |= 1u << aid; remote.role = Role::Child; remote.aid = static_cast<std::uint8_t>(aid); acceptedNonces[sender] = nonce;
            events.push_back({EventKind::Join,static_cast<std::uint8_t>(aid),0,{b.begin()+9,b.end()}});
        }
        return true;
    }
    case JoinResponse: {
        if (b.size() != 10 || sender != joinParent || remote.role != Role::Parent || U32(b,5) != joinNonce || b[9] >= 16) return false;
        if (b[9] == 0) {
            if (local.role != Role::Off) return false;
            joinParent = MaxPeers; joinNonce = 0; return true;
        }
        if (local.role == Role::Child) return local.aid == b[9];
        if (local.role != Role::Off) return false;
        if (frames.size() + count - 1 > 128) return false;
        local.role = Role::Child; local.aid = b[9]; Advertise(); return true;
    }
    case DisconnectPeer:
        if (b.size() != 6 || !((local.role == Role::Parent && remote.role == Role::Child && b[5] == remote.aid && b[5] > 0)
            || (local.role == Role::Child && sender == joinParent && remote.role == Role::Parent && b[5] == 0))) return false;
        {
            const bool parentStillPresent = local.role == Role::Child;
            Lost(sender);
            // A kick severs this association; the parent's advertised session
            // still exists and must remain available to a later scan/rejoin.
            if (parentStillPresent) remote.role = Role::Parent;
            return true;
        }
    case Data: {
        if (b.size() < 9 || b[6] >= 16 || b.size() - 9 > MaxData || ((unsigned(b[7]) << 8) | b[8]) != b.size()-9) return false;
        if (!((local.role == Role::Parent && remote.role == Role::Child && remote.aid > 0 && remote.aid == b[5])
            || (local.role == Role::Child && sender == joinParent && remote.role == Role::Parent && b[5] == 0))) return false;
        events.push_back({EventKind::Data,b[5],b[6],{b.begin()+9,b.end()}}); return true;
    }
    default: return false;
    }
}
std::vector<Frame> Core::TakeFrames() { std::vector<Frame> result; result.swap(frames); return result; }
std::vector<Event> Core::TakeEvents() { std::vector<Event> result; result.swap(events); return result; }
}
