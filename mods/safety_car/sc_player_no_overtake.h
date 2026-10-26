#ifndef SC_PLAYER_NO_OVERTAKE_H
#define SC_PLAYER_NO_OVERTAKE_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define SC_NP_MAX_TARGETS 128
#define SC_NP_GIVEBACK_SECONDS 3.0
#define SC_NP_MAX_SAMPLE_SECONDS 0.5
#define SC_NP_CROSS_METRES 2.0
#define SC_NP_NEAR_METRES 60.0

typedef enum {
    SC_NP_NONE=0, SC_NP_WARNING_1=1, SC_NP_WARNING_2=2,
    SC_NP_STOP_GO=3, SC_NP_DISQUALIFY=4
} SCNPAction;

/* Supply a fixed slot-indexed array each tick. signed_gap_metres is target-relative continuous lap progress. Negative means
 * the player is behind. The adapter must prove same race, on-track motion,
 * healthy upright target and non-pit intent before setting eligible. A lapped
 * target is never eligible. Safety Car eligibility ends at XPITIN. */
typedef struct {
    uint64_t identity;
    double signed_gap_metres;
    int eligible;
} SCNPTarget;

typedef struct {
    int identity_valid,active,lap_relation_valid,lapped;
    int pit_called,pit_state,spun,incident,damaged,off_track;
    int upright_valid,upright,is_safety_car,safety_car_xpitin;
    double speed_metres_per_second;
} SCNPTargetStatus;

/* Conservative target filter. The Safety Car is special: native pit intention
 * during recall does not authorize a pass until the observed XPITIN handover. */
static int sc_np_target_eligible(const SCNPTargetStatus *t) {
    if(!t||!t->identity_valid||!t->active||!t->lap_relation_valid||t->lapped||
       !isfinite(t->speed_metres_per_second)||
       t->speed_metres_per_second<(t->is_safety_car?0.5:5.0)||
       t->speed_metres_per_second>200.0||
       !t->upright_valid||!t->upright||t->spun||t->incident||
       t->damaged||t->off_track)return 0;
    if(t->is_safety_car)return !t->safety_car_xpitin;
    return !t->pit_called&&t->pit_state==0;
}

typedef struct {
    uint64_t identity;
    double previous_gap,previous_time,pass_time;
    int seen,pending,counted,behind_armed;
} SCNPPair;

typedef struct {
    uint64_t race_id;
    double previous_time;
    int clock_valid,caution_active,violations,terminal;
    SCNPAction awaiting;
    uint64_t awaiting_target;
    int native_in_flight;
    SCNPPair pair[SC_NP_MAX_TARGETS];
} SCNPState;

typedef struct {
    int give_back_visible;
    uint64_t give_back_target;
    double give_back_seconds_left;
    int give_back_countdown; /* visible 3, 2, 1; zero outside grace */
    SCNPAction action;
    uint64_t action_target;
    int violations_committed;
} SCNPOutput;

static void sc_np_reset(SCNPState *s,uint64_t race_id) {
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->race_id=race_id;
}

static void sc_np_clear_pairs(SCNPState *s) {
    memset(s->pair,0,sizeof(s->pair));
    s->awaiting=SC_NP_NONE;
    s->awaiting_target=0;
    s->native_in_flight=0;
}

static void sc_np_drop_awaiting(SCNPState *s) {
    s->awaiting=SC_NP_NONE;
    s->awaiting_target=0;
    s->native_in_flight=0;
}

/* Returns zero if the frame cannot be trusted. The escalation counter is
 * preserved across Safety Car cycles in one race and reset at a new race_id. */
