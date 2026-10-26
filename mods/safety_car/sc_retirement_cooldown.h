#ifndef SC_RETIREMENT_COOLDOWN_H
#define SC_RETIREMENT_COOLDOWN_H
#include <math.h>
#include <string.h>
#define SC_RETIRE_SLOTS 128
typedef struct {unsigned identity;int healthy,event;} SCRetirementEvent;
typedef struct {unsigned epoch;int initialized,was_caution;double last,until;SCRetirementEvent car[SC_RETIRE_SLOTS];} SCRetirementCooldown;
static void sc_retirement_clock(SCRetirementCooldown *s,unsigned epoch,double now,int caution,double seconds){
 if(!isfinite(now))return;
 if(!s->initialized||s->epoch!=epoch||now<s->last){memset(s,0,sizeof(*s));s->initialized=1;s->epoch=epoch;}
 if(s->was_caution&&!caution)s->until=now+seconds;
 s->was_caution=caution;s->last=now;
}
/* Observe event identity; ignore only affirmatively non-critical geometry.
   An existing ledger entry or new danger always wins over cooldown. */
static int sc_retirement_observe(SCRetirementCooldown *s,int slot,unsigned identity,int valid,int caution,int failed,float speed,int geometry_known,int dangerous,int already_tracked){
 if(slot<0||slot>=SC_RETIRE_SLOTS||!valid)return 0;
 SCRetirementEvent *t=&s->car[slot];
 if(t->identity!=identity){memset(t,0,sizeof(*t));t->identity=identity;}
 if(!failed&&isfinite(speed)&&speed>=12){t->healthy=1;t->event=0;}
 if(t->healthy&&failed&&(caution||s->last<s->until))t->event=1;
 if(!caution&&s->last>=s->until)t->event=0;
 return t->event&&geometry_known&&!dangerous&&!already_tracked;
}
#endif
