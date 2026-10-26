#ifndef SC_AUTO_INCIDENT_H
#define SC_AUTO_INCIDENT_H
#include <math.h>
#include <string.h>
#define SC_AUTO_SLOTS 128
#ifndef SC_AUTO_QUIET_SECONDS
#define SC_AUTO_QUIET_SECONDS 30.0
#endif
#ifndef SC_INVERTED_CONFIRM
#define SC_INVERTED_CONFIRM 3.0
#define SC_INVERTED_SPEED 2.0f
#endif
/* Pure policy; caller supplies only observed game facts, no writes. */
typedef struct { int valid,pit; unsigned identity; float speed,distance; int road_known,road_contacts,damage,terminal,restart_pending,start_stall_dns,lap,engine_motion,ai_control,restart_known,is_player,inverted,inverted_near,controlled; } SCAutoCar;
typedef struct { unsigned identity; int seen,handled,slow,recovering,stopped,was_inverted,hazard,stall_only,severity,handled_severity; double moving, since,recovery,stop_since; } SCAutoTrack;
typedef struct { int initialized,racing_seen; unsigned epoch; double last,quiet_until,evidence_now,wall_last,race_since; int race_clock,wall_valid; SCAutoTrack car[SC_AUTO_SLOTS]; } SCAutoState;
typedef struct { int fire,reason,slot,count; double held,required; } SCAutoResult;
static SCAutoResult sc_auto_step_observed(SCAutoState *s,const SCAutoCar *c,int n,
 unsigned epoch,double now,int enabled,int idle,int yellow,int redflag,float lap,double wall_now) {
 SCAutoResult r={0,0,-1,0,0,8};
 if(!isfinite(now)||n<0||n>SC_AUTO_SLOTS){memset(s,0,sizeof(*s));return r;}
 if(!s->initialized||s->epoch!=epoch||now<s->last){
   memset(s,0,sizeof(*s));s->initialized=1;s->epoch=epoch;s->quiet_until=now;
 }
 /* Observed callback freshness is wall time, while race dwell advances with
    simulation. Cap credited time per observation: fast-forward cannot count
    an unseen whole incident, and must still supply repeated matching samples. */
 int stale=s->wall_valid && (!isfinite(wall_now) || wall_now<s->wall_last || wall_now-s->wall_last>2.0);
 double dt=now-s->last;
 if(stale){
   for(int i=0;i<SC_AUTO_SLOTS;i++){unsigned id=s->car[i].identity;int handled=s->car[i].handled,seen=s->car[i].seen,severity=s->car[i].handled_severity;
     memset(&s->car[i],0,sizeof(s->car[i]));s->car[i].identity=id;s->car[i].handled=handled;s->car[i].seen=seen;s->car[i].handled_severity=severity;}
 }
 if(isfinite(wall_now)){s->wall_last=wall_now;s->wall_valid=1;}
 if(!stale && dt>0)s->evidence_now+=fmin(dt,2.0);
 double evidence=s->evidence_now;
 s->last=now;
 if((!idle&&s->racing_seen)||redflag)s->quiet_until=now+SC_AUTO_QUIET_SECONDS;
 if(idle&&enabled)s->racing_seen=1;
 if(enabled){if(!s->race_clock){s->race_clock=1;s->race_since=evidence;}}
 else {s->race_clock=0;s->race_since=evidence;}
 int field_n=0,departed=0;
 for(int i=0;i<n;i++)if(c[i].valid&&!c[i].is_player&&!c[i].pit&&!c[i].terminal&&isfinite(c[i].speed)){
   field_n++;
   if(c[i].speed>=12.0f && isfinite(c[i].distance) && (c[i].distance>=100.0f||c[i].lap>0))departed++;
 }
 int field_departed=(departed>=3 && departed*2>field_n) || (s->race_clock && evidence-s->race_since>=15.0);
 int gate=enabled&&idle&&!redflag&&now>=s->quiet_until;
 for(int i=0;i<n;i++){
   SCAutoTrack *t=&s->car[i];
   if(!c[i].valid||!isfinite(c[i].speed)||c[i].speed<0||
      !isfinite(c[i].distance)||c[i].distance<0){
      unsigned id=t->identity;int seen=t->seen,handled=t->handled,severity=t->handled_severity;
      memset(t,0,sizeof(*t));t->identity=id;t->seen=seen;t->handled=handled;t->handled_severity=severity;continue;
   }
   if(t->identity!=c[i].identity){memset(t,0,sizeof(*t));t->identity=c[i].identity;}
   if(c[i].pit){unsigned id=t->identity;int handled=t->handled,severity=t->handled_severity;
     memset(t,0,sizeof(*t));t->identity=id;t->handled=handled;t->handled_severity=severity;continue;}
   if(!c[i].terminal && c[i].inverted==0 && c[i].speed>=12){
     if(!t->recovering){t->recovery=evidence;t->recovering=1;}
     if(evidence-t->recovery>=10)t->handled=0;
     t->moving=evidence;t->seen=1;
   } else t->recovering=0;
   if(t->was_inverted!=c[i].inverted){t->slow=0;t->stopped=0;t->since=evidence;}
   t->was_inverted=c[i].inverted;
   int inverted_hazard=c[i].inverted==1 && c[i].inverted_near && c[i].speed<SC_INVERTED_SPEED;
   int hazard=inverted_hazard || (c[i].road_known && c[i].road_contacts>0 &&
       (c[i].terminal || c[i].speed<1.5f || (t->stopped&&c[i].speed<2.0f) ||
        (c[i].damage&&c[i].speed<8.0f)));
   /* A post-GREEN grace must not erase a proven moving identity after forty
      seconds: recovery/handled state, epoch and freshness already bound it. */
   int irrecoverable_stall=c[i].start_stall_dns && c[i].lap==0 && c[i].engine_motion==2;
   int opening_stall=irrecoverable_stall && field_departed && c[i].speed<1.5f;
   /* A zero/unknown engine plus speed alone cannot prove irrecoverability.
      Separate damage/terminal hazards and running-engine obstructions remain. */
   int unscheduled_engine_obstruction=t->seen && c[i].ai_control && c[i].restart_known && c[i].engine_motion==2;
   int cause_known=inverted_hazard || c[i].terminal || c[i].damage || c[i].engine_motion==1 || irrecoverable_stall || unscheduled_engine_obstruction;
   /* Owner policy: player damage, retirement and inversion never authorize SC. */
   int player_eligible=!c[i].is_player;
   /* A healthy car held by our own caution controls is not a new incident.
      Independent damage, terminal state or inversion still takes precedence. */
   int independent_danger=!c[i].controlled||c[i].damage||c[i].terminal||inverted_hazard;
   t->hazard=player_eligible&&independent_danger&&(t->seen||opening_stall||inverted_hazard)&&hazard&&cause_known&&(!c[i].restart_pending||inverted_hazard);
   /* A lone engine-off car is a local problem, including a grid-start stall.
      Two independently proven engine-off obstructions may justify SC. Damage,
      retirement and inversion retain their separate incident policies. */
   t->stall_only=t->hazard && c[i].engine_motion==2 && c[i].speed<2.0f &&
       !c[i].damage && !inverted_hazard;
   int severity=inverted_hazard?4:(c[i].terminal?3:(c[i].damage?2:1));
   t->severity=severity;
   if(t->handled && t->hazard && severity>t->handled_severity && t->handled_severity>0) {
       t->handled=0;t->slow=0;t->stopped=0;t->since=evidence;
   }
   /* Fresh physical danger still needs its full observed confirmation. */
   int fresh_danger=t->hazard && severity>=2 && !t->handled;
   int incident=(gate||(enabled&&idle&&!redflag&&fresh_danger))&&t->hazard&&!t->handled;
   if(!incident){t->slow=0;t->stopped=0;t->since=evidence;continue;}
   if(!t->slow){t->slow=1;t->since=evidence;}
   if(c[i].terminal || c[i].speed<1.5f || (t->stopped&&c[i].speed<2.0f)){if(!t->stopped){t->stopped=1;t->stop_since=evidence;}}else t->stopped=0;
 }
 int stalled_count=0;
 double stalled_oldest=-1.0,stalled_second=-1.0;
 int stalled_unscheduled=0;
 for(int j=0;j<n;j++)if(s->car[j].slow && s->car[j].stopped && s->car[j].stall_only){
   double age=evidence-s->car[j].stop_since;
   stalled_count++;
   if(age>stalled_oldest){stalled_second=stalled_oldest;stalled_oldest=age;}
   else if(age>stalled_second)stalled_second=age;
   if(!c[j].start_stall_dns)stalled_unscheduled=1;
 }
 for(int i=0;i<n;i++){
   SCAutoTrack *t=&s->car[i];if(!t->slow)continue;
   if(t->stall_only && stalled_count<2)continue;
   int count=0;
   for(int j=0;j<n;j++)if(s->car[j].slow){
     float gap=fabsf(c[j].distance-c[i].distance);
     if(isfinite(lap)&&lap>100&&gap<=lap)gap=fminf(gap,lap-gap);
     if(gap<=100)count++;
   }
   int reason=c[i].inverted==1&&c[i].inverted_near?4:
       (t->stall_only?5:(count>=2?2:(t->stopped?1:(c[i].damage?3:0))));
   double need=reason==4?SC_INVERTED_CONFIRM:(reason==1?5:(reason==2?6:8)),held=evidence-(reason==1?t->stop_since:t->since);
   if(reason==5){
     need=stalled_unscheduled?30.0:5.0;
     held=stalled_second;
     count=stalled_count;
   }
   /* Every member of a multi-car group must have persisted, not just its anchor. */
   if(reason==2){
     double longest=-1.0,second=-1.0;
     for(int j=0;j<n;j++)if(s->car[j].slow){
       float gap=fabsf(c[j].distance-c[i].distance);
       if(isfinite(lap)&&lap>100&&gap<=lap)gap=fminf(gap,lap-gap);
       if(gap<=100){double age=evidence-s->car[j].since;
         if(c[j].engine_motion==2 && !c[j].terminal && !c[j].damage && !c[j].start_stall_dns && need<30.0)need=30.0;
         if(age>longest){second=longest;longest=age;}else if(age>second)second=age;
       }
     }
     /* Two proven members: show the second-longest age even before expiry. */
     held=second>=0.0?second:0.0;
   }
   /* A native yellow supports shorter confirmation, not exclusive authority.
      Unknown engine state is never promoted by timer absence alone. */
   if(reason!=4 && !yellow && need<12.0)need=12.0;
   if(reason!=4 && c[i].engine_motion==2 && !c[i].terminal && !c[i].damage && !c[i].start_stall_dns && need<30.0)need=30.0;
   if(reason && (r.slot<0||held/need>r.held/r.required)){r.reason=reason;r.slot=i;r.count=count;r.held=held;r.required=need;}

 }
 if(r.slot>=0&&r.held>=r.required){r.fire=1;}
 return r;
}
/* Compatibility for existing isolated policy callers: wall and sim advance
   together. Runtime must supply observed monotonic wall time separately. */
static SCAutoResult sc_auto_step(SCAutoState *s,const SCAutoCar *c,int n,
 unsigned epoch,double now,int enabled,int idle,int yellow,int redflag,float lap) {
 return sc_auto_step_observed(s,c,n,epoch,now,enabled,idle,yellow,redflag,lap,now);
}
static void sc_auto_handoff_reconcile(SCAutoState *s,double now) {
 /* A handoff cannot prove the original incident recovered. Preserve handled
    identity until observed upright recovery or actual epoch/identity change. */
 for(int i=0;i<SC_AUTO_SLOTS;i++){s->car[i].slow=0;s->car[i].stopped=0;}
 s->quiet_until=now+SC_AUTO_QUIET_SECONDS;
}
static void sc_auto_cover_active(SCAutoState *s){
 for(int i=0;i<SC_AUTO_SLOTS;i++)if(s->car[i].hazard){s->car[i].handled=1;s->car[i].handled_severity=s->car[i].severity;}
}
static void sc_auto_accept(SCAutoState *s){
 for(int i=0;i<SC_AUTO_SLOTS;i++)if(s->car[i].slow){s->car[i].handled=1;s->car[i].handled_severity=s->car[i].severity;s->car[i].slow=0;}
}
#endif
