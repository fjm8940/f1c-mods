#ifndef SC_RECALL_READINESS_H
#define SC_RECALL_READINESS_H
#include <math.h>
#include <string.h>
typedef struct {unsigned epoch;int active,announced;double last,ready,warning,elapsed;} SCRecallReady;
/* 1 announce once, 2 request native pit return, 0 keep pacing. No GREEN writes. */
static int sc_recall_ready_step(SCRecallReady *s,unsigned epoch,double now,int active,int safe,int formed,int order_locked,int lap_ok,int entry_ok,double ready_seconds,double warning_seconds){
 if(!active||!isfinite(now)){memset(s,0,sizeof(*s));return 0;}
 if(!s->active||s->epoch!=epoch||now<s->last){memset(s,0,sizeof(*s));s->active=1;s->epoch=epoch;s->last=now;}
 double dt=fmin(.25,fmax(0,now-s->last));s->last=now;s->elapsed+=dt;
 if(!safe){s->ready=s->warning=0;s->announced=0;return 0;}
 if(!s->announced){
  s->ready=(formed&&order_locked&&lap_ok)?s->ready+dt:0;
  if(s->ready>=ready_seconds){s->announced=1;s->warning=0;return 1;}
 }else{
  /* Normal tiny gap/order oscillations do not restart the warning. A real
     hazard still cancels above; native pit/leader guards retain GREEN. */
  s->warning+=dt;
  if(s->warning>=warning_seconds&&entry_ok)return 2;
 }
 return 0;
}
#endif
