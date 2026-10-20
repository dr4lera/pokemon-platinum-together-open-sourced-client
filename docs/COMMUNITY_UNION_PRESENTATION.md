# Native Union Room presentation

This unit displays the native chosen Union Room appearance and authenticated
community account nametags on the room's existing player objects. It does not
spawn actors, change positions, set save flags, write trainer data, or add a
second gameplay synchronization system. Link battle and trade continue through
the engine's existing wireless lifecycle.

## Identity and appearance

`community_union_presentation.cpp` reads the active community link and lobby
snapshots. A signed immutable roster MAC is bound to its exact account player ID
and current authenticated party member name. Local seat is excluded. Seats may
differ from DS AIDs and trainer IDs; neither AID nor trainer ID nor display name
is used as an account identifier. Duplicate display names remain separate peers.
Inactive, cancelling, offline, reordered, malformed or mismatched snapshots
produce no authenticated bindings. Installing a community provider also filters
the room's beacons to the bound remote MACs. Legacy LAN discovery remains usable
without community account nametags when no community provider is installed.

The native game advertises the chosen trainer appearance in its native trainer
profile. The exact native allowlist from `src/appearance.c` is:

| Gender | Raw native sprite graphics IDs |
| --- | --- |
| Male | 3, 5, 11, 31, 50, 51, 62, 70 |
| Female | 6, 7, 13, 14, 35, 37, 42, 63 |

Other values use the matching native default (3 or 6). Invalid gender uses 3.
Companion profiles use their existing gender-bit/native-appearance encoding.
Their appearances are validated too, but companion trainer IDs never grant an
authenticated account nametag. A changed appearance is refreshed only when the
existing actor is ready; a busy animation defers the update until a later tick.
Together `look` and `outfit` IDs are opaque and are not mapped to these graphics
IDs.

## Label ownership and teardown

`community_union_labels.c` borrows at most three existing native remote objects
from local IDs 1 through 10. It owns a bounded FontOAM pool, sprite list, one
palette transfer task, and each label's character transfer range. No global OAM,
character transfer, or palette manager is reset. Labels convert the backend's
1–10 ASCII name contract to actual native character codes, including explicit
EOS; raw ASCII is never treated as native character codes. Each name uses a
128-by-16 offscreen window and is projected above the native actor under the
field's current camera/projection. Hidden, non-drawable and offscreen objects
hide their labels. Names are centered and clamped to the main screen.

The owner field pointer, authenticated connection generation, link session UUID,
and roster epoch must remain the same. Each render rechecks that identity and
the current object occupying the borrowed local slot before dereferencing it.
A missing scan binding, reused slot, cancelled/disconnected link or changed
generation deletes the old label. Room task destruction and field exit both
release labels before the field's graphics managers and heap are destroyed.
Destruction is idempotent. Returning to the same field address starts without
old borrowed pointers or graphics resources.

`MapObjectMan_SaveAll` saves every active native object, regardless of the
persistent flag. This unit therefore never creates a remote map object and never
tries to make a new actor safe by changing that flag.

## Validation

Run `python tests/run_community_union.py` after configuring the native build.
The portable C++17 fixture uses strict warnings and checks all 16 appearances,
name/code bounds and output canaries, MAC/account binding, duplicate names,
nonzero local seat, malformed membership and cancellation. The actual C label
controller runs against the actual engine and SDK header declarations with
disposable renderer endpoints. It checks visible/hidden/clipped labels, name
refresh, slot reuse, omitted bindings, ownership/generation changes, cancellation,
idempotent destruction, graphics teardown order and refused allocations. All
three remote labels are exercised together. An additional fixture extracts and
executes the actual room actor functions against the real SDK and engine types,
checking all 16 choices, deferred appearance refresh while animation is busy,
invalid appearances, truncated/unbound beacons and companion encoding. All
four integration sources also compile against the configured actual SDK headers
without writing to the shared build. These checks do not claim visible native
game verification; that requires the full build and a two-client room check.

## Separate pending work

The [custom PTTR asset unit](COMMUNITY_TRAINER_ART.md) now provides semantic
palette/frame decoding and verified bounded hash downloads. Live installation
still needs confirmed native direction/pose binding, per-actor graphics
ownership and generation-safe installation. The current server only validates
the structural envelope. Custom art is not silently treated as an arbitrary
native sprite ID.

Inverna presence is separate from native Union Room actors. Its existing opaque
eight-byte pose route has no source-defined position/frame semantics in this
client. Visible plaza movement requires an explicit authenticated pose contract,
a separate transient scene and unconditional return to the retained world
location. The native room's SpecialLocation return slot must not be reused for
that scene. Avatars and nametags alone do not implement world co-op.
