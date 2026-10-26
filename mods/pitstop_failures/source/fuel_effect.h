#pragma once
#include <cmath>
#include <algorithm>
#include "f1c_pit_struct.h"

/* Per-vehicle fuel service effect. The game computes fuel from fuel_start,
 * fuel_start_time and its native 12 L/s rate. These are the reviewed fields
 * at vehicle+0x347C/0x3480/0x3484; no process-global rate is changed. */
struct FuelEffect {
    int variant=0;                 /* 1 slow, 2 no flow, 3 interrupted, 4 spill */
    bool captured=false, cut=false, recovered=false, spill_notified=false;
    double captured_at=0, cut_at=0, cleanup_until=0;
    float entry_fuel=0, cut_fuel=0, original_target=0, spill_target=0;
};
struct FuelEffectResult { bool hold=false, spill_now=false, recovered_now=false, invalid=false; };

static inline FuelEffectResult fuel_effect_step(FuelEffect &s,F1CPitMachine &pit,
                                               float fuel,double now,double delay) {
    FuelEffectResult out{};
    if(s.variant<1||s.variant>4)return out;
    if(!std::isfinite(now)||!std::isfinite(delay)||delay<0||
       !std::isfinite(fuel)||!std::isfinite(pit.fuel_start)||
       !std::isfinite(pit.fuel_start_time)||!std::isfinite(pit.fuel_target)||
       pit.fuel_target<0||fuel<0) {out.invalid=true;return out;}
    if(!s.captured) {
        if(pit.fuel_start_time<=0 || now+0.1<pit.fuel_start_time) {out.hold=true;return out;}
        if(pit.fuel_target<=fuel+0.5f)return out;
        s.captured=true;s.captured_at=now;s.entry_fuel=fuel;
        s.original_target=pit.fuel_target;
        if(s.variant==4) {
            float loss=0.30f*(s.original_target-pit.fuel_start);
            s.spill_target=std::max(fuel+0.5f,s.original_target-loss);
        }
    }
    if(now-s.captured_at>90.0) {out.invalid=true;return out;}
    const float target=s.variant==4?s.spill_target:s.original_target;
    if(s.variant==4) {
        pit.fuel_target=target;
        if(fuel>=target-0.15f&&!s.spill_notified) {
            s.spill_notified=true;s.cleanup_until=now+3.0;out.spill_now=true;
        }
        out.hold=!s.spill_notified||now<s.cleanup_until;
        return out;
    }
    if(s.variant==1 && !s.recovered) {
        if(fuel>=target-0.15f) {
            pit.fuel_start=fuel;pit.fuel_start_time=(float)now;
            s.recovered=true;out.recovered_now=true;
        } else {
            const float desired=std::min(target,s.entry_fuel+4.0f*(float)(now-s.captured_at));
            pit.fuel_start_time=(float)(now-(desired-pit.fuel_start)/12.0f);
        }
    } else if(s.variant==2 && !s.recovered) {
        if(now-s.captured_at<delay) {
            pit.fuel_start_time=(float)(now-(s.entry_fuel-pit.fuel_start)/12.0f);
        } else {
            pit.fuel_start=fuel;pit.fuel_start_time=(float)now;
            s.recovered=true;out.recovered_now=true;
        }
    } else if(s.variant==3 && !s.recovered) {
        if(!s.cut && fuel>=s.entry_fuel+3.0f) {
            s.cut=true;s.cut_at=now;s.cut_fuel=fuel;
        }
        if(s.cut) {
            if(now-s.cut_at<delay) {
                pit.fuel_start_time=(float)(now-(s.cut_fuel-pit.fuel_start)/12.0f);
            } else {
                pit.fuel_start=fuel;pit.fuel_start_time=(float)now;
                s.recovered=true;out.recovered_now=true;
            }
        }
    }
    out.hold=fuel<target-0.15f || !s.recovered;
    return out;
}
