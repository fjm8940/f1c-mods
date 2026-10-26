#ifndef SC_NO_PASS_DISPLAY_H
#define SC_NO_PASS_DISPLAY_H

#include "sc_player_no_overtake.h"

typedef struct {
    const char *primary;
    const char *secondary;
} SCNPDisplay;

/* The policy countdown remains private. The driver first sees a correction
 * prompt only after the initial full three-second grace has elapsed. */
static SCNPDisplay sc_np_display(SCNPAction action,int prompt_active,
                                int warning_active,int investigation,int rules_off) {
    SCNPDisplay result={0,0};
    if(rules_off){
        result.primary="FLAG RULES OFF";
        result.secondary="NO PENALTY";
    }else if(investigation){
        result.primary="UNDER INVESTIGATION";
    }else if(action==SC_NP_WARNING_2||warning_active){
        result.primary="WARNING";
        result.secondary="GIVE BACK POSITION";
    }else if(action==SC_NP_WARNING_1||prompt_active){
        result.primary="GIVE BACK POSITION";
    }
    return result;
}

#endif
