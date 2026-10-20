# Own-client accounts, parties, chat and link transport

`community_online.cpp` is an original Windows C++17 adapter for the project's
existing private-backend account and lobby protocols. It uses libcurl WebSockets,
OpenSSL Ed25519/SHA-256 and pinned RapidJSON headers. It does not use the
foundation's ENet/Nitro wireless transport, reference-client code or game saves.

## Game-facing interface

Include `port/community_online.h` and own one `CommunityOnline::Service` for the
application's online session. Fill `Options.serverUrl`, explicitly trusted
`serverKey` (canonical base64 raw Ed25519 key), dedicated `profileDirectory` and
new-account `username`. The default build identifies this own-client protocol.
Never impersonate a reference game's release ID.

`Connect(options)` starts a worker and returns false if an earlier worker is
still active. `Read()` copies a bounded snapshot for rendering. `CreateParty()`,
`JoinParty(code)`, `LeaveParty()` and `SendGlobalChat(text)` return whether the
command entered the bounded queue, not whether the server applied it. Display
server snapshots/echoed chat as confirmation. Only enable those actions when
`State::Ready`. `Disconnect()` requests asynchronous shutdown; wait for
`State::Stopped` before connecting again. Destruction joins the worker. Call
session lifecycle methods from one owning game/UI thread; reads and queue
operations synchronize with the worker.

The snapshot contains connection/error state, authenticated player/username,
party code/members, recent global messages and the count of successful lobby
authentications. `StateName()` covers the UI labels, including `Stopping`.
Keepalive defaults are a 20-second ping interval and 10-second matched-pong
timeout; both option values accept 1,000–60,000 milliseconds. A successfully
written ping or an unrelated incoming message does not satisfy that deadline.
No callback touches `FieldSystem`, `Pokemon`, a renderer or `SaveData`. The
existing project GUI can call this API directly. A native game menu still needs
its own registration and layout; this adapter does not supply a placeholder UI.

## Ephemeral link API

The same service implements the server's optional `community-link` protocol1
on a second independently polled WebSocket worker. Account/profile and lobby
operations continue on the original worker. There is no game/save pointer or
wireless callback in either worker. Link connection/proof waits cannot suspend
lobby keepalive processing. The second worker owns its separate Ed25519 key
object, cloned privately from the protected account and cleansed on destruction.

`ReadLink()` returns `LinkSnapshot`: availability, `LinkState`, cancellation acknowledgment pending (`cancelling`), error, session,
epoch, party, exact build/catalog/rules fingerprints, roster hash, expiry, local
seat, request ID and roster `{seat,player,mac}`. It exposes no tickets, account
keys or seed. A seat is a routing node; the DS parent/child AID is independent.

After normal party membership is established and `available` is true:

1. Every participant calls `PrepareLink(catalogHash,rulesHash)`, using canonical
   lowercase SHA-256 hashes. Wait for `Prepared` on each adapter. The own build
   must match the authenticated lobby build and the server's own-build format.
2. The party leader calls `OpenLink(requestId)`, or omits the ID to generate a
   fresh UUID. The saved request ID can be supplied explicitly for an idempotent
   retry while `Opening`. Guests automatically attach a valid offer after their
   explicit preparation; preparation is consent for this party's next allocation.
3. Wait for `Active`. `Connecting`, `Authenticating` and `Waiting` do not permit
   wireless frames. Descriptor validation checks the entire roster, its canonical
   hash, derived MACs, fingerprints, own key/seat, exact origin and signed proof.
4. `SendLink(destination,payload)` queues one owned binary message to a roster
   seat or 255 for broadcast. `ReceiveLink(LinkPacket&)` transfers one owned FIFO
   payload with authoritative sender seat, session and epoch. Each payload is
   1–8192 bytes. Both queues are capped at 128 packets and 256 KiB; outgoing totals
   include transport headers. The worker paces packets below backend frame/byte
   limits. A false enqueue return means the caller must handle backpressure.
5. `CancelLink()` immediately disables local traffic and clears both queues,
   then requests server cancellation after previously queued lobby commands,
   including when only preparing/prepared and no session has been allocated.
   The backend withdraws that exact lobby socket's prepared fingerprint and
   replies `community.link.cancelled`, even without a party/session. Repeated
   cancellation is idempotent. `cancelling` remains true until that acknowledgment;
   a session-ended event alone does not release the new-prepare gate.
   Cancellation has a separate control slot so a full ordinary command queue
   cannot suppress it. A fresh preparation may return false while cancellation,
   the previous socket's release, or a previous prepare acknowledgment is pending.
   Retry through the owning UI/provider loop after those operations settle.

