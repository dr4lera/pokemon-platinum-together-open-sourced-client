# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$BackendRoot,
    [Parameter(Mandatory=$true)][string]$FixtureDirectory,
    [string]$NodePath='node',
    [string]$Compiler='C:/msys64/ucrt64/bin/g++.exe',
    [string]$BuildDirectory=''
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=& $NodePath --version
if($LASTEXITCODE -ne 0 -or $version -notmatch '^v(\d+)\.' -or [int]$Matches[1] -lt 24){throw 'The real backend fixture requires Node.js24 or newer.'}
$scratch=Join-Path $root '.build-tools/community-wireless-provider'
New-Item -ItemType Directory -Path $scratch -Force|Out-Null
if(!$BuildDirectory){$BuildDirectory=Join-Path $root 'build_win64'}
$nativeFlags=(& python (Join-Path $PSScriptRoot 'native_build_flags.py') $BuildDirectory 'src/port/community_wireless.cpp') | ConvertFrom-Json
if($LASTEXITCODE -ne 0){throw 'An existing native build is required for the actual SDK ABI fixture.'}
$executable=Join-Path $scratch 'community-wireless-provider-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' @nativeFlags (Join-Path $root 'src/port/community_online.cpp') (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'src/port/community_trainer_art_download.cpp') (Join-Path $root 'src/port/community_wireless_core.cpp') (Join-Path $root 'tests/community_wireless_provider_test.cpp') '-o' $executable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Actual provider fixture compilation failed.'}
$previousPath=$env:PATH
try {
    $env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH
    & $NodePath (Join-Path $root 'tests/community_wireless_provider_backend.mjs') $BackendRoot $FixtureDirectory $executable
    if($LASTEXITCODE -ne 0){throw 'Actual provider loopback fixture failed; preserve its private output for inspection.'}
} finally {$env:PATH=$previousPath}
