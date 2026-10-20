# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual scan consumer against the real native SDK declarations.

Requires an existing Windows native build for its exact SDK flags. No network,
game files, running game or keyboard/mouse input is used.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, default=root / "build_win64")
parser.add_argument("--baseline", type=Path, help="Optional source file whose actual Scan function must fail the fixture")
args = parser.parse_args()
build = args.build.resolve()
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(item for item in commands if item["file"].replace("\\", "/").endswith("src/port/community_wireless.cpp"))
original = shlex.split(entry["command"])
flags = [word for word in original if word.startswith(("-I", "-D"))]
compiler = os.environ.get("CXX", "C:/msys64/ucrt64/bin/g++.exe")
env = {**os.environ, "PATH": str(Path(compiler).parent) + os.pathsep + os.environ.get("PATH", "")}


def scan_function(source):
    start = source.index("int Scan(void* opaque,WMBssDesc* output)")
    end = source.index("\nstd::string Hash", start)
    return source[start:end]


fixture = r'''
#define SDL_MAIN_HANDLED
#include <nitro.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include "port/community_wireless_core.h"
using namespace CommunityWireless;
struct Provider {std::mutex mutex;Core core;};
static Provider& Get(void* opaque){return *static_cast<Provider*>(opaque);}
SCAN_FUNCTION
int main(){
    static_assert(sizeof(((WMGameInfo*)nullptr)->userGameInfo)==112,"actual SDK byte capacity changed");
    static_assert(sizeof(((WMGameInfo*)nullptr)->userGameInfo[0])==2,"fixture must use actual SDK word array");
    std::vector<std::array<std::uint8_t,6>> roster{{2,1,2,3,4,5},{2,6,7,8,9,10}};
    Provider recipient;Core sender;
    assert(recipient.core.Begin(0,roster)&&sender.Begin(1,roster));
    recipient.core.TakeFrames();sender.TakeFrames();
    const ParentInfo metadata{512,128,120,11,1,5};
    for(unsigned size: {6u,112u,111u,1u,0u}){
        std::vector<std::uint8_t> bytes(size);for(unsigned i=0;i<size;i++)bytes[i]=static_cast<std::uint8_t>(i*3+1);
        assert(sender.SetInfo(0x12345678,0x2345,bytes.data(),size,metadata));assert(sender.SetRole(Role::Parent));
        for(auto& frame:sender.TakeFrames())assert(recipient.core.Receive(1,frame.bytes));
        struct Guard {std::uint8_t before[64];WMBssDesc descriptor;std::uint8_t after[256];} guard;
        std::memset(&guard,0xa5,sizeof(guard));
        if(Scan(&recipient,&guard.descriptor)!=1)return 2;
        const auto& d=guard.descriptor;
        // Byte equality fails the former u8-to-u16 widening copy, even for6 bytes.
        if(size&&std::memcmp(d.gameInfo.userGameInfo,bytes.data(),size))return 3;
        for(auto byte:guard.before)if(byte!=0xa5)return 4;
        for(auto byte:guard.after)if(byte!=0xa5)return 5;
        if(d.gameInfoLength!=16+size||d.length*2!=offsetof(WMBssDesc,gameInfo)+((d.gameInfoLength+1u)&~1u))return 6;
        if(d.gameInfo.parentMaxSize!=512||d.gameInfo.childMaxSize!=128||d.channel!=11||d.beaconPeriod!=120)return 7;
        if(d.gameInfo.userGameInfoLength!=size||d.gameInfo.ver!=WM_GAMEINFO_VERSION_NUMBER||d.gameInfo.platform||d.gameInfo.gameNameCount_attribute!=5)return 8;
        if(d.gameInfo.ggid!=0x12345678||d.gameInfo.tgid!=0x2345||std::memcmp(d.bssid,roster[1].data(),6))return 9;
    }
    assert(Scan(&recipient,nullptr)==0);
    std::puts("Actual SDK scan consumer: word-array byte equality,112-byte capacity/canaries,0/1/6/111/112-byte metadata, rounded descriptor lengths and parent fields passed.");
}
'''
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="wireless-real-scan-", dir=scratch) as folder:
    temporary = Path(folder)
    sources = [(root / "src/port/community_wireless.cpp", False)]
    if args.baseline:
        sources.append((args.baseline.resolve(), True))
    for index, (path, baseline) in enumerate(sources):
        source = temporary / f"scan{index}.cpp"
        source.write_text(fixture.replace("SCAN_FUNCTION", scan_function(path.read_text())), encoding="utf-8")
        executable = temporary / f"scan{index}.exe"
        subprocess.run([compiler, *flags, "-std=c++17", "-w", str(source), str(root / "src/port/community_wireless_core.cpp"), "-o", str(executable)], cwd=build, env=env, check=True)
        result = subprocess.run([str(executable)], cwd=temporary, env=env)
        if baseline:
            if result.returncode == 0:
                raise SystemExit("Expected the old SDK scan path to fail the regression")
            print("Pre-fix source fails actual SDK scan regression as expected:", result.returncode)
        else:
            result.check_returncode()
