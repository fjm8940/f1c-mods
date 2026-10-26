#pragma once
/* Original corner HUD labels and multi-stage wheel feedback, now exclusively
 * driven by policy commands. No legacy RNG, duration extension or escalation. */
#include "pit_live_voice.h"
struct LegacyFeedback { int eid=-1; int burst=-1; const char *message=nullptr; };
static inline LegacyFeedback legacy_feedback(const pf_plan_t &p,pf_command_kind_t kind,int attempt,bool player) {
    static const char *failed[]={"WHEEL GUN FAILURE (FL)","WHEEL GUN FAILURE (FR)","WHEEL GUN FAILURE (RL)","WHEEL GUN FAILURE (RR)"};
    static const char *fixed[]={"WHEEL GUN FIXED (FL)","WHEEL GUN FIXED (FR)","WHEEL GUN FIXED (RL)","WHEEL GUN FIXED (RR)"};
    static const char *slow[]={"SLOW WHEEL CHANGE (FL)","SLOW WHEEL CHANGE (FR)","SLOW WHEEL CHANGE (RL)","SLOW WHEEL CHANGE (RR)"};
    static const char *done[]={"WHEEL CHANGE DONE (FL)","WHEEL CHANGE DONE (FR)","WHEEL CHANGE DONE (RL)","WHEEL CHANGE DONE (RR)"};
    LegacyFeedback f;
    if(p.outcome==PF_OUTCOME_CLEAN||p.terminal)return f;
    bool wheel=p.wheel>=0&&p.wheel<4 && (p.outcome==PF_OUTCOME_SLOW_CORNER||p.outcome==PF_OUTCOME_WHEELGUN||p.outcome==PF_OUTCOME_CROSS_THREAD);
    if(kind==PF_CMD_FAULT_NOTICE) {
        f.eid=pit_voice_fault(p,player);
        if(wheel) {
            const int corner[]={63,65,68,70};
            if(player)f.eid=corner[p.wheel];
            f.message=p.outcome==PF_OUTCOME_SLOW_CORNER?slow[p.wheel]:failed[p.wheel];
        } else if(p.outcome==PF_OUTCOME_FUEL_RIG)f.message="FUEL HOSE PROBLEM";
        else if(p.outcome==PF_OUTCOME_STALL_DELAY)f.message="ENGINE STALLED";
    } else if(kind==PF_CMD_RECOVERY_ATTEMPT) {
        if(wheel && p.outcome!=PF_OUTCOME_SLOW_CORNER){f.message=failed[p.wheel];if(player)f.eid=66;}
        else if(p.outcome==PF_OUTCOME_FUEL_RIG)f.message="FUEL HOSE - RETRYING";
        else if(p.outcome==PF_OUTCOME_STALL_DELAY)f.message="ENGINE - RESTARTING";
    } else if(kind==PF_CMD_RECOVERED) {
        f.eid=pit_voice_recovered(p,player);
        f.message=p.outcome==PF_OUTCOME_SLOW_CORNER?(wheel?done[p.wheel]:nullptr):
                  wheel?fixed[p.wheel]:p.outcome==PF_OUTCOME_FUEL_RIG?"FUEL SERVICE RESUMED":"ENGINE RESTARTED";
    }
    if(wheel&&player && (kind==PF_CMD_FAULT_NOTICE||kind==PF_CMD_RECOVERY_ATTEMPT||kind==PF_CMD_RECOVERED))
        f.burst=(p.wheel+attempt)%3; /* Cosmetic variation, no probability RNG consumed. */
    return f;
}
