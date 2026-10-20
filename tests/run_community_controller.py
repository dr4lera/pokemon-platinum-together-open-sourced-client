#!/usr/bin/env python3
"""Compile the actual portable controller parser/state helper, no game/input."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "C:/msys64/ucrt64/bin/gcc.exe" if os.name == "nt" else "cc"))
args = parser.parse_args()
parent = root / ".build-tools"
parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-controller-", dir=parent) as temporary:
    scratch = Path(temporary)
    source = scratch / "controller.c"
    source.write_text(r'''
#include <assert.h>
#include <string.h>
#include <simulator/community_controller.h>
static SIM_CommunityControllerRecord Parse(const char *text) {
    SIM_CommunityControllerRecord record;
    assert(SIM_CommunityController_Parse(text,strlen(text),&record));
    return record;
}
int main(int argc,char **argv) {
    assert(argc==2);
    SIM_CommunityControllerRecord record={999,999,999};
    SIM_CommunityControllerState state={0};
    const char *bad[]={"","0 1 1\n","2147483648 1 1\n","999999999999999999 1 1\n",
      "+1 1 1\n","-1 1 1\n","1 -1 1\n","1 0x1 1\n","1 1000 1\n","1 0000 1\n",
      "1 1 0\n","1 1 181\n","1 1 99999999\n","1 1 +1\n","1 1 -1\n","1 1 1",
      "1\t1 1\n","1  1 1\n","1 1 1\r\n","1 1 1 \n","1 1 1\n2 1 1\n","1 g 1\n",
      "1 1 1\nexec anything\n"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        assert(!SIM_CommunityController_Parse(bad[i],strlen(bad[i]),&record));
        assert(record.id==999&&record.mask==999&&record.duration==999);
    }
    char embedded[]={'1',' ','1',' ','1','\n','\0'};
    assert(!SIM_CommunityController_Parse(embedded,sizeof(embedded),&record));
    char oversized[65];memset(oversized,'1',sizeof(oversized));
    assert(!SIM_CommunityController_Parse(oversized,sizeof(oversized),&record));
    assert(!SIM_CommunityController_Parse(NULL,6,&record));
    assert(!SIM_CommunityController_Parse("1 1 1\n",6,NULL));
    record=Parse("2147483647 FfF 180\n");
    assert(record.id==2147483647u&&record.mask==0xfff&&record.duration==180);
    record=Parse("1 001 2\n");assert(SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==1);
    assert(!SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==1);
    assert(!SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==0);
    assert(SIM_CommunityController_Frame(&state)==0);
    record=Parse("10 fff 180\n");assert(SIM_CommunityController_Accept(&state,&record));
    for(int i=0;i<180;++i){assert(!SIM_CommunityController_Accept(&state,&record));assert(SIM_CommunityController_Frame(&state)==0xfff);}
    assert(SIM_CommunityController_Frame(&state)==0);
    record=Parse("11 800 2\n");assert(SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==0x800);
    record=Parse("12 000 1\n");assert(SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==0);
    assert(SIM_CommunityController_Frame(&state)==0);
    record=Parse("2147483647 001 1\n");assert(SIM_CommunityController_Accept(&state,&record));
    assert(SIM_CommunityController_Frame(&state)==1);
    assert(SIM_CommunityController_Frame(&state)==0);
    record.id=2147483648u;assert(!SIM_CommunityController_Accept(&state,&record));
    state=(SIM_CommunityControllerState){0};
    record=(SIM_CommunityControllerRecord){1,4096,1};assert(!SIM_CommunityController_Accept(&state,&record));
    record=(SIM_CommunityControllerRecord){1,1,181};assert(!SIM_CommunityController_Accept(&state,&record));
    FILE *file=fopen(argv[1],"wb");assert(file);assert(fwrite("27 a 3\n",1,7,file)==7);assert(!fclose(file));
    file=fopen(argv[1],"rb");assert(file);assert(SIM_CommunityController_Read(file,&record));assert(record.id==27&&record.mask==10&&record.duration==3);fclose(file);
    file=fopen(argv[1],"wb");assert(file);fwrite(oversized,1,sizeof(oversized),file);fclose(file);
    file=fopen(argv[1],"rb");assert(file);assert(!SIM_CommunityController_Read(file,&record));assert(ftell(file)==65);fclose(file);
    unsigned capture=999,lastCapture=0;
    assert(SIM_CommunityCapture_Parse("1\n",2,&capture)&&capture==1);
    assert(SIM_CommunityCapture_Accept(&lastCapture,capture));
    assert(!SIM_CommunityCapture_Accept(&lastCapture,capture));
    assert(SIM_CommunityCapture_Parse("2147483647\n",11,&capture)&&capture==2147483647u);
    assert(SIM_CommunityCapture_Accept(&lastCapture,capture));
    const char *badCapture[]={"0\n","-1\n","+1\n","2147483648\n","1\r\n","1","1 \n","1\n2\n","1 1 1\n","99999999999999999\n"};
    for(unsigned i=0;i<sizeof(badCapture)/sizeof(badCapture[0]);++i)assert(!SIM_CommunityCapture_Parse(badCapture[i],strlen(badCapture[i]),&capture));
    assert(!SIM_CommunityCapture_Parse(embedded,sizeof(embedded),&capture));
    file=fopen(argv[1],"wb");assert(file);fwrite("17\n",1,3,file);fclose(file);
    file=fopen(argv[1],"rb");assert(file);assert(SIM_CommunityCapture_Read(file,&capture)&&capture==17);fclose(file);
    file=fopen(argv[1],"wb");assert(file);fwrite(oversized,1,sizeof(oversized),file);fclose(file);
    file=fopen(argv[1],"rb");assert(file);assert(!SIM_CommunityCapture_Read(file,&capture));assert(ftell(file)==17);fclose(file);
    puts("Actual capture-request helper: separate positive decimal IDs, strict LF/size bounds, monotonic replay refusal and17-byte read limit passed.");
    puts("Actual controller helper: strict bounded records, overflow/mask/duration refusal, monotonic IDs, replay immunity, replacement, explicit release and exact 1/2/180-frame auto-release passed. No OS input or game process used.");
}
''', encoding="utf-8")
    executable = scratch / ("controller.exe" if os.name == "nt" else "controller")
    subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "subprojects/libntr/include"), str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(scratch / "command")], check=True)
