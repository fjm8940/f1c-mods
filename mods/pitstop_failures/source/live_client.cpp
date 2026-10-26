#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include "f1c_runtime.h"
#include "f1c_pit.h"
#include "f1c_pit_crew.h"
extern "C" {
#include "f1c_broker.h"
#include "f1c_broker_session_end.h"
#include "pit_usage.h"
#include "pit_test_bridge.h"
}
#include "pit_live_timing.h"
#include "pit_live_presentation.h"
#include "policy_config.h"
#include "policy_config_v3.h"
#include "pit_failure_policy_v3.h"
#include "pit_player_settings.h"
#include "pit_policy_precision.h"
#include "f1c_pit_voice.h"
#include "pit_live_voice.h"
#include "pit_live_xpitin.h"
#include "f1c_pit_hud.h"
#include "coop_mf_bridge.h"

#include "legacy_feedback.h"
#include "fuel_effect.h"
#ifndef PIT_AI_MECHANICS_TEST
#include "f1c_ai_pit_failure.h"
#endif

static HMODULE module;
static F1CBrokerApi broker;
static F1CBrokerToken client;
static F1CBrokerSetSessionEndFn session_end_bind;
static bool session_end_registered;
static HANDLE stop_event, owner_mutex;
static CRITICAL_SECTION lock;
static volatile LONG pending;
static SOCKET listener=INVALID_SOCKET;
/* Model 3 is the two-stage v3 adapter. Rebuild with -DPIT_LIVE_POLICY_MODEL=2
 * to fall back to the accepted v2 runtime; both code paths stay compiled. */
#ifndef PIT_LIVE_POLICY_MODEL
#define PIT_LIVE_POLICY_MODEL 3
#endif
#ifndef PIT_LIVE_ENABLE_CUSTOM_WAV
/* Guarded owner-requested WAV trial. The 2026-09-22 3D-sound crash was not
 * attributed; this worker-owned waveOut path still requires live validation. */
#define PIT_LIVE_ENABLE_CUSTOM_WAV 1
#endif
#ifndef PIT_LIVE_PUBLIC_BUILD
#define PIT_LIVE_PUBLIC_BUILD 0
#endif
static const int policy_model = PIT_LIVE_POLICY_MODEL;
static pf_config_t config;
static pf_session_t *policy;
static pf3_config_t config3;
static pf3_session_t *policy3;
static const char *settings_source="embedded_defaults";
static uint64_t seed=0;
static unsigned epoch=0;
static double now=0, last_time=-1;
static DWORD observed=0;
static bool active=false, invalid_clock=false;
static int race_phase=0;
static int audio_listener_slot=-1;
/* Box-call self-check: 1 = trusted, 0 = disabled for the session after the
 * car was serviced more than 6 m from the box position it was given. */
static int box_call_enabled=1;
static int native_read(uintptr_t address,void *out,size_t n);
static void event(const std::string &s);

/* ---- CO-OP sync (2026-10-10, owner: "pitstop failures synced") ------------
 * coop_mode creates Local\COOP_PIT_BRIDGE_V1 (same layout as the Mechanical
 * Failures bridge) and carries its events on the CO-OP UDP link. The PC that
 * drives a car rolls its stop (host: bots + own car; client: own car) and
 * sends the plan; the other PCs commit exactly that plan for their copy of the
 * car (controller role 2) and never roll their own. No bridge / inactive =
 * single-player behaviour, unchanged. */
#define COOP_PIT_BRIDGE_NAME "Local\\COOP_PIT_BRIDGE_V1"
#define COOP_PIT_EV_PLAN 1
static CoopMfBridge *coop_pit=nullptr;
static DWORD coop_pit_next_open=0;
static uint32_t coop_pit_seq=0;
struct CoopPitPlan { pf3_plan_t plan; DWORD received; };
static std::map<std::string,CoopPitPlan> coop_pit_plans;
static bool coop_pit_active() {
    if(!coop_pit && (LONG)(GetTickCount()-coop_pit_next_open)>=0) {
        coop_pit_next_open=GetTickCount()+3000;
        HANDLE map=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,COOP_PIT_BRIDGE_NAME);
        if(map) {
            CoopMfBridge *b=(CoopMfBridge*)MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(CoopMfBridge));
            if(b && b->magic==COOP_MF_MAGIC && b->version==COOP_MF_VERSION) {coop_pit=b;InterlockedExchange(&b->mf_attached,1);}
            else {if(b)UnmapViewOfFile(b);CloseHandle(map);}
        }
    }
    return coop_pit && coop_pit->active;
}
static void coop_pit_send(const char *driver,const pf3_plan_t &p) {
    CoopMfEvent e{};
    e.type=COOP_PIT_EV_PLAN|COOP_MF_RELIABLE;
    e.id=(uint32_t)(GetCurrentProcessId()&0xFFF)<<20|(++coop_pit_seq&0xFFFFF);
    lstrcpynA(e.name,driver,sizeof e.name);
    e.i[0]=(p.parent+1)|((p.resolution+1)<<8)|((p.wheel+1)<<16)|((p.fire_selected?1:0)<<24);
    e.f[0]=(float)(p.terminal?p.terminal_presentation_seconds:p.extra_job_seconds);
    e.f[1]=(float)p.fire_burn_seconds;
    e.f[2]=(float)p.fire_inspection_seconds;
    coop_mf_push_out(coop_pit,&e);
}
static void coop_pit_receive(const pf_identity_t &any_id) {
    CoopMfEvent e;
    static uint32_t seen[64]; static unsigned seen_n=0;
    while(coop_pit && coop_mf_pop(coop_pit->in,&coop_pit->in_r,&coop_pit->in_w,&e)) {
        if((e.type&0x7FFF)!=COOP_PIT_EV_PLAN)continue;
        bool dup=false;for(uint32_t v:seen)if(v==e.id){dup=true;break;}
        if(dup)continue;
        seen[seen_n++&63]=e.id;
        int parent=(e.i[0]&0xFF)-1,resolution=((e.i[0]>>8)&0xFF)-1,wheel=((e.i[0]>>16)&0xFF)-1,fire=(e.i[0]>>24)&1;
        pf3_plan_t p{};
        if(parent<0) pf3_plan_build_full(nullptr,&any_id,nullptr,0,nullptr,&p);   /* clean */
        else {
            pf3_forcing_t f=pf3_forcing_natural();f.parent=parent;f.resolution=resolution;f.fire=fire;f.fire_supported=fire;
            pf_context_t ctx{};ctx.eligibility.tyre_work=1;ctx.eligibility.fuel_allowed=1;ctx.eligibility.fuel_add_liters=1;ctx.eligibility.real_service_possible=1;ctx.eligibility.release_possible=1;
            pit_policy_math([&]{pf3_plan_build_full(&config3,&any_id,&ctx,seed,&f,&p);return 0;});
            p.wheel=wheel;
            if(p.terminal)p.terminal_presentation_seconds=e.f[0];else p.extra_job_seconds=e.f[0];
            p.fire_burn_seconds=e.f[1];p.fire_inspection_seconds=e.f[2];
            p.fire_extra_seconds=p.fire_selected?p.fire_burn_seconds+p.fire_inspection_seconds:0;
        }
        e.name[sizeof e.name-1]=0;
        coop_pit_plans[e.name]=CoopPitPlan{p,GetTickCount()};
        event(std::string(e.name)+": CO-OP plan received "+(parent<0?std::string("clean"):std::string(pf3_parent_name(parent))+"/"+pf3_resolution_name(parent,resolution)));
    }
}
/* AIW waypoints within 150 m of XPITIN, cached per marker position. */
static std::vector<std::array<float,3>> pit_road_main,pit_road_pit;
static float pit_road_centre[3]={1.0e9f,0,0};
static bool pit_road_distances(const float marker[3],const float player[3],double *dmain,double *dpit) {
    float dx=marker[0]-pit_road_centre[0],dz=marker[2]-pit_road_centre[2];
    if(dx*dx+dz*dz>25.0f) {
        static float xyz[4096][3];static int branch[4096];
        int n=f1c_aiw_waypoints_near(native_read,marker,150.0f,xyz,branch,4096);
        pit_road_main.clear();pit_road_pit.clear();
        for(int i=0;i<n;i++)
            (branch[i]==1?pit_road_pit:pit_road_main).push_back({xyz[i][0],xyz[i][1],xyz[i][2]});
        for(int k=0;k<3;k++)pit_road_centre[k]=marker[k];
        event("Pit road geometry: "+std::to_string(pit_road_main.size())+" main and "+
              std::to_string(pit_road_pit.size())+" pit-road waypoints near XPITIN");
    }
    if(pit_road_main.empty() || pit_road_pit.empty())return false;
    auto nearest=[&](const std::vector<std::array<float,3>> &set) {
        double best=1.0e18;
        for(const auto &q:set){double x=q[0]-player[0],z=q[2]-player[2];best=std::min(best,x*x+z*z);}
        return std::sqrt(best);
    };
    *dmain=nearest(pit_road_main);*dpit=nearest(pit_road_pit);
    return true;
}
static std::string instance, error;
static volatile LONG player_retire_pending=0;
static uint32_t player_retire_generation=0;
static uint64_t broker_event_cursor=0;
struct Request { std::string id, state, message, failure="random", resolution="natural"; int selector=1; int parent=-1, res_index=-1; unsigned generation=0, object=0, slot=0; DWORD expires=0; double submitted=0; };
struct DeferredFeedback { pf_command_t command{}; double at=0; };
struct Car {
    F1CBrokerVehicleSnapshot snapshot{};
    bool seen=false, ready=false, holding=false, terminal=false, safety_car_logged=false;
    float probe_win[F1C_PIT_CREW_MAX_ACTORS][4][2]{}; double probe_until=0, probe_stop=-1, probe_end=-1;
    double release_shift=0, hold_end=0, work_done_at=0;
    float last_x=0,last_z=0; bool pos_valid=false; double teleport_until=0;
    int last_pit_status=0; bool serviced_this_visit=false; int hold_corner=-1, hold_contact=0; double hold_from=0, hold_until=0, hold_last=0, hold_delay=0;
    bool stalled=false, player_ignition_owned=false;
    bool stall_notice_pending=false, stall_notice_announced=false;
    double stall_notice_retry_at=0;
    bool player_restart_pending=false, player_restart_confirmed=false;
    bool player_restart_feedback_pending=false, player_restart_announced=false;
    bool player_retire_voice_queued=false;
    unsigned player_restart_attempts=0;
    double player_restart_started_at=0;
    unsigned char player_ignition_restore=1;
    bool retirement_requested=false, garaged=false, mechanic_started=false;
    double terminal_at=0, reveal_at=0;
    float rpm=0;
    uint64_t token=0;
    /* Monotonic per-identity service sequence. Distinct from the active token:
     * cancelling retires the active identity but never rewinds the sequence, so
     * a later stop for the same epoch/object generation cannot reuse a
     * cancelled tombstone. Reset only on a new epoch or object generation. */
    uint64_t sequence=0;
    double baseline=0,target=0,applied=0,start=0;
    pf_plan_t plan{};
    pf3_plan_t plan3{};
    FuelEffect fuel{};
    bool fuel_recovery_owned=false, fuel_recovery_announced=false;
    float fuel_requested_target=0;
    bool fuel_clear_announced=false, pit_entry_latch=false, pit_entry_announced=false;
    bool pit_entry_triggered=false;
    double xpitin_closest_m=1.0e9;   /* closest horizontal pass of the XPITIN marker */
    bool pit_road_logged=false;
    bool pit_entry_pending=false;
    double pit_entry_retry_at=0,pit_entry_until=0;
    PitXpitinReading xpitin{};
    bool pit_entry_call_active=false;
    double pit_entry_ack_at=-1,pit_entry_ack_distance_m=-1;
    std::string pit_entry_ack_source="none";
    double pit_entry_voice_attempt_at=-1,pit_entry_voice_queued_at=-1;
    int pit_entry_voice_result=0;
    bool gun_audio=false;
    F1CPitCrewSnapshot crew{};
    bool crew_read=false;
    double crew_anchor=0;
    double gun_off_at[4]{},gun_on_at[4]{},gun_retry_at=0;
    double wheelgun_generic_at=0;
    bool wheelgun_generic_announced=false;
    unsigned char gun_off_done[4]{},gun_on_done[4]{};
    int gun_retry_count=0;
    /* Crew-synced wheel audio: bursts follow the visible gun at the nut. */
    bool gun_synced=false,gun_sync_closed=false;
    float gun_dwell[4][2]{};
    double gun_stutter_at[4]{},gun_stutter_until[4]{};
    /* Race-engineer "Gear" call when the lollipop shows 1ST GEAR (player only). */
    double gear_at=0;bool gear_done=false,gear_closed=false;
    /* "Box! Pit now" near the player's own box; BRAKE busy-retry window. */
    bool box_called=false;double box_retry_at=0,brake_until=0,brake_retry_at=0;
    std::vector<DeferredFeedback> deferred;
    std::string arm, phase="On track";
    DWORD coop_wait_since=0;   /* CO-OP: remote car stopped, plan not yet received */
};
static Car cars[104];
static std::map<std::string,Request> requests;
static std::vector<std::string> events;

/* --- two-stage (v3) tables, plan view and session wrappers --------------- */
static const char *const v3_parent_names[PF3_PARENT_COUNT]={"slow_wheel","wheelgun","fuel_rig","stall","cross_thread"};
static const char *const v3_parent_labels[PF3_PARENT_COUNT]={"Slow wheel change","Wheel gun / wheel nut problem","Fuel rig / hose problem","Engine stall","Cross-threaded / binding nut"};
static const char *const v3_resolution_names[PF3_PARENT_COUNT][PF3_RESOLUTION_MAX]={
    {"minor","awkward",nullptr},{"quick_fix","backup_gun","irreparable_nut"},
    {"slow_flow","temporary_no_flow","interrupted_flow","spill"},
    {"straightforward","difficult_restart",nullptr},
    {"corrected","lengthy_rework","irreparable"}};
static int v3_parent_index(const std::string &name) {
    for(int i=0;i<PF3_PARENT_COUNT;i++)if(name==v3_parent_names[i])return i;
    return -1;
}
static int v3_resolution_index(int parent,const std::string &name) {
    if(parent<0||parent>=PF3_PARENT_COUNT)return -1;
    for(int i=0;i<PF3_RESOLUTION_MAX;i++)if(v3_resolution_names[parent][i] && name==v3_resolution_names[parent][i])return i;
    return -1;
}
/* Shared adapter code keeps reading the accepted plan fields; the v3 payload
 * stays available for diagnostics. Terminal comes from the severity flag. */
static pf_plan_t plan_view_v3(const pf3_plan_t &p) {
    pf_plan_t out{};
    out.outcome=p.outcome;out.severity=p.severity;out.category_index=p.category_index;
    out.affected_job_kind=p.affected_job_kind;out.affected_job_extra_seconds=p.affected_job_extra_seconds;
    out.terminal_presentation_seconds=p.terminal_presentation_seconds;out.wheel=p.wheel;
    out.fuel_subtype=p.fuel_subtype;out.extra_job_seconds=p.extra_job_seconds;
    out.attempts=p.attempts;out.terminal=p.terminal;
    return out;
}
static int session_commit(int selector,const pf3_forcing_t *forcing,
    const pf_identity_t &id,const pf_context_t &ctx,double at,
    pf_plan_t *plan2,pf3_plan_t *plan3,int *committed) {
    if(policy_model==3) {
        int rc=pit_policy_math([&]{return pf3_session_commit(policy3,&id,&ctx,&ctx.eligibility,at,forcing,plan3,committed);});
        if(rc==0&&*committed)*plan2=plan_view_v3(*plan3);
        return rc;
    }
    return pit_policy_math([&]{return pft_session_commit(policy,&id,&ctx,&ctx.eligibility,at,plan2,committed,selector);});
}
static void session_bind_timing(const pf_identity_t &id,const pf_job_t *jobs,size_t n) {
    if(policy_model==3)pit_policy_math([&]{return pf3_session_bind_timing(policy3,&id,jobs,n);});
    else pit_policy_math([&]{return pf_session_bind_timing(policy,&id,jobs,n);});
}
static int session_advance(double at,pf_command_t *cmds,int cap) {
    if(policy_model==3)return pit_policy_math([&]{return pf3_session_advance(policy3,at,cmds,cap);});
    return pit_policy_math([&]{return pf_session_advance(policy,at,cmds,cap);});
}
static int session_apply_timing_model(int model,const Car &c,const pf_job_t *jobs,size_t n,pf_timing_result_t *timing) {
    /* Timing math must run under the same scoped double-precision guard as
     * every other policy call (review L2). */
    if(model==3)return pit_policy_math([&]{return pf3_plan_apply_timing(&c.plan3,jobs,n,timing);});
    return pit_policy_math([&]{return pf_plan_apply_timing(&c.plan,jobs,n,timing);});
}
static int session_apply_timing(const Car &c,const pf_job_t *jobs,size_t n,pf_timing_result_t *timing) {
    return session_apply_timing_model(policy_model,c,jobs,n,timing);
}
static pf_session_error_t session_last_error() {
    return policy_model==3?pf3_session_last_error(policy3):pf_session_last_error(policy);
}
struct SfxRequest {unsigned epoch,slot,object;uint64_t token;double at;int corner,burst;float gain;double distance_m;};
static std::vector<SfxRequest> sfx_requests;
static char log_path[MAX_PATH];
static void startup_refuse(DWORD code,const std::string &message) {
    FILE *file=std::fopen(log_path,"a");
    if(file){std::fprintf(file,"Pit Stop Failures refused: %s\n",message.c_str());std::fclose(file);}
    if(policy3)pf3_session_destroy(policy3);
    if(policy)pf_session_destroy(policy);
    f1c_broker_release(&broker);
    if(owner_mutex){ReleaseMutex(owner_mutex);CloseHandle(owner_mutex);owner_mutex=nullptr;}
    FreeLibraryAndExitThread(module,code);
}
static std::string driver_utf8(const char *name) {
    wchar_t wide[128]; char utf8[512];
    if(!MultiByteToWideChar(CP_ACP,0,name,-1,wide,128) ||
       !WideCharToMultiByte(CP_UTF8,0,wide,-1,utf8,512,nullptr,nullptr)) return "Driver";
    return utf8;
}

