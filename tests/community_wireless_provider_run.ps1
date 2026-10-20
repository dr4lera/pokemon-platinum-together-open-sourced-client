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
if($LASTEXITCODE -ne 0 -or $version -notmatch '^v(\d+)\.' -or [int]$Matches[1] -lt 24){throw 'The real backend fixture requires Node.js24 or newer.'}
$scratch=Join-Path $root '.build-tools/community-wireless-provider'
$stubs=Join-Path $scratch 'include'
New-Item -ItemType Directory -Path (Join-Path $stubs 'nitro') -Force|Out-Null
# Minimal SDK declarations for the callback recipient only. The actual provider,
# Core, Service and real simulator provider-table header compile unchanged.
@'
#ifndef PROVIDER_FIXTURE_NITRO_TYPES_H
#define PROVIDER_FIXTURE_NITRO_TYPES_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#endif
'@ | Set-Content -LiteralPath (Join-Path $stubs 'nitro/types.h') -Encoding utf8NoBOM
@'
#ifndef NITRO_H_
#define NITRO_H_
#include <nitro/types.h>
#define WM_SIZE_CHILD_SSID 24
#define WM_GAMEINFO_LENGTH_MIN 16
#define WM_GAMEINFO_MAGIC_NUMBER 1
#define WM_ATTR_FLAG_ENTRY 1
typedef struct {u16 magicNumber;u16 gameNameCount_attribute;u32 ggid;u16 tgid;u8 userGameInfoLength;u8 userGameInfo[112];} FixtureWMGameInfo;
typedef struct {u8 bssid[6];u16 gameInfoLength;FixtureWMGameInfo gameInfo;} WMBssDesc;
#endif
'@ | Set-Content -LiteralPath (Join-Path $stubs 'nitro.h') -Encoding utf8NoBOM
$executable=Join-Path $scratch 'community-wireless-provider-test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' ('-I'+$stubs) ('-I'+(Join-Path $root 'include')) ('-I'+(Join-Path $root 'subprojects/libntr/include')) ('-I'+(Join-Path $root 'subprojects/rapidjson-1.1.0/include')) (Join-Path $root 'src/port/community_online.cpp') (Join-Path $root 'src/port/community_wireless_core.cpp') (Join-Path $root 'tests/community_wireless_provider_test.cpp') '-o' $executable '-lcurl' '-lcrypto' '-lcrypt32' '-pthread'
if($LASTEXITCODE -ne 0){throw 'Actual provider fixture compilation failed.'}
$previousPath=$env:PATH
try {
    $env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH
    & $NodePath (Join-Path $root 'tests/community_wireless_provider_backend.mjs') $BackendRoot $FixtureDirectory $executable
    if($LASTEXITCODE -ne 0){throw 'Actual provider loopback fixture failed; preserve its private output for inspection.'}
} finally {$env:PATH=$previousPath}
