#ifndef SC_STATUS_DISPLAY_H
#define SC_STATUS_DISPLAY_H

#include <stdint.h>

typedef enum {
    SC_HUD_OFF=0,
    SC_HUD_REASON,
    SC_HUD_STANDING_BY,
    SC_HUD_DEPLOYING,
    SC_HUD_ON_TRACK,
    SC_HUD_IN_THIS_LAP,
    SC_HUD_IN_PITS,
    SC_HUD_GREEN
} SCHudPhase;

typedef struct {
    const char *primary;
    const char *secondary;
    uint32_t color_argb;
} SCHudStatus;

/* The pre-deployment display uses race time, so pausing cannot silently
 * advance from the incident explanation to STANDING BY. */
static int sc_hud_reason_window(double now,double armed_at) {
    if(!(now>=armed_at))return 1;
    return ((int)((now-armed_at)/3.0)&1)==0;
}

/* Native IDToColorMaskMapper(2) uses opaque yellow 0xFFFFFF00. The green
 * explicitly requested by the owner is opaque RGB #00BC1C. */
static SCHudStatus sc_hud_status(SCHudPhase phase) {
    SCHudStatus result={0,0,0xFFFFFFFFu};
    switch(phase){
    case SC_HUD_REASON:     result.primary="SC";break;
    case SC_HUD_STANDING_BY: result.primary="SC";result.secondary="STANDING BY";break;
    case SC_HUD_DEPLOYING:  result.primary="SC";result.secondary="DEPLOYING";break;
    case SC_HUD_ON_TRACK:  result.primary="SC";break;
    case SC_HUD_IN_THIS_LAP:result.primary="SC";result.secondary="IN THIS LAP";break;
    case SC_HUD_IN_PITS:   result.primary="SC";result.secondary="IN PITS";break;
    case SC_HUD_GREEN:     result.primary="GREEN";result.color_argb=0xFF00BC1Cu;break;
    case SC_HUD_OFF:       break;
    }
    if(result.primary&&phase!=SC_HUD_GREEN)result.color_argb=0xFFFFFF00u;
    return result;
}

#endif
