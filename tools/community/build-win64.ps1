param([string]$BuildDirectory='build_win64',[int]$Jobs=8)
$ErrorActionPreference='Stop'
$clientRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Set-Location -LiteralPath $clientRoot
if($Jobs -lt 1 -or $Jobs -gt 64){throw 'Jobs must be between 1 and 64.'}
$env:PYTHONUTF8='1'
foreach($name in @('python','git','gcc','g++','nasm','ninja','flex','bison','bash','arm-none-eabi-gcc','arm-none-eabi-objcopy')){
    if(!(Get-Command $name -ErrorAction SilentlyContinue)){throw "Missing build dependency: $name. See docs/BUILD_COMMUNITY.md."}
}
$meson=Join-Path $clientRoot 'subprojects/meson-1.12.0/meson.py'
if(!(Test-Path -LiteralPath $meson)){
    & git clone --depth 1 --branch 1.12.0 https://github.com/mesonbuild/meson.git (Split-Path -Parent $meson)
    if($LASTEXITCODE -ne 0){throw 'Could not obtain pinned Meson source.'}
}
$mesonCommit=& git -C (Split-Path -Parent $meson) rev-parse HEAD
if($mesonCommit -ne 'ab4508e6abe338a3bd2e6eb5da1d18b8cf55dc93'){throw 'Meson source revision differs from the pinned build.'}
$arguments=@('setup',$BuildDirectory,'-Dbuild_target=win64','-Dskip_anim_scripts=false','--native-file=meson/native.ini','--cross-file=meson/cross_pcport_win64.ini')
if(Test-Path -LiteralPath (Join-Path $BuildDirectory 'build.ninja')){$arguments+='--reconfigure'}
& python $meson @arguments
if($LASTEXITCODE -ne 0){throw 'Client configuration failed.'}
& ninja -C $BuildDirectory -j $Jobs main.exe
if($LASTEXITCODE -ne 0){throw 'Client compilation failed.'}
Write-Output ('Compiled client: '+[IO.Path]::GetFullPath((Join-Path $BuildDirectory 'main.exe')))
Write-Output 'This builds the source client; it does not establish Together feature parity or migrate saves.'
