"""Execute the actual QA startup guard; existing/corrupt saves must be refused."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
source = (root / "src/port/community_qa.c").read_text()
guard = source[source.index("BOOL CommunityQA_TryStart(SaveData *saveData)"):]
harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int BOOL;
enum {FALSE,TRUE,FS_OVERLAY_ID_NONE=-1};
typedef struct {int exists,fresh;unsigned status;} SaveData;
static const int sTemplate=1;
static int sScene,queued;
static int modern=1,cap=14;
static int CommunityModernRules_Enabled(void){return modern;}
static int CommunityRules_CurrentLevelCap(void){return cap;}
static int SaveData_DataExists(SaveData* p){return p->exists;}
static int SaveData_IsNewGameData(SaveData* p){return p->fresh;}
static unsigned SaveData_LoadCheckStatus(SaveData* p){return p->status;}
static void EnqueueApplication(int overlay,const int* app){assert(overlay==-1&&app==&sTemplate);++queued;}
GUARD
static void Env(const char* key,const char* value){
#ifdef _WIN32
    _putenv_s(key,value);
#else
    setenv(key,value,1);
#endif
}
int main(void){
    SaveData p={0,1,0};
    Env("PLATINUM_COMMUNITY_QA_SCENE","");assert(!CommunityQA_TryStart(&p));
    Env("PLATINUM_COMMUNITY_QA_SCENE","wild-fairy");assert(!CommunityQA_TryStart(&p));
    Env("PLATINUM_COMMUNITY_QA_CONFIRM_FRESH","yes");assert(!CommunityQA_TryStart(&p));
    Env("PLATINUM_COMMUNITY_QA_CONFIRM_FRESH","1");
    p.exists=1;assert(!CommunityQA_TryStart(&p));p.exists=0;
    p.status=1;assert(!CommunityQA_TryStart(&p));p.status=0;
    p.fresh=0;assert(!CommunityQA_TryStart(&p));p.fresh=1;
    modern=0;assert(!CommunityQA_TryStart(&p));modern=1;
    cap=100;assert(!CommunityQA_TryStart(&p));cap=14;
    Env("PLATINUM_COMMUNITY_QA_SCENE","unknown");assert(!CommunityQA_TryStart(&p)&&queued==0);
    const char* names[]={"wild-fairy","double-balls","trainer-balls","wild-sylveon"};
    for(int i=0;i<4;i++){Env("PLATINUM_COMMUNITY_QA_SCENE",names[i]);assert(CommunityQA_TryStart(&p)&&sScene==i+1&&queued==i+1);}
    Env("PLATINUM_COMMUNITY_QA_SCENE","wild-sylveon");modern=0;assert(!CommunityQA_TryStart(&p));modern=1;
    cap=100;assert(!CommunityQA_TryStart(&p));cap=14;
    puts("Actual QA startup guard: opt-in confirmation, fresh empty profile, stored/corrupt/new-game refusal and bounded scene selection passed.");
}
'''.replace("GUARD", guard)
env = os.environ.copy()
env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="qa-guard-", dir=scratch) as directory:
    temp = Path(directory)
    (temp / "guard.c").write_text(harness)
    executable = temp / ("guard.exe" if os.name == "nt" else "guard")
    subprocess.run([compiler, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror", str(temp / "guard.c"), "-o", str(executable)], check=True, env=env)
    subprocess.run([str(executable)], check=True, env=env)
