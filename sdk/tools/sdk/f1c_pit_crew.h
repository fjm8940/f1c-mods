/* Read-only snapshot of pit crew channel windows for the supported 32-bit F1C build.
 * Layout evidence: pitstop_modding/docs/HANDOFF_ANIMATION_MAPPING.md and the
 * old crew-dump GUI. Channel values describe animation ranges; they are not
 * calibrated audible seconds. Keep sound scheduling on measured service time.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define F1C_PIT_CREW_MAX_ACTORS 64
#define F1C_PIT_CREW_NAME_SIZE 48
typedef int (*F1CPitCrewRead)(uintptr_t address, void *out, size_t length);
typedef struct F1CPitCrewActorWindow {
    char name[F1C_PIT_CREW_NAME_SIZE];
    float start, end;
    uint8_t channel_count, window_valid;
    int8_t corner; /* 0 FL, 1 FR, 2 RL, 3 RR, -1 other */
} F1CPitCrewActorWindow;
typedef struct F1CPitCrewSnapshot {
    uint32_t actor_count, readable_count, window_count;
    F1CPitCrewActorWindow actors[F1C_PIT_CREW_MAX_ACTORS];
} F1CPitCrewSnapshot;

static inline int f1c_pit_crew_address(uintptr_t base, size_t offset, size_t length,
                                       uintptr_t *address)
{
    if (base < 0x10000u || base > 0x7fff0000u ||
        offset > (size_t)(0x7fff0000u - base) ||
        length > (size_t)(0x7fff0000u - base - offset)) return 0;
    *address = base + offset;
    return 1;
}
static inline int f1c_pit_crew_read_at(F1CPitCrewRead read, uintptr_t base,
                                        size_t offset, void *out, size_t length)
{
    uintptr_t address;
    return read && f1c_pit_crew_address(base, offset, length, &address) &&
           read(address, out, length);
}
static inline int f1c_pit_crew_corner(const char *name)
{
    if (strstr(name,"FRONT_LEFT")) return 0;
    if (strstr(name,"FRONT_RIGHT")) return 1;
    if (strstr(name,"REAR_LEFT")) return 2;
    if (strstr(name,"REAR_RIGHT")) return 3;
    return -1;
}
static inline int f1c_pit_crew_snapshot(F1CPitCrewRead read, uintptr_t vehicle,
                                         F1CPitCrewSnapshot *out)
{
    uint32_t crew=0, array=0, count=0;
    if (!out) return 0;
    memset(out,0,sizeof *out);
    if (!f1c_pit_crew_read_at(read,vehicle,0x32d4u,&crew,sizeof crew) || !crew ||
        !f1c_pit_crew_read_at(read,crew,0x0cu,&count,sizeof count) ||
        !f1c_pit_crew_read_at(read,crew,0x10u,&array,sizeof array) ||
        !array || !count || count>F1C_PIT_CREW_MAX_ACTORS) return 0;
    out->actor_count=count;
    for (uint32_t i=0;i<count;i++) {
        const size_t offset=(size_t)i*120u;
        uint32_t name_ptr=0, channel_ptr=0, channels=0;
        F1CPitCrewActorWindow *actor=&out->actors[i];
        actor->corner=-1;
        if (!f1c_pit_crew_read_at(read,array,offset+100u,&name_ptr,sizeof name_ptr) ||
            !f1c_pit_crew_read_at(read,array,offset+112u,&channels,sizeof channels) ||
            !f1c_pit_crew_read_at(read,array,offset+116u,&channel_ptr,sizeof channel_ptr)) continue;
        if (!name_ptr || channels>32 || (channels && !channel_ptr)) continue;
        int name_ok=0;
        for (size_t j=0;j<F1C_PIT_CREW_NAME_SIZE;j++) {
            char ch=0;
            if (!f1c_pit_crew_read_at(read,name_ptr,j,&ch,1)) break;
            actor->name[j]=ch;
            if (!ch) {name_ok=j>0;break;}
            if ((unsigned char)ch<32 || (unsigned char)ch>126) break;
        }
        if (!name_ok) {actor->name[0]=0;continue;}
        actor->corner=(int8_t)f1c_pit_crew_corner(actor->name);
        actor->channel_count=(uint8_t)channels;
        out->readable_count++;
        float best=0;
        for (uint32_t j=0;j<channels;j++) {
            float start=0,end=0;
            const size_t channel_offset=(size_t)j*28u;
            if (!f1c_pit_crew_read_at(read,channel_ptr,channel_offset+12u,&start,sizeof start) ||
                !f1c_pit_crew_read_at(read,channel_ptr,channel_offset+16u,&end,sizeof end) ||
                !isfinite(start) || !isfinite(end) || start<0 || end>60 || end<=start) continue;
            if (end-start>best) {best=end-start;actor->start=start;actor->end=end;actor->window_valid=1;}
        }
        if (actor->window_valid) out->window_count++;
    }
    return 1;
}
