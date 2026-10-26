#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "f1c_sdk.h"
/* Read-only buffer decoder. Caller proves exact target and copies VEH_STRIDE
 * bytes on a broker game-thread callback with a current vehicle handle.
 * No new hooks, calls or writes. Existing SDK field evidence is retained.
 * Supplemental offsets: Java F1CUI a7772c7f... reader's 0x4714 anchor minus
 * 1736/1732/1728/1652/1648/13408/1552/1740. OBSERVED community semantics,
 * not newly native-validated. See RACE_DATA_OBSERVATION_REVIEW.md.
 */
typedef struct F1CRaceDataExtra {
    float fuel, efficiency, total_crossing_time, qualifying_time;
    float last_lap, last_i1, last_i2, best_i1, best_i2, track_fraction;
    float engine_max_rpm, pit_started, pit_ended;
    float qualifying_ability_norm, race_ability_norm, rain_ability_scaled;
    int32_t tire_choice, hits, track_limits, penalties, planned_pit_lap;
    char veh_file[128];
} F1CRaceDataExtra;
static inline int f1c_race_data_decode(const void *buffer, size_t length,
                                      F1CRaceDataExtra *out) {
    const unsigned char *b=(const unsigned char*)buffer;
    if(!b||!out||length<VEH_STRIDE)return 0;
    memset(out,0,sizeof(*out));
#define RDC_READ(field,offset) memcpy(&out->field,b+(offset),sizeof(out->field))
    RDC_READ(fuel,F1C_OFF_FuelLevel);
    RDC_READ(efficiency,F1C_OFF_theoricNumberOfLapsWithOneLitre);
    RDC_READ(total_crossing_time,F1C_OFF_totalTimeOnFinishLine);
    RDC_READ(qualifying_time,F1C_OFF_timeOfQualySession);
    RDC_READ(last_lap,0x4714u-1736u);
    RDC_READ(last_i1,0x4714u-1732u);
    RDC_READ(last_i2,0x4714u-1728u);
    RDC_READ(best_i1,0x4714u-1652u);
    RDC_READ(best_i2,0x4714u-1648u);
    RDC_READ(track_fraction,0x4714u-1740u);
    RDC_READ(hits,0x4714u-13408u);
    RDC_READ(track_limits,0x4714u-1552u);
    RDC_READ(tire_choice,F1C_OFF_tireChoice);
    RDC_READ(penalties,F1C_OFF_noOfPenaltiesToProcess);
    RDC_READ(planned_pit_lap,F1C_OFF_PitStopSchedule0);
    RDC_READ(engine_max_rpm,F1C_OFF_EngineMaxRPM);
    RDC_READ(pit_started,F1C_OFF_PitStopStartTime);
    RDC_READ(pit_ended,F1C_OFF_PitStopEndTime);
    RDC_READ(qualifying_ability_norm,F1C_OFF_QualifyingAbilityNorm);
    RDC_READ(race_ability_norm,F1C_OFF_RaceAbilityNorm);
    RDC_READ(rain_ability_scaled,F1C_OFF_RainAbilityScaled);
    memcpy(out->veh_file,b+F1C_OFF_VehiclePath,sizeof(out->veh_file));
    out->veh_file[sizeof(out->veh_file)-1]=0;
#undef RDC_READ
    return 1;
}