static std::string quoted(const std::string &s) {
    std::string out="\"";
    for (unsigned char c:s) {
        if (c=='"' || c=='\\') { out+='\\'; out+=char(c); }
        else if (c>=32) out+=char(c);
        else out+=' ';
    }
    return out+'"';
}
static std::string number(double n) { char b[64]; std::snprintf(b,sizeof b,"%.6f",std::isfinite(n)?n:0); return b; }
static void event(const std::string &s) {
    std::string line="{\"time\":"+number(now)+",\"message\":"+quoted(s)+"}";
    events.push_back(line); if(events.size()>100) events.erase(events.begin());
    /* Logging is drained by the worker, never disk I/O on the game thread. */
}
static void log_w(const char *fmt,...) {
    char buffer[512];va_list args;va_start(args,fmt);std::vsnprintf(buffer,sizeof buffer,fmt,args);va_end(args);
    pitu_tap(buffer);   /* batch U2: feed the usage summary */
    EnterCriticalSection(&lock);event(buffer);LeaveCriticalSection(&lock);
}
#include "legacy_audio.h"
#include "pit_gun_sync.h"
/* Wheel-gun bursts bypass the game's mixer, so they follow the player's own
 * sound sliders from the most recently written profile under Save. A
 * single-profile install resolves exactly; several profiles use the newest. */
