#pragma once
/* Exact-target native commentary, broker game-thread callers only.
 * Evidence and ABI: PIT_VOICE_REVIEW.md. Accepted means queued, not audible.
 * No cooldown, queue-head, priority or settings writes are performed here;
 * the _front variant only chooses its own record's due time. */
#include "f1c_pit.h"
#include <math.h>
#if defined(_MSC_VER)
#define F1C_VOICE_CALL __thiscall
#elif defined(__i386__)
#define F1C_VOICE_CALL __attribute__((thiscall))
#else
#define F1C_VOICE_CALL
#endif
typedef void (F1C_VOICE_CALL *F1CPitVoiceInit)(void *);
typedef void (F1C_VOICE_CALL *F1CPitVoiceFormat)(void *,int,int,int,int,int,float,float);
typedef char (F1C_VOICE_CALL *F1CPitVoiceInsert)(void *,void *);
static inline int f1c_pit_voice_allowed(int eid) {
    switch(eid) {
    case 44: /* PWS_PIT09 "Gear": lollipop turned to 1ST GEAR (pit procedure). */
    case 46: /* PWS_MISC54 "Box! Pit now": player within 10 m of own pit box. */
    case 43:case 45:case 50:case 51:case 53:case 54:case 56:case 57:case 61:
    case 62:case 63:case 64:case 65:case 66:case 67:case 68:case 69:case 70:
    case 117: /* PWS_RETIRE02: confirmed player retirement line. */
    case 183:case 184:case 185:case 186:case 187:case 188:case 189:case 237:return 1;
    default:return 0;
    }
}
/* Generic AI retirement: native caller 0041B250 uses subject + context 3.
 * A longer bounded lifetime lets the retirement wait behind existing radio. */
static inline int f1c_pit_voice_context(int eid) { return eid==237?3:-1; }
static inline float f1c_pit_voice_lifetime(int eid) {
    if(eid==237||eid==117)return 20.0f;
    if(eid==44)return 1.0f;           /* Gear: only on the board turn */
    if(eid==43||eid==45||eid==46||eid==50||eid==51||eid==53)return 2.0f;
    return 4.0f;
}
/* Player pit-procedure calls: exit, gear, go, box, pit-entry, limiter, brake.
 * They are only useful on the action, so they expire quickly instead of
 * stacking up behind each other. */
static inline int f1c_pit_voice_is_procedure(int eid) {
    return eid==43||eid==44||eid==45||eid==46||eid==50||eid==51||eid==53;
}
/* Front of queue. InsertDriverTableEntry 00418560 keeps the controller's 20
 * records (base 005EA618+0x864, 32 bytes: eid at +0, due time at +8, empty
 * due = -1.0f) sorted by due time, then priority. The formatter 004144F0 sets
 * due = trunc((clock + delay) * 5) * 0.2. A procedure call is placed one grid
 * step before the earliest pending other record but after any procedure call
 * still pending, so procedure calls keep their own order. Nothing already
 * queued or playing is modified or cut off. Returns the delay argument. */
static inline double f1c_pit_voice_front_delay(const float due[20],const int eid[20],double clock) {
    double earliest_other=clock,latest_own=-1.0e9;
    for(int i=0;i<20;i++) {
        if(due[i]==-1.0f || !isfinite(due[i]))continue;
        if(f1c_pit_voice_is_procedure(eid[i])) {if(due[i]>latest_own)latest_own=due[i];}
        else if(due[i]<earliest_other)earliest_other=due[i];
    }
    double target=earliest_other-0.2;
    if(latest_own>-1.0e8 && latest_own+0.2>target)target=latest_own+0.2;
    double delay=target-clock+0.1;   /* middle of the 0.2 s grid cell */
    if(delay<-10)delay=-10;
    if(delay>10)delay=10;
    return delay;
}
#ifndef F1C_PIT_VOICE_TEST
static inline int f1c_pit_voice_enqueue(int eid,int driver_id) {
    float record[8];
    if(!f1c_pit_voice_allowed(eid) || (eid>=183 && driver_id<0))return -1;
    if(!*(volatile int*)G_RaceActiveFlag || *(volatile float*)G_TrackConditionState==0)return 0;
    ((F1CPitVoiceInit)0x004144D0u)(record);
    ((F1CPitVoiceFormat)F1C_FN_CommentaryEventQueue)(record,eid,eid>=183?driver_id:-1,f1c_pit_voice_context(eid),-1,-1,0.0f,f1c_pit_voice_lifetime(eid));
    return ((F1CPitVoiceInsert)0x00418560u)((void*)0x005EA618u,record)?1:0;
}
static inline int f1c_pit_voice_enqueue_front(int eid,int driver_id) {
    float record[8],due[20];
    if(!f1c_pit_voice_allowed(eid) || (eid>=183 && driver_id<0))return -1;
    if(!*(volatile int*)G_RaceActiveFlag || *(volatile float*)G_TrackConditionState==0)return 0;
    int ids[20];
    const volatile unsigned char *queue=(const volatile unsigned char*)0x005EA618u+0x864u;
    for(int i=0;i<20;i++){ids[i]=*(const volatile int*)(queue+i*32);due[i]=*(const volatile float*)(queue+i*32+8);}
    double delay=f1c_pit_voice_front_delay(due,ids,*(volatile double*)0x008AFEA8u);
    double extend=delay<0?-delay:0; /* absolute expiry stays now + lifetime */
    ((F1CPitVoiceInit)0x004144D0u)(record);
    ((F1CPitVoiceFormat)F1C_FN_CommentaryEventQueue)(record,eid,eid>=183?driver_id:-1,f1c_pit_voice_context(eid),-1,-1,
        (float)delay,f1c_pit_voice_lifetime(eid)+(float)extend);
    return ((F1CPitVoiceInsert)0x00418560u)((void*)0x005EA618u,record)?1:0;
}
#endif
