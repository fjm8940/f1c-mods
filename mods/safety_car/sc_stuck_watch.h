#ifndef SC_STUCK_WATCH_H
#define SC_STUCK_WATCH_H
#include <math.h>
typedef struct { double last, elapsed; int valid; } SCStuckWatch;
static int sc_stuck_step(SCStuckWatch *s,double now,float speed,float command) {
    if(!isfinite(now)||!isfinite(speed)||!isfinite(command)||speed<0) {
        s->valid=0;s->elapsed=0;return 0;
    }
    double dt=s->valid?now-s->last:0;
    s->last=now;s->valid=1;
    /* Pauses add zero; session rollback cancels the previous observation. */
    /* A sample gap is unknown time, not stationary proof. */
    if(dt<0 || dt>2.0){s->elapsed=0;dt=0;}
    /* A crawling/yielding car is not a wreck. Require near-stationary motion
       while our previous command actually permits the car to move. */
    if(speed>=1.0f || command<8.0f){s->elapsed=0;return 0;}
    s->elapsed+=dt;
    return s->elapsed>=12.0;
}
#endif