static PitSoundMix sound_mix;
static std::string sound_mix_source="defaults (no player profile)";
static void load_sound_mix(void) {
    char root[MAX_PATH];
    DWORD n=GetModuleFileNameA(nullptr,root,MAX_PATH);
    if(!n||n>=MAX_PATH)return;
    char *slash=std::strrchr(root,'\\');if(!slash)return;slash[1]=0;
    std::string save=std::string(root)+"Save\\";
    WIN32_FIND_DATAA found{};
    HANDLE search=FindFirstFileA((save+"*").c_str(),&found);
    if(search==INVALID_HANDLE_VALUE)return;
    std::string best;FILETIME best_time{};
    do {
        if(!(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||found.cFileName[0]=='.')continue;
        std::string candidate=save+found.cFileName+"\\"+found.cFileName+".PLR";
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if(!GetFileAttributesExA(candidate.c_str(),GetFileExInfoStandard,&info))continue;
        if(best.empty()||CompareFileTime(&info.ftLastWriteTime,&best_time)>0) {
            best=candidate;best_time=info.ftLastWriteTime;
        }
    } while(FindNextFileA(search,&found));
    FindClose(search);
    if(best.empty())return;
    FILE *file=std::fopen(best.c_str(),"rb");if(!file)return;
    std::string text(262144,'\0');
    size_t got=std::fread(&text[0],1,text.size(),file);std::fclose(file);
    text.resize(got);
    PitSoundMix parsed=pit_sound_mix_parse(text);
    if(!parsed.from_profile)return;
    sound_mix=parsed;
    sound_mix_source=best.substr(save.size());
}
static int native_read(uintptr_t address,void *out,size_t n) {
    if(!address || IsBadReadPtr((void*)address,n))return 0;
    memcpy(out,(void*)address,n);return 1;
}
#ifdef PIT_AI_MECHANICS_TEST
static unsigned char test_player_ignition=1;
static double test_player_physics_rpm=0.0,test_player_idle_rpm=900.0;
static int32_t test_retire_player_out=0;   /* PLAYER_OUT fixture for player_out_now() */
static volatile unsigned char *player_ignition_flag() { return &test_player_ignition; }
static volatile double *player_physics_rpm() { return &test_player_physics_rpm; }
static volatile double *player_idle_rpm() { return &test_player_idle_rpm; }
#else
static volatile unsigned char *player_ignition_flag() {
    return (volatile unsigned char*)G_EngineRunningIgnitionFlag;
}
/* sub_4471F0 uses player physics object 0x7C7428 plus offsets 7260 and
 * 7300. The old 0x7C90C4/EC labels were retracted; these are 0x7C9084/AC. */
static volatile double *player_physics_rpm() { return (volatile double*)0x007C9084u; }
static volatile double *player_idle_rpm() { return (volatile double*)0x007C90ACu; }
#endif
static bool player_out_now() {
#ifdef PIT_AI_MECHANICS_TEST
    return test_retire_player_out!=0;
#else
    int32_t value=0;
    return native_read(0x007CE258u,&value,sizeof value) && value!=0;
#endif
}

/* 1.0.8: the player wheel-jam DNF (and every terminal player pit failure)
 * uses the broker's game-thread native retire route (Lightloader >=2.2.3):
 * arm the pause menu, send RETIRE_PITS; the broker retires the player on the
 * game thread and reports CONTINUE. The old cross-thread SessionType flip and
 * the cross-thread vehicle write set are GONE. An older broker that only
 * echoes RETIRE_PITS gets a refusal - PSF never retires the player from
 * another thread. */
static void handle_broker_event(const F1CBrokerEvent &e) {
    if(e.type!=F1C_BROKER_EVENT_PAUSE_MENU)return;
    if(e.value==F1C_BROKER_MENU_CONTINUE) {
        if(InterlockedCompareExchange(&player_retire_pending,0,1)==1) {
            broker.pause_menu_arm(client,0u);
            log_w("PLAYER RETIRE: broker-owned native garage retirement (Lightloader 2.2.3+); menu arm released");
        }
        return;
    }
    if(e.value==F1C_BROKER_MENU_RETIRE_PITS) {
        if(InterlockedCompareExchange(&player_retire_pending,0,1)==1) {
            broker.pause_menu_arm(client,0u);
            log_w("PLAYER RETIRE: broker did not own the native retire route; no cross-thread retirement (Lightloader 2.2.3+ required)");
        }
        return;
    }
}
static bool start_owned_stall(Car &car,void *vehicle) {
    if(car.snapshot.controller_role==0) {
        volatile unsigned char *ignition=player_ignition_flag();
#ifndef PIT_AI_MECHANICS_TEST
        if(IsBadReadPtr((const void*)ignition,1)||IsBadWritePtr((void*)ignition,1))return false;
#endif
        unsigned char before=*ignition;
        if(before>1)return false;
        car.player_ignition_restore=before;
        car.player_restart_pending=false;car.player_restart_confirmed=false;
        car.player_restart_feedback_pending=false;car.player_restart_announced=false;
        car.player_restart_attempts=0;car.player_restart_started_at=0;
        *ignition=0;
        car.player_ignition_owned=(*ignition==0);
        car.stalled=car.player_ignition_owned;
        event(driver_utf8(car.snapshot.driver_name)+(car.stalled?": player ignition stopped":": player ignition stop refused"));
        return car.stalled;
    }
    car.stalled=f1c_ai_engine_stop(native_read,(uintptr_t)vehicle,GetCurrentThreadId(),1)!=0;
    event(driver_utf8(car.snapshot.driver_name)+(car.stalled?": native AI engine stopped":": native AI engine stop refused"));
    return car.stalled;
}
static int voice(Car &car,int eid);
static void restart_owned(Car &car,bool require_confirmation=true) {
    if(!car.stalled)return;
    if(car.player_ignition_owned) {
        volatile unsigned char *ignition=player_ignition_flag();
        bool valid=true;
#ifndef PIT_AI_MECHANICS_TEST
        valid=!IsBadWritePtr((void*)ignition,1);
#endif
        if(!valid){event(driver_utf8(car.snapshot.driver_name)+": player starter preflight refused");return;}
        /* The old mechanical-failure mod proved 0x7C9081 as the ignition
         * control. Its later documentation explicitly retracts 0x7C90C4 as
         * an RPM address, so do not write that unrelated double here. */
        if(require_confirmation && car.player_restart_pending && car.player_restart_attempts>0 &&
           car.player_ignition_restore && *ignition==car.player_ignition_restore &&
           std::isfinite(car.rpm) && car.rpm>100.0f) {
            car.player_restart_pending=false;car.player_restart_confirmed=true;
            car.player_ignition_owned=false;car.stalled=false;
            event(driver_utf8(car.snapshot.driver_name)+": engine restart confirmed at rpm "+number(car.rpm));
            return;
        }
        if(require_confirmation && car.player_restart_pending &&
           now-car.player_restart_started_at>=3.0) {
            *ignition=0;
            car.player_restart_pending=false;car.player_restart_feedback_pending=false;
            car.terminal=true;car.terminal_at=now+5.0;car.holding=true;
            voice(car,117);
            car.player_retire_voice_queued=true;
            event(driver_utf8(car.snapshot.driver_name)+": engine restart timed out; retire-to-garage fallback armed");
            return;
        }
        *ignition=car.player_ignition_restore;
        if(!require_confirmation) {
            car.player_restart_pending=false;car.player_restart_confirmed=false;
            car.player_ignition_owned=false;car.stalled=false;
            return;
        }
        /* Match the native starter's ignition + idle-speed lift. Never use
         * the lifted physics value as proof: only a later vehicle snapshot
         * above 100 RPM may confirm recovery. */
        volatile double *rpm=player_physics_rpm(),*idle=player_idle_rpm();
        bool starter_ready=true;
#ifndef PIT_AI_MECHANICS_TEST
        starter_ready=!IsBadWritePtr((void*)rpm,sizeof(double)) &&
                      !IsBadReadPtr((const void*)idle,sizeof(double));
#endif
        if(starter_ready && std::isfinite(*rpm) && std::isfinite(*idle) &&
           *rpm>=0.0 && *rpm<*idle && *idle>=500.0 && *idle<=3000.0)
            *rpm=*idle;
        if(!car.player_restart_pending) {
            event(driver_utf8(car.snapshot.driver_name)+": player starter engaged; awaiting engine confirmation");
            car.player_restart_started_at=now;
        }
        car.player_restart_pending=true;car.player_restart_attempts++;
        return;
    }
    F1CBrokerVehicleSnapshot current{};current.structure_size=sizeof current;
    if(broker.get_vehicle_snapshot(client,car.snapshot.handle,&current)==F1C_BROKER_OK &&
       !memcmp(&current.handle,&car.snapshot.handle,sizeof current.handle)) {
        void *v=f1c_vehicle_slot((int)current.handle.slot);
        if(f1c_ai_pit_restart(native_read,(uintptr_t)v,GetCurrentThreadId()))
            event(driver_utf8(car.snapshot.driver_name)+": engine restart confirmed");
    }
    car.stalled=false;
}
static void cancel(Car &car, const char *why) {
    restart_owned(car,false);
    car.fuel=FuelEffect{};
    if(policy_model==3 && car.token && car.snapshot.handle.slot<104) {
        /* v3 adapter: retire the committed identity so late callbacks and
         * undelivered events for a replaced/vanished driver cannot resurface. */
        pf_identity_t id{epoch,((uint64_t)car.snapshot.handle.object_generation<<8)|car.snapshot.handle.slot,car.token};
        pit_policy_math([&]{return pf3_session_cancel(policy3,&id,now);});
        car.token=0;
    }
    if (!car.arm.empty()) { auto &r=requests.at(car.arm); r.state="cancelled"; r.message=why; car.arm.clear(); }
    car.holding=false;car.gun_audio=false;car.gun_retry_at=0;car.hold_corner=-1;car.hold_end=0;car.work_done_at=0;
    car.crew=F1CPitCrewSnapshot{};car.crew_read=false;car.crew_anchor=0;
    car.pit_entry_pending=false;car.pit_entry_retry_at=0;car.pit_entry_until=0;
    car.wheelgun_generic_at=0;car.deferred.clear();
    car.stall_notice_pending=false;car.stall_notice_retry_at=0;
}
static volatile LONG cleanup_done=0;
static void __cdecl cleanup_mechanics(void*) {
    EnterCriticalSection(&lock);
    for(auto &car:cars)cancel(car,"Client unloading");
    LeaveCriticalSection(&lock);InterlockedExchange(&cleanup_done,1);
}
static int voice(Car &car,int eid) {
    if(eid<0)return -1;
    void *v=f1c_vehicle_slot((int)car.snapshot.handle.slot);
    int subject=car.snapshot.controller_role==0?-1:VEHF_DriverID(v);
    /* The player's pit-procedure calls (entry, limiter, box, brake, gear, go)
     * go to the front of the native radio queue so they land on the action. */
    bool front=car.snapshot.controller_role==0 &&
        (eid==44||eid==45||eid==46||eid==50||eid==51||eid==53);
    int accepted=front?f1c_pit_voice_enqueue_front(eid,subject):f1c_pit_voice_enqueue(eid,subject);
    event(driver_utf8(car.snapshot.driver_name)+": voice "+std::to_string(eid)+
          " subject "+std::to_string(subject)+(accepted==1?" queued":accepted==0?" busy":" refused"));
    return accepted;
}
/* r15: the Safety Car is identified like the Safety Car mod does it: its
 * .veh Classes line carries the tokens "Safety" and "NotAI" (comma list,
 * case-insensitive; sc_vehicle_selection.h). The loaded car's Classes string
 * is in the roster table: dword_8AC394 + 1060*roster + 444 (VehFileParser
 * 0x4C3200 parses Classes to this+444; team at +124 is the ABI-8 confirmed
 * read), roster index at vehicle+0x4898, count at dword_8AC390. The Safety
 * Car mod's year token only picks which file it loads, so it is not needed
 * to recognise a car that is already in the field. */
static bool classes_token(const char *list,const char *want) {
    const size_t w=std::strlen(want);
    const char *p=list;
    while(*p) {
        while(*p==',' || *p=='"' || std::isspace((unsigned char)*p))++p;
        const char *b=p;
        while(*p && *p!=',' && *p!='"')++p;
        const char *e=p;
        while(e>b && std::isspace((unsigned char)e[-1]))--e;
        if((size_t)(e-b)==w && _strnicmp(b,want,w)==0)return true;
    }
    return false;
}
static bool is_safety_car(void *v) {
    if(!v || IsBadReadPtr((uint8_t*)v+0x4898,4) || IsBadReadPtr((void*)0x008AC390u,8))return false;
    const int roster=*(const int*)((uint8_t*)v+0x4898);
    const int count=*(volatile int*)0x008AC390u;
    const uintptr_t table=*(volatile uintptr_t*)0x008AC394u;
    if(!table || roster<0 || roster>=count)return false;
    const char *src=(const char*)(table+1060u*(uintptr_t)roster+444u);
    if(IsBadReadPtr(src,256))return false;
    char classes[257];
    std::memcpy(classes,src,256);classes[256]=0;
    return classes_token(classes,"Safety") && classes_token(classes,"NotAI");
}
/* r12: "rejoined in P" lines the game never plays. Native pit-exit
 * commentary (PitExit 0x41C0F0) formats (eid, driver id, position). EID 60
 * PWS_2001_96 (player, "you've rejoined in P"), EID 190 PWS_AIPIT13 (AI,
 * "X has rejoined in P"). Local call: the SDK allowlist excludes them.
 * r13: race sessions only (SessionType 7; 5 is qualifying).
 * r15: the Safety Car never reaches here (is_safety_car, main loop); the
 * r13 driver-name check is removed.
 * r14: EID 60's clip type (0x10) makes the splice resolver read record
 * word4, not word5 (static: SoundEventDispatcher case 0x10 -> sub_412490
 * with a3[4]); the player had subject -1 there, so no position was spliced.
 * Pass the position in word4 for the player as well (subject is unused for
 * player lines). AI (EID 190, type 0x17) reads word5 and is unchanged. */
static int voice_rejoin(Car &car,int position) {
    const bool player=car.snapshot.controller_role==0;
    const int eid=player?60:190;
    void *v=f1c_vehicle_slot((int)car.snapshot.handle.slot);
    const int subject=player?-1:VEHF_DriverID(v);
    int accepted=-1;(void)subject;
    const std::string who=driver_utf8(car.snapshot.driver_name);
    const char *why="refused";
#ifndef F1C_PIT_VOICE_TEST
    if(position<1 || position>40 || (!player && subject<0))why="out of range";
    else if(!*(volatile int*)G_RaceActiveFlag || *(volatile float*)G_TrackConditionState==0)why="no active race";
    else if(*(volatile int*)G_SessionType!=7)why="not race session";
    else {
        float record[8];
        ((F1CPitVoiceInit)0x004144D0u)(record);
        const int slot4=player?position:subject;
        ((F1CPitVoiceFormat)F1C_FN_CommentaryEventQueue)(record,eid,slot4,position,-1,-1,0.0f,4.0f);
        accepted=((F1CPitVoiceInsert)0x00418560u)((void*)0x005EA618u,record)?1:0;
        why=accepted==1?"queued":"queue busy";
    }
#endif
    event(who+": rejoin voice "+std::to_string(eid)+" P"+std::to_string(position)+
          " "+why);
    return accepted;
}
static void observe_retirement(Car &car,const F1CVehicleLifecycle &life) {
    if(!car.retirement_requested || !car.snapshot.retired)return;
    if(life.valid && life.garage_valid && life.garage_clear && !life.ai_control && life.finish==2) {
        if(!car.garaged) {
            event(driver_utf8(car.snapshot.driver_name)+": DNF and garage placement confirmed");
            voice(car,237); // Catalogue AIRETIRE02: generic AI retirement, correct named subject.
        }
        car.garaged=true;car.phase="Retired in garage";
    }else car.phase="Retirement: waiting for garage confirmation";
}
/* r9: the native writer (0x45B9C0) also sets colour (+0x8C) and priority
 * (+0x94); the SDK helper sets only text/timer, so a slot kept its last
 * occupant's colour (e.g. the green-flag message). Force white (-1). */
static int pit_hud_post_white(const char *msg) {
    int written=f1c_pit_hud_post(msg);
#ifndef F1C_PIT_HUD_TEST
    if(written>0 && written<=8) {
        unsigned char *inner=*(unsigned char *volatile*)0x007CD204u;
        int *colour=inner?(int*)(inner+(written-1)*0x5C+0x8C):nullptr;
        if(colour && !IsBadWritePtr(colour,sizeof *colour))*colour=-1;
    }
#endif
    return written;
}
static void deliver_feedback(Car &car,const pf_command_t &command) {
    bool player=car.snapshot.controller_role==0;
    LegacyFeedback f=legacy_feedback(car.plan,(pf_command_kind_t)command.kind,command.attempt,player);
    voice(car,f.eid);
    if(player && car.plan.outcome==PF_OUTCOME_WHEELGUN &&
       command.kind==PF_CMD_FAULT_NOTICE && !car.wheelgun_generic_announced)
        car.wheelgun_generic_at=now+3.0;
    if(f.message) {
        /* Player's in-car HUD; AI retains named engineer/timeline reporting. */
        int written=player?pit_hud_post_white(f.message):0;
        event(driver_utf8(car.snapshot.driver_name)+": "+f.message+(player?(written?" [HUD written]":" [HUD busy]"):""));
    }
}
/* Failure-animation probe (trial only, read-only): log every crew actor's
 * channel window whenever it changes, from stop start until 20 s after the
 * game releases the car. Answers whether windows are re-read, extended by
 * refuelling, or tied to service completion. */
static void probe_crew(Car &car,void *vehicle,bool ready,double now,double stop_start,double stop_end) {
    if(ready)car.probe_until=now+20.0;
    if(now>car.probe_until)return;
    const std::string who=driver_utf8(car.snapshot.driver_name);
    if(stop_start!=car.probe_stop || stop_end!=car.probe_end) {
        event("probe "+who+": now "+number(now)+" ready "+std::to_string(ready)+" stop "+number(stop_start)+
              "-"+number(stop_end)+" outcome "+std::to_string((int)car.plan.outcome)+
              " fuel_subtype "+std::to_string((int)car.plan.fuel_subtype)+" extra "+number(car.plan.extra_job_seconds));
        if(stop_start!=car.probe_stop)memset(car.probe_win,0,sizeof car.probe_win);
        car.probe_stop=stop_start;car.probe_end=stop_end;
    }
    uint32_t crew=0,array=0,count=0;
    if(!f1c_pit_crew_read_at(native_read,(uintptr_t)vehicle,0x32d4u,&crew,sizeof crew) || !crew ||
       !f1c_pit_crew_read_at(native_read,crew,0x0cu,&count,sizeof count) ||
       !f1c_pit_crew_read_at(native_read,crew,0x10u,&array,sizeof array) ||
       !array || !count || count>F1C_PIT_CREW_MAX_ACTORS)return;
    for(uint32_t i=0;i<count;i++) {
        const size_t offset=(size_t)i*120u;
        uint32_t name_ptr=0,channel_ptr=0,channels=0;
        if(!f1c_pit_crew_read_at(native_read,array,offset+100u,&name_ptr,sizeof name_ptr) ||
           !f1c_pit_crew_read_at(native_read,array,offset+112u,&channels,sizeof channels) ||
           !f1c_pit_crew_read_at(native_read,array,offset+116u,&channel_ptr,sizeof channel_ptr) ||
           !channels || channels>32 || !channel_ptr)continue;
        char name[F1C_PIT_CREW_NAME_SIZE]{};
        for(size_t j=0;name_ptr && j+1<sizeof name;j++) {
            if(!f1c_pit_crew_read_at(native_read,name_ptr,j,&name[j],1)||!name[j])break;
            if((unsigned char)name[j]<32||(unsigned char)name[j]>126){name[j]=0;break;}
        }
        for(uint32_t ch=0;ch<channels && ch<4;ch++) {
            float w[5]{};
            if(!f1c_pit_crew_read_at(native_read,channel_ptr,(size_t)ch*28u,w,sizeof w))continue;
            if(std::fabs(w[0]-car.probe_win[i][ch][0])<0.001f && std::fabs(w[1]-car.probe_win[i][ch][1])<0.001f)continue;
            car.probe_win[i][ch][0]=w[0];car.probe_win[i][ch][1]=w[1];
            event("probe "+who+": rel "+number(now-stop_start)+" a"+std::to_string(i)+" "+name+" ch"+std::to_string(ch)+
                  " abs "+number(w[0])+"-"+number(w[1])+" base "+number(w[3])+"-"+number(w[4]));
        }
    }
}

/* Trial r2 (write test): move the failing corner's crew, the jacks'
 * lowering (ch1) and the lollipop turn (ch1) later by the failure delay, so
 * the fault plays during that corner's work and the release crew wait.
 * Only channels that have not started yet are moved. Returns channels moved. */
static int shift_crew_for_fault(Car &car,void *vehicle,double rel_now,double delay) {
    const int corner=car.plan.wheel;
    if(corner<0||corner>3||!std::isfinite(delay)||delay<=0.05||delay>60)return 0;
    uint32_t crew=0,array=0,count=0;
    if(!f1c_pit_crew_read_at(native_read,(uintptr_t)vehicle,0x32d4u,&crew,sizeof crew) || !crew ||
       !f1c_pit_crew_read_at(native_read,crew,0x0cu,&count,sizeof count) ||
       !f1c_pit_crew_read_at(native_read,crew,0x10u,&array,sizeof array) ||
       !array || !count || count>F1C_PIT_CREW_MAX_ACTORS)return 0;
    /* r5: the lollipop turn (and the jacks with it) waits until the failing
     * corner's last tightening ends, plus 0.3 s. */
    double release_shift=delay;
    {
        PitGunChannel gun[4],pop;
        const PitGunContact *track=corner<2?PIT_GUN_FRONT:PIT_GUN_REAR;
        if(pit_gun_channels_read(native_read,(uintptr_t)vehicle,gun)>0 && gun[corner].found &&
           pit_lollipop_channel_read(native_read,(uintptr_t)vehicle,&pop)) {
            const double on_end=gun[corner].start_abs+((double)track[1].at-gun[corner].base_start)+track[1].dwell+delay;
            const double turn=pop.start_abs+PIT_LOLLIPOP_TURN;
            if(std::isfinite(on_end))car.work_done_at=car.start+on_end;
            if(std::isfinite(on_end)&&std::isfinite(turn))release_shift=std::max(delay,on_end+1.5-turn);
            if(release_shift>delay+30)release_shift=delay;
        }
    }
    car.release_shift=release_shift;
    int moved=0;std::string list;
    for(uint32_t i=0;i<count;i++) {
        const size_t offset=(size_t)i*120u;
        uint32_t name_ptr=0,channel_ptr=0,channels=0;
        if(!f1c_pit_crew_read_at(native_read,array,offset+100u,&name_ptr,sizeof name_ptr) ||
           !f1c_pit_crew_read_at(native_read,array,offset+112u,&channels,sizeof channels) ||
           !f1c_pit_crew_read_at(native_read,array,offset+116u,&channel_ptr,sizeof channel_ptr) ||
           !name_ptr || !channels || channels>32 || !channel_ptr)continue;
        char name[F1C_PIT_CREW_NAME_SIZE]{};
        for(size_t j=0;j+1<sizeof name;j++) {
            if(!f1c_pit_crew_read_at(native_read,name_ptr,j,&name[j],1)||!name[j])break;
            name[j]=(char)std::toupper((unsigned char)name[j]);
        }
        /* Objects use FRONT_LEFT, men use FLEFT (FMAN_FLEFT etc.). */
        static const char *const men[4]={"_FLEFT","_FRIGHT","_RLEFT","_RRIGHT"};
        const bool own=f1c_pit_crew_corner(name)==corner||std::strstr(name,men[corner])!=nullptr;
        const bool release=!std::strcmp(name,"JACK_FRONT")||!std::strcmp(name,"TMAN")||
                           !std::strcmp(name,"JACK_REAR")||!std::strcmp(name,"RMAN")||
                           !std::strcmp(name,"LMAN")||!std::strcmp(name,"LOLLIPOP");
        if(own) {
            /* r4: the corner is held mid-motion per tick, not moved here. */
            if(pit_gun_name_is_gun(name) && channels>0) {
                float g[5]{};
                if(f1c_pit_crew_read_at(native_read,channel_ptr,0,g,sizeof g) && std::isfinite(g[0])) {
                    const PitGunContact *track=corner<2?PIT_GUN_FRONT:PIT_GUN_REAR;
                    const bool slow=car.plan.outcome==PF_OUTCOME_SLOW_CORNER;
                    const int k=(car.plan.outcome==PF_OUTCOME_CROSS_THREAD||slow)?1:0;
                    const double offset=slow?-0.4:0.1;
                    car.hold_contact=k;car.hold_delay=delay;car.hold_corner=corner;
                    car.hold_from=car.start+g[0]+((double)track[k].at-g[3])+offset;
                    car.hold_until=car.hold_from+delay;car.hold_last=car.hold_from;car.hold_end=car.hold_until;
                    list+=std::string(" hold ")+name+" contact "+std::to_string(k)+" at "+number(car.hold_from);
                }
            }
            continue;
        }
        if(!release)continue;
        for(uint32_t ch=release?1:0;ch<channels && ch<4;ch++) {
            float *w=(float*)(uintptr_t)(channel_ptr+ch*28u);
            if(IsBadReadPtr(w,8)||IsBadWritePtr(w,8))continue;
            if(!std::isfinite(w[0])||!std::isfinite(w[1])||w[1]<w[0]||w[0]<=rel_now+0.05)continue;
            w[0]+=(float)release_shift;w[1]+=(float)release_shift;moved++;
            list+=std::string(" ")+name+"/"+std::to_string(ch);
            if(release)break;
        }
    }
    event(driver_utf8(car.snapshot.driver_name)+": r7 crew hold +"+number(delay)+" s, release shift +"+number(release_shift)+" s corner "+
          std::to_string(corner)+" work done "+number(car.work_done_at)+" moved "+std::to_string(moved)+":"+list);
    return moved;
}

/* r4: hold the failing corner's pose by moving its windows forward by
 * each tick's sim time while the hold is active. */
static void pump_corner_hold(Car &car,void *vehicle) {
    if(car.hold_corner<0 || now<car.hold_from)return;
    const double to=std::min(now,car.hold_until);
    const double dt=to-car.hold_last;
    if(dt<=0){if(now>=car.hold_until){event(driver_utf8(car.snapshot.driver_name)+": r7 corner hold released");car.hold_corner=-1;}return;}
    car.hold_last=to;
    uint32_t crew=0,array=0,count=0;
    if(!f1c_pit_crew_read_at(native_read,(uintptr_t)vehicle,0x32d4u,&crew,sizeof crew) || !crew ||
       !f1c_pit_crew_read_at(native_read,crew,0x0cu,&count,sizeof count) ||
       !f1c_pit_crew_read_at(native_read,crew,0x10u,&array,sizeof array) ||
       !array || !count || count>F1C_PIT_CREW_MAX_ACTORS){car.hold_corner=-1;return;}
    static const char *const men[4]={"_FLEFT","_FRIGHT","_RLEFT","_RRIGHT"};
    for(uint32_t i=0;i<count;i++) {
        const size_t offset=(size_t)i*120u;
        uint32_t name_ptr=0,channel_ptr=0,channels=0;
        if(!f1c_pit_crew_read_at(native_read,array,offset+100u,&name_ptr,sizeof name_ptr) ||
           !f1c_pit_crew_read_at(native_read,array,offset+112u,&channels,sizeof channels) ||
           !f1c_pit_crew_read_at(native_read,array,offset+116u,&channel_ptr,sizeof channel_ptr) ||
           !name_ptr || !channels || channels>32 || !channel_ptr)continue;
        char name[F1C_PIT_CREW_NAME_SIZE]{};
        for(size_t j=0;j+1<sizeof name;j++) {
            if(!f1c_pit_crew_read_at(native_read,name_ptr,j,&name[j],1)||!name[j])break;
            name[j]=(char)std::toupper((unsigned char)name[j]);
        }
        if(f1c_pit_crew_corner(name)!=car.hold_corner && !std::strstr(name,men[car.hold_corner]))continue;
        for(uint32_t ch=0;ch<channels && ch<4;ch++) {
            float *w=(float*)(uintptr_t)(channel_ptr+ch*28u);
            if(IsBadReadPtr(w,8)||IsBadWritePtr(w,8)||!std::isfinite(w[0])||!std::isfinite(w[1]))continue;
            w[0]+=(float)dt;w[1]+=(float)dt;
        }
    }
}

static void try_stall_notice(Car &car) {
    if(!car.stalled || !car.stall_notice_pending || car.stall_notice_announced ||
       now<car.stall_notice_retry_at)return;
    LegacyFeedback f=legacy_feedback(car.plan,PF_CMD_FAULT_NOTICE,0,
                                     car.snapshot.controller_role==0);
    int accepted=voice(car,f.eid);
    if(accepted==1) {
        car.stall_notice_announced=true;car.stall_notice_pending=false;
        if(f.message) {
            bool player=car.snapshot.controller_role==0;
            int written=player?pit_hud_post_white(f.message):0;
            event(driver_utf8(car.snapshot.driver_name)+": "+f.message+
                  (player?(written?" [HUD written]":" [HUD busy]"):""));
        }
    } else car.stall_notice_retry_at=now+0.5;
}
static void feedback(Car &car,const pf_command_t &command) {
    bool player=car.snapshot.controller_role==0;
    if(car.plan.outcome==PF_OUTCOME_STALL_DELAY) {
        if(command.kind==PF_CMD_FAULT_NOTICE) {
            if(car.stall_notice_announced)return;
            car.stall_notice_pending=true;
            try_stall_notice(car);
            return;
        }
        if(command.kind==PF_CMD_RECOVERED) {
            if(player)car.player_restart_feedback_pending=true;
            if(!car.stalled) {
                if(!player || !car.player_restart_confirmed)return;
            } else {
                restart_owned(car);
                if(car.stalled)return;
            }
        } else if(!car.stalled)return;
    }
    deliver_feedback(car,command);
    if(player && car.plan.outcome==PF_OUTCOME_STALL_DELAY && command.kind==PF_CMD_RECOVERED) {
        car.player_restart_announced=true;car.player_restart_feedback_pending=false;
        car.player_restart_confirmed=false;
    }
    /* Wheel-gun WAVs retain the measured service clock. Crew channel values
     * are observed separately and are not calibrated audible seconds. */
}
static void fuel_recovered_feedback(Car &car) {
    if(!car.fuel_recovery_owned || car.fuel_recovery_announced)return;
    car.fuel_recovery_announced=true;
    pf_command_t recovered{};recovered.kind=PF_CMD_RECOVERED;
    recovered.attempt=car.plan.attempts;
    deliver_feedback(car,recovered);
}
static void queue_wheel_sfx(Car &car,int corner,int variant) {
    if(sfx_requests.size()<128)
        sfx_requests.push_back({epoch,car.snapshot.handle.slot,car.snapshot.handle.object_generation,
                                car.token,now,corner,variant,0.0f,-1.0});
}
static int wheel_sfx_variant(const Car &car,int corner,int phase) {
    uint64_t vehicle=((uint64_t)car.snapshot.handle.object_generation<<8)|car.snapshot.handle.slot;
    uint64_t value=pit_presentation_mix(seed^vehicle^pit_presentation_mix(car.token)^ 
                                        ((uint64_t)(corner+1)<<16)^(uint64_t)phase);
    return (int)(value%3);
}
static void schedule_presentation(Car &car,const F1CPitMachine *pit,bool play_wheels) {
    uint64_t vehicle=((uint64_t)car.snapshot.handle.object_generation<<8)|car.snapshot.handle.slot;
    car.mechanic_started=false;car.deferred.clear();car.gun_retry_at=0;car.gun_retry_count=0;
    car.stall_notice_pending=false;car.stall_notice_announced=false;
    car.stall_notice_retry_at=0;
    car.wheelgun_generic_at=0;car.wheelgun_generic_announced=false;
    car.gun_audio=play_wheels && pit->tire_change_active!=0;
    memset(car.gun_off_done,0,sizeof car.gun_off_done);
    memset(car.gun_on_done,0,sizeof car.gun_on_done);
    car.gun_synced=false;car.gun_sync_closed=false;
    car.gear_at=0;car.gear_done=false;car.gear_closed=car.snapshot.controller_role!=0;
    memset(car.gun_dwell,0,sizeof car.gun_dwell);
    memset(car.gun_stutter_at,0,sizeof car.gun_stutter_at);
    memset(car.gun_stutter_until,0,sizeof car.gun_stutter_until);
    /* Measured fallback schedule; replaced by the crew-synced one below as
     * soon as the gun channels for this stop are readable. */
    if(car.gun_audio)
        pit_presentation_wheel_audio(car.start,pit->tire_service_end_time,seed,vehicle,car.token,
                                     car.gun_off_at,car.gun_on_at);
    car.reveal_at=pit_presentation_failure_time(&car.plan,car.start,car.baseline,
                                                pit->fuel_start_time,seed,vehicle,car.token);
    if(car.gun_audio&&pit_presentation_is_wheel_failure(&car.plan))
        car.reveal_at=car.gun_off_at[car.plan.wheel];
    if(car.gun_audio)event(driver_utf8(car.snapshot.driver_name)+": gun SFX native tyre end "+
                           number(pit->tire_service_end_time)+"; off FL/FR/RL/RR "+
                           number(car.gun_off_at[0])+"/"+number(car.gun_off_at[1])+"/"+
                           number(car.gun_off_at[2])+"/"+number(car.gun_off_at[3])+"; on "+
                           number(car.gun_on_at[0])+"/"+number(car.gun_on_at[1])+"/"+
                           number(car.gun_on_at[2])+"/"+number(car.gun_on_at[3]));
    if(car.reveal_at<now+0.35)car.reveal_at=now+0.35;
}
static bool gun_corner_faulty(const Car &car,int corner) {
    return corner==car.plan.wheel &&
           (car.plan.outcome==PF_OUTCOME_WHEELGUN||car.plan.outcome==PF_OUTCOME_CROSS_THREAD);
}
/* Replace the measured schedule with the visible gun's contact times. Tried
 * every tick until the first fallback burst is due; any unreadable or
 * implausible corner keeps the whole stop on the measured schedule. */
static void try_sync_gun_audio(Car &car,void *vehicle) {
    if(!car.gun_audio || car.gun_synced || car.gun_sync_closed)return;
    double first_due=car.gun_off_at[0];
    for(int c=1;c<4;c++)first_due=std::min(first_due,car.gun_off_at[c]);
    if(now>=first_due) {
        car.gun_sync_closed=true;
        /* Diagnostic: the raw gun channels and which gate rejected them. */
        PitGunChannel seen[4];
        int found=pit_gun_channels_read(native_read,(uintptr_t)vehicle,seen);
        std::string detail=" found "+std::to_string(found)+"; stop start "+number(car.start);
        const char *label[4]={"FL","FR","RL","RR"};
        for(int c=0;c<4;c++) {
            double at[2];float dwell[2];
            detail+=std::string("; ")+label[c]+(seen[c].found?"":" missing")+
                    " abs "+number(seen[c].start_abs)+"-"+number(seen[c].end_abs)+
                    " base "+number(seen[c].base_start)+"-"+number(seen[c].base_end)+
                    (pit_gun_channel_plausible(seen[c],car.start)?
                        (pit_gun_contact_times(seen[c],c,car.start,at,dwell)?" ok":" contact-range"):" implausible");
        }
        if(found<4 && car.crew_read) {
            detail+="; crew names";
            for(uint32_t i=0;i<car.crew.actor_count && i<F1C_PIT_CREW_MAX_ACTORS;i++)
                if(car.crew.actors[i].corner>=0)detail+=std::string(" ")+car.crew.actors[i].name;
        }
        event(driver_utf8(car.snapshot.driver_name)+": gun SFX crew sync unavailable; measured schedule kept;"+detail);
        return;
    }
    PitGunChannel channel[4];
    if(pit_gun_channels_read(native_read,(uintptr_t)vehicle,channel)!=4)return;
    double at[4][2];float dwell[4][2];
    for(int c=0;c<4;c++)
        if(!pit_gun_channel_plausible(channel[c],car.start) ||
           !pit_gun_contact_times(channel[c],c,car.start,at[c],dwell[c]))return;
    for(int c=0;c<4;c++) {
        car.gun_off_at[c]=at[c][0];car.gun_on_at[c]=at[c][1];
        car.gun_dwell[c][0]=dwell[c][0];car.gun_dwell[c][1]=dwell[c][1];
    }
    if(car.hold_corner>=0 && car.hold_corner<4) {
        const int h=car.hold_corner;
        if(car.hold_contact==0){car.gun_dwell[h][0]+=(float)car.hold_delay;car.gun_on_at[h]+=car.hold_delay;}
        else if(car.plan.outcome==PF_OUTCOME_SLOW_CORNER)car.gun_on_at[h]+=car.hold_delay;
        else car.gun_dwell[h][1]+=(float)car.hold_delay;
    }
    car.gun_synced=true;car.gun_retry_at=0;
    if(pit_presentation_is_wheel_failure(&car.plan) && !car.mechanic_started)
        car.reveal_at=std::max(now+0.35,car.hold_corner>=0 && car.hold_corner==car.plan.wheel?
                                         car.hold_from:car.gun_off_at[car.plan.wheel]);
    event(driver_utf8(car.snapshot.driver_name)+": gun SFX synced to crew; channel start FL/FR/RL/RR "+
          number(channel[0].start_abs)+"/"+number(channel[1].start_abs)+"/"+
          number(channel[2].start_abs)+"/"+number(channel[3].start_abs)+"; off "+
          number(car.gun_off_at[0])+"/"+number(car.gun_off_at[1])+"/"+
          number(car.gun_off_at[2])+"/"+number(car.gun_off_at[3])+"; on "+
          number(car.gun_on_at[0])+"/"+number(car.gun_on_at[1])+"/"+
          number(car.gun_on_at[2])+"/"+number(car.gun_on_at[3]));
}
/* Synced playback: one clip each time the gun comes to rest at the nut,
 * sized to its stay there. A faulty gun stutters for as long as it stays. */
static void pump_synced_gun_audio(Car &car) {
    const float seconds[3]={wav_seconds(0),wav_seconds(1),wav_seconds(2)};
    for(int corner=0;corner<4;corner++) {
        for(int k=0;k<2;k++) {
            unsigned char &done=k?car.gun_on_done[corner]:car.gun_off_done[corner];
            double due=k?car.gun_on_at[corner]:car.gun_off_at[corner];
            if(done || now<due)continue;
            done=1;
            if(now-due>0.3)continue; /* never play a contact that already passed */
            bool faulty=gun_corner_faulty(car,corner);
            queue_wheel_sfx(car,corner,pit_gun_pick_wav(seconds,car.gun_dwell[corner][k],faulty));
            if(faulty && seconds[1]>0) {
                car.gun_stutter_at[corner]=now+seconds[1]+0.12;
                car.gun_stutter_until[corner]=due+car.gun_dwell[corner][k];
            }
        }
        double &stutter=car.gun_stutter_at[corner];
        if(stutter>0 && now>=stutter) {
            if(stutter+seconds[1]<=car.gun_stutter_until[corner]) {
                queue_wheel_sfx(car,corner,1);
                /* r5: during a mid-motion hold, retry bursts with pauses
                 * (crew working the nut), not a continuous loop. */
                double gap=0.12;
                if(corner==car.hold_corner && stutter>car.gun_stutter_until[corner]-car.hold_delay)
                    gap=1.4+0.2*(double)((car.token+(uint64_t)(stutter*10.0))%5);
                stutter+=seconds[1]+gap;
            } else stutter=0;
        }
    }
}
/* The lollipop board's turn to 1ST GEAR is read from its own animation
 * channel; the engineer's "Gear" follows the visible board. A busy radio gets
 * a short retry window; a late line is dropped rather than said late. */
static void pump_gear_call(Car &car,void *vehicle) {
    if(car.gear_closed || car.gear_done)return;
    if(car.gear_at<=0) {
        if(now>car.baseline+1.0) {
            car.gear_closed=true;
            event(driver_utf8(car.snapshot.driver_name)+": gear call unavailable (lollipop channel not read)");
            return;
        }
        PitGunChannel lollipop;
        if(!pit_lollipop_channel_read(native_read,(uintptr_t)vehicle,&lollipop))return;
        double at=pit_lollipop_gear_time(lollipop,car.start);
        if(at<=0)return;
        car.gear_at=at;
        event(driver_utf8(car.snapshot.driver_name)+": gear call at "+number(at)+" (lollipop start "+
              number(lollipop.start_abs)+", native end "+number(car.baseline)+")");
    }
    if(now<car.gear_at)return;
    if(now>car.gear_at+0.8){car.gear_done=true;event(driver_utf8(car.snapshot.driver_name)+": gear call dropped (radio busy)");return;}
    if(voice(car,44)!=0)car.gear_done=true;
}
static void pump_presentation(Car &car,void *vehicle,bool ready) {
    if(!car.token||!ready)return;
    pump_corner_hold(car,vehicle);
    try_sync_gun_audio(car,vehicle);
    pump_gear_call(car,vehicle);
    if(car.gun_audio && car.gun_synced)pump_synced_gun_audio(car);
    else if(car.gun_audio) {
        for(int corner=0;corner<4;corner++) {
            if(!car.gun_off_done[corner]&&now>=car.gun_off_at[corner]) {
                queue_wheel_sfx(car,corner,wheel_sfx_variant(car,corner,0));
                car.gun_off_done[corner]=1;
                if((car.plan.outcome==PF_OUTCOME_WHEELGUN||car.plan.outcome==PF_OUTCOME_CROSS_THREAD)&&
                   corner==car.plan.wheel&&car.plan.attempts>0) {
                    uint64_t key=((uint64_t)car.snapshot.handle.object_generation<<8)|car.snapshot.handle.slot;
                    car.gun_retry_at=now+2.5+pit_presentation_jitter(seed,key,car.token,
                                            UINT64_C(0x47554e5245545259),2.5);
                }
            }
            if(!car.gun_on_done[corner]&&now>=car.gun_on_at[corner]) {
                queue_wheel_sfx(car,corner,wheel_sfx_variant(car,corner,1));
                car.gun_on_done[corner]=1;
            }
        }
        if(car.gun_retry_at>0&&now>=car.gun_retry_at) {
            queue_wheel_sfx(car,car.plan.wheel,wheel_sfx_variant(car,car.plan.wheel,2+car.gun_retry_count));
            car.gun_retry_count++;
            if(car.gun_retry_count<std::min(2,std::max(1,car.plan.attempts))) {
                uint64_t key=((uint64_t)car.snapshot.handle.object_generation<<8)|car.snapshot.handle.slot;
                car.gun_retry_at=now+2.5+pit_presentation_jitter(seed,key,car.token,
                                     UINT64_C(0x47554e5245545259)+(uint64_t)car.gun_retry_count,2.5);
            } else car.gun_retry_at=0;
        }
    }
    if(!car.mechanic_started&&now>=car.reveal_at) {
        car.mechanic_started=true;
        if(car.plan.outcome==PF_OUTCOME_STALL_DELAY && start_owned_stall(car,vehicle))
            car.stall_notice_pending=true;
        if(car.terminal && car.snapshot.controller_role==0 &&
           car.plan.outcome==PF_OUTCOME_WHEELGUN && car.plan.wheel>=0 && car.plan.wheel<4) {
            const int corner[]={63,65,68,70};
            voice(car,corner[car.plan.wheel]);
            car.wheelgun_generic_at=now+3.0;
        }
        if(car.terminal&&car.snapshot.controller_role==1) {
            voice(car,186+car.plan.wheel);
            event(driver_utf8(car.snapshot.driver_name)+": wheel jammed; diagnosis before retirement");
        }
    }
    try_stall_notice(car);
    if(car.wheelgun_generic_at>0 && !car.wheelgun_generic_announced &&
       now>=car.wheelgun_generic_at) {
        voice(car,61);car.wheelgun_generic_announced=true;
        car.wheelgun_generic_at=0;
    }
    if(car.stalled && !car.player_restart_pending) {
        if(car.player_ignition_owned)*player_ignition_flag()=0;
        else f1c_ai_engine_stop(native_read,(uintptr_t)vehicle,GetCurrentThreadId(),1);
    }
}
static void queue_feedback(Car &car,const pf_command_t &command) {
    /* The physical fuel effect owns its recovery cue. Policy completion is a
     * timer and can fall after this service, then leak into the next stop. */
    if(car.fuel_recovery_owned && command.kind==PF_CMD_RECOVERED)return;
    DeferredFeedback item;item.command=command;
    /* r6: with a crew hold, "fixed" is said when the held gun is freed and
     * attempts fall inside the hold, before the lollipop turns. */
    /* r7: "fixed" only when the failing corner's work is done (last tightening ends). */
    const double story_end=car.work_done_at>car.reveal_at+0.6?car.work_done_at+1.0:car.target;
    item.at=pit_presentation_command_time(&car.plan,car.reveal_at,story_end,command.kind,command.attempt);
    car.deferred.push_back(item);
}
static void flush_feedback(Car &car) {
    if(car.terminal||!car.ready||car.snapshot.pit_status!=3)return;
    for(size_t i=0;i<car.deferred.size();) {
        if(now+0.001<car.deferred[i].at){i++;continue;}
        feedback(car,car.deferred[i].command);
        car.deferred.erase(car.deferred.begin()+i);
    }
}
static std::string plan_json(const Car &c) {
    if (!c.token) return "null";
    std::string out="{\"outcome\":"+quoted(pf_outcome_name(c.plan.outcome))+",\"wheel\":"+quoted(pf_wheel_name(c.plan.wheel))+
        ",\"fuel_subtype\":"+quoted(pf_fuel_subtype_name(c.plan.fuel_subtype))+",\"terminal\":"+(c.terminal?"true":"false")+
        ",\"normal_seconds\":"+number(c.baseline-c.start)+",\"stop_seconds\":"+number(c.target-c.start)+
        ",\"added_seconds\":"+number(c.target-c.baseline);
    if(policy_model==3) {
        out+=",\"parent\":"+quoted(pf3_parent_name(c.plan3.parent))+
            ",\"resolution\":"+quoted(pf3_resolution_name(c.plan3.parent,c.plan3.resolution))+
            ",\"v3_status\":"+quoted(pf3_plan_status_name(c.plan3.status))+
            ",\"fire_selected\":"+(c.plan3.fire_selected?"true":"false")+
            ",\"fire_supported\":"+(c.plan3.fire_supported?"true":"false")+
            ",\"fire_suppressed\":"+(c.plan3.fire_suppressed?"true":"false");
    }
    return out+"}";
}
static void __cdecl update(void *) {
    EnterCriticalSection(&lock);
    F1CBrokerStats stats{}; stats.structure_size=sizeof stats;
    F1CBrokerSessionSnapshot session{}; session.structure_size=sizeof session;
    F1CBrokerVehicleHandle handles[104]; uint32_t count=0;
    if (WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0 || broker.get_stats(&stats)!=F1C_BROKER_OK || !stats.session_generation ||
        broker.get_session_snapshot(client,stats.session_generation,&session)!=F1C_BROKER_OK ||
        !(session.valid_fields & F1C_SESSION_SNAPSHOT_CORE) || !session.session_active) {
        active=false; for(auto &c:cars) cancel(c,"Session unavailable");
        LeaveCriticalSection(&lock); InterlockedExchange(&pending,0); return;
    }
    double clock=*(volatile double *)G_SessionClockSeconds;
    if (epoch!=session.session_generation) {
        for(auto &c:cars) { cancel(c,"Session changed"); c=Car{}; }
        epoch=session.session_generation;
        if(policy_model==3)pf3_session_new_epoch(policy3,epoch);else pf_session_new_epoch(policy,epoch);
        last_time=-1; invalid_clock=false; error.clear();
        event("Live session attached; existing ready services are skipped");
        event("Game x87 control word: "+std::to_string(pit_fp_control()));
    }
    if (!std::isfinite(clock) || clock<last_time) {
        invalid_clock=true; for(auto &c:cars) cancel(c,"Simulation clock invalidated");
        error="Simulation clock discontinuity; wait for a new session";
    }
    if(invalid_clock) { active=false; LeaveCriticalSection(&lock); InterlockedExchange(&pending,0); return; }
    now=clock; last_time=clock;race_phase=session.race_phase;
    audio_listener_slot=(session.spectated_vehicle_slot>=0 && session.spectated_vehicle_slot<104)?
                        session.spectated_vehicle_slot:-1;
    if(broker.get_vehicle_handles(client,epoch,handles,104,&count)!=F1C_BROKER_OK || count>104) {
        active=false; LeaveCriticalSection(&lock); InterlockedExchange(&pending,0); return;
    }
    F1CBrokerVehicleSnapshot batch[104]{};
    bool valid=true;
    for(unsigned i=0;i<count;i++) {
        batch[i].structure_size=sizeof batch[i];
        if(handles[i].slot>=104 || broker.get_vehicle_snapshot(client,handles[i],&batch[i])!=F1C_BROKER_OK ||
            batch[i].snapshot_sequence!=session.snapshot_sequence ||
            memcmp(&batch[i].handle,&handles[i],sizeof handles[i])!=0) { valid=false; break; }
    }
    if(!valid) { LeaveCriticalSection(&lock); InterlockedExchange(&pending,0); return; }
    bool present[104]{};
    for(unsigned i=0;i<count;i++) present[handles[i].slot]=true;
    for(unsigned i=0;i<104;i++) if(!present[i]) { cancel(cars[i],"Driver disappeared"); cars[i].seen=false; }
    for(unsigned i=0;i<count;i++) {
        const auto &s=batch[i];
        if(s.handle.slot>=104) continue;
        Car &c=cars[s.handle.slot];
        uint64_t required=F1C_VEH_SNAPSHOT_PIT|F1C_VEH_SNAPSHOT_FIELD_STATUS|F1C_VEH_SNAPSHOT_NAMES;
        if((s.valid_fields&required)==required && s.retired && c.seen && c.retirement_requested &&
           !memcmp(&s.handle,&c.snapshot.handle,sizeof s.handle)) {
            c.snapshot=s;c.holding=false;c.stalled=false;
            auto life=f1c_vehicle_lifecycle_read(native_read,(uintptr_t)f1c_vehicle_slot((int)s.handle.slot));
            observe_retirement(c,life);
            continue;
        }
        /* 1.1.4 (2026-10-11): in a synced CO-OP session another PC's car (role 2)
         * stays in the loop so the plan received from its driving PC is shown here
         * (1.1.2 dropped it before the plan code ran - host log: plan received,
         * never shown). */
        const bool coop_peer_car=s.controller_role==2 && policy_model==3 && coop_pit_active();
        if((s.valid_fields&required)!=required || s.retired ||
           (s.controller_role!=0 && s.controller_role!=1 && !coop_peer_car)) {
            cancel(c,"Driver unavailable"); c.seen=false; continue;
        }
        void *v=f1c_vehicle_slot((int)s.handle.slot);
        if(!v || !f1c_vehicle_slot_live((int)s.handle.slot)) { cancel(c,"Vehicle not live"); c.seen=false; continue; }
        if(is_safety_car(v)) {
            if(!c.safety_car_logged)event(driver_utf8(s.driver_name)+": r15 Safety Car (Classes Safety+NotAI); not a participant, skipped");
            cancel(c,"Safety Car"); c.seen=false; c.safety_car_logged=true; continue;
        }
        F1CPitMachine *pit=f1c_pit_machine(v);
        bool ready=f1c_pit_service_active(v)!=0;
        bool fresh=!c.seen || c.snapshot.handle.object_generation!=s.handle.object_generation;
        if(fresh) {
            /* Same object generation after a transient disappearance keeps the
             * service sequence; a genuinely replaced object or new epoch
             * resets it. */
            uint64_t keep=(!c.seen && c.snapshot.handle.object_generation==s.handle.object_generation)?c.sequence:0;
            cancel(c,"Driver replaced"); c=Car{}; c.sequence=keep; c.ready=ready;
        }
        int previous_pit=c.snapshot.pit_status;
        bool previous_call=c.snapshot.call_to_box!=0;
        c.seen=true; c.snapshot=s;
        c.rpm=VEHF_EngineRPM(v);
        c.snapshot.driver_name[63]=0;
        auto current_life=f1c_vehicle_lifecycle_read(native_read,(uintptr_t)v);
        bool retiring=c.terminal || c.retirement_requested || s.retired ||
            (current_life.valid && f1c_lifecycle_terminal(current_life.retired,current_life.finish)) ||
            (s.controller_role==0 && player_out_now());
        int edge_voice=!fresh?pit_voice_edge(previous_pit,s.pit_status,s.controller_role==0,retiring):-1;
        bool current_entry=VEHF_PitEntryLatch(v)!=0;
        bool call_active=pit_voice_call_active(s.pit_status,s.call_to_box!=0,
            previous_call,VEHF_CallToBox(v)!=0,!c.arm.empty());
        if(s.controller_role==0) {
            const float position[]={VEHF_WorldPositionX(v),VEHF_WorldPositionY(v),VEHF_WorldPositionZ(v)};
            c.xpitin=pit_live_xpitin_read((uintptr_t)G_StewardObject,native_read,position);
            c.pit_entry_call_active=call_active;
            /* The player's own box from the game's pit-slot table. Trusted only
             * when authored (non-zero) and near the pit-entry marker when that
             * is known; the first service start then checks it for real. */
            float box[3];
            int team=VEHF_PitTeamIndex(v);
            bool box_ok=box_call_enabled && f1c_pit_box_position(native_read,team,box);
            if(box_ok && c.xpitin.marker_valid) {
                double mx=(double)box[0]-c.xpitin.marker[0],mz=(double)box[2]-c.xpitin.marker[2];
                box_ok=std::sqrt(mx*mx+mz*mz)<=1000.0;
            }
            if(box_ok) {
                double bx=(double)position[0]-box[0],by=(double)position[1]-box[1],bz=(double)position[2]-box[2];
                double horizontal=std::sqrt(bx*bx+bz*bz);
                if(!fresh && previous_pit!=F1C_PIT_SERVICE && s.pit_status==F1C_PIT_SERVICE) {
                    event(driver_utf8(s.driver_name)+": box check: serviced "+number(horizontal)+
                          " m from pit-slot "+std::to_string(team));
                    if(horizontal>6.0) {
                        box_call_enabled=0;
                        event("Box call disabled for this session: pit-slot position does not match the service position");
                    }
                }
                if(!fresh && !c.box_called && s.pit_status==F1C_PIT_LANE &&
                   horizontal<=10.0 && std::fabs(by)<=5.0 && now>=c.box_retry_at) {
                    int accepted=voice(c,46);
                    if(accepted!=0)c.box_called=true;
                    else c.box_retry_at=now+0.5;
                }
            }
        }
        /* "Watch for the pit lane entrance" only when the car actually reaches
         * XPITIN: the game's own entry latch, or the marker within 10 m, or
         * the marker's closest pass when it sits off the car's line. */
        bool reached_xpitin=false;
        if(s.controller_role==0 && c.xpitin.marker_valid && std::fabs(c.xpitin.height_delta_m)<=20.0) {
            double d=c.xpitin.horizontal_m;
            if(d<=10.0 || (c.xpitin_closest_m<30.0 && d>c.xpitin_closest_m+2.0))reached_xpitin=true;
            if(d<c.xpitin_closest_m)c.xpitin_closest_m=d;
        }
        /* Pit intent before the lane line, from geometry: near XPITIN the car
         * is clearly closer to the pit-road line (AIW branch 1) than to the
         * racing line (branch 0) - the point where the AI swerve off. */
        bool on_pit_road=false;
        if(s.controller_role==0 && c.xpitin.marker_valid) {
            double dmain=0,dpit=0;
            if(pit_road_distances(c.xpitin.marker,c.xpitin.player,&dmain,&dpit)) {
                /* On the pit-road line itself, well clear of the racing line:
                 * r9 lap 1 passed 8.9 m/10.5 m (near miss); real entry 1.1 m/12.1 m. */
                on_pit_road=c.xpitin.horizontal_m<=60.0 && dpit<=4.0 && dpit+3.0<dmain;
                if(on_pit_road && !c.pit_road_logged) {
                    c.pit_road_logged=true;
                    event(driver_utf8(s.driver_name)+": on pit road "+number(c.xpitin.horizontal_m)+
                          " m from XPITIN (main "+number(dmain)+" m, pit "+number(dpit)+" m)");
                }
            }
        }
        /* r10: an ESC/garage teleport sets the game's entry latch; ignore
         * pit-entry edges for 2 s after a position jump > 25 m in one tick. */
        {
            /* rc.2: live position, same source as the pit-road geometry; the
             * snapshot lags one tick and let EID 50 queue before the guard. */
            const float live_x=VEHF_WorldPositionX(v),live_z=VEHF_WorldPositionZ(v);
            const float dx=live_x-c.last_x,dz=live_z-c.last_z;
            if(s.controller_role==0 && c.pos_valid && dx*dx+dz*dz>25.0f*25.0f) {
                c.teleport_until=now+2.0;c.pit_entry_triggered=true;
                event(driver_utf8(s.driver_name)+": r10 teleport detected; pit-entry call suppressed");
            }
            c.last_x=live_x;c.last_z=live_z;c.pos_valid=true;
        }
        /* r12: rejoin call when a serviced car crosses the pit exit. */
        if(s.pit_status>=F1C_PIT_SERVICE)c.serviced_this_visit=true;
        if(c.last_pit_status>0 && s.pit_status==0 && c.serviced_this_visit && !c.terminal && !s.retired) {
            voice_rejoin(c,s.race_position);
        }
        if(s.pit_status==0)c.serviced_this_visit=false;
        c.last_pit_status=s.pit_status;
        bool approach_edge=!fresh && !c.pit_entry_triggered && now>=c.teleport_until &&
           s.pit_status<F1C_PIT_LANE && s.controller_role==0 &&
           (on_pit_road || (!c.pit_entry_latch && current_entry));  /* or the game's own entry latch */
        if(approach_edge) {
            c.pit_entry_triggered=true;
            /* Next tick: a same-tick LIMITER ON is queued first. */
            c.pit_entry_pending=true;c.pit_entry_retry_at=now+1.0e-6;c.pit_entry_until=now+3.0;
            c.pit_entry_ack_at=now;
            c.pit_entry_ack_distance_m=c.xpitin.marker_valid?c.xpitin.distance_m:-1;
            c.pit_entry_voice_attempt_at=-1;c.pit_entry_voice_queued_at=-1;
            c.pit_entry_voice_result=0;
            c.pit_entry_ack_source=reached_xpitin?"xpitin_pit_road":"pit_road";
            event(driver_utf8(s.driver_name)+": XPITIN vicinity acknowledged by "+
                  c.pit_entry_ack_source+" at "+
                  (c.xpitin.marker_valid?number(c.xpitin.distance_m)+" m":"unavailable distance")+
                  "; active pit call; EID 50 armed");
        }
        if(c.pit_entry_pending) {
            if(s.pit_status>2 || now>c.pit_entry_until) c.pit_entry_pending=false;
            else if(now>=c.pit_entry_retry_at) {
                int accepted=voice(c,50);
                c.pit_entry_voice_attempt_at=now;c.pit_entry_voice_result=accepted;
                if(accepted==1) {
                    c.pit_entry_announced=true;c.pit_entry_pending=false;c.pit_entry_voice_queued_at=now;
                    event(driver_utf8(s.driver_name)+": XPITIN EID 50 accepted by native voice queue");
                }
                else if(accepted<0)c.pit_entry_pending=false;
                else c.pit_entry_retry_at=now+0.50;
            }
        }
        /* LIMITER ON always plays at the pit-lane line. */
        {
            int accepted=voice(c,edge_voice);
            /* BRAKE must not be lost behind a just-played "Box!" call. */
            if(edge_voice==53 && accepted==0){c.brake_until=now+1.5;c.brake_retry_at=now+0.25;}
        }
        if(c.brake_until>0 && now>=c.brake_retry_at) {
            if(now>c.brake_until || s.pit_status!=F1C_PIT_SERVICE)c.brake_until=0;
            else if(voice(c,53)!=0)c.brake_until=0;
            else c.brake_retry_at=now+0.25;
        }
        c.pit_entry_latch=current_entry;
        /* r8: keep the pit-entry latch while the car is still on the pit-road
         * line or near XPITIN; clearing it there re-armed EID 50 every tick. */
        const bool entry_zone=on_pit_road || (c.xpitin.marker_valid && c.xpitin.horizontal_m<=80.0);
        if(s.pit_status==0 && !s.call_to_box && !VEHF_CallToBox(v) && c.arm.empty() && !entry_zone)
            {c.pit_entry_announced=false;c.pit_entry_triggered=false;c.pit_entry_pending=false;
             c.xpitin_closest_m=1.0e9;c.pit_road_logged=false;
             c.box_called=false;c.box_retry_at=0;}
        if(!c.arm.empty()) {
            Request &r=requests.at(c.arm);
            if((LONG)(GetTickCount()-r.expires)>=0) {r.state="expired";r.message="One-stop request expired";c.arm.clear();}
            else if(r.state=="queued") {
                if(s.pit_status>=3 || pit->status>=3) {r.state="rejected";r.message="Service already started";c.arm.clear();}
                else {
                    /* Reserve already exists; all identity checks happened on
                     * this same game-thread batch before semantic submission. */
                    /* The old broker's queued setter carries only slot/session,
                     * not object generation. Apply that same reviewed SDK
                     * command input here after full-handle validation instead. */
                    if(session.race_phase==5) {
                        VEHF_CallToBox(v)=1;
                        r.state="armed";r.submitted=now;r.message="Call-to-box input accepted on game thread; next eligible stop armed";
                        event(std::string(s.driver_name)+": call-to-box input set");
                    } else {r.state="rejected";r.message="Call-to-box requires active racing phase";c.arm.clear();}
                }
            } else if(r.state=="accepted") {
                if(s.call_to_box || s.pit_status>0) {r.state="armed";r.message="Game acknowledged pit-in; next eligible stop armed";}
                else if(now-r.submitted>2) {r.state="rejected";r.message="Game did not acknowledge pit-in";c.arm.clear();}
            }
        }
        const bool coop=policy_model==3 && coop_pit_active();
        const bool coop_remote=coop && s.controller_role==2;
        if(coop) coop_pit_receive(pf_identity_t{epoch,0,0});
        bool coop_have=false; pf3_plan_t coop_plan{};
        if(coop_remote && ready && (!c.ready || c.coop_wait_since) && !fresh && (!c.token || pit->stop_start_time!=c.start)) {
            auto it=coop_pit_plans.find(s.driver_name);
            if(it!=coop_pit_plans.end() && GetTickCount()-it->second.received<30000) {
                coop_have=true;coop_plan=it->second.plan;coop_pit_plans.erase(it);
            } else {
                if(!c.coop_wait_since)c.coop_wait_since=GetTickCount()|1;
                if(GetTickCount()-c.coop_wait_since<2000){c.ready=false;continue;}
                pf3_plan_build_full(nullptr,nullptr,nullptr,0,nullptr,&coop_plan);
                coop_have=true;
                event(driver_utf8(s.driver_name)+": CO-OP no plan from the driving PC; clean stop shown");
            }
            c.coop_wait_since=0;
        }
        if(ready && !c.ready && !fresh && (!c.token || pit->stop_start_time!=c.start)) {
            pf_job_t jobs[PF_JOB_MAX]; size_t n=0; pf_context_t context{};
            pit_live_service_result_t service_result=PIT_LIVE_SERVICE_INVALID;
            c.holding=false; c.terminal=false;
            if(pit_live_jobs(pit->stop_start_time,pit->stop_end_time,pit->tire_change_active,pit->tire_service_end_time,
                pit->refuel_active,pit->fuel_start_time,pit->fuel_target-pit->fuel_start,
                pit->wing_adjustment_active,pit->repair_active,jobs,&n,&context.eligibility,&service_result)) {
                c.token=++c.sequence;
                pf_identity_t identity{epoch,((uint64_t)s.handle.object_generation<<8)|s.handle.slot,c.token};
                bool force=!c.arm.empty() && requests.at(c.arm).state=="armed";
                int committed=0;
                int selector=force?requests.at(c.arm).selector:0;
                pf_plan_t committed_plan{};
                pf3_plan_t committed_plan3{};
                pf3_forcing_t forcing=pf3_forcing_natural();
                if(force && policy_model==3 && requests.at(c.arm).failure!="random") {
                    forcing.parent=requests.at(c.arm).parent;
                    if(requests.at(c.arm).res_index>=0)forcing.resolution=requests.at(c.arm).res_index;
                    /* Hypothetical capability only; native fire stays unadvertised. */
                    forcing.fire_supported=0;
                }
                int commit_rc=coop_have
                    ? pit_policy_math([&]{return pf3_session_commit_given(policy3,&identity,&context,&context.eligibility,now,&coop_plan,&committed_plan3,&committed);})
                    : session_commit(selector,&forcing,identity,context,now,&committed_plan,&committed_plan3,&committed);
                if(coop_have && commit_rc==0 && committed)committed_plan=plan_view_v3(committed_plan3);
                if(commit_rc==0 && committed && coop && !coop_remote)coop_pit_send(s.driver_name,committed_plan3);
                if(commit_rc==0 && committed) {
                    c.plan=committed_plan;
                    if(policy_model==3)c.plan3=committed_plan3;
                    c.fuel=FuelEffect{};
                    if(c.plan.outcome==PF_OUTCOME_FUEL_RIG && context.eligibility.fuel_allowed)
                        c.fuel.variant=(int)c.plan.fuel_subtype;
                    c.fuel_recovery_owned=c.fuel.variant!=0;
                    c.fuel_recovery_announced=false;
                    c.fuel_requested_target=0;
                    c.fuel_clear_announced=false;
                    session_bind_timing(identity,jobs,n);
                    c.start=pit->stop_start_time; c.baseline=pit->stop_end_time; c.target=c.baseline; c.applied=c.baseline;
                    c.crew=F1CPitCrewSnapshot{};c.crew_read=false;c.crew_anchor=0;
                    if((s.controller_role==0 || c.plan.outcome!=PF_OUTCOME_CLEAN) &&
                       pit->tire_change_active && std::isfinite(pit->tire_service_start_time) &&
                       pit->tire_service_start_time>=c.start &&
                       pit->tire_service_start_time<=c.baseline) {
                        c.crew_anchor=pit->tire_service_start_time;
                        c.crew_read=f1c_pit_crew_snapshot(native_read,(uintptr_t)v,&c.crew)!=0;
                        event(driver_utf8(s.driver_name)+": crew windows "+
                              std::to_string(c.crew.window_count)+"/"+
                              std::to_string(c.crew.actor_count)+"; tyre anchor "+
                              number(c.crew_anchor)+(c.crew_read?" [read]":" [unavailable]"));
                    }
                    c.terminal=c.plan.terminal!=0;
                    pf_timing_result_t timing{};
                    if(c.plan.severity==PF_RECOVERABLE && session_apply_timing(c,jobs,n,&timing) && timing.known)
                        c.target=c.start+timing.fault_end;
                    if(c.target<c.baseline) c.target=c.baseline;
                    /* Slow wheel is a general stop delay. A longer fuel job
                     * must not absorb the selected wheel-change delay. */
                    if(c.plan.outcome==PF_OUTCOME_SLOW_CORNER &&
                       c.plan.severity==PF_RECOVERABLE &&
                       std::isfinite(c.plan.extra_job_seconds) &&
                       c.plan.extra_job_seconds>0)
                        c.target=std::max(c.target,c.baseline+c.plan.extra_job_seconds);
                    c.holding=!c.terminal && c.target>c.baseline && c.target<=c.start+120;
                    if(c.holding && pit_presentation_is_wheel_failure(&c.plan))
                        {c.hold_corner=-1;c.release_shift=0;c.hold_end=0;c.work_done_at=0;c.start=pit->stop_start_time;
                         shift_crew_for_fault(c,v,now-pit->stop_start_time,c.target-c.baseline);
                         if(c.release_shift>c.target-c.baseline)c.target=c.baseline+c.release_shift;}
                    /* Native tyre work has wheel audio even on a clean AI stop. */
                    schedule_presentation(c,pit,true);
                    /* r10: PitStopProgress 0x4A68B0 keeps the car raised until
                     * tire_service_end_time (0x1D4); move it with the jacks. */
                    if(c.release_shift>0.05 && pit->tire_change_active && std::isfinite(pit->tire_service_end_time) &&
                       pit->tire_service_end_time>now) {
                        const float before=pit->tire_service_end_time;
                        pit->tire_service_end_time=before+(float)c.release_shift;
                        event(std::string(s.driver_name)+": r10 car stays raised; tyre service end "+number(before)+" -> "+number(pit->tire_service_end_time));
                    }
                    if(c.terminal) {
                        c.terminal_at=c.reveal_at+c.plan.terminal_presentation_seconds;
                        c.target=std::max(c.baseline,c.terminal_at+0.5);c.holding=true;
                    }
                    event(std::string(s.driver_name)+": "+(policy_model==3?std::string(pf3_parent_name(c.plan3.parent))+"/"+pf3_resolution_name(c.plan3.parent,c.plan3.resolution):std::string(pf_outcome_name(c.plan.outcome)))+"; added "+number(c.target-c.baseline)+" seconds");
                    if(force) { auto &r=requests.at(c.arm); r.state="consumed";
                        r.message=policy_model==3?std::string("Live policy selected ")+pf3_parent_name(c.plan3.parent)+"/"+pf3_resolution_name(c.plan3.parent,c.plan3.resolution):std::string("Live policy selected ")+pf_outcome_name(c.plan.outcome);
                        c.arm.clear(); }
                } else if(force) {
                    auto &r=requests.at(c.arm); r.state="rejected";
                    /* An ineligible forced fault does not cancel native tyre
                     * work. Keep its ordinary wheel audio on the measured
                     * tyre clock even though no failure plan was committed. */
                    c.plan=pf_plan_t{};c.plan.outcome=PF_OUTCOME_CLEAN;c.plan.wheel=-1;
                    c.plan3=pf3_plan_t{};c.plan3.parent=-1;c.plan3.resolution=-1;
                    c.start=pit->stop_start_time;c.baseline=pit->stop_end_time;
                    c.target=c.baseline;c.applied=c.baseline;
                    schedule_presentation(c,pit,true);
                    if(policy_model==3) {
                        r.message=(r.parent==PF3_PARENT_FUEL_RIG && !context.eligibility.fuel_allowed)?"This stop has no refuelling. Select a wheel fault or schedule fuel in the game.":
                            ((r.parent==PF3_PARENT_SLOW_WHEEL||r.parent==PF3_PARENT_WHEELGUN||r.parent==PF3_PARENT_CROSS_THREAD)&&!context.eligibility.tyre_work)?"This stop has no tyre change. Schedule tyres in the game or select another fault.":
                            "Policy refused this stop (code "+std::to_string((int)session_last_error())+").";
                    } else {
                        r.message=(selector==4 && !context.eligibility.fuel_allowed)?"This stop has no refuelling. Select a wheel fault or schedule fuel in the game.":
                            ((selector==2||selector==3||selector==6)&&!context.eligibility.tyre_work)?"This stop has no tyre change. Schedule tyres in the game or select another fault.":
                            "Policy refused this stop (code "+std::to_string((int)session_last_error())+").";
                    }
                    event(driver_utf8(s.driver_name)+": "+r.message+
                          " [tyre="+std::to_string(context.eligibility.tyre_work)+
                          " fuel="+std::to_string(context.eligibility.fuel_allowed)+"]");
                    c.arm.clear();
                }
            } else if(service_result==PIT_LIVE_SERVICE_SKIP_NO_RECOGNISED_WORK ||
                      service_result==PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY) {
                /* This is an auditable refusal, not a clean incident. In
                 * particular, repair-only stops cannot reach the generic
                 * release/stall parent. */
                event(driver_utf8(s.driver_name)+": Pit Stop Failures skipped "+
                      pit_live_service_result_name(service_result));
                if(!c.arm.empty()) {
                    auto &r=requests.at(c.arm);r.state="rejected";
                    r.message=service_result==PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY?
                        "Repair-only stop: no pit-failure category applies":
                        "No recognised service work: no pit failure applies";
                    c.arm.clear();
                }
            } else if(!c.arm.empty()) {
                auto &r=requests.at(c.arm);r.state="rejected";r.message="Native job timing could not be established; no failure injected";c.arm.clear();
                event(std::string(s.driver_name)+": service baseline unavailable; refused");
                event("Native timing start="+number(pit->stop_start_time)+" end="+number(pit->stop_end_time)+
                      " tyre="+std::to_string(pit->tire_change_active)+" tyre_end="+number(pit->tire_service_end_time)+
                      " fuel="+std::to_string(pit->refuel_active)+" fuel_start="+number(pit->fuel_start_time)+" quantity="+number(pit->fuel_target-pit->fuel_start));
            }
        }
        (void)probe_crew;
        pump_presentation(c,v,ready);
        if(c.stalled && !c.terminal && !c.retirement_requested &&
           (!ready || c.player_restart_pending)) {
            c.player_restart_feedback_pending=c.snapshot.controller_role==0;
            restart_owned(c);
        }
        if(c.snapshot.controller_role==0 && c.player_restart_confirmed &&
           c.player_restart_feedback_pending && !c.player_restart_announced) {
            pf_command_t recovered{};recovered.kind=PF_CMD_RECOVERED;
            recovered.attempt=c.plan.attempts;
            feedback(c,recovered);
        }
        if(c.terminal && s.controller_role==1 && c.holding && now>=c.terminal_at) {
            c.retirement_requested=f1c_ai_pit_retire(native_read,(uintptr_t)v,GetCurrentThreadId())!=0;
            c.holding=false;
            event(driver_utf8(s.driver_name)+(c.retirement_requested?": native pit retirement requested":": native pit retirement refused"));
            if(c.retirement_requested){c.phase="Retirement: waiting for garage confirmation";c.ready=false;continue;}
        }
        /* 1.1.4: a terminal stop on another PC's car - that PC retires it; this
         * PC marks its copy retired (FinishState 2, only from 0) so the order
         * and timing screens agree. Same rule as Mechanical Failures CO-OP. */
        if(c.terminal && s.controller_role==2 && !c.retirement_requested && now>=c.terminal_at) {
            volatile int32_t *finish=(volatile int32_t*)((uintptr_t)v+0x4064u);
            int32_t before=*finish;
            if(before==0)*finish=2;
            c.retirement_requested=true;c.holding=false;c.ready=false;
            c.phase="Retired on its driving PC";
            event(driver_utf8(s.driver_name)+": CO-OP terminal stop - copy marked retired (FinishState "+
                  std::to_string(before)+" -> "+std::to_string(*finish)+")");
            continue;
        }
        if(c.terminal && s.controller_role==0 && !c.retirement_requested &&
           (c.holding || c.player_retire_voice_queued) && now>=c.terminal_at) {
            F1CBrokerResult armed=broker.pause_menu_arm?broker.pause_menu_arm(client,1u):F1C_BROKER_NOT_READY;
            F1CBrokerResult queued=armed==F1C_BROKER_OK&&broker.pause_menu_command?
                broker.pause_menu_command(client,F1C_BROKER_MENU_RETIRE_PITS):F1C_BROKER_NOT_READY;
            if(queued==F1C_BROKER_OK) {
                if(!c.player_retire_voice_queued)voice(c,117); /* PWS_RETIRE02 */
                c.retirement_requested=true;c.holding=false;c.phase="Retirement: garage handoff queued";
                player_retire_generation=epoch;InterlockedExchange(&player_retire_pending,1);
                event(driver_utf8(s.driver_name)+": player retire-to-garage handoff queued");
                c.ready=false;continue;
            }
            if(armed==F1C_BROKER_OK&&broker.pause_menu_arm)broker.pause_menu_arm(client,0u);
            f1c_pit_hold_after_service(v,(float)now,0.5f);c.applied=pit->stop_end_time;
            c.phase="Retirement: waiting for broker handoff";continue;
        }
        if(c.fuel.variant && ready && !c.terminal) {
            FuelEffectResult result=fuel_effect_step(c.fuel,*pit,VEHF_FuelLevel(v),now,
                                                     c.plan.extra_job_seconds);
            if(result.invalid) {
                event(driver_utf8(s.driver_name)+": fuel effect refused invalid service state");
                c.fuel=FuelEffect{};
            } else {
                if(c.fuel.captured && c.fuel_requested_target<=0)
                    c.fuel_requested_target=c.fuel.original_target;
                if(result.spill_now) {
                    int written=s.controller_role==0?pit_hud_post_white("FUEL SPILL - SHORT FILL"):0;
                    event(driver_utf8(s.driver_name)+": fuel spill; target "+
                          number(c.fuel.original_target)+" -> "+number(c.fuel.spill_target)+
                          (written?" [HUD written]":""));
                }
                if(result.recovered_now) {
                    event(driver_utf8(s.driver_name)+": fuel flow resumed after physical fault");
                    fuel_recovered_feedback(c);
                }
                if(pit_voice_fuel_clear(s.controller_role==0,c.fuel_recovery_owned,
                       c.fuel.variant==4,ready,c.fuel_clear_announced,
                       VEHF_FuelLevel(v),c.fuel_requested_target)==57) {
                    voice(c,57);c.fuel_clear_announced=true;
                }
                if(result.hold) {
                    if(pit->stop_end_time<(float)now+0.75f)
                        f1c_pit_hold_after_service(v,(float)now,0.75f);
                    c.applied=pit->stop_end_time;c.holding=true;c.ready=ready;
                    c.phase="Fuel service: physical fault active";
                    continue;
                }
                c.fuel=FuelEffect{};
            }
        }
        if(c.holding) {
            if(!ready) {c.holding=false;event(std::string(s.driver_name)+": game released service");}
            else if(!std::isfinite(pit->stop_end_time) || pit->stop_end_time>c.applied+0.002) {
                c.holding=false;event(std::string(s.driver_name)+": native/other holder superseded delay");
            } else {
                double value=pit_live_hold_value(now,c.baseline,c.target,pit->stop_end_time);
                if(value>pit->stop_end_time && value-now>=0.1 && now<c.target)
                    f1c_pit_hold_after_service(v,(float)now,(float)(value-now));
                c.applied=pit->stop_end_time;
                if(now>=c.target) { c.holding=false;event(std::string(s.driver_name)+": delay completed; awaiting native release"); }
            }
        }
        if(!ready && c.ready)c.deferred.clear();
        c.ready=ready;
        c.phase=c.holding?"Failure delay":s.pit_status==3?"In service":s.pit_status==2?"Pit lane":s.pit_status==1?"Pit requested":s.pit_status==4?"Pit exit":"On track";
    }
    pf_command_t commands[128]; int command_count=session_advance(now,commands,128);
    if(command_count<0){error="Policy event clock invalid; start a new session";event(error);invalid_clock=true;for(auto &c:cars)cancel(c,"Policy invalidated");active=false;LeaveCriticalSection(&lock);InterlockedExchange(&pending,0);return;}
    for(int i=0;i<command_count;i++) {
        unsigned slot=(unsigned)(commands[i].identity_vehicle&255);
        if(slot<104 && cars[slot].seen && commands[i].identity_token==cars[slot].token &&
           commands[i].identity_vehicle==(((uint64_t)cars[slot].snapshot.handle.object_generation<<8)|slot)) {
            event(driver_utf8(cars[slot].snapshot.driver_name)+": "+pf_command_kind_name(commands[i].kind));
            Car &c=cars[slot];
            if(c.ready && c.snapshot.pit_status==3 && !c.terminal)queue_feedback(c,commands[i]);
        }
    }
    for(auto &car:cars)if(car.seen)flush_feedback(car);
    active=true; observed=GetTickCount();
    LeaveCriticalSection(&lock); InterlockedExchange(&pending,0);
}

static std::string crew_json(const Car &c,bool include_actors) {
    if(!c.token || !c.crew_anchor) return "null";
    double elapsed=now-c.crew_anchor;
    std::string out="{\"calibration\":\"raw_channel_vs_tyre_elapsed_unverified\",\"read\":"+
        std::string(c.crew_read?"true":"false")+
        ",\"anchor\":"+number(c.crew_anchor)+",\"elapsed\":"+number(elapsed)+
        ",\"actor_count\":"+std::to_string(c.crew.actor_count)+
        ",\"readable_count\":"+std::to_string(c.crew.readable_count)+
        ",\"window_count\":"+std::to_string(c.crew.window_count);
    if(include_actors) {
        out+=",\"actors\":[";
        bool comma=false;
        for(uint32_t i=0;i<c.crew.actor_count;i++) {
            const F1CPitCrewActorWindow &a=c.crew.actors[i];
            if(!a.name[0])continue;
            if(comma)out+=',';
            comma=true;
            const char *relation=!a.window_valid?"unavailable":
                elapsed<a.start?"before":elapsed>=a.end?"after":"within";
            out+="{\"name\":"+quoted(a.name)+",\"corner\":"+std::to_string((int)a.corner)+
                 ",\"channels\":"+std::to_string((int)a.channel_count)+
                 ",\"window_valid\":"+(a.window_valid?"true":"false")+
                 ",\"start\":"+number(a.start)+",\"end\":"+number(a.end)+
                 ",\"range_relation\":"+quoted(relation)+"}";
        }
        out+="]";
    }
    return out+"}";
}
static float wheel_audio_gain_for(const Car &source,double *distance_m) {
    if(distance_m)*distance_m=-1;
    bool human=source.snapshot.controller_role==0;
    if(audio_listener_slot==(int)source.snapshot.handle.slot) {
        if(distance_m)*distance_m=0;
        return pit_wheel_level(sound_mix,human,0,false);
    }
    const Car *listener_car=nullptr;
    if(audio_listener_slot>=0 && audio_listener_slot<104) {
        const Car &candidate=cars[audio_listener_slot];
        if(candidate.seen && (candidate.snapshot.valid_fields&F1C_VEH_SNAPSHOT_POSITION))
            listener_car=&candidate;
    }
    if(!listener_car)for(const auto &candidate:cars) {
        if(candidate.seen && candidate.snapshot.controller_role==0 &&
           (candidate.snapshot.valid_fields&F1C_VEH_SNAPSHOT_POSITION)) {
            listener_car=&candidate;break;
        }
    }
    if(listener_car && (source.snapshot.valid_fields&F1C_VEH_SNAPSHOT_POSITION)) {
        double distance=pit_wheel_distance_m(source.snapshot.world_x,source.snapshot.world_y,
             source.snapshot.world_z,listener_car->snapshot.world_x,
             listener_car->snapshot.world_y,listener_car->snapshot.world_z);
        if(distance_m)*distance_m=distance;
        /* Only a listener known to be out on the circuit is masked; a car in
         * the lane, its box or with unknown pit state hears the normal mix. */
        bool on_track=(listener_car->snapshot.valid_fields&F1C_VEH_SNAPSHOT_PIT) &&
                      (listener_car->snapshot.pit_status==F1C_PIT_TRACK ||
                       listener_car->snapshot.pit_status==F1C_PIT_CALLED);
        return pit_wheel_level(sound_mix,human,distance,on_track);
    }
    /* Missing AI position data must never turn into global audio; the human
     * car's own stop keeps its normal, slider-scaled level. */
    if(human) {
        if(distance_m)*distance_m=0;
        return pit_wheel_level(sound_mix,true,0,false);
    }
    return 0;
}
static std::string xpitin_json(const Car &c) {
    const PitXpitinReading &p=c.xpitin;
    auto point=[](const float v[3]) {
        return "["+number(v[0])+","+number(v[1])+","+number(v[2])+"]";
    };
    auto timestamp=[](double value)->std::string {return value>=0?number(value):"null";};
    std::string out="{\"trigger\":\"active_call_within_150m_or_game_latch_rising\",\"player_position\":"+
        (p.player_valid?point(p.player):"null")+
        ",\"marker_position\":"+(p.marker_valid?point(p.marker):"null")+
        ",\"distance_m\":"+(p.marker_valid?number(p.distance_m):"null")+
        ",\"horizontal_m\":"+(p.marker_valid?number(p.horizontal_m):"null")+
        ",\"height_delta_m\":"+(p.marker_valid?number(p.height_delta_m):"null")+
        ",\"within_100m_reference\":"+(p.marker_valid?(p.within_100m?"true":"false"):"null")+
        ",\"within_150m_voice\":"+(p.marker_valid?(p.within_voice_radius?"true":"false"):"null")+
        ",\"game_latch\":"+(c.pit_entry_latch?"true":"false")+
        ",\"call_active\":"+(c.pit_entry_call_active?"true":"false")+
        ",\"pit_status\":"+std::to_string(c.snapshot.pit_status)+
        ",\"call_source\":"+quoted(c.snapshot.pit_status==F1C_PIT_CALLED ||
            c.snapshot.pit_status==F1C_PIT_LANE?"game_pit_state":
            c.pit_entry_call_active?"call_input_or_gui":"none")+
        ",\"last_acknowledged_at\":"+timestamp(c.pit_entry_ack_at)+
        ",\"last_ack_source\":"+quoted(c.pit_entry_ack_source)+
        ",\"last_ack_distance_m\":"+(c.pit_entry_ack_distance_m>=0?number(c.pit_entry_ack_distance_m):"null")+
        ",\"voice_pending\":"+(c.pit_entry_pending?"true":"false")+
        ",\"last_voice_attempt_at\":"+timestamp(c.pit_entry_voice_attempt_at)+
        ",\"last_voice_result\":"+quoted(c.pit_entry_voice_attempt_at<0?"none":
            c.pit_entry_voice_result==1?"queued":c.pit_entry_voice_result==0?"busy":"refused")+
        ",\"last_voice_queued_at\":"+timestamp(c.pit_entry_voice_queued_at)+"}";
    return out;
}
static std::string status() {
    EnterCriticalSection(&lock);
    FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER t; t.LowPart=ft.dwLowDateTime;t.HighPart=ft.dwHighDateTime;
    double unix_time=(double)(t.QuadPart-116444736000000000ULL)/10000000.0;
    double age=observed?(double)(GetTickCount()-observed)/1000.0:10000;
    std::string selectable,labels,resolutions,mode_message;
    std::string policy_version=policy_model==3?pf3_policy_version():"0.2.0-policy.3";
    std::string config_hash=policy_model==3?pf3_config_hash(&config3):"v2-legacy";
    int gate_bp=policy_model==3?config3.incident_chance_bp:0;
    if(policy_model==3) {
        selectable="[\"random\",\"slow_wheel\",\"wheelgun\",\"fuel_rig\",\"stall\",\"cross_thread\"]";
        labels="{\"random\":\"Normal policy (no forcing)\",\"slow_wheel\":\"Slow wheel change\",\"wheelgun\":\"Wheel gun / wheel nut problem\",\"fuel_rig\":\"Fuel rig / hose problem\",\"stall\":\"Engine stall\",\"cross_thread\":\"Cross-threaded / binding nut\"}";
        resolutions="{\"slow_wheel\":[\"minor\",\"awkward\"],\"wheelgun\":[\"quick_fix\",\"backup_gun\",\"irreparable_nut\"],\"fuel_rig\":[\"slow_flow\",\"temporary_no_flow\",\"interrupted_flow\",\"spill\"],\"stall\":[\"straightforward\",\"difficult_restart\"],\"cross_thread\":[\"corrected\",\"lengthy_rework\",\"irreparable\"]}";
        mode_message="LIVE GAME - two-stage v3 policy (gate "+std::to_string(gate_bp)+" bp; settings: "+settings_source+"). Player/AI stall and operation-timed callouts are active; custom wheel-gun WAV trial is enabled; player/AI terminal retirement is active.";
    } else {
        selectable="[\"slow_corner\",\"wheelgun\",\"fuel_rig\",\"stall_delay\",\"cross_thread\",\"terminal_wheel\"]";
        labels="{}";resolutions="{}";
        mode_message="LIVE GAME - accepted v2 policy (rollback build). AI engine stall/restart and terminal pit retirement trial. Fire pending.";
    }
    std::string out="{\"protocol\":\"pit-test-v1\",\"mode\":\"live\",\"test_mode\":true,\"instance_id\":"+quoted(instance)+
        ",\"session_generation\":"+quoted(std::to_string(epoch))+",\"observed_unix_seconds\":"+number(unix_time-age)+
        ",\"session_active\":"+(active?"true":"false")+",\"force_and_pit_supported\":"+((active && age<=3 && race_phase==5)?"true":"false")+
        ",\"status_reason\":"+quoted(!error.empty()?error:race_phase!=5?"Enter the driving session before requesting a pit stop":"Waiting for fresh game data")+
        ",\"policy_model\":"+std::to_string(policy_model)+
        ",\"policy_version\":"+quoted(policy_version)+",\"config_hash\":"+quoted(config_hash)+
        ",\"settings_source\":"+quoted(settings_source)+
        ",\"audio_listener_slot\":"+std::to_string(audio_listener_slot)+
        ",\"seed\":"+quoted(std::to_string(seed))+",\"gate_bp\":"+std::to_string(gate_bp)+
        ",\"selectable_failures\":"+selectable+",\"failure_labels\":"+labels+
        ",\"failure_resolutions\":"+resolutions+",\"fire_available\":false,\"mode_message\":"+quoted(mode_message)+",\"sim_time\":"+number(now)+
        ",\"player_xpitin\":";
    const Car *player=nullptr;
    if(active && age<=3)for(const auto &c:cars)
        if(c.seen && c.snapshot.controller_role==0) {player=&c;break;}
    out+=player?xpitin_json(*player):"null";
    out+=",\"drivers\":[";
    bool comma=false;
    for(unsigned i=0;i<104;i++) { const Car &c=cars[i]; if(!c.seen)continue;
        if(comma)out+=',';
        comma=true;
        out+="{\"slot\":"+std::to_string(i)+",\"object_generation\":"+quoted(std::to_string(c.snapshot.handle.object_generation))+
            ",\"name\":"+quoted(driver_utf8(c.snapshot.driver_name))+",\"role\":"+quoted(c.snapshot.controller_role==0?"human":"ai")+
            ",\"retired\":"+(c.snapshot.retired?"true":"false")+",\"engine_rpm\":"+number(c.rpm)+",\"phase\":"+quoted(c.phase)+",\"started\":"+number(c.start)+",\"plan\":"+plan_json(c)+
            ",\"crew_animation\":"+crew_json(c,c.snapshot.controller_role==0)+"}";
    }
    out+="],\"requests\":{";comma=false;
    for(const auto &entry:requests) {if(comma)out+=',';comma=true;
        out+=quoted(entry.first)+":{\"state\":"+quoted(entry.second.state=="queued"?"accepted":entry.second.state)+",\"message\":"+quoted(entry.second.message)+"}";}
    out+="},\"events\":[";comma=false;for(const auto &e:events){if(comma)out+=',';comma=true;out+=e;}out+="]}";
    LeaveCriticalSection(&lock);return out;
}
/* Bounded command-only JSON subset: all allowed string values are ASCII and
 * contain no escapes. Unknown/duplicate keys and non-integral numbers fail. */
static bool parse(const std::string &s,std::map<std::string,std::string> &out) {
    size_t i=0; auto white=[&](){while(i<s.size() && (s[i]==' '||s[i]=='\r'||s[i]=='\n'||s[i]=='\t'))i++;};
    auto string=[&](std::string &v){if(i>=s.size()||s[i++]!='"')return false;while(i<s.size()&&s[i]!='"'){unsigned char c=s[i++];if(c<32||c>126||c=='\\'||v.size()>=80)return false;v+=char(c);}return i<s.size()&&s[i++]=='"';};
    white();if(i>=s.size()||s[i++]!='{')return false;
    while(true){white();std::string k,v;if(!string(k))return false;white();if(i>=s.size()||s[i++]!=':')return false;white();
        bool numeric=k=="slot" || k=="failure_probability_bp" || k=="ttl_seconds";
        if(i<s.size()&&s[i]=='"'){if(numeric || !string(v))return false;}else{if(!numeric)return false;while(i<s.size()&&s[i]>='0'&&s[i]<='9'&&v.size()<20)v+=s[i++];if(v.empty())return false;}
        if(!out.emplace(k,v).second)return false;
        white();if(i>=s.size())return false;if(s[i]=='}'){i++;break;}if(s[i++]!=',')return false;}
    white();return i==s.size() && (out.size()==10 || out.size()==11 || (policy_model==3 && out.size()==12));
}
static std::string submit(const std::string &body) {
    std::map<std::string,std::string> d; if(!parse(body,d))return "{\"state\":\"rejected\",\"message\":\"Invalid JSON command\"}";
    const char *keys[]={"protocol","request_id","instance_id","session_generation","slot","object_generation","operation","failure_probability_bp","scope","ttl_seconds"};
    for(auto key:keys)if(!d.count(key))return "{\"state\":\"rejected\",\"message\":\"Invalid command fields\"}";
    if(d.size()==11 && !d.count("failure") && !(policy_model==3 && d.count("failure_resolution")))return "{\"state\":\"rejected\",\"message\":\"Unknown field\"}";
    if(d.size()==12 && (!d.count("failure")||!d.count("failure_resolution")))return "{\"state\":\"rejected\",\"message\":\"Unknown field\"}";
    std::string failure=d.count("failure")?d["failure"]:"random";
    std::string resolution=d.count("failure_resolution")?d["failure_resolution"]:"natural";
    int selector=1,parent=-1,res_index=-1;
    if(policy_model==3) {
        if(failure!="random") {
            parent=v3_parent_index(failure);
            if(parent<0)return "{\"state\":\"rejected\",\"message\":\"Selected failure unavailable\"}";
        }
        if(!resolution.empty() && resolution!="natural") {
            if(failure=="random")return "{\"state\":\"rejected\",\"message\":\"A forced resolution requires a selected forced failure\"}";
            res_index=v3_resolution_index(parent,resolution);
            if(res_index<0)return "{\"state\":\"rejected\",\"message\":\"Selected resolution unavailable for this failure\"}";
        } else resolution="natural";
    } else {
        if(d.count("failure_resolution"))return "{\"state\":\"rejected\",\"message\":\"Forced resolutions require the two-stage policy\"}";
        resolution="natural";
        if(failure!="random") {
            const char *names[]={"slow_corner","wheelgun","fuel_rig","stall_delay","cross_thread","terminal_wheel"};
            selector=0;for(int i=0;i<6;i++)if(failure==names[i])selector=i+2;
            if(!selector)return "{\"state\":\"rejected\",\"message\":\"Selected failure unavailable\"}";
        }
    }
    uint64_t slot,gen,obj;
    if(!pf_parse_u64_strict(d["slot"].c_str(),&slot)||slot>=104||!pf_parse_u64_strict(d["session_generation"].c_str(),&gen)||
        !pf_parse_u64_strict(d["object_generation"].c_str(),&obj))return "{\"state\":\"rejected\",\"message\":\"Invalid identity\"}";
    EnterCriticalSection(&lock);std::string message;Car &c=cars[slot];
    if(d["protocol"]!="pit-test-v1"||d["instance_id"]!=instance||gen!=epoch||!c.seen||obj!=c.snapshot.handle.object_generation) message="Stale driver/session identity";
    else if(!active||!observed||GetTickCount()-observed>3000)message="No fresh game update";
    else if(c.snapshot.retired)message="Driver already retired";
    else if(d["operation"]!="force_failure_and_pit"||d["failure_probability_bp"]!="10000"||d["scope"]!="next_eligible_service"||d["ttl_seconds"]!="300")message="Unsupported override";
    else if(d["request_id"].empty()||d["request_id"].size()>63)message="Invalid request ID";
    else if(requests.count(d["request_id"])) {
        const auto &r=requests.at(d["request_id"]);
        message=(r.slot==slot&&r.object==obj&&r.generation==gen)?"Already received; see authoritative status":"Request identity conflict";
        bool same=r.slot==slot&&r.object==obj&&r.generation==gen&&r.failure==failure&&r.resolution==resolution;
        if(!same)message="Request identity or failure conflict";
        LeaveCriticalSection(&lock);return "{\"state\":"+quoted(same?"accepted":"rejected")+",\"message\":"+quoted(message)+"}";
    } else if(requests.size()>=256)message="Receipt capacity reached";
    else if(!c.arm.empty()||c.snapshot.pit_status>=3)message="Driver already armed or in service";
    else {
        Request r;r.id=d["request_id"];r.slot=(unsigned)slot;r.object=(unsigned)obj;r.generation=(unsigned)gen;r.expires=GetTickCount()+300000;
        r.failure=failure;r.resolution=resolution;r.selector=selector;r.parent=parent;r.res_index=res_index;
        r.state="queued";r.message="Reserved; waiting for game-thread execution";requests.emplace(r.id,r);c.arm=r.id;
        LeaveCriticalSection(&lock);return "{\"state\":\"accepted\",\"message\":\"Reserved for game-thread execution\"}";
    }
    LeaveCriticalSection(&lock);return "{\"state\":\"rejected\",\"message\":"+quoted(message)+"}";
}
static DWORD WINAPI serve(void *) {
    while(WaitForSingleObject(stop_event,0)!=WAIT_OBJECT_0) {
        fd_set set;FD_ZERO(&set);FD_SET(listener,&set);timeval timeout{0,200000};
        if(select(0,&set,nullptr,nullptr,&timeout)<=0)continue;
        SOCKET peer=accept(listener,nullptr,nullptr);if(peer==INVALID_SOCKET)continue;
        DWORD ms=1000;setsockopt(peer,SOL_SOCKET,SO_RCVTIMEO,(char*)&ms,sizeof ms);setsockopt(peer,SOL_SOCKET,SO_SNDTIMEO,(char*)&ms,sizeof ms);
        std::string input;char buffer[2048];size_t split=std::string::npos;int length=0;
        while(input.size()<8192 && (split=input.find("\r\n\r\n"))==std::string::npos) {int n=recv(peer,buffer,sizeof buffer,0);if(n<=0)break;input.append(buffer,n);}
        std::string reply="{\"error\":\"Invalid local request\"}";int code=400;
        if(split!=std::string::npos) {
            std::string header=input.substr(0,split);std::string lower=header;
            std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)std::tolower(c);});
            bool local=lower.find("\r\nhost: 127.0.0.1:18743")!=std::string::npos && lower.find("\r\norigin:")==std::string::npos && lower.find("transfer-encoding:")==std::string::npos;
            if(local && header.rfind("GET /v1/status HTTP/1.",0)==0) {reply=status();code=200;}
            else if(local && header.rfind("POST /v1/commands HTTP/1.",0)==0 && lower.find("content-type: application/json")!=std::string::npos) {
                size_t at=lower.find("content-length:");if(at!=std::string::npos)length=atoi(lower.c_str()+at+15);
                if(length>0 && length<=4096) {
                    while(input.size()<split+4+(size_t)length){int n=recv(peer,buffer,sizeof buffer,0);if(n<=0)break;input.append(buffer,n);}
                    if(input.size()==split+4+(size_t)length){reply=submit(input.substr(split+4));code=200;}
                }
            }
        }
        std::string response="HTTP/1.1 "+std::to_string(code)+(code==200?" OK":" Bad Request")+"\r\nContent-Type: application/json\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "+std::to_string(reply.size())+"\r\n\r\n"+reply;
        size_t sent=0;while(sent<response.size()){int n=send(peer,response.data()+sent,(int)(response.size()-sent),0);if(n<=0)break;sent+=(size_t)n;}
        closesocket(peer);
    }return 0;
}
/* The broker owns the tested icon hook independently of the ESC menu client.
 * Refuse older brokers so terminal failures cannot silently lose results. */
