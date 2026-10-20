# Private source-client scenes

`PLATINUM_COMMUNITY_QA_SCENE` explicitly selects `wild-fairy`, `double-balls` or
`trainer-balls`. It also requires `PLATINUM_COMMUNITY_QA_CONFIRM_FRESH=1`, no
stored game, a new-game state and a clean load status. Existing or unreadable
profiles are refused. With no scene selected, ordinary startup is unchanged.
These development scenes mutate only the fresh in-memory game and call no game
save/erase/load API. The completed battle logs its outcome and exits.

Scenes use the actual native battle application, owned DTO, bag/dex/party APIs
and normal battle UI. Their player starts with a foreign-OT level14 Clefairy,
three Poke Balls and one Master Ball. Bidoof is registered caught; Starly in the
double scene is merely seen. The Fairy scene requires a validated modern gate
and initial world cap14; it never silently substitutes another move. Trainer
mode loads a valid existing trainer record so prize-money handling remains valid.

For unattended UI checks, the runtime's optional private scenario file supplies
bounded frame/button-mask records, and the optional framebuffer capture reads
only this executable's OpenGL frame. Neither tool focuses windows or injects OS
keyboard/mouse input. `run_community_qa_guard.py` executes the actual startup
guard with stored/corrupt/disabled/invalid-scene fixtures. Guard checks establish
profile isolation, not a completed battle or visual parity.
