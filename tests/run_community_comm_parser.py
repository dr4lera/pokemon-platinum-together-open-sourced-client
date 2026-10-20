#!/usr/bin/env python3
"""Compile actual public ring/parser/command functions; no game or assets."""
import os
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
CC = os.environ.get("CC", "C:/msys64/ucrt64/bin/gcc.exe" if os.name == "nt" else "cc")
parser = argparse.ArgumentParser()
parser.add_argument("--engine-build", type=Path, help="Compile the three full source units in isolated temporary outputs using configured engine headers")
args = parser.parse_args()


def function(source, name):
    match = re.search(r"^(?:static )?[\w *]+\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise RuntimeError("Missing actual source function: " + name)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


comm = (ROOT / "src/communication_system.c").read_text()
cmd = (ROOT / "src/unk_02032798.c").read_text()
ring = re.sub(r'^#include .*$', '', (ROOT / "src/comm_ring.c").read_text(), flags=re.M)
body = "\n".join(function(comm, name) for name in ("CommSys_EndCallback", "CommSys_RecvDataSingle"))
commands = "\n".join(function(cmd, name) for name in ("CommCmd_Callback", "CommCmd_PacketSizeOf", "sub_020328D0", "sub_0203290C"))
logger = function(comm, "CommunityCommDiagnostics_Record")
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef int16_t s16; typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define PACKET_SIZE_VARIABLE 65535
static int assertions;
#define GF_ASSERT(v) ((void)((v) ? 0 : ++assertions))
#include "comm_ring.h"
#include "port/community_comm_diagnostics.h"
typedef void (*UnkFuncPtr_020F8E60)(int,int,void*,void*);
typedef int (*UnkFuncPtr_02032868)(void);
typedef void *(*UnkFuncPtr_0203290C)(int,void*,int);
typedef struct { UnkFuncPtr_020F8E60 unk_00; UnkFuncPtr_02032868 unk_04; UnkFuncPtr_0203290C unk_08; } CommCmdTable;
typedef struct {const CommCmdTable *cmdTable; int unk_04; void *unk_08;} CommCmdManager;
static CommCmdTable Unk_020E5D64[22];
static CommCmdManager *sCommCmdManager;
typedef struct {int unk_00; u8 *unk_04; u16 unk_08; u8 unk_0A;} CommRecvPackage;
static struct {int unk_6B1;} communication;
#define sCommunicationSystem (&communication)
static int faults, callbacks, seen_size;
static u8 received[512], allocated[512];
static void sub_020363BC(void) { ++faults; communication.unk_6B1=1; }
static void MI_CpuCopy8(const void *a,void *b,int size) {memcpy(b,a,(size_t)size);}
static int Variable(void) {return PACKET_SIZE_VARIABLE;}
static void Receive(int netId,int size,void *data,void *context) {
    (void)context; assert(netId==1); ++callbacks; seen_size=size; memcpy(received,data,(size_t)size);
}
static void *Allocate(int netId,void *context,int size) {
    (void)context; assert(netId==1 && size==260); return allocated;
}
'''
tests = r'''
static void Fragment(int custom, int split, int start) {
    u8 storage[1024], scratch[512], wire[263];
    CommRing r; CommRing_Init(&r,storage,sizeof(storage));
    r.startIndex=r.endIndex=r.backupEndIndex=start;
    CommRecvPackage state={0,NULL,65535,238};
    CommCmdTable table={Receive,Variable,custom ? Allocate : NULL};
    CommCmdManager manager={&table,1,NULL}; sCommCmdManager=&manager;
    communication.unk_6B1=0; callbacks=0;
    wire[0]=22; wire[1]=1; wire[2]=4;
    for(int i=0;i<260;++i) wire[i+3]=(u8)(i^91);
    CommRring_Write(&r,wire,split,123); CommRing_UpdateEndPos(&r);
    CommSys_RecvDataSingle(&r,1,scratch,&state);
    assert(callbacks==0 && faults==0);
    if(split<3) assert(state.unk_08==65535);
    if(split==2) assert(CommRing_DataSize(&r)==1);
    CommRring_Write(&r,wire+split,263-split,123); CommRing_UpdateEndPos(&r);
    CommSys_RecvDataSingle(&r,1,scratch,&state);
    assert(callbacks==1 && seen_size==260 && !memcmp(received,wire+3,260));
    assert(!CommRing_DataSize(&r) && state.unk_0A==238 && state.unk_08==65535);
    CommSys_RecvDataSingle(&r,1,scratch,&state); assert(callbacks==1);
}
int main(int argc,char **argv) {
    if(argc>1) {
        CommunityCommDiagnostics_Record("first",NULL,1,2,3,4,5,6);
        if(!strcmp(argv[1],"immutable")) {
#ifdef _WIN32
            _putenv_s("PLATINUM_COMMUNITY_DIAGNOSTICS","1");
#else
            setenv("PLATINUM_COMMUNITY_DIAGNOSTICS","1",1);
#endif
        }
        CommunityCommDiagnostics_Record("second",NULL,0,0,0,0,0,0);
        return 0;
    }
    for(int custom=0;custom<2;++custom)
        for(int split=1;split<263;++split) {
            Fragment(custom,split,0); Fragment(custom,split,1023);
        }
    CommCmdTable table={Receive,Variable,NULL}; CommCmdManager manager={&table,1,NULL};
    sCommCmdManager=&manager; callbacks=0; communication.unk_6B1=0;
    assert(CommCmd_PacketSizeOf(23)==0 && faults==1);
    CommCmd_Callback(1,23,0,NULL); assert(faults==2 && callbacks==0);
    u8 storage[8], data[8]={0}; CommRing r; CommRing_Init(&r,storage,8);
    CommRring_Write(&r,data,8,456); assert(faults==3 && r.backupEndIndex==0);
    return 0;
}
'''
scratch_root = ROOT / ".build-tools"
scratch_root.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-comm-", dir=scratch_root) as temporary:
    scratch = Path(temporary)
    source, executable = scratch / "fixture.c", scratch / "fixture.exe"
    actual = prefix + logger + ring + commands + body + tests
    source.write_text(actual)
    subprocess.run([CC, "-std=c11", "-DSDK_PORT", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-I"+str(ROOT / "include"), str(source), "-o", str(executable)], check=True)
    env = dict(os.environ, PLATINUM_COMMUNITY_DIAGNOSTICS="0")
    subprocess.run([str(executable)], env=env, check=True)
    for value, expected in (("1",1),("0",0),("true",0),("01",0),("",0)):
        result = subprocess.run([str(executable), "logger"], env=dict(env, PLATINUM_COMMUNITY_DIAGNOSTICS=value), capture_output=True, text=True, check=True)
        assert result.stderr.count("community-comm-first-fault")==expected
        assert "second" not in result.stderr
    result = subprocess.run([str(executable), "immutable"], env=env, capture_output=True, text=True, check=True)
    assert not result.stderr
    # Mutating only the historical predicate must make this actual-source test fail.
    assert "CommRing_DataSize(ring) < 2" in actual
    source.write_text(actual.replace("CommRing_DataSize(ring) < 2", "CommRing_DataSize(ring) < 1"))
    subprocess.run([CC,"-std=c11","-DSDK_PORT","-D_POSIX_C_SOURCE=200809L","-I"+str(ROOT / "include"),str(source),"-o",str(executable)],check=True)
    result = subprocess.run([str(executable)], env=env, capture_output=True)
    assert result.returncode != 0, "Historical fragmented-header bug was not reproduced"
    assert actual.count("cmd >= (sCommCmdManager->unk_04 + 22)")==2
    source.write_text(actual.replace("cmd >= (sCommCmdManager->unk_04 + 22)", "cmd > (sCommCmdManager->unk_04 + 22)"))
    subprocess.run([CC,"-std=c11","-DSDK_PORT","-D_POSIX_C_SOURCE=200809L","-I"+str(ROOT / "include"),str(source),"-o",str(executable)],check=True)
    result = subprocess.run([str(executable)], env=env, capture_output=True)
    assert result.returncode != 0, "Historical one-past command bound was not reproduced"
    if args.engine_build:
        commands = json.loads((args.engine_build.resolve() / "compile_commands.json").read_text())
        for name in ("communication_system.c", "comm_ring.c", "unk_02032798.c"):
            entry = next(item for item in commands if item["file"].endswith("/"+name))
            compile_args = shlex.split(entry["command"])
            compile_args[0] = CC
            compile_args = compile_args[:compile_args.index("-MD")]
            subprocess.run(compile_args + ["-c",str(ROOT / "src" / name),"-o",str(scratch / (name+".o"))],cwd=entry["directory"],check=True)
print("Actual communication parser: every split + wrap + allocated path, table bound, overflow, diagnostics PASS; both historical predicates FAIL as expected")
