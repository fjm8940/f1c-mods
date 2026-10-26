#ifndef SC_STINT_H
#define SC_STINT_H
#include <math.h>
#include <string.h>
typedef struct {int begun,valid;unsigned epoch;double last,travel;float distance;} SCStint;
static void sc_stint_begin(SCStint*s,unsigned epoch){memset(s,0,sizeof(*s));s->begun=1;s->epoch=epoch;}
/* Request entry on the final permitted circuit, leaving a native pit approach
   rather than teleporting at an arbitrary distance. Departure partial lap is
   included. Accumulate SC distance, not the lap counter forced to the leader. */
static int sc_stint_step(SCStint*s,unsigned epoch,double now,float distance,float length,int maximum){
    if(!s->begun || s->epoch!=epoch || maximum<1 || !isfinite(now) ||
       !isfinite(distance)||!isfinite(length)||length<100||distance<0||distance>=length)return 0;
    if(!s->valid){s->valid=1;s->last=now;s->distance=distance;return maximum==1;}
    double dt=now-s->last;if(dt<=0)return 0;
    float delta=distance-s->distance;s->last=now;s->distance=distance;
    if(delta < -length*.5f)delta+=length;
    if(dt<=2 && delta>=0 && delta<=200*dt+1)s->travel+=delta;
    return s->travel >= (maximum-1)*length;
}
#endif
