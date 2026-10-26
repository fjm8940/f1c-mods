/* Compile the actual client against deterministic game memory/broker fixtures. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <cstdint>
static unsigned char test_vehicles[104][0x48c0];
#define F1C_CUSTOM_VEH_ACCESS
#define VEH_SLOTS 104
#define VEH(slot) (test_vehicles[slot])
#define VEH_ACTIVE(slot) (1)
#include "f1c_runtime.h"
static double test_clock;
#undef G_SessionClockSeconds
#define G_SessionClockSeconds (&test_clock)
#define F1C_PIT_VOICE_TEST
#define F1C_PIT_HUD_TEST
#define PIT_AI_MECHANICS_TEST
#include "f1c_pit_voice.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "f1c_vehicle_lifecycle.h"
#pragma GCC diagnostic pop
static int stops=0,starts=0,retires=0,menu_arms=0,menu_disarms=0,menu_commands=0;
static bool test_hide_driver=false;
static int f1c_ai_engine_stop(F1CLifecycleRead,uintptr_t v,DWORD,int) {stops++;VEHF_EngineRPM((void*)v)=0;return 1;}
static int f1c_ai_pit_restart(F1CLifecycleRead,uintptr_t v,DWORD) {starts++;VEHF_EngineRPM((void*)v)=1000;return 1;}
static int f1c_ai_pit_retire(F1CLifecycleRead,uintptr_t,DWORD) {retires++;return 1;}
static int f1c_pit_hud_post(const char *) {return 1;}
static int voice_count, voice_eids[256], voice_subjects[256];
static int test_voice_busy_eid=-1,test_voice_busy_attempts=0;
static int f1c_pit_voice_enqueue(int eid,int subject);
static int voice_front_count=0;
static int f1c_pit_voice_enqueue_front(int eid,int subject) {
    int accepted=f1c_pit_voice_enqueue(eid,subject);
    if(accepted==1)voice_front_count++;
    return accepted;
}
static int f1c_pit_voice_enqueue(int eid,int subject) {
    if(!f1c_pit_voice_allowed(eid))return -1;
    if(eid==test_voice_busy_eid && test_voice_busy_attempts>0) {
        test_voice_busy_attempts--;return 0;
    }
    if(voice_count<256){voice_eids[voice_count]=eid;voice_subjects[voice_count]=subject;voice_count++;}
    return 1;
}
#include "live_client.cpp"
#include <cassert>
#include "mods/sdk_sandbox/game_update_broker/broker_garage_results.h"
static F1CBrokerVehicleSnapshot test_snapshot;
static unsigned test_epoch=1, test_object=1;
static F1CBrokerResult __cdecl fake_stats(F1CBrokerStats *s){s->session_generation=test_epoch;return F1C_BROKER_OK;}
static F1CBrokerResult __cdecl fake_session(F1CBrokerToken,uint32_t generation,F1CBrokerSessionSnapshot *s){
    if(generation!=test_epoch)return F1C_BROKER_STALE_SESSION;
    s->valid_fields=F1C_SESSION_SNAPSHOT_CORE;s->session_active=1;s->session_generation=test_epoch;s->snapshot_sequence=10;s->race_phase=5;return F1C_BROKER_OK;
}
static F1CBrokerResult __cdecl fake_handles(F1CBrokerToken,uint32_t generation,F1CBrokerVehicleHandle *h,uint32_t,uint32_t *n){if(test_hide_driver){*n=0;return F1C_BROKER_OK;}h[0]={generation,test_object,0,0};*n=1;return F1C_BROKER_OK;}
static F1CBrokerResult __cdecl fake_snapshot(F1CBrokerToken,F1CBrokerVehicleHandle h,F1CBrokerVehicleSnapshot *s){*s=test_snapshot;s->handle=h;return F1C_BROKER_OK;}
static F1CBrokerResult __cdecl fake_pause_arm(F1CBrokerToken,uint32_t enabled){if(enabled)menu_arms++;else menu_disarms++;return F1C_BROKER_OK;}
static F1CBrokerResult __cdecl fake_pause_command(F1CBrokerToken,F1CBrokerMenuCommand command){if(command==F1C_BROKER_MENU_RETIRE_PITS)menu_commands++;return F1C_BROKER_OK;}
static std::string command(const char *id="test-1",const char *object="1"){
    return std::string("{\"protocol\":\"pit-test-v1\",\"request_id\":\"")+id+"\",\"instance_id\":\"test\",\"session_generation\":\"1\",\"slot\":0,\"object_generation\":\""+object+"\",\"operation\":\"force_failure_and_pit\",\"failure_probability_bp\":10000,\"scope\":\"next_eligible_service\",\"ttl_seconds\":300}";
}
static unsigned char crew_fixture_vehicle[0x32d8],crew_fixture_object[0x14];
static unsigned char crew_fixture_actors[5*120],crew_fixture_channels[5*2*28];
static unsigned char crew_fixture_names[5*48];
static unsigned char xpitin_fixture_steward[0x198],xpitin_fixture_node[0x208],xpitin_fixture_render[0xf4];
static int xpitin_fixture_read(uintptr_t address,void *out,size_t length) {
    struct Region {uintptr_t start;const unsigned char *bytes;size_t length;};
    const Region regions[]={
        {0x70000,xpitin_fixture_steward,sizeof xpitin_fixture_steward},
        {0x71000,xpitin_fixture_node,sizeof xpitin_fixture_node},
        {0x72000,xpitin_fixture_render,sizeof xpitin_fixture_render}};
    for(const auto &region:regions)
        if(address>=region.start && address-region.start<=region.length &&
           length<=region.length-(address-region.start)) {
            memcpy(out,region.bytes+(address-region.start),length);return 1;
        }
    return 0;
}
static int crew_fixture_read(uintptr_t address,void *out,size_t length) {
    struct Region {uintptr_t start;const unsigned char *bytes;size_t length;};
    const Region regions[]={
        {0x20000,crew_fixture_vehicle,sizeof crew_fixture_vehicle},
        {0x30000,crew_fixture_object,sizeof crew_fixture_object},
        {0x40000,crew_fixture_actors,sizeof crew_fixture_actors},
        {0x50000,crew_fixture_channels,sizeof crew_fixture_channels},
        {0x60000,crew_fixture_names,sizeof crew_fixture_names}};
    for(const auto &region:regions)
        if(address>=region.start && address-region.start<=region.length &&
           length<=region.length-(address-region.start)) {
            memcpy(out,region.bytes+(address-region.start),length);return 1;
        }
    return 0;
}
static void crew_fixture_u32(unsigned char *base,size_t offset,uint32_t value) {
    memcpy(base+offset,&value,sizeof value);
}
static void crew_fixture_f32(unsigned char *base,size_t offset,float value) {
    memcpy(base+offset,&value,sizeof value);
}
int main(){
    seed=20260921; /* Fixed only for deterministic fixture expectations. */
    { /* Exact cached XPITIN marker, player distance, and unavailable marker. */
      crew_fixture_u32(xpitin_fixture_steward,0x194,0x71000);
      crew_fixture_u32(xpitin_fixture_node,0x204,0x72000);
      crew_fixture_u32(xpitin_fixture_render,0x14,0x20000000);
      const float marker[]={100.0f,1.0f,50.0f};
      const float player[]={40.0f,1.0f,50.0f};
      memcpy(xpitin_fixture_render+0xe8,marker,sizeof marker);
      auto reading=pit_live_xpitin_read(0x70000,xpitin_fixture_read,player);
      assert(reading.player_valid && reading.marker_valid && reading.within_100m);
      assert(reading.distance_m==60.0 && reading.horizontal_m==60.0);
      assert(reading.within_voice_radius);
      const float outside_player[]={-51.0f,1.0f,50.0f};
      const float boundary_player[]={-50.0f,1.0f,50.0f};
      const float inside_player[]={-49.0f,1.0f,50.0f};
      const auto outside_reading=pit_live_xpitin_read(0x70000,xpitin_fixture_read,outside_player);
      const auto boundary_reading=pit_live_xpitin_read(0x70000,xpitin_fixture_read,boundary_player);
      assert(!outside_reading.within_voice_radius && outside_reading.horizontal_m==151.0);
      assert(boundary_reading.within_voice_radius && boundary_reading.horizontal_m==150.0);
      assert(pit_live_xpitin_read(0x70000,xpitin_fixture_read,inside_player).within_voice_radius);
      assert(pit_voice_approach(true,true,false,false,outside_reading.within_voice_radius,F1C_PIT_CALLED)==-1);
      assert(pit_voice_approach(true,true,false,false,boundary_reading.within_voice_radius,F1C_PIT_CALLED)==50);
      const float near_player[]={65.0f,1.0f,50.0f};
      auto near_reading=pit_live_xpitin_read(0x70000,xpitin_fixture_read,near_player);
      assert(near_reading.within_voice_radius && near_reading.distance_m==35.0);
      assert(pit_voice_approach(true,
          pit_voice_call_active(F1C_PIT_CALLED,false,false,false,false),
          false,false,near_reading.within_voice_radius,F1C_PIT_CALLED)==50);
      const float high_player[]={65.0f,22.0f,50.0f};
      assert(!pit_live_xpitin_read(0x70000,xpitin_fixture_read,high_player).within_voice_radius);
      Car diagnostic{};diagnostic.xpitin=reading;diagnostic.pit_entry_latch=true;
      diagnostic.snapshot.pit_status=F1C_PIT_CALLED;
      diagnostic.pit_entry_call_active=true;diagnostic.pit_entry_ack_at=12.0;
      diagnostic.pit_entry_ack_distance_m=60.0;diagnostic.pit_entry_voice_queued_at=12.1;
      diagnostic.pit_entry_voice_attempt_at=12.1;diagnostic.pit_entry_voice_result=1;
      diagnostic.pit_entry_ack_source="game_latch";
      std::string display=xpitin_json(diagnostic);
      assert(display.find("\"distance_m\":60.000000")!=std::string::npos);
      assert(display.find("\"trigger\":\"active_call_within_150m_or_game_latch_rising\"")!=std::string::npos);
      assert(display.find("\"within_150m_voice\":true")!=std::string::npos);
      assert(display.find("\"last_ack_source\":\"game_latch\"")!=std::string::npos);
      assert(display.find("\"last_acknowledged_at\":12.000000")!=std::string::npos);
      assert(display.find("\"last_voice_result\":\"queued\"")!=std::string::npos);
      assert(display.find("\"call_source\":\"game_pit_state\"")!=std::string::npos);
      crew_fixture_u32(xpitin_fixture_steward,0x194,0);
      reading=pit_live_xpitin_read(0x70000,xpitin_fixture_read,player);
      assert(reading.player_valid && !reading.marker_valid);
      diagnostic.xpitin=reading;display=xpitin_json(diagnostic);
      assert(display.find("\"distance_m\":null")!=std::string::npos);
      crew_fixture_u32(xpitin_fixture_steward,0x194,0x71000);
    }
    { /* Bounded actor walk, longest channel, partial data, and stale-pointer refusal. */
      crew_fixture_u32(crew_fixture_vehicle,0x32d4,0x30000);
      crew_fixture_u32(crew_fixture_object,0x0c,5);
      crew_fixture_u32(crew_fixture_object,0x10,0x40000);
      const char *names[]={"WHEELGUN_FRONT_LEFT","WHEELGUN_FRONT_RIGHT",
                            "WHEELGUN_REAR_LEFT","WHEELGUN_REAR_RIGHT","FUEL_RIG"};
      for(int i=0;i<5;i++) {
        memcpy(crew_fixture_names+i*48,names[i],strlen(names[i])+1);
        crew_fixture_u32(crew_fixture_actors,i*120+100,0x60000+i*48);
        crew_fixture_u32(crew_fixture_actors,i*120+112,i==4?0:2);
        crew_fixture_u32(crew_fixture_actors,i*120+116,i==4?0:0x50000+i*56);
        crew_fixture_f32(crew_fixture_channels,i*56+12,1.0f+i*0.5f);
        crew_fixture_f32(crew_fixture_channels,i*56+16,3.0f+i*0.5f);
        crew_fixture_f32(crew_fixture_channels,i*56+28+12,2.0f+i*0.5f);
        crew_fixture_f32(crew_fixture_channels,i*56+28+16,9.0f+i*0.5f);
      }
      crew_fixture_f32(crew_fixture_channels,2*56+28+16,NAN);
      F1CPitCrewSnapshot observed{};
      assert(f1c_pit_crew_snapshot(crew_fixture_read,0x20000,&observed));
      assert(observed.actor_count==5 && observed.readable_count==5 && observed.window_count==4);
      for(int i=0;i<4;i++)assert(observed.actors[i].corner==i && observed.actors[i].window_valid);
      assert(observed.actors[0].start==2.0f && observed.actors[0].end==9.0f);
      assert(observed.actors[2].start==2.0f && observed.actors[2].end==4.0f);
      assert(!observed.actors[4].window_valid && observed.actors[4].channel_count==0);
      Car diagnostic{};diagnostic.token=1;diagnostic.crew=observed;diagnostic.crew_read=true;
      diagnostic.crew_anchor=100;now=105;
      std::string json=crew_json(diagnostic,true);
      assert(json.find("\"WHEELGUN_FRONT_LEFT\"")!=std::string::npos);
      assert(json.find("\"range_relation\":\"within\"")!=std::string::npos);
      assert(json.find("\"range_relation\":\"after\"")!=std::string::npos);
      crew_fixture_u32(crew_fixture_object,0x0c,65);
      assert(!f1c_pit_crew_snapshot(crew_fixture_read,0x20000,&observed));
      crew_fixture_u32(crew_fixture_object,0x0c,5);
      crew_fixture_u32(crew_fixture_object,0x10,0x7fffffe0u);
      assert(f1c_pit_crew_snapshot(crew_fixture_read,0x20000,&observed));
      assert(observed.readable_count==0);
      crew_fixture_u32(crew_fixture_object,0x10,0x40000);
      now=0;
    }
    { /* Game's reviewed per-car 12 L/s formula, with each physical variant. */
      for(int variant=1;variant<=4;variant++) {
        F1CPitMachine pit{};pit.fuel_start=20;pit.fuel_start_time=100;
        pit.fuel_target=80;pit.refuel_active=1;
        FuelEffect effect{};effect.variant=variant;
        float fuel=20,at_101=0,at_105=0;bool saw_cut=false,saw_spill=false,saw_resume=false;
        for(int step=0;step<300;step++) {
          double t=100.0+step*0.1;
          FuelEffectResult r=fuel_effect_step(effect,pit,fuel,t,4.0);
          assert(!r.invalid);
          if(r.spill_now)saw_spill=true;
          if(r.recovered_now)saw_resume=true;
          if(effect.cut)saw_cut=true;
          float next=pit.fuel_start+(float)(t+0.1-pit.fuel_start_time)*12.0f;
          fuel=std::min(pit.fuel_target,std::max(0.0f,next));
          if(step==10)at_101=fuel;
          if(step==50)at_105=fuel;
          if(!r.hold && (variant==4||fuel>=pit.fuel_target-0.15f))break;
        }
        if(variant==1){assert(at_101>23 && at_101<27);assert(fuel>=79.8f);}
        if(variant==2){assert(at_101<23 && at_105>at_101+3 && saw_resume);}
        if(variant==3){assert(saw_cut && at_105<40 && saw_resume);}
        if(variant==4){assert(saw_spill && pit.fuel_target<65 && fuel<65);}
      }
    }
    { /* Reproduce game-style x87 single precision, including a same-tick commit/drain. */
      unsigned short saved, single;
      __asm__ __volatile__("fnstcw %0" : "=m"(saved));
      single=(saved & ~0x0300);
      pf_config_t cfg;pf_config_defaults(&cfg);
      pf_session_t *s=pf_session_create(&cfg,seed);pf_session_new_epoch(s,1);
      pf_command_t cmd[8];
      __asm__ __volatile__("fldcw %0" : : "m"(single));
      volatile double at=250.123456789;
      int first=pf_session_advance(s,at,cmd,8);
      int second=pf_session_advance(s,at,cmd,8);
      __asm__ __volatile__("fldcw %0" : : "m"(saved));
      printf("x87 same-clock probe: %d then %d (error %d)\n",first,second,(int)pf_session_last_error(s));
      pf_session_destroy(s);
      assert(first==0 && second<0); // This reproduces the unguarded defect.
      for(unsigned precision : {0u,0x0200u,0x0300u})for(unsigned rounding : {0u,0x0400u,0x0800u,0x0c00u}) {
        unsigned short game=(saved & ~0x0f00u)|precision|rounding;
        s=pf_session_create(&cfg,seed);pf_session_new_epoch(s,1);
        pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
        pit_fp_restore(game);
        for(unsigned car=1;car<=24;car++) {
          pf_identity_t id{1,car,1};pf_plan_t plan{};int committed=0;
          assert(pit_policy_math([&]{return pft_session_commit(s,&id,&ctx,&ctx.eligibility,at,&plan,&committed,2);})==0 && committed);
          assert(pit_fp_control()==game);
          assert(pit_policy_math([&]{return pf_session_advance(s,at,cmd,8);})>=0);
          assert(pit_fp_control()==game);
        }
        // Real backwards time must still fail; no tolerance or reroll workaround.
        assert(pit_policy_math([&]{return pf_session_advance(s,249,cmd,8);})<0);
        assert(pit_fp_control()==game);
        pit_fp_restore(saved);pf_session_destroy(s);
      }
    }
    {alignas(float) unsigned char hud[800]{};
     assert(f1c_pit_hud_write_slots(hud,"WHEEL GUN FAILURE (FL)")==1);
     assert(!strcmp((char*)hud+0x3C,"WHEEL GUN FAILURE (FL)"));
     for(int i=1;i<8;i++)assert(f1c_pit_hud_write_slots(hud,"occupied")==i+1);
     unsigned char before[800];memcpy(before,hud,800);
     assert(f1c_pit_hud_write_slots(hud,"must not overwrite")==0);assert(!memcmp(before,hud,800));
     pf_plan_t p{};p.outcome=PF_OUTCOME_WHEELGUN;p.wheel=2;
     auto f=legacy_feedback(p,PF_CMD_FAULT_NOTICE,0,true);assert(f.eid==68 && f.burst==2);
     assert(!strcmp(f.message,"WHEEL GUN FAILURE (RL)"));
     f=legacy_feedback(p,PF_CMD_RECOVERY_ATTEMPT,1,true);assert(f.eid==66 && f.burst==0);
     f=legacy_feedback(p,PF_CMD_RECOVERED,2,true);assert(f.eid==67 && !strcmp(f.message,"WHEEL GUN FIXED (RL)"));
     f=legacy_feedback(p,PF_CMD_FAULT_NOTICE,0,false);assert(f.eid==183 && f.burst==-1);
     p.terminal=1;assert(legacy_feedback(p,PF_CMD_RECOVERED,1,true).message==nullptr);
     p.terminal=0;p.outcome=PF_OUTCOME_SLOW_CORNER;p.wheel=2;
     f=legacy_feedback(p,PF_CMD_FAULT_NOTICE,0,true);
     assert(f.eid==68 && !strcmp(f.message,"SLOW WHEEL CHANGE (RL)"));
     f=legacy_feedback(p,PF_CMD_RECOVERED,0,true);
     assert(f.eid==67 && !strcmp(f.message,"WHEEL CHANGE DONE (RL)"));
    }
    {pf_plan_t p{};
     const int player_eids[]={-1,63,61,56,54,66};
     const int ai_eids[]={-1,183,183,185,184,186};
     for(int outcome=0;outcome<=5;outcome++) {
       p.outcome=(pf_outcome_t)outcome;p.wheel=0;
       assert(pit_voice_fault(p,true)==player_eids[outcome]);
       assert(pit_voice_fault(p,false)==ai_eids[outcome]);
     }
     for(int corner=0;corner<4;corner++) {
       p.outcome=PF_OUTCOME_CROSS_THREAD;p.wheel=corner;
       assert(pit_voice_fault(p,false)==186+corner);
       const int clear[]={62,64,67,69};assert(pit_voice_recovered(p,true)==clear[corner]);
     }
     p.terminal=1;assert(pit_voice_fault(p,true)==-1 && pit_voice_recovered(p,true)==-1);
     p.terminal=0;p.outcome=PF_OUTCOME_SLOW_CORNER;p.wheel=2;
     assert(pit_voice_recovered(p,true)==67);
     assert(f1c_pit_voice_allowed(50) && f1c_pit_voice_allowed(57));
     assert(pit_voice_edge(0,2,true,false)==51);assert(pit_voice_edge(1,2,true,false)==51);
     assert(pit_voice_edge(2,3,true,false)==53);assert(pit_voice_edge(3,4,true,false)==45);
     assert(pit_voice_edge(4,0,true,false)==43);assert(pit_voice_edge(3,4,true,true)==-1);
     assert(pit_voice_edge(3,3,true,false)==-1);assert(pit_voice_edge(3,4,false,false)==-1);
     assert(pit_voice_approach(true,true,false,true,false,1)==50);
     assert(pit_voice_call_active(F1C_PIT_CALLED,false,false,false,false));
     assert(pit_voice_call_active(F1C_PIT_LANE,false,false,false,false));
     assert(!pit_voice_call_active(F1C_PIT_TRACK,false,false,false,false));
     assert(pit_voice_approach(true,
         pit_voice_call_active(F1C_PIT_CALLED,false,false,false,false),false,false,true,1)==50);
     assert(pit_voice_approach(true,true,false,false,true,1)==50);
     assert(pit_voice_approach(true,false,false,false,true,1)==-1);
     assert(pit_voice_approach(true,true,true,true,false,1)==-1);
     assert(pit_voice_approach(true,true,false,true,true,3)==-1);
     assert(pit_voice_approach(false,true,false,true,true,1)==-1);
     assert(pit_voice_fuel_clear(true,true,false,true,false,150.0f,150.0f)==57);
     assert(pit_voice_fuel_clear(true,true,false,true,false,149.0f,150.0f)==-1);
     assert(pit_voice_fuel_clear(true,true,true,true,false,150.0f,150.0f)==-1);
     assert(pit_voice_fuel_clear(true,true,false,false,false,150.0f,150.0f)==-1);
     assert(pit_voice_fuel_clear(true,true,false,true,true,150.0f,150.0f)==-1);
    }
    {pf_config_t cfg;pf_config_defaults(&cfg);pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
     int matched=0;
     for(unsigned token=1;token<=10000;token++) {
       pf_identity_t id{1,7,token};pf_plan_t original{};pf_plan_build(&cfg,&id,&ctx,seed,&original);
       for(int selection=2;selection<=7;selection++) {
         pf_plan_t selected{};pft_plan_build(&cfg,&id,&ctx,seed,&selected,selection);
         assert((int)selected.outcome==selection-1);
         if(selected.outcome==original.outcome){assert(memcmp(&selected,&original,sizeof selected)==0);matched++;}
       }
     }
     assert(matched>0);
     pf_session_t *s=pf_session_create(&cfg,seed);pf_session_new_epoch(s,1);
     pf_identity_t id{1,7,1};pf_plan_t plan{};int committed=0;
     ctx.eligibility.fuel_allowed=0;ctx.eligibility.fuel_add_liters=0;
     assert(pft_session_commit(s,&id,&ctx,&ctx.eligibility,1,&plan,&committed,4)==-1);
     assert(pft_session_commit(s,&id,&ctx,&ctx.eligibility,1,&plan,&committed,2)==0 && committed);
     assert(plan.outcome==PF_OUTCOME_SLOW_CORNER);pf_session_destroy(s);
    }
    InitializeCriticalSection(&lock);stop_event=CreateEventA(nullptr,TRUE,FALSE,nullptr);instance="test";
    pf_config_defaults(&config);policy=pf_session_create(&config,seed);
    pf3_config_defaults(&config3);policy3=pf3_session_create(&config3,seed);
    assert(policy_model==PIT_LIVE_POLICY_MODEL && policy3!=nullptr);
    { /* Player INI is read from the DLL directory at startup. Invalid edits
         must leave the already validated embedded policy untouched. */
      pf3_config_t packaged=config3;
      std::string settings_error;
      assert(pit_settings_load_file("PitStopFailures.ini",&packaged,&settings_error)==PIT_SETTINGS_LOADED);
      assert(packaged.incident_chance_bp==750);
      assert(packaged.parent_weight[PF3_PARENT_SLOW_WHEEL]==60);
      assert(packaged.parent_weight[PF3_PARENT_STALL]==4);
      pf3_config_t missing=config3;
      assert(pit_settings_load_file("PitStopFailures.absent-test.ini",&missing,&settings_error)==PIT_SETTINGS_DEFAULTS);
      assert(memcmp(&missing,&config3,sizeof missing)==0);
      const std::string custom="[PitFailures]\nincident_chance_percent = 5.25\n"
          "slow_wheel = 50\nwheel_gun = 20\nfuel_rig = 20\nengine_stall = 5\ncross_thread = 5\n";
      pf3_config_t changed{};
      assert(pit_settings_parse(custom,config3,&changed,&settings_error));
      assert(changed.incident_chance_bp==525 && changed.parent_weight[PF3_PARENT_STALL]==5);
      std::string changed_hash=pf3_config_hash(&changed);
      std::string default_hash=pf3_config_hash(&config3);
      assert(changed_hash!=default_hash);
      pf3_config_t unchanged=config3;
      assert(!pit_settings_parse(custom+"slow_wheel = 50\n",config3,&unchanged,&settings_error));
      assert(settings_error.find("duplicate")!=std::string::npos);
      assert(memcmp(&unchanged,&config3,sizeof unchanged)==0);
      assert(!pit_settings_parse("[PitFailures]\nincident_chance_percent = 101\n",
                                 config3,&unchanged,&settings_error));
      assert(!pit_settings_parse("[PitFailures]\nincident_chance_percent = 7.5\n"
                                 "slow_wheel = 0\nwheel_gun = 0\nfuel_rig = 0\n"
                                 "engine_stall = 0\ncross_thread = 0\n",
                                 config3,&unchanged,&settings_error));
      assert(settings_error.find("at least one")!=std::string::npos);
      assert(memcmp(&unchanged,&config3,sizeof unchanged)==0);
      const std::string single_edit="[PitFailures]\nincident_chance_percent = 7.5\n"
          "slow_wheel = 60\nwheel_gun = 17\nfuel_rig = 17\nengine_stall = 1\ncross_thread = 2\n";
      assert(pit_settings_parse(single_edit,config3,&changed,&settings_error));
      assert(changed.parent_weight[PF3_PARENT_STALL]==1);
    }
    { /* 1.1.1 configurable gate, subtype selection, and atomic rejection. */
      std::string err;
      const std::string old="[PitFailures]\nincident_chance_percent = 7.50\n"
          "slow_wheel = 60\nwheel_gun = 17\nfuel_rig = 17\nengine_stall = 4\ncross_thread = 2\n";
      pf3_config_t parsed{};
      assert(pit_settings_parse(old,config3,&parsed,&err));
      assert(memcmp(&parsed,&config3,sizeof parsed)==0);
      for(const char *value:{"0","0.00","0.01","4.99","10.01","99.99","100","100.0","100.00"}) {
        int bp=-1; assert(pit_settings_percent_bp(value,&bp));
        pf3_config_t c=config3;c.incident_chance_bp=bp;
        assert(pf3_config_validate(&c,nullptr,0)==PF_CONFIG_OK);
        for(unsigned draw=0;draw<10000;draw++)assert(pf3_gate_passes_for_draw(&c,draw)==(draw<(unsigned)bp));
      }
      for(const char *value:{"-1","100.01","101","1.001","nan","inf","1e2",".5","1.","1.2.3",""," 100"}) {
        int bp=789;assert(!pit_settings_percent_bp(value,&bp));assert(bp==789);
      }
      assert(pit_settings_parse(old+"incident_chance_override_percent = 100.00\n",config3,&parsed,&err));
      assert(parsed.incident_chance_bp==10000);
      assert(pit_settings_parse(old+"incident_chance_override_percent = inherit\n",config3,&parsed,&err));
      assert(parsed.incident_chance_bp==750);
      assert(pit_settings_parse("[PitFailures]\nincident_chance_override_percent = 100\n"
          "incident_chance_percent = 7.5\nslow_wheel = 60\nwheel_gun = 17\nfuel_rig = 17\n"
          "engine_stall = 4\ncross_thread = 2\n",config3,&parsed,&err));
      assert(parsed.incident_chance_bp==10000);
      const char *subkeys[5][4]={
        {"slow_wheel_minor","slow_wheel_awkward",nullptr,nullptr},
        {"wheel_gun_quick","wheel_gun_backup","wheel_gun_irreparable",nullptr},
        {"fuel_slow_flow","fuel_delayed_flow","fuel_interrupted_flow","fuel_spill"},
        {"stall_straightforward","stall_difficult",nullptr,nullptr},
        {"cross_thread_corrected","cross_thread_lengthy","cross_thread_irreparable",nullptr}};
      const char *parents[]={"slow_wheel","wheel_gun","fuel_rig","engine_stall","cross_thread"};
      int natural=0;
      for(int parent=0;parent<5;parent++)for(int res=0;res<pf3_resolution_count(parent);res++) {
        std::string file="[PitFailures]\nincident_chance_percent = 100.00\n";
        for(int k=0;k<5;k++)file+=std::string(parents[k])+" = "+(k==parent?"100":"0")+"\n";
        for(int k=0;k<pf3_resolution_count(parent);k++)file+=std::string(subkeys[parent][k])+" = "+(k==res?"100":"0")+"\n";
        file+="fuel_fire_chance_percent = 0\n";
        pf3_config_t c{};assert(pit_settings_parse(file,config3,&c,&err));
        pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
        for(unsigned token=1;token<=2000;token++) {
          pf_identity_t id{1,7,token};pf3_plan_t plan{};
          pf3_plan_build(&c,&id,&ctx,seed,&plan);
          assert(plan.status==PF_PLAN_V3_INCIDENT && plan.parent==parent && plan.resolution==res);
          assert(plan.gate_passed && !plan.fire_selected);natural++;
        }
        // Same complete configuration with a zero gate produces no natural incidents.
        c.incident_chance_bp=0;
        for(unsigned token=1;token<=100;token++) {
          pf_identity_t id{1,7,token};pf3_plan_t plan{};pf3_plan_build(&c,&id,&ctx,seed,&plan);
          assert(plan.status==PF_PLAN_V3_CLEAN && !plan.gate_passed);
        }
      }
      assert(natural==28000);
      pf3_config_t fuel=config3;fuel.incident_chance_bp=10000;fuel.fire_chance_bp=10000;
      for(int k=0;k<5;k++)fuel.parent_weight[k]=(k==PF3_PARENT_FUEL_RIG?100:0);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      for(unsigned token=1;token<=2000;token++) {
        pf_identity_t id{1,7,token};pf3_plan_t plan{};pf3_plan_build(&fuel,&id,&ctx,seed,&plan);
        assert(plan.parent==PF3_PARENT_FUEL_RIG && plan.fire_selected);
      }
      // An ineligible fuel-only stop still has no selected incident at 100%.
      ctx.eligibility.fuel_add_liters=0;
      {pf_identity_t id{1,7,1};pf3_plan_t plan{};pf3_plan_build(&fuel,&id,&ctx,seed,&plan);
       assert(plan.status==PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY);}
      for(const std::string &extra:{std::string("incident_chance_override_percent = 100.01\n"),std::string("incident_chance_override_percent = invalid\n"),std::string("incident_chance_override_percent = inherit\nincident_chance_override_percent = 100\n"),std::string("fuel_spill = 101\n"),std::string("fuel_spill = -1\n"),
          std::string("fuel_spill = 1.5\n"),std::string("unknown = 1\n"),
          std::string("fuel_fire_chance_percent = 100.01\n"),
          std::string("fuel_spill = 5\nfuel_spill = 6\n"),
          std::string("fuel_slow_flow = 0\nfuel_delayed_flow = 0\nfuel_interrupted_flow = 0\nfuel_spill = 0\n")}) {
        pf3_config_t untouched=config3;
        assert(!pit_settings_parse(old+extra,config3,&untouched,&err));
        assert(memcmp(&untouched,&config3,sizeof untouched)==0);
      }
      std::puts("PASS 1.1.1 gate endpoints, 28,000 exclusive natural subtype plans, fuel fire endpoints, atomic settings");
    }
    { /* The embedded canonical v3 preset must equal the canonical defaults. */
      pf3_config_t embedded{};
      assert(pf3_config_parse(policy_config_v3,&embedded,nullptr,0)==PF_CONFIG_OK);
      assert(memcmp(&embedded,&config3,sizeof embedded)==0);
      assert(embedded.incident_chance_bp==750);
      assert(strcmp(pf3_policy_version(),"0.4.0-two-stage.3")==0);
      assert(pf3_config_hash(&embedded)[0]!=0);
    }
    { /* Every parent/resolution leaf through the v3 session: resolution,
       * severity, terminal flag and attempts come from the resolution. */
      pf3_session_t *v3=pf3_session_create(&config3,seed);pf3_session_new_epoch(v3,1);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      int leaves=0,terminals=0;
      for(int parent=0;parent<PF3_PARENT_COUNT;parent++)
        for(int resolution=0;resolution<pf3_resolution_count(parent);resolution++) {
          pf3_forcing_t forcing=pf3_forcing_natural();
          forcing.parent=parent;forcing.resolution=resolution;
          pf_identity_t id{1,(uint64_t)(100+leaves),1};pf3_plan_t plan{};int committed=0;
          assert(pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,1,&forcing,&plan,&committed)==0&&committed);
          assert(plan.parent==parent&&plan.resolution==resolution);
          assert(plan.status==PF_PLAN_V3_INCIDENT);
          if(parent==PF3_PARENT_FUEL_RIG)
            assert((int)plan.fuel_subtype==(int)PF_FUEL_SUBTYPE_SLOW_FLOW+resolution);
          if(pf3_resolution_is_terminal(parent,resolution)) {
            assert(plan.terminal==1&&plan.severity==PF_TERMINAL&&plan.attempts==0);
            assert(plan.outcome==(parent==PF3_PARENT_WHEELGUN?PF_OUTCOME_WHEELGUN:PF_OUTCOME_CROSS_THREAD));
            assert(plan_view_v3(plan).terminal==1&&plan_view_v3(plan).severity==PF_TERMINAL);
            assert(plan.terminal_presentation_seconds>=config3.resolution_band[parent][resolution].min);
            terminals++;
          } else {
            assert(plan.terminal==0&&plan.severity==PF_RECOVERABLE);
            assert(plan.attempts==pf3_resolution_attempts(parent,resolution));
            assert(plan_view_v3(plan).attempts==plan.attempts);
            assert(plan.extra_job_seconds>=config3.resolution_band[parent][resolution].min);
            assert(plan.extra_job_seconds<=config3.resolution_band[parent][resolution].max);
          }
          leaves++;
        }
      assert(leaves==14&&terminals==2);
      pf3_session_destroy(v3);
    }
    { /* Natural fuel selection is one authoritative draw; fire stays
       * hypothetical and never mutates the ordinary delay. */
      pf3_session_t *v3=pf3_session_create(&config3,seed);pf3_session_new_epoch(v3,1);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      int fuel=0,fire=0,incidents=0;double at=1.0;
      for(unsigned token=1;token<=20000;token++) {
        pf_identity_t id{1,7,token};pf3_plan_t plan{};int committed=0;
        assert(pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,at,nullptr,&plan,&committed)==0&&committed);
        at+=40.0;
        assert(pf3_session_advance(v3,at,nullptr,0)>=0);
        assert(plan.gate_bp==750 && plan.gate_draw>=0 && plan.gate_draw<10000);
        assert(plan.gate_passed==(plan.gate_draw<750));
        if(plan.status==PF_PLAN_V3_INCIDENT)incidents++;
        if(plan.status!=PF_PLAN_V3_INCIDENT||plan.parent!=PF3_PARENT_FUEL_RIG)continue;
        fuel++;
        assert(plan.resolution==(int)plan.fuel_subtype-(int)PF_FUEL_SUBTYPE_SLOW_FLOW);
        assert(plan.extra_job_seconds>=config3.resolution_band[PF3_PARENT_FUEL_RIG][plan.resolution].min);
        assert(plan.extra_job_seconds<=config3.resolution_band[PF3_PARENT_FUEL_RIG][plan.resolution].max);
        if(plan.fire_selected) {
          fire++;
          assert(plan.fire_supported==0&&plan.fire_suppressed==1);
          assert(plan.fire_extra_seconds>0);
          assert(plan.fire_extra_seconds>=config3.fire_burn.min+config3.fire_inspection.min);
          assert(plan.fire_extra_seconds<=config3.fire_burn.max+config3.fire_inspection.max);
        }
      }
      assert(incidents>=1350&&incidents<=1650);
      assert(fuel>200&&fire>0);
      std::printf("Natural 20,000 eligible stops: %d incidents (%.3f%%), %d fuel faults\n",
                  incidents,100.0*incidents/20000.0,fuel);
      pf3_session_destroy(v3);
    }
    { /* Cancellation and repeated stops through the v3 session. */
      pf3_session_t *v3=pf3_session_create(&config3,seed);pf3_session_new_epoch(v3,1);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      pf3_forcing_t forcing=pf3_forcing_natural();forcing.parent=PF3_PARENT_STALL;
      pf_identity_t id{1,55,1};pf3_plan_t plan{};pf3_plan_t stored{};int committed=0;
      assert(pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,1,&forcing,&plan,&committed)==0&&committed);
      stored=plan;
      assert(pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,1,&forcing,&plan,&committed)==0&&!committed);
      assert(memcmp(&stored,&plan,sizeof plan)==0);
      assert(pf3_session_cancel(v3,&id,2)==0);
      assert(pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,3,&forcing,&plan,&committed)==-1);
      assert(pf3_session_last_error(v3)==PF_SESSION_ERR_TOKEN_CANCELLED);
      pf_identity_t next{1,55,2};
      assert(pf3_session_commit(v3,&next,&ctx,&ctx.eligibility,4,&forcing,&plan,&committed)==0&&committed);
      assert(pf3_session_commit(v3,&next,&ctx,&ctx.eligibility,4,&forcing,&plan,&committed)==0&&!committed);
      pf3_session_destroy(v3);
    }
    { /* v3 commits and drains under the game's x87 precision, guard restored. */
      unsigned short saved, single;
      __asm__ __volatile__("fnstcw %0" : "=m"(saved));
      single=(saved & ~0x0300);
      pf3_session_t *v3=pf3_session_create(&config3,seed);pf3_session_new_epoch(v3,1);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      pf3_forcing_t forcing=pf3_forcing_natural();forcing.parent=PF3_PARENT_WHEELGUN;
      pf_identity_t id{1,9,1};pf3_plan_t plan{};pf_command_t cmd[8];int committed=0;
      __asm__ __volatile__("fldcw %0" : : "m"(single));
      assert(pit_policy_math([&]{return pf3_session_commit(v3,&id,&ctx,&ctx.eligibility,50.25,&forcing,&plan,&committed);})==0&&committed);
      assert(pit_fp_control()==single);
      assert(pit_policy_math([&]{return pf3_session_advance(v3,50.25,cmd,8);})>=0);
      assert(pit_fp_control()==single);
      __asm__ __volatile__("fldcw %0" : : "m"(saved));
      pf3_session_destroy(v3);
    }
    { /* Accepted v2 runtime stays available for rollback. */
      pf_config_t v2cfg;pf_config_defaults(&v2cfg);
      pf_session_t *v2=pf_session_create(&v2cfg,seed);pf_session_new_epoch(v2,1);
      pf_context_t ctx{};ctx.eligibility={1,1,40,1,1};
      pf_identity_t id{1,7,1};pf_plan_t plan{};int committed=0;
      assert(pft_session_commit(v2,&id,&ctx,&ctx.eligibility,1,&plan,&committed,2)==0&&committed);
      assert(plan.outcome==PF_OUTCOME_SLOW_CORNER);
      pf_session_destroy(v2);
    }
    broker.get_stats=fake_stats;broker.get_session_snapshot=fake_session;broker.get_vehicle_handles=fake_handles;broker.get_vehicle_snapshot=fake_snapshot;
    broker.pause_menu_arm=fake_pause_arm;broker.pause_menu_command=fake_pause_command;
    { /* A remote AI gun is inaudible; a watched or nearby car is audible.
       Software PCM scaling must leave the game's device volume untouched. */
      assert(pit_wheel_gain(0)==1 && pit_wheel_gain(6)==1);
      assert(std::fabs(pit_wheel_gain(12)-0.5f)<1e-6f);          /* inverse distance */
      assert(pit_wheel_gain(40)>0 && pit_wheel_gain(40)<0.15f);
      assert(pit_wheel_gain(59.9)>0 && pit_wheel_gain(59.9)<0.001f); /* smooth edge */
      assert(pit_wheel_gain(60)==0 && pit_wheel_gain(-1)==0);
      for(double d=6;d<60;d+=0.5)assert(pit_wheel_gain(d+0.5)<=pit_wheel_gain(d));
      /* The player's own sliders drive the level; nothing is full scale. */
      PitSoundMix plr=pit_sound_mix_parse(
          "[ Sound Options ]\r\nSoundFX Volume=\"0.38537\" // 0.0-1.0\r\n"
          "Player Volume Ratio=\"1.00000\" // x\r\nOther Volume Ratio=\"0.40000\" // x\r\n");
      assert(plr.from_profile && std::fabs(plr.sfx_volume-0.38537f)<1e-5f &&
             plr.player_ratio==1.0f && std::fabs(plr.other_ratio-0.4f)<1e-6f);
      PitSoundMix none=pit_sound_mix_parse("SoundFX Volume=\"7\"\nOther Volume Ratio=\"x\"\n");
      assert(!none.from_profile && none.sfx_volume==0.5f && none.other_ratio==0.5f);
      assert(std::fabs(pit_wheel_level(plr,true,0,false)-0.38537f*0.5f)<1e-5f);
      assert(std::fabs(pit_wheel_level(plr,false,0,false)-0.38537f*0.4f*0.5f)<1e-5f);
      /* Driving past on the straight: masked and far below a box-side stop. */
      float passing=pit_wheel_level(plr,false,20,true);
      assert(passing>0 && passing<pit_wheel_level(plr,false,20,false)*0.25f && passing<0.01f);
      PitSoundMix loud;loud.sfx_volume=1;loud.player_ratio=4;
      assert(pit_wheel_level(loud,true,0,false)==1.0f);            /* clamped */
      int16_t samples[]={1000,-1000};unsigned char input[sizeof samples],output[sizeof samples];
      std::memcpy(input,samples,sizeof samples);
      assert(pit_wheel_scale_pcm16(input,output,sizeof output,0.5f));
      int16_t scaled[2];std::memcpy(scaled,output,sizeof scaled);
      assert(scaled[0]==500 && scaled[1]==-500);
      Car source{};source.snapshot.handle.slot=1;source.snapshot.controller_role=1;
      source.snapshot.valid_fields=F1C_VEH_SNAPSHOT_POSITION;
      source.snapshot.world_x=200;source.snapshot.world_y=0;source.snapshot.world_z=0;
      cars[0].seen=true;cars[0].snapshot.controller_role=0;
      cars[0].snapshot.valid_fields=F1C_VEH_SNAPSHOT_POSITION;
      cars[0].snapshot.world_x=0;cars[0].snapshot.world_y=0;cars[0].snapshot.world_z=0;
      sound_mix=plr;
      audio_listener_slot=0;double distance=0;
      assert(wheel_audio_gain_for(source,&distance)==0 && distance==200);
      source.snapshot.world_x=20;
      float boxside=wheel_audio_gain_for(source,&distance);
      assert(boxside>0 && distance==20);
      cars[0].snapshot.valid_fields|=F1C_VEH_SNAPSHOT_PIT;cars[0].snapshot.pit_status=F1C_PIT_TRACK;
      float trackside=wheel_audio_gain_for(source,&distance);
      assert(trackside>0 && trackside<boxside*0.25f);
      cars[0].snapshot.pit_status=F1C_PIT_SERVICE;
      assert(wheel_audio_gain_for(source,&distance)==boxside);
      audio_listener_slot=1;
      assert(std::fabs(wheel_audio_gain_for(source,&distance)-0.38537f*0.4f*0.5f)<1e-5f && distance==0);
      audio_listener_slot=-1;cars[0]=Car{};sound_mix=PitSoundMix{};
    }
    { /* Wheel-gun audio follows the visible gun at the nut. */
      const float clips[3]={0.907f,0.787f,0.640f}; /* BRRRRT BRRRRT, BRT BRT, BR-RRR */
      assert(pit_gun_pick_wav(clips,0.691f,false)==2);   /* front removal */
      assert(pit_gun_pick_wav(clips,2.140f,false)==0);   /* front refit, long stay */
      assert(pit_gun_pick_wav(clips,0.757f,false)==2);   /* rear removal */
      assert(pit_gun_pick_wav(clips,0.856f,false)==1);   /* rear refit */
      assert(pit_gun_pick_wav(clips,0.2f,false)==2);     /* nothing fits: shortest */
      assert(pit_gun_pick_wav(clips,2.0f,true)==1);      /* faulty gun stutters */
      const float none[3]={0,0,0};assert(pit_gun_pick_wav(none,1,false)==-1);
      const float only_long[3]={0.907f,0,0};assert(pit_gun_pick_wav(only_long,1,true)==0);

      PitGunChannel front;front.found=true;front.start_abs=4.6;front.base_start=2.3f;
      front.base_end=9.8f;front.end_abs=front.start_abs+7.5;
      assert(pit_gun_channel_plausible(front,30.28));
      PitGunChannel absolute=front;absolute.start_abs=34.9;absolute.end_abs=42.4;
      assert(!pit_gun_channel_plausible(absolute,30.28));        /* not the stop clock */
      PitGunChannel negative=front;negative.start_abs=-1;negative.end_abs=6.5;
      assert(!pit_gun_channel_plausible(negative,30.28));
      PitGunChannel bad=front;bad.end_abs+=1;assert(!pit_gun_channel_plausible(bad,100.0));
      bad=front;bad.base_start=NAN;assert(!pit_gun_channel_plausible(bad,100.0));
      double at[2];float dwell[2];
      assert(pit_gun_contact_times(front,0,30.28,at,dwell));
      assert(std::fabs(at[0]-(30.28+4.6+3.193-2.3))<1e-4 && std::fabs(at[1]-(30.28+4.6+6.649-2.3))<1e-4);
      assert(dwell[0]==PIT_GUN_FRONT[0].dwell && dwell[1]==PIT_GUN_FRONT[1].dwell);
      PitGunChannel rear=front;rear.base_start=1.0f;rear.base_end=10.4f;rear.end_abs=rear.start_abs+9.4;
      assert(pit_gun_contact_times(rear,3,30.28,at,dwell) && std::fabs(at[0]-(30.28+4.6+0.975))<1e-4);
      assert(!pit_gun_contact_times(front,4,30.28,at,dwell));
      PitGunChannel late=front;late.base_start=4.0f;assert(!pit_gun_contact_times(late,0,30.28,at,dwell));

      /* Reader against a synthetic crew with the reviewed layout. */
      static unsigned char vehicle_mem[0x3400]{};static unsigned char crew_mem[0x20]{};
      static unsigned char actors[6*120]{};static float windows[6][7]{};
      static const char *names[6]={"TMAN","WHEELGUN_FRONT_LEFT","WheelGun_Front_Right",
                                   "WHEELGUN_REAR_LEFT","WHEELGUN_REAR_RIGHT","WHEELON_FRONT_LEFT"};
      const float bases[6]={2.0f,2.3f,2.5f,1.0f,1.0f,2.3f};
      for(int i=0;i<6;i++) {
          windows[i][0]=4.6f;windows[i][3]=bases[i];windows[i][4]=bases[i]+7.5f;
          windows[i][1]=windows[i][0]+7.5f;windows[i][2]=1/7.5f;
          uint32_t name=(uint32_t)(uintptr_t)names[i],channels=1,channel=(uint32_t)(uintptr_t)windows[i];
          std::memcpy(actors+i*120+100,&name,4);std::memcpy(actors+i*120+112,&channels,4);
          std::memcpy(actors+i*120+116,&channel,4);
      }
      uint32_t crew_ptr=(uint32_t)(uintptr_t)crew_mem,actor_ptr=(uint32_t)(uintptr_t)actors,count=6;
      std::memcpy(vehicle_mem+0x32d4,&crew_ptr,4);std::memcpy(crew_mem+0x0c,&count,4);
      std::memcpy(crew_mem+0x10,&actor_ptr,4);
      PitGunChannel read_back[4];
      assert(pit_gun_channels_read(native_read,(uintptr_t)vehicle_mem,read_back)==4);
      assert(read_back[1].found && read_back[1].base_start==2.5f && read_back[3].base_start==1.0f);
      static unsigned char no_crew[0x3400]{};
      assert(pit_gun_channels_read(native_read,(uintptr_t)no_crew,read_back)==0);

      /* Sync replaces the measured schedule; a wheel-gun fault stutters. */
      Car gun{};gun.token=9;gun.gun_audio=true;gun.start=100.0;
      gun.plan.outcome=PF_OUTCOME_WHEELGUN;gun.plan.wheel=0;gun.reveal_at=110;
      for(int c=0;c<4;c++){gun.gun_off_at[c]=104;gun.gun_on_at[c]=106;}
      double saved_now=now;now=100.6;
      try_sync_gun_audio(gun,vehicle_mem);
      assert(gun.gun_synced && std::fabs(gun.gun_off_at[0]-(100.0+4.6+0.893))<1e-3);
      assert(std::fabs(gun.gun_on_at[3]-(100.0+4.6+5.09))<1e-3 && gun.reveal_at==gun.gun_off_at[0]);
      sfx_requests.clear();
      now=gun.gun_on_at[0];pump_synced_gun_audio(gun);
      /* Contacts that passed more than 0.3 s ago are dropped, not played late:
       * only the FL refit (due now) and FR refit (0.2 s ago) remain. */
      assert(sfx_requests.size()==2 && sfx_requests[0].corner==0 && sfx_requests[1].corner==1);
      assert(gun.gun_off_done[0] && gun.gun_off_done[2] && !gun.gun_on_done[2]);
      Car late_sync{};late_sync.token=9;late_sync.gun_audio=true;late_sync.start=100.0;
      for(int c=0;c<4;c++){late_sync.gun_off_at[c]=101;late_sync.gun_on_at[c]=103;}
      now=101.0;try_sync_gun_audio(late_sync,no_crew);
      assert(!late_sync.gun_synced && late_sync.gun_sync_closed);
      sfx_requests.clear();now=saved_now;

      /* Lollipop: the UP channel (offset 0, long) gives the 1ST GEAR moment. */
      static unsigned char lolly_vehicle[0x3400]{};static unsigned char lolly_crew[0x20]{};
      static unsigned char lolly_actors[2*120]{};static float lolly_ch[2][7]{};static float lman_ch[7]{};
      static const char *lolly_names[2]={"LMan","Lollipop"};
      lolly_ch[0][0]=0;lolly_ch[0][1]=0;lolly_ch[0][3]=5.0f;lolly_ch[0][4]=5.0f;       /* DOWN */
      lolly_ch[1][0]=9.4f;lolly_ch[1][1]=9.4f+7.9f;lolly_ch[1][3]=0.0f;lolly_ch[1][4]=7.9f; /* UP */
      lman_ch[0]=9.4f;lman_ch[1]=17.3f;lman_ch[3]=0.0f;lman_ch[4]=7.9f;
      for(int i=0;i<2;i++) {
          uint32_t name=(uint32_t)(uintptr_t)lolly_names[i],channels=i?2:1;
          uint32_t channel=(uint32_t)(uintptr_t)(i?(void*)lolly_ch:(void*)lman_ch);
          std::memcpy(lolly_actors+i*120+100,&name,4);std::memcpy(lolly_actors+i*120+112,&channels,4);
          std::memcpy(lolly_actors+i*120+116,&channel,4);
      }
      uint32_t lcrew=(uint32_t)(uintptr_t)lolly_crew,lactors=(uint32_t)(uintptr_t)lolly_actors,lcount=2;
      std::memcpy(lolly_vehicle+0x32d4,&lcrew,4);std::memcpy(lolly_crew+0x0c,&lcount,4);
      std::memcpy(lolly_crew+0x10,&lactors,4);
      PitGunChannel lolly;
      assert(pit_lollipop_channel_read(native_read,(uintptr_t)lolly_vehicle,&lolly));
      assert(std::fabs(lolly.start_abs-9.4)<1e-4 && std::fabs(lolly.base_end-7.9)<1e-4);       /* UP, not DOWN, not LMan */
      assert(std::fabs(pit_lollipop_gear_time(lolly,30.0)-(30.0+9.4+0.165))<1e-4);
      PitGunChannel wrong=lolly;wrong.end_abs+=1;assert(pit_lollipop_gear_time(wrong,30.0)<0);
      wrong=lolly;wrong.start_abs=-2;assert(pit_lollipop_gear_time(wrong,30.0)<0);
      assert(!pit_lollipop_channel_read(native_read,(uintptr_t)no_crew,&lolly));
      assert(f1c_pit_voice_allowed(44));                             /* "Gear" line allowed */
      assert(f1c_pit_voice_allowed(46));                             /* "Box! Pit now" allowed */
      { /* Front of queue, but pit-procedure calls keep their own order. */
        auto grid=[](double clock,double delay){return std::trunc((clock+delay)*5.0)*0.2;};
        float due[20];int ids[20];for(int i=0;i<20;i++){due[i]=-1.0f;ids[i]=-1;}
        double delay=f1c_pit_voice_front_delay(due,ids,100.0);
        assert(grid(100.0,delay)<100.0 && grid(100.0,delay)>99.5);   /* empty: due now */
        due[3]=101.0f;ids[3]=190;due[7]=99.4f;ids[7]=120;due[9]=NAN;ids[9]=7;
        delay=f1c_pit_voice_front_delay(due,ids,100.0);
        assert(grid(100.0,delay)<99.4-1e-6);                          /* ahead of other chatter */
        due[5]=99.8f;ids[5]=46;                                       /* "Box!" still pending */
        delay=f1c_pit_voice_front_delay(due,ids,100.0);
        assert(grid(100.0,delay)>99.8+1e-6);                          /* BRAKE stays behind Box */
        due[5]=-1.0f;due[7]=50.0f;
        assert(f1c_pit_voice_front_delay(due,ids,100.0)==-10);        /* bounded */
        assert(f1c_pit_voice_lifetime(44)==1.0f && f1c_pit_voice_lifetime(46)==2.0f &&
               f1c_pit_voice_lifetime(53)==2.0f && f1c_pit_voice_lifetime(61)==4.0f &&
               f1c_pit_voice_lifetime(117)==20.0f);
      }

      /* Pit-box position from a fake AIW pit-slot table. */
      struct FakeAiw {
          static int read(uintptr_t a,void *o,size_t n) {
              static float slots[52*7]{};static bool init=false;
              if(!init){slots[3*7+0]=120.5f;slots[3*7+1]=2.0f;slots[3*7+2]=-40.25f;init=true;}
              static uint32_t array=0x20000000u,count=52;
              if(a==0x005EA2C0u+0x154 && n==4){std::memcpy(o,&array,4);return 1;}
              if(a==0x005EA2C0u+0x164 && n==4){std::memcpy(o,&count,4);return 1;}
              if(a>=array && a+n<=array+sizeof slots){std::memcpy(o,(unsigned char*)slots+(a-array),n);return 1;}
              return 0;
          }
      };
      float box[3];
      assert(f1c_pit_box_position(FakeAiw::read,3,box) && box[0]==120.5f && box[2]==-40.25f);
      assert(!f1c_pit_box_position(FakeAiw::read,4,box));             /* unauthored: all zero */
      assert(!f1c_pit_box_position(FakeAiw::read,-1,box) && !f1c_pit_box_position(FakeAiw::read,52,box));
    }
    { /* Old wheel-gun sequence: corner line first, generic gun line later. */
      assert(PIT_LIVE_ENABLE_CUSTOM_WAV==1);
      Car wheel{};wheel.token=1;wheel.mechanic_started=true;
      wheel.plan.outcome=PF_OUTCOME_WHEELGUN;wheel.plan.wheel=2;
      wheel.snapshot.controller_role=0;wheel.snapshot.handle.slot=0;
      strcpy(wheel.snapshot.driver_name,"Wheel driver");
      pf_command_t notice{};notice.kind=PF_CMD_FAULT_NOTICE;
      now=500;int before=voice_count;deliver_feedback(wheel,notice);
      assert(voice_count==before+1 && voice_eids[before]==68);
      now=502.9;pump_presentation(wheel,test_vehicles[0],true);
      assert(voice_count==before+1);
      now=503.0;pump_presentation(wheel,test_vehicles[0],true);
      assert(voice_count==before+2 && voice_eids[before+1]==61);
      now=504;pump_presentation(wheel,test_vehicles[0],true);
      assert(voice_count==before+2);
      voice_count=0;events.clear();
    }
    { /* Early policy notice must survive until the AI engine really stops;
         a busy native voice queue is retried, then the cue is sent only once. */
      Car stall{};stall.token=1;stall.ready=true;
      stall.plan.outcome=PF_OUTCOME_STALL_DELAY;
      stall.snapshot.controller_role=1;stall.snapshot.handle.slot=1;
      strcpy(stall.snapshot.driver_name,"Stall driver");
      pf_command_t notice{};notice.kind=PF_CMD_FAULT_NOTICE;
      int before=voice_count,stops_before=stops;
      now=600;feedback(stall,notice);
      assert(stall.stall_notice_pending && !stall.stall_notice_announced);
      assert(voice_count==before);
      test_voice_busy_eid=184;test_voice_busy_attempts=1;
      stall.reveal_at=601;now=601;
      pump_presentation(stall,test_vehicles[1],true);
      assert(stops>=stops_before+1 && stall.stalled);
      assert(stall.stall_notice_pending && !stall.stall_notice_announced);
      assert(voice_count==before);
      now=601.25;pump_presentation(stall,test_vehicles[1],true);
      assert(voice_count==before);
      now=601.5;pump_presentation(stall,test_vehicles[1],true);
      assert(stall.stall_notice_announced && !stall.stall_notice_pending);
      assert(voice_count==before+1 && voice_eids[before]==184);
      feedback(stall,notice);
      assert(voice_count==before+1 && !stall.stall_notice_pending);
      test_voice_busy_eid=-1;test_voice_busy_attempts=0;
      voice_count=0;events.clear();
    }
    test_snapshot.valid_fields=F1C_VEH_SNAPSHOT_PIT|F1C_VEH_SNAPSHOT_FIELD_STATUS|F1C_VEH_SNAPSHOT_NAMES;
    test_snapshot.snapshot_sequence=10;test_snapshot.controller_role=0;strcpy(test_snapshot.driver_name,"Test driver");
    auto *pit=f1c_pit_machine(test_vehicles[0]);
    test_clock=100;update(nullptr);assert(active && epoch==1);
    { /* The same player/object must be able to request XPITIN again after a
         completed 1->2->3->4->0 pit cycle in the same race epoch. */
      auto count_pit03=[](){
          int n=0;
          for(int i=0;i<voice_count;i++) {
              if(voice_eids[i]==50)n++;
          }
          return n;
      };
      int before=count_pit03();
      test_snapshot.pit_status=1;pit->status=1;test_clock=100.001;update(nullptr);
      VEHF_PitEntryLatch(test_vehicles[0])=1;test_clock=100.002;update(nullptr);
      /* Reaching XPITIN arms the line; it is queued on the next tick so a
       * same-tick LIMITER ON goes first. */
      assert(count_pit03()==before && cars[0].pit_entry_triggered && cars[0].pit_entry_pending);
      test_clock=100.0025;update(nullptr);
      assert(count_pit03()==before+1 && cars[0].pit_entry_announced);
      test_clock=100.0027;update(nullptr);assert(count_pit03()==before+1);
      VEHF_PitEntryLatch(test_vehicles[0])=0;
      test_snapshot.pit_status=2;pit->status=2;test_clock=100.003;update(nullptr);
      test_snapshot.pit_status=3;pit->status=3;test_clock=100.004;update(nullptr);
      test_snapshot.pit_status=4;pit->status=4;test_clock=100.005;update(nullptr);
      test_snapshot.pit_status=0;pit->status=0;VEHF_CallToBox(test_vehicles[0])=0;
      test_clock=100.006;update(nullptr);
      assert(!cars[0].pit_entry_triggered && !cars[0].pit_entry_announced);
      test_snapshot.pit_status=1;pit->status=1;test_clock=100.007;update(nullptr);
      VEHF_PitEntryLatch(test_vehicles[0])=1;test_clock=100.008;update(nullptr);
      test_clock=100.0082;update(nullptr);
      assert(count_pit03()==before+2 && cars[0].pit_entry_triggered &&
             cars[0].pit_entry_announced &&
             std::fabs(cars[0].pit_entry_voice_queued_at-100.0082)<0.001);
      test_clock=100.0085;update(nullptr);assert(count_pit03()==before+2);
      VEHF_PitEntryLatch(test_vehicles[0])=0;
      test_snapshot.pit_status=0;pit->status=0;test_clock=100.009;update(nullptr);
      assert(!cars[0].pit_entry_triggered && !cars[0].pit_entry_announced);
      voice_count=0;events.clear();
    }
    { /* A retirement transition must never emit Go/limiter-off while the
       * game's PLAYER_OUT gate is active, even before the snapshot retires. */
      int before=voice_count;test_snapshot.pit_status=3;pit->status=3;test_clock=100.01;update(nullptr);
      test_retire_player_out=1;test_snapshot.pit_status=4;pit->status=4;test_clock=100.02;update(nullptr);
      test_snapshot.pit_status=0;pit->status=0;test_clock=100.03;update(nullptr);
      assert(voice_count==before);test_retire_player_out=0;
    }
    {std::string st=status();
     assert(st.find("\"policy_model\":3")!=std::string::npos);
     assert(st.find("\"failure_resolutions\"")!=std::string::npos);
     assert(st.find("\"fire_available\":false")!=std::string::npos);
     assert(st.find("\"slow_wheel\"")!=std::string::npos);
     assert(st.find("\"policy_version\":\"0.4.0-two-stage.3\"")!=std::string::npos);
     assert(st.find("\"config_hash\":\""+std::string(pf3_config_hash(&config3))+"\"")!=std::string::npos);
     assert(st.find("\"seed\":\"20260921\"")!=std::string::npos);
     assert(st.find("\"gate_bp\":750")!=std::string::npos);}
    std::string selected_command=command();selected_command.pop_back();selected_command+=",\"failure\":\"slow_wheel\"}";
    assert(submit(selected_command).find("accepted")!=std::string::npos);
    assert(submit(selected_command).find("Already received")!=std::string::npos);
    assert(submit(command()).find("rejected")!=std::string::npos);
    assert(submit(command("stale","999")).find("Stale")!=std::string::npos);
    {std::string no_parent=command("no-parent");no_parent.pop_back();no_parent+=",\"failure_resolution\":\"minor\"}";
     assert(submit(no_parent).find("requires a selected forced failure")!=std::string::npos);
     std::string wrong_leaf=command("wrong-leaf");wrong_leaf.pop_back();wrong_leaf+=",\"failure\":\"wheelgun\",\"failure_resolution\":\"minor\"}";
     assert(submit(wrong_leaf).find("resolution unavailable")!=std::string::npos);}
    assert(VEHF_CallToBox(test_vehicles[0])==0);
    update(nullptr);assert(VEHF_CallToBox(test_vehicles[0])==1);assert(requests.at("test-1").state=="armed");
    { /* Reaching XPITIN arms the line for the next tick (a same-tick LIMITER
         ON goes first); a busy native queue retries without consuming the
         one-shot announcement. */
      int before=voice_count;
      test_voice_busy_eid=50;test_voice_busy_attempts=1;
      VEHF_PitEntryLatch(test_vehicles[0])=1;
      test_clock=100.05;update(nullptr);
      assert(voice_count==before && cars[0].pit_entry_pending);
      assert(std::fabs(cars[0].pit_entry_ack_at-100.05)<0.001 && cars[0].pit_entry_call_active);
      assert(cars[0].pit_entry_voice_result==0 && cars[0].pit_entry_voice_queued_at<0);
      test_clock=100.06;update(nullptr);assert(voice_count==before);   /* first try: busy */
      test_clock=100.35;update(nullptr);assert(voice_count==before);   /* 0.5 s retry spacing */
      test_clock=100.56;update(nullptr);
      assert(voice_count==before+1 && voice_eids[before]==50);
      assert(!cars[0].pit_entry_pending && cars[0].pit_entry_announced);
      assert(std::fabs(cars[0].pit_entry_voice_queued_at-100.56)<0.001 && cars[0].pit_entry_voice_result==1);
      test_clock=100.57;update(nullptr);assert(voice_count==before+1);
      test_voice_busy_eid=-1;
      VEHF_PitEntryLatch(test_vehicles[0])=0;
    }
    pit->status=3;test_snapshot.pit_status=3;pit->service_ready=1;
    pit->stop_start_time=100;pit->stop_end_time=110;pit->tire_change_active=1;pit->tire_service_end_time=109;
    test_clock=100.6;update(nullptr);
    assert(cars[0].plan.outcome==PF_OUTCOME_SLOW_CORNER);assert(requests.at("test-1").state=="consumed");
    assert(voice_count==1); /* Pit approach announced; incident remains silent. */
    {int before=voice_count;update(nullptr);assert(voice_count==before);}
    assert(cars[0].holding && cars[0].target>110);assert(pit->stop_end_time==110);
    assert(cars[0].reveal_at>=cars[0].gun_off_at[cars[0].plan.wheel]-0.001);
    assert(cars[0].reveal_at<cars[0].gun_off_at[cars[0].plan.wheel]+0.4);
    test_clock=cars[0].reveal_at+0.01;update(nullptr);
    assert(voice_count==2&&!sfx_requests.empty()); /* Native animation window. */
    auto target=cars[0].target;update(nullptr);assert(cars[0].token==1 && cars[0].target==target);
    pit->service_ready=0;update(nullptr);pit->service_ready=1;update(nullptr);
    assert(cars[0].token==1 && cars[0].plan.outcome==PF_OUTCOME_SLOW_CORNER);
    cars[0].holding=true; /* Continue independent hold test after simulated latch loss. */
    test_clock=110.1;update(nullptr);assert(pit->stop_end_time>110 && pit->stop_end_time<=test_clock+0.501);
    float held=pit->stop_end_time;test_object=2;test_clock=110.2;update(nullptr);assert(!cars[0].holding && pit->stop_end_time==held);
    assert(cars[0].token==0); /* Late attachment/replacement never commits existing ready service. */
    {unsigned short saved=pit_fp_control(), game=saved & ~0x0300u;
     pit_fp_restore(game);
     // A later stop in the actual adapter, under the game thread's precision.
     pit->service_ready=0;pit->status=0;test_snapshot.pit_status=0;test_clock=250;update(nullptr);
     std::string next=command("second-stop","2");next.pop_back();next+=",\"failure\":\"slow_wheel\"}";
     assert(submit(next).find("accepted")!=std::string::npos);update(nullptr);
     pit->service_ready=1;pit->status=3;test_snapshot.pit_status=3;
     pit->stop_start_time=250;pit->stop_end_time=260;pit->tire_service_end_time=259;
     test_clock=250.123456789;int voices=voice_count;update(nullptr);
     assert(active && !invalid_clock && requests.at("second-stop").state=="consumed");
     assert(voice_count==voices && cars[0].holding);
     test_clock=cars[0].reveal_at+0.01;update(nullptr);assert(voice_count>voices);
     update(nullptr);assert(active && !invalid_clock);
     assert(pit_fp_control()==game);pit_fp_restore(saved);
     held=pit->stop_end_time;
    }
    test_clock=99;update(nullptr);assert(invalid_clock && !active && pit->stop_end_time==held);
    test_epoch=2;test_clock=0;update(nullptr);assert(active && !invalid_clock);
    {Car ai=cars[0];ai.snapshot.controller_role=1;VEHF_DriverID(test_vehicles[0])=17;
     int before=voice_count;voice(ai,183);assert(voice_count==before+1);
     assert(voice_eids[before]==183 && voice_subjects[before]==17);
     ai.snapshot.controller_role=0;voice(ai,51);assert(voice_subjects[before+1]==-1);
    }
    { // Real adapter lifecycle with mocked native mechanics, not just voice maps.
      test_epoch=1;test_object=3;test_snapshot.controller_role=1;
      test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;test_clock=900;update(nullptr);
      std::string arm=command("stall-ai","3");arm.pop_back();arm+=",\"failure\":\"stall\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=900;pit->stop_end_time=910;pit->tire_service_end_time=909;
      test_clock=900.1;update(nullptr);assert(!cars[0].stalled);
      test_clock=cars[0].reveal_at+0.01;update(nullptr);
      assert(cars[0].stalled && stops>0 && VEHF_EngineRPM(test_vehicles[0])==0);
      int before=starts;test_clock=cars[0].target+0.01;update(nullptr);
      assert(!cars[0].stalled && starts==before+1 && VEHF_EngineRPM(test_vehicles[0])>0);
      /* The player path owns the driver-confirmed ignition flag and restores
       * its captured value at recovery. */
      test_object=40;test_snapshot.controller_role=0;test_snapshot.pit_status=0;
      pit->status=0;pit->service_ready=0;test_player_ignition=1;
      test_player_physics_rpm=900;test_player_idle_rpm=900;test_clock=930;update(nullptr);
      arm=command("stall-player","40");arm.pop_back();arm+=",\"failure\":\"stall\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=930;pit->stop_end_time=940;pit->tire_service_end_time=939;
      test_clock=930.1;update(nullptr);assert(test_player_ignition==1&&!cars[0].stalled);
      test_clock=cars[0].reveal_at+0.01;update(nullptr);
      VEHF_EngineRPM(test_vehicles[0])=0;
      assert(test_player_ignition==0&&cars[0].stalled&&cars[0].player_ignition_owned);
      int player_restart_voice_before=voice_count;
      auto restart_messages=[](){int n=0;for(const auto &e:events)if(e.find("ENGINE RESTARTED")!=std::string::npos)n++;return n;};
      int player_restart_message_before=restart_messages();
      test_clock=cars[0].target+0.01;update(nullptr);
      assert(test_player_ignition==1);
      assert(cars[0].stalled&&cars[0].player_ignition_owned&&cars[0].player_restart_pending);
      assert(voice_count==player_restart_voice_before);
      assert(restart_messages()==player_restart_message_before);
      /* Native pit-exit logic may erase the first starter write.  The adapter
       * must retry and may not announce recovery from its own write. */
      test_player_ignition=0;test_player_physics_rpm=0;VEHF_EngineRPM(test_vehicles[0])=0;
      test_clock+=0.05;update(nullptr);
      assert(test_player_ignition==1&&cars[0].stalled&&cars[0].player_restart_pending);
      assert(test_player_physics_rpm==test_player_idle_rpm);
      assert(voice_count==player_restart_voice_before);
      assert(restart_messages()==player_restart_message_before);
      /* A later authoritative vehicle snapshot proves that the engine caught. */
      VEHF_EngineRPM(test_vehicles[0])=850;
      test_clock+=0.05;update(nullptr);
      assert(!cars[0].stalled&&!cars[0].player_ignition_owned&&
             !cars[0].player_restart_pending&&cars[0].player_restart_announced);
      assert(restart_messages()==player_restart_message_before+1);
      int player_restart_voice_after=voice_count,player_restart_message_after=restart_messages();
      test_clock+=0.05;update(nullptr);
      assert(voice_count==player_restart_voice_after);
      assert(restart_messages()==player_restart_message_after);
      /* If the proven ignition write still cannot produce real RPM, the
       * adapter must not leave a false-running car in permanent limbo. */
      { Car stuck{};stuck.snapshot.controller_role=0;strcpy(stuck.snapshot.driver_name,"Stuck player");
        stuck.stalled=true;stuck.player_ignition_owned=true;stuck.player_ignition_restore=1;
        test_player_ignition=0;test_player_physics_rpm=0;now=1000;restart_owned(stuck);
        assert(stuck.player_restart_pending&&!stuck.terminal&&test_player_ignition==1);
        assert(test_player_physics_rpm==test_player_idle_rpm);
        now=1003.01;restart_owned(stuck);
        assert(stuck.terminal&&stuck.holding&&!stuck.player_restart_pending&&test_player_ignition==0);
        assert(stuck.player_retire_voice_queued&&stuck.terminal_at==now+5.0);
      }
      test_object=4;test_snapshot.controller_role=1;test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;test_clock=950;update(nullptr);
      arm=command("retire-ai","4");arm.pop_back();arm+=",\"failure\":\"wheelgun\",\"failure_resolution\":\"irreparable_nut\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=950;pit->stop_end_time=960;pit->tire_service_end_time=959;
      test_clock=950.1;update(nullptr);
      printf("terminal fixture: %s\n",status().c_str());fflush(stdout);
      assert(cars[0].terminal && cars[0].holding && retires==0);
      assert(cars[0].plan3.parent==PF3_PARENT_WHEELGUN && cars[0].plan3.resolution==PF3_RES_WHEELGUN_IRREPARABLE);
      assert(cars[0].plan3.severity==PF_TERMINAL && cars[0].plan.outcome==PF_OUTCOME_WHEELGUN);
      {int before_voice=voice_count;test_clock=cars[0].reveal_at+0.01;update(nullptr);
       assert(voice_count==before_voice+1&&retires==0);}
      test_clock=cars[0].terminal_at+0.01;update(nullptr);
      assert(cars[0].retirement_requested && !cars[0].holding && retires==1);
      update(nullptr);assert(retires==1);
      // No announcement at native request, before retirement, or before garage proof.
      int announced=voice_count;F1CVehicleLifecycle life{};
      life.valid=life.garage_valid=life.garage_clear=1;life.finish=2;
      observe_retirement(cars[0],life);assert(voice_count==announced);
      cars[0].snapshot.retired=1;life.garage_clear=0;
      observe_retirement(cars[0],life);assert(voice_count==announced);
      life.garage_clear=1;life.ai_control=1;
      observe_retirement(cars[0],life);assert(voice_count==announced);
      life.ai_control=0;VEHF_DriverID(test_vehicles[0])=59;
      cars[0].snapshot.controller_role=-1; // Native garaged AI is no longer an active controller.
      observe_retirement(cars[0],life);
      assert(cars[0].garaged && voice_count==announced+1);
      assert(voice_eids[announced]==237 && voice_subjects[announced]==59);
      observe_retirement(cars[0],life);assert(voice_count==announced+1);
      assert(f1c_pit_voice_allowed(237) && !f1c_pit_voice_allowed(235));
      assert(f1c_pit_voice_context(237)==3 && f1c_pit_voice_lifetime(237)==20.0f);
      assert(f1c_pit_voice_lifetime(117)==20.0f);
      assert(f1c_pit_voice_context(183)==-1 && f1c_pit_voice_lifetime(183)==4.0f);

      /* Player terminal leaves use the old mod's broker handoff and the
       * player-specific retirement line, then wait for the worker sequence. */
      test_object=41;test_snapshot.controller_role=0;test_snapshot.retired=0;
      test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;test_clock=980;update(nullptr);
      arm=command("retire-player","41");arm.pop_back();arm+=",\"failure\":\"wheelgun\",\"failure_resolution\":\"irreparable_nut\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=980;pit->stop_end_time=990;pit->tire_service_end_time=989;
      test_clock=980.1;update(nullptr);assert(cars[0].terminal&&cars[0].holding);
      int before_retire_voice=voice_count,before_arms=menu_arms,before_commands=menu_commands;
      test_clock=cars[0].reveal_at+0.01;update(nullptr);
      const int corner_lines[]={63,65,68,70};
      assert(voice_count==before_retire_voice+1);
      assert(voice_eids[before_retire_voice]==corner_lines[cars[0].plan.wheel]);
      test_clock+=3.01;update(nullptr);
      assert(voice_count==before_retire_voice+2 && voice_eids[before_retire_voice+1]==61);
      test_clock=cars[0].terminal_at+0.01;update(nullptr);
      assert(cars[0].retirement_requested&&!cars[0].holding);
      assert(menu_arms==before_arms+1&&menu_commands==before_commands+1);
      assert(f1c_pit_voice_allowed(117));
      assert(voice_count==before_retire_voice+3&&voice_eids[before_retire_voice+2]==117);
      assert(player_retire_pending==1&&player_retire_generation==test_epoch);
      int before_disarms=menu_disarms;
      /* 1.0.8: the retire notify path must not write any vehicle/retire field
       * or the SessionType from the client thread. Snapshot the old retire
       * write-set and PLAYER_OUT. */
      int32_t veh_condition=*(int32_t*)(test_vehicles[0]+0x3D6C);
      int32_t veh_reason=*(int32_t*)(test_vehicles[0]+0x4068);
      int32_t veh_finish=*(int32_t*)(test_vehicles[0]+0x4064);
      int32_t veh_garage_a=*(int32_t*)(test_vehicles[0]+0x4488);
      int32_t veh_garage_b=*(int32_t*)(test_vehicles[0]+0x4578);
      unsigned char veh_hidden=test_vehicles[0][0x105D];
      test_retire_player_out=0;
      F1CBrokerEvent retire_event{};retire_event.type=F1C_BROKER_EVENT_PAUSE_MENU;retire_event.value=F1C_BROKER_MENU_RETIRE_PITS;
      /* Older broker: it echoes RETIRE_PITS without the native route. 1.0.8
       * releases the arm and refuses; NO cross-thread retirement. */
      handle_broker_event(retire_event);
      assert(player_retire_pending==0&&menu_disarms==before_disarms+1);
      assert(test_retire_player_out==0);
      assert(test_vehicles[0][0x105D]==veh_hidden);
      assert(*(int32_t*)(test_vehicles[0]+0x3D6C)==veh_condition);
      assert(*(int32_t*)(test_vehicles[0]+0x4068)==veh_reason);
      assert(*(int32_t*)(test_vehicles[0]+0x4064)==veh_finish);
      assert(*(int32_t*)(test_vehicles[0]+0x4488)==veh_garage_a);
      assert(*(int32_t*)(test_vehicles[0]+0x4578)==veh_garage_b);
      /* Lightloader >=2.2.3: the broker retires natively and reports CONTINUE;
       * PSF releases its arm and writes nothing. */
      player_retire_pending=1;player_retire_generation=test_epoch;
      int d0=menu_disarms;
      F1CBrokerEvent cont{};cont.type=F1C_BROKER_EVENT_PAUSE_MENU;cont.value=F1C_BROKER_MENU_CONTINUE;
      handle_broker_event(cont);
      assert(player_retire_pending==0&&menu_disarms==d0+1);
      handle_broker_event(cont);           /* not pending: no further disarm */
      assert(menu_disarms==d0+1);
      assert(test_retire_player_out==0);
      assert(test_vehicles[0][0x105D]==veh_hidden);
      assert(*(int32_t*)(test_vehicles[0]+0x3D6C)==veh_condition);
      assert(*(int32_t*)(test_vehicles[0]+0x4064)==veh_finish);
      assert(*(int32_t*)(test_vehicles[0]+0x4488)==veh_garage_a);
      assert(*(int32_t*)(test_vehicles[0]+0x4578)==veh_garage_b);
      assert(pit_required_broker_capabilities() & F1C_BROKER_CAP_GARAGE_RESULTS);
      /* The broker writes the retire set on the game thread; simulate its
       * result for the shared icon policy (PSF itself wrote none of it). */
      *(int32_t*)(test_vehicles[0]+0x4064)=2;   /* FinishState retired */
      BrokerGarageResultsContext garage{56,7,3,5,
          2,"MONITORPAGE",test_vehicles[0]};
      assert(broker_garage_results_eligible(&garage));
      garage.page="MONITOR MAXIMIZED";
      assert(broker_garage_results_eligible(&garage));

      /* A stale RETIRE_PITS after leaving the race must not write anything. */
      int32_t stale_condition=*(int32_t*)(test_vehicles[0]+0x3D6C);
      int32_t stale_reason=*(int32_t*)(test_vehicles[0]+0x4068);
      int32_t stale_finish=*(int32_t*)(test_vehicles[0]+0x4064);
      int32_t stale_garage_a=*(int32_t*)(test_vehicles[0]+0x4488);
      int32_t stale_garage_b=*(int32_t*)(test_vehicles[0]+0x4578);
      unsigned char stale_hidden=test_vehicles[0][0x105D];
      player_retire_pending=1;player_retire_generation=test_epoch;
      d0=menu_disarms;
      handle_broker_event(retire_event);
      assert(player_retire_pending==0&&menu_disarms==d0+1);
      assert(test_vehicles[0][0x105D]==stale_hidden);
      assert(*(int32_t*)(test_vehicles[0]+0x3D6C)==stale_condition);
      assert(*(int32_t*)(test_vehicles[0]+0x4068)==stale_reason);
      assert(*(int32_t*)(test_vehicles[0]+0x4064)==stale_finish);
      assert(*(int32_t*)(test_vehicles[0]+0x4488)==stale_garage_a);
      assert(*(int32_t*)(test_vehicles[0]+0x4578)==stale_garage_b);

      /* A player stall that cannot regain real RPM escalates once.  After the
       * garage command, it must never restart the recovery timer or replay
       * either retirement line. */
      test_object=42;test_snapshot.controller_role=0;test_snapshot.retired=0;
      test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;
      test_player_ignition=1;VEHF_EngineRPM(test_vehicles[0])=900;test_clock=1020;update(nullptr);
      arm=command("stall-terminal-player","42");arm.pop_back();arm+=",\"failure\":\"stall\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1020;pit->stop_end_time=1030;pit->tire_service_end_time=1029;
      test_clock=1020.1;update(nullptr);
      test_clock=cars[0].reveal_at+0.01;update(nullptr);
      VEHF_EngineRPM(test_vehicles[0])=0;
      test_clock=cars[0].target+0.01;update(nullptr);
      assert(cars[0].player_restart_pending&&!cars[0].retirement_requested);
      int stall_retire_commands=menu_commands,stall_retire_voices=voice_count;
      test_clock+=3.01;update(nullptr);
      assert(cars[0].terminal&&!cars[0].retirement_requested&&cars[0].player_retire_voice_queued);
      assert(menu_commands==stall_retire_commands&&voice_count==stall_retire_voices+1);
      test_clock+=5.01;update(nullptr);
      assert(cars[0].retirement_requested&&!cars[0].holding);
      assert(menu_commands==stall_retire_commands+1&&voice_count==stall_retire_voices+1);
      int after_stall_commands=menu_commands,after_stall_voices=voice_count;
      test_clock+=3.01;update(nullptr);test_clock+=3.01;update(nullptr);
      assert(menu_commands==after_stall_commands&&voice_count==after_stall_voices);
      assert(!cars[0].player_restart_pending);
    }
    { /* Actual adapter binds every forced fuel leaf to per-car pit fields. */
      const char *leaves[]={"slow_flow","temporary_no_flow","interrupted_flow","spill"};
      for(int variant=1;variant<=4;variant++) {
        int object=50+variant;double t=1040+variant*20;
        std::string obj=std::to_string(object),id="physical-fuel-"+std::to_string(variant);
        test_object=object;test_snapshot.controller_role=0;test_snapshot.retired=0;
        test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;
        pit->refuel_active=1;pit->tire_change_active=0;
        pit->fuel_start=20;pit->fuel_start_time=(float)(t+0.1);
        pit->fuel_target=80;VEHF_FuelLevel(test_vehicles[0])=20;
        test_clock=t;update(nullptr);
        std::string arm=command(id.c_str(),obj.c_str());arm.pop_back();
        arm+=std::string(",\"failure\":\"fuel_rig\",\"failure_resolution\":\"")+leaves[variant-1]+"\"}";
        assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
        pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
        pit->stop_start_time=(float)t;pit->stop_end_time=(float)(t+5.6);
        test_clock=t+0.1;update(nullptr);
        if(!(cars[0].fuel.variant==variant&&cars[0].fuel.captured))
          fprintf(stderr,"fuel integration v%d: state %s variant%d captured%d plan%d request %s (%s) ready%d start%.3f ramp%.3f target%.3f\n",
                  variant,cars[0].phase.c_str(),cars[0].fuel.variant,(int)cars[0].fuel.captured,
                  (int)cars[0].plan.outcome,requests.at(id).state.c_str(),requests.at(id).message.c_str(),(int)cars[0].ready,
                  pit->stop_start_time,pit->fuel_start_time,pit->fuel_target);
        assert(cars[0].fuel.variant==variant&&cars[0].fuel.captured);
        assert(cars[0].fuel_recovery_owned&&!cars[0].fuel_recovery_announced);
        assert(requests.at(id).state=="consumed");
        { /* Timer completion must never queue a cue for this or the next stop. */
          pf_command_t recovered{};recovered.kind=PF_CMD_RECOVERED;
          size_t before=cars[0].deferred.size();
          queue_feedback(cars[0],recovered);
          assert(cars[0].deferred.size()==before);
        }
        if(variant==4)assert(pit->fuel_target<65);
        else {
          VEHF_FuelLevel(test_vehicles[0])=variant==2?20:variant==3?24:21;
          test_clock=t+1.0;update(nullptr);
          assert(cars[0].fuel.variant==variant&&cars[0].holding);
          assert(pit->fuel_start_time>(float)(t+0.1));
          if(variant==3)assert(cars[0].fuel.cut);
        }
        if(variant==2) { /* Real no-flow recovery emits once at ramp reanchor. */
          auto resumed_count=[](){
            int n=0;
            for(const auto &e:events) {
              if(e.find("FUEL SERVICE RESUMED")!=std::string::npos)n++;
            }
            return n;
          };
          int before=resumed_count();
          test_clock=t+1.1+cars[0].plan.extra_job_seconds;
          update(nullptr);
          assert(cars[0].fuel_recovery_announced&&resumed_count()==before+1);
          test_clock+=0.1;update(nullptr);
          assert(resumed_count()==before+1);
          int before_voice=voice_count;
          VEHF_FuelLevel(test_vehicles[0])=80.0f;
          test_clock+=0.1;update(nullptr);
          assert(cars[0].fuel_clear_announced);
          assert(voice_count==before_voice+1 && voice_eids[before_voice]==57);
          test_clock+=0.1;update(nullptr);
          assert(voice_count==before_voice+1);
        }
        if(variant==4)assert(!cars[0].fuel_clear_announced);
      }
      pit->refuel_active=0;pit->tire_change_active=1;
    }
    { /* A forced fuel fault on a tyre-only AI stop is rejected, but native
       tyre work still gets scheduled gun sounds. Clean AI stops do too. */
      test_object=88;test_snapshot.controller_role=1;test_snapshot.retired=0;
      test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;
      pit->refuel_active=0;pit->tire_change_active=1;
      pit->fuel_start=30;pit->fuel_target=30;
      test_clock=1140;update(nullptr);
      std::string arm=command("ai-no-fuel","88");arm.pop_back();
      arm+=",\"failure\":\"fuel_rig\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1140;pit->stop_end_time=1146;
      pit->tire_service_end_time=1145;
      test_clock=1140.1;update(nullptr);
      assert(requests.at("ai-no-fuel").state=="rejected");
      assert(cars[0].plan.outcome==PF_OUTCOME_CLEAN && cars[0].gun_audio);
      assert(cars[0].gun_off_at[0]>1140);
      sfx_requests.clear();test_clock=cars[0].gun_off_at[0]+0.01;
      update(nullptr);assert(!sfx_requests.empty());
      test_object=89;test_snapshot.pit_status=0;pit->status=0;
      pit->service_ready=0;test_clock=1160;update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1160;pit->stop_end_time=1166;
      pit->tire_service_end_time=1165;
      test_clock=1160.1;update(nullptr);
      assert(cars[0].token>0 && cars[0].gun_audio);
      test_object=90;test_snapshot.pit_status=0;pit->status=0;
      pit->service_ready=0;pit->refuel_active=1;
      pit->fuel_start=20;pit->fuel_target=104;pit->fuel_start_time=1180.5f;
      test_clock=1180;update(nullptr);
      arm=command("ai-slow-wheel-fuel-overlap","90");arm.pop_back();
      arm+=",\"failure\":\"slow_wheel\",\"failure_resolution\":\"minor\"}";
      assert(submit(arm).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1180;pit->stop_end_time=1188;
      pit->tire_service_end_time=1183;
      test_clock=1180.1;update(nullptr);
      assert(requests.at("ai-slow-wheel-fuel-overlap").state=="consumed");
      assert(cars[0].plan.outcome==PF_OUTCOME_SLOW_CORNER &&
             cars[0].plan.extra_job_seconds>0);
      assert(cars[0].target-cars[0].baseline>=
             cars[0].plan.extra_job_seconds-0.001);
      pit->refuel_active=0;
    }
    { /* L1: cancellation retires the identity without rewinding the service
       * sequence; only a new epoch or object generation resets it. */
      test_epoch=1;test_object=5;test_snapshot.controller_role=1;
      test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;test_clock=1200;update(nullptr);
      std::string first=command("l1-first","5");first.pop_back();first+=",\"failure\":\"slow_wheel\"}";
      assert(submit(first).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1200;pit->stop_end_time=1210;pit->tire_service_end_time=1209;
      test_clock=1200.1;update(nullptr);
      assert(cars[0].token==1&&cars[0].sequence==1&&requests.at("l1-first").state=="consumed");
      /* Transient disappearance: cancel with no object/epoch change. */
      test_hide_driver=true;test_clock=1200.2;update(nullptr);test_hide_driver=false;
      assert(!cars[0].seen&&cars[0].token==0&&cars[0].sequence==1);
      /* The first stop was already consumed; its identity is now retired. */
      assert(requests.at("l1-first").state=="consumed");
      /* Reattach the same object generation; the sequence is preserved. */
      test_clock=1300;test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;update(nullptr);
      assert(cars[0].seen&&cars[0].sequence==1);
      std::string second=command("l1-second","5");second.pop_back();second+=",\"failure\":\"stall\"}";
      assert(submit(second).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1300;pit->stop_end_time=1310;pit->tire_service_end_time=1309;
      test_clock=1300.1;update(nullptr);
      assert(cars[0].token==2&&cars[0].sequence==2&&requests.at("l1-second").state=="consumed");
      assert(cars[0].plan3.parent==PF3_PARENT_STALL);
      {pf_identity_t cancelled{1,((uint64_t)5<<8)|0,1};
       assert(pf3_session_state(policy3,&cancelled)==PF_SESSION_CANCELLED);}
      /* Confirmed object replacement resets the sequence for the new identity. */
      test_object=6;test_snapshot.pit_status=0;pit->status=0;pit->service_ready=0;test_clock=1400;update(nullptr);
      assert(cars[0].sequence==0);
      std::string third=command("l1-third","6");third.pop_back();third+=",\"failure\":\"slow_wheel\"}";
      assert(submit(third).find("accepted")!=std::string::npos);update(nullptr);
      pit->status=3;pit->service_ready=1;test_snapshot.pit_status=3;
      pit->stop_start_time=1400;pit->stop_end_time=1410;pit->tire_service_end_time=1409;
      test_clock=1400.1;update(nullptr);
      assert(cars[0].token==1&&requests.at("l1-third").state=="consumed");
      /* A new epoch resets the sequence as well. */
      test_epoch=2;test_clock=0;update(nullptr);
      assert(cars[0].sequence==0);
      test_epoch=1;
    }
    { /* L2: the adapter timing wrapper is guarded for both policy models,
       * with exact control-word restoration and overlap cases. */
      unsigned short saved;
      __asm__ __volatile__("fnstcw %0" : "=m"(saved));
      pf_job_t jobs[2];jobs[0]=(pf_job_t){PF_JOB_TYRE,6.0};jobs[1]=(pf_job_t){PF_JOB_FUEL,10.123456789};
      for(unsigned precision : {0u,0x0200u,0x0300u})for(unsigned rounding : {0u,0x0400u,0x0800u,0x0c00u}) {
        unsigned short game=(saved & ~0x0f00u)|precision|rounding;
        Car c{};
        c.plan3.severity=PF_RECOVERABLE;c.plan3.affected_job_kind=PF_JOB_STALL;c.plan3.extra_job_seconds=2.1700439453125;
        c.plan.severity=PF_RECOVERABLE;c.plan.affected_job_kind=PF_JOB_STALL;c.plan.extra_job_seconds=2.1700439453125;
        for(int model : {3,2}) {
          pf_timing_result_t wrapped{},reference{};
          pit_fp_restore(game);
          int rc=session_apply_timing_model(model,c,jobs,2,&wrapped);
          assert(pit_fp_control()==game);
          int rr=pit_policy_math([&]{
            return model==3?pf3_plan_apply_timing(&c.plan3,jobs,2,&reference)
                           :pf_plan_apply_timing(&c.plan,jobs,2,&reference);});
          assert(pit_fp_control()==game);
          assert(rc==rr&&wrapped.known&&reference.known);
          assert(memcmp(&wrapped,&reference,sizeof wrapped)==0);
          assert(wrapped.added_stop_time>=0.0);
        }
        pit_fp_restore(saved);
      }
      pit_fp_restore(saved);
    }
    {pf_job_t jobs[8];size_t n=0;pf_eligibility_t e{};pit_live_service_result_t result;
      assert(pit_live_jobs(100,110,1,109,0,0,0,0,0,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_OK&&e.tyre_work && !e.fuel_allowed);
      assert(pit_live_jobs(100,110,0,0,1,101,102,0,0,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_OK&&e.fuel_allowed);
      /* Penalty/no-work and wing/body-repair-only visits are fail-closed:
       * neither can commit a failure plan or expose a wheel/fuel/stall parent. */
      assert(!pit_live_jobs(100,110,0,0,0,0,0,0,0,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_SKIP_NO_RECOGNISED_WORK);
      assert(!pit_live_jobs(100,139,0,0,0,0,0,1,0,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY);
      assert(!pit_live_jobs(100,109,0,0,0,0,0,0,1,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY);
      /* A repair alongside genuine tyre work remains eligible for tyre-only
       * faults; the repair baseline stays native OTHER work. */
      assert(pit_live_jobs(100,139,1,106,0,0,0,1,0,jobs,&n,&e,&result));assert(result==PIT_LIVE_SERVICE_OK&&e.tyre_work&&!e.fuel_allowed);
      assert(!pit_live_jobs(100,110,1,108,0,0,0,0,0,jobs,&n,&e,&result));
      assert(!pit_live_jobs(100,NAN,1,109,0,0,0,0,0,jobs,&n,&e,&result));
      assert(pit_live_hold_value(111,110,115,120)==120);
      assert(pit_live_hold_value(111,110,115,110)==111.5);
      pf_plan_t presentation{};presentation.outcome=PF_OUTCOME_WHEELGUN;presentation.wheel=2;presentation.attempts=2;
      double off[4],on[4];pit_presentation_wheel_audio(100,106.38,seed,7,1,off,on);
      for(int i=0;i<4;i++){assert(off[i]>100 && on[i]>off[i]);assert(on[i]+0.90<106.38);}
      assert(off[2]<off[0] && off[3]<off[1] && on[2]<on[0] && on[3]<on[1]);
      double reveal=pit_presentation_failure_time(&presentation,100,112,0,seed,7,1);
      assert(reveal>=106&&reveal<106.30);
      assert(pit_presentation_command_time(&presentation,reveal,120,PF_CMD_FAULT_NOTICE,0)==reveal);
      assert(pit_presentation_command_time(&presentation,reveal,120,PF_CMD_RECOVERED,0)==119);
      presentation.outcome=PF_OUTCOME_SLOW_CORNER;presentation.wheel=0;
      assert(fabs(pit_presentation_command_time(&presentation,129.8,131.6,PF_CMD_RECOVERED,0)-130.6)<1e-9);
    }
    {std::map<std::string,std::string> d;assert(parse(command(),d));d.clear();assert(!parse("{\"slot\":\"0\"}",d));}
    pf_session_destroy(policy);if(policy3)pf3_session_destroy(policy3);CloseHandle(stop_event);DeleteCriticalSection(&lock);
    puts("PASS two-stage v3 adapter (14 forced leaves, natural fuel/fire contract, cancellation, repeated stops, x87), accepted v2 rollback path, AI engine stop/restart, terminal diagnosis/native request/dedup; precision, voice and lifecycle regressions; 60,000 selected plans");
}
