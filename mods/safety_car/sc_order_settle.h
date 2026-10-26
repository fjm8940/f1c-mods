#ifndef SC_ORDER_SETTLE_H
#define SC_ORDER_SETTLE_H
#include "sc_queue_order.h"
#include <math.h>
typedef struct {float speed; int controlled,inverted;} SCOrderMotion;
typedef struct {int seen,roster,excluded,slow; char name[32]; double stopped,recovery;} SCOrderMember;
typedef struct {unsigned epoch; int active,locked; double last,elapsed,quiet; int position[SC_QUEUE_SLOTS]; SCOrderMember car[SC_QUEUE_SLOTS];} SCOrderSettle;
/* Time is the shared observed simulation clock: pauses contribute zero. */
static int sc_order_settle_step(SCOrderSettle *s,SCQueueCar *c,const SCOrderMotion *m,int n,unsigned epoch,double now,int active,double minimum,double quiet,double stop_seconds,double recover_seconds,float stop_speed,float recover_speed){
 if(n<0||n>SC_QUEUE_SLOTS||!isfinite(now)||!active){memset(s,0,sizeof(*s));return 0;}
 if(!s->active||s->epoch!=epoch||now<s->last){memset(s,0,sizeof(*s));s->active=1;s->epoch=epoch;s->last=now;}
 double dt=fmin(0.25,fmax(0,now-s->last));s->last=now;
 int disturbed=0,changed=0;
 for(int i=0;i<n;i++){
  SCOrderMember *t=&s->car[i];
  if(!c[i].known){disturbed=1;continue;}
  if(!t->seen||t->roster!=c[i].roster||memcmp(t->name,c[i].name,32)){
   memset(t,0,sizeof(*t));t->seen=1;t->roster=c[i].roster;memcpy(t->name,c[i].name,32);disturbed=1;
  }
  if(!c[i].participant||c[i].pit){t->stopped=t->recovery=0;t->slow=0;continue;}
  int valid=isfinite(m[i].speed)&&m[i].speed>=0;
  int disabled=!valid||!m[i].controlled||(m[i].inverted&&m[i].speed<recover_speed);
  int slow=valid&&m[i].speed<stop_speed;
  if(slow&&!t->slow&&!t->excluded)disturbed=1;
  t->slow=slow;t->stopped=slow?t->stopped+dt:0;
  if(disabled||t->stopped>=stop_seconds){
   if(!t->excluded)disturbed=1;
   t->excluded=1;t->recovery=0;
  }else if(t->excluded){
   t->recovery=(valid&&m[i].controlled&&!m[i].inverted&&m[i].speed>=recover_speed)?t->recovery+dt:0;
   if(t->recovery>=recover_seconds){t->excluded=0;t->recovery=0;disturbed=1;}
  }
  /* An unresolved fresh slow car keeps order unlocked until confirmed out
     or moving again. Already excluded cars do not hold the settle clock. */
  if(slow&&!t->excluded)disturbed=1;
  if(t->excluded)c[i].participant=0;
  int position=c[i].participant?c[i].position:0;
  if(position!=s->position[i])changed=1;
  s->position[i]=position;
 }
 if(disturbed){s->locked=0;s->elapsed=0;s->quiet=0;}
 if(!s->locked){s->elapsed+=dt;s->quiet=changed?0:s->quiet+dt;if(s->elapsed>=minimum&&s->quiet>=quiet)s->locked=1;}
 return s->locked;
}
#endif
