"""Exercise the actual runtime wireless parser with a disposable transport/WM fixture."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
runtime = root / "subprojects/libntr"
compiler = os.environ.get("CXX") or shutil.which("g++")
if not compiler and Path("C:/msys64/ucrt64/bin/g++.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/g++.exe"
if not compiler:
    raise SystemExit("Set CXX to a C++ compiler")

stubs = {
"nitro.h": r'''#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t; using u64=uint64_t;
using BOOL=int;
#define TRUE 1
#define FALSE 0
#define WM_SIZE_CHILD_SSID 24
#define WM_SIZE_MP_DATA_MAX 512
#define WM_GAMEINFO_MAGIC_NUMBER 1
#define WM_GAMEINFO_LENGTH_MIN 16
#define WM_ATTR_FLAG_ENTRY 1
struct WMBssDesc {u8 bssid[6];int gameInfoLength;struct {int magicNumber,gameNameCount_attribute,ggid,tgid,userGameInfoLength;u8 userGameInfo[112];} gameInfo;};
struct OSOwnerInfo {u16 nickName[10];};
inline void OS_GetOwnerInfo(OSOwnerInfo* p){std::memset(p,0,sizeof(*p));}
''',
"nitro/types.h": '#include <nitro.h>\n',
"SDL2/SDL.h": r'''#pragma once
#include <cstdint>
struct SDL_mutex{}; struct SDL_sem{};
SDL_mutex* SDL_CreateMutex(); SDL_sem* SDL_CreateSemaphore(unsigned);
int SDL_LockMutex(SDL_mutex*); int SDL_UnlockMutex(SDL_mutex*);
int SDL_SemWaitTimeout(SDL_sem*,unsigned); int SDL_SemPost(SDL_sem*);
uint64_t SDL_GetTicks64(); void SDL_Delay(unsigned);
void* SDL_CreateThread(int(*)(void*),const char*,void*);
''',
"enet/enet.h": r'''#pragma once
#include <cstddef>
#include <cstdint>
struct ENetAddress {uint32_t host;uint16_t port;};
struct ENetPacket {size_t referenceCount,dataLength;unsigned char* data;};
struct ENetPeer {ENetAddress address;void* data;};
struct ENetHost{};
enum {ENET_EVENT_TYPE_NONE,ENET_EVENT_TYPE_CONNECT,ENET_EVENT_TYPE_DISCONNECT,ENET_EVENT_TYPE_RECEIVE};
enum {ENET_HOST_ANY=0,ENET_PACKET_FLAG_RELIABLE=1};
struct ENetEvent {int type;ENetPeer* peer;uint8_t channelID;ENetPacket* packet;};
int enet_host_service(ENetHost*,ENetEvent*,unsigned);
ENetHost* enet_host_create(const ENetAddress*,size_t,size_t,uint32_t,uint32_t);
ENetPeer* enet_host_connect(ENetHost*,const ENetAddress*,size_t,uint32_t);
void enet_host_destroy(ENetHost*);int enet_address_set_host(ENetAddress*,const char*);
ENetPacket* enet_packet_create(const void*,size_t,unsigned);void enet_packet_destroy(ENetPacket*);
int enet_peer_send(ENetPeer*,uint8_t,ENetPacket*);void enet_peer_disconnect(ENetPeer*,uint32_t);
void enet_host_broadcast(ENetHost*,uint8_t,ENetPacket*);
''',
"simulator/sim.h": r'''#pragma once
#include <cassert>
#define SIM_assert_always_msg(x) assert(false && x)
#define SIM_assert_always() assert(false)
#define SIM_assert(x) assert(x)
''',
"simulator/config/sim_config.h": r'''#pragma once
struct SIM_config_type {unsigned char macAddr[6];};
SIM_config_type* SIM_GetConfigPtr();
''',
}
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="wireless-tests-", dir=scratch) as directory:
    temp = Path(directory)
    for name, text in stubs.items():
        target = temp / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    executable = temp / ("wireless.exe" if os.name == "nt" else "wireless")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-Wno-missing-field-initializers",
                    "-I" + str(temp), "-I" + str(runtime / "include"),
                    str(root / "tests/community_wireless_test.cpp"), "-o", str(executable)]
                   + (["-lws2_32"] if os.name == "nt" else []), check=True)
    subprocess.run([str(executable)], check=True)
    # Compile the actual ARM7 send callback function in an isolated SDK fixture.
    # This verifies the error result at the real call site, rather than a model.
    request = (runtime / "libraries/sim7/src/wm/wmsp_request.c").read_text()
    start = request.index("void WMSP_SetMPData(OSMessage msg)")
    end = request.index("void WMSP_EndMP(OSMessage msg)", start)
    (temp / "wm_send_under_test.inc").write_text(request[start:end])
    send_executable = temp / ("wireless-send.exe" if os.name == "nt" else "wireless-send")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                    "-I" + str(temp), str(root / "tests/community_wireless_send_test.cpp"),
                    "-o", str(send_executable)], check=True)
    subprocess.run([str(send_executable)], check=True)
