#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
/* RF bridge is veto-only. PAUSE belongs to native session authorization,
   never to incident ownership or a post-incident quiet deadline. */
static int sc_rf_incident_veto(const int32_t *a){
 return a[11]!=0||a[12]!=0||a[24]!=0||a[25]!=0;
}
/* Millisecond presentation/legacy deadlines use the same simulation clock.
   Unsigned subtraction retains Windows tick-wrap behavior. Zero is unset. */
static uint32_t sc_sim_millis(double now){
 if(!isfinite(now)||now<0)return 0;
 return (uint32_t)fmod(floor(fmod(now,4294967.296)*1000.0)+1.0,4294967296.0);
}
typedef struct {unsigned epoch;double last;int valid;} SCSimTick;
static int sc_sim_tick_advance(SCSimTick *s,unsigned epoch,double now){
 if(!isfinite(now)||now<0)return 0;
 if(!s->valid||s->epoch!=epoch){s->valid=1;s->epoch=epoch;s->last=now;return 1;}
 if(now==s->last)return 0;
 if(now<s->last){s->last=now;return 0;}
 s->last=now;return 1;
}

/* Bounded observed simulation clock for weather and clearance evidence.
 * A genuine wall observation outage emits a >2-second discontinuity, allowing
 * existing policies to reset dwell while preserving their hazard ledger. */
typedef struct {int valid;unsigned epoch;double sim,wall,value;} SCEvidenceClock;
static double sc_evidence_clock_step(SCEvidenceClock *s,unsigned epoch,double sim,double wall){
 if(!isfinite(sim)||!isfinite(wall))return s->value;
 if(!s->valid||s->epoch!=epoch||sim<s->sim){memset(s,0,sizeof(*s));s->valid=1;s->epoch=epoch;s->sim=sim;s->wall=wall;s->value=0.001;return s->value;}
 if(sim>s->sim){double dt=sim-s->sim;s->value+=(wall<s->wall||wall-s->wall>2.0)?3.0:fmin(dt,2.0);s->sim=sim;s->wall=wall;}
 return s->value;
}
