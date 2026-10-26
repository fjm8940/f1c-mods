#ifndef SC_ORDER_RECOVERY_H
#define SC_ORDER_RECOVERY_H
#include <math.h>
#include "sc_catchup.h"
#include <string.h>
#define SC_ORDER_SLOTS 128
#define SC_ORDER_FREE 99999.0f
typedef struct { int valid, rank; float distance, speed; int lap,lap_known; } SCOrderCar;
typedef struct { unsigned char yielding[SC_ORDER_SLOTS], reclaim[SC_ORDER_SLOTS]; float cap[SC_ORDER_SLOTS]; int active, guard; } SCOrderRecovery;
static void sc_order_reset(SCOrderRecovery *s) { memset(s,0,sizeof(*s)); }
static float sc_order_delta(float a,float b,float lap) {
    float d=fmodf(a-b,lap);
    if(d>lap*0.5f)d-=lap;
    if(d< -lap*0.5f)d+=lap;
    return d;
}
static int sc_order_different_lap(const SCOrderCar *a,const SCOrderCar *b,float lap){
    /* The counters can differ by one at the line without a car being lapped.
       Combine counters and distance; use no modulo to erase that distinction. */
    if(!a->lap_known||!b->lap_known)return 0;
    double separation=((double)a->lap-b->lap)*lap+a->distance-b->distance;
    return isfinite(separation)&&fabs(separation)>lap*0.5;
}
/* Compare saved neighbours in race progress, not independently wrapped SC
   coordinates: lapped cars on opposite sides of the SC are not an overtake. */
