#ifndef SC_ENGINE_STOP_WATCH_H
#define SC_ENGINE_STOP_WATCH_H
#include <math.h>
#include <string.h>
typedef struct { unsigned epoch,identity; int valid,announced; double last,elapsed; } SCEngineStopWatch;
static int sc_engine_stop_confirm(SCEngineStopWatch *s,unsigned epoch,unsigned identity,
                                  double now,int eligible,double seconds) {
    if(!eligible||!isfinite(now)||!isfinite(seconds)||seconds<2||seconds>15) {
        memset(s,0,sizeof(*s));return 0;
    }
    if(!s->valid||s->epoch!=epoch||s->identity!=identity||now<s->last||now-s->last>2) {
        memset(s,0,sizeof(*s));s->valid=1;s->epoch=epoch;s->identity=identity;s->last=now;return 0;
    }
    double dt=now-s->last;s->last=now;
    if(dt<=0)return 0;
    s->elapsed+=dt;
    return s->elapsed>=seconds;
}
#endif
