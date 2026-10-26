#include "sc_config.h"
#ifndef SC_INCIDENT_CLEARANCE_H
#define SC_INCIDENT_CLEARANCE_H

/*
 * Per-caution incident ledger.  It deliberately knows no game addresses: the
 * caller supplies reviewed observations.  Clearance is affirmative and uses
 * simulation time only; no sample, backward clock, epoch change or gap can
 * make an unresolved hazard disappear.
 */
#include <string.h>
#include <math.h>
#define SC_CLEAR_SLOTS 128
#define SC_CLEAR_SECONDS (g_sc_config.SC_CLEAR_SECONDS)
typedef struct { int observed,active,pit,sc; unsigned identity; float speed; int damage; int terminal,disabled,garage;
    /* Road contact is necessary but not corridor geometry.  free_to_progress
       is supplied only when this car had a positive controller/native motion
       allowance; a constrained queue car can never self-declare an incident. */
    int road_known,road_contacts,free_to_progress; float distance,lap_length; int ignore_new,is_player;
} SCClearCar;
typedef struct { int tracked,slot,damage,seen_moving; unsigned identity; } SCClearEntry;
typedef struct { int initialized,last_verdict; unsigned epoch; double last,clear_since,event_since[SC_CLEAR_SLOTS],moving_since[SC_CLEAR_SLOTS],progress_since[SC_CLEAR_SLOTS]; SCClearEntry entry[SC_CLEAR_SLOTS]; int last_damage[SC_CLEAR_SLOTS],last_terminal[SC_CLEAR_SLOTS],seen_moving[SC_CLEAR_SLOTS],damage_event[SC_CLEAR_SLOTS]; float last_distance[SC_CLEAR_SLOTS],progress_m[SC_CLEAR_SLOTS]; unsigned last_identity[SC_CLEAR_SLOTS]; } SCClearState;

static void sc_clear_reset(SCClearState *s) { memset(s,0,sizeof(*s)); }
static void sc_clear_capture(SCClearState *s,const SCClearCar *c,int n,const int *qualifies,unsigned epoch,double now) {
    if(!s||!c||!qualifies||n<0||n>SC_CLEAR_SLOTS||!isfinite(now))return;
    if(!s->initialized||s->epoch!=epoch){sc_clear_reset(s);s->initialized=1;s->epoch=epoch;s->last=now;}
    for(int i=0;i<n;i++) {
        s->last_distance[i]=c[i].observed?c[i].distance:NAN;
        if(c[i].observed){s->last_identity[i]=c[i].identity;s->last_damage[i]=c[i].damage;s->last_terminal[i]=c[i].terminal;s->seen_moving[i]=!c[i].terminal&&!c[i].disabled&&isfinite(c[i].speed)&&c[i].speed>=12.0f;s->moving_since[i]=now;}
        if(qualifies[i]&&!c[i].sc&&!c[i].is_player) {
        SCClearEntry *e=&s->entry[i]; e->tracked=1;e->slot=i;e->identity=c[i].identity;e->damage=c[i].damage;
        if(c[i].speed>=12.0f)e->seen_moving=1;
        }
    }
}
/* Returns 1 only after every recorded or newly corroborated hazard has had
 * five contiguous seconds of positive removal/recovery evidence. */
