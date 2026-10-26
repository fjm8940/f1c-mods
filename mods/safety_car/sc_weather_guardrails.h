#include "sc_config.h"
#pragma once
#include <string.h>
#include <math.h>
/* Test calibration, normalized racing-path wetness. Simulation seconds. */
#define SC_WEATHER_DEPLOY (g_sc_config.SC_WEATHER_DEPLOY)
#define SC_WEATHER_RECOVER (g_sc_config.SC_WEATHER_RECOVER)
#define SC_WEATHER_BAD_SECONDS (g_sc_config.SC_WEATHER_BAD_SECONDS)
#define SC_WEATHER_GOOD_SECONDS (g_sc_config.SC_WEATHER_GOOD_SECONDS)
typedef struct {
    int initialized,hold,bad,good,pending,pending_fresh,handled;
    unsigned epoch;
    double last,bad_since,good_since;
} SCWeatherState;
static void sc_weather_step(SCWeatherState *s,unsigned epoch,double now,
                           int racing,int enabled,int veto,int active,int valid,float wet) {
    if(!racing || !isfinite(now)) { memset(s,0,sizeof(*s)); return; }
    if(!s->initialized || s->epoch!=epoch || now<s->last) {
        memset(s,0,sizeof(*s));s->initialized=1;s->epoch=epoch;s->last=now;
    }
    /* One millionth tolerance avoids x87 float storage rounding at exact thresholds. */
    /* An observation gap is not proof of continuous weather. */
    if(now-s->last>2.0){s->bad=0;s->good=0;s->pending=0;s->pending_fresh=0;}
    if(!valid || !isfinite(wet) || wet<0 || wet>1) {
        s->bad=0;s->good=0;s->pending=0;s->pending_fresh=0;s->last=now;return;
    }
    s->last=now;
    if(active && s->hold){s->handled=1;s->pending=0;s->pending_fresh=0;}
    if(s->hold) {
        /* A retained inactive hold is not permission to deploy. Re-confirm a
           new request from contiguous fresh bad-weather samples. */
        if(!s->handled && !active && !s->pending && enabled && !veto &&
           (double)wet>=SC_WEATHER_DEPLOY-0.000001) {
            if(!s->bad){s->bad=1;s->bad_since=now;}
            if(now-s->bad_since>=SC_WEATHER_BAD_SECONDS){s->pending=1;s->pending_fresh=1;s->good=0;}
        } else if(!active && !s->pending) s->bad=0;
        if((double)wet<=SC_WEATHER_RECOVER+0.000001 && !veto) {
            if(!s->good){s->good=1;s->good_since=now;}
            if(now-s->good_since>=SC_WEATHER_GOOD_SECONDS){s->handled=0;s->hold=0;s->pending=0;s->pending_fresh=0;s->bad=0;s->good=0;}
        } else s->good=0;
    } else {
        if(enabled && !veto && (double)wet>=SC_WEATHER_DEPLOY-0.000001) {
            if(!s->bad){s->bad=1;s->bad_since=now;}
            if(now-s->bad_since>=SC_WEATHER_BAD_SECONDS){s->hold=1;s->pending=1;s->pending_fresh=1;s->good=0;}
        } else s->bad=0;
    }
    /* An active hold survives unreadable weather; a new departure does not.
       Disable and RF veto revoke only the uncommitted pending request. */
    if(!enabled || veto){s->pending=0;s->pending_fresh=0;s->bad=0;}
}