static uint64_t pit_required_broker_capabilities() {
    return F1C_BROKER_CAP_LIFECYCLE|F1C_BROKER_CAP_SEMANTIC_PIT|
        F1C_BROKER_CAP_VEHICLE_SNAPSHOTS|F1C_BROKER_CAP_SESSION_SNAPSHOT|
        F1C_BROKER_CAP_POLL_EVENTS|F1C_BROKER_CAP_PAUSE_MENU|
        F1C_BROKER_CAP_GARAGE_RESULTS;
}
static bool pit_runtime_seed(uint64_t *out) {
    if(!out)return false;
    HMODULE library=LoadLibraryA("advapi32.dll");
    if(!library)return false;
    using EntropyFn=BOOLEAN (WINAPI *)(void *,ULONG);
    EntropyFn entropy=(EntropyFn)(uintptr_t)GetProcAddress(library,"SystemFunction036");
    uint64_t value=0;
    bool ok=entropy && entropy(&value,(ULONG)sizeof value)!=0 && value!=0;
    FreeLibrary(library);
    if(ok)*out=value;
    return ok;
}
#include "pit_compat.h"
#include "pit_loader_host.h"
static bool pit_target_supported(void) { return pit_compat::module(); }

/* 1.0.9: game-thread broker session-end callback. Sets a flag only; the Pit
 * worker loop calls pitu_pump() to perform the file write. The game thread
 * must never do the I/O. */
