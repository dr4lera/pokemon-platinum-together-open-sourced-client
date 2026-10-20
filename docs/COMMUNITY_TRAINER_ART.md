# Custom trainer pack decoder

`community_trainer_art.cpp` is an original portable decoder for the native PTTR
container. It owns validated pixels and exposes no file, network, renderer or
native graphics-ID mutation. This unit does not yet install custom art on a live
actor. The server's structural acceptance alone does not mean a pack contains
valid renderable pixels.

## Established format

The selected reference is 0.1.7, SHA-256
`3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9`.
The independently inspected 0.1.10 artifact is SHA-256
`45185bc3f1849c6b93910efef451ea70cd7a03fd81dd4733e736eae65ed0b4d8`.
Both exported validators establish the same rules listed here. Static exported
analysis is evidence about the format; it is not a runtime compatibility test.
No recovered function bodies, reference executable bytes, or artwork are bundled
in this unit. Tests construct their own complete synthetic artwork.

PTTR has a 64-byte header and one to six 12-byte section table entries. Total
length is exact and at most 2 MiB. Versions 1 and 2 are supported. Template is
`platinum-trainer-209-v1`, followed by zero header padding. Gender is 0 or 1.
Version 1 reserves the two bytes at offset 6 as zero; version 2 permits fallback
selector 0, 1 or 2 in byte 6 and reserves byte 7 as zero. These values are not
Together's opaque `look` or `outfit` IDs.

Sections must occur once, in this order, contiguously without gaps or trailing
bytes. PAL, FELD, FRNT and TILE are required. POSE and BIG are optional.

| Tag | Payload and semantic validation |
| --- | --- |
| `PAL ` | Little-endian u16 count 2–256, reserved u16 zero, then count RGBA colors. Color 0 is exactly transparent black; all other colors have alpha 255. Exact size is 4 + count × 4. |
| `FELD` | Exactly 36 × 1024 row-major palette-index bytes: 36 frames of 32 × 32 pixels. Every index is below the palette count. |
| `POSE` | u16 count 1–256, reserved u16 zero, then count records of u32 native pose CRC32 key and 1024 palette-index bytes. Keys strictly increase; frames are nonempty and indices valid. |
| `FRNT` | Exactly 6400 RGBA pixels (25,600 bytes). Pixels are opaque or exactly transparent black. |
| `BIG ` | Exactly 8192 RGBA pixels (32,768 bytes), under the same alpha rule. Its dimensions remain unassigned by this decoder. |
| `TILE` | Exactly 26,976 bytes: seven native packed image blobs, each immediately followed by 16 little-endian RGB555 colors. Image byte lengths are 4096, 4096, 4096, 4096, 4096, 3200, 3072. Palette bit 15 must be zero. |

Fallback selector zero requires all 36 FELD frames and the FRNT image to be
nonempty. Selectors 1 or 2 allow empty FELD/FRNT slots for the original native
fallback. POSE frames and an included BIG image remain nonempty. TILE payloads
may be empty, because the original consumer handles those with native fallback.
The decoder returns missing/empty frame status; it does not invent fallback
artwork or a selector-to-look mapping.

Native consumers select explicit POSE CRC32 matches before looking up the base
field atlas. A POSE key is not an arbitrary frame number. The original base
atlas selector uses three groups of 12 slots, each split into four directions
and three phases. The precise native direction/animation mapping must be bound
to this engine's actual actor resources before a renderer hook is added.
TILE views preserve verified byte boundaries and RGB555 colors; they do not
reinterpret arbitrary native blobs as pointer-bearing SDK structures.

## Ownership and integration boundary

Decode rejects malformed input before publishing its result and copies all
validated data into a single owned pack. The caller can discard the input
buffer immediately. Returned TILE pointers borrow that pack; they expire with
it. Field and POSE conversion writes exactly one 4096-byte RGBA frame and clears
the output on a missing/empty frame. No borrowed source pointer survives decode.

`Service::RequestTrainerArt(player, art)` authorizes only the current active
signed link roster member's exact authenticated party art reference. It returns
a monotonic request ID or zero on rejection. The separate content worker requests
only `/api/creators/v1/packs/<lowercase-sha256>.pttrainer` at a copied canonical
server origin. This actual backend content route is public; no account key or
signature is copied to the worker. Each transfer owns its CURL handle, verifies
TLS peer and host, disables redirects and proxies, enforces a 2 MiB declared and
streamed body limit, and has an 8-second connect and 15-second total deadline.
SHA-256 must match the authenticated reference before semantic decoding.

`ReceiveTrainerArt` returns an immutable owned decoded pack or a bounded local
error, together with request ID, connection generation, session UUID, epoch,
player and exact art reference. It rechecks that entire context under the Service
publication lock. Eight queued/active/completed requests are permitted in total;
unconsumed results retain that budget. There is no unbounded persistent cache.
Link end, disconnect, reconnect and changed/missing party art cancel old jobs
and clear results. Cancellation signals atomics and removes queued results;
Disconnect never joins a transfer under the Service lock. The content worker's
destructor joins outside that lock. It retains no Service or game pointers.
Its HTTP work cannot block the lobby or gameplay link workers.

The renderer must additionally match its own current field instance, actor slot
and request ID before installing a returned pack on the main thread. This asset
unit does not introduce that renderer hook or a custom-art selection UI.

One remote custom actor cannot overwrite the shared native graphics-ID cache:
that cache may also be used by other players and NPCs. Installation needs owned
per-actor texture/palette state and teardown before field graphics destruction.
Existing native actions and world positions remain authoritative. Custom art
does not authorize spawning persistent objects, changing positions or altering
battle/trade state.

## Checks

Run `python tests/run_community_trainer_art.py`. It compiles the actual portable
decoder with C++17 and `-Wall -Wextra -Werror`. Complete original synthetic packs
exercise 2- and 256-color palettes, zero and 256 extra CRC poses, optional BIG,
all seven TILE views, copied ownership, output canaries, sparse version-2 modes,
bounded headers/sections, alpha and index errors, duplicate poses, malformed
RGB555 colors and deterministic input mutations. Neither this fixture nor the
backend's structural tests claim visible native custom-sprite verification.

Run `tests/community_trainer_art_download_run.ps1` with `-BackendRoot`, a fresh
private `-FixtureDirectory` outside the checkout, and Node.js 24 `-NodePath`.
The actual Service/real backend fixture exercises signed accounts, party and
community link binding, owned art selection, the public content route, valid
decode, slow transfer with concurrent lobby ping and link binary delivery,
declared/chunked oversize, redirected content, incorrect digest and semantically
invalid content, bounded admission, profile-change cancellation and nonblocking
shutdown. A deterministic actual-implementation fixture checks connection,
session, epoch, reference, roster drop and expiry gates, cancellation and a stop
that races a waiting result publisher. Existing online, presentation, provider
and QA runner commands link both new content sources.
