#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
/* Exact supported F1C executable only; reviewed Red Flag v76 read-only
 * on/off racing-path wetness globals. Not rain intensity or visibility.
 * Caller must establish target identity and a stable game-thread session. */
typedef int (*F1CWeatherRead)(uintptr_t,void*,size_t);
typedef struct { int valid; float on_path,off_path; } F1CWeather;
static F1CWeather f1c_weather_read(F1CWeatherRead read_at) {
    F1CWeather w={0,0,0}; float values[2];
    if(!read_at || !read_at(0x008AFEE8u,values,sizeof(values)))return w;
    if(!isfinite(values[0]) || !isfinite(values[1]) ||
       values[0]<0 || values[0]>1 || values[1]<0 || values[1]>1)return w;
    w.valid=1;w.on_path=values[0];w.off_path=values[1];return w;
}