static float sc_order_pair_gap(const SCOrderCar *a,const SCOrderCar *b,
                               float wrapped,float lap){
    if(a->lap_known && b->lap_known){
        double progress=((double)a->lap-b->lap)*lap+a->distance-b->distance;
        if(isfinite(progress)){
            if(sc_order_different_lap(a,b,lap)){
                float forward=fmodf(a->distance-b->distance+lap,lap);
                return forward;
            }
            return (float)progress;
        }
    }
    return wrapped;
}
/* Pure snapshot planner. Caps are advisory native speed inputs, not motion writes. */
static void sc_order_plan_strict(SCOrderRecovery *s,const SCOrderCar *cars,int n,
                          int enabled,float scdist,float scspeed,float lap,int catch_leader) {
    int order[SC_ORDER_SLOTS], count=0,front=-1;
    float x[SC_ORDER_SLOTS];
    int was_active=s->active;
    s->active=0;
    s->guard=0;
    memset(s->reclaim,0,sizeof(s->reclaim));
    for(int i=0;i<SC_ORDER_SLOTS;i++)s->cap[i]=SC_ORDER_FREE;
    if(!enabled||n<0||n>SC_ORDER_SLOTS||!isfinite(lap)||lap<100||
       !isfinite(scdist)||!isfinite(scspeed)||scspeed<0||scdist<0||scdist>=lap){
        memset(s->yielding,0,sizeof(s->yielding));return;
    }
    s->guard=1;
    for(int i=0;i<n;i++)if(cars[i].valid&&cars[i].rank>0&&
        (front<0||cars[i].rank<cars[front].rank))front=i;
    for(int i=0;i<n;i++) {
        if(!cars[i].valid||cars[i].rank<=0||!isfinite(cars[i].distance)||
           cars[i].distance<0||cars[i].distance>=lap||!isfinite(cars[i].speed)||cars[i].speed<0){
            s->yielding[i]=0;continue;
        }
        x[i]=sc_order_delta(cars[i].distance,scdist,lap);
        /* Nearby overtakes are unambiguous. Do not fold a distant straggler
           into a yielding car just because it is more than half a lap behind. */
        float window=fminf(400.0f,lap*0.25f);
        int separate=front>=0&&sc_order_different_lap(&cars[front],&cars[i],lap);
        if(separate)s->yielding[i]=0; /* lapped traffic is not a proved SC pass */
        if(!separate && x[i]>=0 && (x[i]<=window || i==catch_leader))s->yielding[i]=1;
        if(x[i]<=-40.0f)s->yielding[i]=0;
        if(!s->yielding[i] && x[i]>0) x[i]-=lap;
        s->active|=s->yielding[i];
        int k=count++;
        while(k>0 && (cars[order[k-1]].rank>cars[i].rank ||
              (cars[order[k-1]].rank==cars[i].rank && order[k-1]>i))){order[k]=order[k-1];k--;}
        order[k]=i;
    }
    for(int i=n;i<SC_ORDER_SLOTS;i++)s->yielding[i]=0;
    /* Do not release recovery just because everyone crossed behind the SC:
       a lower-ranked car may still be ahead of its rightful predecessor. */
    for(int k=1;k<count;k++) {
        if(sc_order_different_lap(&cars[order[k-1]],&cars[order[k]],lap))continue;
        float gap=sc_order_pair_gap(&cars[order[k-1]],&cars[order[k]],x[order[k-1]]-x[order[k]],lap);
        if(gap<0.0f || (was_active && gap<15.0f)) {
            s->active=1;
            s->reclaim[order[k-1]]=1;
        }
    }
    /* Always reserve the saved predecessor's space, including before the
       first incident. Recovery state is diagnostic, not the guard switch. */
    int prev=-1;
    for(int k=0;k<count;k++) {
        int i=order[k];
        float cap=SC_ORDER_FREE;
        if(s->yielding[i])cap=fmaxf(3.0f,scspeed*0.65f);
        if(prev<0) {
            /* P1 approaches the SC after it has passed, without a speed floor
               that could override the recovery deficit. */
            if(x[i]<0)cap=fminf(cap,fmaxf(3.0f,sc_catchup_limit(scspeed,-x[i]-40.0f,0.25f)));
        } else {
            /* A predecessor behind this car needs space to reclaim its place.
               Negative gap deliberately makes the lower rank yield harder. */
            float gap=sc_order_pair_gap(&cars[prev],&cars[i],x[prev]-x[i],lap);
            /* Keep physically interleaved lapped cars in place. Classification
               rank does not grant a right to pass them or force them to yield.
               The physical approach controller still respects their bodies. */
            if(sc_order_different_lap(&cars[prev],&cars[i],lap) && gap<0.0f)gap+=lap;
            /* React to actual motion, while anticipating a predecessor's
               braking. Do not subtract every compact spacing deficit from an
               already reduced command down the whole queue in the same tick. */
            float desired=sc_catchup_limit(cars[prev].speed,gap-20.0f,0.25f);
            float allowance=desired-cars[prev].speed;
            float limit=fminf(desired,s->cap[prev]+fmaxf(0.0f,allowance));
            if(gap<0.0f)limit=fminf(limit,s->cap[prev]+allowance);
            cap=fminf(cap,fmaxf(3.0f,limit));
        }
        s->cap[i]=cap;
        prev=i;
    }
}
static void sc_order_plan_relaxed(SCOrderRecovery *s,const SCOrderCar *cars,int n,
                          int enabled,float scdist,float scspeed,float lap,int catch_leader) {
    int order[SC_ORDER_SLOTS], count=0,front=-1;
    float x[SC_ORDER_SLOTS];
    s->active=0;
    s->guard=0;
    memset(s->reclaim,0,sizeof(s->reclaim));
    for(int i=0;i<SC_ORDER_SLOTS;i++)s->cap[i]=SC_ORDER_FREE;
    if(!enabled||n<0||n>SC_ORDER_SLOTS||!isfinite(lap)||lap<100||
       !isfinite(scdist)||!isfinite(scspeed)||scspeed<0||scdist<0||scdist>=lap){
        memset(s->yielding,0,sizeof(s->yielding));return;
    }
    s->guard=1;
    for(int i=0;i<n;i++)if(cars[i].valid&&cars[i].rank>0&&
        (front<0||cars[i].rank<cars[front].rank))front=i;
    for(int i=0;i<n;i++) {
        if(!cars[i].valid||cars[i].rank<=0||!isfinite(cars[i].distance)||
           cars[i].distance<0||cars[i].distance>=lap||!isfinite(cars[i].speed)||cars[i].speed<0){
            s->yielding[i]=0;continue;
        }
        x[i]=sc_order_delta(cars[i].distance,scdist,lap);
        /* Nearby overtakes are unambiguous. Do not fold a distant straggler
           into a yielding car just because it is more than half a lap behind. */
        float window=fminf(400.0f,lap*0.25f);
        int separate=front>=0&&sc_order_different_lap(&cars[front],&cars[i],lap);
        if(separate)s->yielding[i]=0; /* lapped traffic is not a proved SC pass */
        if(!separate && x[i]>=0 && (x[i]<=window || i==catch_leader))s->yielding[i]=1;
        if(x[i]<=-40.0f)s->yielding[i]=0;
        if(!s->yielding[i] && x[i]>0) x[i]-=lap;
        s->active|=s->yielding[i];
        int k=count++;
        while(k>0 && (cars[order[k-1]].rank>cars[i].rank ||
              (cars[order[k-1]].rank==cars[i].rank && order[k-1]>i))){order[k]=order[k-1];k--;}
        order[k]=i;
    }
    for(int i=n;i<SC_ORDER_SLOTS;i++)s->yielding[i]=0;
    /* Saved ranks may be stale after a spin, stop or lawful incident pass.
       Never request position reclamation or make a car wait for one behind.
       The physical approach controller separately prevents new passes. */
    int prev=-1;
    for(int k=0;k<count;k++) {
        int i=order[k];
        float cap=SC_ORDER_FREE;
        if(s->yielding[i])cap=fmaxf(3.0f,scspeed*0.65f);
        if(prev<0) {
            /* P1 approaches the SC after it has passed, without a speed floor
               that could override the recovery deficit. */
            if(x[i]<0)cap=fminf(cap,fmaxf(3.0f,sc_catchup_limit(scspeed,-x[i]-40.0f,0.25f)));
        } else {
            float gap=sc_order_pair_gap(&cars[prev],&cars[i],x[prev]-x[i],lap);
            /* Only a predecessor actually ahead can limit this car. Do not
               cascade its command: that command may itself be constrained by
               another saved neighbour behind it. Use observed motion and
               braking space; the physical controller handles nearer cars. */
            if(gap>=0.0f){
                float limit=sc_catchup_limit(cars[prev].speed,gap-20.0f,0.25f);
                cap=fminf(cap,fmaxf(3.0f,limit));
            }

        }
        s->cap[i]=cap;
        prev=i;
    }
}

static int sc_order_strict=0;
static void sc_order_plan(SCOrderRecovery *s,const SCOrderCar *c,int n,int enabled,float distance,float speed,float lap,int leader){
 if(sc_order_strict)sc_order_plan_strict(s,c,n,enabled,distance,speed,lap,leader);
 else sc_order_plan_relaxed(s,c,n,enabled,distance,speed,lap,leader);
}
#endif
