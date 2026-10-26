/* Read-only x86 vehicle snapshot. Evidence: AI_INTERACTION_READ_2026-09-08.md.
   Caller owns exact-target, game-thread, stable-field and live identity gates.
   Never dereference target_address; resolve against current owned slot records. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
typedef struct { uint32_t target_address; float lateral_request, passing; } F1CAIInteraction;
static int f1c_ai_interaction_read(const void *vehicle,size_t bytes,F1CAIInteraction *out){
    F1CAIInteraction s;
    if(!vehicle || !out || bytes<0x4600u)return 0;
    memcpy(&s.target_address,(const unsigned char*)vehicle+0x45FCu,4);
    memcpy(&s.lateral_request,(const unsigned char*)vehicle+0x4420u,4);
    memcpy(&s.passing,(const unsigned char*)vehicle+0x44CCu,4);
    if(!isfinite(s.lateral_request)||!isfinite(s.passing))return 0;
    *out=s;return 1;
}
