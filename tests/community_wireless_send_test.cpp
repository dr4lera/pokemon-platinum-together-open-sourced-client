// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstdint>
#include <cstdio>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;
using BOOL=int;using OSMessage=void*;using WMCallbackFunc=void(*)(void*);
enum {WM_ERRCODE_ILLEGAL_STATE=1,WM_ERRCODE_SUCCESS=2,WM_ERRCODE_SEND_FAILED=3,
      WM_ERRCODE_OPERATING=4,WM_ERRCODE_SEND_QUEUE_FULL=5,WM_APIID_PORT_SEND=6,WM_STATECODE_PORT_SEND=7};
struct WMSPWork{};
struct WMStatus {u16 aid;u32 child_bitmap;int mp_flag;u16 mp_parentSize,mp_childSize;};
struct WMPortSendCallback {u16 apiid,errcode,state,port,destBitmap,restBitmap,sentBitmap,size;u16* data;WMCallbackFunc callback;void* arg;u16 seqNo,maxSendDataSize,maxRecvDataSize;};
static WMSPWork work;
static WMStatus status{};
static WMPortSendCallback result{};
static bool transportSuccess=true;
static unsigned sends=0,replies=0;
WMSPWork* WMSP_GetSystemWork(){return &work;}
WMStatus* WMSP_GetStatusStructure(){return &status;}
void* WMSP_GetBuffer4Callback2Wm9(){return &result;}
void WMSP_ReturnResult2Wm9(void* value){assert(value==&result);++replies;}
BOOL SIM_Net_SendMPData(u16,u16,u8*,u16){++sends;return transportSuccess;}
#include "wm_send_under_test.inc"
int main(){
    u16 data=0;
    u64 args[8]{};args[1]=reinterpret_cast<uintptr_t>(&data);args[2]=2;args[3]=2;args[4]=1;
    status={0,2,1,512,512};transportSuccess=false;
    WMSP_SetMPData(args);assert(sends==1&&replies==1&&result.errcode==WM_ERRCODE_SEND_FAILED);
    transportSuccess=true;WMSP_SetMPData(args);assert(sends==2&&replies==2&&result.errcode==WM_ERRCODE_SUCCESS);
    status.mp_flag=0;WMSP_SetMPData(args);assert(sends==2&&replies==3&&result.errcode==WM_ERRCODE_ILLEGAL_STATE);
    status.mp_flag=1;status.child_bitmap=0;WMSP_SetMPData(args);assert(sends==2&&replies==4&&result.errcode==WM_ERRCODE_SUCCESS);
    std::puts("Actual ARM7 WM send callback: failed/successful transport, inactive MP and no-recipient status passed.");
}
