# Native link transport adaptation

The community engine already contains Platinum's local wireless battle and
trade applications. The practical integration boundary is its simulator WM
transport, rather than a second implementation of Pokémon exchange or battle.
This inspection uses the public engine/runtime and this project's backend;
it does not use the installed original executable or private reference files.

## Existing path

- `CommManager_StartUnion` / `CommManager_ConnectUnion` start Union Room
  discovery and connection. Battle rooms additionally use
  `FieldCommManager_StartBattleServer` / `StartBattleClient`.
- `src/field_comm_manager.c` prepares a native link battle DTO, then calls
  `Encounter_NewVsLinkWithRecording` (or its party variant).
- `src/unk_0207A6DC.c` registers and implements native battle handshake,
  trainer, party and command transfers. `src/battle/battle_main.c` drives the
  link communication screen and synchronization.
- `src/applications/trade_room/comm.c` exchanges party, confirmation, cursor,
  Pal Pad, ribbons and Chatot data through `CommSys`.
- `TradeRoom_ExecuteTrade` updates the actual party, preserves Pokémon
  provenance through the existing trade routine and updates catch records.
  The field task in `src/unk_0203D1B8.c` runs the trade cutscene/evolution and
  returns to the room. The existing sync-save state machine is in
  `src/unk_02038ED4.c` (sync points 111 and 112).
- `src/wireless_manager.c` turns `CommSys` messages into WM MP port traffic.
  The simulator's ARM7 WM calls `SIM_Net_SetGameInfo`, `Scan`, `JoinMP`,
  `SetMPState`, `SendMPData` and `DisconnectMP`.

The community level-cap, obedience and capture changes remain in the engine.
A link transport must not replace those rules or rewrite traded OT identities.

## Existing transport and limitations

The inherited simulator uses ENet reliable packets on UDP port 2009 with two
channels (management and MP data). It transmits native C structures headed by
network-order `NTR0` and protocol version zero. Its wire layout is not the
backend's WebSocket protocol. It permits sixteen simulator nodes; the custom
party backend currently permits four authenticated members.

Simulator node ID, backend party seat and DS AID are distinct. Any node can
advertise itself as a DS MP parent; that parent uses AID zero and allocates
child AIDs 1–15. Mapping a party leader's seat directly to every DS AID would
break role changes and discovery.

The current backend's original reference relay uses two-byte routing headers
and an opaque payload, but its allocation is explicitly a persistent world
session with city restrictions. Native link play should use its own negotiated
session protocol, not silently advertise a world or original-client support.

## Completed bounded runtime hardening

The runtime now rejects truncated MP records, invalid ports/lengths/AIDs,
negative or out-of-range node IDs, malformed player metadata and peer identity
claims that disagree with their established connection. Player lists are
accepted from the established ENet host and keep native node IDs without an
incorrect byte swap. Repeated join requests retain the assigned child AID.
Child disconnect actually changes its MP state to off.

Received ENet packets have one release path, including unknown/malformed
packets. MP payload and join-SSID storage stay alive until WM acknowledges
its callback. Unqueued outgoing packets are freed, and failed/no-recipient
MP sends return failure instead of reporting a throwaway success. The ARM7
WM send callback maps transport failure to `WM_ERRCODE_SEND_FAILED`.

### Receive ownership contract

`WMSP_Indicate` places a pointer to borrowed data in a WM FIFO callback; it
does not copy that data. `WMSP_ReturnResult2Wm9` returns before the ARM9
callback necessarily runs. The ARM9 WM callback invokes the registered
consumer, then clears `WM_EXCEPTION_CB_MASK`.

The existing network worker therefore delivers as follows:

1. Retain the incoming packet and call `WMSP_Indicate`.
2. Call `WMSP_Wait4Wm9` to wait for that callback's completion flag.
3. Release the packet only after the wait returns.

The no-port-callback path still acknowledges the FIFO callback. A future
provider must preserve this ownership contract for its own buffers too.

