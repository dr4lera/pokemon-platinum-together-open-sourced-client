# Retained native reference export

For a new reference project, use the [version 2 workflow](REFERENCE_EXPORT_V2.md).
This page describes the retained first-version exporter for existing projects.

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

Before using `-Resume`, confirm Ghidra's successful program save in the original
log. The first import saves after the export script finishes; its in-memory
analysis marker alone is insufficient. Preserve the original index, coverage and
logs before retrying, and record the retry's actual timeout separately. Successful
function bodies can be reused, but this legacy cache only checks paired file
hashes. The wrapper checks reference, Ghidra and exporter revisions; V2 adds a
saved-project checkpoint and stronger cache/run identity.

The legacy function total includes externals while its iterator visits internal
functions only. Its completion flag therefore requires a separate internal /
external count reconciliation. An interrupted retry may leave old coverage;
never accept an earlier report as evidence that the current run finished.

The exporter compiles against the installed Ghidra 12.1.4 APIs. The retained
0.1.7 export saved its project on 2026-10-20. Eight initial failures succeeded on
a 60-second retry. A separate read-only audit reconciled 65,152 recognized
internal functions and 558 external functions, and checked every internal body's
artifact/address header and SHA-256 against its index. All recognized internal
bodies have pseudocode; this does not establish undiscovered-code coverage,
semantic correctness, buildable original source or gameplay parity. REA's
ephemeral analysis exceeded its 300-second request timeout; this retained run
allows longer analysis without repeatedly discarding its project.

Permission records are retained privately and are not part of this repository.
Keep accounts, player saves, original executables and extracted asset packs out
of source commits. The export tool's source and recovered reference output are
different artifacts; our playable implementation remains the migration target.
