# Provenance and notices

This is a separately maintained community fork of
[cybervisi0n/pokeplatinum](https://github.com/cybervisi0n/pokeplatinum), branch
`pc_port`, starting at `35067868f64f23d25d17ac3e536cfdd9a7f40523`. That project
builds on [pret/pokeplatinum](https://github.com/pret/pokeplatinum). Preserve their
history, author credits, source notices and applicable terms. This project does
not claim the original game or upstream engine was authored here.

The libntr SDK replacements and their sibling libraries retain their own notices;
libntr is MIT, copyright 2026 cybervisi0n. Meson, SDL2, ENet, ImGui, zlib, libpng,
RapidJSON, yyjson, metang, GNU tools and other dependencies retain their licenses.
The selected libntr runtime is maintained in the user's
[community fork](https://github.com/dr4lera/pokemon-platinum-together-open-sourced-runtime),
based on upstream `1555d911ac80c0190ac1c321f99f1323de8b3c09`. Its new resource
resolver remains MIT. Native online support also uses libcurl and OpenSSL under
their respective licenses and Windows DPAPI for local account-key protection.
The selected libntrsystem implementation is maintained in the
[community system fork](https://github.com/dr4lera/pokemon-platinum-together-open-sourced-runtime-system),
based on upstream `cd778c9d5e45869f8ecb8245b316ac20ea576958`. Preserve its existing
notices. Its upstream checkout contains no repository-wide LICENSE file; this
project does not represent the whole system library as MIT or relicense it.
When distributing binaries, collect the notices from the exact pinned sources
and bundled runtime versions; a source checkout is not a finished release package.

New community-specific code is developed with OpenAI Codex assistance for dr4lera
and licensed under GPL-3.0-or-later, as specified in LICENSE-COMMUNITY. This does
not relicense existing upstream game/decompilation code, dependencies or assets.
The upstream project does not accept AI-created pull requests. No upstream
contribution or endorsement is implied by this separately maintained fork.

Platinum Together 0.1.7 is a private behavioral reference supplied by the user.
Its executable, extracted assets, decompiled/disassembled reference, signatures,
player saves and credentials are not included in community changes or releases.
Our implementation is not its original source, and full 1:1 parity is unfinished.
Pokémon and the original games retain their respective owners' credits.
