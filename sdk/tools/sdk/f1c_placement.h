/* Same-session AI placement operations. Exact-byte review: PLACEMENT_REVIEW_2026-09-08.md. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef struct { float position[3],orientation[3]; } F1CPlacement;
typedef void (__attribute__((thiscall))*F1CPlacementGet)(void*,float*,float*);
typedef void (__attribute__((thiscall))*F1CPlacementSet)(void*,const float*,const float*);
typedef void (__attribute__((thiscall))*F1CMotionReset)(void*);
#define F1C_PLACEMENT_GET 0x004EFB20u
#define F1C_PLACEMENT_SET 0x004EF7C0u
#define F1C_MOTION_RESET 0x004F2320u
#define F1C_VEHICLE_PHYSICS_OFFSET 12u
static int f1c_placement_valid(const F1CPlacement *p){
    if(!p)return 0;
    for(int i=0;i<3;i++)if(!isfinite(p->position[i])||fabsf(p->position[i])>1000000||
        !isfinite(p->orientation[i])||fabsf(p->orientation[i])>100)return 0;
    return 1;
}
static int f1c_placement_code_valid(void){
    const unsigned char get[]={0x83,0xec,0x0c,0x8d,0x44,0x24,0x00,0x50};
    const unsigned char set[]={0x83,0xec,0x30,0x8b,0x44,0x24,0x38};
    const unsigned char reset[]={0x33,0xd2,0x89,0x91,0xec,0x34,0x00,0x00};
    return !memcmp((const void*)F1C_PLACEMENT_GET,get,sizeof(get))&&
           !memcmp((const void*)F1C_PLACEMENT_SET,set,sizeof(set))&&
           !memcmp((const void*)F1C_MOTION_RESET,reset,sizeof(reset));
}
/* Caller must verify exact target, game thread, stable same-session owned AI.
   Never use on player slot (native routines branch into global player physics). */
static int f1c_placement_capture(void *vehicle,F1CPlacement *p){
    if(!vehicle||!p||!f1c_placement_code_valid())return 0;
    ((F1CPlacementGet)F1C_PLACEMENT_GET)((char*)vehicle+F1C_VEHICLE_PHYSICS_OFFSET,p->position,p->orientation);
    return f1c_placement_valid(p);
}
static int f1c_placement_restore(void *vehicle,const F1CPlacement *p){
    if(!vehicle||!f1c_placement_valid(p)||!f1c_placement_code_valid())return 0;
    void *body=(char*)vehicle+F1C_VEHICLE_PHYSICS_OFFSET;
    ((F1CMotionReset)F1C_MOTION_RESET)(body);
    ((F1CPlacementSet)F1C_PLACEMENT_SET)(body,p->position,p->orientation);
    return 1;
}