## Remaining native acceptance

Semantic provider hooks and authenticated session negotiation are implemented
below. Outstanding native acceptance requires two disposable own-client instances
to complete a battle and a trade, persist the received Pokemon, and exercise
cancel/recovery. Runtime shutdown while a WM callback awaits acknowledgment also
requires explicit verification. The inherited ENet host remains a separate local
transport; its bounds fixes do not establish Internet transport safety.
Modern species compatibility and Together overworld/co-op behavior remain
separate acceptance items.

## Evidence

`python tests/run_community_wireless.py` compiles the actual runtime
`sim_net.cpp` against disposable ENet/SDL/WM fixtures and executes its real
receive/send call sites. It checks malformed input, source binding, valid
delivery, callback → acknowledgment → release order (including no consumer),
duplicate joins, child state transitions and zero-recipient/failed-send
cleanup. It also compiles the actual ARM7 `WMSP_SetMPData` function and checks
its success, failure, inactive-MP and no-recipient callback results.

These tests passed locally. They are parser/lifecycle fixture evidence,
not a live WM simulation, native gameplay verification or a full client build.

## Own-client semantic provider

`community_wireless_core.cpp` now implements a bounded, fixed-width `CWM2`
protocol behind an explicitly installed runtime provider. Account/party seats
route messages through the signed Service link; DS parents allocate child AIDs
independently. Discovery metadata, parent/child join with24-byte SSID, MP ports,
disconnects and rejoining retain the native WM contract. No raw ENet structure
crosses this transport. Role/metadata transitions refuse insufficient queue
capacity before mutating state, and each generation starts with fresh identities.

`community_wireless.cpp` supplies the actual WM callbacks, preserves metadata
advertised before transport selection, keeps owned receive bytes alive through
ARM9 acknowledgment and releases its mutex before that wait. Queued callbacks
recheck the current session before delivering more data. Cancellation clears the
session and its queues; game-link reconnection requires fresh preparation.
Gameplay settings in the GUI are locked while prepared/connected. Compatibility
fingerprints include the effective catalog, cap, gameplay settings and rule
revision; source changes to semantics must also change the compatibility revision.

`python tests/run_community_wireless_core.py` passes four-peer state-machine
checks, including a parent at seat2, independently assigned AIDs, duplicate joins,
malformed inputs, two-way data, disconnect/rejoin, queue pressure and teardown.
`tests/community_wireless_provider_run.ps1` compiles the actual provider/Core/
Service against the real backend with only SDK recipients/config/catalog stubbed.
It passes dynamic parent discovery/join,512-byte bidirectional data, callback
lifetime, provider access during acknowledgment, fresh epochs/MACs and a paused
two-packet batch canceled while the first callback waits. The second old packet
is discarded. These fixtures use no game assets, original saves or OS input.

The complete Windows client links with this adapter and exposes prepare/open/
close controls under Community servers. Enable `communityLink.enabled` only on
a combined backend. After all party members prepare and the leader opens, the
native Pokemon Center Union Room is the intended game entry point. Two actual
games completing a battle/trade and their save/recovery checks remain pending.

## SDK discovery correction

Provider ABI2 and the `community-wm-v2` compatibility revision carry real parent
packet limits, channel, beacon period, entry count and attributes. SDK beacon
payloads are copied as bytes into the actual `u16[56]` field. The scan regression
uses the actual SDK declarations and fails the previous widening copy. The
loopback provider fixture now compiles against the real native SDK headers too.

Runtime ordinary scan writes one descriptor, rotates matching parents and
publishes full bounded callbacks. ScanEx respects output extent and filters;
community discovery excludes the synthetic legacy access point. Connect sets
actual parent MAC and MP sizes. EndMP retains the association role; EndParent
withdraws the beacon. Full-room association refusal leaves the authenticated
session available for a later retry. The isolated actual-SDK runtime fixture
covers these boundaries, including output canaries; two-game activity acceptance
remains separate.
