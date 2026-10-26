#ifndef RF_COMPLETION_POLICY_H
#define RF_COMPLETION_POLICY_H
#include <stdint.h>
#include <math.h>
/* Pure policy: active_ms must exclude pause and unobserved gaps. */
typedef struct {
    int required, good, prompt, overridden;
    uint32_t good_since, prompt_since;
} RfWeatherRestart;
enum { RF_WEATHER_WAIT=0, RF_WEATHER_READY=1, RF_WEATHER_CONFIRM=2 };
static int rf_weather_restart_step(RfWeatherRestart *s,uint32_t active_ms,
        int paused,int fresh,float wet,float resume_max,uint32_t dwell_ms,
        int input_enabled,int key_edge,int override_enabled) {
    if(!s || !isfinite(resume_max) || resume_max<0 || resume_max>=0.96f ||
       dwell_ms<1000u)return RF_WEATHER_WAIT;
    if(!s->required)return RF_WEATHER_READY;
    if(paused)return RF_WEATHER_WAIT;
    if(s->overridden)return RF_WEATHER_READY;
    if(!fresh || !isfinite(wet) || wet<0 || wet>1 || wet>resume_max) {
        s->good=0;
    } else {
        if(!s->good){s->good=1;s->good_since=active_ms;}
        if((uint32_t)(active_ms-s->good_since)>=dwell_ms) {
            s->prompt=0;return RF_WEATHER_READY;
        }
    }
    if(s->prompt && (uint32_t)(active_ms-s->prompt_since)>=15000u)s->prompt=0;
    if(!input_enabled || !override_enabled)s->prompt=0;
    else if(key_edge) {
        if(s->prompt){s->overridden=1;s->prompt=0;return RF_WEATHER_READY;}
        s->prompt=1;s->prompt_since=active_ms;
    }
    return s->prompt?RF_WEATHER_CONFIRM:RF_WEATHER_WAIT;
}
typedef struct {int waiting,failed;uint32_t since;} RfHandoffWait;
/* Timeout never transfers a lease or asserts physical parking succeeded. */
static int rf_handoff_wait_step(RfHandoffWait *s,uint32_t active_ms,
        int waiting,int paused,int acquired,uint32_t timeout_ms) {
    if(!s)return 0;
    if(s->failed)return 0;
    if(acquired){s->waiting=0;return 1;}
    if(!waiting){s->waiting=0;return 0;}
    if(paused)return 0;
    if(!s->waiting){s->waiting=1;s->since=active_ms;}
    if((uint32_t)(active_ms-s->since)>=timeout_ms)s->failed=1;
    return 0;
}
#endif
