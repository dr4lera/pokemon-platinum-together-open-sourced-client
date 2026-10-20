# Continue emitter motion from its current position

The existing linear emitter callback now accepts mode 2. It retains attacker and
defender as endpoint identities, while its starting X/Y coordinates come from
the emitter's current position. The emitter resource's base position is subtracted
before the existing world-to-screen conversion. The ordinary emitter setters add
that base position once when storing each interpolated coordinate.

Modes 0 and 1 keep their original attacker-to-defender and defender-to-attacker
behavior. Destination offsets retain the attacker's transform direction. The
change leaves Z, interpolation helpers, start delay, skip/max frames, curves,
particle lifetime and task completion unchanged. It adds no new VM command,
particle format, function index or asset.

This supplies the mode-2 behavior required by the inspected Moonblast animation.
It does not itself enable a private animation overlay or establish visual parity.
Only the six validated move scripts and their required particle member should be
merged privately into archives generated for this source client; unrelated
patched animation archives remain outside the supported scope.

Run `python tests/run_community_emitter_mode.py` with an existing foundation build
(default `build_win64`, override `COMMUNITY_TEST_BUILD`). It compiles the actual
initializer, linear callback and task, position setters and interpolation helper
functions with small battle/particle API doubles. Tests cover modes 0/1/2 in both
perspectives, positive and negative resource origins, fractional negative starting
positions, frame zero/midpoint/last positions, the 36-frame start delay, and keeping
the task alive until active particles expire. Fixed-point division is a test
double; selected interpolation deltas divide exactly. The complete changed
animation translation unit also compiles independently against the real headers.
Outputs stay in a disposable directory; no private game assets are read.

These are deterministic code checks. In-game rendering, audio and perspective
verification remain separate acceptance checks.

## Preparing the narrowly merged private resources

First prepare a pristine source runtime with `prepare-private-runtime.py`. Then
run the separate merge tool, choosing a fresh output directory outside this
checkout and outside both input directories:

```powershell
python tools/community/prepare-modern-assets.py C:/Private/ModernAssets --base-runtime C:/Private/SourceRuntime --reference-root C:/Games/OwnedGame
```

The reference root is the user's installed game directory containing `patch`.
It is an explicit argument; the tool does not discover accounts or game installs.
Both input trees remain read-only. Nothing is written until all three complete
source archive hashes, reference archive hashes, member counts, selected member
hashes and compatibility checks pass. Existing output directories are refused.

Only these members enter the new archives:

- Animation scripts 468–473 replace the source build's reserved placeholders.
  Its other 495 script members remain byte-for-byte unchanged; output count501.
- Particle bundle485 is appended to the source particle archive; output count486.
- Fairy icon343 is appended to the source battle-icon archive; output count344.

The complete reference animation and particle archives are never copied over
the source-generated archives. Their unrelated modern scripts/resources remain
excluded. The selected particle bundle is checked against the supported SPL
section layout,14 resource slots,25 textures and existing texture format6.
The icon's bounded LZ10 stream and NCGR container are also checked. Exact hashes
bind these checks to the previously inspected assets; no hash override option is
provided. A different source build or reference revision requires a separate
review and explicit new pins, rather than silently accepting changed data.

`PRIVATE-MODERN-ASSETS.json` records each resulting archive/member hash and the
required engine features. It contains no input directory names, executable,
account or save. The manifest marks runtime verification as pending. The output
is private test material containing user-owned assets and must not be committed,
distributed or included in a source release. The tool does not enable the game
gate, alter the filesystem allowlist or replace anything in the pristine runtime.
Controlled integration into a disposable runtime is a separate step.

Run `python tests/run_community_modern_assets.py` for synthetic-only tests of NARC
structure/round trips, duplicate or overlapping bounds, selected-member hashes,
preservation of unselected source members, particle/icon bounds, whole-archive
hash refusal, existing output, symlinks and directory containment. These fixtures
are original test data and contain no extracted art or animation scripts.
