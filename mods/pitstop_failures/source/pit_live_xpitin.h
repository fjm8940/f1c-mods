#pragma once
#include <cmath>
#include "f1c_pit_marker.h"

struct PitXpitinReading {
    bool player_valid=false,marker_valid=false,within_100m=false,within_voice_radius=false;
    float player[3]{},marker[3]{};
    double distance_m=0,horizontal_m=0,height_delta_m=0;
};

/* The measured marker radius requests the pit-entry voice before the game's
 * entry latch; that latch remains the fallback when the marker is unavailable. */
static PitXpitinReading pit_live_xpitin_read(uintptr_t steward,F1CPitReadFn read,
                                              const float player[3]) {
    PitXpitinReading result{};
    if(!player)return result;
    for(int i=0;i<3;i++)
        if(!std::isfinite(player[i]) || std::fabs(player[i])>1000000.0f)return result;
    result.player_valid=true;
    for(int i=0;i<3;i++)result.player[i]=player[i];
    if(!f1c_pit_entry_marker(steward,read,result.marker))return result;
    result.marker_valid=true;
    double dx=(double)player[0]-result.marker[0];
    double dy=(double)player[1]-result.marker[1];
    double dz=(double)player[2]-result.marker[2];
    result.horizontal_m=std::sqrt(dx*dx+dz*dz);
    result.distance_m=std::sqrt(dx*dx+dy*dy+dz*dz);
    result.height_delta_m=dy;
    result.within_100m=f1c_pit_within_100(player,result.marker)!=0;
    /* The marker is the centre of the game's entry volume. Announce on the
     * approach, before the car must actually trip that volume's latch. */
    result.within_voice_radius=std::fabs(dy)<=20.0 && result.horizontal_m<=150.0;
    return result;
}
