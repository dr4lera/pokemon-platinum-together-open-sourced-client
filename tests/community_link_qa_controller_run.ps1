# SPDX-License-Identifier: GPL-3.0-or-later
param(
 [Parameter(Mandatory=$true)][string]$BackendRoot,
 [Parameter(Mandatory=$true)][string]$FixtureDirectory,
 [string]$NodePath='node',
 [string]$Compiler='C:/msys64/ucrt64/bin/g++.exe'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$scratch=Join-Path $root '.build-tools/community-link-qa-controller'
$stubs=Join-Path $scratch 'include'
New-Item -ItemType Directory -Path (Join-Path $stubs 'nitro') -Force|Out-Null
@'
#ifndef QA_FIXTURE_NITRO_TYPES
#define QA_FIXTURE_NITRO_TYPES
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int32_t s32;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#endif
'@ | Set-Content -LiteralPath (Join-Path $stubs 'nitro/types.h') -Encoding utf8NoBOM
'#include <nitro/types.h>' | Set-Content -LiteralPath (Join-Path $stubs 'nitro.h') -Encoding utf8NoBOM
$executable=Join-Path $scratch 'community-link-qa-controller.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+$stubs) ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'src/port/community_online.cpp') (Join-Path $root 'src/port/community_trainer_art.cpp') (Join-Path $root 'src/port/community_trainer_art_download.cpp') (Join-Path $root 'src/port/community_link_qa_guard.cpp') (Join-Path $root 'tests/community_link_qa_controller_test.cpp') '-o' $executable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Actual QA controller fixture compilation failed.'}
$previousPath=$env:PATH
try{
 $env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH
 & $NodePath (Join-Path $root 'tests/community_link_qa_controller_backend.mjs') $BackendRoot $FixtureDirectory $executable
 if($LASTEXITCODE -ne 0){throw 'Actual QA controller fixture failed; preserve private output.'}
}finally{$env:PATH=$previousPath}
