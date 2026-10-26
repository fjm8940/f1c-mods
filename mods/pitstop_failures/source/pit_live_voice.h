#pragma once
#include <cmath>
#include "pit_failure_policy.h"
#include "f1c_pit.h"
static inline bool pit_voice_call_active(int pit_status,bool snapshot_call,
                                         bool previous_call,bool raw_call,bool gui_arm) {
    /* CallToBox is an input pulse; the game retains the accepted call in its
     * pit state after clearing that input. */
    return pit_status==F1C_PIT_CALLED || pit_status==F1C_PIT_LANE ||
           snapshot_call || previous_call || raw_call || gui_arm;
}
static inline int pit_voice_approach(bool player,bool call_active,bool previous_latch,
                                    bool current_latch,bool near_marker,int pit_status) {
    return player && call_active && pit_status<=2 &&
           (near_marker || (!previous_latch && current_latch)) ? 50 : -1;
    /* PWS_PIT03 near XPITIN, with the native latch as a fallback. */
}
static inline int pit_voice_fuel_clear(bool player,bool fuel_fault,bool spill,
                                       bool ready,bool already,float fuel,float requested) {
    return player && fuel_fault && !spill && ready && !already &&
           std::isfinite(fuel) && std::isfinite(requested) && requested>0 &&
           fuel>=requested-0.15f ? 57 : -1; /* PWS_2001_78 */
}
static inline int pit_voice_fault(const pf_plan_t &p,bool player) {
    if(p.outcome==PF_OUTCOME_CLEAN || p.terminal)return -1;
    if(!player) {
        if(p.outcome==PF_OUTCOME_FUEL_RIG)return 185;
        if(p.outcome==PF_OUTCOME_STALL_DELAY)return 184;
        /* Generic wording does not falsely claim that a slow wheel is stuck. */
        if(p.outcome==PF_OUTCOME_CROSS_THREAD && p.wheel>=0 && p.wheel<4)return 186+p.wheel;
        return 183;
    }
    const int corner[]={63,65,68,70};
    switch(p.outcome) {
    case PF_OUTCOME_SLOW_CORNER:return p.wheel>=0&&p.wheel<4?corner[p.wheel]:61;
    case PF_OUTCOME_WHEELGUN:return 61;
    case PF_OUTCOME_FUEL_RIG:return 56;
    case PF_OUTCOME_STALL_DELAY:return 54;
    case PF_OUTCOME_CROSS_THREAD:return 66;
    default:return -1;
    }
}
static inline int pit_voice_recovered(const pf_plan_t &p,bool player) {
    const int clear[]={62,64,67,69};
    if(player && !p.terminal && p.wheel>=0 && p.wheel<4 &&
       (p.outcome==PF_OUTCOME_SLOW_CORNER||p.outcome==PF_OUTCOME_WHEELGUN||
        p.outcome==PF_OUTCOME_CROSS_THREAD))return clear[p.wheel];
    return -1;
}
static inline int pit_voice_edge(int previous,int current,bool player,bool terminal) {
    if(!player)return -1;
    if((previous==0||previous==1)&&current==2)return 51;
    if(previous==2&&current==3)return 53;
    if(previous==3&&current==4&&!terminal)return 45;
    if(previous==4&&current==0&&!terminal)return 43;
    return -1;
}
