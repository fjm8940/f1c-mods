#ifndef SC_CONFIG_H
#define SC_CONFIG_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
typedef struct {
    double PaceSpeed;
    double TargetGap;
    double GatherPace;
    double GatherSeconds;
    double CautionLaps;
    double WavePolicy;
    double Bunch;
    double AutoCaution;
    double IncludePlayer;
    double Ramp;
    double Telemetry;
    double Park;
    double HideMode;
    double Hush;
    double PitStart;
    double QuickLaps;
    double FullThreshold;
    double FinalLapGate;
    double GraceSeconds;
    double HoldSeconds;
    double InstantReturn;
    double HideInlap;
    double EmergencyDisarmAuto;
    double APPROACH_RANGE;
    double APPROACH_GAIN;
    double BLOCK_RANGE;
    double BLOCK_GAP;
    double FOLLOW_MIN_M;
    double SC_LEAD_MAX_M;
    double SC_RELEASE_WINDOW_M;
    double START_LIGHTS_MOVING_MS;
    double ABREAST_WINDOW;
    double OBSTACLE_MIN;
    double SC_ONTRACK_MS;
    double SC_ARM_DELAY_MS;
    double SC_PICKUP_M;
    double SC_INSTA_DELAY_MS;
    double SC_INLAP_STALL_MS;
    double RAMP_SECONDS;
    double TRIG_STOP_SPEED;
    double TRIG_OFFPACE_FRAC;
    double TRIG_MIN_FIELD;
    double RESTART_TIMEOUT_MS;
    double DeployKey;
    double EmergencyKey;
    double add;
    double SC_WEATHER_DEPLOY;
    double SC_WEATHER_RECOVER;
    double SC_WEATHER_BAD_SECONDS;
    double SC_WEATHER_GOOD_SECONDS;
    double SC_CLEAR_SECONDS;
    double ProgressArmSeconds;
    double ProgressStallSeconds;
    double ProgressTravelSeconds;
    double CatchupBrakeMps2;
    double CatchupReactionSeconds;
    double CatchupGain;
    double ConfirmSeconds;
    double InvertedConfirmSeconds,InvertedMaxSpeed,InvertedUpDotMax;
    double OrderSettleSeconds,OrderQuietSeconds,OrderStoppedSeconds,OrderRecoverSeconds,OrderStoppedSpeed,OrderRecoverSpeed;
    double VehicleRemovalSeconds;
    double RetirementCooldownSeconds,RecallReadySeconds,RecallWarningSeconds,RecallEntryMarginMeters,RecallUseFixedLaps,RecallWatchdogSeconds;
    double RaceDistanceCutoffPercent;
    double MaximumStintLaps;
} SCConfig;
static SCConfig g_sc_config = {56,20,32,90,2,0,0,1,0,1,0,1,1,1,0,1,2,1,10,3,0,0,1,400.0,0.22,30.0,12.0,8.0,45.0,900.0,5.0,7.0,8.0,25.0,5000.0,40.0,2500.0,4000.0,6.0,1.5,0.45,4.0,90000.0,85,73,1,0.96,0.85,10,30,5,600,20,300,8,0.5,0.8,5,3,2,-0.25,15,3,3,3,2,5,60,30,5,15,200,0,300,10,5};
typedef struct {const char *key; size_t offset; double fallback,minimum,maximum;int integer;} SCConfigSpec;
static const SCConfigSpec sc_config_specs[]={
    {"MaximumStintLaps",offsetof(SCConfig,MaximumStintLaps),5,1,20,1},
    {"RaceDistanceCutoffPercent",offsetof(SCConfig,RaceDistanceCutoffPercent),10,0,100,1},
    {"RetirementCooldownSeconds",offsetof(SCConfig,RetirementCooldownSeconds),30,0,300,0},
    {"RecallReadySeconds",offsetof(SCConfig,RecallReadySeconds),5,1,30,0},
    {"RecallWarningSeconds",offsetof(SCConfig,RecallWarningSeconds),15,3,60,0},
    {"RecallEntryMarginMeters",offsetof(SCConfig,RecallEntryMarginMeters),200,100,1000,0},
    {"RecallUseFixedLaps",offsetof(SCConfig,RecallUseFixedLaps),0,0,1,1},
    {"RecallWatchdogSeconds",offsetof(SCConfig,RecallWatchdogSeconds),300,30,1800,0},

    {"VehicleRemovalSeconds",offsetof(SCConfig,VehicleRemovalSeconds),60,1,600,0},
    {"OrderSettleSeconds",offsetof(SCConfig,OrderSettleSeconds),15,3,120,0},
    {"OrderQuietSeconds",offsetof(SCConfig,OrderQuietSeconds),3,1,30,0},
    {"OrderStoppedSeconds",offsetof(SCConfig,OrderStoppedSeconds),3,1,15,0},
    {"OrderRecoverSeconds",offsetof(SCConfig,OrderRecoverSeconds),3,1,15,0},
    {"OrderStoppedSpeed",offsetof(SCConfig,OrderStoppedSpeed),2,0.1,3,0},
    {"OrderRecoverSpeed",offsetof(SCConfig,OrderRecoverSpeed),5,4,15,0},

    {"InvertedConfirmSeconds",offsetof(SCConfig,InvertedConfirmSeconds),3,2,15,0},
    {"InvertedMaxSpeed",offsetof(SCConfig,InvertedMaxSpeed),2,0.5,3,0},
    {"InvertedUpDotMax",offsetof(SCConfig,InvertedUpDotMax),-0.25,-1,-0.1,0},
    {"ConfirmSeconds",offsetof(SCConfig,ConfirmSeconds),5,1,15,0},
    {"CatchupGain",offsetof(SCConfig,CatchupGain),0.8,0.25,1.5,0},
    {"CatchupReactionSeconds",offsetof(SCConfig,CatchupReactionSeconds),0.5,0.25,2,0},
    {"CatchupBrakeMps2",offsetof(SCConfig,CatchupBrakeMps2),8,2,12,0},
    {"ProgressTravelSeconds",offsetof(SCConfig,ProgressTravelSeconds),300,30,1200,0},
    {"ProgressStallSeconds",offsetof(SCConfig,ProgressStallSeconds),20,5,120,0},
    {"ProgressArmSeconds",offsetof(SCConfig,ProgressArmSeconds),600,30,1200,0},
    {"SC_CLEAR_SECONDS",offsetof(SCConfig,SC_CLEAR_SECONDS),5,1,60,0},
    {"SC_WEATHER_GOOD_SECONDS",offsetof(SCConfig,SC_WEATHER_GOOD_SECONDS),30,1,300,0},
    {"SC_WEATHER_BAD_SECONDS",offsetof(SCConfig,SC_WEATHER_BAD_SECONDS),10,1,120,0},
    {"SC_WEATHER_RECOVER",offsetof(SCConfig,SC_WEATHER_RECOVER),0.85,0.1,0.99,0},
    {"SC_WEATHER_DEPLOY",offsetof(SCConfig,SC_WEATHER_DEPLOY),0.96,0.5,1,0},
    {"PaceSpeed",offsetof(SCConfig,PaceSpeed),56,5,80,1},
    {"TargetGap",offsetof(SCConfig,TargetGap),20,5,100,1},
    {"GatherPace",offsetof(SCConfig,GatherPace),32,0,80,1},
    {"GatherSeconds",offsetof(SCConfig,GatherSeconds),90,5,600,1},
    {"CautionLaps",offsetof(SCConfig,CautionLaps),2,1,20,1},
    {"WavePolicy",offsetof(SCConfig,WavePolicy),0,0,0,1},
    {"Bunch",offsetof(SCConfig,Bunch),0,0,1,1},
    {"AutoCaution",offsetof(SCConfig,AutoCaution),1,0,1,1},
    {"IncludePlayer",offsetof(SCConfig,IncludePlayer),0,0,1,1},
    {"Ramp",offsetof(SCConfig,Ramp),1,0,1,1},
    {"Telemetry",offsetof(SCConfig,Telemetry),0,0,1,1},
    {"Park",offsetof(SCConfig,Park),1,1,1,1},
    {"HideMode",offsetof(SCConfig,HideMode),1,1,1,1},
    {"Hush",offsetof(SCConfig,Hush),1,0,1,1},
    {"PitStart",offsetof(SCConfig,PitStart),0,0,1,1},
    {"QuickLaps",offsetof(SCConfig,QuickLaps),1,1,20,1},
    {"FullThreshold",offsetof(SCConfig,FullThreshold),2,1,20,1},
    {"FinalLapGate",offsetof(SCConfig,FinalLapGate),1,0,1,1},
    {"GraceSeconds",offsetof(SCConfig,GraceSeconds),10,0,120,1},
    {"HoldSeconds",offsetof(SCConfig,HoldSeconds),3,1,60,1},
    {"InstantReturn",offsetof(SCConfig,InstantReturn),0,0,1,1},
    {"HideInlap",offsetof(SCConfig,HideInlap),0,0,1,1},
    {"EmergencyDisarmAuto",offsetof(SCConfig,EmergencyDisarmAuto),1,0,1,1},
    {"APPROACH_RANGE",offsetof(SCConfig,APPROACH_RANGE),400.0,50,1000,0},
    {"APPROACH_GAIN",offsetof(SCConfig,APPROACH_GAIN),0.22,0.01,1,0},
    {"BLOCK_RANGE",offsetof(SCConfig,BLOCK_RANGE),30.0,10,100,0},
    {"BLOCK_GAP",offsetof(SCConfig,BLOCK_GAP),12.0,5,50,0},
    {"FOLLOW_MIN_M",offsetof(SCConfig,FOLLOW_MIN_M),8.0,5,30,0},
    {"SC_LEAD_MAX_M",offsetof(SCConfig,SC_LEAD_MAX_M),45.0,10,150,0},
    {"SC_RELEASE_WINDOW_M",offsetof(SCConfig,SC_RELEASE_WINDOW_M),900.0,100,3000,0},
    {"START_LIGHTS_MOVING_MS",offsetof(SCConfig,START_LIGHTS_MOVING_MS),5.0,1,15,0},
    {"ABREAST_WINDOW",offsetof(SCConfig,ABREAST_WINDOW),7.0,3,20,0},
    {"OBSTACLE_MIN",offsetof(SCConfig,OBSTACLE_MIN),8.0,5,30,0},
    {"SC_ONTRACK_MS",offsetof(SCConfig,SC_ONTRACK_MS),25.0,10,50,0},
    {"SC_ARM_DELAY_MS",offsetof(SCConfig,SC_ARM_DELAY_MS),5000.0,1000,30000,1},
    {"SC_PICKUP_M",offsetof(SCConfig,SC_PICKUP_M),40.0,10,100,0},
    {"SC_INSTA_DELAY_MS",offsetof(SCConfig,SC_INSTA_DELAY_MS),2500.0,500,10000,1},
    {"SC_INLAP_STALL_MS",offsetof(SCConfig,SC_INLAP_STALL_MS),4000.0,1000,30000,1},
    {"RAMP_SECONDS",offsetof(SCConfig,RAMP_SECONDS),6.0,1,30,0},
    {"TRIG_STOP_SPEED",offsetof(SCConfig,TRIG_STOP_SPEED),1.5,0.1,5,0},
    {"TRIG_OFFPACE_FRAC",offsetof(SCConfig,TRIG_OFFPACE_FRAC),0.45,0.1,0.9,0},
    {"TRIG_MIN_FIELD",offsetof(SCConfig,TRIG_MIN_FIELD),4.0,1,30,1},
    {"RESTART_TIMEOUT_MS",offsetof(SCConfig,RESTART_TIMEOUT_MS),90000.0,10000,300000,1},
    {"DeployKey",offsetof(SCConfig,DeployKey),85,8,255,1},
    {"EmergencyKey",offsetof(SCConfig,EmergencyKey),73,8,255,1},
    {"add",offsetof(SCConfig,add),1,0,1,1},
};
/* Strict, finite parse with inline comments; rejected values retain defaults. */
static int sc_config_number(char *raw,const SCConfigSpec *spec,double *out) {
    char *comment=strpbrk(raw,";#"),*end;double value;
    if(comment)*comment=0;
    if(!strcmp(spec->key,"DeployKey") || !strcmp(spec->key,"EmergencyKey")){
        char *q=raw;while(*q==' '||*q=='\t')q++;
        size_t n=strlen(q);while(n && (q[n-1]==' '||q[n-1]=='\t'))q[--n]=0;
        if(n==1 && isalpha((unsigned char)*q)){*out=toupper((unsigned char)*q);return 1;}
    }
    errno=0;value=strtod(raw,&end);
    if(end==raw || errno)return 0;
    while(*end==' ' || *end=='\t')end++;
    if(*end || !isfinite(value) || value<spec->minimum || value>spec->maximum ||
       (spec->integer && floor(value)!=value))return 0;
    *out=value;return 1;
}
static void sc_config_load(const char *path) {
    SCConfig next=g_sc_config;
    for(unsigned i=0;i<sizeof(sc_config_specs)/sizeof(sc_config_specs[0]);i++) {
        const SCConfigSpec *spec=&sc_config_specs[i];char raw[128];double value=spec->fallback;
        DWORD n=GetPrivateProfileStringA("sc",spec->key,"",raw,sizeof(raw),path);
        if(n && (n>=sizeof(raw)-1 || !sc_config_number(raw,spec,&value)))value=spec->fallback;
        *(double*)((char*)&next+spec->offset)=value;
    }
    /* Reserved RF keys and equal bindings cannot silently disable emergency. */
    if(next.DeployKey==next.EmergencyKey || next.DeployKey==84 || next.EmergencyKey==84 || next.DeployKey==89 || next.EmergencyKey==89 ||
       next.DeployKey<48 || next.EmergencyKey<48 ||
       (next.DeployKey>=97 && next.DeployKey<=99) ||
       (next.EmergencyKey>=97 && next.EmergencyKey<=99)) {
        next.DeployKey=85;next.EmergencyKey=73;
    }
    if(next.BLOCK_GAP>next.BLOCK_RANGE)next.BLOCK_GAP=next.BLOCK_RANGE;
    if(next.SC_PICKUP_M>next.SC_LEAD_MAX_M)next.SC_PICKUP_M=next.SC_LEAD_MAX_M;
    if(next.SC_WEATHER_RECOVER>=next.SC_WEATHER_DEPLOY){next.SC_WEATHER_RECOVER=.85;next.SC_WEATHER_DEPLOY=.96;}
    g_sc_config=next;
}
#endif