static int sc_np_step(SCNPState *s,uint64_t race_id,double now,int caution_active,
                      const SCNPTarget *targets,int n,SCNPOutput *out) {
    int i,j;
    if (!s||!out)return 0;
    if (!race_id||n<0||n>SC_NP_MAX_TARGETS||
        (n&&!targets)||!isfinite(now)||now<0) {
        sc_np_clear_pairs(s);s->clock_valid=0;s->caution_active=0;
        return 0;
    }
    for(i=0;i<n;i++) {
        if(targets[i].identity&&!isfinite(targets[i].signed_gap_metres)){
            sc_np_clear_pairs(s);s->clock_valid=0;s->caution_active=0;return 0;
        }
        for(j=0;j<i;j++)if(targets[i].identity&&
            targets[i].identity==targets[j].identity){
            sc_np_clear_pairs(s);s->clock_valid=0;s->caution_active=0;return 0;
        }
    }
    memset(out,0,sizeof(*out));
    if (s->race_id!=race_id)sc_np_reset(s,race_id);
    if (s->clock_valid&&now<s->previous_time) {
        sc_np_clear_pairs(s);s->clock_valid=0;s->caution_active=0;
        return 0;
    }
    if (s->terminal) {
        s->clock_valid=1;s->previous_time=now;
        out->violations_committed=s->violations;
        return 1;
    }
    if (!caution_active) {
        sc_np_clear_pairs(s);s->caution_active=0;
        s->clock_valid=1;s->previous_time=now;
        out->violations_committed=s->violations;
        return 1;
    }
    if (!s->caution_active) {
        sc_np_clear_pairs(s);
        s->caution_active=1;
    }
    s->clock_valid=1;s->previous_time=now;
    for (i=0;i<n;i++) {
        const SCNPTarget *t=&targets[i];
        SCNPPair *p=&s->pair[i];
        if (!t->identity) {
            if(s->awaiting_target==p->identity)sc_np_drop_awaiting(s);
            memset(p,0,sizeof(*p));continue;
        }
        if (p->identity!=t->identity) {
            if(s->awaiting_target==p->identity)sc_np_drop_awaiting(s);
            memset(p,0,sizeof(*p));p->identity=t->identity;
        }
        if (!t->eligible) {
            if(s->awaiting_target==t->identity&&!s->native_in_flight)
                sc_np_drop_awaiting(s);
            p->seen=p->pending=p->counted=p->behind_armed=0;
            continue;
        }
        double gap=t->signed_gap_metres;
        if (p->seen&&now-p->previous_time>SC_NP_MAX_SAMPLE_SECONDS) {
            if(s->awaiting_target==t->identity&&!s->native_in_flight)
                sc_np_drop_awaiting(s);
            p->seen=p->pending=p->behind_armed=0; /* missed observation cannot prove a pass */
        }
        if (p->pending) {
            if (gap<=-SC_NP_CROSS_METRES) {
                p->pending=0;
                if(s->awaiting_target==t->identity&&!s->native_in_flight){
                    sc_np_drop_awaiting(s);
                }
            }
        }
        if (p->counted && gap<=-SC_NP_CROSS_METRES)p->counted=0;
        /* Keep proof that the player was behind through the intermediate
         * samples inside the crossing deadband. A normal pass does not jump
         * from -2 m to +2 m in a single game tick. */
        if (gap < -SC_NP_NEAR_METRES || gap > SC_NP_NEAR_METRES)
            p->behind_armed=0;
        else if (gap<=-SC_NP_CROSS_METRES)
            p->behind_armed=1;
        else if (!p->pending&&!p->counted&&p->seen&&
                 now>p->previous_time&&p->behind_armed&&
                 gap>=SC_NP_CROSS_METRES) {
            p->pending=1;p->pass_time=now;p->behind_armed=0;
        }
        p->seen=1;p->previous_gap=gap;p->previous_time=now;
    }
    for(i=n;i<SC_NP_MAX_TARGETS;i++) {
        if(s->awaiting_target==s->pair[i].identity)sc_np_drop_awaiting(s);
        memset(&s->pair[i],0,sizeof(s->pair[i]));
    }
    /* One correction window covers every place gained before it expires.
     * Slot order and later passes must not restart the displayed countdown. */
    int oldest=-1;
    for(i=0;i<n;i++)if(s->pair[i].pending&&
       (oldest<0||s->pair[i].pass_time<s->pair[oldest].pass_time))oldest=i;
    if(oldest>=0&&!s->awaiting){
        SCNPPair *p=&s->pair[oldest];
        double left=SC_NP_GIVEBACK_SECONDS-(now-p->pass_time);
        if(left<=1e-6){
            s->awaiting=s->violations<2?(SCNPAction)(s->violations+1):
                (s->violations==2?SC_NP_STOP_GO:SC_NP_DISQUALIFY);
            s->awaiting_target=p->identity;
        }else{
            out->give_back_visible=1;out->give_back_target=p->identity;
            out->give_back_seconds_left=left;
            out->give_back_countdown=(int)ceil(left-1e-6);
            if(out->give_back_countdown<1)out->give_back_countdown=1;
            if(out->give_back_countdown>3)out->give_back_countdown=3;
        }
    }
    out->action=s->awaiting;out->action_target=s->awaiting_target;
    out->violations_committed=s->violations;
    return 1;
}

/* Only confirmed visible warning or native penalty acceptance advances the
 * race counter. One fresh three-second correction window covers all still
 * held places in the incident. Failed delivery never escalates. */
static int sc_np_ack(SCNPState *s,SCNPAction action,uint64_t target,int accepted) {
    int i;
    if (!s||!accepted||action==SC_NP_NONE||
        s->awaiting!=action||s->awaiting_target!=target)return 0;
    for(i=0;i<SC_NP_MAX_TARGETS;i++)if(s->pair[i].identity==target) {
        SCNPPair *p=&s->pair[i];
        p->pending=action!=SC_NP_DISQUALIFY&&p->seen&&
                   p->previous_gap>-SC_NP_CROSS_METRES;
        p->counted=action==SC_NP_DISQUALIFY;
        break;
    }
    if(action!=SC_NP_DISQUALIFY)
        for(i=0;i<SC_NP_MAX_TARGETS;i++)if(s->pair[i].pending)
            s->pair[i].pass_time=s->previous_time;
    if(s->violations<4)s->violations++;
    s->awaiting=SC_NP_NONE;s->awaiting_target=0;
    s->native_in_flight=0;
    if(action==SC_NP_DISQUALIFY){s->terminal=1;sc_np_clear_pairs(s);}
    return 1;
}

/* Once a native event has entered the game's queue, the driver has crossed
 * the grace deadline and the request cannot be withdrawn. A later give-back
 * still stops the *next* escalation, but the accepted penalty is counted. */
static int sc_np_mark_native_in_flight(SCNPState *s,SCNPAction action,uint64_t target) {
    if(!s||s->awaiting!=action||s->awaiting_target!=target||
       (action!=SC_NP_STOP_GO&&action!=SC_NP_DISQUALIFY))return 0;
    s->native_in_flight=1;return 1;
}

#endif
