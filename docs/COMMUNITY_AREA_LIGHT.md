# Desktop field lighting reader

The field avatars use textured 3D billboards. Their materials inherit global
area light colors through `AreaLight_UseGlobalModelAttributes`; this is separate
from the 2D NCLR palette loader used by battle sprites.

The lab's existing lighting archive contains CRCRLF line endings. The legacy
`Ascii_CopyToTerminator(..., '\r')` reader consumes that sequence as two lines.
Its fixed-field lighting loop therefore shifts records and reads empty lines as
material colors. An isolated test of the existing legacy functions reproduces
zero/shifted colors on CRCRLF with the same logical input that parses correctly
with ordinary CRLF. The normal new-game setup's additional berry and RNG
initialization does not initialize renderer colors; the field map initializes
its own model attributes and area light manager.

For `SDK_PORT`, `AreaLightTemplate_New` now reads the exact owned NARC member
extent and uses `port/community_area_light.h`. It accepts CR, LF, CRLF and
CRCRLF, ignoring empty separator lines. Each complete record retains source
order, end time, light validity/colors, clamped fixed-point vectors and four
material colors. Validating the whole resource precedes caller output mutation.
The archive owner and temporary records are released after conversion into
native `AreaLightTemplate` values. The original ARM reader remains under its
existing behavior branch.

The desktop reader bounds a member to128KiB,256 records and255 bytes per line.
It requires the EOF sentinel, complete CSV fields, valid color/flag ranges and
ordered time boundaries. Truncation, malformed data and partial records do not
publish a partially initialized result. They produce a diagnostic and one
fully initialized neutral light template, with white light and gray diffuse,
ambient and emission. This fallback is an explicit desktop stability deviation;
it is not a claim that the original game specifies neutral malformed-resource
lighting. It still depends on the engine being able to allocate its small
fallback object.

Run `python tests/run_community_area_light.py`. The actual shared parser is
compiled with C11 warnings as errors. The regression also extracts the retained
public legacy reader functions into its disposable fixture and demonstrates the
old dark-color result. Synthetic fixtures cover all four newline conventions,
exact color/vector values, strict bounds, truncation, invalid/partial records,
insufficient output capacity and unchanged output on refusal. Zero-initialized
legacy fixture locals make the empty-field failure deterministic; the old game
reader itself did not initialize those scratch arrays.

Optionally add `--engine-build build_win64` to compile the actual
`AreaLightTemplate_New` consumer using a configured native build's headers and
arguments. Only NARC/heap/platform primitives are supplied by the fixture. It
checks conversion, exact read extents, owner cleanup, malformed/unavailable/
oversized member fallback and transient allocation failure recovery. No shared
build object is written. `--narc <private-file>` validates existing private
members in place without copying assets into the repository; all four lab
members validate15 complete records each.

These are parser/consumer checks. A newly built whole client must still verify
the resulting field avatar colors. No existing resource, user save or running
game process was modified for this investigation.
