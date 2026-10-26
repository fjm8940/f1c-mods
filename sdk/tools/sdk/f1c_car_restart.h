/* Experimental exact-target per-car restart. CAR_RESTART_REVIEW_2026-09-08.md.
   Caller: game thread, stable owned nonplayer AI, active role1, live resources.
   One call per return; never use this as periodic parked-state maintenance. */
#pragma once
#include <stdint.h>
#include <string.h>
#define F1C_CAR_RESTART_ADDRESS 0x004C6D90u
#define F1C_CAR_AI_INIT_ADDRESS 0x00424790u
typedef void (__attribute__((thiscall))*F1CCarInitFn)(void*,uint32_t);
static int f1c_car_restart_ready(void){
    const unsigned char restart[]={0x83,0xec,0x18,0x53,0x8b,0x5c,0x24,0x20};
    const unsigned char ai[]={0x83,0xec,0x1c,0x53,0x33,0xdb,0x55,0x56,0x8b,0xf1};
    const unsigned char tail[]={0xc2,0x04,0x00};
    return !memcmp((void*)F1C_CAR_RESTART_ADDRESS,restart,sizeof(restart)) &&
           !memcmp((void*)F1C_CAR_AI_INIT_ADDRESS,ai,sizeof(ai)) &&
           !memcmp((void*)0x004C6E7Au,tail,3) && !memcmp((void*)0x00424AB2u,tail,3);
}
static int f1c_car_restart(void *vehicle){
    if(!vehicle||!f1c_car_restart_ready())return 0;
    /* Integer1: the native code tests the low byte, not floating-point1.0. */
    ((F1CCarInitFn)F1C_CAR_RESTART_ADDRESS)(vehicle,1u);return 1;
}
static int f1c_car_rebind_ai(void *vehicle){
    if(!vehicle||!f1c_car_restart_ready())return 0;
    ((F1CCarInitFn)F1C_CAR_AI_INIT_ADDRESS)(vehicle,1u);return 1;
}
