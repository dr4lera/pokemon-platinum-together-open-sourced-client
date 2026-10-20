# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$ReferencePath,
    [Parameter(Mandatory=$true)][string]$GhidraHome,
    [Parameter(Mandatory=$true)][string]$JavaHome,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$ExpectedSha256='3688d6b1c5b762b9a1f8d74b64f3a6a62d4b07d525074a94b432e4b0b2866ec9',
    [int]$FunctionTimeoutSeconds=10,
    [int]$AnalysisTimeoutSeconds=3600,
    [int]$Jobs=2,
    [switch]$Resume
)
$ErrorActionPreference='Stop'
function FileHash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function WriteJson([string]$path,$value) {
    $temporary=$path+'.tmp'
    [IO.File]::WriteAllText($temporary,($value|ConvertTo-Json -Depth 12)+"`n",[Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporary,$path,$true)
}
function ProjectFingerprint([string]$root,[string]$inventoryPath) {
    # Keep current indexes and every program database/property file. Ghidra's
    # IndexedLocalFileSystem.writeIndex rotates historical index/journal backups
    # whenever the writable project closes, including read-only program opens.
    $database=Join-Path $root 'ReferenceV2.rep'
    $rows=[Collections.Generic.List[string]]::new()
    $inventory=[Collections.Generic.List[object]]::new()
    foreach($relative in @('idata','versioned')) {
        $directory=Join-Path $database $relative
        if(!(Test-Path -LiteralPath $directory)){throw 'Saved project database is missing.'}
        foreach($file in Get-ChildItem -LiteralPath $directory -Recurse -File | Sort-Object FullName -CaseSensitive) {
            if(($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Project database reparse point refused.'}
            $name=[IO.Path]::GetRelativePath($database,$file.FullName).Replace('\','/')
            if($name -cin @('idata/~index.bak','versioned/~index.bak','idata/~journal.bak','versioned/~journal.bak')){continue}
            $digest=FileHash $file.FullName
            $rows.Add($name+':'+$digest)
            $inventory.Add([ordered]@{path=$name;sha256=$digest;length=$file.Length})
        }
    }
    if($rows.Count -lt 2){throw 'No persisted program database was found.'}
    if($inventoryPath){WriteJson $inventoryPath @($inventory.ToArray())}
    $bytes=[Text.Encoding]::UTF8.GetBytes(($rows -join "`n")+"`n")
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
}
function RunHeadless([string[]]$arguments,[string]$log) {
    & $script:headless @arguments '-log' $log
    if($LASTEXITCODE -ne 0){throw 'Ghidra failed; retain this output directory for inspection.'}
}

if($PSVersionTable.PSVersion.Major -lt 7){throw 'PowerShell 7 or newer is required.'}
if($Jobs -lt 1 -or $Jobs -gt 16 -or $FunctionTimeoutSeconds -lt 1 -or $FunctionTimeoutSeconds -gt 120 -or $AnalysisTimeoutSeconds -lt 60){throw 'Invalid resource limits.'}
$clientRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=[IO.Path]::GetFullPath($OutputDirectory)
if($output.Equals($clientRoot,[StringComparison]::OrdinalIgnoreCase) -or $output.StartsWith($clientRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Reference output must be outside the public source checkout.'}
# Reject existing junctions/symlinks in the output ancestry as well as lexical paths.
$ancestor=$output
while($ancestor) {
    if(Test-Path -LiteralPath $ancestor) {
        if(((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Reference output ancestry must not contain reparse points.'}
    }
    $parent=[IO.Directory]::GetParent($ancestor)
    $ancestor=if($parent){$parent.FullName}else{$null}
}
$source=(Resolve-Path -LiteralPath $ReferencePath).Path
$artifactHash=FileHash $source
if($artifactHash -ne $ExpectedSha256.ToLowerInvariant()){throw 'Reference artifact differs from selected version.'}
$headless=Join-Path $GhidraHome 'support/analyzeHeadless.bat'
$decompiler=Join-Path $GhidraHome 'Ghidra/Features/Decompiler/os/win_x86_64/decompile.exe'
foreach($required in @($headless,$decompiler,(Join-Path $JavaHome 'bin/java.exe'))){if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw 'Selected Ghidra/JDK/decompiler is missing.'}}
$version=((Get-Content -LiteralPath (Join-Path $GhidraHome 'Ghidra/application.properties') | Where-Object {$_ -match '^application.version='}) -replace '^application.version=','')
if(!$version){throw 'Ghidra version is missing.'}
$identity=[ordered]@{
    schema=2;artifactSha256=$artifactHash
    exporterSha256=(FileHash (Join-Path $PSScriptRoot 'ExportReferenceV2.java'))
    workflowSha256=(FileHash $PSCommandPath);ghidraVersion=$version
    decompilerBinarySha256=(FileHash $decompiler)
    savedProjectFingerprintAlgorithm='sha256-path-content-v2-current-index-no-rotated-index-journal-backups'
    originalSource=$false;runtimeVerified=$false
}
$manifestPath=Join-Path $output 'reference-v2.json'
$project=Join-Path $output 'project'
if(Test-Path -LiteralPath $output) {
    if(!$Resume -or !(Test-Path -LiteralPath $manifestPath)){throw 'Use a fresh V2 directory or -Resume a matching V2 checkpoint.'}
    $manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json -AsHashtable
    foreach($key in $identity.Keys){if($manifest[$key] -cne $identity[$key]){throw "Reference profile mismatch: $key"}}
    if(!$manifest.savedProjectFingerprint){throw 'No verified saved checkpoint. Preserve this directory; recover analysis separately or use a new output directory.'}
    if((ProjectFingerprint $project) -cne $manifest.savedProjectFingerprint){throw 'Persisted project changed; use a new output directory.'}
} else {
    if($Resume){throw 'No V2 project exists to resume.'}
    New-Item -ItemType Directory -Path $output | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $output 'input'),$project,(Join-Path $output 'runs') | Out-Null
    Copy-Item -LiteralPath $source -Destination (Join-Path $output 'input/Reference.exe')
    if((FileHash (Join-Path $output 'input/Reference.exe')) -cne $artifactHash){throw 'Reference copy mismatch.'}
    $manifest=$identity
    WriteJson $manifestPath $manifest
}
$workflowLock=[IO.File]::Open((Join-Path $output '.workflow.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$previousJavaHome=$env:JAVA_HOME
$previousMaxMemory=$env:GHIDRA_HEADLESS_MAXMEM
try {
$env:JAVA_HOME=$JavaHome
$env:GHIDRA_HEADLESS_MAXMEM='4G'
$runId=[Guid]::NewGuid().ToString()
$run=Join-Path $output ('runs/'+$runId)
New-Item -ItemType Directory -Path $run | Out-Null
$request=[ordered]@{schema=2;runId=$runId;phase='starting';startedAt=[DateTime]::UtcNow.ToString('o');functionTimeoutSeconds=$FunctionTimeoutSeconds;analysisTimeoutSeconds=$AnalysisTimeoutSeconds;jobs=$Jobs;resume=[bool]$Resume}
WriteJson (Join-Path $run 'request.json') $request
# latest.json is only a pointer. Completion must come from this run's validated report.
WriteJson (Join-Path $output 'latest.json') ([ordered]@{runId=$runId;phase='starting'})
try {
    if(!$Resume) {
        $analysisLog=Join-Path $run 'analysis.log'
        RunHeadless @($project,'ReferenceV2','-max-cpu',"$Jobs",'-analysisTimeoutPerFile',"$AnalysisTimeoutSeconds",'-import',(Join-Path $output 'input/Reference.exe'),'-scriptPath',$PSScriptRoot,'-postScript','ExportReferenceV2.java','analyze',$output,$runId,"$FunctionTimeoutSeconds") $analysisLog
        $checked=Get-Content -LiteralPath (Join-Path $run 'analysis.json') -Raw|ConvertFrom-Json
        if($checked.runId -cne $runId -or $checked.phase -cne 'analysis_checked_in_memory' -or $checked.artifactSha256 -cne $artifactHash){throw 'Analysis identity/check report is missing or inconsistent.'}
        $log=Get-Content -LiteralPath $analysisLog -Raw
        if($log -notmatch 'REPORT: Save succeeded for:.*Reference\.exe' -or $log -match 'REPORT: Save failed for:'){throw 'No verified program save after analysis. Retain the project; export was not started.'}
        $manifest.savedProjectFingerprint=ProjectFingerprint $project (Join-Path $run 'project-saved-files.json')
        $manifest.analysisRunId=$runId
        $manifest.savedCheckpointAt=[DateTime]::UtcNow.ToString('o')
        WriteJson $manifestPath $manifest
    }
    if((FileHash (Join-Path $output 'input/Reference.exe')) -cne $artifactHash){throw 'Retained input changed.'}
    if((ProjectFingerprint $project (Join-Path $run 'project-before-files.json')) -cne $manifest.savedProjectFingerprint){throw 'Persisted project changed before export.'}
    WriteJson (Join-Path $output 'latest.json') ([ordered]@{runId=$runId;phase='exporting'})
    RunHeadless @($project,'ReferenceV2','-process','Reference.exe','-noanalysis','-readOnly','-max-cpu',"$Jobs",'-scriptPath',$PSScriptRoot,'-postScript','ExportReferenceV2.java','export',$output,$runId,"$FunctionTimeoutSeconds") (Join-Path $run 'export.log')
    if((ProjectFingerprint $project (Join-Path $run 'project-after-files.json')) -cne $manifest.savedProjectFingerprint){throw 'Persisted project changed during export; do not accept this run.'}
    $coverage=Get-Content -LiteralPath (Join-Path $run 'coverage.json') -Raw|ConvertFrom-Json
    if($coverage.runId -cne $runId -or $coverage.artifactSha256 -cne $artifactHash -or $coverage.functionTimeoutSeconds -ne $FunctionTimeoutSeconds -or $coverage.phase -cne 'exported'){throw 'This export run did not finish with a matching report.'}
    if(!(Test-Path -LiteralPath (Join-Path $run 'functions.jsonl') -PathType Leaf)){throw 'Final function index is missing.'}
    if($coverage.totalFunctions -ne ($coverage.internalFunctions+$coverage.externalFunctions) -or $coverage.processedInternal -ne $coverage.internalFunctions -or $coverage.processedExternal -ne $coverage.externalFunctions -or ($coverage.recovered+$coverage.cached+$coverage.failed) -ne $coverage.internalFunctions){throw 'Export counts do not reconcile.'}
    WriteJson (Join-Path $run 'workflow-validation.json') ([ordered]@{runId=$runId;phase='validated';savedProjectUnchanged=$true;countsReconciled=$true;allRecognizedInternalFunctionsExported=$coverage.allRecognizedInternalFunctionsExported;finishedAt=[DateTime]::UtcNow.ToString('o')})
    WriteJson (Join-Path $output 'latest.json') ([ordered]@{runId=$runId;phase='exported';allRecognizedInternalFunctionsExported=$coverage.allRecognizedInternalFunctionsExported})
    $coverage|ConvertTo-Json -Compress
    if(!$coverage.allRecognizedInternalFunctionsExported -or $coverage.failed -ne 0){throw 'Some recognized functions failed. Inspect this run index; retry with -Resume and a larger function timeout.'}
    Write-Output 'All recognized internal functions exported; this is pseudocode, not original source recovery or gameplay parity.'
} catch {
    WriteJson (Join-Path $run 'workflow-validation.json') ([ordered]@{runId=$runId;phase='rejected';allRecognizedInternalFunctionsExported=$false;finishedAt=[DateTime]::UtcNow.ToString('o')})
    WriteJson (Join-Path $run 'workflow-error.json') ([ordered]@{runId=$runId;phase='failed';finishedAt=[DateTime]::UtcNow.ToString('o');error=$_.Exception.Message})
    WriteJson (Join-Path $output 'latest.json') ([ordered]@{runId=$runId;phase='failed'})
    throw
}
} finally {
    $env:JAVA_HOME=$previousJavaHome
    $env:GHIDRA_HEADLESS_MAXMEM=$previousMaxMemory
    $workflowLock.Dispose()
}