static int sc_clear_step(SCClearState *s,const SCClearCar *c,int n,unsigned epoch,double now,int active) {
    int unresolved=0;
    double dt;
    if(!s||!c||n<0||n>SC_CLEAR_SLOTS||!isfinite(now)){if(s){s->clear_since=0;s->last_verdict=0;}return 0;}
    if(!active){s->clear_since=0;s->last_verdict=0;return 0;}
    if(!s->initialized||s->epoch!=epoch||now<s->last){
        s->initialized=1;s->epoch=epoch;s->last=now;s->clear_since=0;s->last_verdict=0;return 0;
    }
    /* Several authorized callers may consult one game-tick observation. It is
       idempotent, not a second sample. Pause is handled by the game guard. */
    if(now==s->last)return s->last_verdict;
    dt=now-s->last;
    if(now-s->last>2.0){
        s->clear_since=0;
        for(int i=0;i<SC_CLEAR_SLOTS;i++){s->damage_event[i]=0;s->event_since[i]=0;s->last_distance[i]=NAN;s->progress_since[i]=0;s->progress_m[i]=0;}
    }
    s->last=now;
    for(int i=0;i<SC_CLEAR_SLOTS;i++) {
        SCClearEntry *e=&s->entry[i];
        if(i>=n){s->damage_event[i]=0;s->event_since[i]=0;s->last_distance[i]=NAN;if(e->tracked)unresolved=1;continue;}
        if(!c[i].observed){/* Missing evidence cannot clear a ledger entry or
                              erase a prior movement identity, but it does
                              reset a pending contiguous stopped proof. */
            s->damage_event[i]=0;s->event_since[i]=0;s->progress_since[i]=0;s->progress_m[i]=0;
            s->last_distance[i]=NAN;
            if(e->tracked) unresolved=1;continue;}
        /* Owner policy: a positively identified player never owns a caution
           incident. Retire any old player entry without weakening missing-AI
           evidence or classifying a garaged AI solely from its roster role. */
        if(c[i].is_player){
            memset(e,0,sizeof(*e));s->damage_event[i]=0;s->event_since[i]=0;
            s->seen_moving[i]=0;s->last_identity[i]=c[i].identity;
            s->last_distance[i]=NAN;continue;
        }
        if(s->last_identity[i] && s->last_identity[i]!=c[i].identity){
            s->last_damage[i]=c[i].damage;s->last_terminal[i]=c[i].terminal;s->seen_moving[i]=0;s->damage_event[i]=0;s->event_since[i]=0;
        }
        s->last_identity[i]=c[i].identity;
        /* New road obstructions are independent of a damage edge.  They need
           fresh prior movement, positive road contact, a stopped state and a
           demonstrated freedom to progress.  This deliberately does not guess
           lane occupancy from a surface contact or declare queue crawl. */
        /* Keep identity-scoped movement evidence for this cycle.  A secondary
           terminal transition can occur after a long managed lap; it is not
           made safe merely because the earlier movement was over 40 seconds
           ago.  Missing samples still cannot clear an existing entry. */
        /* Candidate stopped proof is contiguous. A moving, constrained,
           unreadable or off-road sample resets it before it can accumulate. */
        if(!c[i].ignore_new&&!c[i].sc&&c[i].active&&!c[i].pit&&!c[i].garage&&s->seen_moving[i]&&
           c[i].road_known&&c[i].road_contacts>0&&(c[i].terminal||c[i].free_to_progress)&&
           (c[i].terminal || (isfinite(c[i].speed)&&c[i].speed<1.5f))){
            if(!s->damage_event[i]){s->damage_event[i]=1;s->event_since[i]=now;}
        } else if(!c[i].terminal) { s->damage_event[i]=0;s->event_since[i]=0; }
        if(s->damage_event[i]&&now-s->event_since[i]>10.0){s->damage_event[i]=0;s->event_since[i]=0;}
        if(!c[i].ignore_new&&!c[i].sc&&c[i].active&&!c[i].pit&&!c[i].garage&&s->damage_event[i]&&c[i].road_known&&c[i].road_contacts>0&&(c[i].terminal||c[i].free_to_progress)&&
           (c[i].terminal || (isfinite(c[i].speed)&&c[i].speed<1.5f && now-s->event_since[i]>=5.0))){
            e->tracked=1;e->slot=i;e->identity=c[i].identity;e->damage=c[i].damage;
        }
        s->last_damage[i]=c[i].damage;
        s->last_terminal[i]=c[i].terminal;
        if(!c[i].terminal&&!c[i].disabled&&isfinite(c[i].speed)&&c[i].speed>=12.0f){s->seen_moving[i]=1;s->moving_since[i]=now;}
        if(!e->tracked)continue;
        /* Active=-1 is a positive removal; pit ownership and sustained normal
           movement are positive recovery. Generic !moving is not removal. */
        if(!c[i].active)continue;
        if(e->identity!=c[i].identity){unresolved=1;continue;}
        if(c[i].pit||c[i].garage){s->last_distance[i]=NAN;continue;}
        if(!isfinite(c[i].speed)){s->last_distance[i]=NAN;unresolved=1;continue;}
        /* Low-speed SC recovery is affirmative only when a known road car has
           made measurable fresh progress while it was free to do so. */
        if(!c[i].terminal && !c[i].disabled && c[i].speed>=0.5f && c[i].speed<=200.0f &&
           c[i].road_known && c[i].road_contacts>0 && c[i].free_to_progress &&
           dt>0.0 && dt<=2.0 && isfinite(c[i].distance) && isfinite(s->last_distance[i])) {
           double d=(double)c[i].distance-s->last_distance[i];
           if(isfinite(c[i].lap_length)&&c[i].lap_length>=100&&c[i].lap_length<=100000 &&
              d< -c[i].lap_length*.5)d+=c[i].lap_length;
           s->last_distance[i]=c[i].distance;
           /* Rate-based forward progress, followed by the one shared five-second
              all-hazards clock below. One car cannot backdate another's clear. */
           if(d/dt>=0.5 && d/dt<=200.0)continue;
        }
        s->last_distance[i]=c[i].distance;
        if(!c[i].terminal&&!c[i].disabled&&c[i].speed>=12.0f)continue;
        unresolved=1;
    }
    if(unresolved){s->clear_since=0;s->last_verdict=0;return 0;}
    if(!s->clear_since)s->clear_since=now;
    s->last_verdict=now-s->clear_since>=SC_CLEAR_SECONDS;
    return s->last_verdict;
}
static int sc_clear_has_hazard(const SCClearState *s) { for(int i=0;s&&i<SC_CLEAR_SLOTS;i++)if(s->entry[i].tracked)return 1; return 0; }
#endif
