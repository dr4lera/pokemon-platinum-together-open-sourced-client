# Source milestone checks

On 2026-10-20, the community client linked successfully as a Windows x64
`build_win64/main.exe` using GCC 15.2.0, Meson 1.12.0 and the dependency revisions
in `dependency-lock.json`. This is compilation evidence, not a gameplay test.

The real engine compile caught an incompatible pointer type in our capture-turn
mask: the SDK's `u32` is `unsigned long`, whereas the portable transaction helper
uses `unsigned`. The new internal context field now uses the helper's exact
type; no cast or aliasing workaround is used.

Checks run:

```text
python tests/run_community_rules.py
  community level-cap rules: passed
  community level-cap configuration: passed
python tests/run_community_learnsets.py
  community learnsets: decoder, bounded consumers and actual Move Reminder passed
gcc -std=c99 -Wall -Wextra -Werror -Iinclude tests/capture_rules_core_test.c ...
  capture rules core: target eligibility and transactional debit checks passed
ninja -C build_win64 -j 8 main.exe
  Linking target main.exe: succeeded
```

The level-cap checks compile the actual arithmetic, game adapter and INI parser
against small game API doubles. The capture checks exercise eligibility and
transactional inventory behavior. Independent read-only reviews covered engine
call sites, OT provenance, post-bonus EXP ordering, Candy/DayCare guards and
capture authorization/target lifetime; those reviews did not execute a game.
The learnset checks exercise the actual record API and Move Reminder consumer,
including legacy ordering, malformed inputs and synthetic move IDs above 511.
Expanded checks cover the observed modern word format and 23/32-entry lists,
while keeping the independent legacy limit at 20. The complete Windows client
also links with those changes; modern content remains inactive.
The optional, hash-pinned offline check also matched all 65 modern reference
learnsets against the actual C decoder. It reads a user-supplied reference and
does not include reference data in source or CI.
Review also caught an ARM file-handle leak in an existing archive-size helper
that the new loader would have called. The loader now owns and closes its archive
on every path, avoiding a change to the old helper's behavior. The final Windows
client links with that fix.

The published Linux rule workflow passes all three suites. The changed-source
publication lint reported zero failures and four identifier warnings; every
warned identifier already exists in the pinned public engine foundation. No new
closed-reference decompiler identifiers or copied reference files were added.

No original installation, running game or player save was modified for these
checks. No native gameplay, full asset packaging, new-client networking, modern
content, save compatibility or 1:1 Together parity is established by this record.
User-confirmed reference-client battles and trades are separate evidence.

## Native transport and bounded modern content (2026-10-20)

The full Windows x64 client links after the native account/party GUI, signed
community link, semantic WM provider, V3 numeric catalog, evolution consumers,
six Fairy moves, emitter mode2 and move-menu palette additions. The actual
source fixtures passed for catalog parsing (including optional private pinned
reference validation), species loaders, evolution ordering, move-table bounds,
modern move effects/type chart/text encoder/archive generation, emitter behavior,
private-asset merge invariants and fresh-profile QA guards.

The real backend passes 58 tests. The native online fixture passes twelve
scenarios against it, including profile/TLS/party/chat/link/cancellation behavior.
The provider fixture passes four scenarios through actual Service/Core/provider
code and the runtime provider table. It checks discovery, dynamic parent/AID
assignment, two-way 512-byte delivery, callback-buffer ownership through WM
acknowledgment, reconnect identities and discarding queued old-generation data.
These are transport fixtures; they do not establish a completed native trade.

A private own-client runtime reached the new-game introduction using emulated
buttons and its own framebuffer capture. A fresh in-memory native battle scene
then exposed a missing Fairy move-menu palette at frame565. Its bounded accessor
and actual-consumer regression pass. A subsequent run exposed a compressed NCLR
whose expanded palette size exceeds its stored sparse bytes. That loader case
is under repair; no completed native battle is claimed here. All scenes use
separate fresh profiles and no OS keyboard/mouse injection.

No full species roster, modern ability catalog, Together world/co-op behavior,
save migration or 1:1 parity is established. These remain release acceptance
items; private assets and reference-derived caches are not part of the source.
