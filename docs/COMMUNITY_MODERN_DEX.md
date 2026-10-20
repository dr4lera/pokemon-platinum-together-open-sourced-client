# Modern seen/caught persistence

CDX1 adds the own client's modern-species seen/caught history without enlarging
the native Pokédex or changing save-table offsets, page sizes or checksums. The
native `Pokedex` remains804 bytes; its496 language bytes remain at offset296.
Only their high bits carry CDX1. Every lower-seven-bit value and every other
native byte is preserved by the codec.

The public source audit found exactly two direct language-array operations:
`UpdateLanguage` in `src/pokedex.c` ORs a supported language bit, and
`Pokedex_IsLanguageObtained` tests a mapped language bit. The language table in
`src/pokedex_language.c` contains six entries: updates use bits0..5, unknown
languages map to6 and are rejected by the updater. The query never tests bit7.
`Pokedex_Init` clears the whole structure, while `Pokedex_Copy` copies its entire
unchanged size. Battle DTO copies use that same API. The record-mixing table in
`src/overlay059/ov59_021D2F88.c` exchanges RNG, photos, television, rankings and
Wi-Fi profile records; it neither exchanges nor merges Pokédex language bytes.
The existing normal save block owns Pokédex persistence and its checksum.

## CDX1 version1 layout

The high bit of language byte`i` is bit`i % 8` of record byte`i / 8`.
Thus496 carrier bits provide exactly62 record bytes. IDs512..576 are fixed by
this format; changing the catalogue's ID namespace requires an explicit format
migration. No catalogue data or Pokémon metadata is serialized into this record.

| Bytes | Meaning |
| --- | --- |
| 0..3 | ASCII`CDX1` |
| 4 | Version1 |
| 5 | Species count65 |
| 6..7 | First internal species ID512, little endian |
| 8..16 | Seen bits, one per species, least significant bit first |
| 17..25 | Caught bits, same order |
| 26..42 | Observed gender mask, two bits per species: bit0 male, bit1 female |
| 43..57 | Zero-reserved bytes; no form data in version1 |
| 58..61 | CRC32 of bytes0..57, little endian |

CRC32 uses the reflected IEEE polynomial`0xedb88320`, initial value`0xffffffff`
and final XOR`0xffffffff`. It detects damage; it is not authentication. Native
language flags are excluded from this CRC, so ordinary language updates cannot
invalidate modern history. Native save checksums continue covering the full
unchanged native page, including the CDX1 carrier bits.

All-zero carrier bits mean a legacy empty extension. A query returns empty
history without initializing it. The first valid write initializes CDX1. Any
other record must have the exact version, namespace, count, CRC and canonical
zero padding/reserved bits. Caught bits must be a subset of seen; unseen species
cannot have observed gender bits. Invalid or unknown nonzero data returns no
modern flags and is never rewritten or automatically repaired. Codec failure
does not modify either caller output or the carrier.

Recording an encounter sets seen. Recording a capture sets both seen and caught.
Observed male/female bits accumulate; genderless observations add neither bit.
A zero gender mask does not assert that the species is genderless. Version1 has
no form history, modern language history or modern Pokédex UI implementation.

## Native integration boundary

The desktop hooks intercept the fixed modern range before legacy validation or
bit indexing. Captures/encounters require enabled modern rules and a valid loaded
catalogue; otherwise they return without changing the Pokédex. Queries may read
valid persisted history independently of the active catalogue or rules setting.
The regular capture/trade/evolution catch-record paths therefore share the same
modern flags, and the battle's existing caught-marker query can read them.

Base-species behavior, original dex counts/completion, native language handling,
save structure, native checksum code and ARM behavior remain unchanged. This
unit does not establish modern Pokédex navigation/area/sort/form support, a new
save migration path, or successful two-game trading/recovery.

## Validation

`python tests/run_community_modern_dex.py --engine-build build_win64` runs the
portable codec and actual native-source integration fixtures. An optional
`--pokedex-source` may select a private proposed source copy while a shared build
is frozen; normal validation must use the tracked source after integration.

The fixtures cover all65 IDs; every496 single carrier-bit corruption; unknown
version and noncanonical metadata with recomputed CRC; caught/seen invariants;
gender masks; rejected-input immutability; and exact lower-seven-bit preservation.
The integration fixture compiles actual Pokédex Init/Copy/Capture/Encounter,
native language functions, trade catch-record registration, trainer-score caught
query, battle marker query, native checksum wrapper and SDK CRC16 implementation
against real engine headers. It verifies unchanged804-byte layout and offsets,
gate failures, egg exclusion, original dex counts, immutable Pokémon bytes, and
serializes an unchanged-size synthetic native Pokédex page plus native CRC16 to
a fresh temporary file and reads it back. It accesses no installed save and does
not test physical save-card IO, a whole-game trade or interrupted-trade recovery.
