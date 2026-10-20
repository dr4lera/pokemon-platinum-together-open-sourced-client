# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$BackendRoot,
    [Parameter(Mandatory=$true)][string]$FixtureDirectory,
    [string]$NodePath='node',
    [string]$Compiler='C:/msys64/ucrt64/bin/g++.exe'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=& $NodePath --version
if($LASTEXITCODE -ne 0 -or $version -notmatch '^v(\d+)\.' -or [int]$Matches[1] -lt 24){throw 'The real backend fixture requires Node.js 24 or newer.'}
$scratch=Join-Path $root '.build-tools'
New-Item -ItemType Directory -Path $scratch -Force|Out-Null
$executable=Join-Path $scratch 'community-presentation-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'src/port/community_online.cpp') (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'src/port/community_trainer_art_download.cpp') (Join-Path $root 'tests/community_presentation_test.cpp') '-o' $executable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Native presentation test compilation failed.'}
$shutdownExecutable=Join-Path $scratch 'community-presentation-shutdown-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'tests/community_presentation_shutdown_test.cpp') (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'src/port/community_trainer_art_download.cpp') '-o' $shutdownExecutable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Native presentation shutdown test compilation failed.'}
$previousPath=$env:PATH
try {
    $env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH
    & $shutdownExecutable
    if($LASTEXITCODE -ne 0){throw 'Deterministic presentation shutdown/ordering fixture failed.'}
    & $NodePath (Join-Path $root 'tests/community_presentation_backend.mjs') $BackendRoot $FixtureDirectory $executable
    if($LASTEXITCODE -ne 0){throw 'Real backend/native presentation test failed; preserve its private fixture for inspection.'}
} finally {$env:PATH=$previousPath}
