#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/* The broker's spectated car is the best reviewed listener position available
 * to this client. Fade an emitter out with distance rather than sending the
 * same full-volume PCM to every camera on the circuit. */
static double pit_wheel_distance_m(float sx,float sy,float sz,
                                   float lx,float ly,float lz) {
    if(!std::isfinite(sx)||!std::isfinite(sy)||!std::isfinite(sz)||
       !std::isfinite(lx)||!std::isfinite(ly)||!std::isfinite(lz))return -1;
    double dx=(double)sx-lx,dy=(double)sy-ly,dz=(double)sz-lz;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}

/* Native model (F_3DSoundPositioningUpdate 0x405B90, attenuation 0x406020,
 * pan 0x406060): the game mixes 2D Miles samples in software, sending
 * level=(1-pan)*att and pan*att with pan=0.5 when centred, so a centred
 * native effect reaches each channel at half its attenuated volume. Its
 * attenuation is an inverse-distance law beyond a full-volume radius. The
 * wheel-gun WAVs are mastered like native effects (0 dBFS peak), so they
 * need the same chain instead of raw full-scale waveOut playback. */
static const double PIT_WHEEL_FULL_RADIUS_M=6.0;  /* crew-to-car distance */
static const double PIT_WHEEL_FADE_START_M=35.0;  /* begin fade to silence */
static const double PIT_WHEEL_SILENT_M=60.0;      /* hard cut, no far bursts */
static const float  PIT_WHEEL_CENTRE_PAN=0.5f;    /* native centred level */
/* A listener at racing speed on the circuit is behind the pit wall and
 * masked by its own engine; the native mix has the same effect implicitly. */
static const float  PIT_WHEEL_TRACK_LISTENER=0.2f; /* about -14 dB */

static float pit_wheel_gain(double distance_m) {
    if(!std::isfinite(distance_m)||distance_m<0||distance_m>=PIT_WHEEL_SILENT_M)return 0;
    if(distance_m<=PIT_WHEEL_FULL_RADIUS_M)return 1;
    double gain=PIT_WHEEL_FULL_RADIUS_M/distance_m;
    if(distance_m>PIT_WHEEL_FADE_START_M) {
        double t=(PIT_WHEEL_SILENT_M-distance_m)/(PIT_WHEEL_SILENT_M-PIT_WHEEL_FADE_START_M);
        gain*=t*t*(3-2*t); /* smooth edge, no audible step at the cut */
    }
    return (float)gain;
}

/* The player's own sound sliders, read from the game's player profile. The
 * game defaults (1.0) are deliberately not used when the profile is missing:
 * an unknown mix falls back to a moderate level instead of full scale. */
struct PitSoundMix {
    float sfx_volume=0.5f;      /* "SoundFX Volume" */
    float player_ratio=1.0f;    /* "Player Volume Ratio" */
    float other_ratio=0.5f;     /* "Other Volume Ratio" */
    bool from_profile=false;
};

static bool pit_mix_value(const std::string &text,const char *key,float maximum,float *out) {
    std::string needle=std::string("\n")+key+"=\"";
    size_t at=text.find(needle);
    if(at==std::string::npos)return false;
    const char *begin=text.c_str()+at+needle.size();
    char *end=nullptr;
    double value=std::strtod(begin,&end);
    if(end==begin||*end!='"'||!std::isfinite(value)||value<0||value>maximum)return false;
    *out=(float)value;
    return true;
}

/* Parse the [ Sound Options ] keys of a .PLR text. Each key is independent:
 * a missing or malformed key keeps its conservative default. */
static PitSoundMix pit_sound_mix_parse(const std::string &plr) {
    PitSoundMix mix;
    std::string text="\n"+plr;
    bool any=false;
    any|=pit_mix_value(text,"SoundFX Volume",1.0f,&mix.sfx_volume);
    any|=pit_mix_value(text,"Player Volume Ratio",4.0f,&mix.player_ratio);
    any|=pit_mix_value(text,"Other Volume Ratio",4.0f,&mix.other_ratio);
    mix.from_profile=any;
    return mix;
}

/* Level for one burst: slider chain, native centred pan, distance and the
 * on-track masking. The result is clamped to full scale. */
static float pit_wheel_level(const PitSoundMix &mix,bool source_is_human,
                             double distance_m,bool listener_on_track) {
    float distance_gain=pit_wheel_gain(distance_m);
    if(distance_gain<=0)return 0;
    double level=(double)mix.sfx_volume*
                 (source_is_human?mix.player_ratio:mix.other_ratio)*
                 PIT_WHEEL_CENTRE_PAN*distance_gain;
    if(listener_on_track)level*=PIT_WHEEL_TRACK_LISTENER;
    if(!std::isfinite(level)||level<=0)return 0;
    return level>1?1.0f:(float)level;
}

/* Scale the loaded 16-bit PCM into a private waveOut buffer. The device's
 * volume is never changed, so game audio and concurrent bursts are isolated. */
static bool pit_wheel_scale_pcm16(const unsigned char *source,
                                  unsigned char *destination,size_t bytes,
                                  float gain) {
    if(!source||!destination||(bytes&1u)||!std::isfinite(gain)||
       gain<0||gain>1)return false;
    for(size_t at=0;at<bytes;at+=2) {
        int16_t sample=0;
        std::memcpy(&sample,source+at,sizeof sample);
        int16_t scaled=(int16_t)((float)sample*gain);
        std::memcpy(destination+at,&scaled,sizeof scaled);
    }
    return true;
}
