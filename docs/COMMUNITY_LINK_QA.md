# Whole-client Union Room QA entry

This explicitly enabled development entry prepares two new accounts and a real
authenticated community link, then enters the existing field engine's Union
Room. It provides a repeatable starting point for testing native menus. It does
not itself accept invitations, choose Pokémon, execute turns, confirm a trade,
or prove that whole-engine battling/trading works.

## Integration

Compile these native client sources:

- `src/port/community_link_qa.cpp`: asynchronous Service setup controller.
- `src/port/community_link_qa_guard.cpp`: portable filesystem preflight.
- `src/port/community_link_qa_game.c`: C engine initialization and field task.

The C bridge deliberately keeps Pokémon and field data out of C++: several
existing public engine headers use C-only identifiers and declarations.
Include `port/community_link_qa.h` at the following three hook sites, guarded by
`PLATINUM_COMMUNITY_CLIENT`:

1. In `NitroMain`, call `CommunityLinkQA_Preflight()` immediately **before**
   `SaveData_Init()`. A requested invalid scene exits with status2.
2. At the existing QA startup branch, use
   `if (!CommunityLinkQA_TryStart(save) && !CommunityQA_TryStart(save))` before
   the ordinary startup path.
3. At the start of `HandleInputsEventsAndProcesses(FieldSystem*)`, call
   `CommunityLinkQA_Tick(fieldSystem)` before ordinary field input/task work.

The backend must have community link sessions enabled. The existing actual
Service, libcurl/OpenSSL dependencies and wireless provider are reused.

## Disposable setup

Create a new private parent directory outside the source checkout. Generate a
fresh32-character lowercase hexadecimal nonce. Create two new working
directories under that parent, named
`community-link-qa-<nonce>-host` and `community-link-qa-<nonce>-guest`. Their
`.community-link-qa` files must contain exactly these UTF-8 bytes with LF line
endings and a final newline, using the appropriate role:

```text
community-link-qa-v1
nonce=<nonce>
role=host
```

Each process must run with its corresponding working directory and environment:

| Variable | Value |
| --- | --- |
| `PLATINUM_COMMUNITY_LINK_QA` | `union-room` |
| `PLATINUM_COMMUNITY_QA_CONFIRM_FRESH` | `1` |
| `PLATINUM_COMMUNITY_LINK_QA_DIRECTORY` | Absolute exact working directory |
| `PLATINUM_COMMUNITY_LINK_QA_ROLE` | `host` or `guest` |
| `PLATINUM_COMMUNITY_SERVER_URL` | Explicit test backend origin |
| `PLATINUM_COMMUNITY_SERVER_KEY` | Trusted backend public identity pin |

Leave `PLATINUM_COMMUNITY_QA_SCENE` unset. Use isolated copies of the runtime
files/assets needed by the engine; no existing save, account directory, or
installed profile belongs in these working directories. Existing test window,
capture, and emulated-controller options may be supplied separately. This
module does not use the OS keyboard/mouse or add controller automation.

The preflight requires the absolute configured path to equal the actual current
directory, validates every ancestor against symlinks and Windows reparse
points, verifies the exact marker and role, and refuses an existing `save.bin`
or `CommunityOnline` path. The host also refuses an already used coordination
nonce. No save is deleted or reset. This check runs before the backup read that
would create a new save file. It assumes the private test directory is not
concurrently altered by another process; it is a development isolation guard,
not a defense against an adversary replacing files after validation.

## Data flow and native entry

The C bridge verifies the fresh `SaveData` flags, seeds `QAHOST` or `QAGUEST`
with two distinct level10 Pokémon, initializes system time/options and starts
`gFieldSystemNewGameTemplate` at Jubilife Pokémon Center2F. Initial-new-game
script initialization and map object creation remain active. Existing world
rules and original trainer metadata remain in effect.

The controller installs the real provider, creates a fresh signed account under
the working directory's `CommunityOnline/QAAccount`, and creates/joins a real
party. The host publishes the party code to its new nonce-bound coordination
file. Both peers freeze actual catalog/rules through `CommunityWireless::Prepare`.
The guest records its server prepare acknowledgment; the host checks that its
build/catalog/rules match before allocating the session. Coordination records
are bounded, private, and published atomically without replacing existing
files. No account key or peer ticket is written to these records.

Only an `Active` two-peer session permits the field task. After the field map
is running with no other task/child application, the task sets the normal club
flag and calls `sub_02054708`. That existing routine records the return
location, initializes `FieldSystem_InitCommUnionRoom` (which starts
`CommManager_StartUnion`), selects `MAP_LOAD_TYPE_UNION`, and pushes the normal
Union Room warp at `(8,14)`. The QA parent returns FALSE while its child task is
pending, preserving the engine's actual task-stack ownership.

The real `scripts_union_room.s` activity menu is then used. Battle checks two
eligible non-eggs, opens party selection and executes `StartLinkBattle` through
`Encounter_NewVsLink`. Trade checks at least two non-eggs, then calls the
existing `sub_0203DDDC` field task, `TradeRoom` application, trade animation and
evolution flow. Directly starting either application would skip these native
communication and synchronization prerequisites, so this QA entry does not do
that.

Native trade code invokes synchronized saving during later trade rounds;
native menu save flows are retained. The desktop backup implementation uses
relative `save.bin` for read/write/verify in
`subprojects/libntr/include/nitro/card/backup.h`, so these writes remain in the
new disposable working directory. Never claim this scene cannot save. There is
no automatic recovery, reconnection, or replay of a cancelled trade. A fresh
nonce and new working directories are required for another run.

## Verification boundaries

`python tests/run_community_link_qa_guard.py` compiles the actual guard in C++17
with warnings as errors. It checks fresh directory admission, incorrect
path/role/marker, existing save/account refusal, bounded reads, exclusive atomic
publication and old nonce refusal. On Windows it also tests a real directory
junction; symbolic-link creation may be unavailable separately.

`tests/community_link_qa_controller_run.ps1` compiles the actual controller and
Service and runs two fresh private processes against the real loopback backend.
It verifies account authentication, party creation/join, matching prepare
acknowledgments and two signed WebSocket attachments reaching an Active session.
The fixture supplies only a wireless install stub and fixed compatibility
fingerprints; it does not run the C game bridge, native map, WM implementation,
or any activity menu. Those remain whole-client verification work. The new C
and C++ modules have also passed isolated syntax checks with the actual native
engine build arguments, without invoking the shared build.