`Ended` and `Failed` stop traffic. Link disconnect, peer/party change, lobby loss,
malformed frames, receiver queue overflow and missed matched pongs clear pending
frames; there is **no automatic link reconnection or binary replay**. Lobby
reconnection can restore account/party/chat, but new link use requires explicit
preparation and a fresh allocation. A duplicate offer for the same generation
does not reopen it. `LinkStateName()` labels all states.

The wireless semantic provider must validate inner AIDs/group/port/length claims,
map routing nodes to current DS roles, and retain received packet storage through
actual ARM9 acknowledgment. Once a packet has been transferred by `ReceiveLink`,
its caller owns it; check its session/epoch against current link state before
dispatching delayed callbacks and cancel held callbacks on any end. This API
does not implement those native wireless semantics or a save/trade transaction.

## Identity, transport and storage

Only HTTPS origins are accepted remotely. HTTP is limited to literal loopback
addresses. Redirects, proxy discovery and URL credentials/query/fragment are
disabled/refused. TLS certificate and hostname verification remain enabled.
Windows native CA trust is enabled explicitly instead of depending on an MSYS2
CA bundle pathname. Packaged trusted HTTPS/WSS still needs its release check.
The server's advertised account key must equal the supplied trusted pin; the
adapter never silently trusts a discovered key.

Each dedicated directory owns `community-online.json` and its exclusive lock.
This schema is separate from launcher/reference profiles. Keys are generated on
the worker; the seed is protected with Windows DPAPI for the current user. A
pending key profile is flushed and atomically promoted before registration, so
an ambiguous registration can retry/login with that same identity. Updates use
flushed temporary files and write-through replacement. Existing unknown profile
schemas, different server identities, damaged protected secrets and concurrent
use are refused. Tokens remain in memory. No original profile or save path is
opened, migrated or rewritten.
If a pending registration's name is taken, a newly entered valid username is
saved with the same key before retrying. Settled accounts keep their server
identity; this is not an account rename feature. Choose a distinct dedicated
profile directory for each normalized server origin and trusted pin.

The adapter validates the server-signed 58-byte token, key/player identity and
lifetime before signing the `PT2-LOBBY` challenge. It handles actual signed
`PT2-RENEW` renewal and manual/automatic same-key reauthentication. Transport
reconnects use bounded backoff; queued actions are discarded across a failed
connection rather than silently replayed. Party snapshots and chat history are
requested/restored by the actual backend connection flow. Server refusal codes
remain visible. JSON, WebSocket assembly, queues, party size and chat history
have explicit bounds; duplicate JSON object keys and invalid UTF-8 are refused.
Nesting is checked lexically before DOM allocation, with an iterative JSON
reader as a second guard. This also bounds cleanup of rejected nested input.

## Build integration

For the win64 gameplay target, add `src/port/community_online.cpp`, the existing
RapidJSON include dependency and distinct dependencies for system `libcurl`,
OpenSSL `libcrypto`, Windows `crypt32` and C++ threads. Use scoped Meson variable
names to distinguish OpenSSL from the foundation's existing Nitro crypto libs.
The inspected runtime is libcurl 8.17.0 with `ws`/`wss`, OpenSSL 3.6.0 and the
already-pinned RapidJSON 1.1.0 headers. libcurl WebSocket support must be enabled;
headers alone are insufficient. Windows runtime DLL packaging remains a separate
release requirement. This implementation intentionally has no ARM/Linux DPAPI
fallback or insecure plaintext-seed mode.

## Real backend verification

The standalone executable compiles the **same adapter** used by the game-facing
API. Its Node runner imports the project's actual `Hub`, binds an ephemeral
loopback port and generates a new disposable server plus two own-client
accounts. Supply a fresh private fixture directory outside this checkout:

```powershell
./tests/community_online_run.ps1 `
  -BackendRoot 'D:/Source/plat-together-server' `
  -FixtureDirectory 'D:/PrivateFixtures/community-online-001' `
  -NodePath 'D:/Runtime/node.exe'
