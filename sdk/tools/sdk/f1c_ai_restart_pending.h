#pragma once
#include <stdint.h>
#include <stddef.h>
#include <float.h>
#include <math.h>
/* Read-only exact-target observation. AIMainUpdate_00424e70.c writes
 * vehicle+16752 with sim+10+(rng&0x14), compares it with current simulation
 * time, then replaces it with FLT_MAX before requesting native starter.
 * Caller establishes supported layout, live AI/nonterminal identity and tick.
 * This observes a future scheduled attempt, not guaranteed recovery. */
typedef int (*F1CAIRestartRead)(uintptr_t,void*,size_t);
typedef struct { int valid,pending; float deadline; } F1CAIRestartPending;
static F1CAIRestartPending f1c_ai_restart_pending_read(
    F1CAIRestartRead rd,uintptr_t vehicle,double now) {
    F1CAIRestartPending out={0,0,0};
    if(!rd || !vehicle || vehicle>UINT32_MAX-0x4174u ||
       !isfinite(now) || now<0 ||
       !rd(vehicle+0x4170u,&out.deadline,4) ||
       !isfinite(out.deadline) || out.deadline<0)return out;
    out.valid=1;
    out.pending=out.deadline>0 && out.deadline<FLT_MAX &&
                (double)out.deadline>=now;
    return out;
}
