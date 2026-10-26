#pragma once
/* Port of original pit_commentary.c hud_write. Game-thread only, exact target.
 * Writes only vacant slots; 31-byte bounded text, native expiry. No restore.
 * Original layout evidence: REUSED_PIT_PRESENTATION.md. */
#include <windows.h>
#include <cstring>
#include <cmath>
static inline int f1c_pit_hud_write_slots(unsigned char *inner,const char *msg) {
    if(!inner||!msg||!msg[0])return 0;
    for(int n=0;n<8;n++) {
        float *timer=(float*)(inner+n*0x5C+0x90);
        if(std::isfinite(*timer)&&*timer<=0) {
            char *text=(char*)(inner+n*0x5C+0x3C);
            std::strncpy(text,msg,31);text[31]=0;
            *timer=4.0f;
            return n+1;
        }
    }
    return 0;
}
#ifndef F1C_PIT_HUD_TEST
static inline int f1c_pit_hud_post(const char *msg) {
    unsigned char *inner=*(unsigned char *volatile*)0x007CD204u;
    MEMORY_BASIC_INFORMATION info{};
    if(!inner||!VirtualQuery(inner,&info,sizeof info)||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
       !(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return 0;
    const size_t extent=7*0x5C+0x90+sizeof(float);
    if((uintptr_t)inner<(uintptr_t)info.BaseAddress ||
       (uintptr_t)inner-(uintptr_t)info.BaseAddress+extent>info.RegionSize)return 0;
    return f1c_pit_hud_write_slots(inner,msg);
}
#endif