static void __cdecl pit_session_end(void *context) {
    (void)context;
    pitu_request();
}

static DWORD WINAPI worker(void *) {
    char path[MAX_PATH];DWORD path_len=GetModuleFileNameA(module,path,sizeof path);
    if(!path_len || path_len>=sizeof path)FreeLibraryAndExitThread(module,2);
    char *slash=strrchr(path,'\\');if(!slash)FreeLibraryAndExitThread(module,2);
    slash[1]=0;
    std::snprintf(log_path,sizeof log_path,PIT_LIVE_PUBLIC_BUILD?
                  "%spitstop_failures.log":"%spit_live_trial.log",path);
    { FILE *f=std::fopen(log_path,"a"); if(f){SYSTEMTIME t{};GetSystemTime(&t);
      std::fprintf(f,"Pit Stop Failures 1.0.6: startup %04u-%02u-%02uT%02u:%02u:%02uZ pid=%lu; Lightloader 2.0 hub host accepted.\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId());std::fclose(f);} }
    if(!pit_target_supported()) {
        FILE *file=std::fopen(log_path,"a");
        if(file){std::fputs("Pit Stop Failures refused: unsupported F1 Challenge executable layout.\n",file);std::fclose(file);}
        FreeLibraryAndExitThread(module,2);
    }
    char mutex_name[80];std::snprintf(mutex_name,sizeof mutex_name,PIT_LIVE_PUBLIC_BUILD?
        "Local\\F1C_PITSTOP_FAILURES_%lu":"Local\\F1C_PIT_TRIAL_%lu",GetCurrentProcessId());
    owner_mutex=CreateMutexA(nullptr,TRUE,mutex_name);if(!owner_mutex||GetLastError()==ERROR_ALREADY_EXISTS)FreeLibraryAndExitThread(module,3);
    broker.structure_size=sizeof broker;
    F1CBrokerDiscoveryResult broker_result=F1C_BROKER_DISCOVERY_NOT_FOUND;
    for(int attempt=0;attempt<40;attempt++) {
        broker_result=f1c_broker_discover(15,15,pit_required_broker_capabilities(),&broker);
        if(broker_result==F1C_BROKER_DISCOVERY_OK)break;
        if(broker_result!=F1C_BROKER_DISCOVERY_NOT_FOUND &&
           broker_result!=F1C_BROKER_DISCOVERY_QUERY_FAILED)break;
        Sleep(100);
    }
    if(broker_result!=F1C_BROKER_DISCOVERY_OK) {
        FILE *f=std::fopen(log_path,"a");
        if(f){std::fprintf(f,"Pit Stop Failures refused: broker ABI15/GARAGE_RESULTS unavailable (discovery %d).\n",(int)broker_result);std::fclose(f);}
        FreeLibraryAndExitThread(module,4);
    }
    if(!pit_runtime_seed(&seed)) {   /* never refuse: fall back to a time-based seed */
        LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
        seed=pit_presentation_mix((uint64_t)counter.QuadPart^((uint64_t)GetCurrentProcessId()<<32)^GetTickCount());
        if(!seed)seed=0x9E3779B97F4A7C15ull;
    }
    {
        FILE *f=std::fopen(log_path,"a");
        if(f){std::fprintf(f,"Pit Stop Failures: policy seed %llu\n",(unsigned long long)seed);std::fclose(f);}
    }
    char config_error[256];
    if(pf_config_parse(policy_config,&config,config_error,sizeof config_error)!=PF_CONFIG_OK)
        startup_refuse(6,std::string("embedded v2 policy: ")+config_error);
    policy=pf_session_create(&config,seed);if(!policy)startup_refuse(7,"could not create v2 policy session");
    if(policy_model==3) {
        if(pf3_config_parse(policy_config_v3,&config3,config_error,sizeof config_error)!=PF_CONFIG_OK)
            startup_refuse(8,std::string("embedded v3 policy: ")+config_error);
        char ini_path[MAX_PATH];
        int ini_length=std::snprintf(ini_path,sizeof ini_path,"%sPitStopFailures.ini",path);
        if(ini_length<0 || ini_length>=(int)sizeof ini_path)
            startup_refuse(8,"settings path exceeds MAX_PATH");
        std::string settings_error;
        PitPlayerSettingsLoad settings=pit_settings_load_file(ini_path,&config3,&settings_error);
        /* An invalid settings file never stops the mod: standard settings. */
        if(settings==PIT_SETTINGS_INVALID) {
            FILE *file=std::fopen(log_path,"a");
            if(file){std::fprintf(file,"Pit Stop Failures: PitStopFailures.ini ignored (%s); standard settings used.\n",settings_error.c_str());std::fclose(file);}
        }
        settings_source=settings==PIT_SETTINGS_LOADED?"PitStopFailures.ini":
                        settings==PIT_SETTINGS_INVALID?"embedded defaults (INI invalid)":"embedded defaults (INI missing)";
        if(settings==PIT_SETTINGS_DEFAULTS) {
            FILE *file=std::fopen(log_path,"a");
            if(file){std::fputs("Pit Stop Failures: PitStopFailures.ini missing; embedded standard settings used.\n",file);std::fclose(file);}
        }
        policy3=pf3_session_create(&config3,seed);
        if(!policy3)startup_refuse(9,"could not create v3 policy session");
    }
    if(broker.register_client(module,PIT_LIVE_PUBLIC_BUILD?"pitstop-failures":"pit-live-trial",&client)!=F1C_BROKER_OK){if(policy3)pf3_session_destroy(policy3);pf_session_destroy(policy);f1c_broker_release(&broker);FreeLibraryAndExitThread(module,5);}
    /* 1.0.9: optional ABI15 session-end hook writes the usage summary after the
     * race, outside DllMain and off the game thread. Absence only disables the
     * session-end flush (usage would then write only at unload/crash). */
    session_end_bind=f1c_broker_session_end_bind(&broker);
    if(session_end_bind && session_end_bind(client,pit_session_end,nullptr)==F1C_BROKER_OK)
        session_end_registered=true;
    InitializeCriticalSection(&lock);
    stop_event=CreateEventA(nullptr,TRUE,FALSE,PIT_LIVE_PUBLIC_BUILD?
                            "F1C_PITSTOP_FAILURES_UNLOAD":"F1C_PIT_LIVE_TRIAL_UNLOAD");
    if(!stop_event){broker.unregister_client(client,5000);return 0;}
    instance=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount());
    HANDLE server=nullptr;
    if(!PIT_LIVE_PUBLIC_BUILD) {
        WSADATA data;WSAStartup(MAKEWORD(2,2),&data);listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        int exclusive=1;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(char*)&exclusive,sizeof exclusive);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(18743);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(listener,(sockaddr*)&address,sizeof address)||listen(listener,8))SetEvent(stop_event);
        server=CreateThread(nullptr,0,serve,nullptr,0,nullptr);
        if(!server)SetEvent(stop_event);
    }
    if(PIT_LIVE_ENABLE_CUSTOM_WAV && init_sound_paths())audio_init(WG_SOUNDS,3);
    if(PIT_LIVE_ENABLE_CUSTOM_WAV) {
        load_sound_mix();
        FILE *file=std::fopen(log_path,"a");
        if(file){std::fprintf(file,"Pit Stop Failures: wheel-gun mix from %s: SoundFX %.3f, player %.3f, other %.3f.\n",
            sound_mix_source.c_str(),sound_mix.sfx_volume,sound_mix.player_ratio,sound_mix.other_ratio);std::fclose(file);}
    }
    { FILE *f=std::fopen(log_path,"a"); if(f){std::fputs("Pit Stop Failures 1.0.5: initialized; required game layout and broker accepted; waiting for a race.\n",f);std::fclose(f);} }
    pitu_veh_install();   /* batch U2: crash summary on a real fault */
    size_t logged=0;std::string last_log;
    while(WaitForSingleObject(stop_event,25)==WAIT_TIMEOUT) {
        F1CBrokerEvent broker_events[16]{};uint32_t event_count=0;uint64_t latest=0,dropped=0;
        if(broker.poll_events(client,broker_event_cursor,broker_events,16,&event_count,
                              &broker_event_cursor,&latest,&dropped)==F1C_BROKER_OK) {
            for(uint32_t i=0;i<event_count;i++)handle_broker_event(broker_events[i]);
        }
        pitu_pump();   /* 1.0.9: honour a pending session-end usage request */
        if(InterlockedCompareExchange(&pending,1,0)==0 && broker.post(client,update,nullptr)!=F1C_BROKER_OK)InterlockedExchange(&pending,0);
        EnterCriticalSection(&lock);
        std::vector<std::string> copy=events;
        std::vector<SfxRequest> bursts;
        for(const auto &r:sfx_requests) {
            if(r.slot>=104)continue;
            const Car &c=cars[r.slot];
            if(active && r.epoch==epoch && c.seen && c.token==r.token &&
               c.snapshot.handle.object_generation==r.object && now>=r.at && now-r.at<=1) {
                SfxRequest playback=r;
                playback.gain=wheel_audio_gain_for(c,&playback.distance_m);
                if(playback.gain>0)bursts.push_back(playback);
                else if(r.corner==0)event("SFX: car "+std::to_string(r.slot)+
                    " outside listener range (slot "+std::to_string(audio_listener_slot)+")");
            }
        }
        sfx_requests.clear();
        LeaveCriticalSection(&lock);
        for(const auto &r:bursts)play_burst(r.corner,r.burst,r.gain,r.slot,r.distance_m);
        FILE *file=nullptr;
        if(!copy.empty() && copy.back()!=last_log) {
            file=fopen(log_path,"a");
            size_t begin=0;if(!last_log.empty()){auto found=std::find(copy.begin(),copy.end(),last_log);if(found!=copy.end())begin=(size_t)(found-copy.begin())+1;}
            if(file){for(size_t i=begin;i<copy.size();i++)std::fprintf(file,"%s\n",copy[i].c_str());fclose(file);}last_log=copy.back();logged+=copy.size()-begin;
        }
    }
    (void)logged;
    if(server){if(WaitForSingleObject(server,3000)!=WAIT_OBJECT_0)return 0;CloseHandle(server);}
    if(!PIT_LIVE_PUBLIC_BUILD){closesocket(listener);WSACleanup();}
    broker.pause_menu_arm(client,0u);
    if(broker.post(client,cleanup_mechanics,nullptr)!=F1C_BROKER_OK)return 0;
    for(int i=0;i<200 && !cleanup_done;i++)Sleep(10);
    if(!cleanup_done)return 0; // Stay mapped until the game-thread cleanup can run.
    if(session_end_registered && session_end_bind) session_end_bind(client,nullptr,nullptr);
    if(broker.unregister_client(client,5000)!=F1C_BROKER_OK) return 0; /* Stay mapped if callback still owns code. */
    audio_shutdown();
    if(policy3)pf3_session_destroy(policy3);
    pf_session_destroy(policy);f1c_broker_release(&broker);CloseHandle(stop_event);ReleaseMutex(owner_mutex);CloseHandle(owner_mutex);
    pitu_veh_remove();   /* batch U2: detach the crash handler before flush */
    pitu_flush("unload");   /* batch U2: write the usage summary */
    DeleteCriticalSection(&lock);FreeLibraryAndExitThread(module,0);return 0;
}
extern "C" __declspec(dllexport) const char *F1CModSdkVersion(void) {return F1C_SDK_VERSION_STRING;}
static volatile LONG loader_init_state;
extern "C" __declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(const LightLoaderProtectedHostV2 *host) {
    if(InterlockedCompareExchange(&loader_init_state,1,0)!=0)return FALSE;
    if(!pit_validate_loader_host(host)){InterlockedExchange(&loader_init_state,0);return FALSE;}
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if(!thread){InterlockedExchange(&loader_init_state,0);return FALSE;}
    CloseHandle(thread);return TRUE;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}
    return TRUE;
}
