#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
/* Read-only canonical EngineRPM field (vehicle+0x39A0, CONFIRMED SDK).
 * Observe zero/nonzero only; no unit conversion or recovery prediction.
 * 0 unknown, 1 rotating, 2 stopped. Caller establishes supported layout. */
typedef int (*F1CEngineMotionRead)(uintptr_t,void*,size_t);
static int f1c_engine_motion(F1CEngineMotionRead rd,uintptr_t vehicle){
 float value=0;
 if(!rd||!vehicle||vehicle>UINT32_MAX-0x39a4u||
    !rd(vehicle+0x39a0u,&value,4)||!isfinite(value)||value<0)return 0;
 return value>0?1:2;
}