```

Verified locally: genuine account registration and signed lobby proofs, distinct
protected keys, party create/join/leave/presence, global chat both directions,
automatic reconnect after server close, manual reconnect with stable player ID,
exclusive profile use, wrong pin and damaged-secret refusal. The fixture mints
authentic nearly-expired tokens to exercise the real renewal handler immediately
and fragments chat frames to check actual WebSocket reassembly. It suppresses a
matched application pong while delivering unrelated control pongs to verify
timeout/reconnection, sends 30,000-level JSON
to verify a clean refusal, checks 480-byte Japanese chat plus invalid/over-limit
UTF-8, and retries a taken pending name without changing the staged public key.
Two primary accounts and one explicit pending-name recovery account remain;
reconnections create no duplicate identities. A separate self-signed loopback
TLS server is refused before any HTTP request or profile creation, without
installing its certificate or changing Windows trust. The test uses no game assets,
UI automation, original accounts/saves or public service.

With the real backend link feature enabled, the same two native adapters also
verify signed second-WebSocket joins, immutable roster/seat/MAC validation,
broadcast/direct authoritative sender rewriting, an 8192-byte payload fragmented
across WebSocket frames, bounded outgoing admission and FIFO delivery, explicit
cancellation, immediate cancellation while opening, and fresh session/epoch
allocation. Losing the bound lobby ends both old links without binary replay.
Prepared-only cancellation prevents a leader allocating the withdrawn guest;
canceling a queued prepare retains the gate through its ordered acknowledgment.

The adapter and Community servers UI are registered in the complete Windows
client, which compiles and links. The semantic WM provider is a separate engine
adapter; see `COMMUNITY_WIRELESS.md` for its actual two-adapter fixture. Rendered
online UI, packaged HTTPS behavior and two actual game battle/trade flows remain
acceptance checks. World co-op, Inverna actors, save migration and full Together
parity remain separate work. Global chat is not party-private messaging.

## Trainer profile and plaza presentation

`Snapshot::trainer` is the authenticated account's server trainer profile.
`RefreshTrainer` and `UpdateTrainer` use the actual `trainer.get` / `trainer.put`
contract: ASCII names up to 10 bytes, look 0–255, outfit 0–65535,
`tutorial_done`, and the server's optimistic revision. An update sends the
caller's revision unchanged. A conflicting edit returns the latest profile
with `conflict=true`; it never overwrites the newer edit or retries silently.
Only one profile request can be outstanding. The GUI preserves an edited draft
until save succeeds or the user chooses the latest profile. Profile values are
separate from native trainer/save data. `loaded` distinguishes an unanswered
request from an explicit `trainer:null`: only the latter permits revision-zero
profile creation, which also supports owner-enrolled accounts.

Party members and city players now carry the server's exact look/outfit IDs
and validated optional `{creation,version,template,hash}` custom-art reference.
These IDs are **opaque Together appearance identifiers**. They are not vanilla
Appearance indices or raw map-object graphics IDs. The GUI displays the values
and metadata, with explicit unsupported-rendering labels. This unit does not
download, decode, install or render PTTR art and does not offer an art-selection
UI: `trainer.art` has no acknowledgement and `trainer.get` omits the selected
reference. An acknowledged owned-collection workflow is a separate unit.

`EnterCity`, `LeaveCity`, and `SetCityStatus` use the authenticated `plaza`
social routes. `Snapshot::city` reports Absent/Entering/Present/Leaving, the
authenticated connection generation, local server slot, and at most 63 other
socket profiles. Actors are keyed by slot within that generation: multiple
sockets for one player remain distinct. A same-slot profile refresh replaces
presentation metadata, preserving its server status; no game position exists
in this snapshot. Unknown status/drop slots do not create actors.

Leaving immediately hides the roster and gates reentry until `city.left`.
It also cancels an in-flight enter: a late joined response cannot restore
presence while Leaving. The leave request has a dedicated flag outside the
bounded ordinary command queue, and party/invitation/link activity never gates
it. Profile and enter/leave replies have a 15-second deadline; missing replies
fail the connection rather than leaving an indefinite pending UI. Disconnect,
failure, and reconnect clear all presentation state. A new lobby connection
does not automatically reenter the city or replay old mutations.
An enter error while Leaving preserves the gate until the queued leave's ack,
so an old ack cannot cancel a newly requested entry. Receive handlers check
the stop flag under their publication lock and cannot repopulate presentation
after concurrent disconnection.

The existing backend's city position envelope is recognized only as the exact
bounded `[2,1,slot,8,eight opaque bytes]` framing, including fragmented delivery.
Its payload is ignored. No invented coordinates, scene warps, map objects,
nametag overlays, save writes or shared-world gameplay are introduced. Joining
plaza presence in the GUI does **not** enter an Inverna map. Native Union Room
actors and their wireless lifecycle remain separate.

Run the isolated actual-adapter / real-Hub fixture with a fresh private path:

```powershell
./tests/community_presentation_run.ps1 `
  -BackendRoot 'D:/Source/plat-together-server' `
  -FixtureDirectory 'D:/PrivateFixtures/community-presentation-001' `
  -NodePath 'D:/Runtime/node.exe'
```

It verifies valid and refused profile fields, optimistic revision conflict,
party and same-slot city appearance refresh, authenticated owned-art metadata,
status changes, fragmented opaque position framing, two real authenticated
sockets for the same account in separate slots, leaving with queued activity,
canceling a pending enter, fresh connection generation after reconnect, explicit
reentry, confirmed absent-profile creation, malformed appearance rejection and
cleanup. A deterministic actual-handler test also covers stop/publication races
and enter-error/cancel acknowledgement ordering. It copies only a newly
generated fixture's protected account file to exercise the same-account case;
no original profiles, saves, assets or public services are involved. UI rendering
and actual Inverna/world integration remain separate acceptance work.
