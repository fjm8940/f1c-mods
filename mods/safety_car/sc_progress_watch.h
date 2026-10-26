#ifndef SC_PROGRESS_WATCH_H
#define SC_PROGRESS_WATCH_H
#include <math.h>
#include "sc_config.h"
#include <string.h>
typedef struct { unsigned epoch;int phase,valid;double last,elapsed,stalled;float distance,travel; } SCProgressWatch;
/* Simulation observation watchdog only. Failure never teleports, retries a
   native reset, clears a caution, or pretends the field can go GREEN. */
static int sc_progress_step(SCProgressWatch *s,unsigned epoch,int phase,double now,float distance,float lap){
    if(!s)return 0;
    if(phase==1){distance=0;lap=100;} /* ARM waits need time, no garage distance */
    if(!isfinite(now)||!isfinite(distance)||!isfinite(lap)||lap<100||lap>100000||distance<0||distance>=lap){
        s->valid=0;return 1;
    }
    if(!s->valid||s->epoch!=epoch||s->phase!=phase){
        memset(s,0,sizeof(*s));s->epoch=epoch;s->phase=phase;s->last=now;s->distance=distance;s->valid=1;return 0;
    }
    double dt=now-s->last;s->last=now;
    float d=distance-s->distance;s->distance=distance;
    if(dt<=0)return 0;
    if(dt>2.0){s->elapsed=s->stalled=0;s->travel=0;return 0;}
    if(d< -lap*.5f)d+=lap;
    s->elapsed+=dt;s->stalled+=dt;
    if(d>=0 && d<=200.0*dt+1.0)s->travel+=d;
    else {s->travel=0;s->stalled=0;}
    if(s->travel>=2.0f){s->travel=0;s->stalled=0;}
    if(phase==1)return s->elapsed>=g_sc_config.ProgressArmSeconds?2:0;
    if(phase==2||phase==3)return s->stalled>=g_sc_config.ProgressStallSeconds?3:(s->elapsed>=g_sc_config.ProgressTravelSeconds?2:0);
    /* Normal pacing may last many laps; only sustained lack of progress is
       a failure on track or on the return. No overall pacing time limit. */
    if(phase==4||phase==5)return s->stalled>=g_sc_config.ProgressStallSeconds?3:0;
    return 0;
}
#endif
