# Character sprite archive lifetime

The unpacked NCGR descriptor and its pixel data borrow the archive returned by
the NARC allocator. The desktop NCGR cleanup can release the descriptor when
that archive is freed. `CharacterSprite_LoadSpriteData` now returns the owner to
its caller; the Pokémon path releases it only after decryption, optional Spinda
spot drawing, and copying all requested tiles. ARM follows the same ownership
order and retains the successful rendering algorithm.

The generic tiled path already retained its archive through copying. It now
shares the checked loader and tile-copy helper so allocation/decode failures
propagate to allocating callers. Failed public void loads leave their output
untouched. Region checks cover the descriptor's declared dimensions and byte
count; decryption requires the full 6,400-byte two-frame image consumed by both
existing Pokémon decryption routines. These checks cannot establish an archive
allocation's real extent through the legacy unsized unpack API.

Run `python tests/run_community_character_sprite.py`. It compiles the actual
`src/character_sprite.c` and actual SDK descriptor declarations. Synthetic NARC
storage is quarantined and poisoned on free, and decryption and raw-data copy
assert that the owner remains live. Both frames, Spinda handling, tile ordering,
allocation/decode failures, invalid regions, and cleanup are exercised. The
pre-fix source fails the live-owner assertion before decryption. No game assets
are used. NARC, heap, decryption, and spot drawing are fixture primitives; this
test verifies the consumer's ordering, not those separate implementations or a
complete game rendering session. The existing subregion layout and Spinda
species lookup are unchanged.

## Two-frame trade backgrounds

`TradeSequence_LoadPokemonBGTiles` requests a 20-by-10 tile region when loading
both animation frames and uploads all 6,400 bytes. The inherited packed-region
loader copied only the first frame's six rectangles (3,200 bytes), leaving the
second half of the upload uninitialized. The loader now emits the same six
rectangles for frame zero, then repeats them with source columns shifted by
ten for frame one. Single-frame requests retain their exact existing order.

Packed-region APIs accept only the observed 10-by-10 and 20-by-10 layouts.
Other dimensions return before allocating or changing output, rather than
applying fixed 10-by-10 rectangles to undersized storage. The lower-level
untiled-to-tiled region copier retains its separately bounded region support.

The actual-source fixture first reproduced the missing second frame. It now
compares every byte of the complete 6,400-byte result, including animated
Spinda, and checks canaries on both sides. It also compares each separate
3,200-byte packed frame, the generic packed loader, and refusal of unsupported
dimensions. These are synthetic rendering-data checks; no native trade
animation has been claimed from this fixture alone.
