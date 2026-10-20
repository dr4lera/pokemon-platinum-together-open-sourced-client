# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$BackendRoot,[Parameter(Mandatory=$true)][string]$FixtureDirectory,[string]$NodePath='node',[string]$Compiler='C:/msys64/ucrt64/bin/g++.exe')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$scratch=Join-Path $root '.build-tools'
New-Item -ItemType Directory -Path $scratch -Force|Out-Null
$executable=Join-Path $scratch 'community-trainer-art-download-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'src/port/community_online.cpp') (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'src/port/community_trainer_art_download.cpp') (Join-Path $root 'tests/community_trainer_art_download_test.cpp') '-o' $executable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Art download fixture build failed'}
$generation=Join-Path $scratch 'community-trainer-art-generation-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'tests/community_trainer_art_generation_test.cpp') '-o' $generation '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Art publication fixture build failed'}
$previousPath=$env:PATH
try{$env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH;& $generation;if($LASTEXITCODE -ne 0){throw 'Art publication fixture failed'};& $NodePath (Join-Path $root 'tests/community_trainer_art_download_backend.mjs') $BackendRoot $FixtureDirectory $executable;if($LASTEXITCODE -ne 0){throw 'Actual backend art download fixture failed; retain private fixture'}}finally{$env:PATH=$previousPath}
