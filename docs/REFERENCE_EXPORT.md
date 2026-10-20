# Retained native reference export

`tools/community/export-reference.ps1` runs the installed Ghidra headless analyzer
against a hash-verified copy of the selected reference. It never executes or
patches the game. The output must be outside this public source checkout.

Supply the installed Ghidra directory, a compatible JDK and a dedicated new output
directory. The default artifact hash selects Together 0.1.7; a different version
requires an explicit expected hash and a separate comparison. Example paths below
are placeholders to replace with your own installation paths:

```powershell
./tools/community/export-reference.ps1 `
  -ReferencePath 'D:/Reference/Platinum.exe' `
  -GhidraHome 'D:/Tools/ghidra_12.1.4_PUBLIC' `
  -JavaHome 'D:/Tools/jdk-21' `
  -OutputDirectory 'D:/PrivateReference/Together017'
```

The process retains its Ghidra project, logs, input digest, analyzer version and
exporter digest. `ExportReference.java` exports one pseudocode file per analyzed
function, with an address/name index, file hashes, failures and coverage counts.
Per-function timeouts are explicit failures. Missing debug metadata, unrecognized
code, global data/type recovery, assets and runtime behavior remain outside this
function export's guarantees. Even zero recorded function failures would not
prove that every original function was recognized or that the output rebuilds.

Use the same command with `-Resume` to reuse a matching retained project and
hash-verified function outputs. Completed auto-analysis is reused; interrupted
analysis runs again against its saved project. Keep timeout/errors explicit and
inspect `coverage.json`, rather than treating Ghidra's process exit as completion.
The wrapper rejects mismatched reference, Ghidra or exporter revisions.

The exporter compiles against the installed Ghidra 12.1.4 APIs. The first full
0.1.7 export was started on 2026-10-20 and is still being assessed; this document
does not claim completed decompilation or recovered original source. REA's
ephemeral analysis exceeded its 300-second request timeout; this retained run
allows longer analysis without repeatedly discarding its project.

Permission records are retained privately and are not part of this repository.
Keep accounts, player saves, original executables and extracted asset packs out
of source commits. The export tool's source and recovered reference output are
different artifacts; our playable implementation remains the migration target.
