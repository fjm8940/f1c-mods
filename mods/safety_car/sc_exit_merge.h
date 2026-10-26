#ifndef SC_EXIT_MERGE_H
#define SC_EXIT_MERGE_H
#include <math.h>
typedef struct { int valid, leader; double time; float cap; } SCExitMerge;
static void sc_exit_reset(SCExitMerge *s) { s->valid=0; }
/* Track-projected separation: leave normal caution pace untouched when distant.
   Use a 40m following margin and 2.5m/s2 closing envelope. Native AI retains
   steering/braking. If already passed, the on-track order recovery owns yielding. */
static float sc_exit_step(SCExitMerge *s,int leader,double time,float speed,
                          float gap,float scspeed) {
    if(!isfinite(time)||!isfinite(speed)||speed<0||!isfinite(gap)||gap<0||
       !isfinite(scspeed)||scspeed<0){sc_exit_reset(s);return 99999.0f;}
    float target=fmaxf(3.0f,sqrtf(scspeed*scspeed+5.0f*fmaxf(0.0f,gap-40.0f)));
    if(!s->valid||s->leader!=leader||time<s->time){
        s->cap=fmaxf(speed,target);s->time=time;s->leader=leader;s->valid=1;
    }
    double dt=time-s->time;
    /* No further slowing while paused; no inherited ramp across sessions. */
    if(target<s->cap)s->cap=fmaxf(target,s->cap-3.0f*(float)dt);
    else s->cap=target;
    s->time=time;
    return s->cap;
}
#endif
