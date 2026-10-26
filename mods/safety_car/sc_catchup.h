#ifndef SC_CATCHUP_H
#define SC_CATCHUP_H
#include "sc_config.h"
#include <math.h>
/* Room excludes the desired following space. Preserve compact-queue behavior;
   increase headroom progressively only with positive clear space. Native AI
   still owns throttle, cornering and braking. This is not a collision proof. */
static float sc_catchup_limit(float front,float room,float near_gain) {
    if(!isfinite(front)||front<0||!isfinite(room))return 0;
    if(room<=0.0f)return front+near_gain*room;
    float mix=fminf(1.0f,fmaxf(0.0f,(room-20.0f)/100.0f));
    float gain=near_gain+mix*fmaxf(0.0f,(float)g_sc_config.CatchupGain-near_gain);
    float linear=front+gain*room;
    float a=(float)g_sc_config.CatchupBrakeMps2;
    float delay=a*(float)g_sc_config.CatchupReactionSeconds;
    /* room = (v^2-front^2)/(2*a) + (v-front)*reaction */
    float braking=sqrtf((front+delay)*(front+delay)+2.0f*a*room)-delay;
    return fminf(linear,braking);
}
#endif
