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
$clientRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=[IO.Path]::GetFullPath($OutputDirectory)
if($output.Equals($clientRoot,[StringComparison]::OrdinalIgnoreCase) -or $output.StartsWith($clientRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'Reference output must be outside the public source checkout.'
}
if($Jobs -lt 1 -or $Jobs -gt 16 -or $FunctionTimeoutSeconds -lt 1 -or $FunctionTimeoutSeconds -gt 120 -or $AnalysisTimeoutSeconds -lt 60){
    throw 'Invalid resource limits.'
}
$source=(Resolve-Path -LiteralPath $ReferencePath).Path
$hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($hash -ne $ExpectedSha256.ToLowerInvariant()){throw 'Reference artifact differs from the selected version.'}
$headless=Join-Path $GhidraHome 'support/analyzeHeadless.bat'
if(!(Test-Path -LiteralPath $headless) -or !(Test-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe'))){throw 'Ghidra headless or selected Java runtime is missing.'}
$exporterHash=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'ExportReference.java') -Algorithm SHA256).Hash.ToLowerInvariant()
$properties=Get-Content -LiteralPath (Join-Path $GhidraHome 'Ghidra/application.properties')
$version=($properties | Where-Object {$_ -match '^application.version='}) -replace '^application.version=',''
if(!$version){throw 'Could not identify Ghidra version.'}
$manifestPath=Join-Path $output 'reference.json'
if(Test-Path -LiteralPath $output){
    if(!$Resume -or !(Test-Path -LiteralPath $manifestPath)){throw 'Use a fresh output directory, or -Resume for a matching reference project.'}
    $prior=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if($prior.sha256 -ne $hash -or $prior.exporterSha256 -ne $exporterHash -or $prior.ghidraVersion -ne $version){throw 'Reference, exporter or Ghidra version differs from the retained project.'}
}else{
    if($Resume){throw 'There is no reference project to resume.'}
    New-Item -ItemType Directory -Path $output | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $output 'input'),(Join-Path $output 'project') | Out-Null
    Copy-Item -LiteralPath $source -Destination (Join-Path $output 'input/Reference.exe')
    if((Get-FileHash -LiteralPath (Join-Path $output 'input/Reference.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash){throw 'Reference copy hash mismatch.'}
    [ordered]@{sha256=$hash;exporterSha256=$exporterHash;ghidraVersion=$version;sourceVersion='Platinum Together 0.1.7';originalSource=$false;runtimeVerified=$false;functionTimeoutSeconds=$FunctionTimeoutSeconds} |
        ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding utf8
}
$env:JAVA_HOME=$JavaHome
$env:GHIDRA_HEADLESS_MAXMEM='4G'
$arguments=@((Join-Path $output 'project'),'Reference017','-max-cpu',"$Jobs",'-analysisTimeoutPerFile',"$AnalysisTimeoutSeconds")
if($Resume){
    $arguments+=@('-process','Reference.exe')
    if(Test-Path -LiteralPath (Join-Path $output 'analysis-complete.json')){$arguments+='-noanalysis'}
}else{
    $arguments+=@('-import',(Join-Path $output 'input/Reference.exe'))
}
$arguments+=@('-scriptPath',$PSScriptRoot,'-postScript','ExportReference.java',$output,"$FunctionTimeoutSeconds",'-log',(Join-Path $output 'analysis.log'),'-scriptlog',(Join-Path $output 'export.log'))
& $headless @arguments
if($LASTEXITCODE -ne 0){throw 'Ghidra failed; retain the directory and logs for recovery.'}
$coverage=Get-Content -LiteralPath (Join-Path $output 'coverage.json') -Raw | ConvertFrom-Json
if($coverage.phase -ne 'exported'){throw 'Reference export did not finish; inspect retained coverage and logs.'}
$coverage | ConvertTo-Json -Compress
Write-Output 'Recovered pseudocode is a reference, not original buildable source or gameplay parity.'
