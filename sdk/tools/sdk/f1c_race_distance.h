#ifndef F1C_RACE_DISTANCE_H
#define F1C_RACE_DISTANCE_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
/* Read-only, exact supported executable. G_RaceSetupScaling in f1c_sdk.h
   and RF race_distance_eligible use this confirmed native float (1=100%).
   Reader must copy bytes safely; no writes and no completed-lap inference. */
static int f1c_race_distance_percent(int (*read)(uintptr_t,void*,size_t),int *percent) {
    float scale=0;
    if(!read||!percent||!read((uintptr_t)0x007D0798u,&scale,sizeof(scale))||
       !isfinite(scale)||scale<=0||scale>1.001f)return 0;
    *percent=(int)(scale*100.0f+0.5f); /* native integer percentage, as in RF */
    return 1;
}
static int f1c_race_distance_above_cutoff(int known,int percent,int cutoff) {
    return cutoff==0 || (known && percent>=0 && percent<=100 && percent>cutoff);
}
#endif
