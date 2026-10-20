# Version 2 retained reference export

This optional Windows/PowerShell 7 workflow creates private, offline pseudocode
references. It does not run or patch the game. It neither changes nor migrates
the original exporter or its retained projects/caches. Use a **new directory
outside the public checkout**, and keep all generated artifacts private.

```powershell
./tools/community/export-reference-v2.ps1 `
  -ReferencePath 'D:/Reference/Platinum.exe' `
  -GhidraHome 'D:/Tools/ghidra_12.1.4_PUBLIC' `
  -JavaHome 'D:/Tools/jdk-21' `
  -OutputDirectory 'D:/PrivateReference/Together017-v2'
```

The default digest selects the pinned 0.1.7 artifact. A different reference needs
its explicit expected digest and a separate output directory.

## Durable analysis and export

The first headless process imports, analyzes and checks artifact identity. Its
script returns before Ghidra saves. The wrapper requires that run's analysis
report and explicit successful save log, then hashes persisted project database
material. Only then does a separate process open the saved program with
`-process -noanalysis -readOnly` and export pseudocode.

The fingerprint includes current filesystem indexes and all program database and
property files under `idata`/`versioned`. It excludes only the root `~index.bak`
and `~journal.bak` historical backups in those two directories. Ghidra's
`IndexedLocalFileSystem.writeIndex()` rotates/deletes those backups when the writable
project closes, even when its program was opened read-only. Current index bytes,
program bytes and metadata remain checked. Per-run file inventories make before
and after comparisons inspectable; per-user preferences remain outside this
saved-program fingerprint.

An interrupted import without a verified saved checkpoint cannot be resumed by
this wrapper. Preserve it for inspection/recovery or use a fresh directory.
Nothing is deleted automatically. A hard stop during export preserves the saved
project and completed cache entries; its index may remain partial.
The wrapper holds an exclusive output lock so concurrent invocations cannot
replace one another's latest-run pointer or cache files.

## Reports, cache and retries

Every invocation has a UUID directory under `runs/`, containing request settings,
logs, profile, coverage, workflow validation and index. Accept completion only
when that run's workflow validation is `validated` and its recognized-functions
flag is true. `latest.json` is a status pointer; never infer completion from a
previous run's report. Index records are flushed to a partial
file, then renamed atomically after iteration/count reconciliation. Reports use
atomic replacement. This protects normal interruption, not every possible
power-loss/storage failure.

Internal and external functions are counted and indexed separately. Externals
have no exported function body. Successful completion means all **recognized
internal** functions produced hash-validated pseudocode and counts reconcile;
it does not mean the original program's source has been recovered. Diagnostics
are retained even when the decompiler returns a function successfully.

Cache identity binds artifact, entry address, exporter/workflow digests, Ghidra
and decompiler identity, language/compiler, explicit decompiler profile, saved
project database fingerprint and domain-file revision. Mismatches fail closed.
Do not hand-edit manifests to accept a changed tool/project. Use a fresh output
directory for those changes. Function timeouts are per-run metadata, deliberately
outside cache identity so successful bodies can be reused on a longer retry:

```powershell
# Repeat the same paths from the original V2 invocation:
./tools/community/export-reference-v2.ps1 @samePaths -Resume -FunctionTimeoutSeconds 60
```

A completed iteration with failed functions returns an explicit workflow error
after preserving its report/index. Retrying keeps prior run reports and attempts
uncached/invalid bodies again. Cache hashes are integrity/provenance checks, not
digital signatures against an adversary who can rewrite the whole private tree.

## Boundaries

Recognized function coverage excludes undiscovered code and does not establish
correct control flow, data/type recovery, buildable source, original names,
assets, runtime behavior or migration parity. Generated references, executables,
accounts and saves must stay outside source commits. No private correspondence
is embedded by these tools.

V2 compiles against the installed Ghidra 12.1.4 APIs. PowerShell parsing and a
synthetic headless fixture checked phase ordering, read-only/no-analysis flags,
timeout provenance, stale/missing reports, explicit function failures, changed
project refusal and the output path guard.

A real Ghidra 12.1.4/JDK 21 run against a tiny original no-CRT fixture completed
the saved-analysis/read-only-export workflow: three recognized internal
functions exported and one external indexed, with zero failures. A separate
60-second resume reused all three cached bodies, with the same export profile
and unchanged saved program/index/property digests. Flipping one byte of the
database in a separate copy was refused before another Ghidra process started.
The original failing fixture and successful fixture reports were retained.
These checks cover this fixture and do not establish full-reference export,
power-loss recovery or interruption behavior for every Ghidra project.
