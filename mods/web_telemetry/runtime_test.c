/* Synthetic Windows/x86 regression harness. Never attaches to a real game. */
#define TELEMETRY_TEST 1
#define HTTP_PORT 18080
#include "web_telemetry.c"
#include <assert.h>
#include <stdlib.h>
#include <tlhelp32.h>
#include <psapi.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(2); } } while (0)
static F1CBrokerStats mock_stats;
static F1CBrokerResult __cdecl mock_get_stats(F1CBrokerStats *out) { *out=mock_stats; return F1C_BROKER_OK; }
__declspec(dllexport) uint64_t __cdecl llv_6a20c4e7(uint64_t nonce) { return nonce ^ UINT64_C(0x123456789abcdef0); }

static unsigned char test_image[0x600000];
void *telemetry_test_address(uintptr_t address,size_t size) {
    if (address>=0x400000 && address<0xa00000 && size<=0xa00000-address)
        return test_image+address-0x400000;
    return (void *)address;
}
static void set_i(uintptr_t at,int value) { memcpy(telemetry_test_address(at,4),&value,4); }
static void set_f(uintptr_t at,float value) { memcpy(telemetry_test_address(at,4),&value,4); }
static void set_p(uintptr_t at,void *value) { memcpy(telemetry_test_address(at,sizeof value),&value,sizeof value); }
static void setup(void) {
    InitializeCriticalSection(&g_telemetry_cs); InitializeCriticalSection(&g_sse_cs);
    telemetry_broker.get_stats=mock_get_stats;
    mock_stats.structure_size=sizeof mock_stats;
    mock_stats.hook_thread_id=GetCurrentThreadId(); mock_stats.session_active=1;
    mock_stats.session_generation=1;
    set_i(GAME_ADDR_SESSION_TYPE,5); set_i(GAME_ADDR_RACE_PHASE,5);
    set_i(GAME_ADDR_RACE_ACTIVE,3); set_i(GAME_ADDR_VEHICLE_COUNT,24);
    set_i(GAME_ADDR_TOTAL_LAPS,20);
    set_f(GAME_ADDR_RACE_TIME_THRESH,1800.5f);
    set_f(GAME_ADDR_AMBIENT_TEMP,20); set_f(GAME_ADDR_TRACK_TEMP,30);
    for (int w=0;w<WHEEL_COUNT;w++) {
        double raw=0.975; /* normalized usable tread 0.9; the validity source is real raw data */
        memcpy(telemetry_test_address(GAME_ADDR_WHEEL_THERMAL+(size_t)w*WHEEL_THERMAL_STRIDE+WH_OFF_TREAD,8),&raw,8);
        /* 4.3.0: player surface zones 360 K (86.84 C), carcass 350 K, and the
         * W+796 load term (RESEARCH) 3100. */
        for (int z=0;z<3;z++) {
            double k=360.0;
            memcpy(telemetry_test_address(GAME_ADDR_WHEEL_THERMAL+(size_t)w*WHEEL_THERMAL_STRIDE+WH_OFF_TIRE_TEMP+8*z,8),&k,8);
        }
        double carc=350.0, load=3100.0;
        memcpy(telemetry_test_address(GAME_ADDR_WHEEL_THERMAL+(size_t)w*WHEEL_THERMAL_STRIDE+WH_OFF_CARCASS,8),&carc,8);
        memcpy(telemetry_test_address(GAME_ADDR_WHEEL_LOAD+(size_t)w*WHEEL_THERMAL_STRIDE,8),&load,8);
    }
    for (int i=0;i<MAX_CARS;i++) {
        uintptr_t v=VEH_BASE+(size_t)i*VEH_STRIDE;
        set_p(GAME_ADDR_VEHICLE_PTR_ARRAY+sizeof(void *)*i,(void *)v);
        set_i(v+OFF_POSITION,i+1); set_i(v+OFF_LAP_NUMBER,2);
        set_f(v+OFF_ENGINE_RPM,1000); set_f(v+OFF_ENGINE_ANGVEL,1000);
        set_f(v+OFF_ENGINE_MAX_RPM,1000);
        set_i(v+OFF_AI_CONTROL_FLAG,i?1:0); set_f(v+OFF_SPEED,50);
        set_i(v+OFF_CONTROLLER_MODE,i?1:0); /* live shape: player 0, AI 1 */
        set_f(v+OFF_THROTTLE,0.5f); set_f(v+OFF_BRAKE,0.1f);
        set_f(v+OFF_STEER_INPUT,i?0.2f:0.0f);
        set_f(v+OFF_ACCEL_LAT,1.5f); set_f(v+OFF_ACCEL_LON,-2.0f);
        set_f(v+OFF_ACCEL_VERT,9.8f);
        set_f(v+OFF_HEADING_COS,1.0f); set_f(v+OFF_HEADING_SIN,0.0f);
        set_f(v+OFF_TIRE_WEAR_FL,1.0f); set_f(v+OFF_TIRE_WEAR_FR,1.0f);
        set_f(v+OFF_TIRE_WEAR_RL,1.0f); set_f(v+OFF_TIRE_WEAR_RR,1.0f);
        set_f(v+OFF_TRACK_PART_EXACT,100.0f);
        /* 4.3.0 RESEARCH wheel-block candidates (array index k = RR,RL,FR,FL). */
        for (int k=0;k<4;k++) {
            uintptr_t wb=v+OFF_WHEEL_BASE+(size_t)k*WHEEL_BLOCK_STRIDE;
            set_f(wb+WB_OFF_TEMP_K,350.0f+(float)k); set_f(wb+WB_OFF_LOAD,3000.0f+(float)k);
            set_f(wb+WB_OFF_SUSP,0.05f); set_f(wb+WB_OFF_SLIP,0.01f); set_f(wb+WB_OFF_FORCE,5000.0f);
        }
        set_f(v+0x4048,-1.0f); /* obsolete field deliberately disagrees */
        snprintf(telemetry_test_address(v+OFF_DRIVER_NAME,NAME_LEN),NAME_LEN,"Driver %d",i);
        snprintf(telemetry_test_address(v+OFF_CAR_NAME,NAME_LEN),NAME_LEN,"Team %d",i/2);
    }
    lap_history_init(); incidents_init();
}

static void test_host(void) {
    LightLoaderProtectedHostV2 h={0};
    CHECK(DllMain(GetModuleHandleA(NULL),DLL_PROCESS_ATTACH,NULL)==TRUE);
    CHECK(telemetry_started==0 && telemetry_worker_thread==NULL);
    CHECK(!verified_host(NULL)); CHECK(!rfe_1d3b7a91(NULL));
    h.size=sizeof h; h.magic=LLP_ABI_MAGIC; h.abi_version=LLP_ABI_VERSION;
    h.process_id=GetCurrentProcessId(); h.loader_module=GetModuleHandleA(NULL);
    h.nonce=1234; h.product_id=LLP_PRODUCT_ID; h.capabilities=LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2;
    h.compatibility_generation=LLP_HUB_2_GENERATION;
    h.seal=LightLoaderHostSeal(&h); h.proof=llv_6a20c4e7(LightLoaderProofChallenge(&h));
    CHECK(verified_host(&h));
    h.proof^=1; CHECK(!verified_host(&h)); h.proof^=1;
    h.process_id++; CHECK(!verified_host(&h)); h.process_id--;
    h.abi_version=1; CHECK(!verified_host(&h)); h.abi_version=LLP_ABI_VERSION;
    h.size=0; CHECK(!verified_host(&h));
    CHECK(!layout_compatible()); /* relocated synthetic executable */
    IMAGE_DOS_HEADER dos={0}; IMAGE_NT_HEADERS32 nt={0};
    dos.e_magic=IMAGE_DOS_SIGNATURE; dos.e_lfanew=0x80;
    nt.Signature=IMAGE_NT_SIGNATURE; nt.FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt.OptionalHeader.ImageBase=F1C_TARGET_IMAGE_BASE;
    nt.OptionalHeader.SizeOfImage=F1C_TARGET_IMAGE_SIZE;
    memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE,sizeof dos),&dos,sizeof dos);
    memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE+0x80,sizeof nt),&nt,sizeof nt);
    memcpy(telemetry_test_address(F1C_TARGET_PROBE_ADDRESS,F1C_TARGET_PROBE_LENGTH),F1C_TARGET_PROBE_BYTES,F1C_TARGET_PROBE_LENGTH);
    CHECK(layout_for_image(F1C_TARGET_IMAGE_BASE));
    ((unsigned char *)telemetry_test_address(F1C_TARGET_PROBE_ADDRESS,1))[0]^=1;
    CHECK(!layout_for_image(F1C_TARGET_IMAGE_BASE));
    /* rc.17: another build (probe differs) passes when its code references the
     * mirrored globals; too few references refuse. No EXE name/hash is used. */
    {
        IMAGE_NT_HEADERS32 nt2=nt; IMAGE_SECTION_HEADER sec={0};
        size_t nreg=sizeof mirror_regions/sizeof mirror_regions[0];
        uintptr_t code=F1C_TARGET_IMAGE_BASE+0x1000;
        nt2.FileHeader.NumberOfSections=1; nt2.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);
        memcpy(sec.Name,".text",5); sec.VirtualAddress=0x1000; sec.Misc.VirtualSize=0x1000;
        sec.Characteristics=IMAGE_SCN_CNT_CODE;
        memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE+0x80,sizeof nt2),&nt2,sizeof nt2);
        memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE+0x80+4+sizeof(IMAGE_FILE_HEADER)+sizeof(IMAGE_OPTIONAL_HEADER32),sizeof sec),&sec,sizeof sec);
        for (size_t r=0;r<nreg;r++) { uint32_t v=(uint32_t)mirror_regions[r].address; memcpy(telemetry_test_address(code+3+r*7,4),&v,4); }
        CHECK(layout_for_image(F1C_TARGET_IMAGE_BASE) && g_layout_mode==1 && g_layout_anchors==(int)nreg);
        memset(telemetry_test_address(code+3,4),0,4); memset(telemetry_test_address(code+10,4),0,4);
        CHECK(layout_for_image(F1C_TARGET_IMAGE_BASE) && g_layout_anchors==(int)nreg-2);   /* slack 2 */
        memset(telemetry_test_address(code+17,4),0,4);
        CHECK(!layout_for_image(F1C_TARGET_IMAGE_BASE) && g_layout_mode==0);               /* 3 missing */
        memset(telemetry_test_address(code,0x1000),0,0x1000);
        memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE+0x80,sizeof nt),&nt,sizeof nt);
    }
    ((unsigned char *)telemetry_test_address(F1C_TARGET_PROBE_ADDRESS,1))[0]^=1;
    CHECK(layout_for_image(F1C_TARGET_IMAGE_BASE) && g_layout_mode==2);
    nt.OptionalHeader.SizeOfImage=0x100000;
    memcpy(telemetry_test_address(F1C_TARGET_IMAGE_BASE+0x80,sizeof nt),&nt,sizeof nt);
    CHECK(!layout_for_image(F1C_TARGET_IMAGE_BASE));
}

static void test_copies(void) {
    static char json[262144];
    capture_on_game_thread(NULL); CHECK(capture_sequence==1);
    read_telemetry(); CHECK(g_telemetry.vehicle_count==24);
    CHECK(g_telemetry.race_time_threshold==1800.5f);
    CHECK(g_telemetry.cars[1].lap_dist==100.0f);
    CHECK(g_telemetry.cars[0].speed==50 && g_telemetry.player_slot==0);
    set_f(VEH_BASE+OFF_SPEED,75);
    read_telemetry(); CHECK(g_telemetry.cars[0].speed==50); /* private copy */
    capture_on_game_thread(NULL); read_telemetry(); CHECK(g_telemetry.cars[0].speed==75);
    set_i(GAME_ADDR_VEHICLE_COUNT,-1); capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.vehicle_count==0);
    set_i(GAME_ADDR_VEHICLE_COUNT,105); capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.vehicle_count==0);
    set_i(GAME_ADDR_VEHICLE_COUNT,24);
    set_p(GAME_ADDR_VEHICLE_PTR_ARRAY,(void *)0xdeadbeef);
    capture_on_game_thread(NULL); read_telemetry(); CHECK(g_telemetry.cars[0].position==0);
    CHECK(g_telemetry.player_slot==-1);
    set_p(GAME_ADDR_VEHICLE_PTR_ARRAY,(void *)VEH_BASE);
    set_f(VEH_BASE+OFF_SPEED,NAN); capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.cars[0].speed==0);
    mock_stats.session_active=0; capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.vehicle_count==0 && g_telemetry.race_active==0);
    mock_stats.session_active=1;
    set_i(GAME_ADDR_VEHICLE_COUNT,MAX_CARS); capture_on_game_thread(NULL); read_telemetry();
    format_failed=0; int n=json_telemetry(json,sizeof json);
    CHECK(!format_failed && n>0 && n<(int)sizeof json);
    CHECK(strstr(json,"\"version\":\"1.2.5\"")!=NULL);
    CHECK(strstr(json,"\"lhlim\":120")!=NULL);
    CHECK(strstr(json,"\"vf\":")!=NULL);
    CHECK(strstr(json,"\"tid\":")!=NULL);
    CHECK(strstr(json,"\"secel\":")!=NULL);
    /* Fix 1 (api_version 2): r/mr are raw rad/s (1000), rpm/max_rpm carry
     * the converted display values; no RPM-labelled field is rad/s. */
    CHECK(strstr(json,"\"r\":1000")!=NULL && strstr(json,"\"mr\":1000")!=NULL);
    CHECK(strstr(json,"\"rpm\":9549")!=NULL && strstr(json,"\"max_rpm\":9549")!=NULL);
    CHECK(strstr(json,"\"r\":9549")==NULL && strstr(json,"\"mr\":9549")==NULL);
    /* Fix 3: the player's steer stays unavailable with a reason; the AI
     * cars keep the documented valid steering path. */
    CHECK((g_telemetry.cars[0].car_valid&8)==0 && (g_telemetry.cars[1].car_valid&8)==8);
    CHECK(strstr(json,"\"steerwhy\":\"player steering unavailable")!=NULL);
    CHECK(strstr(json,"\"steerwhy\":\"\"")!=NULL);
    CHECK(strstr(json,"\"steerwhy\":\"AI steering")==NULL);
    unsigned epoch=1; capture_pending=1;
    update_queue_generation(2,&epoch); CHECK(capture_pending==0 && epoch==2);
    capture_pending=1; update_queue_generation(2,&epoch); CHECK(capture_pending==1);
    capture_pending=0;
    FILE *f=fopen("telemetry.json","w"); CHECK(f!=NULL); fputs(json,f); fclose(f);
}

/* 1.1.0: player chassis (full-physics object) and AI brake/spin channels.
 * Values mirror the live 2026-10-13 captures (COMMUNITY_CT_INTEGRATION). */
static void set_d(uintptr_t at,double value) { memcpy(telemetry_test_address(at,8),&value,8); }
static void test_chassis(void) {
    static char json[262144];
    const uintptr_t P=GAME_ADDR_PLAYER_PHYS;
    static unsigned char saved[0x1E40];   /* overlaps the thermal/load fixture */
    memcpy(saved,telemetry_test_address(P,0x1E40),0x1E40);
    /* zeroed block (menus / session end) -> no data */
    memset(telemetry_test_address(P,0x1E40),0,0x1E40);
    set_d(GAME_ADDR_SESSION_TIME,100.0);
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.pch_valid==0);
    for (int w=0;w<4;w++) {
        uintptr_t W=P+652+1124*(uintptr_t)w;
        set_d(W+820,385.16); set_d(W+884,120.0+w); set_d(W+892,135.0); set_d(W+924,0.98);
        set_d(W+488,0.0033); set_d(W+804,0.006); set_d(W+788,w&1?-0.042:0.042);
        set_d(W+188,-50.0/0.34); set_d(W+1068,0.028); set_d(W+1092,0.015);
        set_d(P+5872+8*(uintptr_t)w,w<2?0.032:0.060);
    }
    *(unsigned char *)telemetry_test_address(P+1124*3+1105,1)=1;   /* RR bent */
    set_d(P+1124*2+1116,-0.12);
    *(unsigned char *)telemetry_test_address(P+5912,1)=1;
    set_d(P+0x48,1.0); set_d(P+0x08,0.4);   /* raw vs used throttle: TC cut */
    set_d(P+0x50,0.0); set_d(P+0x10,0.0);
    set_d(P+0x19C,64.5); set_d(P+5476,0.41); set_d(P+7540,800.0);
    set_d(P+6876,0.408); set_d(P+7060,-0.179); set_d(P+7068,0.45);
    set_d(P+0x1C5C,1500.0); set_d(P+0x1E38,1400.0);
    *(int *)telemetry_test_address(P+5212,4)=1;
    set_d((uintptr_t)G_EngineLifeSeconds,5000.0); set_f(VEH_BASE+OFF_SPEED,50.0f);
    /* AI car 1: front axle brake 900 K, rear 600 K, wheel spin, tread order */
    uintptr_t v1=VEH_BASE+VEH_STRIDE;
    for (int k=0;k<4;k++) {
        uintptr_t wb=v1+OFF_WHEEL_BASE+(size_t)k*WHEEL_BLOCK_STRIDE;
        set_f(wb+AI_WB_BRAKE_K,k<2?900.0f:600.0f); set_f(wb+AI_WB_SPIN,-100.0f-(float)k);
    }
    set_f(v1+OFF_TIRE_WEAR_FL,0.99f); set_f(v1+OFF_TIRE_WEAR_RR,0.95f);
    set_d(GAME_ADDR_SESSION_TIME,100.0); capture_on_game_thread(NULL); read_telemetry();
    set_d((uintptr_t)G_EngineLifeSeconds,4999.95); set_d(GAME_ADDR_SESSION_TIME,100.1);
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.pch_valid==1);
    CHECK(fabsf(g_telemetry.pch_ride_mm[0]-32.0f)<0.01f && fabsf(g_telemetry.pch_ride_mm[3]-60.0f)<0.01f);
    CHECK(fabsf(g_telemetry.pch_opt_c[0]-112.0f)<0.01f);
    CHECK(fabsf(g_telemetry.pch_press[1]-121.0f)<0.01f && fabsf(g_telemetry.pch_press_set[1]-135.0f)<0.01f);
    CHECK(fabsf(g_telemetry.pch_susp_mm[0]-3.3f)<0.01f && fabsf(g_telemetry.pch_disc_mm[0]-28.0f)<0.01f);
    CHECK(g_telemetry.pch_camber_deg[0]>2.4f && g_telemetry.pch_camber_deg[1]<-2.4f);
    CHECK(g_telemetry.pch_bent[3]==1 && g_telemetry.pch_bent[0]==0 && g_telemetry.pch_bottom==1);
    CHECK(fabsf(g_telemetry.pch_bend[2]+0.12f)<1e-4f && g_telemetry.pch_compound==1);
    CHECK(fabsf(g_telemetry.pch_in_raw[0]-1.0f)<1e-6f && fabsf(g_telemetry.pch_in_used[0]-0.4f)<1e-6f);
    CHECK(fabsf(g_telemetry.pch_eng_wear-0.5f)<0.01f);
    CHECK(fabsf(g_telemetry.pch_rpm-14324.0f)<1.0f && fabsf(g_telemetry.pch_clutch_rpm-13369.0f)<1.0f);
    /* roll ratio: radius learned at 50 m/s coasting -> 1.0 */
    CHECK(fabsf(g_telemetry.pch_roll[0]-1.0f)<0.01f);
    /* AI: per-axle brake temp, spin, and the corrected FL..RR tread order */
    car_telemetry_t *c1=&g_telemetry.cars[1];
    CHECK(fabsf(c1->abt[0]-626.84f)<0.01f && fabsf(c1->abt[1]-326.84f)<0.01f);
    CHECK(fabsf(c1->aws[0]+100.0f)<1e-4f && fabsf(c1->aws[3]+103.0f)<1e-4f);
    CHECK(fabsf(c1->tire_wear_fl-0.99f)<1e-6f && fabsf(c1->tire_wear_rr-0.95f)<1e-6f);
    CHECK(g_telemetry.cars[0].abt[0]<=-999.0f);   /* player has no AI brake channel */
    format_failed=0; int n=json_telemetry(json,sizeof json);
    CHECK(!format_failed && n>0 && n<(int)sizeof json);
    CHECK(strstr(json,"\"pch\":{\"v\":1,\"rh\":[32.00,32.00,60.00,60.00]")!=NULL);
    CHECK(strstr(json,"\"bent\":[0,0,0,1]")!=NULL && strstr(json,"\"cmp\":1")!=NULL);
    CHECK(strstr(json,"\"abt\":[626.8,326.8]")!=NULL);
    /* session end: sim time jumps to ~1e9 -> no data */
    set_d(GAME_ADDR_SESSION_TIME,1.0e9); capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.pch_valid==0);
    json_telemetry(json,sizeof json); CHECK(strstr(json,"\"pch\":{\"v\":0,")!=NULL);
    /* restore the fixture for later tests */
    memcpy(telemetry_test_address(P,0x1E40),saved,0x1E40);
    set_d(GAME_ADDR_SESSION_TIME,0.0); set_f(v1+OFF_TIRE_WEAR_FL,1.0f); set_f(v1+OFF_TIRE_WEAR_RR,1.0f);
    capture_on_game_thread(NULL); read_telemetry();
}

static void test_waypoints(void) {
    char json[2048]; uintptr_t db=GAME_ADDR_AI_DB_BASE;
    unsigned char *wp=VirtualAlloc(NULL,MAX_WAYPOINTS*WP_STRIDE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(wp!=NULL);
    set_i(db+OFF_WP_COUNT,2); set_p(db+OFF_WP_ARRAY,wp);
    set_f((uintptr_t)wp+WP_OFF_X,12.5f); set_f((uintptr_t)wp+WP_OFF_Z,25);
    capture_on_game_thread(NULL);
    set_f((uintptr_t)wp+WP_OFF_X,99);
    format_failed=0; build_track_json(json,sizeof json);
    CHECK(strstr(json,"[12.5,25.0]")!=NULL);
    capture_on_game_thread(NULL); build_track_json(json,sizeof json);
    CHECK(strstr(json,"[99.0,25.0]")!=NULL); /* same-count track changes */
    /* rc.12 weather map: dry/wet lines from wp_path x wp_perp (both records need
     * a real unit perp or the document refuses the arrays). The waypoint array
     * is mirrored at capture, so the edits must be published first. */
    for (int i=0;i<2;i++) {
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PERP+0,1.0f);
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PERP+8,0.0f);
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PATH+0,-2.0f);
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PATH+4,-4.0f);
    }
    capture_on_game_thread(NULL);
    build_track_json(json,sizeof json);
    CHECK(strstr(json,"\"dl\":[[97.0,25.0]")!=NULL && strstr(json,"\"wl\":[[95.0,25.0]")!=NULL);
    CHECK(strstr(json,"\"gw\":")!=NULL && strstr(json,"\"gww\":")!=NULL);
    for (int i=0;i<2;i++) {   /* a zero perp refuses the whole pair */
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PERP+0,0.0f);
        set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_PERP+8,0.0f);
    }
    capture_on_game_thread(NULL);
    build_track_json(json,sizeof json);
    CHECK(strstr(json,"\"dl\"")==NULL && strstr(json,"\"wl\"")==NULL && strstr(json,"[99.0,25.0]")!=NULL);
    CHECK(VirtualFree(wp,0,MEM_RELEASE));
    build_track_json(json,sizeof json); CHECK(strstr(json,"99.0")!=NULL);
    capture_on_game_thread(NULL); build_track_json(json,sizeof json);
    CHECK(strstr(json,"\"pts\":[]")!=NULL); /* freed live path refused */
    set_i(db+OFF_WP_COUNT,MAX_WAYPOINTS+1); capture_on_game_thread(NULL);
    build_track_json(json,sizeof json); CHECK(strstr(json,"\"pts\":[]")!=NULL);
    strcpy(g_telemetry.track_name,"old track"); g_telemetry.lap_len=5000;
    mirror_ADDR_AIW_PATH.bytes[0]=0; sectors_update();
    CHECK(g_telemetry.track_name[0]==0 && g_telemetry.lap_len==0);
    unsigned old=capture_sequence;
    mock_stats.hook_thread_id++; capture_on_game_thread(NULL);
    CHECK(capture_sequence==old); mock_stats.hook_thread_id--;
}

/* rc.12: the GDB weather-script parser + its /weather emission. Fixture files
 * live next to this exe (the game-dir anchor); disposable test output. */
static void test_gdb_script(void) {
    char dir[MAX_PATH], aiw[MAX_PATH], gdb[MAX_PATH];
    static char wbuf[200000];
    if (!GetModuleFileNameA(NULL,dir,sizeof dir)) return;
    char *cut=strrchr(dir,'\\'); if(!cut) return; cut[1]=0;
    size_t dl=strlen(dir);
    if (dl+10>=sizeof aiw) return;
    memcpy(aiw,dir,dl); memcpy(aiw+dl,"rtfix.AIW",10);
    memcpy(gdb,dir,dl); memcpy(gdb+dl,"rtfix.gdb",10);
    FILE *f=fopen(aiw,"w");
    if(!f){ CHECK(0&&"rtfix.AIW not writable"); return; }
    fprintf(f,"[Waypoint]\nlap_length=5088.1\nsector_1_length=1968.9\nsector_2_length=3985.7\n");
    fclose(f);
    f=fopen(gdb,"w");
    CHECK(f!=NULL);
    fprintf(f,
        "rtfix\n{\n  Weather\n  {\n    TestDay\n    {\n      Conditions = Overcast\n"
        "      TrackWetness = Wet\n      AmbientTemp = 19\n      TrackTemp = 22\n"
        "      Minute = 5\n      {\n        Conditions = Rain\n        AmbientTemp = 16\n      }\n    }\n"
        "    Race\n    {\n      Conditions = Nice\n      TrackWetness = Dry\n"
        "      AmbientTemp = 18\n      TrackTemp = 19\n      Lap = 34\n      {\n"
        "        Conditions = Rain\n        TrackWetness = Wet\n      }\n    }\n  }\n}\n");
    fclose(f);

    strcpy((char*)mirror_ADDR_AIW_PATH.bytes,"rtfix.AIW");
    sectors_update();   /* reads rtfix.AIW then the GDB next to it */
    int n=build_weather_json(wbuf,sizeof wbuf);
    CHECK(n>0 && n<(int)sizeof wbuf);
    CHECK(strstr(wbuf,"\"gdb\":{\"file\":\"rtfix.gdb\"")!=NULL);
    CHECK(strstr(wbuf,"\"name\":\"TestDay\"")!=NULL && strstr(wbuf,"\"name\":\"Race\"")!=NULL);
    CHECK(strstr(wbuf,"\"minute\":5.0,\"conditions\":\"Rain\"")!=NULL);
    CHECK(strstr(wbuf,"\"lap\":34.0,\"conditions\":\"Rain\",\"track_wetness\":\"Wet\"")!=NULL);
    CHECK(strstr(wbuf,"\"track_wetness\":\"Dry\"")!=NULL && strstr(wbuf,"\"ambient\":\"19\"")!=NULL);

    mirror_ADDR_AIW_PATH.bytes[0]=0;
    sectors_update();   /* clears the script with the track */
    build_weather_json(wbuf,sizeof wbuf);
    CHECK(strstr(wbuf,"\"file\":\"\"")!=NULL && strstr(wbuf,"\"sessions\":[]")!=NULL);
    DeleteFileA(aiw); DeleteFileA(gdb);   /* disposable %TEMP% fixtures */
}

static int driver_builder(char *buf,int size) { return build_driver_json(buf,size,0); }
static int trace_status_builder(char *buf,int size) { return build_trace_status_json(buf,size); }
static int trace_data_builder(char *buf,int size) { return build_trace_data_json(buf,size,0,2); }
static int trace_csv_builder(char *buf,int size) { return build_trace_data_csv(buf,size,0,2); }
static int trace_compare_builder(char *buf,int size) { return build_trace_compare_json(buf,size,0,2,0,1,2,0,5.0f); }
static int weather_builder(char *buf,int size) { return build_weather_json(buf,size); }
static void test_formatting(void) {
    char escaped[1024];
    esc_json(escaped,sizeof escaped,"A\"B\\C\n\t\001");
    CHECK(strcmp(escaped,"A\\\"B\\\\C\\u000a\\u0009\\u0001")==0);
    g_telemetry.vehicle_count=MAX_CARS;
    for (int i=0;i<MAX_CARS;++i) {
        g_telemetry.cars[i].position=i+1;
        memset(g_telemetry.cars[i].name,'\001',NAME_LEN-1);
        g_telemetry.cars[i].name[NAME_LEN-1]=0;
    }
    g_incident_count=MAX_INCIDENTS;
    for (int i=0;i<MAX_INCIDENTS;i++) {
        g_incidents[i].car_id=i%MAX_CARS;
        /* Contract-valid fixture: the served /incidents doc must validate
         * (BACKEND 2), so give every synthetic entry a real type. */
        strcpy(g_incidents[i].type,"SYNTH");
    }
    g_telemetry.lap_len=5000.0f;   /* canary exercises serializers, not the 404 path */
    int (*builders[])(char *,int)={json_telemetry,build_track_json,build_laps_json,build_incidents_json,build_pits_json,driver_builder,
        trace_status_builder,trace_data_builder,trace_csv_builder,trace_compare_builder,weather_builder};
    for (unsigned k=0;k<sizeof builders/sizeof builders[0];k++) {
        for (int capacity=1;capacity<4096;capacity+=17) {
            unsigned char *bytes=malloc((size_t)capacity+32);
            memset(bytes,0xa5,(size_t)capacity+32); format_failed=0;
            int n=builders[k]((char *)bytes+16,capacity);
            if (!(n>=0 && n<capacity)) fprintf(stderr,"CANARY builder=%u capacity=%d n=%d\n",k,capacity,n);
            CHECK(n>=0 && n<capacity);
            for (int j=0;j<16;j++) CHECK(bytes[j]==0xa5 && bytes[capacity+16+j]==0xa5);
            free(bytes);
        }
    }
    for(int i=0;i<MAX_CARS;i++) snprintf(g_telemetry.cars[i].name,NAME_LEN,"Driver %d",i);
    char large[262144]; format_failed=0;
    json_telemetry(large,sizeof large); CHECK(!format_failed);
}

static void write_text_file(const char *path,const char *text) {
    FILE *f=fopen(path,"w"); CHECK(f!=NULL); fputs(text,f); fclose(f);
}

/* [Mod] Port parse: valid / invalid / missing values fall back exactly as the
 * shipped DLL does (compiled default), and the load is one-shot. */
static void test_port_settings(void) {
    char tmp[MAX_PATH], ini[MAX_PATH+64];
    DWORD n=GetTempPathA(sizeof tmp,tmp);
    CHECK(n>0 && n<(DWORD)sizeof tmp && tmp[n-1]=='\\');
    snprintf(ini,sizeof ini,"%sf1c_port_settings_test.ini",tmp);
    /* Missing file keeps the compiled default (8080 in the shipped build). */
    g_configured_port=-1;
    settings_read_ini("Z:\\missing\\web_telemetry.ini");
    CHECK(g_configured_port==HTTP_PORT);
    /* Valid value; spaces, key case and a trailing comment are tolerated. */
    g_configured_port=-1; write_text_file(ini,"[Mod]\nEnabled=1\nPort=28123\n");
    settings_read_ini(ini); CHECK(g_configured_port==28123);
    g_configured_port=-1; write_text_file(ini,"[mod]\n PORT = 28124 ; comment\n");
    settings_read_ini(ini); CHECK(g_configured_port==28124);
    /* Out of range, junk, empty, negative and wrong-section values are invalid. */
    g_configured_port=-1; write_text_file(ini,"[Mod]\nPort=99\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Mod]\nPort=65536\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Mod]\nPort=abc\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Mod]\nPort=\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Mod]\nPort=-1\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Other]\nPort=28125\n"); settings_read_ini(ini); CHECK(g_configured_port==HTTP_PORT);
    g_configured_port=-1; write_text_file(ini,"[Other]\nPort=28126\n[Mod]\nPort=28127\n"); settings_read_ini(ini); CHECK(g_configured_port==28127);
    /* One-shot load: a second call must not re-read the ini. */
    g_configured_port=-1; g_settings_loaded=0;
    telemetry_settings_load("Z:\\missing\\web_telemetry.ini");
    CHECK(g_settings_loaded==1 && g_configured_port==HTTP_PORT);
    g_configured_port=28111;
    telemetry_settings_load("Z:\\missing\\web_telemetry.ini");
    CHECK(g_configured_port==28111);
    /* Restore harness defaults for the serve phase. */
    g_settings_loaded=0; g_configured_port=HTTP_PORT;
    remove(ini);
}

static int test_probe_bind(unsigned short port,SOCKET *out) {
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    struct sockaddr_in a;
    BOOL exclusive=TRUE;
    if (s==INVALID_SOCKET) return 0;
    setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&exclusive,sizeof exclusive);
    memset(&a,0,sizeof a);
    a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
    if (bind(s,(struct sockaddr*)&a,sizeof a)==SOCKET_ERROR) { closesocket(s); return 0; }
    *out=s; return 1;
}

/* Occupied primary port -> +1 fallback; all six candidates occupied -> 0
 * (HTTP disabled) while the harness process keeps running. */
static void test_port_bind(void) {
    WSADATA wsa; SOCKET held[6]={0};
    unsigned short base=0,candidate;
    CHECK(WSAStartup(MAKEWORD(2,2),&wsa)==0);
    for (candidate=40211;candidate<43000&&!base;candidate+=7) {
        int ok=1;
        for (int i=0;i<6;i++) if (!test_probe_bind((unsigned short)(candidate+i),&held[i])) { ok=0; break; }
        if (ok) base=candidate;
        else for (int i=0;i<6;i++) if (held[i]) { closesocket(held[i]); held[i]=0; }
    }
    CHECK(base!=0);
    CHECK(http_listen_open(base)==0);
    CHECK(g_listen_sock==INVALID_SOCKET);
    closesocket(held[1]); held[1]=0;
    CHECK(http_listen_open(base)==base+1);
    CHECK(g_listen_sock!=INVALID_SOCKET);
    closesocket(g_listen_sock); g_listen_sock=INVALID_SOCKET;
    for (int i=0;i<6;i++) if (held[i]) closesocket(held[i]);
    WSACleanup();
}

static unsigned thread_count(void) {
    unsigned count=0;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    THREADENTRY32 entry; memset(&entry,0,sizeof entry); entry.dwSize=sizeof entry;
    CHECK(snapshot!=INVALID_HANDLE_VALUE);
    if (Thread32First(snapshot,&entry)) do {
        if (entry.th32OwnerProcessID==GetCurrentProcessId()) count++;
    } while(Thread32Next(snapshot,&entry));
    CloseHandle(snapshot); return count;
}
static void test_real_dll(const char *path) {
    unsigned before=thread_count();
    HMODULE dll=LoadLibraryA(path); CHECK(dll!=NULL);
    union { FARPROC raw; BOOL (__cdecl *init)(const LightLoaderProtectedHostV2 *); } entry={0};
    entry.raw=GetProcAddress(dll,LLP_ENTRY_NAME); CHECK(entry.raw!=NULL);
    CHECK(!entry.init(NULL)); CHECK(thread_count()==before);
    CHECK(FreeLibrary(dll));
}

/* ---- engineering trace tests (synthetic; no game attachment) ---- */
static void trace_vehicle(int slot,int ai,float dist,float speed,float yaw,
                          float steer,float tread) {
    uintptr_t v=VEH_BASE+(size_t)slot*VEH_STRIDE;
    set_f(v+OFF_SPEED,speed);
    set_f(v+OFF_ENGINE_RPM,1000.0f);   /* raw rad/s; trace stores x LF_RPM_FACTOR */
    set_i(v+OFF_CURRENT_GEAR,5);
    set_f(v+OFF_TRACK_X,dist);
    set_f(v+OFF_TRACK_Z,50.0f);
    set_f(v+OFF_HEADING_COS,cosf(yaw)); set_f(v+OFF_HEADING_SIN,sinf(yaw));
    set_f(v+OFF_STEER_INPUT,steer);
    set_f(v+OFF_ACCEL_LAT,1.5f);
    set_f(v+OFF_ACCEL_LON,-2.0f);
    set_f(v+OFF_ACCEL_VERT,9.8f);
    set_f(v+OFF_TRACK_PART_EXACT,dist);
    set_f(v+OFF_TIRE_WEAR_FL,tread); set_f(v+OFF_TIRE_WEAR_FR,tread);
    set_f(v+OFF_TIRE_WEAR_RL,tread); set_f(v+OFF_TIRE_WEAR_RR,tread);
    set_i(v+OFF_AI_CONTROL_FLAG,ai);
    car_telemetry_t *c=&g_telemetry.cars[slot];
    c->position=slot+1; c->on_track=1; c->pitting=0;
    c->lap_dist=dist; c->speed=speed; c->throttle=0.5f; c->brake=0.1f;
    c->ai_control=ai;
    c->rpm=1000.0f; c->gear=5; c->track_x=dist; c->track_z=50.0f;
    c->tire_wear_fl=c->tire_wear_fr=c->tire_wear_rl=c->tire_wear_rr=tread;
}

/* Bugs found by native acceptance: these fixtures deliberately violate the
 * recorder's assumptions instead of only exercising its happy path. */
static void test_trace_guards(void) {
    capture_generation=99;g_telemetry.lap_len=5000;g_telemetry.player_slot=0;
    trace_select(0,0,1);trace_select(1,1,1);
    trace_vehicle(0,0,2500,50,0,0,0.9f);
    g_telemetry.cars[0].lap_number=10;g_telemetry.session_time=10;
    capture_on_game_thread(NULL);trace_update(24);
    for(int k=0;k<40;k++) {
        g_telemetry.cars[0].lap_dist=2500+k*25;
        trace_sample_car(0,24,10+k*0.5f);
    }
    g_telemetry.cars[0].lap_dist=10;g_telemetry.cars[0].lap_number=11;
    trace_sample_car(0,24,30);
    CHECK(g_trace[0].lap_total==0);CHECK(g_trace[0].dropped_laps==1);
    CHECK(!g_trace[0].start_seen); /* increment alone is not a crossing */
    int samples=g_trace[0].sample_count;
    trace_sample_car(0,24,30);CHECK(g_trace[0].sample_count==samples);
    unsigned rev=g_trace[0].revision;
    g_telemetry.cars[0].ai_control=1;trace_update(24);
    CHECK(g_trace[0].revision==rev); /* AI control can drop before retirement is published */
    CHECK(g_trace[0].lap_total==0);rev=g_trace[0].revision;
    g_telemetry.cars[0].ai_control=0;g_telemetry.cars[0].finish_state=2;g_telemetry.cars[0].on_track=0;
    trace_update(24);CHECK(g_trace[0].revision==rev);
    g_telemetry.cars[0].ai_control=1;g_telemetry.cars[0].finish_state=0;g_telemetry.cars[0].on_track=1;
    strcpy(g_telemetry.cars[0].name,"Replacement driver");trace_update(24);
    CHECK(g_trace[0].revision!=rev);
    CHECK(!strcmp(g_trace[0].driver_name,"Replacement driver"));
    CHECK(trace_team_id(2166136261u)==0 && trace_team_id(123)==123);
    /* Empty broker hashes must never pair unrelated teams. */
    g_trace_auto[1]=1;
    for(int i=0;i<24;i++) {g_telemetry.cars[i].on_track=1;g_telemetry.cars[i].position=i+1;g_telemetry.cars[i].ai_control=1;g_telemetry.cars[i].team_id=2166136261u;}
    strcpy(g_telemetry.cars[0].car_name,"Ferrari");
    strcpy(g_telemetry.cars[1].car_name,"McLaren");
    strcpy(g_telemetry.cars[2].car_name,"Ferrari");
    trace_default_selection(24);CHECK(g_trace[1].slot==2);
    /* The actual retired human retains identity, without becoming a recorder sample. */
    g_player_slot=0;g_player_slot_found=1;g_telemetry.cars[0].finish_state=3;
    player_slot_update(24);CHECK(g_player_slot==0);
    g_telemetry.cars[0].finish_state=0;
    /* Batch 2c task 1: a stopped/retired player is never auto A; the leader is. */
    g_telemetry.player_slot=0; g_trace_auto[0]=1;
    for(int i=0;i<24;i++){g_telemetry.cars[i].on_track=1;g_telemetry.cars[i].position=i+1;g_telemetry.cars[i].finish_state=0;}
    g_telemetry.cars[0].finish_state=3;
    trace_select(0,-1,0);
    trace_default_selection(24);CHECK(g_trace[0].slot==1);
    /* Batch 2c task 1: re-selecting the same slot keeps recorded laps; a slot switch clears them (one ring per side). */
    g_telemetry.cars[0].finish_state=0;
    trace_select(0,1,1);g_trace[0].lap_number=7;
    trace_select(0,1,1);CHECK(g_trace[0].slot==1 && g_trace[0].lap_number==7);
    trace_select(0,2,1);CHECK(g_trace[0].slot==2 && g_trace[0].lap_number==-1);
    trace_select(0,-1,0);g_trace_auto[0]=1;
    /* Restore canonical mirrors for the independent HTTP fixture. */
    capture_on_game_thread(NULL);read_telemetry();
}

static void test_trace_retention(void) {
    capture_generation=100;g_telemetry.lap_len=5000;
    trace_select(0,0,1);trace_select(1,1,1);
    trace_vehicle(0,0,25,50,0,0,0.9f);g_telemetry.cars[0].lap_number=1;
    g_telemetry.session_time=0;capture_on_game_thread(NULL);trace_update(24);
    for(int lap=1;lap<=9;lap++) for(int k=0;k<200;k++) {
        g_telemetry.cars[0].lap_number=lap;
        g_telemetry.cars[0].lap_dist=(k+1)*25.0f;
        trace_sample_car(0,24,(lap-1)*100.0f+k*0.5f);
    }
    CHECK(g_trace[0].lap_total==7); /* first partial rejected, laps 2..8 closed */
    int retained=0;
    for(int i=0;i<TRACE_LAP_SLOTS;i++)retained+=!!(g_trace[0].lap_valid[i]&TRL_COMPLETE);
    CHECK(retained==6);CHECK(trace_find_lap(&g_trace[0],2)==-1);
    CHECK(trace_find_lap(&g_trace[0],3)>=0 && trace_find_lap(&g_trace[0],8)>=0);
    /* Fix 2 (api_version 2): the evicted reference is explicit in
     * /trace/status and classified as evicted (-2) on a data request, never
     * aliased; never-recorded laps (-1) and pinned gen/rev mismatches
     * (-3/-4) stay distinguishable. */
    {
        static char ebuf[524288];
        CHECK(build_trace_status_json(ebuf,sizeof ebuf)>0);
        CHECK(strstr(ebuf,"\"evicted\":[{\"lap\":2,")!=NULL);
        CHECK(strstr(ebuf,"\"src\":\"live\"")!=NULL);
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,2,-1,-1)==-2);
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,1,-1,-1)==-1);
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,9,-1,-1)==-1);
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,3,-1,-1)>0);
        {
            /* capture_on_game_thread refreshes capture_generation from the
             * mock broker (1), so build the expected lid pattern at runtime. */
            char lidpat[64];
            snprintf(lidpat,sizeof lidpat,"\"lid\":{\"gen\":%u,\"rev\":%u,",
                     capture_generation,g_trace[0].revision);
            CHECK(strstr(ebuf,lidpat)!=NULL);
        }
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,3,99,-1)==-3);
        CHECK(build_trace_data_json_id(ebuf,sizeof ebuf,0,3,-1,(long)g_trace[0].revision+1)==-4);
        CHECK(build_trace_data_csv_id(ebuf,sizeof ebuf,0,2,-1,-1)==-2);
        CHECK(build_trace_data_csv_id(ebuf,sizeof ebuf,0,3,-1,-1)>0);
        CHECK(strstr(ebuf,"src=live")!=NULL);
    }
    /* Replay observed native ordering: counter increments BEFORE distance wrap.
     * Both boundaries must remain continuous and close a complete lap. */
    trace_session_clear();trace_select(0,0,1);g_trace[0].used=1;g_telemetry.lap_len=6784.1f;
    trace_vehicle(0,0,6770.5161f,74,0,0,0.9f);
    g_telemetry.cars[0].lap_number=5;g_telemetry.cars[0].lap_dist=6770.5161f;
    trace_sample_car(0,24,637.5f);
    g_telemetry.cars[0].lap_number=6;g_telemetry.cars[0].lap_dist=6775.6592f;
    trace_sample_car(0,24,637.6f);CHECK(g_trace[0].lap_number==5);
    g_telemetry.cars[0].lap_dist=2.341f;trace_sample_car(0,24,637.7f);
    CHECK(g_trace[0].lap_number==6 && g_trace[0].start_seen);
    for(int k=1;k<200;k++) {g_telemetry.cars[0].lap_dist=2.341f+k*33.9f;
        trace_sample_car(0,24,637.7f+k*0.5f);}
    g_telemetry.cars[0].lap_number=7;g_telemetry.cars[0].lap_dist=6775.6592f;
    trace_sample_car(0,24,737.3f);CHECK(g_trace[0].lap_number==6);
    g_telemetry.cars[0].lap_dist=2.341f;trace_sample_car(0,24,737.4f);
    CHECK(trace_find_lap(&g_trace[0],6)>=0);
    /* Restore old fixture context for overflow guard. */
    g_telemetry.lap_len=5000;g_trace[0].lap_number=9;g_telemetry.cars[0].lap_number=9;
    /* Overflow cannot turn a truncated lap into a complete comparison. */
    g_trace[0].laps[g_trace[0].current_index][TRACE_SAMPLES-1]=g_trace[0].laps[g_trace[0].current_index][199];
    g_trace[0].sample_count=TRACE_SAMPLES;
    trace_sample_car(0,24,900);CHECK(g_trace[0].dropped_samples==1);
    g_telemetry.cars[0].lap_number=10;g_telemetry.cars[0].lap_dist=25;
    trace_sample_car(0,24,900.5f);CHECK(trace_find_lap(&g_trace[0],9)==-1);
}

/* 4.4.0-rc.5 (Batch 2h task A): the game lap counter is 0 for the whole first
 * racing lap. A moving lap-0 player must be recorded; a grid standstill must not. */
static void test_trace_lap0(void) {
    capture_generation=101;g_telemetry.lap_len=5000;g_telemetry.player_slot=0;
    trace_select(0,0,1);
    trace_vehicle(0,0,2500,50,0,0,0.9f);
    g_telemetry.cars[0].lap_number=0;g_telemetry.cars[0].position=1;
    g_telemetry.cars[0].on_track=1;g_telemetry.cars[0].pitting=0;
    g_telemetry.session_time=10;
    capture_on_game_thread(NULL);trace_update(24);
    /* trace_update above may already record the moving setup sample, so the
     * checks compare against a baseline rather than absolute counts. */
    int base=g_trace[0].sample_count;
    g_telemetry.cars[0].speed=0;g_telemetry.cars[0].lap_dist=10;
    trace_sample_car(0,24,10);
    CHECK(g_trace[0].sample_count==base);
    for(int k=0;k<5;k++) {
        g_telemetry.cars[0].speed=50;g_telemetry.cars[0].lap_dist=100+k*25;
        trace_sample_car(0,24,11+k*0.5f);
    }
    CHECK(g_trace[0].sample_count>=base+5);
    CHECK(g_trace[0].total_samples>=5);
}

static void test_trace(void) {
    static char buf[524288];
    trace_session_clear();
    capture_generation=1;
    snprintf(g_telemetry.track_name,sizeof g_telemetry.track_name,"Synthetic Track");
    g_telemetry.player_slot=0; g_telemetry.lap_len=5000.0f; g_telemetry.session_time=0;
    for (int w=0;w<4;w++) g_telemetry.tread[w]=0.9f;
    trace_select(0,0,1); trace_select(1,1,1);
    for (int lap=1;lap<=3;lap++) {
        for (int k=0;k<200;k++) {
            float dist=(float)(k+1)*25.0f;
            float t=(float)(lap-1)*100.0f+(float)k*0.5f;
            if (lap==3 && k>=100) t+=5.0f;          /* one gap in lap 3 */
            g_telemetry.session_time=t;
            set_i(VEH_BASE+OFF_LAP_NUMBER,lap);
            set_i(VEH_BASE+VEH_STRIDE+OFF_LAP_NUMBER,lap);
            g_telemetry.cars[0].lap_number=lap; g_telemetry.cars[1].lap_number=lap;
            trace_vehicle(0,0,dist,50.0f+(float)lap,0.01f*(float)k,0.0f,0.9f);
            trace_vehicle(1,1,dist,49.0f+(float)lap,0.01f*(float)k,0.25f,0.95f);
            capture_on_game_thread(NULL);
            trace_update(24);
        }
    }
    /* close lap 3 */
    g_telemetry.session_time=305.0f;
    g_telemetry.cars[0].lap_number=4; g_telemetry.cars[1].lap_number=4;
    set_i(VEH_BASE+OFF_LAP_NUMBER,4); set_i(VEH_BASE+VEH_STRIDE+OFF_LAP_NUMBER,4);
    trace_vehicle(0,0,10,50,0,0,0.9f); trace_vehicle(1,1,8,49,0,0.25f,0.95f);
    capture_on_game_thread(NULL);
    trace_update(24);

    for (int idx=0;idx<TRACE_LAP_SLOTS;idx++) if (g_trace[1].lap_valid[idx]) {
        g_trace[1].lap_start_sim[idx]=g_trace[1].lap_start_sim[idx]*1.1f+1000.0f;
        g_trace[1].lap_time[idx]*=1.1f;
        for (int k=0;k<g_trace[1].lap_samples[idx];k++)
            g_trace[1].laps[idx][k].sim_t=g_trace[1].laps[idx][k].sim_t*1.1f+1000.0f;
    }
    CHECK(g_trace[0].lap_total==2);
    CHECK(g_trace[1].lap_total==2);
    int la=trace_find_lap(&g_trace[0],2); CHECK(la==0);
    CHECK(trace_find_lap(&g_trace[0],1)==-1);
    CHECK(g_trace[0].lap_num[0]==2 && g_trace[0].lap_num[1]==3);
    CHECK((g_trace[0].lap_valid[0]&TRL_COMPLETE)!=0);
    CHECK((g_trace[0].lap_valid[1]&TRL_BREAKS)!=0);   /* the crafted gap */
    CHECK(g_trace[0].lap_samples[0]==200);
    /* player: no steering channel; AI: steering valid */
    CHECK((g_trace[0].channels_seen&TRF_STEER)==0);
    CHECK((g_trace[1].channels_seen&TRF_STEER)!=0);
    CHECK((g_trace[0].channels_seen&TRF_TREAD)!=0);
    const trace_sample_t *s0=&g_trace[0].laps[0][5];
    CHECK((s0->flags&TRF_SPEED)&&(s0->flags&TRF_GLAT)&&(s0->flags&TRF_GLON)&&
          (s0->flags&TRF_GVERT)&&(s0->flags&TRF_YAW)&&(s0->flags&TRF_YAWRATE)&&
          (s0->flags&TRF_TREAD));
    CHECK((s0->flags&TRF_STEER)==0);
    CHECK(fabsf(s0->tread[0]-0.9f)<1e-4f);            /* player tread source */
    const trace_sample_t *s1=&g_trace[1].laps[0][5];
    CHECK((s1->flags&TRF_STEER)!=0 && fabsf(s1->steer-0.25f)<1e-4f);
    CHECK(fabsf(s1->tread[0]-0.8f)<1e-3f);            /* (0.95-0.75)*4 */
    CHECK(fabsf(s1->yaw_rate)>0.0f);                  /* derived yaw rate */
    /* rc.5 workbench channels: converted RPM, gear, track coordinates. */
    CHECK((s0->flags&TRF_RPM) && fabsf(s0->rpm-9549.296585513721f)<0.1f);
    CHECK((s0->flags&TRF_GEAR) && s0->gear==5);
    CHECK((s0->flags&TRF_TRACK) && fabsf(s0->tx-150.0f)<0.001f && fabsf(s0->tz-50.0f)<0.001f);
    CHECK((g_trace[0].channels_seen&TRF_RPM)!=0 && (g_trace[0].channels_seen&TRF_GEAR)!=0 &&
          (g_trace[0].channels_seen&TRF_TRACK)!=0);

    /* A short gate interruption must remain a hole even with dt<1 and ds<50. */
    g_trace[0].laps[0][10].flags |= TRF_SEGMENT;
    trace_grid_fill(&g_trace[0],0,5,1000,&g_grid_a);
    CHECK(g_grid_a.breaks==1 && !(g_grid_a.have[51]&TGF_ELAPSED));
    g_trace[0].laps[0][10].flags &= ~TRF_SEGMENT;
    /* Compare relative lap elapsed time, despite a 1000-second B origin. */
    int n=build_trace_compare_json(buf,sizeof buf,0,2,0,1,2,0,5.0f);
    CHECK(n>0 && strstr(buf,"\"schema\":\"f1c.compare/1\"")!=NULL);
    CHECK(strstr(buf,"\"steer\":[null")!=NULL);       /* player side all holes */
    char *bch=strstr(buf,"\"b_ch\":{"); CHECK(bch!=NULL);
    {
        char *bs=strstr(bch,"\"steer\":["); CHECK(bs!=NULL);
        CHECK(strstr(bs,"0.25")!=NULL);               /* AI side has values */
    }
    /* B is slower (dist*0.8), so time delta B-A must be positive somewhere */
    {
        char *d=strstr(buf,"\"delta\":["); CHECK(d!=NULL); d+=9;
        double first=0.0; int found=0;
        for (int guard=0;guard<32&&*d;guard++) {
            if (!strncmp(d,"null",4)) { d=strchr(d,','); if(!d)break; d++; continue; }
            first=strtod(d,NULL); found=1; break;
        }
        CHECK(found && first>0.001 && first<1.0);
    }
    CHECK(strstr(buf,"\"rpm\":[")!=NULL && strstr(buf,"9549.30")!=NULL); /* rc.5 compare channel */
    CHECK(strstr(buf,"\"lid\":{\"gen\":1,\"rev\":")!=NULL);
    CHECK(strstr(buf,"\"src\":\"live\"")!=NULL);
    CHECK(build_trace_compare_json(buf,sizeof buf,0,999,0,1,1,0,5.0f)==-1);
    CHECK(build_trace_data_json(buf,sizeof buf,0,999)==-1);
    n=build_trace_data_json(buf,sizeof buf,0,2);
    CHECK(n>0 && strstr(buf,"\"schema\":\"f1c.trace/1\"")!=NULL);
    CHECK(strstr(buf,"\"st\":null")!=NULL);           /* player steering unavailable */
    /* 4.3.0: player tyre temperature is the FACT mean of the three zones;
     * the player research block carries only the W+796 load term. */
    CHECK(strstr(buf,"\"tt\":[86.8,86.8,86.8,86.8]")!=NULL);
    CHECK(strstr(buf,"\"rx\":[0,0,0,0,3100,3100,3100,3100,0,0,0,0,0,0,0,0]")!=NULL);
    CHECK(strstr(buf,"\"rpm\":9549.3")!=NULL && strstr(buf,"\"ge\":5")!=NULL);
    CHECK(strstr(buf,"\"tx\":150.0")!=NULL && strstr(buf,"\"tz\":50.0")!=NULL);
    CHECK(strstr(buf,"\"lid\":{\"gen\":1,\"rev\":")!=NULL);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,2,1,g_trace[0].revision)>0);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,2,2,-1)==-3);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,2,-1,12345)==-4);
    n=build_trace_data_csv(buf,sizeof buf,0,2);
    CHECK(n>0 && strstr(buf,"sim_t,dist_m")!=NULL);
    CHECK(strstr(buf,",rpm,gear,tx,tz")!=NULL && strstr(buf,"src=live")!=NULL);
    n=build_trace_status_json(buf,sizeof buf);
    CHECK(n>0 && strstr(buf,"\"unavailable\":[\"steering\"]")!=NULL);
    /* Fix 3: the unavailable channel carries a machine-readable reason. */
    CHECK(strstr(buf,"\"unavailable_why\":{\"steering\":\"player steering unavailable")!=NULL);
    n=build_trace_status_json(buf,sizeof buf);
    CHECK(n>0 && strstr(buf,"\"laps\":[")!=NULL && strstr(buf,"\"lid\":{\"gen\":1,\"rev\":")!=NULL);
    CHECK(strstr(buf,"\"evicted\":[")!=NULL);
    /* rc.5: open-lap serving (partial lap drawn immediately). */
    n=build_trace_open_json(buf,sizeof buf,0);
    CHECK(n>0 && strstr(buf,"\"open\":true")!=NULL && strstr(buf,"\"count\":1")!=NULL);
    CHECK(strstr(buf,"\"rpm\":9549.3")!=NULL);
    n=build_trace_open_json(buf,sizeof buf,1);
    CHECK(n>0 && strstr(buf,"\"open\":true")!=NULL);
    {
        int saved=g_trace[1].lap_open; g_trace[1].lap_open=0;
        CHECK(build_trace_open_json(buf,sizeof buf,1)==-1);
        g_trace[1].lap_open=saved;
    }
}

/* ---- rc.5 legacy import: synthetic fixtures -> C decoders -> f1c.trace/1 ---- */
typedef struct { unsigned char data[8192]; size_t len; } rt_wbuf;
static void rt_wput(rt_wbuf *b,const void *src,size_t n){
    CHECK(b->len+n<=sizeof b->data);
    memcpy(b->data+b->len,src,n); b->len+=n;
}
static void rt_wu32(rt_wbuf *b,uint32_t v){rt_wput(b,&v,4);}
static void rt_wf32(rt_wbuf *b,float v){rt_wput(b,&v,4);}
static void rt_wstr(rt_wbuf *b,const char *s){rt_wput(b,s,strlen(s));}
static void rt_enc5(const char *text,unsigned char *out,size_t *out_len){
    size_t n=strlen(text);
    for(size_t i=0;i<n;i++) out[i]=(unsigned char)(((unsigned char)text[n-1-i]+5)&0xFF);
    *out_len=n;
}
static size_t rt_build_lap(rt_wbuf *b){
    b->len=0;
    rt_wstr(b,"CHECKSUM=0\r\n");
    unsigned char enc[512]; size_t enc_len=0;
    rt_enc5("DRIVER=Import Test VERSION=2003.14",enc,&enc_len);
    rt_wu32(b,(uint32_t)enc_len); rt_wput(b,enc,enc_len);
    rt_wu32(b,2); rt_wu32(b,0);
    rt_wf32(b,0.1f); rt_wf32(b,0); rt_wf32(b,0); rt_wf32(b,0); rt_wf32(b,0);
    rt_wu32(b,7); rt_wf32(b,1.0f); rt_wf32(b,0);
    rt_wu32(b,0); rt_wu32(b,0); rt_wu32(b,0); rt_wu32(b,0); rt_wu32(b,0);
    rt_wu32(b,9); rt_wstr(b,"lap desc$");
    rt_enc5("RAIN=0,0",enc,&enc_len);
    rt_wu32(b,(uint32_t)enc_len); rt_wput(b,enc,enc_len);
    const char *names[11]={"time","distance","velocity","throttle","brake",
        "engine rpm","gear","yaw","lat accel","x","z"};
    rt_wu32(b,1); rt_wu32(b,11);
    for(int i=0;i<11;i++){rt_wu32(b,(uint32_t)strlen(names[i]));rt_wstr(b,names[i]);}
    const float rows[3][11]={
        {0.0f,100.0f,0.0f,0.0f,100.0f,0.0f,0.0f,-180.0f,0.0f,100.0f,5.0f},
        {0.1f,120.0f,22.371365f,50.0f,0.0f,8000.0f,3.0f,0.0f,0.5f,120.0f,6.0f},
        {0.2f,140.0f,44.74273f,100.0f,0.0f,12000.0f,4.0f,90.0f,-0.25f,140.0f,7.0f}};
    for(int s=0;s<3;s++) for(int c=0;c<11;c++) rt_wf32(b,rows[s][c]);
    return b->len;
}
static size_t rt_build_spt(rt_wbuf *b){
    b->len=0;
    rt_wu32(b,0);
    unsigned char seed[4]={1,2,3,4}; rt_wput(b,seed,4);
    rt_wf32(b,0.05f);
    rt_wstr(b,"Import Tester"); rt_wput(b,"\0",1);
    rt_wstr(b,"Team"); rt_wput(b,"\0",1);
    rt_wstr(b,"VEH"); rt_wput(b,"\0",1);
    rt_wstr(b,"Synthetic"); rt_wput(b,"\0",1);
    rt_wu32(b,57);
    for(int i=0;i<57;i++){
        unsigned char code=4; rt_wput(b,&code,1);
        char nm[16]; snprintf(nm,sizeof nm,"c%d",i);
        rt_wstr(b,nm); rt_wput(b,"\0",1);
    }
    for(int i=0;i<17;i++) rt_wu32(b,0);
    rt_wu32(b,0); /* embedded size 0 */
    for(int r=0;r<2;r++){
        unsigned char t=0x00; rt_wput(b,&t,1);
        float row[57]; memset(row,0,sizeof row);
        row[0]=r*0.1f; row[1]=r*3.0f; row[3]=r*4.0f;
        row[8]=8000.0f; row[11]=0.5f; row[13]=0.1f; row[15]=0.2f; row[54]=4.0f;
        for(int i=0;i<57;i++) rt_wf32(b,row[i]);
    }
    return b->len;
}
static void test_import(void){
    static rt_wbuf b;
    char err[LF_ERR_CAP];
    err[0]=0;
    size_t len=rt_build_lap(&b);
    int n=build_import_json(0,b.data,len,err,sizeof err);
    CHECK(n>0);
    CHECK(strstr(g_import_json[0],"\"source\":\"legacy\"")!=NULL);
    CHECK(strstr(g_import_json[0],"\"format\":\"lap\"")!=NULL);
    CHECK(strstr(g_import_json[0],"\"partial\":true")!=NULL); /* starts at 100 m */
    CHECK(strstr(g_import_json[0],"velocity mph->m/s")!=NULL);
    CHECK(strstr(g_import_json[0],"\"s\":10.00")!=NULL);      /* 22.371365 mph */
    CHECK(strstr(g_import_json[0],"\"rpm\":8000.0")!=NULL);   /* already RPM */
    CHECK(strstr(g_import_json[0],"\"ge\":3")!=NULL);
    CHECK(strstr(g_import_json[0],"\"gl\":4.90")!=NULL);      /* 0.5 g x 9.8 */
    CHECK(strstr(g_import_json[0],"\"y\":1.571")!=NULL);      /* 90 deg -> rad */
    CHECK(strstr(g_import_json[0],"\"tx\":120.0")!=NULL);
    CHECK(g_import_len[0]==n);
    /* Refusals: malformed lap block and non-telemetry bytes. */
    {
        static const unsigned char short_lap[19]={
            'C','H','E','C','K','S','U','M','=','1','\r','\n',
            0x02,0x00,0x00,0x00,'a','b','c'};
        CHECK(build_import_json(0,short_lap,sizeof short_lap,err,sizeof err)==-1 && err[0]!=0);
    }
    CHECK(build_import_json(0,(const unsigned char*)"not a telemetry file",21,err,sizeof err)==-1);
    /* Synthetic spt: distance/speed derived per DM#8 (3-4-5 triangle). */
    len=rt_build_spt(&b);
    n=build_import_json(1,b.data,len,err,sizeof err);
    CHECK(n>0);
    CHECK(strstr(g_import_json[1],"\"format\":\"spt\"")!=NULL);
    CHECK(strstr(g_import_json[1],"\"partial\":true")!=NULL);
    CHECK(strstr(g_import_json[1],"\"d\":5.0")!=NULL);
    CHECK(strstr(g_import_json[1],"\"s\":50.00")!=NULL);       /* 5 m / 0.1 s */
    CHECK(strstr(g_import_json[1],"\"rpm\":8000.0")!=NULL);
    CHECK(strstr(g_import_json[1],"\"ge\":4")!=NULL);
    CHECK(g_import_len[1]==n);
    /* Fix 2 (api_version 2): the import document carries a stable lid
     * (gen + import serial); /trace/compare accepts an imported lap on
     * either side, and a missing side is refused. */
    CHECK(strstr(g_import_json[0],"\"src\":\"import\",\"serial\":1")!=NULL);
    CHECK(strstr(g_import_json[1],"\"src\":\"import\",\"serial\":1")!=NULL);
    {
        static char cbuf[524288];
        int cn=build_trace_compare_json(cbuf,sizeof cbuf,0,-1,1,1,-1,1,5.0f);
        CHECK(cn>0 && strstr(cbuf,"\"schema\":\"f1c.compare/1\"")!=NULL);
        CHECK(strstr(cbuf,"\"a\":{\"which\":\"A\"")!=NULL);
        CHECK(strstr(cbuf,"\"b\":{\"which\":\"B\"")!=NULL);
        CHECK(strstr(cbuf,"\"rev\":0,\"slot\":-1,\"lap\":0,\"src\":\"import\",\"serial\":1")!=NULL);
        /* live lap vs import on either side, both orders */
        cn=build_trace_compare_json(cbuf,sizeof cbuf,0,2,0,1,-1,1,5.0f);
        CHECK(cn>0 && strstr(cbuf,"\"src\":\"live\"")!=NULL && strstr(cbuf,"\"src\":\"import\"")!=NULL);
        cn=build_trace_compare_json(cbuf,sizeof cbuf,0,-1,1,1,2,0,5.0f);
        CHECK(cn>0 && strstr(cbuf,"\"a_ch\":{")!=NULL);
        CHECK(build_trace_compare_json(cbuf,sizeof cbuf,0,-1,0,0,-1,1,5.0f)==-1);
        CHECK(build_trace_compare_json(cbuf,sizeof cbuf,0,999,0,1,-1,1,5.0f)==-1);
    }
}

/* ---- rc.6 sidecar integrity: SHA-256 vectors + /health report ---- */
static void test_audit_regressions(void) {
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: 42\r\n\r\n")==42);
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: 42junk\r\n\r\n")==-1);
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: +42\r\n\r\n")==-1);
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n")==-1);
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n")==-1);
    CHECK(http_content_length("POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n")==-1);
    char value[8], escaped[128];
    CHECK(qs_get("car=00000000000001","car",value,sizeof value));
    CHECK(!strcmp(value,"!"));
    format_failed=0;
    esc_json(escaped,sizeof escaped,"H\xe4kkinen \x96 lap");
    CHECK(!strcmp(escaped,"H\\u00e4kkinen \\u2013 lap"));
    esc_json(escaped,sizeof escaped,"H\xc3\xa4kkinen");
    CHECK(!strcmp(escaped,"H\xc3\xa4kkinen"));
    esc_json(escaped,sizeof escaped,"tail\xe0");
    CHECK(!strcmp(escaped,"tail\\u00e0"));
    /* All serializer values remain finite, but invalid raw readings carry
     * no validity bits in either the live frame or the recorder. */
    const int offsets[]={OFF_SPEED,OFF_THROTTLE,OFF_BRAKE,OFF_STEER_INPUT,
        OFF_ACCEL_LAT,OFF_ACCEL_LON,OFF_ACCEL_VERT,OFF_ENGINE_RPM,OFF_TRACK_X};
    float saved[sizeof offsets/sizeof offsets[0]];
    for (unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++) {
        saved[i]=*(float *)telemetry_test_address(VEH_BASE+VEH_STRIDE+offsets[i],4);
        set_f(VEH_BASE+VEH_STRIDE+offsets[i],NAN);
    }
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(!(g_telemetry.cars[1].car_valid & (1|2|4|8|32|64)));
    trace_session_clear(); trace_select(0,1,1);
    g_telemetry.lap_len=5000; g_telemetry.cars[1].on_track=1;
    trace_update(MAX_CARS);
    CHECK(g_trace[0].sample_count>0);
    trace_sample_t *sample=&g_trace[0].laps[g_trace[0].current_index][0];
    CHECK(!(sample->flags & (TRF_SPEED|TRF_THROTTLE|TRF_BRAKE|TRF_STEER|TRF_GLAT|TRF_GLON|TRF_GVERT|TRF_RPM|TRF_TRACK)));
    for (unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++)
        set_f(VEH_BASE+VEH_STRIDE+offsets[i],saved[i]);
    capture_on_game_thread(NULL); read_telemetry();
    /* A new AIW which cannot be opened invalidates all old distances. */
    strcpy((char *)mirror_ADDR_AIW_PATH.bytes,"SEASONDATA\\__audit_missing_track__.AIW");
    g_aiw_seen[0]=0; g_telemetry.sec1=100;g_telemetry.sec2=200;g_telemetry.lap_len=300;
    sectors_update();
    CHECK(g_telemetry.sec1==0 && g_telemetry.sec2==0 && g_telemetry.lap_len==0);
    /* A sticky formatter failure after conversion must preserve the cache. */
    static rt_wbuf body; static char previous[sizeof g_import_json[0]];
    size_t len=rt_build_lap(&body); char err[128];
    format_failed=0; CHECK(build_import_json(0,body.data,len,err,sizeof err)>0);
    memcpy(previous,g_import_json[0],sizeof previous);
    unsigned serial=g_import_serial[0]; int count=g_import_count[0];
    format_failed=1; CHECK(build_import_json(0,body.data,len,err,sizeof err)<0);
    CHECK(!memcmp(previous,g_import_json[0],sizeof previous));
    CHECK(g_import_serial[0]==serial && g_import_count[0]==count);
    /* Restore the normal HTTP fixtures, which later gates expect. */
    format_failed=0; g_import_serial[0]=0;g_import_serial[1]=0;
    test_trace(); test_import();
}

/* ---- 4.4.0-rc.1 CO-OP: controller mode decides the local player; mode-2
 * (remote) cars keep pose/yaw/rpm, lose simulated channels, and get a speed
 * derived from their interpolated position. Mirrors the CO-OP client case:
 * the host's human car (slot 0) is remote with ai_control 0, the local human
 * sits in slot 5, and a retired bot reads ai_control 0 in mode 1. ---- */
static void set_session_time(double t) { memcpy(telemetry_test_address(GAME_ADDR_SESSION_TIME,8),&t,8); }
static void test_coop(void) {
    static char json[262144];
    double saved_time; memcpy(&saved_time,telemetry_test_address(GAME_ADDR_SESSION_TIME,8),8);
    unsigned saved_gen=capture_generation;
    for (int i=0;i<MAX_CARS;i++) {
        uintptr_t v=VEH_BASE+(size_t)i*VEH_STRIDE;
        set_i(v+OFF_CONTROLLER_MODE,1); set_i(v+OFF_AI_CONTROL_FLAG,1);
    }
    uintptr_t host=VEH_BASE, me=VEH_BASE+5*(size_t)VEH_STRIDE, bot=VEH_BASE+7*(size_t)VEH_STRIDE;
    set_i(host+OFF_CONTROLLER_MODE,2); set_i(host+OFF_AI_CONTROL_FLAG,0);
    set_i(me+OFF_CONTROLLER_MODE,0);   set_i(me+OFF_AI_CONTROL_FLAG,0);
    set_i(bot+OFF_AI_CONTROL_FLAG,0);  /* retired-bot shape: ai 0, mode 1 */
    set_f(host+OFF_PHYS_X,100.0f); set_f(host+OFF_PHYS_Z,0.0f);
    set_f(host+OFF_SPEED,0.0f);        /* the network apply path leaves it 0 */
    /* 1.2.2: the receiver writes brake 0/1.0 and steer; throttle is whatever was there (never written by the receiver) */
    set_f(host+OFF_BRAKE,1.0f); set_f(host+OFF_STEER_INPUT,-0.25f); set_f(host+OFF_THROTTLE,0.7f);
    set_session_time(500.0); capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.player_slot==5);
    CHECK(g_telemetry.cars[0].controller_role==2);
    /* 1.2.2: brake (4) and steer (8) kept for the remote car without the ai_control gate; throttle, tread, accel cleared */
    CHECK((g_telemetry.cars[0].car_valid & (2|16|32|64))==0);
    CHECK((g_telemetry.cars[0].car_valid & (4|8))==(4|8));
    CHECK(fabsf(g_telemetry.cars[0].steer+0.25f)<1e-6f && fabsf(g_telemetry.cars[0].brake-1.0f)<1e-6f);
    CHECK((g_telemetry.cars[0].vf6 & 512)!=0 && (g_telemetry.cars[0].vf6 & 4096)!=0 &&
          (g_telemetry.cars[0].vf6 & ~(2|512|4096))==0);
    /* the local player (mode 0, ai 0) still gets no steer and no on/off brake marker */
    CHECK((g_telemetry.cars[5].car_valid & 8)==0 && (g_telemetry.cars[5].vf6 & 4096)==0);
    /* 5 m in 0.1 s of sim time, three times: derived 50 m/s */
    for (int k=1;k<=3;k++) {
        set_f(host+OFF_PHYS_X,100.0f+5.0f*k);
        set_session_time(500.0+0.1*k); capture_on_game_thread(NULL); read_telemetry();
    }
    CHECK(fabsf(g_telemetry.cars[0].speed-50.0f)<0.5f && (g_telemetry.cars[0].car_valid&1));
    /* the local player keeps its FACT tyre block; the remote car does not */
    CHECK((g_telemetry.cars[5].vf6 & 4)!=0 && (g_telemetry.cars[0].vf6 & 4)==0);
    format_failed=0; int n=json_telemetry(json,sizeof json);
    CHECK(!format_failed && n>0);
    CHECK(strstr(json,"\"steer\":-0.250,\"steerwhy\":\"\"")!=NULL);
    CHECK(strstr(json,"remote car (CO-OP): simulated on the other PC")==NULL);
    CHECK(strstr(json,"CO-OP remote DERIVED on/off only")!=NULL);
    /* an out-of-range network steer: bit 8 clear and the remote reason, brake kept */
    set_f(host+OFF_STEER_INPUT,9.0f);
    set_session_time(500.5); capture_on_game_thread(NULL); read_telemetry();
    CHECK((g_telemetry.cars[0].car_valid & 8)==0 && (g_telemetry.cars[0].car_valid & 4)!=0);
    format_failed=0; n=json_telemetry(json,sizeof json);
    CHECK(!format_failed && n>0);
    CHECK(strstr(json,"\"steerwhy\":\"remote car (CO-OP): no valid network steer in this sample\"")!=NULL);
    /* brake off: still valid (on/off), marker stays */
    set_f(host+OFF_BRAKE,0.0f);
    set_session_time(500.6); capture_on_game_thread(NULL); read_telemetry();
    CHECK((g_telemetry.cars[0].car_valid & 4)!=0 && (g_telemetry.cars[0].vf6 & 4096)!=0 && g_telemetry.cars[0].brake==0.0f);
    /* restore the fixture shape used by the other tests */
    for (int i=0;i<MAX_CARS;i++) {
        uintptr_t v=VEH_BASE+(size_t)i*VEH_STRIDE;
        set_i(v+OFF_CONTROLLER_MODE,i?1:0); set_i(v+OFF_AI_CONTROL_FLAG,i?1:0);
    }
    set_f(host+OFF_SPEED,50); set_f(host+OFF_PHYS_X,0.0f);
    set_f(host+OFF_BRAKE,0.1f); set_f(host+OFF_STEER_INPUT,0.0f); set_f(host+OFF_THROTTLE,0.5f);   /* fixture values */
    set_session_time(saved_time);
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.player_slot==0);
    (void)saved_gen;
}

/* ========================================================================
 * rc.11 weather — the bounded session weather ring: cadence, change trigger,
 * paused frames, generation clear, eviction and nulls. Driven through the
 * real capture path (broker callback + read_telemetry) with mirrors only.
 * ======================================================================== */
/* rc.11 weather (Claude): drive weather_update() directly on the published
 * values. The first version went through capture + read_telemetry with new
 * session generations, which cleared the trace fixtures the canary and serve
 * mode rely on. capture_generation and the ring are restored after the test. */
static void weather_frame(unsigned gen, float st, float at, float trkt, float onw, float offw) {
    capture_generation = gen;
    g_telemetry.session_time = st;
    g_telemetry.ambient_temp = at;
    g_telemetry.track_temp = trkt;
    g_telemetry.on_wetness = onw;
    g_telemetry.off_wetness = offw;
    weather_update();
}

static void test_weather_ring(void) {
    unsigned saved_gen = capture_generation;
    double saved_st = g_telemetry.session_time;
    float saved_at = g_telemetry.ambient_temp, saved_trkt = g_telemetry.track_temp;
    float saved_onw = g_telemetry.on_wetness, saved_offw = g_telemetry.off_wetness;
    static char wj[WEATHER_JSON_BYTES];
    /* first rows: the ring is empty, then (a) the 10 s cadence appends. */
    weather_frame(9401, 0.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 1 && g_weather_evicted == 0);
    weather_frame(9401, 5.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 1);
    weather_frame(9401, 10.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 2);

    /* (c) a paused frame (equal session time) appends nothing, even with a
     * different value: a pause is not a new sample. */
    weather_frame(9401, 10.0f, 25.0f, 35.0f, 0.5f, 0.5f);
    CHECK(g_weather_count == 2);

    /* (b) change triggers: onw +0.02 appends; at +0.05 does not; at +0.12 does. */
    weather_frame(9401, 10.5f, 20.0f, 30.0f, 0.02f, 0.0f);
    CHECK(g_weather_count == 3);
    weather_frame(9401, 10.6f, 20.05f, 30.0f, 0.02f, 0.0f);
    CHECK(g_weather_count == 3);
    weather_frame(9401, 10.7f, 20.12f, 30.0f, 0.02f, 0.0f);
    CHECK(g_weather_count == 4);

    /* (d) a new generation clears the ring (same rule as the traces). */
    weather_frame(9402, 11.0f, 20.12f, 30.0f, 0.02f, 0.0f);
    CHECK(g_weather_count == 1 && g_weather_evicted == 0);

    /* (f) the -900 sentinel becomes a JSON null row; the document carries the
     * cadence, capacity and the RESEARCH_WEATHER 1b tiers. */
    weather_frame(9403, 77.0f, -999.0f, 30.0f, 0.0f, 0.0f);
    format_failed = 0;
    int n = build_weather_json(wj, sizeof wj);
    CHECK(!format_failed && n > 0 && n < (int)sizeof wj);
    CHECK(strstr(wj, "[77.0,null,30.0,0.000,0.000,") != NULL);
    CHECK(strstr(wj, "-999") == NULL);
    CHECK(strstr(wj, "\"interval_s\":10.0") != NULL && strstr(wj, "\"capacity\":2048") != NULL);
    CHECK(strstr(wj, "\"evicted\":0") != NULL && strstr(wj, "\"api_version\":\"10\"") != NULL);
    CHECK(strstr(wj, "\"tgf\":\"DERIVED\"") != NULL && strstr(wj, "\"at\":\"CONFIRMED\"") != NULL);
    CHECK(strstr(wj, "\"onw\":\"0..1\"") != NULL && strstr(wj, "\"tgf\":\"raw\"") != NULL);

    /* (e) eviction: fill the ring at the 11 s cadence, then one more append
     * drops the oldest row and counts it. */
    for (int i = 0; i < WEATHER_RING + 1; i++)
        weather_frame(9404, (float)i * 11.0f, 20.0f, 30.0f, (float)(i % 2) * 0.5f, 0.0f);
    CHECK(g_weather_count == WEATHER_RING);
    CHECK(g_weather_evicted == 1);
    format_failed = 0;
    n = build_weather_json(wj, sizeof wj);
    CHECK(!format_failed && n > 0);
    CHECK(strstr(wj, "[22528.0,") != NULL);       /* newest row kept */
    CHECK(strstr(wj, "[0.0,") == NULL);           /* oldest row evicted */
    CHECK(strstr(wj, "\"evicted\":1") != NULL);

    /* The ring document is bounded: no growth past the capacity. */
    CHECK(n < WEATHER_JSON_BYTES);

    /* rc.11 weather (Claude review): a session-clock rewind without a new
     * generation restarts the history instead of stalling it. */
    weather_frame(9405, 100.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    weather_frame(9405, 110.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 2);
    weather_frame(9405, 3.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 1);
    weather_frame(9405, 13.0f, 20.0f, 30.0f, 0.0f, 0.0f);
    CHECK(g_weather_count == 2);

    /* restore the shared fixture state for the later tests and serve mode */
    capture_generation = saved_gen;
    g_telemetry.session_time = saved_st;
    g_telemetry.ambient_temp = saved_at; g_telemetry.track_temp = saved_trkt;
    g_telemetry.on_wetness = saved_onw; g_telemetry.off_wetness = saved_offw;
    weather_ring_clear(); g_weather_gen = 0xFFFFFFFFu;
    weather_update();   /* one fixture row so serve mode's /weather is not empty */
}

static void test_sidecar_integrity(void) {
    static const char *vectors[3][2] = {
        {"", "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"},
        {"abc", "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1"}
    };
    for (unsigned i=0;i<3;i++) {
        unsigned char digest[32]; char hex[65];
        sha256_buf(vectors[i][0],strlen(vectors[i][0]),digest);
        sha256_hex(digest,hex);
        CHECK(strcmp(hex,vectors[i][1])==0);
    }
    static char health[2048];
    int n=build_health_json(health,sizeof health);
    CHECK(n>0 && n<(int)sizeof health);
    CHECK(strstr(health,"\"api_version\":\"10\"")!=NULL);
    /* 4.4.0 (all-cars recorder): the mode and the store size are published. */
    CHECK(strstr(health,"\"trace_mode\":\"")!=NULL && strstr(health,"\"trace_bytes\":")!=NULL &&
          strstr(health,"\"trace_cars\":")!=NULL);
    /* BACKEND 4: native callback cost + rate and process working set in /health. */
    CHECK(strstr(health,"\"capture_cost_us\":{\"samples\":")!=NULL);
    CHECK(strstr(health,"\"rate_per_s\":")!=NULL);
    CHECK(strstr(health,"\"memory\":{\"working_set_kb\":")!=NULL);
    {
        const char *p=strstr(health,"\"capture_cost_us\":{\"samples\":");
        CHECK(p!=NULL && atoi(strstr(p,"\"samples\":")+10)>0);
        p=strstr(health,"\"capture_cost_us\":");
        p=strstr(p,"\"p95\":");
        CHECK(p!=NULL && atof(p+6)<500.0);   /* checklist pass limit */
    }
    if (workbench_js_load()) {
        unsigned char digest[32]; char hex[65];
        sha256_buf(g_workbench_js_cache,(size_t)g_workbench_js_len,digest);
        sha256_hex(digest,hex);
        CHECK(strcmp(hex,g_workbench_js_sha)==0);
        CHECK(strstr(health,"\"workbench_ui\":{\"present\":true")!=NULL);
        CHECK(strstr(health,g_workbench_js_sha)!=NULL);
        CHECK(strstr(health,"\"expected\":\"" WORKBENCH_UI_EXPECTED_SHA256 "\"")!=NULL);
        CHECK(strstr(health,"\"matches\":true")!=NULL);
    } else {
        /* Missing sidecar is a valid state: /health must say present:false
         * and matches:false while still reporting the expected hash. */
        CHECK(strstr(health,"\"workbench_ui\":{\"present\":false")!=NULL);
        CHECK(strstr(health,"\"matches\":false")!=NULL);
        CHECK(strstr(health,WORKBENCH_UI_EXPECTED_SHA256)!=NULL);
    }
}

static double g_perf_p50, g_perf_p95, g_perf_max;
static double g_cap_p50, g_cap_p95, g_cap_max;
static void test_performance(void) {
    LARGE_INTEGER freq,t0,t1;
    double samples[60], cap[120];
    QueryPerformanceFrequency(&freq);
    for (int i=0;i<60;i++) {
        QueryPerformanceCounter(&t0);
        capture_on_game_thread(NULL);
        read_telemetry();
        QueryPerformanceCounter(&t1);
        samples[i]=(double)(t1.QuadPart-t0.QuadPart)*1e6/(double)freq.QuadPart;
    }
    /* Game-callback path alone (single-threaded by design: the owner CPU
     * rule keeps per-sample callback cost measurements serial). */
    for (int i=0;i<120;i++) {
        QueryPerformanceCounter(&t0);
        capture_on_game_thread(NULL);
        QueryPerformanceCounter(&t1);
        cap[i]=(double)(t1.QuadPart-t0.QuadPart)*1e6/(double)freq.QuadPart;
    }
    for (int i=0;i<60;i++) for (int j=i+1;j<60;j++)
        if (samples[j]<samples[i]) { double t=samples[i]; samples[i]=samples[j]; samples[j]=t; }
    for (int i=0;i<120;i++) for (int j=i+1;j<120;j++)
        if (cap[j]<cap[i]) { double t=cap[i]; cap[i]=cap[j]; cap[j]=t; }
    g_perf_p50=samples[30]; g_perf_p95=samples[57]; g_perf_max=samples[59];
    g_cap_p50=cap[60]; g_cap_p95=cap[114]; g_cap_max=cap[119];
    CHECK(samples[57]<20000.0);   /* sanity bound only; native gate stays pending */
    CHECK(cap[114]<500.0);        /* synthetic mirror copy; measured, not estimated */
}

/* ========================================================================
 * 4.4.0 ALL-CARS TRACE (owner rule 2026-10-12): every live car is recorded
 * all the time. The store is allocated once per live car count; selection
 * never resets an existing trace; an allocation failure keeps the old
 * selected-only mode and /health says selected_fallback.
 * ======================================================================== */
static void lifecycle_prepare(unsigned gen, float lap_len, int count);
static void allcars_reset(void) {
    trace_session_clear();
    if (g_trace_store) { VirtualFree(g_trace_store,0,MEM_RELEASE); g_trace_store=NULL; }
    g_trace_store_cap=0; g_trace_store_bytes=0;
    g_trace_mode=TRACE_MODE_SELECTED;
    g_trace_store_failed=0; g_trace_alloc_test_fail=0;
    g_trace_auto[0]=1; g_trace_auto[1]=1;
}

/* Four moving cars, nothing selected: the old mode would record nothing. */
static void allcars_fixture(int count) {
    capture_generation=77; g_telemetry.lap_len=6775.0f; g_telemetry.player_slot=0;
    g_telemetry.vehicle_count=count; g_telemetry.session_time=10.0f;
    for (int k=0;k<count;k++) {
        trace_vehicle(k,k?1:0,100.0f+k*10.0f,50.0f,0.0f,k?0.2f:0.0f,0.9f);
        g_telemetry.cars[k].lap_number=1;
        g_telemetry.cars[k].on_track=1; g_telemetry.cars[k].pitting=0;
        g_telemetry.cars[k].finish_state=0; g_telemetry.cars[k].retired=0;
    }
}
static void allcars_advance(int count,float sim) {
    g_telemetry.session_time=sim;
    for (int i=0;i<count;i++) {
        g_telemetry.cars[i].lap_dist=100.0f+i*10.0f+(sim-10.0f)*50.0f;
        set_f(VEH_BASE+(size_t)i*VEH_STRIDE+OFF_TRACK_PART_EXACT,g_telemetry.cars[i].lap_dist);
    }
}

static void test_trace_all_cars(void) {
    static char sbuf[131072];
    allcars_reset();
    trace_select(0,-1,0); trace_select(1,-1,0);
    g_trace_auto[0]=0; g_trace_auto[1]=0;   /* nothing selected, like a fresh page */
    allcars_fixture(4);
    CHECK(trace_store_ensure(4)==1 && g_trace_mode==TRACE_MODE_ALL && g_trace_store!=NULL);
    CHECK(g_trace_store_bytes==(unsigned long long)4*sizeof(trace_car_t));
    CHECK(TRACE_SAMPLES>=3000);              /* 2:30 at the measured ~20 Hz + margin */
    for (int k=0;k<10;k++) { allcars_advance(4,10.0f+k*0.5f); trace_update(4); }
    CHECK(g_trace_store[0].sample_count>0 && g_trace_store[1].sample_count>0 &&
          g_trace_store[2].sample_count>0 && g_trace_store[3].sample_count>0);
    /* Status lists every car, in all mode, and any car's open lap is
     * addressable by numeric slot. */
    int n=build_trace_status_json(sbuf,sizeof sbuf);
    CHECK(n>0 && strstr(sbuf,"\"mode\":\"all\"")!=NULL && strstr(sbuf,"\"samples_max\":3200")!=NULL);
    CHECK(strstr(sbuf,"\"slot\":2")!=NULL && strstr(sbuf,"\"slot\":3")!=NULL);
    int open=build_trace_open_json(sbuf,sizeof sbuf,1003);
    CHECK(open>0 && strstr(sbuf,"\"open\":true")!=NULL && strstr(sbuf,"\"slot\":3")!=NULL);
    /* A completed lap on a never-selected car is addressable by its slot. */
    {
        trace_car_t *f=&g_trace_store[2];
        f->lap_valid[0]=TRL_COMPLETE; f->lap_num[0]=7; f->lap_samples[0]=3;
        f->lap_time[0]=95.0f; f->lap_start_sim[0]=0.0f;
        int data=build_trace_data_json(sbuf,sizeof sbuf,1002,7);
        CHECK(data>0 && strstr(sbuf,"\"slot\":2")!=NULL && strstr(sbuf,"\"which\":\"\"")!=NULL);
        int csv=build_trace_data_csv(sbuf,sizeof sbuf,1002,7);
        CHECK(csv>0 && strstr(sbuf,"# f1c.trace/1")!=NULL && strstr(sbuf,"which=-")!=NULL);
    }
    /* /health publishes the mode and the store size. */
    {
        char hb[2048];
        int hn=build_health_json(hb,sizeof hb);
        CHECK(hn>0 && strstr(hb,"\"trace_mode\":\"all\"")!=NULL && strstr(hb,"\"trace_cars\":4")!=NULL);
    }
    {
        int samples=g_trace_store[2].sample_count;
        unsigned rev=g_trace_store[2].revision;
        trace_capture_epoch(0,4); /* roster pointer churn within one session */
        CHECK(g_trace_store[2].sample_count==samples && g_trace_store[2].revision==rev &&
              (g_trace_store[2].lap_valid[0] & TRL_COMPLETE));
        trace_capture_epoch(1,4); /* new broker generation */
        CHECK(g_trace_store[2].sample_count==0 && g_trace_store[2].lap_valid[0]==0);
    }
    allcars_reset();
}

/* Owner rule: selecting a driver must show the already-recorded trace; the
 * selection switch must NOT reset it (all-cars mode has one ring per car). */
static void test_trace_select_keeps(void) {
    allcars_reset();
    allcars_fixture(4);
    CHECK(trace_store_ensure(4)==1);
    for (int k=0;k<6;k++) { allcars_advance(4,20.0f+k*0.5f); trace_update(4); }
    int before=g_trace_store[1].sample_count;
    unsigned rev=g_trace_store[1].revision;
    CHECK(before>0);
    trace_select(0,1,1);                     /* side A now points at slot 1 */
    trace_select(1,3,1);                     /* side B points at a never-selected car */
    allcars_advance(4,23.0f);
    trace_update(4);
    CHECK(g_trace_store[1].sample_count>before && g_trace_store[1].revision==rev);
    CHECK(g_trace_store[3].sample_count>0);
    allcars_reset();
}

/* Allocation failure: keep the old selected-only mode, report it in /health,
 * and a later attempt may recover. */
static void test_trace_alloc_fallback(void) {
    allcars_reset();
    g_trace_alloc_test_fail=1;
    CHECK(trace_store_ensure(4)==0 && g_trace_mode==TRACE_MODE_SELECTED &&
          g_trace_store==NULL && g_trace_store_failed==1);
    {
        char hb[2048];
        int hn=build_health_json(hb,sizeof hb);
        CHECK(hn>0 && strstr(hb,"\"trace_mode\":\"selected_fallback\"")!=NULL);
    }
    /* The old selected path still records; the failing state never crashes it. */
    capture_generation=78; g_telemetry.lap_len=6775.0f; g_telemetry.player_slot=0;
    g_telemetry.vehicle_count=2;
    for (int k=0;k<2;k++) {
        trace_vehicle(k,k,200.0f,50.0f,0.0f,0.1f,0.9f);
        g_telemetry.cars[k].lap_number=1; g_telemetry.cars[k].on_track=1;
    }
    trace_select(0,0,1); g_trace_auto[0]=0;
    for (int k=0;k<8;k++) {
        g_telemetry.session_time=30.0f+k*0.5f;
        g_telemetry.cars[0].lap_dist=200.0f+k*25.0f;
        set_f(VEH_BASE+OFF_TRACK_PART_EXACT,g_telemetry.cars[0].lap_dist);
        trace_update(4);
    }
    CHECK(g_trace[0].used && g_trace[0].sample_count>0);
    /* A later session that can allocate switches back to all-cars. */
    g_trace_alloc_test_fail=0;
    CHECK(trace_store_ensure(4)==1 && g_trace_mode==TRACE_MODE_ALL);
    allcars_reset();
}

/* Per-tick recorder cost of the all-cars workload vs the old selected pair on
 * the same 24-car synthetic field. The trace work runs in read_telemetry (the
 * worker consumes the captured mirrors); each timed read feeds the existing
 * capture_cost_snapshot ring, which keeps the last 240 samples, so the two
 * phases are isolated. A real AIW file is needed: without a parsed track
 * sectors_update() zeroes lap_len on every capture and no car can record. */
static void test_performance_allcars(void) {
    const int cars=24;
    double p50a,p95a,maxa,ratea; int na;
    double p50b,p95b,maxb,rateb; int nb;
    char exe[MAX_PATH];
    if (GetModuleFileNameA(NULL,exe,sizeof exe) && strrchr(exe,'\\')) {
        char aiw_dir[MAX_PATH+16], aiw_file[MAX_PATH+32];
        char *slash=strrchr(exe,'\\'); slash[1]=0;
        snprintf(aiw_dir,sizeof aiw_dir,"%swebtel_aiw",exe);
        snprintf(aiw_file,sizeof aiw_file,"%s\\test.AIW",aiw_dir);
        CreateDirectoryA(aiw_dir,NULL);
        FILE *f=fopen(aiw_file,"w");
        if (f) { fputs("lap_length=6775.0\nsector_1_length=2000.0\nsector_2_length=4000.0\nGrooveWidth=8.0\nGrooveWidthWet=4.0\n",f); fclose(f); }
    }
    {
        const char *rel="webtel_aiw\\test.AIW";
        memcpy(telemetry_test_address(GAME_ADDR_AIW_PATH,strlen(rel)+1),rel,strlen(rel)+1);
    }
    lifecycle_prepare(4242u,6775.0f,cars);   /* selects slots 0/1 (old workload) */
    set_session_time(0.0);
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.lap_len>1000.0f);      /* the fixture track parsed */
    for (int t=0;t<320;t++) {
        LARGE_INTEGER t0;
        set_session_time((double)t*0.05);
        for (int k=0;k<cars;k++) {
            float sp=52.0f+(float)((t*7+k*13)%60);
            float d=fmodf(100.0f+(float)(k*29)+sp*(float)t*0.05f,6775.0f);
            int lap=1+(int)((100.0f+(float)(k*29)+sp*(float)t*0.05f)/6775.0f);
            set_i(VEH_BASE+(size_t)k*VEH_STRIDE+OFF_LAP_NUMBER,lap);
            trace_vehicle(k,k?1:0,d,sp,0.0f,k?0.2f:0.0f,0.9f);
            g_telemetry.cars[k].lap_number=lap;
            g_telemetry.cars[k].on_track=1; g_telemetry.cars[k].pitting=0;
            g_telemetry.cars[k].finish_state=0; g_telemetry.cars[k].retired=0;
        }
        capture_on_game_thread(NULL);
        QueryPerformanceCounter(&t0);
        read_telemetry();
        capture_cost_record(t0);
    }
    capture_cost_snapshot(&p50a,&p95a,&maxa,&ratea,&na);
    CHECK(na>0 && g_trace[0].sample_count>0 && g_trace[1].sample_count>0);   /* the pair really recorded */
    CHECK(trace_store_ensure(cars)==1);
    for (int t=320;t<640;t++) {
        LARGE_INTEGER t0;
        set_session_time((double)t*0.05);
        for (int k=0;k<cars;k++) {
            float sp=52.0f+(float)((t*7+k*13)%60);
            float d=fmodf(100.0f+(float)(k*29)+sp*(float)t*0.05f,6775.0f);
            int lap=1+(int)((100.0f+(float)(k*29)+sp*(float)t*0.05f)/6775.0f);
            set_i(VEH_BASE+(size_t)k*VEH_STRIDE+OFF_LAP_NUMBER,lap);
            trace_vehicle(k,k?1:0,d,sp,0.0f,k?0.2f:0.0f,0.9f);
            g_telemetry.cars[k].lap_number=lap;
            g_telemetry.cars[k].on_track=1; g_telemetry.cars[k].pitting=0;
            g_telemetry.cars[k].finish_state=0; g_telemetry.cars[k].retired=0;
        }
        capture_on_game_thread(NULL);
        QueryPerformanceCounter(&t0);
        read_telemetry();
        capture_cost_record(t0);
    }
    capture_cost_snapshot(&p50b,&p95b,&maxb,&rateb,&nb);
    {
        int recorded=0; long total=0;
        for (int i=0;i<cars;i++) {
            if (g_trace_store[i].sample_count<=0) continue;
            recorded++; total+=g_trace_store[i].sample_count;
        }
        printf("PERFALL selected p50=%.0fus p95=%.0fus max=%.0fus n=%d; allcars p50=%.0fus p95=%.0fus max=%.0fus n=%d (%d cars, read_telemetry incl. trace_update, capture_cost_snapshot; all-cars phase recorded=%d total_samples=%ld)\n",
               p50a,p95a,maxa,na,p50b,p95b,maxb,nb,cars,recorded,total);
        fflush(stdout);
        CHECK(recorded>=cars-1);      /* the all-cars phase really traced the field */
    }
    CHECK(na>0 && nb>0);
    CHECK(p95b<20000.0);   /* sanity bound; the doubling rule is reported, not asserted */
    /* restore the no-track fixture shape for the later serve-mode fixtures */
    ((char*)telemetry_test_address(GAME_ADDR_AIW_PATH,1))[0]=0;
    capture_on_game_thread(NULL); read_telemetry();
    allcars_reset();
}

/* ========================================================================
 * BACKEND 3 — lifecycle fixtures. These replay the exact native Hockenheim
 * tuples (NATIVE_BOUNDARY_REPAIR_2026-10-09) and deliberately break the
 * recorder's assumptions. They are ordered, single-threaded sequence
 * fixtures (owner CPU rule exception) and never attach to the game.
 * ======================================================================== */
static void lifecycle_prepare(unsigned gen, float lap_len, int count) {
    capture_generation=gen;
    g_telemetry.lap_len=lap_len;
    g_telemetry.player_slot=0; g_telemetry.vehicle_count=count;
    g_telemetry.session_time=0;
    for (int w=0;w<4;w++) g_telemetry.tread[w]=0.9f;
    for (int i=0;i<count;i++) {
        g_telemetry.cars[i].position=i+1;
        g_telemetry.cars[i].on_track=1;
        g_telemetry.cars[i].pitting=0;
        g_telemetry.cars[i].finish_state=0;
        g_telemetry.cars[i].retired=0;
        g_telemetry.cars[i].ai_control=i?1:0;
        g_telemetry.cars[i].lap_number=0;
        g_telemetry.cars[i].lap_dist=0.0f;
        g_telemetry.cars[i].speed=0.0f;  /* rc.5: a moving lap-0 car is now recorded */
    }
    trace_session_clear();
    trace_select(0,0,1); trace_select(1,1,1);
    /* Establish the recording identity (used/revision/lap-length) without
     * storing a sample: the per-car lap number is still 0 at this point. */
    trace_update(count);
}

static void hock_sample(int slot,int ai,int lap,float dist,float sim) {
    g_telemetry.session_time=sim;
    set_i(VEH_BASE+(size_t)slot*VEH_STRIDE+OFF_LAP_NUMBER,lap);
    trace_vehicle(slot,ai,dist,70.0f,0.0f,ai?0.2f:0.0f,0.9f);
    g_telemetry.cars[slot].lap_number=lap;
    trace_sample_car(slot,24,sim);
}

static void test_hockenheim_sequences(void) {
    /* Michael (slot 0): 635.6 c5 d6775.6592 -> 635.7 c6 d6781.3198 ->
     * 635.8 c6 d8.0820. Ralf (slot 1): 637.5 c5 d6770.5161 ->
     * 637.6 c6 d6775.6592 -> 637.7 c6 d2.3410. Track 6784.1 m. */
    lifecycle_prepare(7,6784.1f,24);
    hock_sample(0,0,5,6775.6592f,635.6f);
    hock_sample(0,0,6,6781.3198f,635.7f);
    CHECK(g_trace[0].lap_number==5);          /* counter-only step keeps the lap */
    hock_sample(0,0,6,8.0820f,635.8f);
    CHECK(g_trace[0].lap_number==6 && g_trace[0].start_seen);
    for(int k=1;k<200;k++) hock_sample(0,0,6,8.082f+k*33.9f,635.8f+k*0.5f);
    hock_sample(0,0,7,6775.6592f,736.0f);     /* counter before wrap, dt 0.5 */
    CHECK(g_trace[0].lap_number==6);
    hock_sample(0,0,7,2.341f,736.5f);         /* wrap closes Michael lap 6 */
    CHECK(trace_find_lap(&g_trace[0],6)>=0);
    CHECK(g_trace[0].lap_total==1);           /* lap 5 had no witnessed start */
    CHECK(g_trace[0].dropped_laps==1);

    lifecycle_prepare(8,6784.1f,24);
    hock_sample(1,1,5,6770.5161f,637.5f);
    hock_sample(1,1,6,6775.6592f,637.6f);
    CHECK(g_trace[1].lap_number==5);
    hock_sample(1,1,6,2.341f,637.7f);
    CHECK(g_trace[1].lap_number==6 && g_trace[1].start_seen);
    for(int k=1;k<200;k++) hock_sample(1,1,6,2.341f+k*33.9f,637.7f+k*0.5f);
    hock_sample(1,1,7,6775.6592f,737.7f);
    CHECK(g_trace[1].lap_number==6);
    hock_sample(1,1,7,2.341f,737.8f);         /* wrap closes Ralf lap 6 */
    CHECK(trace_find_lap(&g_trace[1],6)>=0);
    CHECK(g_trace[1].lap_total==1 && g_trace[1].dropped_laps==1);

    /* Both completed laps carry a stable lid; the status document is valid. */
    {
        static char lb[131072];
        CHECK(build_trace_status_json(lb,sizeof lb)>0);
        CHECK(strstr(lb,"\"lap\":6")!=NULL);
        CHECK(strstr(lb,"\"src\":\"live\"")!=NULL);
        CHECK(build_trace_data_json_id(lb,sizeof lb,1,6,8,-1)>0);
        CHECK(strstr(lb,"\"lid\":{\"gen\":8,\"rev\":")!=NULL);
    }
}

static void test_lifecycle(void) {
    static char buf[524288];
    int n;
    /* --- first partial lap / mid-lap selection: no fake completion until a
     * witnessed crossing, then the next full lap closes normally. --- */
    lifecycle_prepare(20,5000,24);
    for(int k=0;k<100;k++) hock_sample(0,0,3,1000.0f+k*25,100+0.5f*k);
    for(int k=0;k<60;k++) hock_sample(0,0,3,3500.0f+k*25,150+0.5f*k);
    CHECK(g_trace[0].lap_total==0 && g_trace[0].lap_open);
    hock_sample(0,0,4,2.0f,180.0f);            /* crossing, but start unwitnessed */
    CHECK(g_trace[0].lap_total==0 && g_trace[0].dropped_laps==1 && g_trace[0].start_seen);
    for(int k=0;k<200;k++) hock_sample(0,0,4,2.0f+k*25.0f,180.0f+0.5f*k);
    hock_sample(0,0,5,2.0f,280.5f);
    CHECK(g_trace[0].lap_total==1 && trace_find_lap(&g_trace[0],4)>=0);

    /* AI-control transitions preserve the driver's recorded laps. Remote
     * ownership changes still reset the slot and invalidate old pins. */
    unsigned rev_before=g_trace[0].revision;
    g_telemetry.cars[0].ai_control=1;
    trace_update(24);
    CHECK(g_trace[0].revision==rev_before && g_trace[0].lap_total==1);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,4,-1,(long)rev_before)>0);
    g_telemetry.cars[0].controller_role=CTRL_REMOTE;
    trace_update(24);
    CHECK(g_trace[0].revision!=rev_before && g_trace[0].lap_total==0);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,4,-1,(long)rev_before)==-4);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,4,-1,-1)==-1);
    g_telemetry.cars[0].controller_role=CTRL_LOCAL;
    trace_update(24);

    /* --- session generation change: same reset, old generation pins refused. --- */
    unsigned rev2=g_trace[0].revision;
    unsigned gen_old=capture_generation;
    capture_generation=gen_old+10;
    trace_update(24);
    CHECK(g_trace[0].revision!=rev2 && g_trace[0].session_generation==capture_generation);
    CHECK(g_trace[0].lap_total==0);
    CHECK(build_trace_data_json_id(buf,sizeof buf,0,4,(long)gen_old,-1)==-3);

    /* --- track change inside one generation: defensive identity reset so a
     * stale lap length is never served. --- */
    for(int k=0;k<50;k++) hock_sample(0,0,1,100.0f+k*25,0.5f*k);
    unsigned rev3=g_trace[0].revision;
    g_telemetry.lap_len=6784.1f;
    trace_update(24);
    CHECK(g_trace[0].revision!=rev3 && fabsf(g_trace[0].rec_lap_len-6784.1f)<0.5f);
    /* the reset opens a fresh lap immediately; no completed lap survives */
    CHECK(g_trace[0].lap_total==0 && g_trace[0].sample_count<=1);

    /* --- replay/rewind: backwards sim time flags a break and clears the
     * start witness; a lower lap number discards the open lap. --- */
    lifecycle_prepare(21,5000,24);
    for(int k=0;k<60;k++) hock_sample(1,1,5,1000.0f+k*25,300+0.5f*k);
    int samples=g_trace[1].sample_count;
    hock_sample(1,1,5,2000.0f,250.0f);        /* sim jumps backwards */
    CHECK(g_trace[1].sample_count==samples);
    CHECK((g_trace[1].open_flags&TRL_BREAKS)!=0 && g_trace[1].start_seen==0);
    hock_sample(1,1,4,2100.0f,340.0f);        /* lap number rewinds */
    CHECK(g_trace[1].dropped_laps>=1 && g_trace[1].lap_number==4);

    /* --- pause: repeated captures store nothing; the resume gap is a hole. --- */
    lifecycle_prepare(22,5000,24);
    for(int k=0;k<40;k++) hock_sample(1,1,2,500.0f+k*25,10+0.5f*k);
    int n0=g_trace[1].sample_count;
    for(int r=0;r<10;r++) hock_sample(1,1,2,500.0f+39*25,29.5f);
    CHECK(g_trace[1].sample_count==n0);
    hock_sample(1,1,2,1525.0f,31.5f);         /* 2.0 s gap > TRACE_SIM_GAP_S */
    CHECK(g_trace[1].sample_count==n0+1);
    /* The open lap is not in lap_samples yet: fill from the live count. */
    trace_grid_fill_samples(g_trace[1].laps[g_trace[1].current_index],
                            g_trace[1].sample_count,
                            g_trace[1].lap_start_sim[g_trace[1].current_index],
                            5,1000,&g_grid_b);
    CHECK(g_grid_b.breaks>=1);                /* no line across the pause */

    /* --- pit / garage / out-lap: no samples while in the box, the open lap is
     * flagged PITTED, and the lap that spans the stop completes with the flag. --- */
    lifecycle_prepare(23,5000,24);
    for(int k=0;k<40;k++) hock_sample(1,1,2,4000.0f+k*25,0.5f*k);
    hock_sample(1,1,3,2.0f,20.5f);            /* crossing opens a started lap 3 */
    CHECK(g_trace[1].start_seen);
    for(int k=0;k<40;k++) hock_sample(1,1,3,27.0f+k*25,21+0.5f*k);
    g_telemetry.cars[1].pitting=2;
    trace_update(24);                          /* garage/pit: recording stops */
    CHECK((g_trace[1].open_flags&TRL_PITTED)!=0 && !g_trace[1].have_prev);
    g_telemetry.cars[1].pitting=0;
    for(int k=0;k<156;k++) hock_sample(1,1,3,1100.0f+k*25,45+0.5f*k);  /* ~4975 m */
    hock_sample(1,1,4,2.0f,123.0f);           /* wrap closes the pit-stretched lap */
    n=trace_find_lap(&g_trace[1],3);
    CHECK(n>=0 && (g_trace[1].lap_valid[n]&TRL_PITTED)!=0);

    /* --- DNF/retire: recording stops, completed data stays readable. --- */
    int kept=trace_find_lap(&g_trace[1],3);
    unsigned tot=g_trace[1].total_samples;
    g_telemetry.cars[1].finish_state=2;
    g_telemetry.cars[1].on_track=0;
    g_telemetry.cars[1].retired=1;
    for(int k=0;k<20;k++) { g_telemetry.session_time=124+k*0.5f; trace_update(24); }
    CHECK(g_trace[1].total_samples==tot);
    CHECK(trace_find_lap(&g_trace[1],3)==kept);
    CHECK(build_trace_data_json(buf,sizeof buf,1,3)>0);
    g_telemetry.cars[1].finish_state=0;
    g_telemetry.cars[1].on_track=1;
    g_telemetry.cars[1].retired=0;

    /* --- teardown: session inactive -> empty capture, trace_update(0) safe,
     * completed laps survive until a real generation change. --- */
    mock_stats.session_active=0;
    capture_on_game_thread(NULL); read_telemetry();
    CHECK(g_telemetry.vehicle_count==0);
    unsigned tot2=g_trace[1].total_samples;
    trace_update(0);
    CHECK(g_trace[1].total_samples==tot2 && trace_find_lap(&g_trace[1],3)>=0);
    mock_stats.session_active=1;
    capture_on_game_thread(NULL); read_telemetry();   /* canonical restore */
}

/* ---- BACKEND 3: measured worst-case payload sizes (bounded builders) ---- */
static int run_sizes(void) {
    static char buf[TRACE_JSON_BYTES]; /* 4.3.0: production trace buffer */
    format_failed=0;
    /* /telemetry at 104 cars with maximum-width names. */
    g_telemetry.vehicle_count=MAX_CARS;
    for(int i=0;i<MAX_CARS;i++) {
        g_telemetry.cars[i].position=i+1;
        g_telemetry.cars[i].lap_number=999;
        g_telemetry.cars[i].speed=149.9f;
        g_telemetry.cars[i].rpm=2500.0f; g_telemetry.cars[i].max_rpm=2500.0f;
        g_telemetry.cars[i].car_valid=0x3F;
        for(int c=0;c<NAME_LEN-1;c++) { g_telemetry.cars[i].name[c]='N'; g_telemetry.cars[i].car_name[c]='C'; }
        g_telemetry.cars[i].name[NAME_LEN-1]=0; g_telemetry.cars[i].car_name[NAME_LEN-1]=0;
        strcpy(g_telemetry.cars[i].box_soon_reason,"tyre life <15%");
    }
    int tele=json_telemetry(buf,sizeof buf);
    CHECK(tele>0 && tele<262144 && !format_failed);
    /* /trace/data at the full sample ring, all channels. */
    capture_generation=200; g_telemetry.lap_len=6784.1f;
    trace_session_clear(); trace_select(0,0,1);
    {
        trace_car_t *t=&g_trace[0];
        int idx=0;
        t->used=1; t->revision=1; t->slot=0;
        t->lap_valid[idx]=TRL_COMPLETE; t->lap_num[idx]=1; t->lap_head=1;
        t->lap_time[idx]=100.0f; t->lap_start_sim[idx]=0.0f; t->lap_samples[idx]=TRACE_SAMPLES;
        t->first_lap_seen=1; t->last_lap_seen=1;
        t->channels_seen=TRF_ALL;
        for(int k=0;k<TRACE_SAMPLES;k++) {
            trace_sample_t *s=&t->laps[idx][k];
            memset(s,0,sizeof *s);
            s->sim_t=k*0.05f; s->dist=6784.1f*(float)k/(float)(TRACE_SAMPLES-1);
            s->speed=99.99f; s->throttle=1.0f; s->brake=0.5f; s->steer=0.9999f;
            s->glat=-99.99f; s->glon=-99.99f; s->gvert=99.99f; s->yaw=-3.14159f; s->yaw_rate=-9.99f;
            s->rpm=24999.9f; s->gear=8; s->tx=-999999.9f; s->tz=-999999.9f;
            for(int w=0;w<4;w++) { s->tread[w]=1.0f; s->ttemp[w]=-999.9f; }
            for(int r=0;r<16;r++) s->rx[r]=-1.23456e+05f; /* widest %.4g form */
            s->flags=TRF_ALL;
        }
    }
    int data=build_trace_data_json(buf,sizeof buf,0,1);
    CHECK(data>0 && data<TRACE_JSON_BYTES-256 && strstr(buf,"\"truncated\":true")==NULL);
    int csv=build_trace_data_csv(buf,sizeof buf,0,1);
    CHECK(csv>0 && csv<524288);
    /* /trace/compare worst case: the same full lap on both sides, n=1000. */
    {
        trace_car_t *t=&g_trace[0];
        t->lap_valid[1]=TRL_COMPLETE; t->lap_num[1]=2; t->lap_samples[1]=TRACE_SAMPLES;
        t->lap_time[1]=100.0f; t->lap_start_sim[1]=0.0f;
        memcpy(t->laps[1],t->laps[0],sizeof t->laps[0]);
        t->last_lap_seen=2;
    }
    int cmp=build_trace_compare_json(buf,sizeof buf,0,1,0,0,2,0,5.0f);
    CHECK(cmp>0 && cmp<TRACE_JSON_BYTES-256 && strstr(buf,"\"dist\":[")!=NULL && strstr(buf,"\"a_ch\":{")!=NULL);
    /* /trace/status worst case: two full rings + eviction log + reasons. */
    {
        trace_car_t *t=&g_trace[0];
        for(int i=0;i<TRACE_LAP_SLOTS;i++) {
            t->lap_valid[i]=TRL_COMPLETE|TRL_BREAKS; t->lap_num[i]=10+i;
            t->lap_time[i]=100.0f; t->lap_samples[i]=TRACE_SAMPLES; t->lap_start_sim[i]=i;
        }
        for(int i=0;i<TRACE_EVICT_MAX;i++) { t->evicted[i].lap=i; t->evicted[i].t=100.0f; t->evicted[i].at=i; }
        t->evicted_count=TRACE_EVICT_MAX;
    }
    int status=build_trace_status_json(buf,sizeof buf);
    CHECK(status>0 && status<524288);
    /* /track at the maximum waypoint count. */
    {
        unsigned char *wp=VirtualAlloc(NULL,MAX_WAYPOINTS*WP_STRIDE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        CHECK(wp!=NULL);
        set_i(GAME_ADDR_AI_DB_BASE+OFF_WP_COUNT,MAX_WAYPOINTS);
        set_p(GAME_ADDR_AI_DB_BASE+OFF_WP_ARRAY,wp);
        for(int i=0;i<MAX_WAYPOINTS;i++) {
            set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_X,-999999.9f);
            set_f((uintptr_t)wp+i*WP_STRIDE+WP_OFF_Z,-999999.9f);
        }
        capture_on_game_thread(NULL);      /* publish GAME_ADDR writes into the mirrors */
        EnterCriticalSection(&g_telemetry_cs);
        int track=build_track_json(buf,sizeof buf);
        LeaveCriticalSection(&g_telemetry_cs);
        CHECK(track>0 && track<131072 && track>17);
        printf("SIZES telemetry=%d trace_data=%d csv=%d compare=%d status=%d track=%d\n",
               tele,data,csv,cmp,status,track);
        fflush(stdout);
        VirtualFree(wp,0,MEM_RELEASE);
        set_i(GAME_ADDR_AI_DB_BASE+OFF_WP_COUNT,0);
        set_p(GAME_ADDR_AI_DB_BASE+OFF_WP_ARRAY,NULL);
    }
    return 0;
}

/* ---- BACKEND 3: soak shard. One ordered simulation of the recorder state
 * (single-threaded by design: it exercises the shared game-callback path);
 * the Python gate runs one shard per logical CPU with distinct seeds. ---- */
static unsigned g_soak_rng;
static unsigned soak_rand(void) {
    g_soak_rng=g_soak_rng*1664525u+1013904223u;
    return g_soak_rng>>9;
}
static int run_soak(int seed, int ticks) {
    static char buf[524288];
    g_soak_rng=(unsigned)seed*2654435761u+1u;
    const int cars=22;
    lifecycle_prepare((unsigned)seed,6775.0f,cars);
    float dist[2]={100.0f,220.0f};
    int lap[2]={1,1};
    unsigned laps_done[2]={0,0}, evicted[2]={0,0};
    PROCESS_MEMORY_COUNTERS pmc;
    size_t rss_min=0, rss_max=0, rss_first=0;
    size_t warm_min=0, warm_max=0;
    ULONGLONG t0=GetTickCount64();
    for(int tick=0;tick<ticks;tick++) {
        g_telemetry.session_time=(float)tick*0.05f;
        for(int k=0;k<2;k++) {
            float sp=52.0f+(float)(soak_rand()%1200)/100.0f;   /* 52.0-63.9 m/s */
            dist[k]+=sp*0.05f;
            if(dist[k]>=6775.0f) { dist[k]-=6775.0f; lap[k]++; }
            set_i(VEH_BASE+(size_t)k*VEH_STRIDE+OFF_LAP_NUMBER,lap[k]);
            trace_vehicle(k,k?1:0,dist[k],sp,0.0f,k?0.2f:0.0f,0.9f);
            g_telemetry.cars[k].lap_number=lap[k];
        }
        capture_on_game_thread(NULL);
        trace_update(cars);
        if((tick%200)==0) {
            format_failed=0;
            int n=json_telemetry(buf,sizeof buf);
            CHECK(n>0 && !format_failed);
        }
        if((tick%1000)==0) {
            int n=build_trace_status_json(buf,sizeof buf);
            CHECK(n>0);
        }
        if((tick%2000)==0) {
            memset(&pmc,0,sizeof pmc); pmc.cb=sizeof pmc;
            CHECK(K32GetProcessMemoryInfo(GetCurrentProcess(),&pmc,(DWORD)sizeof pmc));
            if(!rss_first) rss_first=pmc.WorkingSetSize;
            if(pmc.WorkingSetSize>rss_max) rss_max=pmc.WorkingSetSize;
            if(!rss_min || pmc.WorkingSetSize<rss_min) rss_min=pmc.WorkingSetSize;
            if(tick>=20000) {
                if(!warm_min || pmc.WorkingSetSize<warm_min) warm_min=pmc.WorkingSetSize;
                if(pmc.WorkingSetSize>warm_max) warm_max=pmc.WorkingSetSize;
            }
        }
    }
    ULONGLONG wall=GetTickCount64()-t0;
    laps_done[0]=g_trace[0].lap_total; laps_done[1]=g_trace[1].lap_total;
    evicted[0]=(unsigned)g_trace[0].evicted_count; evicted[1]=(unsigned)g_trace[1].evicted_count;
    double hours=(double)ticks*0.05/3600.0;
    size_t delta = rss_max>rss_min ? rss_max-rss_min : 0;
    size_t warm_delta = warm_max>warm_min ? warm_max-warm_min : 0;
    /* Flat memory after warm-up: the mirror copies and static buffers are
     * allocated before tick 20k; only the bounded rings churn afterwards. */
    CHECK(ticks<20000 || (warm_max>0 && warm_delta < 8u*1024u*1024u));
    CHECK(delta < 16u*1024u*1024u);
    printf("SOAK seed=%d ticks=%d hours=%.2f wall_ms=%llu rss_first_kb=%llu rss_min_kb=%llu rss_max_kb=%llu delta_kb=%llu warm_delta_kb=%llu laps_a=%u laps_b=%u evicted_a=%u evicted_b=%u\n",
           seed,ticks,hours,(unsigned long long)wall,
           (unsigned long long)rss_first/1024,(unsigned long long)rss_min/1024,
           (unsigned long long)rss_max/1024,(unsigned long long)delta/1024,
           (unsigned long long)warm_delta/1024,
           laps_done[0],laps_done[1],evicted[0],evicted[1]);
    fflush(stdout);
    return 0;
}

/* ---- BACKEND 3: bounded import fuzzing. Deterministic per seed; the current
 * candidate input is written before every decode so a crash preserves the
 * crasher (Python records the seed and reruns it with import-test). ---- */
static unsigned g_fuzz_rng;
static unsigned fuzz_rand(void) {
    g_fuzz_rng=g_fuzz_rng*1664525u+1013904223u;
    return g_fuzz_rng>>8;
}
static int run_fuzz(int seed, int iterations, const char *current_path) {
    static rt_wbuf base_lap, base_spt;
    static unsigned char mut[1<<20];
    char err[LF_ERR_CAP];
    g_fuzz_rng=(unsigned)seed*2246822519u+1u;
    size_t lap_len=rt_build_lap(&base_lap);
    size_t spt_len=rt_build_spt(&base_spt);
    int ok=0, refused=0;
    for(int it=0;it<iterations;it++) {
        const unsigned char *base=base_lap.data; size_t base_len=lap_len;
        int slot=0;
        if(it&1) { base=base_spt.data; base_len=spt_len; slot=1; }
        if(base_len>sizeof mut) base_len=sizeof mut;
        memcpy(mut,base,base_len);
        size_t len=base_len;
        int ops=1+(int)(fuzz_rand()%8);
        for(int o=0;o<ops;o++) {
            unsigned r=fuzz_rand();
            switch(r%8) {
            case 0: if(len) mut[fuzz_rand()%len]=(unsigned char)(fuzz_rand()&0xFF); break;
            case 1: if(len>2) { size_t p=fuzz_rand()%(len-1); mut[p]=(unsigned char)fuzz_rand(); mut[p+1]=(unsigned char)fuzz_rand(); } break;
            case 2: if(len) { size_t newlen=fuzz_rand()%len; len=newlen; } break;
            case 3: {
                size_t add=fuzz_rand()%513;
                if(len+add>sizeof mut) add=sizeof mut-len;
                for(size_t i=0;i<add;i++) mut[len+i]=(unsigned char)fuzz_rand();
                len+=add;
            } break;
            case 4: if(len) { size_t p=fuzz_rand()%len, run=1+fuzz_rand()%64;
                              if(p+run>len) run=len-p;
                              memset(mut+p,0,run); } break;
            case 5: { size_t chunk=8+fuzz_rand()%249; if(len>chunk && len+chunk<=sizeof mut) {
                          size_t src=fuzz_rand()%(len-chunk); memmove(mut+len,mut+src,chunk); len+=chunk; } } break;
            case 6: { size_t p=(fuzz_rand()%4)*4; if(p+4<=len) { uint32_t v=fuzz_rand(); memcpy(mut+p,&v,4); } } break;
            case 7: if(len) mut[fuzz_rand()%len]^=(unsigned char)(1u<<(fuzz_rand()%8)); break;
            }
        }
        if(current_path) {
            FILE *f=fopen(current_path,"wb");
            if(f) { fwrite(mut,1,len,f); fclose(f); }
        }
        err[0]=0;
        int r=build_import_json(slot,mut,len,err,sizeof err);
        if(r>0) ok++; else refused++;
    }
    printf("FUZZ seed=%d iterations=%d ok=%d refused=%d max_bytes=1048576\n",seed,iterations,ok,refused);
    fflush(stdout);
    return 0;
}

static int emit_corpus(const char *dir) {
    static rt_wbuf b;
    char lap_path[MAX_PATH+64], spt_path[MAX_PATH+64];
    size_t n=rt_build_lap(&b);
    snprintf(lap_path,sizeof lap_path,"%s\\base_fixture.lap",dir);
    FILE *f=fopen(lap_path,"wb"); CHECK(f!=NULL); fwrite(b.data,1,n,f); fclose(f);
    n=rt_build_spt(&b);
    snprintf(spt_path,sizeof spt_path,"%s\\base_fixture.spt",dir);
    f=fopen(spt_path,"wb"); CHECK(f!=NULL); fwrite(b.data,1,n,f); fclose(f);
    printf("CORPUS dir=%s lap=%s spt=%s\n",dir,lap_path,spt_path);
    fflush(stdout);
    return 0;
}

static int run_import_test(const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f) { printf("IMPORT result=missing file=%s\n",path); return 2; }
    static unsigned char body[IMPORT_BODY_MAX+1];
    size_t n=fread(body,1,sizeof body,f);
    fclose(f);
    char err[LF_ERR_CAP]; err[0]=0;
    int r=build_import_json(0,body,n,err,sizeof err);
    if(r>0) printf("IMPORT result=ok bytes=%d file=%s\n",r,path);
    else printf("IMPORT result=refused bytes=%d file=%s err=%s\n",(int)n,path,err);
    fflush(stdout);
    return 0;
}


/* ---- 1.0.1: lap-time latch, session-reset rules, store growth, session archive ---- */
static char g_ar_test_root[MAX_PATH];

static int file_has(const char *path, const char *needle) {
    static char b[1 << 20];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    return strstr(b, needle) != NULL;
}

/* The game raises the lap counter before it writes the new last-lap time. */
static void test_lap_latch(void) {
    car_telemetry_t *c = &g_telemetry.cars[0];
    lap_history_init();
    memset(c, 0, sizeof *c);
    snprintf(c->name, NAME_LEN, "Latch Driver"); c->position = 1;
    c->last_lap = 81.0f;                                   /* stale: qualifying */
    g_telemetry.session_time = 10.0; c->lap_number = 0; lap_history_record(0, c);
    g_telemetry.session_time = 100.0; c->lap_number = 1; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 0);                     /* counter up, time not yet */
    g_telemetry.session_time = 100.2; c->last_lap = 95.5f; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 1 && g_lap_history[0][0] == 95.5f && g_lap_rows[0][0].lap == 1);
    g_telemetry.session_time = 190.0; c->lap_number = 2; c->last_lap = 94.1f; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 2 && fabsf(g_lap_history[0][1] - 94.1f) < 1e-4f);   /* same tick (x87: no float ==) */
    g_telemetry.session_time = 280.0; c->lap_number = 3; lap_history_record(0, c);
    g_telemetry.session_time = 285.0; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 2);                     /* still waiting */
    g_telemetry.session_time = 288.5; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 3 && g_lap_history[0][2] < 0.0f);     /* unknown, never the old 94.1 */
    g_telemetry.session_time = 369.5; c->last_lap = 93.0f; lap_history_record(0, c);
    g_telemetry.session_time = 370.0; c->lap_number = 4; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 4 && g_lap_history[0][3] == 93.0f);   /* time just before the counter */
    /* another driver takes the slot: only this slot restarts */
    snprintf(c->name, NAME_LEN, "Other Driver"); g_telemetry.session_time = 400.0; lap_history_record(0, c);
    CHECK(g_lap_history_count[0] == 0);
    lap_history_init();
}

/* A car count change and a lower P1 lap outside the race keep the history. */
static void test_session_reset_rules(void) {
    lap_history_init();
    g_ar_restart_pending = 0;   /* earlier lifecycle tests restart the clock */
    g_sess_type_prev = -1;
    g_telemetry.session_type = 5; g_telemetry.total_laps = 0; g_telemetry.vehicle_count = 24;
    g_telemetry.race_active = 1; g_telemetry.session_time = 100.0;
    session_reset_check();
    g_lap_history_count[3] = 5;
    g_telemetry.vehicle_count = 25; g_telemetry.session_time = 101.0;
    session_reset_check();
    CHECK(g_lap_history_count[3] == 5);                      /* CO-OP join: kept */
    g_sess_leader_lap_prev = 9; session_restart_check(4);    /* qualifying: new P1 with fewer laps */
    CHECK(g_lap_history_count[3] == 5 && g_ar_restart_pending == 0);
    g_telemetry.session_type = 7; g_sess_type_prev = 7;
    g_sess_leader_lap_prev = 9; session_restart_check(4);    /* race: a real restart */
    CHECK(g_lap_history_count[3] == 0 && g_ar_restart_pending == 1);
    g_ar_restart_pending = 0;
    g_telemetry.session_time = 50.0; session_reset_check(); /* clock back = restart */
    CHECK(g_ar_restart_pending == 1);
    g_ar_restart_pending = 0; g_sess_type_prev = -1;
    lap_history_init(); incidents_init();
}

/* A failed GROWTH keeps the store and every recorded lap (1.0.0 dropped it). */
static void test_trace_growth_keeps(void) {
    trace_car_t *before;
    allcars_reset();
    CHECK(trace_store_ensure(4) == 1);
    before = g_trace_store;
    g_trace_store[2].lap_valid[0] = TRL_COMPLETE; g_trace_store[2].lap_num[0] = 3;
    g_trace_alloc_test_fail = 1;
    CHECK(trace_store_ensure(6) == 1 && g_trace_mode == TRACE_MODE_ALL &&
          g_trace_store == before && g_trace_store_cap == 4 && g_trace_store_failed == 1);
    CHECK(trace_pick(1002, NULL) == &g_trace_store[2] && trace_pick(1005, NULL) == NULL);
    allcars_reset();
}

static void ar_test_rmtree(void) {
    static ar_entry_t list[64];
    int n = ar_list_sessions(list, 64);
    g_ar_current_id[0] = 0;
    for (int i = 0; i < n; i++) ar_delete_session_locked(list[i].id, NULL);
    RemoveDirectoryA(g_ar_root);
}

static void test_session_archive(void) {
    static trace_car_t tc;
    static char buf[TRACE_JSON_BYTES];
    char tmp[MAX_PATH], path[MAX_PATH], first_id[AR_ID_LEN];
    int n;
    GetTempPathA(sizeof tmp, tmp);
    CHECK(snprintf(g_ar_test_root, sizeof g_ar_test_root, "%sf1c-web-telemetry-artest-%lu\\", tmp, (unsigned long)GetCurrentProcessId()) < (int)sizeof g_ar_test_root);
    CreateDirectoryA(g_ar_test_root, NULL);
    ar_init(g_ar_test_root);
    CHECK(g_ar_root[0] && strstr(g_ar_root, "telemetry_data") != NULL);
    memset(&g_ar, 0, sizeof g_ar); memset(&g_ar_done, 0, sizeof g_ar_done); g_ar_current_id[0] = 0;

    lap_history_init();
    capture_generation = 900; g_telemetry.session_type = 5; g_telemetry.vehicle_count = 2;
    snprintf(g_telemetry.track_name, sizeof g_telemetry.track_name, "2002_Silverstone");
    g_telemetry.lap_len = 5088.0f; g_telemetry.session_time = 10.0;
    for (int k = 0; k < 2; k++) {
        memset(&g_telemetry.cars[k], 0, sizeof g_telemetry.cars[k]);
        snprintf(g_telemetry.cars[k].name, NAME_LEN, "Archive Driver %d", k);
        snprintf(g_telemetry.cars[k].car_name, NAME_LEN, "Archive Team");
        g_telemetry.cars[k].position = k + 1;
    }
    ar_tick_locked(0);
    CHECK(!g_ar.active);                                       /* stale capture: nothing starts */
    ar_tick_locked(1);
    CHECK(g_ar.active && g_ar.gen == 900 && g_ar.ty == 5 && !g_ar.materialized);
    race_log_open();
    ar_flush_io(0);
    CHECK(!g_ar.materialized && GetFileAttributesA(g_ar_root) == INVALID_FILE_ATTRIBUTES);   /* no lap, no folder */

    /* one timed lap with the late last-lap value */
    {
        car_telemetry_t *c = &g_telemetry.cars[0];
        c->last_lap = 81.0f; c->lap_number = 0; lap_history_record(0, c);
        g_telemetry.session_time = 100.0; c->lap_number = 1; lap_history_record(0, c);
        g_telemetry.session_time = 100.2; c->last_lap = 95.5f; c->last_s1 = 30.0f; c->last_s2 = 62.0f; lap_history_record(0, c);
    }
    CHECK(g_ar.laps == 1);
    /* one completed trace lap with RESEARCH rx values */
    memset(&tc, 0, sizeof tc);
    tc.slot = 1; tc.revision = 44; tc.is_ai = 1; tc.lap_num[0] = 2; tc.lap_samples[0] = 3;
    tc.lap_valid[0] = TRL_COMPLETE; tc.lap_time[0] = 91.25f; tc.lap_start_sim[0] = 5.0f;
    snprintf(tc.driver_name, NAME_LEN, "Archive Driver 1"); snprintf(tc.car_name, NAME_LEN, "Archive Team");
    for (int k = 0; k < 3; k++) {
        tc.laps[0][k].sim_t = 5.0f + k; tc.laps[0][k].dist = 100.0f * k; tc.laps[0][k].speed = 70.0f + k;
        tc.laps[0][k].flags = TRF_DIST | TRF_SPEED | TRF_RX_SU; tc.laps[0][k].rx[0] = 0.123f;
    }
    tc.channels_seen = TRF_DIST | TRF_SPEED | TRF_RX_SU;
    ar_on_trace_lap(&tc, 0);
    {
        incident_t ie; pit_event_t pe;
        memset(&ie, 0, sizeof ie); snprintf(ie.type, sizeof ie.type, "PIT_IN"); ie.car_id = 1; ie.lap = 2;
        ar_on_incident(&ie, "Archive Driver 1");
        memset(&pe, 0, sizeof pe); pe.car_id = 1; snprintf(pe.name, NAME_LEN, "Archive Driver 1");
        pe.lap_in = 2; pe.lap_out = 3; pe.t_in = 50; pe.t_out = 75; pe.completed = 1;
        ar_on_pit(1, &pe);
    }
    g_ar.last_frame = 0; g_ar.last_doc = 0; ar_tick_locked(1);
    ar_flush_io(0);
    CHECK(g_ar.materialized && !strcmp(g_ar_current_id, g_ar.id));
    snprintf(first_id, sizeof first_id, "%s", g_ar.id);
    CHECK(ar_id_valid(first_id) && strstr(first_id, "_s5_g900") != NULL);
    CHECK(ar_path(path, sizeof path, first_id, "meta.json") && file_has(path, "\"laps\":1") &&
          file_has(path, "\"trace_laps\":1") && file_has(path, "2002_Silverstone") && file_has(path, "\"closed\":false"));
    CHECK(ar_path(path, sizeof path, first_id, "frame.json") && file_has(path, "\"session_generation\":900"));
    CHECK(ar_path(path, sizeof path, first_id, "journal.jsonl") && file_has(path, "\"t\":\"session_start\"") &&
          file_has(path, "\"t\":\"lap\""));
    CHECK(ar_path(path, sizeof path, first_id, "traces.bin") && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);

    n = ar_build_laps_json(first_id, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "95.500") && !strstr(buf, "81.000") && strstr(buf, "\"gen\":900"));
    n = ar_build_driver_json(first_id, 0, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"l\":1") && strstr(buf, "\"s2d\":32.000"));
    n = ar_build_driver_json(first_id, 1, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"li\":2") && strstr(buf, "\"done\":1"));
    n = ar_build_pits_json(first_id, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "Archive Driver 1") && strstr(buf, "\"d\":25.00"));
    n = ar_build_incidents_json(first_id, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"ty\":\"PIT_IN\"") && strstr(buf, "Archive Driver 1"));
    n = ar_build_trace_status_json(first_id, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"mode\":\"archive\"") && strstr(buf, "\"slot\":1") &&
          strstr(buf, "\"lap\":2") && strstr(buf, "\"src\":\"archive\"") && strstr(buf, "\"gen\":900"));
    n = ar_build_trace_data_json(first_id, 1, 2, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"source\":\"archive\"") && strstr(buf, "\"count\":3") &&
          strstr(buf, "\"lap_time\":91.250") && strstr(buf, "\"rx\":null") && !strstr(buf, "0.123"));
    CHECK(ar_build_trace_data_json(first_id, 1, 99, buf, sizeof buf) == -1);
    n = ar_live_fallback_trace(1, 2, buf, sizeof buf);                     /* live ring miss -> disk */
    CHECK(n > 0 && strstr(buf, "\"count\":3"));
    n = ar_build_sessions_json(buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, first_id) && strstr(buf, "\"current\":true") && strstr(buf, "\"enabled\":true"));
    n = ar_build_delete_json(first_id, strlen(first_id), buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "still recording") && ar_session_exists(first_id));
    n = ar_build_delete_json("..\\evil\nx", 9, buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "invalid id") && strstr(buf, "\"deleted\":[]"));
    {
        char hb[4096];
        int hn = build_health_json(hb, sizeof hb);
        CHECK(hn > 0 && strstr(hb, "\"archive\":{\"enabled\":true") && strstr(hb, "\"product\":\"Telemetry 1.2.5\""));
    }

    /* new broker generation: the session closes, a new one starts */
    capture_generation = 901;
    ar_tick_locked(1);
    CHECK(g_ar_done.active && g_ar.active && g_ar.gen == 901);
    ar_flush_io(0);
    CHECK(!g_ar_done.active && g_ar_current_id[0] == 0);
    CHECK(ar_path(path, sizeof path, first_id, "meta.json") && file_has(path, "\"closed\":true"));
    /* restart on the same key also starts a new session */
    g_ar_restart_pending = 1; ar_tick_locked(1);
    CHECK(g_ar_done.active && g_ar.active);
    ar_flush_io(0);                                  /* a session without laps leaves no folder */
    {
        static ar_entry_t l[8];
        CHECK(ar_list_sessions(l, 8) == 1);
    }
    /* track change inside one generation */
    snprintf(g_ar.track, sizeof g_ar.track, "2002_Monza"); ar_tick_locked(1);
    CHECK(g_ar_done.active); ar_session_free(&g_ar_done);

    /* data manager delete of a finished session */
    n = ar_build_delete_json(first_id, strlen(first_id), buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, first_id) && strstr(buf, "\"refused\":[]") && !ar_session_exists(first_id));
    CHECK(ar_build_trace_data_json(first_id, 1, 2, buf, sizeof buf) == -1);

    /* storage cap removes the oldest finished session, never the current one */
    {
        static char big[1536 * 1024];
        char d1[MAX_PATH], f1[MAX_PATH];
        FILE *f;
        CHECK(snprintf(d1, sizeof d1, "%s20200101-000000_s1_g1\\", g_ar_root) < (int)sizeof d1); CreateDirectoryA(d1, NULL);
        CHECK(snprintf(f1, sizeof f1, "%sjournal.jsonl", d1) < (int)sizeof f1);
        f = fopen(f1, "wb"); CHECK(f != NULL); fwrite(big, 1, sizeof big, f); fclose(f);
        g_ar_max_mb = 1; ar_retention(); g_ar_max_mb = 3000;
        CHECK(!ar_session_exists("20200101-000000_s1_g1"));
    }
    /* legacy 1.0.0 journals beside the DLL are listed and deletable */
    {
        char lj[MAX_PATH];
        FILE *f;
        CHECK(snprintf(lj, sizeof lj, "%sweb_telemetry_race_20261009_110351.jsonl", g_ar_test_root) < (int)sizeof lj);
        f = fopen(lj, "wb"); CHECK(f != NULL); fputs("{}\n", f); fclose(f);
        n = ar_build_sessions_json(buf, sizeof buf);
        CHECK(strstr(buf, "\"legacy_journals\":{\"files\":1,\"bytes\":3}") != NULL);
        n = ar_build_delete_json("legacy-journals", 15, buf, sizeof buf);
        CHECK(strstr(buf, "\"legacy-journals\"") && GetFileAttributesA(lj) == INVALID_FILE_ATTRIBUTES);
    }
    ar_session_free(&g_ar); ar_session_free(&g_ar_done); ar_buf_free(&g_ar_pre);
    g_race_log = 0;
    ar_test_rmtree();
    RemoveDirectoryA(g_ar_test_root);
    g_ar_root[0] = g_ar_module_dir[0] = 0;
    lap_history_init(); incidents_init();
}

/* 1.0.1: the serve harness also offers one archived (finished) session so the
 * HTTP contract test validates review-mode documents against the schemas.
 * The live state used by the serve checks is saved and restored around it. */
static void serve_seed_archive(void) {
    static telemetry_t saved;
    static trace_car_t tc;
    char tmp[MAX_PATH];
    unsigned saved_gen = capture_generation;
    lap_row_t row;
    incident_t ie;
    pit_event_t pe;
    memcpy(&saved, &g_telemetry, sizeof saved);
    GetTempPathA(sizeof tmp, tmp);
    CHECK(snprintf(g_ar_test_root, sizeof g_ar_test_root, "%sf1c-web-telemetry-arserve-%lu\\", tmp, (unsigned long)GetCurrentProcessId()) < (int)sizeof g_ar_test_root);
    CreateDirectoryA(g_ar_test_root, NULL);
    ar_init(g_ar_test_root);
    memset(&g_ar, 0, sizeof g_ar); memset(&g_ar_done, 0, sizeof g_ar_done); g_ar_current_id[0] = 0;
    capture_generation = 4242;
    g_telemetry.vehicle_count = 2; g_telemetry.session_type = 7;
    snprintf(g_telemetry.track_name, sizeof g_telemetry.track_name, "2002_Monza");
    ar_tick_locked(1);
    CHECK(g_ar.active);
    memset(&row, 0, sizeof row);
    row.lap = 1; row.t = 83.25f; row.s1 = 28.0f; row.s2 = 55.5f; row.pos = 1; row.fuel = 50.0f;
    ar_on_lap_row(0, &g_telemetry.cars[0], &row);
    memset(&tc, 0, sizeof tc);
    tc.slot = 0; tc.revision = 7; tc.lap_num[0] = 1; tc.lap_samples[0] = 40; tc.lap_valid[0] = TRL_COMPLETE;
    tc.lap_time[0] = 83.25f; tc.lap_start_sim[0] = 10.0f;
    snprintf(tc.driver_name, NAME_LEN, "%s", g_telemetry.cars[0].name);
    snprintf(tc.car_name, NAME_LEN, "%s", g_telemetry.cars[0].car_name);
    for (int k = 0; k < 40; k++) {
        trace_sample_t *q = &tc.laps[0][k];
        q->sim_t = 10.0f + k * 2.0f; q->dist = 125.0f * k; q->speed = 60.0f + k; q->throttle = 0.8f; q->brake = 0.0f;
        q->rpm = 15000.0f; q->gear = 6; q->tx = (float)k; q->tz = (float)-k;
        q->flags = TRF_DIST | TRF_SPEED | TRF_THROTTLE | TRF_BRAKE | TRF_RPM | TRF_GEAR | TRF_TRACK;
    }
    tc.channels_seen = TRF_DIST | TRF_SPEED | TRF_THROTTLE | TRF_BRAKE | TRF_RPM | TRF_GEAR | TRF_TRACK;
    ar_on_trace_lap(&tc, 0);
    memset(&ie, 0, sizeof ie); snprintf(ie.type, sizeof ie.type, "PIT_IN"); ie.car_id = 0; ie.lap = 1;
    ar_on_incident(&ie, g_telemetry.cars[0].name);
    memset(&pe, 0, sizeof pe); pe.car_id = 0; snprintf(pe.name, NAME_LEN, "%s", g_telemetry.cars[0].name);
    pe.lap_in = 1; pe.lap_out = 2; pe.t_in = 90; pe.t_out = 112; pe.tire_out = -1; pe.completed = 1;
    ar_on_pit(0, &pe);
    g_ar.last_frame = 0; g_ar.last_doc = 0; ar_tick_locked(1);
    ar_flush_io(0);
    CHECK(g_ar.materialized);
    capture_generation = 4243; ar_tick_locked(1);       /* close it: a finished session */
    ar_flush_io(0);
    ar_session_free(&g_ar);
    CHECK(g_ar_current_id[0] == 0);
    memcpy(&g_telemetry, &saved, sizeof saved);
    capture_generation = saved_gen;
}

static void serve_drop_archive(void) {
    ar_test_rmtree();
    RemoveDirectoryA(g_ar_test_root);
    g_ar_root[0] = g_ar_module_dir[0] = 0;
}

/* ---- 1.1.0 storage v2 core (storage_v2.inc, batch 1): U-1..U-9, F-1, F-2, B-1, B-2 ---- */
static uint64_t t2_rs = 0x9E3779B97F4A7C15ull;
static uint64_t t2_rng(void) { t2_rs ^= t2_rs << 13; t2_rs ^= t2_rs >> 7; t2_rs ^= t2_rs << 17; return t2_rs; }
static uint32_t t2_rnd(uint32_t n) { return (uint32_t)(t2_rng() % n); }
static float t2_frand(float lo, float hi) { return lo + (hi - lo) * (float)((double)(t2_rng() >> 40) / 16777216.0); }

#define T2_POOL_WORDS (1u << 17)
#define T2_META_LEN 1200u
#define T2_F2_RAW 1000000u
#define T2_F2_PACKED 1000000u
static uint32_t t2_pool[T2_POOL_WORDS];
static uint32_t t2_dpool[T2_POOL_WORDS];
static uint32_t t2_spool[T2_POOL_WORDS];
static sv2_data_t t2_gen, t2_dec, t2_scan_data;
static sv2_scan_t t2_rep;
static uint8_t t2_work[1u << 20];
static uint8_t t2_meta[T2_META_LEN];

static void t2_u1_quantizer(void) {
    sv2_channel_t cat[SV2_CATALOG_COUNT];
    unsigned q16 = 0;
    sv2_catalog_to_table(cat);
    for (unsigned i = 0; i < SV2_CATALOG_COUNT; i++) {
        float mn = cat[i].min, mx = cat[i].max, v, d;
        double bound;
        int clamped = 0, bad = 0;
        uint16_t q;
        if (cat[i].type != SV2_TYPE_Q16) continue;
        q16++;
        /* spec 4.5 bound plus the binary32 rounding of the decoded value (half an ulp of the largest magnitude);
         * the absolute 1e-7 term alone is not reachable for channels above about 1 (rpm 25000, tx 10000). */
        bound = ((double)mx - (double)mn) / 131070.0 + 1e-7 + 1.1920929e-7 * fmax(fabs((double)mn), fabs((double)mx));
        for (unsigned k = 0; k < 1000000u; k++) {
            v = t2_frand(mn, mx);
            if (v > mx) v = mx; /* float rounding of lo + span * u can land one ulp past hi */
            if (v < mn) v = mn;
            q = sv2_q16_encode(v, mn, mx, &clamped);
            d = sv2_q16_decode(q, mn, mx);
            if (clamped || !(fabs((double)v - (double)d) <= bound)) bad++;
        }
        CHECK(bad == 0);
        q = sv2_q16_encode(mn, mn, mx, &clamped);
        CHECK(q == 0 && !clamped);
        q = sv2_q16_encode(mx, mn, mx, &clamped);
        CHECK(q == 65535 && !clamped);
        CHECK(fabs((double)sv2_q16_decode(65535, mn, mx) - (double)mx) <= bound);
        q = sv2_q16_encode(mn - 1.0f, mn, mx, &clamped);
        CHECK(q == 0 && clamped);
        q = sv2_q16_encode(mx + 1.0f, mn, mx, &clamped);
        CHECK(q == 65535 && clamped);
        q = sv2_q16_encode((float)NAN, mn, mx, &clamped);
        CHECK(q == 0 && clamped);
        q = sv2_q16_encode((float)INFINITY, mn, mx, &clamped);
        CHECK(q == 65535 && clamped);
        q = sv2_q16_encode(-(float)INFINITY, mn, mx, &clamped);
        CHECK(q == 0 && clamped);
    }
    CHECK(q16 == 44); /* 21 base (ids 1-15, 17, 18, 20-23) + B1 format 2.1 Q16 channels (64-124) */
}

static void t2_u2_zigzag_varint(void) {
    static const int32_t zs[] = {0, 1, -1, 2, -2, 63, -64, 64, -65, INT32_MAX, INT32_MIN, INT32_MIN + 1};
    static const uint32_t vs[] = {0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 0x0FFFFFFFu, 0x10000000u, 0xFFFFFFFFu};
    static const unsigned lens[] = {1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    static const uint8_t bad_long[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    static const uint8_t bad_top[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x1F};
    static const uint8_t ok_max[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    static const uint8_t cut[] = {0x80};
    uint8_t buf[16];
    uint32_t got = 0;
    sv2_r_t r;
    CHECK(sv2_zz_enc(INT32_MIN) == 0xFFFFFFFFu && sv2_zz_enc(INT32_MAX) == 0xFFFFFFFEu && sv2_zz_enc(-1) == 1u);
    for (size_t i = 0; i < sizeof zs / sizeof zs[0]; i++) CHECK(sv2_zz_dec(sv2_zz_enc(zs[i])) == zs[i]);
    for (size_t i = 0; i < sizeof vs / sizeof vs[0]; i++) {
        sv2_w_t w = {buf, sizeof buf, 0, 0};
        sv2_w_varint(&w, vs[i]);
        CHECK(!w.err && w.n == lens[i] && w.n <= 5);
        r.p = buf; r.n = w.n; r.pos = 0; r.err = 0;
        CHECK(sv2_r_varint(&r, &got) && got == vs[i] && r.pos == w.n && !r.err);
    }
    r.p = bad_long; r.n = sizeof bad_long; r.pos = 0; r.err = 0;
    CHECK(!sv2_r_varint(&r, &got) && r.err);
    r.p = bad_top; r.n = sizeof bad_top; r.pos = 0; r.err = 0;
    CHECK(!sv2_r_varint(&r, &got) && r.err);
    r.p = ok_max; r.n = sizeof ok_max; r.pos = 0; r.err = 0;
    CHECK(sv2_r_varint(&r, &got) && got == 0xFFFFFFFFu);
    r.p = cut; r.n = sizeof cut; r.pos = 0; r.err = 0;
    CHECK(!sv2_r_varint(&r, &got) && r.err);
}

static void t2_u3_crc(void) {
    CHECK(sv2_crc32(0, "123456789", 9) == 0xCBF43926u);
    CHECK(sv2_crc32(sv2_crc32(0, "1234", 4), "56789", 5) == 0xCBF43926u);
    CHECK(sv2_crc32(0, "", 0) == 0u);
}

/* Random DATA contents: car sets, masks, first_tick and count, Q16 random walks, RAW32 with wrap. */
static size_t t2_random_data(sv2_data_t *d, uint32_t *pool, size_t cap, uint32_t n_ticks, uint32_t n_cars, const sv2_channel_t *cat, uint32_t ncat) {
    size_t used = 0;
    memset(d, 0, sizeof *d);
    d->n_ticks = (uint8_t)n_ticks;
    d->n_cars = (uint8_t)n_cars;
    for (uint32_t i = 1; i < n_ticks; i++) d->dt_ms[i] = (int32_t)t2_rnd(120) - 20;
    for (uint32_t i = 1; i < n_ticks; i++) d->sim_ms[i] = (int32_t)t2_rnd(400) - 50;   /* 2.1: sim deltas, any sign */
    for (uint32_t c = 0; c < n_cars; c++) {
        sv2_car_t *car = &d->cars[c];
        uint32_t rank = 0, npresent;
        size_t need;
        car->slot = (uint8_t)t2_rnd(104);
        car->scope = (uint8_t)t2_rnd(2);
        car->first_tick = (uint8_t)t2_rnd(n_ticks);
        car->count = (uint8_t)(1 + t2_rnd(n_ticks - car->first_tick));
        car->mask_lo = car->mask_hi = 0;
        for (uint32_t k = 0; k < ncat; k++) {
            uint32_t id = cat[k].id;
            if (id == 0 || !sv2_scope_ok(car->scope, cat[k].scope) || t2_rnd(3) == 0) continue;
            if (id < 64) car->mask_lo |= (uint64_t)1 << id; else car->mask_hi |= (uint64_t)1 << (id - 64);
        }
        npresent = sv2_popcount64(car->mask_lo) + sv2_popcount64(car->mask_hi);
        need = (size_t)npresent * car->count;
        if (need > cap - used) return 0;
        car->vals = pool + used;
        used += need;
        for (uint32_t k = 0; k < ncat; k++) {
            uint32_t id = cat[k].id;
            uint32_t *v;
            if (!sv2_mask_bit(car->mask_lo, car->mask_hi, id)) continue;
            v = car->vals + (size_t)rank * car->count;
            if (cat[k].type == SV2_TYPE_Q16) {
                uint32_t q = t2_rnd(65536);
                for (uint32_t i = 0; i < car->count; i++) {
                    if (i) {
                        int64_t nq = (int64_t)q + ((int64_t)t2_rnd(7) - 3);
                        q = (uint32_t)(nq < 0 ? 0 : nq > 65535 ? 65535 : nq);
                    }
                    v[i] = q;
                }
            } else {
                for (uint32_t i = 0; i < car->count; i++)
                    v[i] = (i == 0 || t2_rnd(4) == 0) ? (uint32_t)t2_rng() : v[i - 1] + t2_rnd(5);
            }
            rank++;
        }
    }
    return used;
}

static void t2_u4_data_roundtrip(void) {
    sv2_channel_t cat[SV2_TABLE_MAX_CHANNELS];
    static uint8_t enc[1u << 19];
    size_t elen;
    sv2_catalog_to_table(cat);
    for (int iter = 0; iter < 400; iter++) {
        uint32_t n = 1 + t2_rnd(20), nc = 1 + t2_rnd(8);
        int bad = 0;
        if (iter == 0) { n = 20; nc = SV2_MAX_CARS; }
        CHECK(t2_random_data(&t2_gen, t2_pool, T2_POOL_WORDS, n, nc, cat, SV2_CATALOG_COUNT) > 0);
        elen = sv2_data_encode(&t2_gen, cat, SV2_CATALOG_COUNT, enc, sizeof enc);
        CHECK(elen > 0);
        memset(&t2_dec, 0, sizeof t2_dec);
        t2_dec.pool = t2_dpool;
        t2_dec.pool_cap = T2_POOL_WORDS;
        CHECK(sv2_data_decode(enc, elen, cat, SV2_CATALOG_COUNT, &t2_dec) == SV2_OK);
        CHECK(t2_dec.n_ticks == t2_gen.n_ticks && t2_dec.n_cars == t2_gen.n_cars);
        for (uint32_t i = 0; i < n; i++) if (t2_dec.dt_ms[i] != t2_gen.dt_ms[i]) bad++;
        for (uint32_t i = 0; i < n; i++) if (t2_dec.sim_ms[i] != t2_gen.sim_ms[i]) bad++;
        for (uint32_t c = 0; c < nc; c++) {
            const sv2_car_t *a = &t2_gen.cars[c], *b = &t2_dec.cars[c];
            uint32_t np = sv2_popcount64(a->mask_lo) + sv2_popcount64(a->mask_hi);
            if (a->slot != b->slot || a->scope != b->scope || a->first_tick != b->first_tick || a->count != b->count) bad++;
            if (a->mask_lo != b->mask_lo || a->mask_hi != b->mask_hi) bad++;
            for (size_t k = 0; k < (size_t)np * a->count; k++) if (a->vals[k] != b->vals[k]) bad++;
        }
        CHECK(bad == 0);
        CHECK(sv2_data_decode(enc, elen - 1, cat, SV2_CATALOG_COUNT, &t2_dec) != SV2_OK);
    }
    {
        sv2_data_t d;
        uint32_t vals[1] = {123};
        memset(&d, 0, sizeof d);
        d.n_ticks = 1; d.n_cars = 1;
        d.cars[0].slot = 1; d.cars[0].scope = 0; d.cars[0].count = 1;
        d.cars[0].mask_lo = (uint64_t)1 << 20; d.cars[0].vals = vals; /* ttemp_fl is player only */
        CHECK(sv2_data_encode(&d, cat, SV2_CATALOG_COUNT, enc, sizeof enc) == 0);
        d.cars[0].scope = 1;
        CHECK(sv2_data_encode(&d, cat, SV2_CATALOG_COUNT, enc, sizeof enc) > 0);
    }
}

static void t2_u5_header_table(void) {
    sv2_header_t h, g;
    uint8_t hb[SV2_HEADER_BYTES], bad[SV2_HEADER_BYTES];
    sv2_channel_t cat[SV2_CATALOG_COUNT], back[SV2_TABLE_MAX_CHANNELS];
    uint8_t tb[8192];
    size_t tl;
    memset(&h, 0, sizeof h);
    h.flags = SV2_FLAG_COMPRESSED_BLOCKS | SV2_FLAG_CLOSED;
    h.compression_id = SV2_COMP_XPRESS;
    h.block_ticks_max = 20; h.block_ms_max = 1000;
    h.channel_table_offset = 192; h.channel_table_bytes = 0;
    h.start_unix_utc = 1791000000.5; h.tz_offset_min = -300; h.session_type = -1;
    h.broker_generation = 9; h.channels_declared = SV2_CATALOG_COUNT;
    strcpy(h.track, "Silverstone");
    strcpy(h.session_name, "Silverstone_2002_2026-10-13_14-05");
    sv2_header_encode(&h, hb);
    CHECK(sv2_header_decode(hb, sizeof hb, &g) == SV2_OK);
    CHECK(g.flags == h.flags && g.compression_id == 3 && g.start_unix_utc == h.start_unix_utc);
    CHECK(g.tz_offset_min == -300 && g.session_type == -1 && g.broker_generation == 9);
    CHECK(strcmp(g.track, "Silverstone") == 0 && strcmp(g.session_name, h.session_name) == 0);
    memcpy(bad, hb, sizeof bad); bad[0] ^= 0x20;
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_HEADER);
    memcpy(bad, hb, sizeof bad); bad[8] = 3;
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_HEADER);
    memcpy(bad, hb, sizeof bad); bad[12] = 200;
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_HEADER);
    memcpy(bad, hb, sizeof bad); bad[100] ^= 1;
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_HEADER);
    CHECK(sv2_header_decode(hb, 191, &g) == SV2_R_HEADER);
    memcpy(bad, hb, sizeof bad); bad[10] = 0; sv2_put_le(bad + 188, sv2_crc32(0, bad, 188), 4);
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_VERSION);   /* pre-2.1 file (owner Q3) */
    memcpy(bad, hb, sizeof bad); bad[10] = 2; sv2_put_le(bad + 188, sv2_crc32(0, bad, 188), 4);
    CHECK(sv2_header_decode(bad, sizeof bad, &g) == SV2_R_VERSION);

    sv2_catalog_to_table(cat);
    tl = sv2_table_encode(cat, SV2_CATALOG_COUNT, tb, sizeof tb);
    CHECK(tl > 8);
    CHECK(sv2_table_decode(tb, tl, SV2_CATALOG_COUNT, back) == SV2_OK);
    CHECK(back[1].id == 1 && strcmp(back[1].name, "dist") == 0 && back[1].max == 8000.0f);
    CHECK(sv2_table_decode(tb, tl, SV2_CATALOG_COUNT - 1, back) == SV2_R_TABLE);
    tb[10] ^= 0x01;
    CHECK(sv2_table_decode(tb, tl, SV2_CATALOG_COUNT, back) == SV2_R_TABLE);
    tb[10] ^= 0x01;
    tb[tl - 1] ^= 0x80;
    CHECK(sv2_table_decode(tb, tl, SV2_CATALOG_COUNT, back) == SV2_R_TABLE);
    tb[tl - 1] ^= 0x80;
    cat[2].id = cat[1].id; /* duplicate id: refused even though the CRC is valid */
    tl = sv2_table_encode(cat, SV2_CATALOG_COUNT, tb, sizeof tb);
    CHECK(tl > 8 && sv2_table_decode(tb, tl, SV2_CATALOG_COUNT, back) == SV2_R_TABLE);
}

/* Recompute the 40-byte block header CRC after a deliberate field change. */
static void t2_fix_block_crc(uint8_t *blk) { sv2_put_le(blk + 36, sv2_crc32(0, blk, 36), 4); }

/* Build a complete file: header, channel table, META, 3 DATA, GAP, END. */
static size_t t2_build_file(uint8_t *buf, size_t cap, uint32_t comp_id, uint32_t hflags, int pack_meta, int pack_data) {
    static sv2_channel_t cat[SV2_CATALOG_COUNT];
    static uint8_t table[8192], pay[1u << 19], raw[1u << 19];
    sv2_header_t h;
    sv2_block_t b;
    size_t tl, len, stored_len = 0, dl;
    uint8_t sflags = 0, e[12], g[20];
    uint32_t seq = 0, total = 0;
    sv2_catalog_to_table(cat);
    tl = sv2_table_encode(cat, SV2_CATALOG_COUNT, table, sizeof table);
    if (!tl || cap < 192 + tl) return 0;
    memset(&h, 0, sizeof h);
    h.flags = hflags | SV2_FLAG_HAS_SIM; h.compression_id = comp_id; h.block_ticks_max = 20; h.block_ms_max = 1000;
    h.channel_table_offset = 192; h.channel_table_bytes = (uint32_t)tl;
    h.start_unix_utc = 1791000000.5; h.tz_offset_min = -300; h.session_type = 1;
    h.broker_generation = 7; h.channels_declared = SV2_CATALOG_COUNT;
    strcpy(h.track, "Silverstone");
    strcpy(h.session_name, "Silverstone_2002_2026-10-13_14-05");
    sv2_header_encode(&h, buf);
    memcpy(buf + 192, table, tl);
    len = 192 + tl;
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_META; b.seq = seq++; b.raw_len = T2_META_LEN;
    if (pack_meta) { if (!sv2_pack_payload(comp_id, t2_meta, T2_META_LEN, pay, sizeof pay, &stored_len, &sflags)) return 0; }
    else { memcpy(pay, t2_meta, T2_META_LEN); stored_len = T2_META_LEN; }
    b.flags = sflags; b.stored_len = (uint32_t)stored_len;
    len += sv2_block_put(buf + len, cap - len, b, pay);
    for (uint32_t k = 0; k < 3; k++) {
        uint32_t nt = 5 + t2_rnd(16), nc = 1 + t2_rnd(4);
        if (!t2_random_data(&t2_gen, t2_pool, T2_POOL_WORDS, nt, nc, cat, SV2_CATALOG_COUNT)) return 0;
        dl = sv2_data_encode(&t2_gen, cat, SV2_CATALOG_COUNT, raw, sizeof raw);
        if (!dl) return 0;
        total += nt;
        memset(&b, 0, sizeof b);
        b.type = SV2_BLOCK_DATA; b.seq = seq++; b.n_ticks = nt; b.t_first = (double)k * 20.0; b.raw_len = (uint32_t)dl;
        if (pack_data) { if (!sv2_pack_payload(comp_id, raw, dl, pay, sizeof pay, &stored_len, &sflags)) return 0; }
        else { memcpy(pay, raw, dl); stored_len = dl; sflags = 0; }
        b.flags = sflags; b.stored_len = (uint32_t)stored_len;
        len += sv2_block_put(buf + len, cap - len, b, pay);
    }
    sv2_put_le(g, 3, 4); sv2_put_le(g + 4, sv2_f64_bits(1.0), 8); sv2_put_le(g + 12, sv2_f64_bits(2.0), 8);
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_GAP; b.seq = seq++; b.raw_len = 20; b.stored_len = 20;
    len += sv2_block_put(buf + len, cap - len, b, g);
    sv2_put_le(e, 3, 4); sv2_put_le(e + 4, total, 4); sv2_put_le(e + 8, 3, 4);
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_END; b.seq = seq++; b.raw_len = 12; b.stored_len = 12;
    len += sv2_block_put(buf + len, cap - len, b, e);
    return len;
}

static void t2_u6_block_framing(void) {
    static uint8_t blk[256], pay[64], file[4096];
    sv2_block_t b, g;
    sv2_channel_t cat[SV2_CATALOG_COUNT];
    sv2_header_t h;
    uint8_t tb[4096];   /* batch 3: 64 catalog channels no longer fit 1024 bytes */
    size_t n, tl, len;
    memset(pay, 7, sizeof pay);
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_DATA; b.seq = 4; b.n_ticks = 3; b.t_first = 12.5; b.raw_len = 64; b.stored_len = 64;
    n = sv2_block_put(blk, sizeof blk, b, pay);
    CHECK(n == 40 + 64);
    CHECK(sv2_block_hdr_decode(blk, 40, &g) == SV2_OK && g.seq == 4 && g.n_ticks == 3 && g.t_first == 12.5);
    CHECK(g.payload_crc == sv2_crc32(0, pay, 64));
    blk[0] ^= 1;
    CHECK(sv2_block_hdr_decode(blk, 40, &g) == SV2_R_BLOCK_HEADER); blk[0] ^= 1;
    blk[36] ^= 1;
    CHECK(sv2_block_hdr_decode(blk, 40, &g) == SV2_R_BLOCK_HEADER); blk[36] ^= 1;
    blk[4] = 9; t2_fix_block_crc(blk);
    CHECK(sv2_block_hdr_decode(blk, 40, &g) == SV2_R_BLOCK_HEADER); blk[4] = 1; t2_fix_block_crc(blk);
    blk[28] = 0xFF; blk[29] = 0xFF; blk[30] = 0x3F; blk[31] = 0; t2_fix_block_crc(blk); /* stored_len over 1 MiB */
    CHECK(sv2_block_hdr_decode(blk, 40, &g) == SV2_R_BLOCK_HEADER);
    /* framing inside a file: seq gap is a stop with BLOCK_HEADER at the expected seq */
    sv2_catalog_to_table(cat);
    tl = sv2_table_encode(cat, SV2_CATALOG_COUNT, tb, sizeof tb);
    memset(&h, 0, sizeof h);
    /* gate2 (2026-10-13): stale fixture. 2.1 requires SV2_FLAG_HAS_SIM in the header or sv2_scan
     * stops at SV2_R_HEADER before block framing is reached. */
    h.flags = SV2_FLAG_HAS_SIM;
    h.block_ticks_max = 20; h.block_ms_max = 1000;
    h.channel_table_offset = 192; h.channel_table_bytes = (uint32_t)tl; h.channels_declared = SV2_CATALOG_COUNT;
    sv2_header_encode(&h, file);
    memcpy(file + 192, tb, tl);
    len = 192 + tl;
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_META; b.seq = 0; b.raw_len = 64; b.stored_len = 64;
    len += sv2_block_put(file + len, sizeof file - len, b, pay);
    b.seq = 2;
    len += sv2_block_put(file + len, sizeof file - len, b, pay);
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(file, len, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_R_BLOCK_HEADER);
    CHECK(t2_rep.reason_seq == 1 && t2_rep.valid_blocks == 1 && t2_rep.closed == 0);
    /* payload CRC mismatch: flip one payload byte of the first block */
    len = 192 + tl;
    b.seq = 0;
    len = 192 + tl;
    len += sv2_block_put(file + len, sizeof file - len, b, pay);
    file[len - 1] ^= 0x40;
    CHECK(sv2_scan(file, len, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_R_BLOCK_CRC);
    CHECK(t2_rep.valid_blocks == 0 && t2_rep.valid_bytes == 192 + tl);
}

static void t2_u7_compression(void) {
    static uint8_t out[1u << 19], back[1u << 19], rnd[4096];
    size_t stored = 0, got = 0, n = T2_META_LEN, i;
    uint8_t flags = 0;
    static uint8_t file[1u << 17];
    size_t flen;
    static const char pat[] = "{\"driver\":\"Driver 0\",\"car\":\"Team 0\"},";
    for (i = 0; i < T2_META_LEN; i++) t2_meta[i] = (uint8_t)pat[i % (sizeof pat - 1)];
    CHECK(sv2_alg_available(SV2_COMP_XPRESS) && sv2_alg_available(SV2_COMP_LZMS));
    CHECK(sv2_pack_payload(SV2_COMP_XPRESS, t2_meta, n, out, sizeof out, &stored, &flags) == 1);
    CHECK(flags == SV2_BLOCK_FLAG_COMPRESSED && stored < n);
    CHECK(sv2_decompress_block(SV2_COMP_XPRESS, out, stored, back, sizeof back, &got) == 1 && got == n && memcmp(back, t2_meta, n) == 0);
    CHECK(sv2_pack_payload(SV2_COMP_LZMS, t2_meta, n, out, sizeof out, &stored, &flags) == 1);
    CHECK(flags == SV2_BLOCK_FLAG_COMPRESSED && stored < n);
    CHECK(sv2_decompress_block(SV2_COMP_LZMS, out, stored, back, sizeof back, &got) == 1 && got == n && memcmp(back, t2_meta, n) == 0);
    /* incompressible input is stored raw with the flag clear */
    for (i = 0; i < sizeof rnd; i++) rnd[i] = (uint8_t)t2_rng();
    CHECK(sv2_pack_payload(SV2_COMP_XPRESS, rnd, sizeof rnd, out, sizeof out, &stored, &flags) == 1);
    CHECK(flags == 0 && stored == sizeof rnd && memcmp(out, rnd, sizeof rnd) == 0);
    /* mixed file: compressed META, raw or compressed DATA, read back with a closed END */
    flen = t2_build_file(file, sizeof file, SV2_COMP_XPRESS, SV2_FLAG_COMPRESSED_BLOCKS, 1, 1);
    CHECK(flen > 0);
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(file, flen, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK);
    CHECK(t2_rep.closed == 1 && t2_rep.valid_blocks == 6 && t2_rep.data_blocks == 3 && t2_rep.ticks == t2_rep.end_total_ticks);
    CHECK(t2_rep.valid_bytes == flen && t2_rep.skipped_blocks == 0 && t2_rep.dropped_ticks == 3);
    CHECK(t2_rep.last_good_t > 39.0 && t2_rep.last_good_t < 41.0 && t2_rep.header.compression_id == SV2_COMP_XPRESS);
}

static void t2_u8_aiw(void) {
    static const struct { const char *p, *track, *prefix; int year; } rows[] = {
        {"SEASONDATA\\CIRCUITS\\Italy\\2001_Monza\\2001_Monza.AIW", "Monza", "Monza_2001", 2001},
        /* 1.1.1 (T1): installed letter forms, year = the 4 digits */
        {"SEASONDATA\\CIRCUITS\\Adelaide\\1995CK_Adelaide.aiw", "Adelaide", "Adelaide_1995", 1995},
        {"SEASONDATA\\CIRCUITS\\Ring\\2003RH_A-1_Ring.aiw", "A1Ring", "A1Ring_2003", 2003},
        {"SEASONDATA\\CIRCUITS\\Buenos\\1996CTDP_BUENOS.AIW", "BUENOS", "BUENOS_1996", 1996},
        {"SEASONDATA\\CIRCUITS\\Buenos\\1996TOOLONGX_BUENOS.AIW", "Unknown", "Unknown_0000", 0},
        {"SEASONDATA\\CIRCUITS\\UK\\2002_Silverstone\\2002_Silverstone.AIW", "Silverstone", "Silverstone_2002", 2002},
        {"webtel_aiw\\test.AIW", "Unknown", "Unknown_0000", 0},
        {"SEASONDATA\\CIRCUITS\\UK\\Silverstone\\Silverstone.AIW", "Unknown", "Unknown_0000", 0},
        {"SEASONDATA\\CIRCUITS\\UK\\2002_Silverstone.AIW", "Silverstone", "Silverstone_2002", 2002},
        {"", "Unknown", "Unknown_0000", 0},
        {"SEASONDATA\\CIRCUITS\\UK\\2002_Silver stone\\2002_Silver stone.AIW", "Silver-stone", "Silver-stone_2002", 2002},
        {"SEASONDATA\\CIRCUITS\\X\\9999_X\\9999_X.AIW", "X", "X_0000", 0},
    };
    static const char alpha[] = "AIWaiw.\\/_ 0123456789-:*?\"<>|";
    char track[64], prefix[96], gen[160];
    int year = -1, bad = 0;
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        sv2_aiw_parse(rows[i].p, strlen(rows[i].p), track, sizeof track, &year);
        CHECK(strcmp(track, rows[i].track) == 0 && year == rows[i].year);
        snprintf(prefix, sizeof prefix, "%s_%04d", track, year);
        CHECK(strcmp(prefix, rows[i].prefix) == 0);
    }
    /* 200 generated strings: never a crash, the track is never empty, the year is 0 or 1950..2099 */
    for (int k = 0; k < 200; k++) {
        size_t n = t2_rnd(41), m;
        for (m = 0; m < n; m++) gen[m] = alpha[t2_rnd(sizeof alpha - 1)];
        gen[n] = 0;
        sv2_aiw_parse(gen, n, track, sizeof track, &year);
        if (track[0] == 0 || !(year == 0 || (year >= 1950 && year <= 2099))) bad++;
        if (k % 2 == 0) {
            snprintf(gen, sizeof gen, "SEASONDATA\\CIRCUITS\\C%u\\%04u_Name %u.AIW", (unsigned)t2_rnd(100), 1900u + (unsigned)t2_rnd(200), (unsigned)t2_rnd(10));
            sv2_aiw_parse(gen, strlen(gen), track, sizeof track, &year);
            if (track[0] == 0 || !(year == 0 || (year >= 1950 && year <= 2099))) bad++;
        }
    }
    CHECK(bad == 0);
}

static int t2_taken_count;
static const char *t2_taken[16];
static int t2_exists(void *ctx, const char *name) {
    (void)ctx;
    for (int i = 0; i < t2_taken_count; i++) if (strcmp(t2_taken[i], name) == 0) return 1;
    return 0;
}

static void t2_u9_naming(void) {
    sv2_local_t t = {2026, 10, 13, 14, 5, 0};
    sv2_local_t t2s = {2026, 1, 2, 3, 4, 5};
    sv2_weekend_t w;
    char stem[120], id[64], key[64];
    char base[120];
    /* 1.1.1 decision A names */
    CHECK(sv2_session_stem(stem, sizeof stem, "BRAZIL", 1995, "Championship", "Race", &t) &&
          strcmp(stem, "BRAZIL 1995 - Championship - Race - 2026-10-13 14-05") == 0);
    CHECK(sv2_session_stem(stem, sizeof stem, "BRAZIL", 1995, "Test Day", NULL, &t2s) &&
          strcmp(stem, "BRAZIL 1995 - Test Day - 2026-01-02 03-04") == 0);
    CHECK(sv2_session_stem(stem, sizeof stem, "BRAZIL", 1995, "", "Practice 1", &t2s) &&
          strcmp(stem, "BRAZIL 1995 - Practice 1 - 2026-01-02 03-04") == 0);
    CHECK(sv2_weekend_stem(stem, sizeof stem, "BRAZIL", 1995, "Championship", &t) &&
          strcmp(stem, "BRAZIL 1995 - Championship - 2026-10-13 14-05") == 0);
    {
        char country[64], mode[24], sess[24];
        CHECK(sv2_country_from_aiw("SEASONDATA\\CIRCUITS\\Adelaide\\1995CK_Adelaide.aiw", country, sizeof country) &&
              strcmp(country, "AUSTRALIA") == 0);
        CHECK(sv2_country_from_aiw("GAME\\Circuits\\Classics\\Monza\\1967_Monza.AIW", country, sizeof country) &&
              strcmp(country, "MONZA") == 0);
        CHECK(sv2_country_from_aiw("GAME\\circuits\\BUENOS_AIRES\\1996CTDP_BUENOS.AIW", country, sizeof country) &&
              strcmp(country, "BUENOS AIRES") == 0);
        CHECK(sv2_country_from_aiw("GAME\\Circuits\\1995_x.AIW", country, sizeof country) == 0);
        CHECK(sv2_mode_token(4, -1, mode, sizeof mode) && strcmp(mode, "Championship") == 0);
        CHECK(sv2_mode_token(5, 2, mode, sizeof mode) && strcmp(mode, "CO-OP") == 0);
        CHECK(sv2_mode_token(5, -1, mode, sizeof mode) && strcmp(mode, "Multiplayer") == 0);
        CHECK(sv2_mode_token(1, -1, mode, sizeof mode) && strcmp(mode, "Test Day") == 0);
        CHECK(sv2_mode_token(9, -1, mode, sizeof mode) && mode[0] == 0);
        CHECK(sv2_session_token(7, sess, sizeof sess) && strcmp(sess, "Race") == 0);
        CHECK(sv2_session_token(6, sess, sizeof sess) && strcmp(sess, "Warmup") == 0);
        CHECK(sv2_session_token(0, sess, sizeof sess) && strcmp(sess, "Test Day") == 0);
        CHECK(sv2_session_token(8, sess, sizeof sess) && strcmp(sess, "S8") == 0);
    }
    CHECK(sv2_session_id(id, sizeof id, &t, 1, 3) && strcmp(id, "20261013-140500_s1_g3") == 0);
    CHECK(sv2_session_id(id, sizeof id, &t, -1, 3) && strcmp(id, "20261013-140500_s99_g3") == 0);
    CHECK(sv2_session_id(id, sizeof id, &t, 7, 0) && strcmp(id, "20261013-140500_s7_g0") == 0);
    /* collisions: base taken, _2 .. _8 taken, so the first free suffix is _9 */
    strcpy(base, "BRAZIL 1995 - Championship - Race - 2026-10-13 14-05");
    t2_taken_count = 0;
    t2_taken[t2_taken_count++] = base;
    for (int k = 2; k <= 8; k++) {
        static char names[8][160];
        snprintf(names[k - 2], sizeof names[k - 2], "%s_%d", base, k);
        t2_taken[t2_taken_count++] = names[k - 2];
    }
    strcpy(stem, base);
    CHECK(sv2_unique_stem(stem, sizeof stem, t2_exists, NULL) == 9 && strcmp(stem, "BRAZIL 1995 - Championship - Race - 2026-10-13 14-05_9") == 0);
    t2_taken_count = 0;
    strcpy(stem, base);
    CHECK(sv2_unique_stem(stem, sizeof stem, t2_exists, NULL) == 1 && strcmp(stem, base) == 0);
    /* weekend grouping (3.5) */
    memset(&w, 0, sizeof w);
    /* 1.1.1 (T2): key = country|year|mode; a Test Day after a race of the same key starts a new weekend */
    CHECK(sv2_weekend_key(key, sizeof key, "BRAZIL", 1995, "Race") && strcmp(key, "BRAZIL|1995|Race") == 0);
    CHECK(sv2_weekend_next(&w, "BRAZIL|1995|Race", 0) == 1);
    CHECK(sv2_weekend_next(&w, "BRAZIL|1995|Race", 1) == 1);
    CHECK(sv2_weekend_next(&w, "BRAZIL|1995|Race", 7) == 1);
    CHECK(sv2_weekend_next(&w, "BRAZIL|1995|Race", 0) == 2);
    CHECK(sv2_weekend_next(&w, "AUSTRALIA|1995|Race", 0) == 3);
    CHECK(sv2_weekend_next(&w, "AUSTRALIA|1995|Test Day", 0) == 4);
    CHECK(sv2_weekend_next(&w, "AUSTRALIA|1995|Test Day", 0) == 4);
    CHECK(sv2_weekend_next(&w, "AUSTRALIA|1996|Race", 0) == 5);
}

static void t2_f1_truncation(const uint8_t *f, size_t len) {
    unsigned bad = 0;
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    for (size_t cut = 0; cut <= len; cut++) {
        sv2_scan(f, cut, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep);
        if (cut < len && t2_rep.closed) bad++;
        if (t2_rep.valid_bytes > cut) bad++;
    }
    CHECK(bad == 0);
    sv2_scan(f, len, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep);
    CHECK(t2_rep.closed == 1 && t2_rep.reason == SV2_OK && t2_rep.valid_bytes == len);
}

static void t2_mutate(uint8_t *f, size_t len) {
    switch (t2_rnd(5)) {
    case 0: f[t2_rnd((uint32_t)len)] ^= (uint8_t)(1u << t2_rnd(8)); break;
    case 1: f[t2_rnd((uint32_t)len)] = (uint8_t)t2_rng(); break;
    case 2: {
        uint32_t a = t2_rnd((uint32_t)len), b = t2_rnd((uint32_t)len), k = 1 + t2_rnd(64);
        if ((size_t)a + k <= len && (size_t)b + k <= len) memmove(f + a, f + b, k);
        break;
    }
    case 3: {
        static const uint32_t v[] = {0u, 0xFFFFFFFFu, 0x00100000u, 0x00100001u, 1u, 0x7FFFFFFFu};
        size_t at = t2_rnd((uint32_t)len);
        uint32_t x = v[t2_rnd(6)];
        for (int i = 0; i < 4 && at + (size_t)i < len; i++) f[at + (size_t)i] = (uint8_t)(x >> (8 * i));
        break;
    }
    default: {
        size_t at = t2_rnd((uint32_t)len);
        f[at] = (uint8_t)(f[at] ^ 0xFFu);
        break;
    }
    }
}

static void t2_f2_mutation(const uint8_t *orig, size_t len, unsigned iters) {
    static uint8_t m[1u << 16];
    unsigned bad = 0;
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    for (unsigned k = 0; k < iters; k++) {
        memcpy(m, orig, len);
        for (unsigned j = 0, nm = 1 + t2_rnd(3); j < nm; j++) t2_mutate(m, len);
        sv2_scan(m, len, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep);
        if (t2_rep.valid_bytes > len || t2_rep.valid_blocks > 64 || t2_rep.channel_count > SV2_TABLE_MAX_CHANNELS) bad++;
    }
    CHECK(bad == 0);
}

static void t2_b_fallback_and_decoder(void) {
    static uint8_t fa[1u << 16], fb[1u << 16], fc[1u << 16];
    size_t la, lb, lc, stored = 0;
    uint8_t flags = 0;
    static uint8_t out[1u << 16];
    /* B-2: compressed META, decoder missing at read time: that block is DECODER_UNAVAILABLE, the rest reads */
    lb = t2_build_file(fb, sizeof fb, SV2_COMP_XPRESS, SV2_FLAG_COMPRESSED_BLOCKS, 1, 0);
    CHECK(lb > 0);
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(fb, lb, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK && t2_rep.skipped_blocks == 0);
    sv2_test_cabinet_forced_off = 1;
    CHECK(sv2_scan(fb, lb, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_R_DECODER_UNAVAILABLE);
    CHECK(t2_rep.skipped_blocks == 1 && t2_rep.data_blocks == 3 && t2_rep.closed == 1 && t2_rep.valid_bytes == lb);
    /* B-1: cabinet missing at write time: blocks go raw, flag set, reader reads, archive refused */
    CHECK(!sv2_alg_available(SV2_COMP_XPRESS) && !sv2_alg_available(SV2_COMP_LZMS));
    CHECK(sv2_pack_payload(SV2_COMP_XPRESS, t2_meta, T2_META_LEN, out, sizeof out, &stored, &flags) == 1);
    CHECK(flags == 0 && stored == T2_META_LEN);
    la = t2_build_file(fa, sizeof fa, SV2_COMP_XPRESS, SV2_FLAG_COMPRESSED_BLOCKS | SV2_FLAG_UNCOMPRESSED_FALLBACK, 1, 1);
    CHECK(la > 0);
    CHECK(sv2_scan(fa, la, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK);
    CHECK(t2_rep.closed == 1 && t2_rep.skipped_blocks == 0 && t2_rep.data_blocks == 3);
    CHECK(t2_rep.header.flags & SV2_FLAG_UNCOMPRESSED_FALLBACK);
    sv2_test_cabinet_forced_off = 0;
    /* a stale raw-only reader path with the cabinet back: same file still reads */
    CHECK(sv2_scan(fa, la, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK && t2_rep.closed == 1);
    /* LZMS archive file (compression_id 5) with a compressed META */
    lc = t2_build_file(fc, sizeof fc, SV2_COMP_LZMS, SV2_FLAG_ARCHIVED, 1, 1);
    CHECK(lc > 0);
    CHECK(sv2_scan(fc, lc, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK && t2_rep.closed == 1);
    CHECK(t2_rep.header.compression_id == SV2_COMP_LZMS && (t2_rep.header.flags & SV2_FLAG_ARCHIVED));
    /* F-1 on the three reference files, then F-2 */
    t2_f1_truncation(fa, la);
    t2_f1_truncation(fb, lb);
    t2_f1_truncation(fc, lc);
    t2_f2_mutation(fa, la, T2_F2_RAW);
    t2_f2_mutation(fb, lb, T2_F2_PACKED);
    t2_f2_mutation(fc, lc, T2_F2_PACKED);
}

static void test_storage_v2(void) {
    t2_u1_quantizer();
    t2_u2_zigzag_varint();
    t2_u3_crc();
    t2_u4_data_roundtrip();
    t2_u5_header_table();
    t2_u6_block_framing();
    t2_u7_compression();
    t2_u8_aiw();
    t2_u9_naming();
    t2_b_fallback_and_decoder();
    printf("PASS storage v2 core: U-1..U-9, F-1, F-2, B-1, B-2\n");
    fflush(stdout);
}

/* ---- Telemetry 1.1.0 batch 2: writer, ring, session boundaries, folder and index (spec 3, 6, 7.1, 7.2, 11) ---- */
static void b2_mkdir(const char *p) { CreateDirectoryA(p, NULL); }
static void b2_base(char *out, size_t cap, const char *tag) {
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof tmp, tmp);
    CHECK(n > 0 && n < sizeof tmp);
    sw_fmt(out, cap, "%sf1c-b2-%s-%lu\\", tmp, tag, (unsigned long)GetTickCount());
    b2_mkdir(out);
}
static size_t b2_read_file(const char *p, uint8_t **out) {
    FILE *f = fopen(p, "rb");
    long n;
    uint8_t *b;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n + 1);
    CHECK(b != NULL);
    CHECK(fread(b, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    *out = b;
    return (size_t)n;
}
static void b2_write_file(const char *p, const void *d, size_t n) {
    FILE *f = fopen(p, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(d, 1, n, f) == n);
    fclose(f);
}

/* Action string for one step: C<reason> O<type> s P, '.' when nothing. */
static void b2_act_str(sw_act_t a, char *out) {
    char *p = out;
    if (!a.acts) { strcpy(out, "."); return; }
    if (a.acts & SWA_CLOSE) {
        static const char letters[] = "?TDRS?";   /* SWR_TYPE, SWR_SIM_RESET, SWR_RESULTS, SWR_STALE */
        *p++ = 'C'; *p++ = letters[a.reason >= 1 && a.reason <= 4 ? a.reason : 0];
    }
    if (a.acts & SWA_OPEN) { *p++ = 'O'; *p++ = (char)('0' + a.open_type); }
    if (a.acts & SWA_SAMPLE) *p++ = 's';
    if (a.acts & SWA_COMPLETE) *p++ = 'P';
    *p = 0;
}

typedef struct { int type, phase, veh; double sim; const char *exp; } b2_step_t;

static void b2_run_scenario(const char *name, const b2_step_t *st, unsigned n) {
    sw_sm_t m;
    memset(&m, 0, sizeof m);
    for (unsigned i = 0; i < n; i++) {
        sw_obs_t o;
        sw_act_t a;
        char got[24];
        o.session_type = st[i].type; o.race_phase = st[i].phase; o.vehicle_count = st[i].veh;
        o.sim = st[i].sim; o.now_ms = 1000u * i;
        a = sw_sm_step(&m, &o);
        b2_act_str(a, got);
        if (strcmp(got, st[i].exp)) {
            fprintf(stderr, "FAIL scenario %s step %u: got %s expected %s (type %d sim %.3f)\n",
                    name, i, got, st[i].exp, st[i].type, st[i].sim);
            exit(2);
        }
        ++checks;
    }
}

/* Session boundary replay: the seven live sequences of 2026-10-13, as synthetic state streams.
 * Expected strings: C<reason> close (T type, D sim reset, R results, S stale), O<type> open, s sample, P complete. */
static void t3_session_replay(void) {
    static b2_step_t ladder[21];
    static const char *lad_exp[21] = {".", "O1s", "s", "CTO2s", "s", "s", "CTO3s", "s", "s",
                                      "CTO4s", "s", "s", "CTO5s", "s", "s", "CTO6s", "s", "s",
                                      "CTO7s", "s", "s"};
    /* session-type-ladder-1-7-live-20261013: P1..P4, Qualifying (5), Warm-up (6), Race (7); sim resets per session */
    for (int t = 1; t <= 7; t++)
        for (int k = 0; k < 3; k++) {
            ladder[(t - 1) * 3 + k].type = t;
            ladder[(t - 1) * 3 + k].phase = 5; ladder[(t - 1) * 3 + k].veh = 22;
            ladder[(t - 1) * 3 + k].sim = 0.4 + 0.1 * k;
            ladder[(t - 1) * 3 + k].exp = lad_exp[(t - 1) * 3 + k];
        }
    b2_run_scenario("ladder-1-7", ladder, 21);

    /* exit-to-monitor-state5-to-2-not-session-end-20261013: sim keeps advancing, no close (game_state is not read) */
    {
        static const b2_step_t s[] = {
            {1, 5, 22, 14.6, "."}, {1, 5, 22, 15.6, "O1s"}, {1, 5, 22, 20.0, "s"}, {1, 5, 22, 32.6, "s"}, {1, 5, 22, 33.0, "s"}};
        b2_run_scenario("exit-to-monitor", s, 5);
    }
    /* session-switch-p1-to-p2-no-1e9-20261013: type 1 -> 2, sim 9.57 -> 0.43 */
    {
        static const b2_step_t s[] = {
            {1, 5, 22, 9.0, "."}, {1, 5, 22, 9.57, "O1s"}, {2, 5, 22, 0.43, "CTO2s"}, {2, 5, 22, 0.9, "s"}};
        b2_run_scenario("p1-to-p2", s, 4);
    }
    /* session-time-expiry-race-phase-8-20261013: Qualifying, race_phase 5 -> 8, sim keeps running */
    {
        static const b2_step_t s[] = {
            {5, 5, 22, 1200, "."}, {5, 5, 22, 1210, "O5s"}, {5, 5, 22, 1220, "s"}, {5, 8, 22, 1230, "sP"}, {5, 8, 22, 1303, "s"}};
        b2_run_scenario("quali-expiry", s, 5);
    }
    /* race-checkered-race-phase-8-20261013 + classification-screen-sim-1e9-20261013: results close, then nothing */
    {
        static const b2_step_t s[] = {
            {7, 5, 22, 270, "."}, {7, 5, 22, 271, "O7s"}, {7, 8, 22, 275, "sP"}, {7, 8, 22, 508, "s"},
            {7, 8, 22, 1000000010.4, "CR"}, {7, 8, 22, 1000000011.4, "."}};
        b2_run_scenario("race-checkered-classification", s, 6);
    }
    /* quit-after-race-state2-sim-frozen-20261013: frozen sim above 1e8 never opens; then a new session starts */
    {
        static const b2_step_t s[] = {
            {7, 8, 22, 1000000064.0, "."}, {7, 8, 22, 1000000064.0, "."}, {7, 8, 22, 1000000064.0, "."},
            {1, 0, 22, 0.4, "O1s"}, {1, 0, 22, 0.5, "s"}};
        b2_run_scenario("quit-after-race", s, 5);
    }
    /* Extra: a frozen session (quit mid-session, no results) closes on the 30 s stale rule. */
    {
        static b2_step_t s[34];
        for (unsigned i = 0; i < 34; i++) {
            s[i].type = 1; s[i].phase = 5; s[i].veh = 22; s[i].sim = 50.0;
            s[i].exp = (i == 0) ? "." : (i == 1 ? "O1s" : "s");
            if (i == 0) s[i].sim = 49.0;   /* sim advances once (opens), then freezes */
        }
        s[32].exp = "CS";   /* now 32000 ms, last live 1000 ms: 31 s > 30 s */
        s[33].exp = ".";
        b2_run_scenario("frozen-stale", s, 34);
    }
    /* Extra: sim reset inside one session_type (live: sim 0.5 with type 1) closes and reopens. */
    {
        static const b2_step_t s[] = {
            {1, 5, 22, 60.0, "."}, {1, 5, 22, 61.0, "O1s"}, {1, 5, 22, 0.5, "CDO1s"}, {1, 5, 22, 0.6, "s"}};
        b2_run_scenario("sim-reset-same-type", s, 4);
    }
    /* Extra: no cars present never opens; an invalid session_type never opens. */
    {
        static const b2_step_t s[] = {{1, 5, 0, 10.0, "."}, {1, 5, 0, 11.0, "."}, {9, 5, 22, 12.0, "."}};
        b2_run_scenario("no-cars-bad-type", s, 3);
    }
    printf("PASS session replay: 7 live sequences + stale, reset and invalid cases\n");
    fflush(stdout);
}

/* Index, OS gate and folder layout (U-10, B-3, spec 3.1, 3.6). */
static void t3_u10_index_and_os_gate(void) {
    char base[1024], game[1024], old[1024], why[200], p[1024];
    uint8_t *buf = NULL;
    size_t n;
    CHECK(sw_os_ok(6, 1) == 0 && sw_os_ok(6, 2) == 1 && sw_os_ok(6, 3) == 1 && sw_os_ok(10, 0) == 1 && sw_os_ok(5, 9) == 0);
    b2_base(base, sizeof base, "idx");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    /* B-3: a Windows 7 (6.1) stub refuses before anything is created */
    sw_fmt(old, sizeof old, "%sold\\", base);
    b2_mkdir(old);
    CHECK(sw_root_init(old, 6, 1, why, sizeof why) == SW_REFUSE_OS);
    sw_fmt(p, sizeof p, "%sTelemetry", old);
    CHECK(!sw_path_exists(p));
    /* a plain file named Telemetry is refused, not renamed */
    sw_fmt(old, sizeof old, "%sclash\\", base);
    b2_mkdir(old);
    sw_fmt(p, sizeof p, "%sclash\\Telemetry", base);
    b2_write_file(p, "x", 1);
    sw_fmt(old, sizeof old, "%sclash\\", base);
    CHECK(sw_root_init(old, 6, 2, why, sizeof why) == SW_REFUSE_FILE);
    CHECK(sw_path_exists(p) && !sw_is_dir(p));
    /* normal layout */
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    CHECK(sw_is_dir(sw_dir) && sw_is_dir(sw_rec_dir));
    sw_enabled = 1;
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_entry_add("Folder_A", "Folder_A.f1t", "Track_2002_w1", SW_ST_CLOSED, 1234, 40);
    CHECK(sw_index_write() == 1);
    CHECK(sw_index_valid(sw_index_path) == 1);
    /* U-10: corrupt cache (half of the file) is refused; a folder scan rebuilds it */
    n = b2_read_file(sw_index_path, &buf);
    CHECK(n > 40);
    b2_write_file(sw_index_path, buf, n / 2);
    CHECK(sw_index_valid(sw_index_path) == 0);
    free(buf);
    sw_fmt(p, sizeof p, "%sFolder_A", sw_rec_dir); b2_mkdir(p);
    sw_fmt(p, sizeof p, "%sFolder_A\\Folder_A.f1t", sw_rec_dir); b2_write_file(p, "0123456789", 10);
    sw_fmt(p, sizeof p, "%sFolder_B", sw_rec_dir); b2_mkdir(p);
    sw_fmt(p, sizeof p, "%sFolder_B\\Folder_B.f1t.part", sw_rec_dir); b2_write_file(p, "abc", 3);
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_scan_folders();
    CHECK(sw_nentries == 2);
    CHECK(sw_entries[0].state == SW_ST_CLOSED && sw_entries[0].bytes == 10.0);
    CHECK(sw_entries[1].state == SW_ST_DAMAGED);   /* leftover .part: listed, repair is batch 6 */
    CHECK(sw_index_write() == 1 && sw_index_valid(sw_index_path) == 1);
    sw_fmt(p, sizeof p, "%s.tmp", sw_index_path);
    CHECK(!sw_path_exists(p));   /* atomic replace leaves no temp file */
    printf("PASS index U-10 and OS gate B-3 (6.1 refused, Telemetry file refused, folder layout)\n");
    fflush(stdout);
}

/* U-11: single producer (capture), single consumer (writer), random stalls. Drops counted exactly. */
/* Stale-list fix: after a delete job, the writer rebuild drops the removed folder while a session is open,
 * keeps the open session listed as OPEN, and re-pins sw_wr.entry. Headless: no game, no capture path. */
static void t3_u12_rebuild_keeps_open(void) {
    char base[1024], game[1024], p[1024], why[200];
    b2_base(base, sizeof base, "stale");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_enabled = 1;
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_fmt(p, sizeof p, "%sFolder_A", sw_rec_dir); b2_mkdir(p);
    sw_fmt(p, sizeof p, "%sFolder_A\\Folder_A.f1t", sw_rec_dir); b2_write_file(p, "0123456789", 10);
    sw_fmt(p, sizeof p, "%sFolder_B", sw_rec_dir); b2_mkdir(p);
    sw_fmt(p, sizeof p, "%sFolder_B\\Live.f1t.part", sw_rec_dir); b2_write_file(p, "abc", 3);
    sw_scan_folders();
    CHECK(sw_nentries == 2);
    /* the writer has session Live open in Folder_B */
    sw_wr.have = 1;
    sv2_copy(sw_wr.folder_name, sizeof sw_wr.folder_name, "Folder_B");
    sv2_copy(sw_wr.stem, sizeof sw_wr.stem, "Live");
    sv2_copy(sw_wr.weekend, sizeof sw_wr.weekend, "");
    sw_wr.bytes = 3.0;
    sw_wr.total_ticks = 7;
    sw_wr.entry = SW_ENTRIES;
    /* the delete job removes Folder_A (file, then folder) and requests a rebuild */
    sw_fmt(p, sizeof p, "%sFolder_A\\Folder_A.f1t", sw_rec_dir); CHECK(DeleteFileA(p));
    sw_fmt(p, sizeof p, "%sFolder_A", sw_rec_dir); CHECK(RemoveDirectoryA(p));
    InterlockedExchange(&sw_rebuild_req, 1);
    /* what sw_writer_main runs between writes: take the request, then rebuild */
    CHECK(InterlockedExchange(&sw_rebuild_req, 0) == 1);
    sw_rebuild_entries();
    CHECK(sw_wr.have == 1);                              /* the session is still open */
    CHECK(sw_nentries == 1);                             /* Folder_A gone from the list */
    CHECK(!strcmp(sw_entries[0].folder, "Folder_B") && !strcmp(sw_entries[0].file, "Live.f1t.part"));
    CHECK(sw_entries[0].state == SW_ST_OPEN);            /* open, not DAMAGED */
    CHECK(sw_wr.entry == 0);
    /* a second rebuild is idempotent: no duplicate open entry, index still valid */
    sw_rebuild_entries();
    CHECK(sw_nentries == 1 && sw_wr.entry == 0 && sw_entries[0].state == SW_ST_OPEN);
    sw_wr.have = 0;
    sw_wr.entry = SW_ENTRIES;
    printf("PASS stale list U-12 (open session kept after delete rebuild, removed folder dropped)\n");
    fflush(stdout);
}

static volatile LONG t3_prod_done;
static unsigned t3_prod_n;
static DWORD WINAPI t3_producer(void *unused) {
    unsigned seed = 12345u;
    (void)unused;
    for (unsigned i = 0; i < t3_prod_n; i++) {
        sw_event_t *e = sw_ring_claim(SWE_SAMPLE);
        if (e) { e->kind = SWE_SAMPLE; e->tick_ms = i; e->n_cars = 0; sw_ring_publish(); }
        seed = seed * 1103515245u + 12345u;
        if ((seed >> 16) % 4096u == 0) Sleep(0);
    }
    MemoryBarrier();
    t3_prod_done = 1;
    return 0;
}
static void t3_u11_ring(void) {
    HANDLE th;
    LONG d0, consumed = 0;
    unsigned last = 0;
    int first = 1;
    CHECK(sw_ring_init());
    t3_prod_n = 10000000u;
    t3_prod_done = 0;
    d0 = sw_ring.dropped;
    th = CreateThread(NULL, 0, t3_producer, NULL, 0, NULL);
    CHECK(th != NULL);
    for (;;) {
        sw_event_t *e = sw_ring_peek();
        if (!e) {
            MemoryBarrier();
            if (t3_prod_done && !sw_ring_peek()) break;
            continue;
        }
        if (!first) CHECK(e->tick_ms > last);   /* order preserved, no duplicates */
        first = 0;
        last = e->tick_ms;
        consumed++;
        sw_ring_pop();
    }
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
    CHECK(consumed + (sw_ring.dropped - d0) == (LONG)t3_prod_n);
    CHECK(sw_ring.control_lost == 0);
    printf("PASS ring U-11: %u pushes, %ld consumed, %ld dropped (counted exactly, no blocking)\n",
           t3_prod_n, (long)consumed, (long)(sw_ring.dropped - d0));
    fflush(stdout);
}

/* Synthetic capture: player in slot 0, cars-1 AI cars, all channels valid. */
static void t3_set_tick(int type, double sim, int cars, int phase) {
    g_telemetry.session_type = type;
    g_telemetry.race_phase = phase;
    g_telemetry.vehicle_count = cars;
    g_telemetry.session_time = sim;
    g_telemetry.player_slot = 0;
    snprintf(g_telemetry.aiw_path, sizeof g_telemetry.aiw_path, "%s", "SEASONDATA\\CIRCUITS\\Italy\\2001_Monza\\2001_Monza.AIW");
    for (int i = 0; i < MAX_CARS; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        memset(c, 0, sizeof *c);
        c->on_track = i < cars;
        c->car_valid = 1 | 2 | 4 | 8 | 16 | 32;   /* batch 3: bit 16 (tread valid) on */
        c->tire_wear_fl = 0.875f; c->tire_wear_fr = 0.875f; c->tire_wear_rl = 0.875f; c->tire_wear_rr = 0.875f;
        c->gear = 4;
        c->lap_dist = (float)(i * 10) + (float)(sim * 0.1);
        c->speed = 50.0f + (float)i; c->throttle = 0.5f; c->brake = 0.0f; c->rpm = 9000.0f;
        c->glat = 1.0f; c->glon = -1.0f; c->gvert = 0.2f; c->yaw = 0.3f; c->yaw_rate = 0.1f;
        c->track_x = 10.0f; c->track_z = -5.0f; c->steer = 0.1f;
        for (int k = 0; k < 4; k++) c->ttemp[k] = 80.0f;
    }
}

static double t3_qpc_ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart;
}

/* Runs the writer on a fresh temp root for `ticks` capture ticks (virtual 20 Hz clock). */
static void t3_run_writer(const char *tag, unsigned ticks, DWORD disk_delay_ms, int pace_per_block,
                          double *producer_ms, double *max_tick_ms, LONG *dropped_out) {
    char base[1024], game[1024], why[200], fpath[1400];
    LARGE_INTEGER f, t0, t1, a, b;
    double mx = 0.0;
    LONG d0;
    QueryPerformanceFrequency(&f);
    b2_base(base, sizeof base, tag);
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_enabled = 1;
    memset(&sw_sm, 0, sizeof sw_sm);
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_wr.gap_reported = 0;
    sw_wk.have = 0;
    sw_wr.folder[0] = 0;
    CHECK(sw_ring_init());
#ifdef TELEMETRY_TEST
    sw_test_write_delay_ms = disk_delay_ms;
#endif
    CHECK(sw_start() == 1);
    d0 = sw_ring.dropped;
    QueryPerformanceCounter(&t0);
    for (unsigned i = 0; i < ticks; i++) {
        double sim = 0.4 + 0.05 * (double)i;
        double d;
        t3_set_tick(1, sim, 24, 5);
        QueryPerformanceCounter(&a);
        sw_capture_tick(50u * i);
        QueryPerformanceCounter(&b);
        d = t3_qpc_ms(a, b, f);
        if (d > mx) mx = d;
        if (pace_per_block && (i % SW_BLOCK_TICKS) == SW_BLOCK_TICKS - 1) Sleep(1);
    }
    QueryPerformanceCounter(&t1);
    *producer_ms = t3_qpc_ms(t0, t1, f);
    *max_tick_ms = mx;
    *dropped_out = sw_ring.dropped - d0;
    CHECK(sw_stop(60000) == 1);
#ifdef TELEMETRY_TEST
    sw_test_write_delay_ms = 0;
#endif
    CHECK(sw_entries[0].state == SW_ST_CLOSED);
    sw_fmt(fpath, sizeof fpath, "%s%s\\%s", sw_rec_dir, sw_entries[0].folder, sw_entries[0].file);
    CHECK(sw_path_exists(fpath));
}

/* Reads the finished session file with the bounded reader and checks the totals. */
static void t3_check_file(unsigned expect_samples, LONG expect_dropped) {
    char fpath[1400];
    uint8_t *buf = NULL;
    size_t n;
    sw_fmt(fpath, sizeof fpath, "%s%s\\%s", sw_rec_dir, sw_entries[0].folder, sw_entries[0].file);
    n = b2_read_file(fpath, &buf);
    CHECK(n > 0);
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(buf, n, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK);
    CHECK(t2_rep.closed == 1 && t2_rep.truncated == 0);
    CHECK(t2_rep.ticks == expect_samples);
    CHECK(t2_rep.end_total_ticks == expect_samples);
    CHECK((LONG)t2_rep.end_dropped_ticks == expect_dropped);
    CHECK(t2_rep.header.flags & SV2_FLAG_CLOSED);
    CHECK(t2_rep.channel_count == SV2_CATALOG_COUNT);
    free(buf);
}

/* 2.1 (B1) writer check: a lap change, a pit change and a contact each write one EVENT block, a session_start and a
 * session_end are written, a 20-tick pause (sim unchanged) is written with the sim time, and the reader reports the
 * sim time of the last tick. Full gate only (game closed); pure codec checks live in tests/storage_b1_codec_test.c. */
static void t3_b1_format21(void) {
    char base[1024], game[1024], why[200];
    unsigned ticks = 240u;
    double sim_now = 0.0;
    b2_base(base, sizeof base, "b1fmt21");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_enabled = 1;
    memset(&sw_sm, 0, sizeof sw_sm);
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_wr.gap_reported = 0;
    sw_wk.have = 0;
    sw_wr.folder[0] = 0;
    CHECK(sw_ring_init());
    CHECK(sw_start() == 1);
    for (unsigned i = 0; i < ticks; i++) {
        if (!(i >= 150u && i < 170u)) sim_now = 0.4 + 0.05 * (double)i;   /* ticks 150..169: paused, sim unchanged */
        t3_set_tick(1, sim_now, 24, 5);
        if (i >= 60u) g_telemetry.cars[1].lap_number = 2;          /* lap event at tick 60 */
        if (i >= 100u) g_telemetry.cars[1].pitting = 3;            /* pit event at tick 100 */
        if (i >= 120u) g_telemetry.cars[0].contact_with = 2;       /* contact incident at tick 120 */
        sw_capture_tick(50u * i);
        /* gate2 (2026-10-13): stale timing precondition. The ring holds SW_RING_CAP=48 slots and a 240-tick burst
         * with no pause overflows it (drops are counted and GAP-reconciled: 39 kept + 200 dropped = 239). Pace one
         * sleep per block as t3_run_writer does, so the test checks the format, not the burst speed. */
        if ((i % SW_BLOCK_TICKS) == SW_BLOCK_TICKS - 1) Sleep(1);
    }
    CHECK(sw_stop(60000) == 1);
    CHECK(sw_entries[0].state == SW_ST_CLOSED);
    /* gate2 (2026-10-13): stale expectation. Tick 0 only primes sw_sm_step (no have_last, so no open);
     * the session opens on the first advancing tick. Samples = ticks - 1, as in t3_slow_disk/t3_s1_soak. */
    t3_check_file(ticks - 1u, 0);
    /* session_start, lap, pit, contact incident, session_end */
    CHECK(t2_rep.events == 5);
    CHECK(fabs(t2_rep.last_good_t - (0.4 + 0.05 * (double)(ticks - 1))) < 0.001);
}

/* Slow fake disk (50 ms per block): the capture side must never wait for it (spec 6.2, 11.4). */
static void t3_slow_disk(void) {
    double prod = 0.0, mx = 0.0;
    LONG dropped = 0;
    t3_run_writer("slow", 2000u, 50u, 0, &prod, &mx, &dropped);
    /* 100 blocks x 50 ms = 5 s of disk time; the producer must not wait for it */
    CHECK(prod < 2000.0);
    CHECK(mx < 20.0);
    CHECK(dropped > 0);   /* the ring overflowed and the drop policy ran */
    /* ticks 1..1999 are samples (tick 0 only primes the state machine); drops reconcile exactly */
    t3_check_file(1999u - (unsigned)dropped, dropped);
    printf("PASS writer never blocks capture: slow disk 50 ms/block, 2000 ticks in %.1f ms, max tick %.3f ms, %ld dropped (counted, GAP written)\n",
           prod, mx, (long)dropped);
    fflush(stdout);
}

/* S-1: 24 cars, 20 Hz, 36000 ticks (30 min), recording on, writer on a real temp file. Paced one sleep per block. */
static void t3_s1_soak(void) {
    double prod = 0.0, mx = 0.0;
    LONG dropped = 0;
    t3_run_writer("s1", 36000u, 0u, 1, &prod, &mx, &dropped);
    CHECK(dropped == 0);
    t3_check_file(35999u, 0);
    printf("PASS S-1 soak (host, paced one sleep per block): 36000 ticks, 24 cars, %ld dropped, producer %.0f ms, max tick %.3f ms\n",
           (long)dropped, prod, mx);
    fflush(stdout);
}

static void test_storage_writer(void) {
    t3_session_replay();
    t3_b1_format21();
    t3_u10_index_and_os_gate();
    t3_u12_rebuild_keeps_open();
    t3_u11_ring();
    t3_slow_disk();
    t3_s1_soak();
    printf("PASS storage writer batch 2: U-10, U-11, B-3, S-1, slow-disk, session replay\n");
    fflush(stdout);
}

/* ---------- batch 3: jobs (7.5, 7.7, 7.8, 18) and channel round trip ---------- */
typedef struct { size_t end; unsigned ticks; int type; } j3_info_t;

/* Walks the block frames of a file image. Returns the block count (0 when the header fails). */
static unsigned j3_walk(const uint8_t *buf, size_t n, j3_info_t *out, unsigned cap) {
    sv2_header_t h;
    uint8_t *work = (uint8_t *)malloc(SJ_WORK);
    unsigned k = 0;
    size_t pos;
    uint32_t seq = 0;
    CHECK(work != NULL);
    if (sv2_header_decode(buf, n, &h) != SV2_OK) { free(work); return 0; }
    pos = (size_t)h.channel_table_offset + h.channel_table_bytes;
    while (pos < n) {
        sj_blk_t x;
        if (sj_block_at(buf, n, pos, seq, h.compression_id, work, &x) != SV2_OK) break;
        if (k < cap) { out[k].end = x.next; out[k].ticks = x.b.type == SV2_BLOCK_DATA ? x.b.n_ticks : 0u; out[k].type = x.b.type; }
        k++;
        pos = x.next;
        seq++;
    }
    free(work);
    return k;
}

/* U-12: cut the closed file at the table end, at every block boundary and inside blocks; repair each. */
static void j3_repair_cases(const uint8_t *buf, size_t n, const char *dir) {
    static j3_info_t info[512];
    unsigned nb = j3_walk(buf, n, info, 512u), idx = 0;
    sv2_header_t h;
    size_t tbl;
    CHECK(nb >= 3 && info[nb - 1].type == SV2_BLOCK_END);
    CHECK(sv2_header_decode(buf, n, &h) == SV2_OK);
    tbl = (size_t)h.channel_table_offset + h.channel_table_bytes;
    for (unsigned i = 0; i + 1 < nb; i++) {
        size_t start = i ? info[i - 1].end : tbl;
        size_t cuts[3];
        unsigned nc = 0;
        cuts[nc++] = info[i].end;
        if (start + 7 < info[i].end) cuts[nc++] = start + 7;
        if (start + (info[i].end - start) / 2 > start) cuts[nc++] = start + (info[i].end - start) / 2;
        if (i == 0) cuts[nc++] = tbl;
        for (unsigned q = 0; q < nc; q++) {
            size_t c = cuts[q];
            unsigned d = 0;
            unsigned long long t = 0;
            char cdir[1400], part[1500], fin[1500], dmg[1500], why[SJ_MSG];
            uint8_t *rb = NULL, *db = NULL;
            size_t rn, dn;
            sv2_scan_t rep;
            int rc;
            for (unsigned j = 0; j + 1 < nb; j++) if (info[j].end <= c && info[j].type == SV2_BLOCK_DATA) { d++; t += info[j].ticks; }
            char rbin[1400], binf[1500];
            sw_fmt(cdir, sizeof cdir, "%s\\rc%u", dir, idx);
            b2_mkdir(cdir);
            /* 1.1.1: the recovered original goes to the Recycle Bin double in this test folder, never to the real bin */
            sw_fmt(rbin, sizeof rbin, "%s\\rbin%u", dir, idx);
            b2_mkdir(rbin);
            sw_fmt(sj_test_bin, sizeof sj_test_bin, "%s\\", rbin);
            sw_fmt(binf, sizeof binf, "%s\\item0.bin", rbin);
            sj_test_shfo = sj_test_shfo_double;
            sj_test_recycle = 1;
            sj_test_mode = 0;
            sw_fmt(part, sizeof part, "%s\\c.f1t.part", cdir);
            sw_fmt(fin, sizeof fin, "%s\\c.f1t", cdir);
            sw_fmt(dmg, sizeof dmg, "%s.damaged", part);
            b2_write_file(part, buf, c);
            rc = sj_repair_part(part, why, sizeof why);
            if (d) {
                CHECK(rc == SJ_OK);
                CHECK(!sw_path_exists(part));
                CHECK(sw_path_exists(fin) && !sw_path_exists(dmg));   /* 1.1.1: the original left for the Recycle Bin */
                rn = b2_read_file(fin, &rb);
                CHECK(rn > 0);
                t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
                CHECK(sv2_scan(rb, rn, &t2_scan_data, t2_work, sizeof t2_work, &rep) == SV2_OK);
                CHECK(rep.closed == 1 && rep.valid_bytes == rn);
                CHECK(rep.ticks == t);
                CHECK(rep.header.flags & SV2_FLAG_RECOVERED);
                dn = b2_read_file(binf, &db);
                CHECK(dn == c);   /* the original is in the bin double, byte count unchanged */
            } else {
                CHECK(rc == SJ_E_DAMAGED);
                CHECK(sw_path_exists(part) && !sw_path_exists(fin));
            }
            free(rb);
            free(db);
            idx++;
        }
    }
    sj_test_shfo = NULL;
    sj_test_recycle = -1;
    printf("PASS repair U-12: %u cut points (block boundaries, inside blocks, after the table): recovered or left unchanged, originals kept\n", idx);
    fflush(stdout);
}

/* U-13: archive to LZMS; bit-exact payloads; working copy unchanged; re-run is idempotent; LZMS off refuses. */
static void j3_archive_cases(const char *src_path, const uint8_t *buf, size_t n, const char *dir) {
    char f1a[1400], partial[1500], off[1400], offa[1400], why[SJ_MSG], offdir[1400];
    uint8_t *a = NULL, *s2 = NULL, *w1 = (uint8_t *)malloc(SJ_WORK), *w2 = (uint8_t *)malloc(SJ_WORK), *o = NULL;
    size_t an, s2n, on;
    sv2_scan_t rs, ra;
    int rc;
    CHECK(w1 && w2);
    sw_fmt(f1a, sizeof f1a, "%.*sf1a", (int)(strlen(src_path) - 3), src_path);
    sw_fmt(partial, sizeof partial, "%s.partial", f1a);
    rc = sj_archive_file(src_path, why, sizeof why);
    if (rc != SJ_OK) fprintf(stderr, "archive refused: code %d: %s\n", rc, why);
    CHECK(rc == SJ_OK);
    CHECK(sw_path_exists(f1a) && !sw_path_exists(partial));
    an = b2_read_file(f1a, &a);
    CHECK(an > 0);
    s2n = b2_read_file(src_path, &s2);
    CHECK(s2n == n && memcmp(s2, buf, n) == 0);   /* working copy byte-unchanged (7.7 step 5) */
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(s2, s2n, &t2_scan_data, t2_work, sizeof t2_work, &rs) == SV2_OK);
    CHECK(sv2_scan(a, an, &t2_scan_data, t2_work, sizeof t2_work, &ra) == SV2_OK);
    CHECK(ra.closed == 1 && ra.valid_bytes == an);
    CHECK(ra.header.flags & SV2_FLAG_ARCHIVED);
    CHECK(ra.header.compression_id == SV2_COMP_LZMS);
    CHECK(ra.ticks == rs.ticks && ra.data_blocks == rs.data_blocks);
    CHECK(sj_same_payloads(s2, s2n, &rs, a, an, &ra, w1, w2));   /* AC-2 bit-exact payloads */
    rc = sj_archive_file(src_path, why, sizeof why);
    CHECK(rc == SJ_OK);   /* idempotent: existing archive verifies, nothing rewritten */
    /* LZMS off: refused before anything is created; the working copy stays */
    sw_fmt(offdir, sizeof offdir, "%s\\off", dir);
    b2_mkdir(offdir);
    sw_fmt(off, sizeof off, "%s\\off.f1t", offdir);
    sw_fmt(offa, sizeof offa, "%s\\off.f1a", offdir);
    b2_write_file(off, buf, n);
#ifdef TELEMETRY_TEST
    sv2_test_cabinet_forced_off = 1;
#endif
    rc = sj_archive_file(off, why, sizeof why);
#ifdef TELEMETRY_TEST
    sv2_test_cabinet_forced_off = 0;
#endif
    CHECK(rc == SJ_E_LZMS);
    CHECK(!sw_path_exists(offa));
    on = b2_read_file(off, &o);
    CHECK(on == n && memcmp(o, buf, n) == 0);
    printf("PASS archive U-13: LZMS bit-exact payloads (%zu blocks, %u ticks), idempotent, LZMS-off refused with working copy unchanged\n",
           (size_t)ra.valid_blocks, (unsigned)ra.ticks);
    fflush(stdout);
    free(a); free(s2); free(o); free(w1); free(w2);
}

/* U-14: Recycle Bin delete through a SHFileOperationW double. */
static void j3_delete_cases(const char *dir) {
    char a[1400], b[1400], bin[1400], why[SJ_MSG];
    const char *paths[2];
    unsigned moved = 0, left = 0, k = 0;
    int c0, rc;
    static char blob[256];
    memset(blob, 'x', sizeof blob);
    paths[0] = a;
    paths[1] = b;
    sw_fmt(a, sizeof a, "%s\\del_a.bin", dir);
    sw_fmt(b, sizeof b, "%s\\del_b.bin", dir);
#define J3_MAKE_AB() do { b2_write_file(a, blob, 100); b2_write_file(b, blob, 200); } while (0)
#define J3_NEW_BIN() do { sw_fmt(bin, sizeof bin, "%s\\bin%u\\", dir, k++); b2_mkdir(bin); sw_fmt(sj_test_bin, sizeof sj_test_bin, "%s", bin); } while (0)
    sj_test_shfo = sj_test_shfo_double;
    sj_test_recycle = 1;
    /* (a) success: every item moved, FOF_ALLOWUNDO requested */
    J3_NEW_BIN(); J3_MAKE_AB(); sj_test_mode = 0; c0 = sj_test_calls;
    rc = sj_delete(paths, 2, 300.0, why, sizeof why, &moved, &left);
    CHECK(rc == SJ_OK && moved == 2 && left == 0);
    CHECK(sj_test_calls == c0 + 1);
    CHECK((sj_test_flags & FOF_ALLOWUNDO) != 0);
    CHECK(!sw_path_exists(a) && !sw_path_exists(b));
    /* (b) double returns an error: nothing reported as moved, no second call */
    J3_NEW_BIN(); J3_MAKE_AB(); sj_test_mode = 1; c0 = sj_test_calls;
    rc = sj_delete(paths, 2, 300.0, why, sizeof why, &moved, &left);
    CHECK(rc == SJ_E_ABORT && moved == 0 && left == 2);
    CHECK(sj_test_calls == c0 + 1);
    CHECK(sw_path_exists(a) && sw_path_exists(b));
    /* (c) aborted after one item: abort reported, the remainder listed */
    J3_NEW_BIN(); J3_MAKE_AB(); sj_test_mode = 2; c0 = sj_test_calls;
    rc = sj_delete(paths, 2, 300.0, why, sizeof why, &moved, &left);
    CHECK(rc == SJ_E_ABORT && moved == 1 && left == 1);
    CHECK(!sw_path_exists(a) && sw_path_exists(b));
    CHECK(sj_test_calls == c0 + 1);
    /* (d) Recycle Bin pre-check fails: no call at all */
    sj_test_recycle = 0; c0 = sj_test_calls;
    rc = sj_delete(paths + 1, 1, 200.0, why, sizeof why, &moved, &left);
    CHECK(rc == SJ_E_RECYCLE_OFF && sj_test_calls == c0 && sw_path_exists(b));
    sj_test_recycle = 1;
    /* (e) confirmed size differs: 409 path, no call */
    c0 = sj_test_calls;
    rc = sj_delete(paths + 1, 1, 999.0, why, sizeof why, &moved, &left);
    CHECK(rc == SJ_E_CHANGED && sj_test_calls == c0 && sw_path_exists(b));
    /* (f) an open session .part is refused, no call */
    sw_wr.have = 1;
    sv2_copy(sw_wr.part_path, sizeof sw_wr.part_path, b);
    c0 = sj_test_calls;
    rc = sj_delete(paths + 1, 1, 200.0, why, sizeof why, &moved, &left);
    sw_wr.have = 0;
    CHECK(rc == SJ_E_OPEN && sj_test_calls == c0 && sw_path_exists(b));
    sj_test_shfo = NULL;
    sj_test_recycle = -1;
    printf("PASS delete U-14: success (FOF_ALLOWUNDO), error, abort after one item, recycle off, size mismatch, open .part: no call where refused\n");
    fflush(stdout);
}

/* U-18: the jobs thread (repair) starts after registration, never before. */
static void j3_startup_order(const uint8_t *buf, size_t n) {
    static j3_info_t info[512];
    unsigned nb = j3_walk(buf, n, info, 512u);
    char base[1024], game[1100], rec[1200], part[1400], fin[1400], why[SJ_MSG];
    LONG c0;
    unsigned waited = 0;
    size_t cut;
    CHECK(nb >= 4);
    /* gate2 (2026-10-13): stale index. 2.1 files start META (0), EVENT session_start (1), then DATA (2, 3, ...).
     * info[1] ends before any DATA block, so the cut must be after the second DATA block: info[3]. */
    CHECK(info[2].type == SV2_BLOCK_DATA && info[3].type == SV2_BLOCK_DATA);
    cut = info[3].end;   /* two complete DATA blocks: repairable */
    b2_base(base, sizeof base, "start");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_fmt(rec, sizeof rec, "%sR1", sw_rec_dir);
    b2_mkdir(rec);
    sw_fmt(part, sizeof part, "%s\\cut.f1t.part", rec);
    sw_fmt(fin, sizeof fin, "%s\\cut.f1t", rec);
    b2_write_file(part, buf, cut);
    sw_enabled = 1;
    memset(&sw_sm, 0, sizeof sw_sm);
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_wr.gap_reported = 0;
    sw_wk.have = 0;
    sw_wr.folder[0] = 0;
    CHECK(sw_ring_init());
    c0 = sj_start_count;
    CHECK(sw_path_exists(part) && !sw_path_exists(fin));   /* nothing repaired before registration */
    CHECK(sw_start() == 1);
    CHECK(sj_start_count == c0 + 1);
    /* gate2 (2026-10-13): stale race in the test. repair renames repair->fin first, then .part->.damaged
     * (storage_jobs.inc, MoveFileExA pair). Poll for the end state, not for the first rename. */
    while (!(sw_path_exists(fin) && !sw_path_exists(part)) && waited < 15000u) { Sleep(50); waited += 50; }
    CHECK(sw_path_exists(fin) && !sw_path_exists(part));
    CHECK(sw_stop(60000) == 1);
    printf("PASS startup order U-18: no repair before registration; repaired on the jobs thread after sw_start (%u ms)\n", waited);
    fflush(stdout);
}

/* Channel round trip (4.6, 4.7): player tread, gear, pch and AI tread/gear through encode, file and decode. */
static unsigned j3_rank(uint64_t mask, unsigned id) { return sv2_popcount64(mask & ((1ull << id) - 1ull)); }
#define J3_VAL(car, id, i) ((car)->vals[j3_rank((car)->mask_lo, (id)) * (car)->count + (i)])

static void j3_channel_round_trip(void) {
    double prod = 0.0, mx = 0.0;
    LONG dropped = 0;
    char src[1400];
    uint8_t *buf = NULL;
    size_t n;
    const sv2_car_t *p, *ai;
    g_telemetry.tread[0] = 0.5f; g_telemetry.tread[1] = 0.25f; g_telemetry.tread[2] = 0.75f; g_telemetry.tread[3] = 1.0f;
    g_telemetry.pch_valid = 1;
    g_telemetry.pch_ride_mm[0] = 12.5f;
    g_telemetry.pch_rpm = 7000.0f;
    g_telemetry.pch_bottom = 3;
    g_telemetry.pch_compound = 2;
    t3_run_writer("chan", 100u, 0u, 1, &prod, &mx, &dropped);
    sw_fmt(src, sizeof src, "%s%s\\%s", sw_rec_dir, sw_entries[0].folder, sw_entries[0].file);
    n = b2_read_file(src, &buf);
    CHECK(n > 0);
    t2_scan_data.pool = t2_spool; t2_scan_data.pool_cap = T2_POOL_WORDS;
    CHECK(sv2_scan(buf, n, &t2_scan_data, t2_work, sizeof t2_work, &t2_rep) == SV2_OK);
    p = &t2_scan_data.cars[0];
    ai = &t2_scan_data.cars[1];
    CHECK(p->slot == 0 && p->scope == 1 && ai->slot == 1 && ai->scope == 0);
    /* gate2 (2026-10-13): stale expectation. 2.1 (B1) adds channel ids 64..124 (sw_car_from), so the high mask
     * is set for cars on track. The old check (mask_hi == 0) predates B1. */
    CHECK(p->mask_hi != 0 && ai->mask_hi != 0);
    /* channels present for the player */
    CHECK(((p->mask_lo >> 11) & 0xFu) == 0xFu);            /* tread 11..14 */
    CHECK(p->mask_lo & (1ull << 16));                       /* gear */
    CHECK(!(p->mask_lo & (1ull << 19)));                    /* flags: no per-car source, not written */
    for (unsigned id = 24; id < 64; id++) CHECK(p->mask_lo & (1ull << id));   /* pch slots present */
    /* values: tread is Q16 (within 1 step), gear and pch integers are exact, pch floats are exact bits */
    CHECK(fabs(sv2_q16_decode((uint16_t)J3_VAL(p, 11, 0), 0.0f, 1.0f) - 0.5f) <= 1.0f / 65535.0f);
    CHECK(fabs(sv2_q16_decode((uint16_t)J3_VAL(p, 12, 0), 0.0f, 1.0f) - 0.25f) <= 1.0f / 65535.0f);
    CHECK(fabs(sv2_q16_decode((uint16_t)J3_VAL(p, 13, 0), 0.0f, 1.0f) - 0.75f) <= 1.0f / 65535.0f);
    CHECK((int32_t)J3_VAL(p, 16, 0) == 4);
    CHECK(J3_VAL(p, 24, 0) == sv2_f32_bits(12.5f));
    CHECK(J3_VAL(p, 63, 0) == sv2_f32_bits(7000.0f));
    CHECK((int32_t)J3_VAL(p, 60, 0) == 3);
    CHECK((int32_t)J3_VAL(p, 61, 0) == 2);
    /* AI car: tread from the wheel block (0.875 -> 0.5 normalized), gear, no pch, no player-only ids */
    CHECK(((ai->mask_lo >> 11) & 0xFu) == 0xFu && (ai->mask_lo & (1ull << 16)));
    CHECK(!(ai->mask_lo & (1ull << 24)) && !(ai->mask_lo & (1ull << 20)));
    CHECK(fabs(sv2_q16_decode((uint16_t)J3_VAL(ai, 11, 0), 0.0f, 1.0f) - 0.5f) <= 1.0f / 65535.0f);
    CHECK((int32_t)J3_VAL(ai, 16, 0) == 4);
    free(buf);
    g_telemetry.pch_valid = 0;
    printf("PASS channels batch 3: tread 11-14, gear 16, pch 24-63 (player) and AI tread/gear round trip through file and decoder; flags 19 not written\n");
    fflush(stdout);
}

static void test_storage_jobs(void) {
    double prod = 0.0, mx = 0.0;
    LONG dropped = 0;
    char src[1400], dir[1400];
    uint8_t *buf = NULL;
    size_t n;
    g_telemetry.tread[0] = 0.5f; g_telemetry.tread[1] = 0.25f; g_telemetry.tread[2] = 0.75f; g_telemetry.tread[3] = 1.0f;
    t3_run_writer("jobs", 200u, 0u, 1, &prod, &mx, &dropped);   /* paced: no ring drops, ~10 DATA blocks */
    sw_fmt(src, sizeof src, "%s%s\\%s", sw_rec_dir, sw_entries[0].folder, sw_entries[0].file);
    sw_fmt(dir, sizeof dir, "%s%s", sw_rec_dir, sw_entries[0].folder);
    n = b2_read_file(src, &buf);
    CHECK(n > 0);
    j3_repair_cases(buf, n, dir);
    j3_archive_cases(src, buf, n, dir);
    j3_delete_cases(dir);
    {   /* batch 6 job report: a recycle archive moves the working .f1t to the bin and the report names the .f1t;
         * a refused bin move leaves it in not_moved; the .f1a is never listed as moved. Uses its own folder, removed after. */
        char jdir[1400], jbin[1400], jw1[1400], ja1[1400], jw2[1400], ja2[1400], jitem[1400];
        const char *const jfold[2] = { "JobRpt", "JobRpt" };
        const char *const jfile[2] = { "JRPT_S1.f1t", "JRPT_S2.f1t" };
        unsigned jseq = 0;
        int jslot;
        sw_fmt(jdir, sizeof jdir, "%sJobRpt", sw_rec_dir);
        sw_fmt(jbin, sizeof jbin, "%sJobRptBin", sw_rec_dir);
        sw_fmt(jw1, sizeof jw1, "%s\\JRPT_S1.f1t", jdir);
        sw_fmt(ja1, sizeof ja1, "%s\\JRPT_S1.f1a", jdir);
        sw_fmt(jw2, sizeof jw2, "%s\\JRPT_S2.f1t", jdir);
        sw_fmt(ja2, sizeof ja2, "%s\\JRPT_S2.f1a", jdir);
        sw_fmt(jitem, sizeof jitem, "%s\\item0.bin", jbin);
        b2_mkdir(jdir);
        b2_mkdir(jbin);
        b2_write_file(jw1, buf, n);
        b2_write_file(jw2, buf, n);
        sj_test_recycle = 1;
        sj_test_shfo = sj_test_shfo_double;
        sw_fmt(sj_test_bin, sizeof sj_test_bin, "%s\\", jbin);
        sj_test_mode = 0;
        jslot = sj_submit(SJ_JOB_ARCHIVE, 1, jfold, jfile, 0.0, 1, &jseq);
        CHECK(jslot >= 0);
        if (jslot >= 0) {
            sj_run(jslot);
            CHECK(sj_jobs[jslot].state == 2 && sj_jobs[jslot].result == SJ_OK);
            CHECK(sj_jobs[jslot].nmoved == 1 && strcmp(sj_jobs[jslot].moved[0], "JRPT_S1.f1t") == 0);
            CHECK(sj_jobs[jslot].nleft == 0);
            CHECK(sw_path_exists(ja1) && !sw_path_exists(jw1) && sw_path_exists(jitem));
            sj_jobs[jslot].used = 0;
        }
        sj_test_mode = 1;   /* the bin move is refused: the working copy stays and is listed as not_moved */
        jslot = sj_submit(SJ_JOB_ARCHIVE, 1, jfold + 1, jfile + 1, 0.0, 1, &jseq);
        CHECK(jslot >= 0);
        if (jslot >= 0) {
            sj_run(jslot);
            CHECK(sj_jobs[jslot].result != SJ_OK);
            CHECK(sj_jobs[jslot].nmoved == 0 && sj_jobs[jslot].nleft == 1);
            CHECK(strcmp(sj_jobs[jslot].left[0], "JRPT_S2.f1t") == 0);
            CHECK(sw_path_exists(ja2) && sw_path_exists(jw2));
            sj_jobs[jslot].used = 0;
        }
        sj_test_mode = 0;
        DeleteFileA(ja1); DeleteFileA(ja2); DeleteFileA(jw2); DeleteFileA(jitem);
        RemoveDirectoryA(jbin); RemoveDirectoryA(jdir);
        sj_test_shfo = NULL;
        sj_test_recycle = -1;
        printf("PASS job report batch 6: recycle archive moved list names the .f1t (not the .f1a); refused bin move lists it under not_moved\n");
        fflush(stdout);
    }
    {   /* 7.6 load: selection only; unknown or unsafe names refused */
        const char *fname = strrchr(dir, '\\');
        CHECK(fname != NULL);
        CHECK(sj_load(fname + 1) == 1 && strcmp(sj_loaded, fname + 1) == 0);
        CHECK(sj_load("..\\x") == 0 && sj_load("no_such_recording") == 0);
    }
    {   /* U-16 (partial): the index carries per-recording and total sizes and the 1 GB flag (false here) */
        uint8_t *ib = NULL;
        size_t inn = b2_read_file(sw_index_path, &ib);
        CHECK(inn > 0);
        CHECK(strstr((const char *)ib, "\"bytes\":") != NULL);
        CHECK(strstr((const char *)ib, "\"warn_active\":false") != NULL);
        CHECK(strstr((const char *)ib, "\"warn_bytes\":1073741824") != NULL);
        free(ib);
    }
    j3_startup_order(buf, n);
    free(buf);
    j3_channel_round_trip();
    printf("PASS storage jobs batch 3: U-12, U-13, U-14, U-18, channel round trip\n");
    fflush(stdout);
}

/* ---------- batch 4: META block (spec 4.3 subset), recordings API (spec 8.1-8.3, 7.7, 7.8, U-16) ---------- */
static char b4_out[1u << 19];
static char b4_zero[512];

/* Occurrences of a text pattern in a binary buffer (bounded by n; NUL bytes are data here, so no strstr). */
static size_t b4_count(const uint8_t *b, size_t n, const char *pat) {
    size_t m = strlen(pat), c = 0;
    for (size_t i = 0; i + m <= n; i++) if (memcmp(b + i, pat, m) == 0) c++;
    return c;
}

static void b4_meta_check(void) {
    double prod = 0.0, mx = 0.0;
    LONG dropped = 0;
    uint8_t *buf = NULL;
    size_t n, found;
    char src[1400];
    t3_run_writer("b4meta", 60u, 0u, 1, &prod, &mx, &dropped);
    sw_fmt(src, sizeof src, "%s%s\\%s", sw_rec_dir, sw_entries[0].folder, sw_entries[0].file);
    n = b2_read_file(src, &buf);
    CHECK(n > 0);
    found = b4_count(buf, n, "\"schema\":\"f1c.telemetry.meta/2\"");
    CHECK(found == 2);   /* initial (after the channel table) and final (before END) */
    CHECK(b4_count(buf, n, "\"final\":false") == 1 && b4_count(buf, n, "\"final\":true") == 1);
    CHECK(b4_count(buf, n, "\"session_type_name\":") == 2 && b4_count(buf, n, "\"dropped_ticks\":") == 2);
    free(buf);
    printf("PASS META 4.3 subset: two f1c.telemetry.meta/2 blocks (initial and final), raw, readable\n");
    fflush(stdout);
}

static int b4_has(const char *s, const char *needle) { return s && strstr(s, needle) != NULL; }

static int b4_find_queued(void) {
    for (unsigned i = 0; i < SJ_QUEUE_N; i++) if (sj_jobs[i].used && sj_jobs[i].state == 0) return (int)i;
    return -1;
}

/* Runs the queued job on the test thread (the jobs thread is not started in this harness). */
static void b4_run_queued(void) {
    int k = b4_find_queued();
    CHECK(k >= 0);
    if (k >= 0) sj_run(k);
}

/* One route call: action and JSON body; the status must equal want. The body is copied to out. */
static void b4_api(const char *action, const char *body, int want, char *out, size_t cap) {
    size_t len = 0;
    int code = sa_api_action(action, body, strlen(body), out, cap, &len);
    if (code != want) printf("B4 %s %s -> %d (want %d): %.200s\n", action, body, code, want, out);
    CHECK(code == want);
    out[len < cap ? len : cap - 1] = 0;
}

static void b4_snapshot(char *out, size_t cap) {
    size_t len = 0;
    int code;
    sw_nentries = 0;
    sw_overflow_bytes = 0;
    sw_scan_folders();
    sa_snapshot_rebuild();
    code = sa_api_recordings(out, cap, &len);
    CHECK(code == 200);
    out[len < cap ? len : cap - 1] = 0;
}

static void b4_make(const char *folder, const char *name, size_t n) {
    char p[1400];
    sw_fmt(p, sizeof p, "%s%s\\%s", sw_rec_dir, folder, name);
    b2_write_file(p, b4_zero, n);
}

/* U-17 skeleton (spec 7.9, AC-10): the 1.0.1 import is not built in batch 4. What is checked here: the storage scan,
 * the snapshot, the load and the job paths never change a 1.0.1 telemetry_data folder (source bytes unchanged). */
static void b4_u17_skeleton(const char *game) {
    char leg[1400], meta[1500], trc[1500];
    uint8_t *m0 = NULL, *t0 = NULL, *m1 = NULL, *t1 = NULL;
    size_t mn0, tn0, mn1, tn1;
    char z96[96 * 3];
    memset(z96, 7, sizeof z96);
    sw_fmt(leg, sizeof leg, "%stelemetry_data", game);
    b2_mkdir(leg);
    sw_fmt(leg, sizeof leg, "%stelemetry_data\\20261012-213454_s5_g12", game);
    b2_mkdir(leg);
    sw_fmt(meta, sizeof meta, "%s\\meta.json", leg);
    sw_fmt(trc, sizeof trc, "%s\\traces.bin", leg);
    b2_write_file(meta, "{\"ty\":5,\"laps\":1}", 17);
    b2_write_file(trc, z96, sizeof z96);
    mn0 = b2_read_file(meta, &m0);
    tn0 = b2_read_file(trc, &t0);
    CHECK(mn0 == 17 && tn0 == sizeof z96);
    b4_snapshot(b4_out, sizeof b4_out);
    b4_api("load", "{\"recording\":\"Rec_A\"}", 200, b4_out, sizeof b4_out);
    b4_api("archive", "{\"sessions\":[\"Rec_A/NOPE\"]}", 404, b4_out, sizeof b4_out);
    sw_nentries = 0;
    sw_scan_folders();
    sa_snapshot_rebuild();
    b4_api("load", "{\"recording\":\"\"}", 200, b4_out, sizeof b4_out);
    mn1 = b2_read_file(meta, &m1);
    tn1 = b2_read_file(trc, &t1);
    CHECK(mn1 == mn0 && tn1 == tn0 && m1 && t1 && memcmp(m0, m1, mn0) == 0 && memcmp(t0, t1, tn0) == 0);
    free(m0); free(t0); free(m1); free(t1);
    printf("PASS U-17 skeleton (7.9 import not built): the 1.0.1 telemetry_data folder is byte-unchanged by scan, snapshot, load and archive refusal\n");
    fflush(stdout);
}

static void test_recordings_api(void) {
    char base[1024], game[1100], why[SJ_MSG], p[1400], q[1400];
    LONG c0;
    b4_meta_check();

    /* 8.1 request parser: flat objects only, strings without escapes, bounded lists */
#define B4P(s) sa_parse((s), strlen(s))
    CHECK(B4P("{\"recording\":\"Rec_A\"}") == 1 && sa_req.type[0] == 1 && strcmp(sa_req.str[0], "Rec_A") == 0);
    CHECK(B4P("{\"sessions\":[\"a/b\",\"c/d\"],\"recycle_working\":true}") == 1 && sa_req.type[0] == 4 &&
          sa_req.cnt[0] == 2 && strcmp(sa_req.list[0][1], "c/d") == 0 && sa_req.type[1] == 3 && sa_req.flag[1] == 1);
    CHECK(B4P("{\"target\":\"file\",\"id\":\"x\",\"confirm_bytes\":12}") == 1 && sa_req.type[2] == 2 && sa_req.num[2] == 12.0);
    CHECK(B4P("{\"sessions\":[]}") == 1 && sa_req.cnt[0] == 0);
    CHECK(B4P("") == 0 && B4P("[]") == 0 && B4P("{\"a\":\"x\\\"y\"}") == 0 && B4P("{\"a\":1,}") == 0);
    CHECK(B4P("{\"a\":12345678901234567890}") == 0 && B4P("{\"a\":1} x") == 0 && B4P("{\"a\":\"\xC3\xA9\"}") == 0);
    CHECK(B4P("{\"a\":[\"x\",]}") == 0 && B4P("{\"a\":[\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\",\"8\",\"9\"]}") == 0);
#undef B4P
    printf("PASS request parser batch 4: objects, lists and booleans accepted; escapes, trailing data, non-ASCII, long numbers and 9-item lists refused\n");
    fflush(stdout);

    /* a fresh recordings folder with known sizes: Rec_A 240 bytes, Rec_B 70 bytes, total 310 */
    sj_loaded[0] = 0;   /* earlier tests may leave a loaded name */
    b2_base(base, sizeof base, "b4api");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_enabled = 1;
    sa_init();
    sw_fmt(p, sizeof p, "%sRec_A", sw_rec_dir); b2_mkdir(p);
    sw_fmt(p, sizeof p, "%sRec_B", sw_rec_dir); b2_mkdir(p);
    b4_make("Rec_A", "S1.f1t", 100);
    b4_make("Rec_A", "S1.f1a", 40);
    b4_make("Rec_A", "S2.f1t.part.damaged", 30);
    b4_make("Rec_A", "S3.f1a.partial", 20);
    b4_make("Rec_A", "S4.f1t", 50);
    b4_make("Rec_B", "S5.f1t", 70);

    /* 8.3 GET /recordings: sizes, state, damaged and partial lists, no absolute path (M11) */
    b4_snapshot(b4_out, sizeof b4_out);
        CHECK(b4_has(b4_out, "\"schema\":\"f1c.recordings/1\"") && b4_has(b4_out, "\"total_bytes\":310"));
    CHECK(b4_has(b4_out, "\"over_warn\":false") && b4_has(b4_out, "\"warning\":null") && b4_has(b4_out, "\"loaded\":null"));
    CHECK(b4_has(b4_out, "Total size now: 310 B"));
    CHECK(b4_has(b4_out, "\"id\":\"Rec_A\"") && b4_has(b4_out, "\"bytes\":240,\"sessions_count\":2,\"archived_count\":1"));
    CHECK(b4_has(b4_out, "\"id\":\"Rec_B\"") && b4_has(b4_out, "\"bytes\":70,\"sessions_count\":1,\"archived_count\":0"));
    CHECK(b4_has(b4_out, "\"archived\":true") && b4_has(b4_out, "\"format\":\"archived\",\"bytes\":40"));
    CHECK(b4_has(b4_out, "\"format\":\"damaged\"") && b4_has(b4_out, "\"format\":\"archive_interrupted\""));
    CHECK(!b4_has(b4_out, ":\\"));   /* no drive path in any response */
    CHECK(sw_entries[0].bytes > 0.0);
    printf("PASS GET /recordings snapshot batch 4: sizes per session and recording, total 310 B, damaged and partial lists, no absolute path\n");
    fflush(stdout);

    /* 8.2 POST /recordings/load: selection only, unknown and unsafe names refused */
    b4_api("load", "{\"recording\":\"Rec_A\"}", 200, b4_out, sizeof b4_out);
    CHECK(b4_has(b4_out, "\"sessions\":2") && b4_has(b4_out, "Loading does not change any file."));
    b4_snapshot(b4_out, sizeof b4_out);
    CHECK(b4_has(b4_out, "\"loaded\":\"Rec_A\""));
    b4_api("load", "{\"recording\":\"Nope\"}", 404, b4_out, sizeof b4_out);
    b4_api("load", "{\"recording\":\"..x\"}", 422, b4_out, sizeof b4_out);
    b4_api("load", "{\"recording\":3}", 400, b4_out, sizeof b4_out);
    printf("PASS POST /recordings/load: loaded, unknown 404, unsafe name 422, wrong type 400\n");
    fflush(stdout);

    /* 7.7 archive refusals: LZMS off (503, nothing changed), no working copy (404), bad ids (422, 400) */
    sv2_test_cabinet_forced_off = 1;
    b4_api("archive", "{\"sessions\":[\"Rec_A/S4\"]}", 503, b4_out, sizeof b4_out);
    sv2_test_cabinet_forced_off = 0;
    b4_api("archive", "{\"sessions\":[\"Rec_A/S2\"]}", 404, b4_out, sizeof b4_out);
    b4_api("archive", "{\"sessions\":[\"Rec_A\"]}", 422, b4_out, sizeof b4_out);
    b4_api("archive", "{\"sessions\":[]}", 422, b4_out, sizeof b4_out);
    b4_api("archive", "{\"sessions\":[\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\",\"Rec_A/S4\"]}", 400, b4_out, sizeof b4_out);
    sj_test_recycle = 0;
    b4_api("archive", "{\"sessions\":[\"Rec_A/S4\"],\"recycle_working\":true}", 503, b4_out, sizeof b4_out);
    sj_test_recycle = 1;
    /* a valid request is queued (202); the job refuses the fake working copy (not a verifiable v2 file) */
    b4_api("archive", "{\"sessions\":[\"Rec_A/S4\"],\"recycle_working\":false}", 202, b4_out, sizeof b4_out);
    CHECK(b4_has(b4_out, "\"job\":\"archive-"));
    b4_run_queued();
    sw_fmt(q, sizeof q, "%sRec_A\\S4.f1t", sw_rec_dir);
    CHECK(sw_path_exists(q));
    sw_fmt(q, sizeof q, "%sRec_A\\S4.f1a", sw_rec_dir);
    CHECK(!sw_path_exists(q));
    {
        size_t len = 0;
        CHECK(sa_api_jobs(b4_out, sizeof b4_out, &len) == 200);
        CHECK(b4_has(b4_out, "\"kind\":\"archive\"") && b4_has(b4_out, "\"result\":\"error\"") && b4_has(b4_out, "\"not_moved\":[]"));
    }
    printf("PASS POST /recordings/archive: LZMS off 503, no working copy 404, bad ids 422/400, recycle off 503; job refuses an unverifiable working copy and changes nothing\n");
    fflush(stdout);

    /* 7.8 delete refusals: loaded (409), stale size (409), unknown (404), missing recording (400), bad target (422), Recycle Bin off (503) */
    b4_api("delete", "{\"target\":\"recording\",\"id\":\"Rec_A\",\"confirm_bytes\":240}", 409, b4_out, sizeof b4_out);
    b4_api("load", "{\"recording\":\"\"}", 200, b4_out, sizeof b4_out);
    b4_api("delete", "{\"target\":\"recording\",\"id\":\"Rec_A\",\"confirm_bytes\":999}", 409, b4_out, sizeof b4_out);
    b4_api("delete", "{\"target\":\"recording\",\"id\":\"Nope\",\"confirm_bytes\":1}", 404, b4_out, sizeof b4_out);
    b4_api("delete", "{\"target\":\"session\",\"id\":\"S4\",\"confirm_bytes\":50}", 400, b4_out, sizeof b4_out);
    b4_api("delete", "{\"target\":\"bogus\",\"id\":\"x\",\"confirm_bytes\":1}", 422, b4_out, sizeof b4_out);
    sj_test_recycle = 0;
    b4_api("delete", "{\"target\":\"file\",\"recording\":\"Rec_B\",\"id\":\"S5.f1t\",\"confirm_bytes\":70}", 503, b4_out, sizeof b4_out);
    sj_test_recycle = 1;
    sw_fmt(q, sizeof q, "%sRec_B\\S5.f1t", sw_rec_dir);
    CHECK(sw_path_exists(q));

    /* success: the whole recording (files, then its folder) goes through one job; SHFileOperationW double, FOF_ALLOWUNDO */
    sw_fmt(p, sizeof p, "%sbin4\\", base);
    b2_mkdir(p);
    sw_fmt(sj_test_bin, sizeof sj_test_bin, "%s", p);
    sj_test_shfo = sj_test_shfo_double;
    sj_test_mode = 0;
    c0 = (LONG)sj_test_calls;
    b4_api("delete", "{\"target\":\"recording\",\"id\":\"Rec_B\",\"confirm_bytes\":70}", 202, b4_out, sizeof b4_out);
    CHECK(b4_has(b4_out, "\"job\":\"delete-"));
    b4_run_queued();
    CHECK(sj_test_calls == c0 + 1);
    CHECK((sj_test_flags & FOF_ALLOWUNDO) != 0);
    sw_fmt(q, sizeof q, "%sRec_B\\S5.f1t", sw_rec_dir);
    CHECK(!sw_path_exists(q));
    sw_fmt(q, sizeof q, "%sRec_B", sw_rec_dir);
    CHECK(!sw_is_dir(q));
    {
        size_t len = 0;
        CHECK(sa_api_jobs(b4_out, sizeof b4_out, &len) == 200);
        CHECK(b4_has(b4_out, "\"kind\":\"delete\"") && b4_has(b4_out, "\"result\":\"ok\"") && b4_has(b4_out, "\"moved\":[\"S5.f1t\",\"Rec_B\"]"));
        CHECK(b4_has(b4_out, "\"recycle_working\":false") && b4_has(b4_out, "\"bytes_moved\":70"));
    }
    sj_test_shfo = NULL;
    sj_test_recycle = -1;
    b4_snapshot(b4_out, sizeof b4_out);
    CHECK(!b4_has(b4_out, "\"id\":\"Rec_B\"") && b4_has(b4_out, "\"total_bytes\":240"));
    printf("PASS POST /recordings/delete: loaded 409, stale size 409, unknown 404, missing recording 400, bad target 422, Recycle Bin off 503; recording delete job through the double moves files then folder with FOF_ALLOWUNDO\n");
    fflush(stdout);

    sv2_test_cabinet_forced_off = 0;
    b4_u17_skeleton(game);
    printf("PASS recordings API batch 4: U-16 sizes, guards on the request layer, job results\n");
    fflush(stdout);
}

/* Security review 2026-10-13 (M1, M3, M4): bind policy, Host allowlist (port-exact, one Host header) and the
 * proxy-header rule for local-only actions. Offline: no socket is opened, no game is needed. */
static void test_bind_policy(void) {
    char ini[MAX_PATH + 64], tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof tmp, tmp);
    ULONG a = 0;
    CHECK(n > 0 && n < (DWORD)sizeof tmp && tmp[n - 1] == '\\');
    snprintf(ini, sizeof ini, "%sf1c_bind_policy_test.ini", tmp);
    /* M1 address policy: private LAN, Tailscale CGNAT, Hamachi and loopback pass; 0.0.0.0, public and malformed text do not. */
    CHECK(http_bind_private_ipv4("192.168.10.20", &a) && a == htonl(0xC0A80A14u));
    CHECK(http_bind_private_ipv4("100.101.102.103", &a) && a == htonl(0x64656667u));
    CHECK(http_bind_private_ipv4("25.1.2.3", &a));
    CHECK(http_bind_private_ipv4("10.0.0.1", &a));
    CHECK(http_bind_private_ipv4("172.16.0.1", &a) && http_bind_private_ipv4("172.31.255.1", &a));
    CHECK(!http_bind_private_ipv4("172.32.0.1", &a) && !http_bind_private_ipv4("172.15.0.1", &a));
    CHECK(!http_bind_private_ipv4("100.63.0.1", &a) && !http_bind_private_ipv4("100.128.0.1", &a));
    CHECK(!http_bind_private_ipv4("0.0.0.0", &a));
    CHECK(!http_bind_private_ipv4("8.8.8.8", &a));
    CHECK(!http_bind_private_ipv4("192.168.1.256", &a));
    CHECK(!http_bind_private_ipv4("192.168.1", &a));
    CHECK(!http_bind_private_ipv4("192.168.1.5x", &a));
    CHECK(!http_bind_private_ipv4("192.168.0001.5", &a));
    CHECK(!http_bind_private_ipv4("", &a));
    /* Settings and resolution: Remote=0 ignores BindAddress; Remote=1 needs a valid private address, else loopback. */
    write_text_file(ini, "[Mod]\nRemote=0\nBindAddress=192.168.1.5\n");
    g_bind_cfg[0] = 0; settings_read_ini(ini);
    CHECK(g_remote_access == 0 && strcmp(g_bind_cfg, "192.168.1.5") == 0);
    http_bind_resolve();
    CHECK(g_bind_s_addr == 0 && strcmp(g_bind_text, "127.0.0.1") == 0);
    write_text_file(ini, "[Mod]\nRemote=1\n");
    settings_read_ini(ini); http_bind_resolve();
    CHECK(g_remote_access == 1 && g_bind_cfg[0] == 0 && g_bind_s_addr == 0 && strcmp(g_bind_text, "127.0.0.1") == 0);
    write_text_file(ini, "[Mod]\nRemote=1\nBindAddress=8.8.8.8\n");
    settings_read_ini(ini); http_bind_resolve();
    CHECK(g_bind_s_addr == 0 && strcmp(g_bind_text, "127.0.0.1") == 0);
    write_text_file(ini, "[Mod]\nRemote=1\nBindAddress=0.0.0.0\n");
    settings_read_ini(ini); http_bind_resolve();
    CHECK(g_bind_s_addr == 0 && strcmp(g_bind_text, "127.0.0.1") == 0);
    write_text_file(ini, "[Mod]\nRemote=1\nBindAddress=192.168.10.20 ; lan\n");
    settings_read_ini(ini); http_bind_resolve();
    CHECK(g_bind_s_addr == htonl(0xC0A80A14u) && strcmp(g_bind_text, "192.168.10.20") == 0);
    write_text_file(ini, "[Mod]\nRemote=1\n;BindAddress=100.101.102.103\n");
    settings_read_ini(ini); http_bind_resolve();
    CHECK(g_bind_s_addr == 0 && strcmp(g_bind_text, "127.0.0.1") == 0);
    remove(ini);
    /* M3 Host allowlist: localhost, 127.0.0.1 or [::1] with exactly the bound port; one Host header; the bind address only when configured. */
    g_http_bound_port = 8080;
    CHECK(dcam_host_ok("GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n"));
    CHECK(dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"));
    CHECK(dcam_host_ok("GET / HTTP/1.1\r\nHost: [::1]:8080\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost:8081\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost:080\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: 127.0.0.1:80800\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: evil.example:8080\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost:8080\r\nHost: localhost:8080\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.0\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: 100.101.102.103:8080\r\n\r\n"));
    strcpy(g_bind_text, "100.101.102.103");
    g_bind_s_addr = htonl(0x64656667u);
    CHECK(dcam_host_ok("GET / HTTP/1.1\r\nHost: 100.101.102.103:8080\r\n\r\n"));
    CHECK(!dcam_host_ok("GET / HTTP/1.1\r\nHost: 100.101.102.104:8080\r\n\r\n"));
    CHECK(dcam_host_ok("GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"));
    g_bind_s_addr = 0; strcpy(g_bind_text, "127.0.0.1");
    g_http_bound_port = 0;
    /* M4: forwarded requests are not local, whatever the peer. */
    CHECK(http_has_proxy_header("GET / HTTP/1.1\r\nHost: x\r\nX-Forwarded-For: 203.0.113.9\r\n\r\n"));
    CHECK(http_has_proxy_header("GET / HTTP/1.1\r\nhost: x\r\nvia: 1.1 proxy\r\n\r\n"));
    CHECK(http_has_proxy_header("GET / HTTP/1.1\r\nHost: x\r\nForwarded: for=1.2.3.4\r\n\r\n"));
    CHECK(http_has_proxy_header("GET / HTTP/1.1\r\nHost: x\r\nCF-Connecting-IP: 198.51.100.7\r\n\r\n"));
    CHECK(!http_has_proxy_header("GET / HTTP/1.1\r\nHost: x\r\nX-Forwarded-Foo: 1\r\n\r\n"));
    CHECK(!http_has_proxy_header("GET / HTTP/1.1\r\nHost: x\r\nX-Note: via the proxy\r\n\r\n"));
    CHECK(!http_has_proxy_header("GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n"));
    printf("PASS bind policy, Host allowlist and proxy-header checks\n");
}

/* M6 (security review 2026-10-13): send_complete must deliver a large response to a slow reader. The
 * old 200 ms total budget cut it off. A real loopback pair: the reader takes 4 KB every 10 ms (about 1.3 s
 * for 512 KB), so the sender needs far longer than 200 ms and must keep going while bytes move. */
typedef struct SlowReadArg { SOCKET s; size_t got; size_t want; } SlowReadArg;
static DWORD WINAPI slow_reader_thread(LPVOID param) {
    SlowReadArg *a = (SlowReadArg *)param;
    static char rb[4096];
    while (a->got < a->want) {
        int n = recv(a->s, rb, (int)sizeof rb, 0);
        if (n <= 0) break;
        a->got += (size_t)n;
        Sleep(10);
    }
    return 0;
}
static void test_send_progress(void) {
    WSADATA wsa;
    SOCKET ls, cs, as;
    struct sockaddr_in addr;
    int al = sizeof addr, rcv = 4096, ok;
    static char payload[512 * 1024];
    SlowReadArg arg;
    HANDLE th;
    size_t i;
    CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(ls != INVALID_SOCKET && bind(ls, (struct sockaddr *)&addr, sizeof addr) == 0);
    CHECK(listen(ls, 1) == 0);
    CHECK(getsockname(ls, (struct sockaddr *)&addr, &al) == 0);
    cs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(cs, SOL_SOCKET, SO_RCVBUF, (const char *)&rcv, sizeof rcv);
    CHECK(connect(cs, (struct sockaddr *)&addr, sizeof addr) == 0);
    as = accept(ls, NULL, NULL);
    CHECK(as != INVALID_SOCKET);
    {
        int sb = 4096;   /* small send buffer: the transfer must follow the reader, as on a slow link */
        u_long mode = 1;
        setsockopt(as, SOL_SOCKET, SO_SNDBUF, (const char *)&sb, sizeof sb);
        ioctlsocket(as, FIONBIO, &mode);
    }
    for (i = 0; i < sizeof payload; i++) payload[i] = (char)('a' + i % 26);
    arg.s = cs; arg.got = 0; arg.want = sizeof payload;
    th = CreateThread(NULL, 0, slow_reader_thread, &arg, 0, NULL);
    CHECK(th != NULL);
    ok = send_complete(as, payload, (int)sizeof payload);
    WaitForSingleObject(th, 30000);
    CloseHandle(th);
    CHECK(ok == 1 && arg.got == sizeof payload);
    closesocket(as); closesocket(cs); closesocket(ls);
    WSACleanup();
    printf("PASS M6 send path: 512 KB to a slow reader (4 KB per 10 ms) delivered in full\n");
}

/* TV Director phase 1 (2026-10-13): offline checks of the pure parts only (body parsing, angle keys, the one-slot queue,
 * the Host allowlist, the default-off write gate, the car-switch block). Game-thread writes need a live game and are
 * covered by owner tests T4-T7 (TV_DIRECTOR_SPEC section 7). Compiled, not run, in the phase 1 task. */
static void test_director_phase1(void)
{
    char v[64], buf[1024];
    int mode = -1;
    uint32_t index = 99, seq = 0;
    DcamCmd c;
    DcamResult r;
    const char *body = "{\"driver\":\"HAKKINEN\",\"angle\":\"ONBOARD:ONBOARD002\"}";
    CHECK(dcam_json_string(body, "driver", v, sizeof v) == 1);
    CHECK(strcmp(v, "HAKKINEN") == 0);
    CHECK(dcam_json_string(body, "angle", v, sizeof v) == 1);
    CHECK(strcmp(v, "ONBOARD:ONBOARD002") == 0);
    CHECK(dcam_json_string("{\"driver\":\"A\\\"B\"}", "driver", v, sizeof v) == 0);
    CHECK(dcam_json_string("{\"driver\":\"\"}", "driver", v, sizeof v) == 0);
    CHECK(dcam_json_string(body, "missing", v, sizeof v) == 0);
    CHECK(dcam_json_string("{\"driver\":\"ABCDEFGH\"}", "driver", v, 4) == 0);
    g_dcam_onboard_n = 2;
    strcpy(g_dcam_onboard[0], "CAM_A");
    strcpy(g_dcam_onboard[1], "CAM_B");
    CHECK(dcam_angle_key("NOSECAM", &mode, &index) && mode == 0 && index == 0);
    CHECK(dcam_angle_key("SWINGMAN", &mode, &index) && mode == 3);
    CHECK(dcam_angle_key("ONBOARD:CAM_B", &mode, &index) && mode == DCAM_MODE_ONBOARD && index == 1);
    CHECK(!dcam_angle_key("ONBOARD:NOPE", &mode, &index));
    CHECK(!dcam_angle_key("CAM_B", &mode, &index));
    CHECK(g_dcam_enabled == 0);
    CHECK(g_dcam_carswitch_enabled == 0);
    memset(&c, 0, sizeof c);
    c.kind = DCAM_CMD_ANGLE;
    c.mode = 2;
    CHECK(dcam_cmd_submit(&c, &seq) == 1 && seq == 1);
    CHECK(dcam_cmd_submit(&c, &seq) == 0);
    dcam_cmd_abandon(seq, "test");
    CHECK(dcam_cmd_submit(&c, &seq) == 1 && seq == 2);
    dcam_cmd_abandon(seq, "test");
    memset(&r, 0, sizeof r);
    r.seq = 5;
    r.done = 1;
    dcam_res(&r, "refused", "car switch blocked");
    dcam_cmd_json(buf, sizeof buf, "cut", &r);
    CHECK(strstr(buf, "\"status\":\"refused\"") != NULL && strstr(buf, "\"command\":\"cut\"") != NULL);
    g_http_bound_port = 8080;
    CHECK(dcam_host_ok("POST /director/cut HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"));
    CHECK(!dcam_host_ok("POST /director/cut HTTP/1.1\r\nHost: evil.example\r\n\r\n"));
    CHECK(!dcam_host_ok("POST /director/cut HTTP/1.1\r\n\r\n"));
    g_http_bound_port = 0;
    printf("PASS TV Director phase 1 offline checks\n");
}

/* TV Director camera change (2026-10-14, final design): offline checks of the pure parts only. The reviewed prologue of
 * GameStateDispatchChain 0x40D240 (exact bytes accepted, every one-byte change refused), the player table pick, the race position
 * pick (unique, none, two cars, out of range, dead slots ignored), the position value from the body, the gate reasons, the give-up
 * reason, plan order, status combination, next-frame verify, result JSON, the default gate. No game memory is read or written.
 * Live checks: TV_DIRECTOR_KEYPATH_RECORD_2026-10-14.md. */
static void test_director_phase2(void)
{
    uint8_t good[DCAM_CHAIN_LEN], bad[DCAM_CHAIN_LEN];
    int32_t pos[VEH_SLOTS], act[VEH_SLOTS];
    uint32_t slot = 99, veh = 0;
    int steps[2], n = 0, i, j;
    const char *why = NULL, *g;
    const char *rs[6];
    int flags[6];
    DcamResult r;
    char buf[1024], reason[DCAM_RESULT_REASON];
    static const char give_up_prefix[] = "the game did not move the view to that car";

    /* the reviewed prologue: exact bytes accepted, every one-byte change refused; addresses as recorded */
    CHECK(DCAM_CHAIN_ADDR == 0x0040D240u && DCAM_CHAIN_LEN == 40u);
    CHECK(DCAM_PLAYER_SLOT_ADDR == 0x007C54E4u && DCAM_PTR_TABLE_ADDR == 0x007C54F0u);
    CHECK(DCAM_VIEW_TARGET_ADDR == 0x007C54E0u && DCAM_VIEW_INDEX_ADDR == 0x007C54DCu);
    CHECK(DCAM_CHAIN_PROLOGUE[0] == 0x56 && DCAM_CHAIN_PROLOGUE[14] == 0xE8 && DCAM_CHAIN_PROLOGUE[39] == 0xFF);
    memcpy(good, DCAM_CHAIN_PROLOGUE, sizeof good);
    CHECK(dcam_chain_prologue_ok(good));
    for (i = 0; i < (int)DCAM_CHAIN_LEN; ++i) {
        memcpy(bad, good, sizeof bad);
        bad[i] ^= 0x01;
        CHECK(!dcam_chain_prologue_ok(bad));
    }

    /* the player: the table entry must be a pool vehicle and the slot must be inside the table */
    CHECK(dcam_player_pick(0, VEH_BASE, &veh) && veh == VEH_BASE);
    CHECK(dcam_player_pick(5, VEH_BASE + 5u * VEH_STRIDE, &veh) && veh == VEH_BASE + 5u * VEH_STRIDE);
    CHECK(!dcam_player_pick(VEH_SLOTS, VEH_BASE, &veh));
    CHECK(!dcam_player_pick(0, 0u, &veh));
    CHECK(!dcam_player_pick(0, VEH_BASE + 1u, &veh));

    /* race position pick: one live car at the position; none; two live cars; out of range; dead slots and zero positions ignored */
    for (i = 0; i < VEH_SLOTS; ++i) { pos[i] = 0; act[i] = -1; }
    pos[0] = 1; act[0] = 0;
    pos[7] = 2; act[7] = 0;
    pos[8] = 3; act[8] = 0;
    pos[9] = 3; act[9] = -1;   /* dead car showing position 3: ignored */
    CHECK(dcam_pick_position(pos, act, VEH_SLOTS, 1, &slot, &why) && slot == 0);
    CHECK(dcam_pick_position(pos, act, VEH_SLOTS, 2, &slot, &why) && slot == 7);
    CHECK(dcam_pick_position(pos, act, VEH_SLOTS, 3, &slot, &why) && slot == 8);
    CHECK(!dcam_pick_position(pos, act, VEH_SLOTS, 4, &slot, &why) && why != NULL && strstr(why, "no live car") != NULL);
    pos[10] = 2; act[10] = 0;
    CHECK(!dcam_pick_position(pos, act, VEH_SLOTS, 2, &slot, &why) && why != NULL && strstr(why, "more than one") != NULL);
    pos[10] = 0; act[10] = 0;
    CHECK(!dcam_pick_position(pos, act, VEH_SLOTS, 0, &slot, &why) && why != NULL && strstr(why, "out of range") != NULL);
    CHECK(!dcam_pick_position(pos, act, VEH_SLOTS, VEH_SLOTS + 1, &slot, &why) && why != NULL && strstr(why, "out of range") != NULL);
    CHECK(!dcam_pick_position(pos, act, VEH_SLOTS, -3, &slot, &why));

    /* the position value of a body: whole numbers only; absent or not a number -> 0; large values capped (refused as out of range) */
    CHECK(dcam_json_int("{\"position\": 3, \"angle\":\"COCKPIT\"}", "position", &n) && n == 3);
    CHECK(dcam_json_int("{\"position\":12}", "position", &n) && n == 12);
    CHECK(!dcam_json_int("{\"driver\":\"X\"}", "position", &n));
    CHECK(!dcam_json_int("{\"position\":\"3\"}", "position", &n));
    CHECK(!dcam_json_int("{\"position\":-2}", "position", &n));
    CHECK(dcam_json_int("{\"position\":999999999}", "position", &n) && n == 100000);

    /* gate: all pass -> NULL; each single failure -> its own non-empty reason shorter than the result field */
    CHECK(dcam_chain_gate(1, 1, 1, 1, 1, 1) == NULL);
    for (j = 0; j < 6; ++j) {
        for (i = 0; i < 6; ++i) flags[i] = 1;
        flags[j] = 0;
        rs[j] = dcam_chain_gate(flags[0], flags[1], flags[2], flags[3], flags[4], flags[5]);
        CHECK(rs[j] != NULL && rs[j][0] != 0 && strlen(rs[j]) < DCAM_RESULT_REASON);
    }
    CHECK(rs[0] && strstr(rs[0], "CarSwitch=0") != NULL);
    CHECK(rs[5] && strstr(rs[5], "0x40D240") != NULL);
    for (j = 0; j < 6; ++j)
        for (i = j + 1; i < 6; ++i)
            CHECK(rs[j] && rs[i] && strcmp(rs[j], rs[i]) != 0);

    /* watch: one decision per game frame; reached wins; give-up after DCAM_WATCH_MAX_STEPS; the reason names the diagnostics */
    CHECK(DCAM_WATCH_MAX_STEPS == 10);
    CHECK(dcam_watch_decide(0, 1, DCAM_WATCH_MAX_STEPS) == DCAM_WATCH_CONTINUE);
    CHECK(dcam_watch_decide(0, 9, DCAM_WATCH_MAX_STEPS) == DCAM_WATCH_CONTINUE);
    CHECK(dcam_watch_decide(0, 10, DCAM_WATCH_MAX_STEPS) == DCAM_WATCH_GIVE_UP);
    CHECK(dcam_watch_decide(1, 1, DCAM_WATCH_MAX_STEPS) == DCAM_WATCH_REACHED);
    CHECK(dcam_watch_decide(1, 10, DCAM_WATCH_MAX_STEPS) == DCAM_WATCH_REACHED);
    dcam_watch_reason(reason, sizeof reason, 10, 7u, 0u, 0, 1u);
    CHECK(strncmp(reason, give_up_prefix, sizeof give_up_prefix - 1) == 0);
    CHECK(strstr(reason, "0x7C54DC=7") != NULL && strstr(reason, "0x7C54E0=0x00000000") != NULL);
    CHECK(strstr(reason, "nothing else was written") != NULL && strlen(reason) < DCAM_RESULT_REASON);

    /* manager states that hand the view to a circuit camera (diagnostic only since 2026-10-14) */
    CHECK(!dcam_manager_state_ok(-1, 0));
    CHECK(!dcam_manager_state_ok(-1, 1));
    CHECK(!dcam_manager_state_ok(1, 1));
    CHECK(!dcam_manager_state_ok(2, 0));
    CHECK(dcam_manager_state_ok(2, 1));
    CHECK(dcam_manager_state_ok(0, 0));
    CHECK(dcam_manager_state_ok(3, 0));

    /* plan order: CAR then ANGLE for CUT and RESTORE; ANGLE only when the car is already on screen or for ANGLE */
    n = dcam_plan_steps(DCAM_CMD_CUT, 0, steps);
    CHECK(n == 2 && steps[0] == DCAM_STEP_CAR && steps[1] == DCAM_STEP_ANGLE);
    n = dcam_plan_steps(DCAM_CMD_CUT, 1, steps);
    CHECK(n == 1 && steps[0] == DCAM_STEP_ANGLE);
    n = dcam_plan_steps(DCAM_CMD_ANGLE, 0, steps);
    CHECK(n == 1 && steps[0] == DCAM_STEP_ANGLE);
    n = dcam_plan_steps(DCAM_CMD_RESTORE, 0, steps);
    CHECK(n == 2 && steps[0] == DCAM_STEP_CAR && steps[1] == DCAM_STEP_ANGLE);
    n = dcam_plan_steps(DCAM_CMD_RESTORE, 1, steps);
    CHECK(n == 1 && steps[0] == DCAM_STEP_ANGLE);

    /* status: applied only when the angle read back; partial when the car step ran and the angle did not */
    CHECK(strcmp(dcam_combine_status(1, "applied"), "applied") == 0);
    CHECK(strcmp(dcam_combine_status(1, "redirected"), "partial") == 0);
    CHECK(strcmp(dcam_combine_status(0, "applied"), "applied") == 0);
    CHECK(strcmp(dcam_combine_status(0, "refused"), "refused") == 0);

    /* next-frame verify: mode (and index for ONBOARD) and, for car changes, the followed car */
    CHECK(dcam_verify_match(1, 0, 0, VEH_BASE, 0, 0, 1, VEH_BASE));
    CHECK(!dcam_verify_match(1, 0, 0, VEH_BASE, 0, 0, 0, 0));
    CHECK(!dcam_verify_match(1, 0, 0, VEH_BASE, 1, 0, 1, VEH_BASE));
    CHECK(!dcam_verify_match(1, 0, 0, VEH_BASE, 0, 0, 1, VEH_BASE + VEH_STRIDE));
    CHECK(dcam_verify_match(0, 2, 0, 0, 2, 0, 0, 0));
    CHECK(!dcam_verify_match(0, DCAM_MODE_ONBOARD, 1, 0, DCAM_MODE_ONBOARD, 2, 0, 0));
    CHECK(strcmp(dcam_verify_token(DCAM_VERIFY_PENDING), "\"pending\"") == 0);
    CHECK(strcmp(dcam_verify_token(DCAM_VERIFY_TRUE), "true") == 0);
    CHECK(strcmp(dcam_verify_token(DCAM_VERIFY_FALSE), "false") == 0);
    CHECK(strcmp(dcam_verify_token(DCAM_VERIFY_NONE), "null") == 0);

    /* arming: only a command that wrote (applied or partial) gets a pending verify */
    memset(&r, 0, sizeof r);
    dcam_res(&r, "refused", "test");
    dcam_arm(&r, 1, 4, 0, VEH_BASE);
    CHECK(r.verify_state == DCAM_VERIFY_NONE);
    dcam_res(&r, "applied", "ok");
    dcam_arm(&r, 1, 4, 0, VEH_BASE);
    CHECK(r.verify_state == DCAM_VERIFY_PENDING && r.exp_check_car == 1 && r.exp_veh == VEH_BASE && r.exp_mode == 4);

    /* finish: car done + angle not applied -> partial with the angle reason; applied -> applied */
    dcam_res(&r, "redirected", "read back mode 1 index 0");
    dcam_finish_car(&r, 1);
    CHECK(strcmp(r.status, "partial") == 0 && strstr(r.reason, "car switched; angle not applied") != NULL);
    dcam_res(&r, "applied", "mode and index read back as requested");
    dcam_finish_car(&r, 1);
    CHECK(strcmp(r.status, "applied") == 0);

    /* result JSON carries verified_next_frame: pending | true | false | null, and the viewed slot */
    memset(&r, 0, sizeof r);
    r.seq = 7;
    r.done = 1;
    dcam_res(&r, "applied", "car switched and angle read back as requested");
    r.readback_ok = 1;
    r.mode = 4;
    r.index = 2;
    r.viewed_slot_ok = 1;
    r.viewed_slot = 5;
    r.car_step = 1;
    r.verify_state = DCAM_VERIFY_PENDING;
    dcam_cmd_json(buf, sizeof buf, "cut", &r);
    CHECK(strstr(buf, "\"verified_next_frame\":\"pending\"") != NULL);
    CHECK(strstr(buf, "\"viewed_slot\":5") != NULL);
    CHECK(strstr(buf, "\"next_frame\":null") != NULL);
    r.verify_state = DCAM_VERIFY_TRUE;
    r.nf_ok = 1;
    r.nf_mode = 4;
    r.nf_index = 2;
    r.nf_veh = VEH_BASE + 5u * VEH_STRIDE;
    dcam_cmd_json(buf, sizeof buf, "cut", &r);
    CHECK(strstr(buf, "\"verified_next_frame\":true") != NULL);
    CHECK(strstr(buf, "\"viewed\":\"0x") != NULL);
    r.verify_state = DCAM_VERIFY_FALSE;
    dcam_cmd_json(buf, sizeof buf, "cut", &r);
    CHECK(strstr(buf, "\"verified_next_frame\":false") != NULL);
    r.verify_state = DCAM_VERIFY_NONE;
    dcam_cmd_json(buf, sizeof buf, "cut", &r);
    CHECK(strstr(buf, "\"verified_next_frame\":null") != NULL);

    /* default gate: car switch off unless [Director] CarSwitch=1 */
    CHECK(g_dcam_carswitch_enabled == 0);
    g = dcam_chain_gate(g_dcam_carswitch_enabled, 1, 1, 1, 1, 1);
    CHECK(g != NULL && strstr(g, "CarSwitch=0") != NULL);
    printf("PASS TV Director camera change offline checks (prologue, player pick, position pick, JSON value, gate, watch, plan, status, verify, JSON)\n");
}

/* ---- R1-A (2026-10-14): file-backed v2 reader, review index and review id (spec 5.4, 6 T1/T4; R1 record) ----
 * Synthetic 60 s file at 20 Hz, 3 cars, lap changes and game lap events as storage_b2_index_test.c. The file is written to
 * %TEMP%\R1_build (build output, never the workspace) and read back through sa_rev_read_at (ReadFile). */
#define R1A_NT   1200u
#define R1A_NBLK (R1A_NT / 20u)
#define R1A_NCARS 3u
static sv2_channel_t r1a_cat[SV2_TABLE_MAX_CHANNELS];
static uint8_t r1a_buf[1u << 20], r1a_copy[1u << 20], r1a_work[SV2_MAX_PAYLOAD + 64], r1a_stored[SV2_MAX_PAYLOAD + 64];
static uint8_t r1a_work2[SV2_MAX_PAYLOAD + 64];
static uint32_t r1a_pool_e[1u << 12], r1a_pool_d[1u << 12], r1a_pool_r[1u << 12];
/* REPLAY 2.1 variant of the R1-A fixture (0 = the R1-A file of R1-A and R1-B, unchanged): positions (ch 17, 18),
 * a driver EVENT for car 0 and pit EVENTs of car 0 (in at 10 s, out at 12 s). Set by r21_test_replay only. */
static int r1a_driver = 0;
static int r1a_num[3] = { 7, 8, 18 };   /* TAPE2 N1/N2: race number of cars 0..2 on channel 97 (the N2 test makes two equal) */
static int r1a_remote2 = 0;   /* TAPE2 RT-E: 1 = car 2 (not the player) carries channel 98 = 2, a remote human */
static sv2_data_t r1a_enc, r1a_d1, r1a_d2;
static sv2_idx_block_t r1a_blk1[256], r1a_blk2[256];
static sv2_idx_lap_t r1a_lap1[64], r1a_lap2[64];
static sv2_idx_t r1a_ix1, r1a_ix2;
static size_t r1a_len;
static uint32_t r1a_seq;
static uint64_t r1a_max_read;

typedef struct { HANDLE h; const uint8_t *mem; size_t mem_len; int use_file; } r1a_src_t;

/* Reader under test: counts the largest request, then reads from the file (sa_rev_read_at) or from a buffer. */
static int r1a_read(void *ctx, uint64_t off, uint8_t *dst, size_t n) {
    r1a_src_t *s = (r1a_src_t *)ctx;
    if ((uint64_t)n > r1a_max_read) r1a_max_read = (uint64_t)n;
    if (s->use_file) return sa_rev_read_at(&s->h, off, dst, n);
    if (off > s->mem_len || n > s->mem_len - off) return -1;
    memcpy(dst, s->mem + off, n);
    return 0;
}

static double r1a_sim(uint32_t k) {   /* 20 Hz; sim time frozen for ticks 300..339 (a 2 s pause) */
    if (k < 300u) return 0.05 * (double)k;
    if (k < 340u) return 0.05 * 299.0;
    return 0.05 * (double)(k - 40u);
}

static const uint32_t r1a_chg_car[] = { 0, 0, 0, 1, 1 };
static const uint32_t r1a_chg_tick[] = { 240, 400, 800, 160, 560 };
#define R1A_NCHG (sizeof r1a_chg_car / sizeof r1a_chg_car[0])

static uint32_t r1a_lap_at(uint32_t car, uint32_t k) {
    uint32_t lap = 1;
    for (size_t i = 0; i < R1A_NCHG; i++) if (r1a_chg_car[i] == car && r1a_chg_tick[i] <= k) lap++;
    return lap;
}

static double r1a_game_time(size_t i) {   /* sim duration of the lap that ends at change i */
    uint32_t car = r1a_chg_car[i], prev = 0, j;
    for (j = 0; j < i; j++) if (r1a_chg_car[j] == car) prev = r1a_chg_tick[j];
    return r1a_sim(r1a_chg_tick[i]) - (prev ? r1a_sim(prev) : 0.0);
}

static void r1a_put(uint8_t type, uint32_t n_ticks, double t_first, const uint8_t *payload, size_t plen) {
    sv2_block_t blk;
    memset(&blk, 0, sizeof blk);
    blk.type = type;
    blk.seq = r1a_seq++;
    blk.n_ticks = n_ticks;
    blk.t_first = t_first;
    blk.raw_len = blk.stored_len = (uint32_t)plen;
    r1a_len += sv2_block_put(r1a_buf + r1a_len, sizeof r1a_buf - r1a_len, blk, payload);
}

/* Q16 raw of a value in the channel range -10000..10000 (catalog ids 17 and 18). */
static uint32_t r21_q16_r(double v, double lo, double hi) {
    double f = (v - lo) / (hi - lo) * 65535.0;
    if (f < 0.0) f = 0.0;
    if (f > 65535.0) f = 65535.0;
    return (uint32_t)llround(f);
}

static uint32_t r21_q16(double v) { return r21_q16_r(v, -10000.0, 10000.0); }

/* f32 bit pattern (the file stores RAW32 floats as their bits, sw_raw_is_int). */
static uint32_t r21_f32(float f) { uint32_t u; memcpy(&u, &f, sizeof u); return u; }

static void r21_pit(int state, double t) {
    char js[SV2_EVENT_MAX];
    snprintf(js, sizeof js, "{\"type\":\"pit\",\"t\":%.3f,\"car\":0,\"state\":%d,\"prev_state\":%d}", t, state, 1 - state);
    r1a_put(SV2_BLOCK_EVENT, 0, t, (const uint8_t *)js, strlen(js));
}

static void r1a_emit_data(uint32_t b) {
    size_t used = 0, n;
    uint32_t t0 = 20u * b;
    double s0 = r1a_sim(t0);
    memset(&r1a_enc, 0, sizeof r1a_enc);
    r1a_enc.n_ticks = 20;
    r1a_enc.n_cars = R1A_NCARS;
    for (uint32_t i = 0; i < 20u; i++) {
        r1a_enc.dt_ms[i] = i ? 50 : 0;
        r1a_enc.sim_ms[i] = (int32_t)llround((r1a_sim(t0 + i) - s0) * 1000.0);
    }
    for (uint32_t c = 0; c < R1A_NCARS; c++) {
        sv2_car_t *car = &r1a_enc.cars[c];
        car->slot = (uint8_t)c;
        car->scope = c == 0 ? 1u : 0u;
        car->first_tick = 0;
        car->count = 20;
        car->mask_lo = 0;
        car->mask_hi = 0;
        sv2_mask_set(&car->mask_lo, &car->mask_hi, SV2_CH_LAP);
        car->vals = r1a_pool_e + used;
        if (r1a_driver) {   /* REPLAY 2.1 / 2.3 / follow-up: channel-major, ascending id; car 0 also has ch 24 (pch f32) */
            static const uint32_t ids0[] = { 2u, 17u, 18u, 24u, 68u, 69u, 86u, 97u, 98u };
            static const uint32_t ids1[] = { 2u, 17u, 18u, 68u, 69u, 86u, 97u, 98u };
            const uint32_t *ids = c == 0 ? ids0 : ids1;
            uint32_t nc = c == 0 ? 9u : 8u;
            for (uint32_t k = 0; k < nc; k++) {
                uint32_t id = ids[k];
                sv2_mask_set(&car->mask_lo, &car->mask_hi, id);
                for (uint32_t i = 0; i < 20u; i++) {
                    uint32_t raw = 0;
                    double tk = (double)(t0 + i);
                    switch (id) {
                    case 2u: raw = r21_q16_r(60.0 + 5.0 * (double)c, 0.0, 120.0); break;
                    case 17u: raw = r21_q16(1000.0 + 5.0 * tk + 100.0 * (double)c); break;
                    case 18u: raw = r21_q16(500.0 + 20.0 * (double)c); break;
                    case 24u: raw = r21_f32(12.5f); break;
                    case 68u: raw = (uint32_t)(int32_t)(c + 1u); break;
                    case 69u: raw = (uint32_t)(int32_t)r1a_lap_at(c, t0 + i); break;
                    case 86u: raw = r21_f32(c == 0 ? 98.5f : 91.25f); break;
                    case 97u: raw = (uint32_t)(int32_t)r1a_num[c > 2u ? 2u : c]; break;   /* TAPE2 N1: race numbers */
                    default:  /* 98: car 0 remote (2) for the first 100 ticks, then player (0); car 1 AI (1); car 2 replay (3) */
                        raw = (uint32_t)(int32_t)(c == 0 ? (t0 + i < 100u ? 2 : 0) : (c == 1u ? 1 : (c == 2u && r1a_remote2 ? 2 : 3)));
                        break;
                    }
                    car->vals[k * 20u + i] = raw;
                }
            }
            used += nc * 20u;
            continue;
        }
        for (uint32_t i = 0; i < 20u; i++) car->vals[i] = (uint32_t)(int32_t)r1a_lap_at(c, t0 + i);
        used += 20;
    }
    n = sv2_data_encode(&r1a_enc, r1a_cat, SV2_CATALOG_COUNT, r1a_work, sizeof r1a_work);
    CHECK(n > 0);
    r1a_put(SV2_BLOCK_DATA, 20, s0, r1a_work, n);
}

static void r1a_build_file(void) {
    static uint8_t table[8192];
    sv2_header_t h;
    size_t tl;
    static const char meta[] = "{\"schema\":\"f1c.telemetry.meta/3\",\"parts\":[\"session\"]}";
    memset(&h, 0, sizeof h);
    h.flags = SV2_FLAG_HAS_SIM | SV2_FLAG_UNCOMPRESSED_FALLBACK;
    h.compression_id = SV2_COMP_NONE;
    h.block_ticks_max = 20;
    h.block_ms_max = 1000;
    h.channel_table_offset = 192;
    h.start_unix_utc = 1791000000.5;
    h.tz_offset_min = -300;
    h.session_type = 7;
    h.broker_generation = 1;
    h.channels_declared = SV2_CATALOG_COUNT;
    strcpy(h.track, "Monza");
    strcpy(h.session_name, "Monza_2001_2026-10-13_14-05");
    tl = sv2_table_encode(r1a_cat, SV2_CATALOG_COUNT, table, sizeof table);
    h.channel_table_bytes = (uint32_t)tl;
    sv2_header_encode(&h, r1a_buf);
    memcpy(r1a_buf + 192, table, tl);
    r1a_len = 192 + tl;
    r1a_seq = 0;
    r1a_put(SV2_BLOCK_META, 0, 0.0, (const uint8_t *)meta, strlen(meta));
    if (r1a_driver) {
        static const char d0[] = "{\"type\":\"driver\",\"t\":0.000,\"car\":0,\"name\":\"Alex Test\",\"team\":\"Team A\",\"num\":7}";
        r1a_put(SV2_BLOCK_EVENT, 0, 0.0, (const uint8_t *)d0, strlen(d0));
    }
    for (uint32_t b = 0; b < R1A_NBLK; b++) {
        r1a_emit_data(b);
        if (r1a_driver && b == 5u) {
            static const char inc[] = "{\"type\":\"incident\",\"kind\":\"contact\",\"t\":5.500,\"car\":1,\"with\":2}";
            r1a_put(SV2_BLOCK_EVENT, 0, r1a_sim(110), (const uint8_t *)inc, strlen(inc));
        }
        if (r1a_driver && b == 10u) r21_pit(1, r1a_sim(200));
        if (r1a_driver && b == 12u) r21_pit(0, r1a_sim(240));
        for (size_t i = 0; i < R1A_NCHG; i++) {   /* EVENT lap of car 0 after its DATA block (writer order) */
            char js[SV2_EVENT_MAX];
            if (r1a_chg_car[i] != 0 || r1a_chg_tick[i] / 20u != b) continue;
            snprintf(js, sizeof js, "{\"type\":\"lap\",\"t\":%.3f,\"car\":0,\"lap\":%u,\"prev_lap\":%u,\"last_lap_s\":%.3f}",
                     r1a_sim(r1a_chg_tick[i]), r1a_lap_at(0, r1a_chg_tick[i]), r1a_lap_at(0, r1a_chg_tick[i] - 1u),
                     r1a_game_time(i));
            r1a_put(SV2_BLOCK_EVENT, 0, r1a_sim(r1a_chg_tick[i]), (const uint8_t *)js, strlen(js));
        }
    }
    {
        uint8_t e[12];
        sv2_put_le(e, R1A_NBLK, 4);
        sv2_put_le(e + 4, R1A_NT, 4);
        sv2_put_le(e + 8, 0, 4);
        r1a_put(SV2_BLOCK_END, 0, 0.0, e, 12);
    }
}

static int r1a_same_block(const sv2_data_t *x, const sv2_data_t *y) {
    if (x->n_ticks != y->n_ticks || x->n_cars != y->n_cars) return 0;
    if (memcmp(x->sim_ms, y->sim_ms, sizeof(int32_t) * x->n_ticks) != 0) return 0;
    if (memcmp(x->dt_ms, y->dt_ms, sizeof(int32_t) * x->n_ticks) != 0) return 0;
    for (uint32_t c = 0; c < x->n_cars; c++) {
        const sv2_car_t *a = &x->cars[c], *b = &y->cars[c];
        uint32_t n;
        if (a->slot != b->slot || a->scope != b->scope || a->first_tick != b->first_tick || a->count != b->count ||
            a->mask_lo != b->mask_lo || a->mask_hi != b->mask_hi || a->cmask_lo != b->cmask_lo || a->cmask_hi != b->cmask_hi)
            return 0;
        n = sv2_popcount64(a->mask_lo) + sv2_popcount64(a->mask_hi);
        if (memcmp(a->vals, b->vals, sizeof(uint32_t) * (size_t)n * a->count) != 0) return 0;
    }
    return 1;
}

static int r1a_same_index(const sv2_idx_t *x, const sv2_idx_t *y) {
    if (x->nblocks != y->nblocks || x->nlaps != y->nlaps || x->reason != y->reason || x->complete != y->complete ||
        x->data_blocks != y->data_blocks || x->events_lap != y->events_lap || x->monotonic != y->monotonic)
        return 0;
    for (uint32_t i = 0; i < x->nblocks; i++) {
        const sv2_idx_block_t *a = &x->blocks[i], *b = &y->blocks[i];
        if (a->file_offset != b->file_offset || a->seq != b->seq || a->ticks != b->ticks || a->t_first != b->t_first ||
            a->t_last != b->t_last)
            return 0;
    }
    for (uint32_t i = 0; i < x->nlaps; i++) {
        const sv2_idx_lap_t *a = &x->laps[i], *b = &y->laps[i];
        if (a->car != b->car || a->lap != b->lap || a->end_sim != b->end_sim || a->start_sim != b->start_sim ||
            a->game_time != b->game_time || a->has_start != b->has_start || a->has_game != b->has_game)
            return 0;
    }
    return 1;
}

static void r1a_test_reader(void) {
    char dir[MAX_PATH], path[MAX_PATH + 64], id_a[24], id_b[24], id_c[24];
    HANDLE h;
    LARGE_INTEGER sz;
    DWORD w = 0;
    r1a_src_t file_src, mem_src;
    sv2_rd_t rd_file, rd_mem;
    int r;
    sv2_catalog_to_table(r1a_cat);
    r1a_build_file();
    CHECK(r1a_len > 192);

    /* Write the synthetic file to %TEMP%\R1_build (build output). */
    GetTempPathA(sizeof dir, dir);
    snprintf(path, sizeof path, "%sR1_build", dir);
    CreateDirectoryA(path, NULL);
    snprintf(path, sizeof path, "%sR1_build\\r1a_synthetic.f1t", dir);
    h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(WriteFile(h, r1a_buf, (DWORD)r1a_len, &w, NULL) && w == (DWORD)r1a_len);
    CloseHandle(h);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(GetFileSizeEx(h, &sz) && (size_t)sz.QuadPart == r1a_len);

    /* Whole-buffer reference and file-backed index must agree in every field. */
    sv2_idx_init(&r1a_ix1, r1a_blk1, 256, r1a_lap1, 64);
    r1a_d1.pool = r1a_pool_d; r1a_d1.pool_cap = 1u << 12;
    r = sv2_index_build(r1a_buf, r1a_len, &r1a_d1, r1a_work2, sizeof r1a_work2, &r1a_ix1);
    CHECK(r == SV2_OK && r1a_ix1.complete == 1 && r1a_ix1.nblocks == R1A_NBLK && r1a_ix1.nlaps == 5);

    file_src.h = h; file_src.mem = r1a_buf; file_src.mem_len = r1a_len; file_src.use_file = 1;
    rd_file.read_at = r1a_read; rd_file.ctx = &file_src; rd_file.len = (uint64_t)sz.QuadPart;
    sv2_idx_init(&r1a_ix2, r1a_blk2, 256, r1a_lap2, 64);
    r1a_d2.pool = r1a_pool_r; r1a_d2.pool_cap = 1u << 12;
    r1a_max_read = 0;
    r = sv2_index_build_rd(&rd_file, &r1a_d2, r1a_stored, r1a_work, SV2_MAX_PAYLOAD, &r1a_ix2);
    CHECK(r == SV2_OK);
    CHECK(r1a_same_index(&r1a_ix1, &r1a_ix2));                              /* T1: same lap and block index */
    CHECK(r1a_max_read <= (uint64_t)(SV2_MAX_PAYLOAD + SV2_BLOCK_HEADER_BYTES)); /* T4: never more than one block */
    CHECK(r1a_ix2.nlaps == 5);
    {
        const sv2_idx_lap_t *lp = sv2_idx_lap_find(&r1a_ix2, 0, 2);       /* B2 truth through the file reader */
        CHECK(lp && lp->has_game && fabs(lp->game_time - 6.0) <= 0.001);
        CHECK(lp && lp->has_start && fabs((lp->end_sim - lp->start_sim) - 6.0) <= 0.001);
    }

    /* Block by block: the file reader decodes exactly what the whole-buffer reader decodes. */
    for (uint32_t i = 0; i < r1a_ix1.nblocks; i++) {
        int ra = sv2_idx_read_block(r1a_buf, r1a_len, &r1a_ix1, i, &r1a_d1, r1a_work2, sizeof r1a_work2);
        int rb = sv2_idx_read_block_rd(&rd_file, &r1a_ix2, i, &r1a_d2, r1a_stored, r1a_work, SV2_MAX_PAYLOAD);
        CHECK(ra == SV2_OK && rb == SV2_OK);
        if (ra == SV2_OK && rb == SV2_OK) CHECK(r1a_same_block(&r1a_d1, &r1a_d2));
    }
    CHECK(r1a_ix2.blocks_read == R1A_NBLK);

    /* Truncated file: same reason as the whole-buffer walk, no crash, the valid prefix is kept. */
    {
        size_t cut = r1a_len - 300;
        sv2_idx_init(&r1a_ix1, r1a_blk1, 256, r1a_lap1, 64);
        sv2_idx_init(&r1a_ix2, r1a_blk2, 256, r1a_lap2, 64);
        mem_src.h = INVALID_HANDLE_VALUE; mem_src.mem = r1a_buf; mem_src.mem_len = cut; mem_src.use_file = 0;
        rd_mem.read_at = r1a_read; rd_mem.ctx = &mem_src; rd_mem.len = cut;
        r1a_d1.pool = r1a_pool_d; r1a_d1.pool_cap = 1u << 12;
        r1a_d2.pool = r1a_pool_r; r1a_d2.pool_cap = 1u << 12;
        r = sv2_index_build(r1a_buf, cut, &r1a_d1, r1a_work2, sizeof r1a_work2, &r1a_ix1);
        CHECK(r != SV2_OK && r1a_ix1.nblocks > 0 && r1a_ix1.nblocks < R1A_NBLK);   /* valid prefix kept */
        r = sv2_index_build_rd(&rd_mem, &r1a_d2, r1a_stored, r1a_work, SV2_MAX_PAYLOAD, &r1a_ix2);
        CHECK(r != SV2_OK && r1a_ix2.nblocks == r1a_ix1.nblocks && r1a_ix2.reason == r1a_ix1.reason);
    }

    /* Corrupt payload of block 5 (CRC): both readers stop at block 5 with the same reason. */
    {
        uint32_t off5 = r1a_ix1.blocks[5].file_offset;
        memcpy(r1a_copy, r1a_buf, r1a_len);
        r1a_copy[off5 + SV2_BLOCK_HEADER_BYTES + 3] ^= 0x5A;
        sv2_idx_init(&r1a_ix1, r1a_blk1, 256, r1a_lap1, 64);
        sv2_idx_init(&r1a_ix2, r1a_blk2, 256, r1a_lap2, 64);
        mem_src.mem = r1a_copy;
        mem_src.mem_len = r1a_len;
        rd_mem.len = r1a_len;
        r1a_d1.pool = r1a_pool_d; r1a_d1.pool_cap = 1u << 12;
        r1a_d2.pool = r1a_pool_r; r1a_d2.pool_cap = 1u << 12;
        r = sv2_index_build(r1a_copy, r1a_len, &r1a_d1, r1a_work2, sizeof r1a_work2, &r1a_ix1);
        CHECK(r == SV2_R_BLOCK_CRC && r1a_ix1.nblocks == 5);
        r = sv2_index_build_rd(&rd_mem, &r1a_d2, r1a_stored, r1a_work, SV2_MAX_PAYLOAD, &r1a_ix2);
        CHECK(r == SV2_R_BLOCK_CRC && r1a_ix2.nblocks == 5);
    }
    CloseHandle(h);

    /* T1-id: the review id is opaque, stable and 19 characters; path-like ids are refused. */
    sa_review_id("Monza", "Race_2026-10-13_14-05", id_a, sizeof id_a);
    sa_review_id("Monza", "Race_2026-10-13_14-05", id_b, sizeof id_b);
    sa_review_id("Monza", "Race_2026-10-13_14-06", id_c, sizeof id_c);
    CHECK(strlen(id_a) == 19 && strncmp(id_a, "v2_", 3) == 0);
    CHECK(strcmp(id_a, id_b) == 0 && strcmp(id_a, id_c) != 0);
    CHECK(sa_rid_id_ok(id_a) == 1);
    CHECK(sa_rid_id_ok("v2_ABCDEF0123456789") == 0);
    CHECK(sa_rid_id_ok("v2_0123456789abcde") == 0);
    CHECK(sa_rid_id_ok("v2_..\\x0123456789ab") == 0);
    CHECK(sa_rid_id_ok("1.0.1-style-session-id") == 0);
}

/* ---------- R1-B (2026-10-14): review adapter on the R1-A synthetic file (memory reader, no HTTP, no game) ----------
 * Checks the 1.0.1 shapes the adapter writes, the lap and sample counts against the index, the 19 sample keys, the
 * refusal of lap numbers that are not timed, and the /telemetry frame with its sentinels. The HTTP route and the
 * 503 / 404 / 409 gates run in test_telemetry.py (review cases) with the game closed. */
static sa_rev_slot_t r1b_slot;
static char r1b_out[1u << 21];

/* Writes one response body to %TEMP%\R1_build\<name> for the schema check (test_telemetry / contract_validate). */
static void r1b_dump(const char *name, const char *body, size_t n) {
    char dir[MAX_PATH], path[MAX_PATH + 64];
    FILE *f;
    GetTempPathA(sizeof dir, dir);
    snprintf(path, sizeof path, "%sR1_build\\%s", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    fwrite(body, 1, n, f);
    fclose(f);
}

static size_t r1b_count(const char *s, const char *needle) {
    size_t n = 0, L = strlen(needle);
    const char *p = s;
    while ((p = strstr(p, needle)) != NULL) { n++; p += L; }
    return n;
}

static void r1b_test_adapter(void) {
    r1a_src_t mem;
    sv2_rd_t rd;
    sv2_header_t hdr;
    sa_rv_bufs_t B;
    sv2_idx_t *ix = &r1b_slot.ix;
    sa_buf_t b;
    int32_t pos[SV2_MAX_CARS];
    uint8_t pos_ok[SV2_MAX_CARS], ai[SV2_MAX_CARS], ai_ok[SV2_MAX_CARS];
    char rid[24];
    uint32_t timed = 0, timed0_lap = 0, timed0_samples = 0;
    const sv2_idx_lap_t *r0 = NULL;
    int code, r;

    r1a_build_file();
    mem.h = INVALID_HANDLE_VALUE;
    mem.mem = r1a_buf;
    mem.mem_len = r1a_len;
    mem.use_file = 0;
    rd.read_at = r1a_read;
    rd.ctx = &mem;
    rd.len = (uint64_t)r1a_len;
    CHECK(sa_rv_bufs_alloc(&B));
    sa_review_id("Monza", "r1b_synthetic", rid, sizeof rid);
    CHECK(sa_rid_id_ok(rid) == 1);

    sv2_idx_init(ix, r1b_slot.blocks, SA_REV_BLOCKS, r1b_slot.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    CHECK(r == SV2_OK && ix->complete == 1 && ix->nblocks == R1A_NBLK);
    CHECK(sv2_rd_header(&rd, &hdr) == SV2_OK && strcmp(hdr.track, "Monza") == 0);

    memset(pos, 0, sizeof pos); memset(pos_ok, 0, sizeof pos_ok);
    memset(ai, 0, sizeof ai); memset(ai_ok, 0, sizeof ai_ok);
    const sa_rv_nm_t *nmt = sa_rv_names(rid, "r1b_synthetic", ix, &rd, &B);
    sa_rv_scan_cars(ix, &rd, &B, pos, pos_ok, ai, ai_ok);
    CHECK(ix->car[0].seen && ix->car[1].seen && ix->car[2].seen && !ix->car[3].seen);
    CHECK(ai_ok[0] && ai[0] == 0);   /* car 0 is the player (scope 1) */
    CHECK(ai_ok[1] && ai[1] == 1);   /* cars 1 and 2 are AI (scope 0) */

    /* timed laps: a row with a start and a game time (the B2 rule; the first partial lap is not timed) */
    for (uint32_t k = 0; k < ix->nlaps; k++) {
        const sv2_idx_lap_t *row = &ix->laps[k];
        if (!row->has_start || !row->has_game) continue;
        timed++;
        if (row->car == 0 && !r0) r0 = row;   /* first timed lap of car 0: lap 2, ticks 240..399 (lap 1 = first sighting, untimed) */
    }
    CHECK(timed == 2 && r0 != NULL);
    if (r0) { timed0_lap = r0->lap; timed0_samples = r0->samples; }
    if (!(r0 && timed0_samples == 160 && timed0_lap == 2)) printf("R1B diag: timed=%u lap=%u samples=%u\n", (unsigned)timed, (unsigned)timed0_lap, (unsigned)timed0_samples);
    CHECK(r0 && timed0_samples == 160 && timed0_lap == 2);

    /* /laps: 1.0.1 shape, recording source, three cars, timed laps only */
    b.p = r1b_out; b.cap = sizeof r1b_out; b.n = 0; b.bad = 0;
    sa_rv_laps_doc(&b, ix, rid, pos, pos_ok, nmt);
    CHECK(!b.bad);
    CHECK(strstr(b.p, "\"source\":\"recording\"") != NULL && strstr(b.p, "\"validated\":false") != NULL);
    CHECK(strstr(b.p, "\"lap_len\":null") != NULL);
    CHECK(r1b_count(b.p, "\"n\":\"Car ") == 3);
    CHECK(r0 && strstr(b.p, "{\"id\":0,") != NULL);
    if (r0) {
        char want[32];
        snprintf(want, sizeof want, "%.3f", r0->game_time);
        CHECK(strstr(b.p, want) != NULL);
    }

    /* /trace/status: the same timed laps, each with its lid (src recording) */
    b.n = 0; b.bad = 0;
    sa_rv_status_doc(&b, ix, rid, ai, ai_ok, nmt);
    CHECK(!b.bad && strstr(b.p, "\"mode\":\"archive\"") != NULL);
    CHECK(r1b_count(b.p, "\"src\":\"recording\"") == timed);
    CHECK(r1b_count(b.p, "\"unavailable\":[]") == 3);

    /* /trace/data for the first timed lap of car 0: 1.0.1 shape, one object per sample, 19 keys each */
    b.n = 0; b.bad = 0;
    code = sa_rv_trace_doc(&b, ix, &rd, &B, rid, 0, timed0_lap, hdr.track, nmt);
    CHECK(code == 200 && !b.bad);
    CHECK(strstr(b.p, "\"schema\":\"f1c.trace/1\"") != NULL && strstr(b.p, "\"source\":\"recording\"") != NULL);
    CHECK(r1b_count(b.p, "\"t\":") == 160 && timed0_samples == 160);
    CHECK(r1b_count(b.p, "\"rx\":null") == 160 && r1b_count(b.p, "\"tr\":[") == 160 && r1b_count(b.p, "\"f\":") == 160);
    CHECK(r1b_count(b.p, "\"tt\":null") == 160 && r1b_count(b.p, "\"st\":null") == 160);
    CHECK(strstr(b.p, "\"count\":160") != NULL && strstr(b.p, "truncated") == NULL);

    /* refusals: a lap that is not timed, a car with no such lap */
    b.n = 0; b.bad = 0;
    code = sa_rv_trace_doc(&b, ix, &rd, &B, rid, 0, 0, hdr.track, nmt);
    CHECK(code == 404 && strstr(b.p, "lap not recorded in this session") != NULL);
    b.n = 0; b.bad = 0;
    code = sa_rv_trace_doc(&b, ix, &rd, &B, rid, 1, 1, hdr.track, nmt);
    CHECK(code == 404);

    /* /telemetry: one frame; the review object, the sentinel pch object and one car object per car seen */
    b.n = 0; b.bad = 0;
    sa_rv_frame_doc(&b, ix, &hdr, rid, "r1b_synthetic", pos, pos_ok, nmt);
    CHECK(!b.bad);
    CHECK(strstr(b.p, "\"review\":{\"session\":\"") != NULL && strstr(b.p, "\"lapm\":0,") != NULL);
    CHECK(strstr(b.p, "\"trk\":\"Monza\"") != NULL && strstr(b.p, "\"pch\":{\"rh\":") != NULL);
    CHECK(r1b_count(b.p, "{\"id\":") == 3);
    CHECK(strstr(b.p, "\"n\":\"Car 0\"") != NULL && strstr(b.p, "\"gai\":-1") != NULL);
    CHECK(sa_rv_is_api_route("/laps") && sa_rv_is_api_route("/trace/data") && sa_rv_is_api_route("/driver"));
    CHECK(!sa_rv_is_api_route("/") && !sa_rv_is_api_route("/telemetry/x") && !sa_rv_is_api_route("/recordings"));

    /* the review bodies for schema validation (R1B record, game closed run) */
    b.n = 0; b.bad = 0;
    sa_rv_laps_doc(&b, ix, rid, pos, pos_ok, nmt);
    r1b_dump("r1b_laps.json", b.p, b.n);
    b.n = 0; b.bad = 0;
    sa_rv_status_doc(&b, ix, rid, ai, ai_ok, nmt);
    r1b_dump("r1b_trace_status.json", b.p, b.n);
    b.n = 0; b.bad = 0;
    code = sa_rv_trace_doc(&b, ix, &rd, &B, rid, 0, timed0_lap, hdr.track, nmt);
    r1b_dump("r1b_trace_data.json", b.p, b.n);
    b.n = 0; b.bad = 0;
    sa_rv_frame_doc(&b, ix, &hdr, rid, "r1b_synthetic", pos, pos_ok, nmt);
    r1b_dump("r1b_telemetry.json", b.p, b.n);
    (void)code;

    sa_rv_bufs_free(&B);
    printf("R1-B review adapter: PASS (timed laps %u, car 0 lap %u, samples %u, reads ok)\n", timed, timed0_lap, timed0_samples);
}

/* ---------- TAPE2 N1 (2026-10-15): names of a recording without driver EVENTs, from the season .veh files ----------
 * The fixture .veh files are written under %TEMP%\TAPE2_n1_season\SEASONDATA\Vehicles and read through TAPE2_SEASON_ROOT
 * (the game folder is never read here). The variable is restored at the end, so serve mode keeps its own value. The
 * fixture mirrors the game's Tyrrell 1997 files: two driver names on Number=18 (no name is given for that race number). */
#define N1_STEM97 "AUSTRALIA 1997 - CO-OP - Race - 2026-10-14 20-17_2"
#define N1_STEM98 "AUSTRALIA 1998 - CO-OP - Race - 2026-10-14 20-17_2"
#define N1_STEM96 "AUSTRALIA 1996 - CO-OP - Race - 2026-10-14 20-17_2"
static char n1_out[1u << 20];
static char n1_frame[1u << 20];
static sa_rev_slot_t n1_slot;

static void n1_veh(const char *veh, const char *team, const char *car, const char *file, const char *text) {
    char p[2048];
    snprintf(p, sizeof p, "%s%s\\", veh, team);
    b2_mkdir(p);
    snprintf(p, sizeof p, "%s%s\\%s\\", veh, team, car);
    b2_mkdir(p);
    snprintf(p, sizeof p, "%s%s\\%s\\%s", veh, team, car, file);
    b2_write_file(p, text, strlen(text));
}

static void n1_test_names(void) {
    char tmp[MAX_PATH], dir[1024], sd[1024], veh[1024], prev[MAX_PATH], rid[24], nm[NAME_LEN + 16], tm[NAME_LEN];
    DWORD n, pl;
    r1a_src_t mem;
    sv2_rd_t rd;
    sv2_header_t hdr;
    sa_rv_bufs_t B;
    sv2_idx_t *ix = &n1_slot.ix;
    sa_buf_t b;
    const sa_rv_nm_t *nmt;
    int32_t pos[SV2_MAX_CARS];
    uint8_t pos_ok[SV2_MAX_CARS], ai[SV2_MAX_CARS], ai_ok[SV2_MAX_CARS];
    int r;

    n = GetTempPathA(sizeof tmp, tmp);
    CHECK(n > 0 && n < sizeof tmp);
    snprintf(dir, sizeof dir, "%sTAPE2_n1_season\\", tmp);
    snprintf(sd, sizeof sd, "%sTAPE2_n1_season\\SEASONDATA\\", tmp);
    snprintf(veh, sizeof veh, "%sTAPE2_n1_season\\SEASONDATA\\Vehicles\\", tmp);
    b2_mkdir(dir);
    b2_mkdir(sd);
    b2_mkdir(veh);
    n1_veh(veh, "Benetton", "1997_B197", "1997_Benetton08.veh",
           "HDVehicle=1997_Benetton.hdv\nNumber=8\nTeam=\"Benetton\"\nDriver=\"Gerhard Berger (1997)\"\n");
    n1_veh(veh, "Tyrrell", "1997_025", "1997_Tyrrell18.veh",
           "Number=18\nTeam=\"Tyrrell\"\nDriver=\"Jos Verstappen (1997)\"\n");
    n1_veh(veh, "Tyrrell", "1997_025", "1997_Tyrrell19.veh",
           "Number=18\nTeam=\"Tyrrell\"\nDriver=\"Mika Salo (1997)\"\n");
    n1_veh(veh, "Benetton", "1998_B198", "1998_Benetton08.veh",
           "Number=8\nTeam=\"Benetton\"\nDriver=\"Someone Else (1998)\"\n");
    prev[0] = '\0';
    pl = GetEnvironmentVariableA("TAPE2_SEASON_ROOT", prev, sizeof prev);
    if (pl >= sizeof prev) prev[0] = '\0';
    CHECK(SetEnvironmentVariableA("TAPE2_SEASON_ROOT", sd));
    vehn_reset();
    sa_rv_nm_have = 0;

    /* the recording: r1a driver fixture (car 0 has a driver EVENT; race numbers 7, 8 and 18 on channel 97) */
    r1a_driver = 1;
    r1a_build_file();
    mem.h = INVALID_HANDLE_VALUE;
    mem.mem = r1a_buf;
    mem.mem_len = r1a_len;
    mem.use_file = 0;
    rd.read_at = r1a_read;
    rd.ctx = &mem;
    rd.len = (uint64_t)r1a_len;
    CHECK(sa_rv_bufs_alloc(&B));
    sa_review_id("Monza", "n1_synthetic", rid, sizeof rid);
    sv2_idx_init(ix, n1_slot.blocks, SA_REV_BLOCKS, n1_slot.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    CHECK(r == SV2_OK && ix->complete == 1);
    CHECK(sv2_rd_header(&rd, &hdr) == SV2_OK);
    memset(pos, 0, sizeof pos); memset(pos_ok, 0, sizeof pos_ok);
    memset(ai, 0, sizeof ai); memset(ai_ok, 0, sizeof ai_ok);
    sa_rv_scan_cars(ix, &rd, &B, pos, pos_ok, ai, ai_ok);

    /* 1997 (year from the stem): EVENT wins for car 0; race 8 -> Gerhard Berger (Benetton); race 18 has two names -> "#18" */
    nmt = sa_rv_names(rid, N1_STEM97, ix, &rd, &B);
    CHECK(nmt[0].src == SA_NM_EVENT && strcmp(nmt[0].name, "Alex Test") == 0);
    CHECK(nmt[1].src == SA_NM_VEH && strcmp(nmt[1].name, "Gerhard Berger") == 0 && strcmp(nmt[1].team, "Benetton") == 0 && nmt[1].num == 8);
    CHECK(nmt[2].src == SA_NM_NUM && strcmp(nmt[2].name, "#18") == 0 && nmt[2].num == 18);
    b.p = n1_out; b.cap = sizeof n1_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_info_doc(&b, ix, &rd, &B, &hdr, rid, N1_STEM97, nmt) == 1);
    CHECK(strstr(n1_out, "\"slot\":1,\"name\":\"Gerhard Berger\",\"team\":\"Benetton\",\"num\":8,\"role\":\"ai\"") != NULL);
    CHECK(strstr(n1_out, "\"slot\":2,\"name\":\"#18\"") != NULL);
    b.p = n1_out; b.cap = sizeof n1_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_driver_doc(&b, ix, &rd, &B, 1, 20.0, nmt) == 1);
    CHECK(strstr(n1_out, "\"id\":1,\"n\":\"Gerhard Berger\",\"cn\":\"Benetton\"") != NULL);
    b.p = n1_out; b.cap = sizeof n1_out; b.n = 0; b.bad = 0;
    sa_rv_laps_doc(&b, ix, rid, pos, pos_ok, nmt);
    CHECK(!b.bad && strstr(n1_out, "\"n\":\"Gerhard Berger\"") != NULL && strstr(n1_out, "\"n\":\"#18\"") != NULL);
    b.p = n1_out; b.cap = sizeof n1_out; b.n = 0; b.bad = 0;
    sa_rv_status_doc(&b, ix, rid, ai, ai_ok, nmt);
    CHECK(!b.bad && strstr(n1_out, "\"n\":\"Gerhard Berger\",\"cn\":\"Benetton\"") != NULL);
    b.p = n1_frame; b.cap = sizeof n1_frame; b.n = 0; b.bad = 0;
    CHECK(sa_rv_frame_tape(&b, ix, &rd, &B, &hdr, rid, N1_STEM97, 10.0, 0.0, 57.95, nmt) == 1);
    CHECK(strstr(n1_frame, "\"n\":\"Alex Test\"") != NULL && strstr(n1_frame, "\"n\":\"Gerhard Berger\"") != NULL &&
          strstr(n1_frame, "\"n\":\"#18\"") != NULL);
    printf("N1 map (1997, CO-OP fixture): car 0 Alex Test (driver EVENT), car 1 Gerhard Berger (.veh, Benetton), car 2 #18 (two names): PASS\n");

    /* the season is the stem year: 1998 has another driver on 8; 1996 has no folder; no year in the stem: "#8" */
    nmt = sa_rv_names(rid, N1_STEM98, ix, &rd, &B);
    CHECK(nmt[1].src == SA_NM_VEH && strcmp(nmt[1].name, "Someone Else") == 0 && strcmp(nmt[1].team, "Benetton") == 0);
    nmt = sa_rv_names(rid, N1_STEM96, ix, &rd, &B);
    CHECK(nmt[1].src == SA_NM_NUM && strcmp(nmt[1].name, "#8") == 0);
    nmt = sa_rv_names(rid, "n1_plain", ix, &rd, &B);
    CHECK(nmt[1].src == SA_NM_NUM && strcmp(nmt[1].name, "#8") == 0 && nmt[0].src == SA_NM_EVENT);
    printf("N1 season by stem year: 1998 Someone Else, 1996 and no year #8: PASS\n");

    /* lookups: one name per race number and year; two names -> refused; not in the season -> refused */
    CHECK(vehn_lookup(1997, 18, nm, sizeof nm, tm, sizeof tm) == 0);
    CHECK(vehn_lookup(1997, 99, nm, sizeof nm, tm, sizeof tm) == 0);
    CHECK(vehn_lookup(1995, 8, nm, sizeof nm, tm, sizeof tm) == 0);
    CHECK(vehn_lookup(1997, 8, nm, sizeof nm, tm, sizeof tm) == 1 && strcmp(nm, "Gerhard Berger") == 0 && strcmp(tm, "Benetton") == 0);
    CHECK(vehn_stem_year(N1_STEM97) == 1997 && vehn_stem_year("n1_plain") == 0 && vehn_stem_year("2026-10-14 20-17") == 2026);
    printf("N1 lookups: 18 refused (two names), 99 and 1995 refused, 8 in 1997 = Gerhard Berger: PASS\n");

    /* N2: cars 1 and 2 both carry race number 8 (the CO-OP remote copy case). Both are kept with the same .veh name, no crash,
     * and both frames and lists carry it. A new review id, so the caches are rebuilt. */
    r1a_num[2] = 8;
    r1a_build_file();
    mem.mem_len = r1a_len;
    rd.len = (uint64_t)r1a_len;
    sa_review_id("Monza", "n1_dup", rid, sizeof rid);
    sv2_idx_init(ix, n1_slot.blocks, SA_REV_BLOCKS, n1_slot.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    CHECK(r == SV2_OK && ix->complete == 1);
    memset(pos, 0, sizeof pos); memset(pos_ok, 0, sizeof pos_ok);
    memset(ai, 0, sizeof ai); memset(ai_ok, 0, sizeof ai_ok);
    sa_rv_scan_cars(ix, &rd, &B, pos, pos_ok, ai, ai_ok);
    nmt = sa_rv_names(rid, N1_STEM97, ix, &rd, &B);
    CHECK(nmt[1].src == SA_NM_VEH && nmt[2].src == SA_NM_VEH && nmt[1].num == 8 && nmt[2].num == 8);
    CHECK(strcmp(nmt[1].name, "Gerhard Berger") == 0 && strcmp(nmt[2].name, "Gerhard Berger") == 0);
    b.p = n1_out; b.cap = sizeof n1_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_info_doc(&b, ix, &rd, &B, &hdr, rid, N1_STEM97, nmt) == 1);
    CHECK(r1b_count(n1_out, "\"name\":\"Gerhard Berger\",\"team\":\"Benetton\",\"num\":8,") == 2);
    b.p = n1_frame; b.cap = sizeof n1_frame; b.n = 0; b.bad = 0;
    CHECK(sa_rv_frame_tape(&b, ix, &rd, &B, &hdr, rid, N1_STEM97, 10.0, 0.0, 57.95, nmt) == 1);
    CHECK(r1b_count(n1_frame, "\"n\":\"Gerhard Berger\"") == 2 && strstr(n1_frame, "\"n\":\"Alex Test\"") != NULL);
    printf("N2 two cars with race number 8: both kept (info 2, frame 2), no crash: PASS\n");
    r1a_num[2] = 18;

    /* restore: the serve run after the tests reads the value it was started with */
    CHECK(SetEnvironmentVariableA("TAPE2_SEASON_ROOT", prev[0] ? prev : NULL));
    vehn_reset();
    sa_rv_nm_have = 0;
    r1a_driver = 0;
    sa_rv_bufs_free(&B);
}

/* ---------- TAPE2 RT-E (Claude 2026-10-15): a remote human is not AI in /replay/frame ----------
 * Car 2 is not the player. Channel 98 = 2 (remote) on car 2 and = 1 (AI) on car 1. At 10 s the frame must give car 2
 * "cr":2 and "a":0 (storage_api.inc sa_rv_tape_map fix after RV_I(98)) and car 1 "cr":1 and "a":1. Without the fix car 2
 * keeps ai_control 1 and reads "a":1. Names come from an empty season folder (no game files are read). */
static sa_rev_slot_t rte_slot;
static char rte_frame[1u << 20];
static char rte_seg[1u << 16];

/* the car object of frame car `id`: from its "id":N,"l": key to the next car key, or to the review block */
static const char *rte_car(const char *buf, unsigned id) {
    char a[32], z[32];
    const char *p, *q;
    size_t n;
    snprintf(a, sizeof a, "\"id\":%u,\"l\":", id);
    snprintf(z, sizeof z, "\"id\":%u,\"l\":", id + 1u);
    p = strstr(buf, a);
    if (!p) return NULL;
    q = strstr(p + 1, z);
    if (!q) q = strstr(p + 1, ",\"review\":");
    n = q ? (size_t)(q - p) : strlen(p);
    if (n + 1 > sizeof rte_seg) return NULL;
    memcpy(rte_seg, p, n);
    rte_seg[n] = '\0';
    return rte_seg;
}

static void rte_test_remote_frame(void) {
    char tmp[MAX_PATH], dir[1024], sd[1024], veh[1024], prev[MAX_PATH], rid[24];
    DWORD n, pl;
    r1a_src_t mem;
    sv2_rd_t rd;
    sv2_header_t hdr;
    sa_rv_bufs_t B;
    sv2_idx_t *ix = &rte_slot.ix;
    sa_buf_t b;
    const sa_rv_nm_t *nmt;
    int32_t pos[SV2_MAX_CARS];
    uint8_t pos_ok[SV2_MAX_CARS], ai[SV2_MAX_CARS], ai_ok[SV2_MAX_CARS];
    int r;

    n = GetTempPathA(sizeof tmp, tmp);
    CHECK(n > 0 && n < sizeof tmp);
    snprintf(dir, sizeof dir, "%sTAPE2_rte_season\\", tmp);
    snprintf(sd, sizeof sd, "%sTAPE2_rte_season\\SEASONDATA\\", tmp);
    snprintf(veh, sizeof veh, "%sTAPE2_rte_season\\SEASONDATA\\Vehicles\\", tmp);
    b2_mkdir(dir);
    b2_mkdir(sd);
    b2_mkdir(veh);
    prev[0] = '\0';
    pl = GetEnvironmentVariableA("TAPE2_SEASON_ROOT", prev, sizeof prev);
    if (pl >= sizeof prev) prev[0] = '\0';
    CHECK(SetEnvironmentVariableA("TAPE2_SEASON_ROOT", sd));
    vehn_reset();
    sa_rv_nm_have = 0;

    r1a_driver = 1;
    r1a_remote2 = 1;
    r1a_build_file();
    mem.h = INVALID_HANDLE_VALUE;
    mem.mem = r1a_buf;
    mem.mem_len = r1a_len;
    mem.use_file = 0;
    rd.read_at = r1a_read;
    rd.ctx = &mem;
    rd.len = (uint64_t)r1a_len;
    CHECK(sa_rv_bufs_alloc(&B));
    sa_review_id("Monza", "rte_remote_car2", rid, sizeof rid);
    sv2_idx_init(ix, rte_slot.blocks, SA_REV_BLOCKS, rte_slot.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    CHECK(r == SV2_OK && ix->complete == 1);
    CHECK(sv2_rd_header(&rd, &hdr) == SV2_OK);
    memset(pos, 0, sizeof pos); memset(pos_ok, 0, sizeof pos_ok);
    memset(ai, 0, sizeof ai); memset(ai_ok, 0, sizeof ai_ok);
    sa_rv_scan_cars(ix, &rd, &B, pos, pos_ok, ai, ai_ok);
    nmt = sa_rv_names(rid, "rte_synthetic", ix, &rd, &B);
    b.p = rte_frame; b.cap = sizeof rte_frame; b.n = 0; b.bad = 0;
    CHECK(sa_rv_frame_tape(&b, ix, &rd, &B, &hdr, rid, "rte_synthetic", 10.0, 0.0, 57.95, nmt) == 1);
    CHECK(!b.bad);
    r1b_dump("rte_frame_t10.json", rte_frame, b.n);
    CHECK(rte_car(rte_frame, 1u) != NULL);
    CHECK(strstr(rte_seg, "\"cr\":1,") != NULL && strstr(rte_seg, "\"a\":1,") != NULL);
    CHECK(rte_car(rte_frame, 2u) != NULL);
    CHECK(strstr(rte_seg, "\"cr\":2,") != NULL && strstr(rte_seg, "\"a\":0,") != NULL);
    printf("RT-E remote human in /replay/frame at 10 s: car 2 cr 2 a 0 (remote), car 1 cr 1 a 1 (AI): PASS\n");

    r1a_remote2 = 0;
    r1a_driver = 0;
    CHECK(SetEnvironmentVariableA("TAPE2_SEASON_ROOT", prev[0] ? prev : NULL));
    vehn_reset();
    sa_rv_nm_have = 0;
    sa_rv_bufs_free(&B);
}

/* R1-C (2026-10-14): serve mode lists the storage root the last test left behind. That root was the b4api fixture
 * (100-byte zero placeholders), so the review route answered SV2_R_HEADER for a file that is not a recording. Serve a
 * real v2 recording instead: the R1-A synthetic file this run wrote to the R1_build folder under TEMP. Test fixture only. */
static void serve_real_recording(void) {
    char base[MAX_PATH], game[MAX_PATH], src[MAX_PATH + 64], dir[MAX_PATH + 64], dst[MAX_PATH + 64], why[160], tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof tmp, tmp);
    CHECK(n > 0 && n < sizeof tmp);
    /* R1-C review harness (2026-10-14): RECREV_ROOT serves a copied owner recordings root instead of the synthetic fixture. */
    const char *rr = getenv("RECREV_ROOT");
    if (rr && rr[0]) {
        char root[MAX_PATH], why2[160];
        size_t rl = strlen(rr);
        CHECK(rl > 0 && rl < MAX_PATH - 2);
        sw_fmt(root, sizeof root, "%s%s", rr, rr[rl - 1] == '\\' ? "" : "\\");
        CHECK(sw_root_init(root, 6, 2, why2, sizeof why2) == SW_OK);
        sw_enabled = 1;
        sa_init();
        sw_nentries = 0;
        sw_overflow_bytes = 0;
        sw_scan_folders();
        sa_snapshot_rebuild();
        return;
    }
    b2_base(base, sizeof base, "r1serve");
    sw_fmt(game, sizeof game, "%sgame\\", base);
    b2_mkdir(game);
    CHECK(sw_root_init(game, 6, 2, why, sizeof why) == SW_OK);
    sw_enabled = 1;
    sa_init();
    sw_fmt(dir, sizeof dir, "%sRec_R1", sw_rec_dir);
    b2_mkdir(dir);
    sw_fmt(src, sizeof src, "%sR1_build\\r1a_synthetic.f1t", tmp);
    sw_fmt(dst, sizeof dst, "%sRec_R1\\S1.f1t", sw_rec_dir);
    CHECK(CopyFileA(src, dst, FALSE));
    sw_nentries = 0;
    sw_overflow_bytes = 0;
    sw_scan_folders();
    sa_snapshot_rebuild();
}

/* ---------- REPLAY 2.1 (2026-10-14): /replay/info, /replay window and the driver EVENT reader (memory reader) ---------- */
static sa_rev_slot_t r21_slot;
static char r21_out[1u << 20];

static size_t r21_count(const char *s, const char *needle) { return r1b_count(s, needle); }

static void r21_test_replay(void) {
    r1a_src_t mem;
    sv2_rd_t rd;
    sv2_header_t hdr;
    sa_rv_bufs_t B;
    sv2_idx_t *ix = &r21_slot.ix;
    sa_buf_t b;
    char rid[24];
    const char *why = "";
    double w0 = 0.0, w1 = 0.0;
    const sa_rv_nm_t *nmt = NULL;
    int r;

    r1a_driver = 1;
    r1a_build_file();
    mem.h = INVALID_HANDLE_VALUE;
    mem.mem = r1a_buf;
    mem.mem_len = r1a_len;
    mem.use_file = 0;
    rd.read_at = r1a_read;
    rd.ctx = &mem;
    rd.len = (uint64_t)r1a_len;
    CHECK(sa_rv_bufs_alloc(&B));
    sa_review_id("Monza", "r21_synthetic", rid, sizeof rid);
    sv2_idx_init(ix, r21_slot.blocks, SA_REV_BLOCKS, r21_slot.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    CHECK(r == SV2_OK && ix->complete == 1 && ix->nblocks == R1A_NBLK);
    CHECK(sv2_rd_header(&rd, &hdr) == SV2_OK);

    /* /replay/info: the driver EVENT names slot 0 (with its team), slots 1 and 2 keep "Car N"; the 2 pit EVENTs and
     * the 3 lap EVENTs of car 0 are listed, the driver EVENT is not; the outline of one timed lap is not empty. */
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    nmt = sa_rv_names(rid, "r21_synthetic", ix, &rd, &B);   /* no year in this stem: race numbers stay "#num" */
    CHECK(sa_rv_info_doc(&b, ix, &rd, &B, &hdr, rid, "r21_synthetic", nmt) == 1);
    CHECK(strstr(r21_out, "\"name\":\"Alex Test\"") != NULL);
    CHECK(strstr(r21_out, "\"team\":\"Team A\"") != NULL);
    CHECK(strstr(r21_out, "\"name\":\"#8\"") != NULL && strstr(r21_out, "\"name\":\"#18\"") != NULL);   /* TAPE2 N1: race numbers */
    CHECK(strstr(r21_out, "\"slot\":0,\"name\":\"Alex Test\",\"team\":\"Team A\",\"num\":7,\"role\":\"remote\"") != NULL);   /* remote at any tick */
    CHECK(strstr(r21_out, "\"name\":\"#8\",\"team\":\"\",\"num\":8,\"role\":\"ai\"") != NULL);
    CHECK(r21_count(r21_out, "\"type\":\"pit\"") == 2);
    CHECK(r21_count(r21_out, "\"type\":\"lap\"") == 3);   /* the R1-A fixture writes lap EVENTs for car 0 only */
    CHECK(strstr(r21_out, "\"type\":\"driver\"") == NULL);
    CHECK(strstr(r21_out, "\"outline\":[[") != NULL);
    CHECK(strstr(r21_out, "\"hz\":20.") != NULL);
    printf("REPLAY 2.1 info: %lu bytes, 2 pit and 3 lap events, driver names read\n", (unsigned long)b.n);

    /* /replay window 10 s .. 20 s at 5 Hz. Car 0 at 10.000 s: x 2000.0, z 500.0, lap 1, other columns absent (null). */
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_window_doc(&b, ix, &rd, &B, rid, 10.0, 20.0) == 1);
    CHECK(strstr(r21_out, "\"t0\":10.000,\"t1\":20.000") != NULL);
    CHECK(strstr(r21_out, "[10.000,2000.0,500.0,60.0,1,1,null,null,null,null,null,null]") != NULL);
    CHECK(strstr(r21_out, "[19.800,") != NULL);   /* on the 5 Hz grid */
    CHECK(strstr(r21_out, "[20.000,") != NULL);   /* the last tick of the window is always listed */
    CHECK(strstr(r21_out, "[19.950,") == NULL);   /* 19.85..19.95 are off the grid and not last: not listed */
    CHECK(strstr(r21_out, "[19.900,") == NULL);
    CHECK(strstr(r21_out, "\"cols\":[\"t\",\"x\",\"z\",\"spd\",\"pos\",\"lap\",\"pit\",\"gear\",\"thr\",\"brk\",\"dist\",\"fin\"]") != NULL);
    printf("REPLAY 2.1 window: %lu bytes for 3 cars, 10 s at 5 Hz (car 0 row at 10.000 s checked)\n", (unsigned long)b.n);

    /* REPLAY 2.2 / TAPE 2.2: review /track, /pits, /incidents and /driver documents from the file */
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_track_doc(&b, ix, &rd, &B) == 1);
    CHECK(strstr(r21_out, "{\"wc\":") == r21_out && strstr(r21_out, "\"pts\":[[") != NULL);
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_pits_doc(&b, ix, &rd, &B, rid, nmt) == 1);
    CHECK(strstr(r21_out, "\"id\":0,\"n\":\"Alex Test\",\"li\":1,\"lo\":2,") != NULL);
    CHECK(strstr(r21_out, "\"done\":1,\"t\":10.000}") != NULL);   /* every /pits item carries t */
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_incidents_doc(&b, ix, &rd, &B, rid, nmt) == 1);
    CHECK(strstr(r21_out, "\"ty\":\"contact\"") != NULL && strstr(r21_out, "\"a\":2,\"b\":0,\"t\":5.500}") != NULL);
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_driver_doc(&b, ix, &rd, &B, 0, 20.0, nmt) == 1);
    CHECK(strstr(r21_out, "\"id\":0,\"n\":\"Alex Test\",\"cn\":\"Team A\"") != NULL);
    CHECK(strstr(r21_out, "\"l\":1,\"t\":") != NULL && strstr(r21_out, "\"l\":2,\"t\":") != NULL);
    CHECK(strstr(r21_out, "\"pits\":[{\"li\":1,\"lo\":2,") != NULL && strstr(r21_out, "\"t\":10.000") == NULL);
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    CHECK(sa_rv_driver_doc(&b, ix, &rd, &B, 0, 11.0, nmt) == 1);
    CHECK(strstr(r21_out, "\"laps\":[]") != NULL && strstr(r21_out, "\"pits\":[{\"li\":1,\"lo\":0,") != NULL);
    printf("REPLAY 2.2 review docs: track, pits (t), incidents (t), driver at 11 s and 20 s: PASS\n");

    /* arguments: clamp to the span; 400 for a window over 30 s, for t0 >= t1 and for non-finite values */
    CHECK(sa_rv_window_args("-5", "3", 0.0, 59.95, &w0, &w1, &why) == 0 && w0 == 0.0 && w1 == 3.0);
    CHECK(sa_rv_window_args("58", "70", 0.0, 59.95, &w0, &w1, &why) == 0 && w0 == 58.0 && fabs(w1 - 59.95) < 1e-9);
    CHECK(sa_rv_window_args("0", "31", 0.0, 59.95, &w0, &w1, &why) == 400 && strcmp(why, "window too long") == 0);
    CHECK(sa_rv_window_args("5", "5", 0.0, 59.95, &w0, &w1, &why) == 400);
    CHECK(sa_rv_window_args("nan", "3", 0.0, 59.95, &w0, &w1, &why) == 400);
    CHECK(sa_rv_window_args("1e400", "3", 0.0, 59.95, &w0, &w1, &why) == 400);
    CHECK(sa_rv_window_args(NULL, "3", 0.0, 59.95, &w0, &w1, &why) == 400);
    CHECK(sa_rv_window_args("0", "30", 0.0, 59.95, &w0, &w1, &why) == 0);

    /* REPLAY 2.3 / TAPE 2.3: /replay/frame at 10, 12 and 25 s (positions, laps, speed and x of the written ticks) */
    {
        static char frame[1u << 20];
        sa_buf_t fb;
        fb.p = frame; fb.cap = sizeof frame; fb.n = 0; fb.bad = 0;
        CHECK(sa_rv_frame_tape(&fb, ix, &rd, &B, &hdr, rid, "r21_synthetic", 10.0, 0.0, 57.95, nmt) == 1);
        CHECK(strstr(frame, "\"id\":0,\"l\":1,\"p\":1,\"n\":\"Alex Test\"") != NULL);
        CHECK(strstr(frame, "\"id\":1,\"l\":2,\"p\":2,\"n\":\"#8\"") != NULL);
        CHECK(strstr(frame, "\"mx\":2000.0,\"mz\":500.0") != NULL);
        CHECK(strstr(frame, "\"s\":60.0,") != NULL);
    CHECK(strstr(frame, "\"bl\":0.000,\"ll\":98.500,\"tt\":0.000") != NULL);   /* no lap EVENT yet; ll is the f32 channel 86 */
        CHECK(strstr(frame, "\"tape\":{\"session\":\"") != NULL);
        CHECK(strstr(frame, "\"t\":10.000,\"t_first\":0.000,\"t_last\":57.950}") != NULL);
        CHECK(strstr(frame, "\"review\":{\"session\":") != NULL);
        CHECK(strstr(frame, "\"session_generation\":0,") != NULL);   /* Claude review: gen 0, as the review trace docs */
        r1b_dump("r21_frame_t10.json", frame, fb.n);
        fb.p = frame; fb.cap = sizeof frame; fb.n = 0; fb.bad = 0;
        CHECK(sa_rv_frame_tape(&fb, ix, &rd, &B, &hdr, rid, "r21_synthetic", 12.0, 0.0, 57.95, nmt) == 1);
        r1b_dump("r21_frame_t12.json", frame, fb.n);
        CHECK(strstr(frame, "\"id\":0,\"l\":2,\"p\":1") != NULL && strstr(frame, "\"mx\":2199.9,") != NULL);
        CHECK(strstr(frame, "\"bl\":12.000,\"ll\":98.500,\"tt\":12.000") != NULL);   /* lap EVENT at 12 s */
        CHECK(strstr(frame, "\"pch\":{\"v\":1,\"rh\":[12.50,") != NULL);             /* f32 pch channel round trip */   /* Q16 quantisation: 0.3 m steps */
        fb.p = frame; fb.cap = sizeof frame; fb.n = 0; fb.bad = 0;
        CHECK(sa_rv_frame_tape(&fb, ix, &rd, &B, &hdr, rid, "r21_synthetic", 25.0, 0.0, 57.95, nmt) == 1);
        r1b_dump("r21_frame_t25.json", frame, fb.n);
        CHECK(strstr(frame, "\"bl\":6.000,\"ll\":98.500,\"tt\":18.000") != NULL);   /* best 6 s, last crossing at 18 s */
        CHECK(strstr(frame, "\"id\":0,\"l\":3,\"p\":1") != NULL && strstr(frame, "\"mx\":3699.9,") != NULL);
        /* clamp: the route clamps t to [t_first, t_last]; a frame at the start is the tick at 0 s */
        fb.p = frame; fb.cap = sizeof frame; fb.n = 0; fb.bad = 0;
        CHECK(sa_rv_frame_tape(&fb, ix, &rd, &B, &hdr, rid, "r21_synthetic", 0.0, 0.0, 57.95, nmt) == 1);
        CHECK(strstr(frame, "\"t\":0.000,\"t_first\":0.000") != NULL);
        printf("REPLAY 2.3 frame: 10 s, 12 s, 25 s and the start, %lu bytes at 10 s: PASS\n", (unsigned long)(size_t)strlen(frame));
    }

    /* old format: the same file without driver EVENT and without pit EVENT: cars named "Car N", team empty */
    r1a_driver = 0;
    r1a_build_file();
    mem.mem = r1a_buf;
    mem.mem_len = r1a_len;
    rd.len = (uint64_t)r1a_len;
    sv2_idx_init(ix, r21_slot.blocks, SA_REV_BLOCKS, r21_slot.laps, SA_REV_LAPS);
    CHECK(sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix) == SV2_OK && ix->complete == 1);
    b.p = r21_out; b.cap = sizeof r21_out; b.n = 0; b.bad = 0;
    sa_review_id("Monza", "r21_old", rid, sizeof rid);   /* its own review id: the name caches are per review id */
    nmt = sa_rv_names(rid, "r21_old", ix, &rd, &B);
    CHECK(sa_rv_info_doc(&b, ix, &rd, &B, &hdr, rid, "r21_old", nmt) == 1);
    CHECK(strstr(r21_out, "\"name\":\"Car 0\"") != NULL && strstr(r21_out, "\"name\":\"Car 1\"") != NULL);
    CHECK(strstr(r21_out, "\"team\":\"\"") != NULL);
    CHECK(r21_count(r21_out, "\"type\":\"pit\"") == 0);
    printf("REPLAY 2.1 old format: Car N names, no driver EVENT: PASS\n");
    sa_rv_bufs_free(&B);
}

/* REPLAY 2.3 golden (byte identity of the live /telemetry body, TAPE 2.3): a fixed 24-car state written to a file, so
 * the bytes of the build before the json_telemetry split can be compared with the bytes after it. Writes and returns. */
static int json_golden(const char *path) {
    static char out[1u << 21];
    int n, i;
    FILE *f;
    EnterCriticalSection(&g_telemetry_cs);
    g_telemetry.session_type = 7;
    g_telemetry.race_phase = 3;
    g_telemetry.race_active = 1;
    g_telemetry.race_active_raw = 3;
    g_telemetry.track_condition = 0.5f;
    g_telemetry.track_grip_factor = 0.97f;
    g_telemetry.on_wetness = 0.0f;
    g_telemetry.off_wetness = 0.25f;
    g_telemetry.total_laps = 58;
    g_telemetry.vehicle_count = 24;
    g_telemetry.race_mode = RACE_MODE_CHAMPIONSHIP;
    g_telemetry.player_index = 0;
    g_telemetry.player_slot = 0;
    g_telemetry.ai_difficulty = 92;
    g_telemetry.session_time = 123.456;
    g_telemetry.tick = 4242;
    g_telemetry.ambient_temp = 21.5f;
    g_telemetry.track_temp = 33.25f;
    g_telemetry.lap_len = 5300.0f;
    g_telemetry.sec1 = 1800.0f;
    g_telemetry.sec2 = 3500.0f;
    g_telemetry.track_len = 5300.0f;
    g_telemetry.seg_count = 6;
    g_telemetry.seg_len = 880.0f;
    g_telemetry.yellow_count = 1;
    g_telemetry.oil_temp_hud = 101.5f;
    g_telemetry.water_temp_hud = 88.25f;
    g_telemetry.player_engine_life_s = 1234.5;
    g_telemetry.fastest_lap_time = 91.234f;
    g_telemetry.fastest_lap_car = 2;
    g_telemetry.dotd_car = 3;
    g_telemetry.dotd_score = 17;
    snprintf(g_telemetry.track_name, sizeof g_telemetry.track_name, "%s", "Albert Park");
    for (i = 0; i < 6; i++) g_telemetry.seg_state[i] = i % 3 ? 2 * (i & 1) : 0;
    for (i = 0; i < 4; i++) {
        g_telemetry.brake_temp[i] = 300.0f + (float)i;
        g_telemetry.tread[i] = 0.9f - 0.01f * (float)i;
        g_telemetry.blister[i] = i;
    }
    for (i = 0; i < 24; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        memset(c, 0, sizeof *c);
        snprintf(c->name, NAME_LEN, "Driver %02d", i);
        snprintf(c->car_name, NAME_LEN, "Team %d", i % 5);
        c->position = i + 1;
        c->lap_number = 1 + i % 3;
        c->speed = 10.5f * (float)i + 3.5f;
        c->rpm = 9000.0f + (float)i;
        c->max_rpm = 18000.0f;
        c->gear = 1 + i % 7;
        c->throttle = 0.5f;
        c->brake = 0.0f;
        c->lap_dist = 100.0f * (float)i;
        c->track_x = 1000.0f + 10.0f * (float)i;
        c->track_z = 500.0f + 3.0f * (float)i;
        c->phys_x = c->track_x;
        c->phys_z = c->track_z;
        c->position = i + 1;
        c->race_num = i + 1;
        c->controller_role = i == 3 ? 2 : (i == 0 ? 0 : 1);
        c->ai_control = i == 0 ? 0 : 1;
        c->on_track = 1;
        c->car_valid = 1 | 2 | 4 | 32;
        c->tire_choice = i % 2;
        c->finish_state = 0;
        c->tire_wear_fl = 0.95f;
        c->tire_wear_fr = 0.94f;
        c->tire_wear_rl = 0.93f;
        c->tire_wear_rr = 0.92f;
        c->fuel_level = 40.0f - (float)i;
        c->gap_ahead_m = 12.5f;
        c->gap_behind_m = 7.25f;
        c->gap_ahead_id = i > 0 ? i - 1 : -1;
        c->gap_behind_id = i + 1;
    }
    LeaveCriticalSection(&g_telemetry_cs);
    capture_sequence = 0;   /* sample_age_ms reads GetTickCount only when a sample exists: keep the bytes stable */
    capture_generation = 3;
    n = json_telemetry(out, (int)sizeof out);
    f = fopen(path, "wb");
    if (!f) return 2;
    if (n > 0) fwrite(out, 1, (size_t)n, f);
    fclose(f);
    printf("GOLDEN %d bytes -> %s\n", n, path);
    return n > 0 ? 0 : 1;
}

/* TAPE 2.5 timing (real files): replay-bench <file.f1t> <t>. Builds the index of the file, then runs the review builders
 * the HTTP routes call (info, windows, frames, track, pits, incidents, driver) on the file itself. The documents go to
 * %TEMP%\REPLAY_bench\; each step prints its time in ms and its size in bytes. Read-only: the file is opened for reading. */
static int bench_step(const char *name, int code, double ms, size_t bytes) {
    printf("BENCH %-22s code=%d ms=%.2f bytes=%lu\n", name, code, ms, (unsigned long)bytes);
    return code;
}

static double bench_ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart;
}

static void bench_dump(const char *name, const char *body, size_t n) {
    char dir[MAX_PATH], path[MAX_PATH + 96];
    FILE *fp;
    GetTempPathA(sizeof dir, dir);
    snprintf(path, sizeof path, "%sREPLAY_bench\\%s", dir, name);
    fp = fopen(path, "wb");
    if (!fp) return;
    fwrite(body, 1, n, fp);
    fclose(fp);
}

static int replay_bench(const char *file, double tq) {
    static char out[1u << 21];
    static sa_rev_slot_t bs;
    static r1a_src_t src;
    sv2_rd_t rd;
    sv2_header_t hdr;
    sa_rv_bufs_t B;
    sv2_idx_t *ix = &bs.ix;
    sa_buf_t b;
    LARGE_INTEGER f, a, z;
    LARGE_INTEGER fs;
    char dir[MAX_PATH];
    double tf = 0.0, tl = 0.0, ms;
    unsigned long long nt = 0;
    int r, k;
    const char *rid = "v2_0000000000000001";
    char stem[300];   /* TAPE2 N1: the file's own stem (it names the season year) */
    {
        const char *slash = strrchr(file, '\\');
        const char *base = slash ? slash + 1 : file;
        size_t L = strlen(base);
        if (L > 4 && !strcmp(base + L - 4, ".f1t")) L -= 4;
        if (L >= sizeof stem) L = sizeof stem - 1;
        memcpy(stem, base, L);
        stem[L] = '\0';
    }
    setup();
    GetTempPathA(sizeof dir, dir);
    {
        char mk[MAX_PATH + 32];
        snprintf(mk, sizeof mk, "%sREPLAY_bench", dir);
        CreateDirectoryA(mk, NULL);
    }
    QueryPerformanceFrequency(&f);
    src.h = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (src.h == INVALID_HANDLE_VALUE) { printf("BENCH cannot open %s\n", file); return 2; }
    GetFileSizeEx(src.h, &fs);
    src.use_file = 1; src.mem = NULL; src.mem_len = 0;
    rd.read_at = r1a_read;
    rd.ctx = &src;
    rd.len = (uint64_t)fs.QuadPart;
    if (!sa_rv_bufs_alloc(&B)) return 3;
    QueryPerformanceCounter(&a);
    sv2_idx_init(ix, bs.blocks, SA_REV_BLOCKS, bs.laps, SA_REV_LAPS);
    r = sv2_index_build_rd(&rd, &B.d, B.stored, B.work, SV2_MAX_PAYLOAD, ix);
    QueryPerformanceCounter(&z);
    printf("BENCH index: reason=%d complete=%d blocks=%u laps=%u file=%llu bytes\n", r, ix->complete, ix->nblocks, ix->nlaps,
           (unsigned long long)rd.len);
    bench_step("index", r, bench_ms(a, z, f), (size_t)rd.len);
    if (sv2_rd_header(&rd, &hdr) != SV2_OK) return 4;
    sa_rv_span(ix, &tf, &tl, &nt);
    QueryPerformanceCounter(&a);
    const sa_rv_nm_t *nmt = sa_rv_names(rid, stem, ix, &rd, &B);
    QueryPerformanceCounter(&z);
    bench_step("review names (first call per id)", 0, bench_ms(a, z, f), 0);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++)
        if (ix->car[s].seen)
            printf("BENCH name slot=%u name=%s team=%s num=%ld src=%u\n", s, nmt[s].name, nmt[s].team, (long)nmt[s].num, (unsigned)nmt[s].src);
    printf("BENCH span: t_first=%.3f t_last=%.3f ticks=%llu hz=%.2f\n", tf, tl, nt, (tl > tf) ? (double)nt / (tl - tf) : 0.0);
    if (tq < tf) tq = tf;
    if (tq > tl) tq = tl;

    b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
    QueryPerformanceCounter(&a);
    r = sa_rv_info_doc(&b, ix, &rd, &B, &hdr, rid, stem, nmt) ? 200 : 500;
    QueryPerformanceCounter(&z);
    ms = bench_ms(a, z, f);
    bench_step("replay/info", r, ms, b.n);
    bench_dump("info.json", out, b.n);

    {   /* three windows of 10 s: the start, the middle and the end of the recording */
        static const double w0s[3] = { 0.0, 0.5, 0.9 };
        for (k = 0; k < 3; k++) {
            double t0 = tf + w0s[k] * (tl - tf);
            b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
            QueryPerformanceCounter(&a);
            r = sa_rv_window_doc(&b, ix, &rd, &B, rid, t0, t0 + 10.0 > tl ? tl : t0 + 10.0) ? 200 : 500;
            QueryPerformanceCounter(&z);
            ms = bench_ms(a, z, f);
            bench_step("replay window 10s", r, ms, b.n);
            if (k == 0) bench_dump("window0.json", out, b.n);
        }
    }
    {   /* frames at the start, the middle and the end, and at tq */
        static const double fs_[4] = { 0.0, 0.5, 0.9, 1.0 };
        for (k = 0; k < 4; k++) {
            double t = tf + fs_[k] * (tl - tf);
            b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
            QueryPerformanceCounter(&a);
            r = sa_rv_frame_tape(&b, ix, &rd, &B, &hdr, rid, stem, t, tf, tl, nmt) ? 200 : 500;
            QueryPerformanceCounter(&z);
            ms = bench_ms(a, z, f);
            bench_step("replay/frame", r, ms, b.n);
            if (k == 2) bench_dump("frame_mid.json", out, b.n);
        }
        b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
        QueryPerformanceCounter(&a);
        r = sa_rv_frame_tape(&b, ix, &rd, &B, &hdr, rid, stem, tq, tf, tl, nmt) ? 200 : 500;
        QueryPerformanceCounter(&z);
        bench_step("replay/frame tq", r, bench_ms(a, z, f), b.n);
    }
    b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
    QueryPerformanceCounter(&a); r = sa_rv_track_doc(&b, ix, &rd, &B) ? 200 : 500; QueryPerformanceCounter(&z);
    bench_step("track", r, bench_ms(a, z, f), b.n); bench_dump("track.json", out, b.n);
    b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
    QueryPerformanceCounter(&a); r = sa_rv_pits_doc(&b, ix, &rd, &B, rid, nmt) ? 200 : 500; QueryPerformanceCounter(&z);
    bench_step("pits", r, bench_ms(a, z, f), b.n); bench_dump("pits.json", out, b.n);
    b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
    QueryPerformanceCounter(&a); r = sa_rv_incidents_doc(&b, ix, &rd, &B, rid, nmt) ? 200 : 500; QueryPerformanceCounter(&z);
    bench_step("incidents", r, bench_ms(a, z, f), b.n); bench_dump("incidents.json", out, b.n);
    b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
    QueryPerformanceCounter(&a); r = sa_rv_driver_doc(&b, ix, &rd, &B, 0u, tl, nmt) ? 200 : 500; QueryPerformanceCounter(&z);
    bench_step("driver car 0", r, bench_ms(a, z, f), b.n); bench_dump("driver0.json", out, b.n);
    sa_rv_bufs_free(&B);
    CloseHandle(src.h);
    return 0;
}

/* 1.2.4 (DVOA report 2026-10-17): replay holds a car after its newest tick (qualifying runs done). Pure rule; the
 * real-file check (EUROPE 2002 qualifying, 22 cars, held at t=3000 s, none at t=1000 s) is in the 1.2.4 record. */
/* 1.2.5 (DVOA report 2026-10-18): repeated in-lap game times, untimed laps listed, review lap flags. */
static void test_dvoa_recording_rules(void) {
    static sv2_idx_block_t blk[4];
    static sv2_idx_lap_t laps[16];
    static sv2_idx_t ix;
    static const char *ev[] = {
        "{\"type\":\"lap\",\"t\":100.000,\"car\":5,\"lap\":2,\"prev_lap\":1,\"last_lap_s\":96.903}",
        "{\"type\":\"lap\",\"t\":198.800,\"car\":5,\"lap\":3,\"prev_lap\":2,\"last_lap_s\":96.903}",
        "{\"type\":\"lap\",\"t\":295.000,\"car\":5,\"lap\":4,\"prev_lap\":3,\"last_lap_s\":96.100}",
        "{\"type\":\"lap\",\"t\":100.500,\"car\":6,\"lap\":2,\"prev_lap\":1,\"last_lap_s\":96.903}" };
    sv2_idx_init(&ix, blk, 4, laps, 16);
    for (unsigned i = 0; i < 4; i++) sv2_idx_apply_event(&ix, (const uint8_t *)ev[i], strlen(ev[i]));
    {
        const sv2_idx_lap_t *l1 = sv2_idx_lap_find(&ix, 5, 1), *l2 = sv2_idx_lap_find(&ix, 5, 2);
        const sv2_idx_lap_t *l3 = sv2_idx_lap_find(&ix, 5, 3), *o1 = sv2_idx_lap_find(&ix, 6, 1);
        CHECK(l1 && l1->has_game && fabs(l1->game_time - 96.903) < 1e-6);
        CHECK(!l2 || !l2->has_game);                       /* the repeat is the previous lap's time */
        CHECK(l3 && l3->has_game && fabs(l3->game_time - 96.100) < 1e-6);
        CHECK(o1 && o1->has_game && fabs(o1->game_time - 96.903) < 1e-6);  /* another car with the same time is kept */
    }
    {
        sv2_idx_lap_t r;
        memset(&r, 0, sizeof r);
        r.has_start = 1; r.start_sim = 100.0; r.end_sim = 198.8;
        CHECK(sv2_idx_lap_listed(&r));                     /* untimed full lap (in lap / out lap) is listed */
        r.end_sim = 110.0;
        CHECK(!sv2_idx_lap_listed(&r));                    /* a 10 s row is a counter change in the garage */
        r.has_game = 1;
        CHECK(sv2_idx_lap_listed(&r));
        r.has_start = 0;
        CHECK(!sv2_idx_lap_listed(&r));                    /* first sighting: no start */
    }
    {
        sw_ev_prev_t p;
        memset(&p, 0, sizeof p);
        CHECK(sw_event_lap_time(&p, -1.0f) == -1.0f);
        CHECK(fabsf(sw_event_lap_time(&p, 96.903f) - 96.903f) < 1e-4f);
        CHECK(sw_event_lap_time(&p, 96.903f) == -1.0f);    /* writer: a repeat is written as no game time */
        CHECK(fabsf(sw_event_lap_time(&p, 96.1f) - 96.1f) < 1e-4f);
        CHECK(sw_event_lap_time(&p, -1.0f) == -1.0f && fabsf(p.ll - 96.1f) < 1e-4f);
    }
    {   /* review status and lap docs: a timed lap is flags 1 and complete; an untimed one flags 3 and partial */
        static char out[4096];
        static sa_rv_nm_t nmt[SV2_MAX_CARS];
        uint8_t ai[SV2_MAX_CARS] = {0}, ai_ok[SV2_MAX_CARS] = {0};
        sa_buf_t b;
        sv2_idx_init(&ix, blk, 4, laps, 16);
        memset(laps, 0, sizeof laps);
        memset(nmt, 0, sizeof nmt);
        ix.car[2].seen = 1;
        laps[0].car = 2; laps[0].lap = 3; laps[0].has_start = 1; laps[0].start_sim = 10.0; laps[0].end_sim = 100.0;
        laps[0].has_game = 1; laps[0].game_time = 89.95; laps[0].samples = 50;
        laps[1].car = 2; laps[1].lap = 4; laps[1].has_start = 1; laps[1].start_sim = 100.0; laps[1].end_sim = 230.0;
        laps[1].samples = 70;
        laps[2].car = 2; laps[2].lap = 5; laps[2].has_start = 1; laps[2].start_sim = 230.0; laps[2].end_sim = 232.0;
        ix.nlaps = 3;
        b.p = out; b.cap = sizeof out; b.n = 0; b.bad = 0;
        sa_rv_status_doc(&b, &ix, "v2_test", ai, ai_ok, nmt);
        CHECK(!b.bad);
        CHECK(strstr(out, "\"lap\":3,\"t\":89.950,\"samples\":50,\"flags\":1,") != NULL);
        CHECK(strstr(out, "\"lap\":4,\"t\":130.000,\"samples\":70,\"flags\":3,") != NULL);
        CHECK(strstr(out, "\"lap\":5,") == NULL);
        CHECK(strstr(out, "\"total_samples\":120,") != NULL);
    }
    printf("PASS 1.2.5 recording rules (repeated in-lap time, untimed laps, review lap flags)\n");
}

static void test_replay_hold_rule(void) {
    CHECK(sa_rv_hold_due(1, 0, 1300.0, 3000.0) == 1);
    CHECK(sa_rv_hold_due(1, 1, 1300.0, 3000.0) == 0);   /* already in the snapshot */
    CHECK(sa_rv_hold_due(0, 0, 0.0, 3000.0) == 0);      /* never seen */
    CHECK(sa_rv_hold_due(1, 0, 1300.0, 1000.0) == 0);   /* before its last tick: a gap, not held */
    printf("PASS replay holds cars after their last tick\n");
}

/* 1.2.3 (owner CO-OP report 2026-10-17): remote lap seed and session reset. */
static void test_remote_lap_reset(void) {
    CHECK(remote_seed_lap(0, 0) == 1 && remote_seed_lap(5, 0) == 5);
    CHECK(remote_seed_lap(3, 4) == 3 && remote_seed_lap(5, 4) == 5);   /* up to the field +1 */
    CHECK(remote_seed_lap(14, 2) == 2 && remote_seed_lap(0, 3) == 3);  /* stale qualifying count refused */
    g_rem_lap[5] = 17; g_rem_best[5] = 88.0f; g_rem_last[5] = 90.0f; g_rem_lap_t0[5] = 500.0; g_rem_game_seen[5] = 1;
    remote_session_reset();
    CHECK(g_rem_lap[5] == 0 && g_rem_best[5] == 0.0f && g_rem_last[5] == 0.0f && g_rem_lap_t0[5] == 0.0 &&
          !g_rem_game_seen[5] && g_rem_wp[5] == -1);
    printf("PASS CO-OP remote lap seed and session reset\n");
}

/* 1.2.3 (DVOA report 2026-10-17): the lap ring never overwrites the fastest clean lap. */
static void test_trace_keep_best(void) {
    static trace_car_t t;
    memset(&t, 0, sizeof t);
    CHECK(trace_best_slot(&t) == -1 && trace_next_head(&t, 0) == 1);
    t.lap_valid[2] = TRL_COMPLETE; t.lap_time[2] = 90.0f;
    t.lap_valid[3] = TRL_COMPLETE; t.lap_time[3] = 95.0f;
    t.lap_valid[4] = TRL_COMPLETE | TRL_BREAKS; t.lap_time[4] = 80.0f;   /* broken lap is not protected */
    t.lap_valid[5] = TRL_COMPLETE | TRL_PITTED; t.lap_time[5] = 70.0f;   /* pit lap is not protected */
    CHECK(trace_best_slot(&t) == 2);
    CHECK(trace_next_head(&t, 1) == 3);                       /* skips the fastest */
    CHECK(trace_next_head(&t, 2) == 3 && trace_next_head(&t, 3) == 4 && trace_next_head(&t, 6) == 0);
    t.lap_valid[0] = TRL_COMPLETE; t.lap_time[0] = 85.0f;     /* a new fastest moves the protection */
    CHECK(trace_best_slot(&t) == 0 && trace_next_head(&t, 6) == 1 && trace_next_head(&t, 1) == 2);
    printf("PASS lap ring keeps the fastest clean lap\n");
}

/* 1.2.2 (DVOA report 2026-10-17): broker quarantine self-heal and the TV Director native budget. Mock broker only. */
static unsigned rq_register_calls, rq_budget_calls;
static F1CBrokerResult rq_register_result;
static F1CBrokerToken rq_budget_token;
static F1CBrokerResult __cdecl rq_register(HMODULE m, const char *name, F1CBrokerToken *out) {
    (void)m; ++rq_register_calls;
    if (rq_register_result == F1C_BROKER_OK) { CHECK(name && !strcmp(name, "web_telemetry")); *out = 42u; }
    return rq_register_result;
}
static F1CBrokerResult __cdecl rq_budget(F1CBrokerToken token) { ++rq_budget_calls; rq_budget_token = token; return F1C_BROKER_OK; }
static void test_broker_quarantine_reregister(void) {
    DWORD last = 0;
    F1CBrokerRegisterFn saved = telemetry_broker.register_client;
    F1CBrokerToken saved_token = telemetry_token;
    telemetry_broker.register_client = rq_register;
    telemetry_token = 7u; telemetry_reregisters = 0;
    InterlockedExchange(&capture_pending, 1);
    g_dcam_watch.posted = 1;
    rq_register_result = F1C_BROKER_OK;
    telemetry_reregister(&last);
    CHECK(rq_register_calls == 1 && telemetry_token == 42u && telemetry_reregisters == 1);
    CHECK(capture_pending == 0 && g_dcam_watch.posted == 0 && last != 0);
    telemetry_reregister(&last);                       /* within 2 s: throttled */
    CHECK(rq_register_calls == 1);
    last = GetTickCount() - 2500u;
    rq_register_result = F1C_BROKER_NO_CLIENT_SLOT;    /* failed register keeps the token */
    telemetry_reregister(&last);
    CHECK(rq_register_calls == 2 && telemetry_token == 42u && telemetry_reregisters == 1);
    /* native budget: only when bound, with the current token */
    telemetry_recovery_ok = 0; telemetry_recovery.native_budget = rq_budget;
    telemetry_native_budget();
    CHECK(rq_budget_calls == 0);
    telemetry_recovery_ok = 1;
    telemetry_native_budget();
    CHECK(rq_budget_calls == 1 && rq_budget_token == 42u);
    telemetry_recovery_ok = 0; memset(&telemetry_recovery, 0, sizeof telemetry_recovery);
    telemetry_broker.register_client = saved; telemetry_token = saved_token; telemetry_reregisters = 0;
    printf("PASS broker quarantine self-heal (register again, 2 s throttle, failed register, native budget)\n");
}

int main(int argc,char **argv) {
    if (argc>3 && !strcmp(argv[1],"replay-bench")) return replay_bench(argv[2], atof(argv[3]));
    if (argc>2 && !strcmp(argv[1],"json-golden")) {
        setup(); return json_golden(argv[2]);
    }
    if (argc>1 && !strcmp(argv[1],"corpus")) {
        setup(); return emit_corpus(argc>2?argv[2]:".");
    }
    if (argc>1 && !strcmp(argv[1],"import-test")) {
        setup(); if(argc<3) return 2; return run_import_test(argv[2]);
    }
    if (argc>1 && !strcmp(argv[1],"fuzz")) {
        setup(); return run_fuzz(atoi(argv[2]),atoi(argv[3]),argc>4?argv[4]:NULL);
    }
    if (argc>1 && !strcmp(argv[1],"soak")) {
        setup(); return run_soak(atoi(argv[2]),atoi(argv[3]));
    }
    if (argc>1 && !strcmp(argv[1],"sizes")) {
        setup(); return run_sizes();
    }
    setup(); test_host(); test_copies(); test_chassis(); test_waypoints(); test_gdb_script();
    test_coop(); test_performance(); test_performance_allcars();
    test_trace_guards(); test_trace_lap0();
    test_trace_all_cars(); test_trace_select_keeps(); test_trace_alloc_fallback();
    test_lap_latch(); test_session_reset_rules(); test_trace_growth_keeps(); test_session_archive();
    test_hockenheim_sequences(); test_lifecycle();
    test_trace_retention(); test_trace();
    test_import(); test_audit_regressions(); test_weather_ring(); test_sidecar_integrity(); test_formatting();
    test_port_settings(); test_port_bind(); test_bind_policy(); test_send_progress();
    test_storage_v2();
    r1a_test_reader();
    r1b_test_adapter();
    r21_test_replay();
    n1_test_names();
    rte_test_remote_frame();
    test_storage_writer();
    test_director_phase1();
    test_director_phase2();
    test_broker_quarantine_reregister();
    test_trace_keep_best();
    test_remote_lap_reset();
    test_replay_hold_rule();
    test_dvoa_recording_rules();
    test_storage_jobs();
    test_recordings_api();
    if (argc>2) test_real_dll(argv[2]);
    printf("PASS %u checks\n",checks); fflush(stdout);
    printf("PERF synthetic capture+read p50=%.0fus p95=%.0fus max=%.0fus (native budget: p95<500us; synthetic includes 1.9MB mirror copy)\n",
        g_perf_p50,g_perf_p95,g_perf_max);
    printf("PERFG callback-only p50=%.1fus p95=%.1fus max=%.1fus (single-threaded by design)\n",
        g_cap_p50,g_cap_p95,g_cap_max);
    if (argc>1 && !strcmp(argv[1],"serve")) {
        serve_real_recording();
        serve_seed_archive();
        HANDLE thread=CreateThread(NULL,0,http_thread,NULL,0,NULL);
        CHECK(thread!=NULL);
        static char data[262144];
        for (int i=0;i<(getenv("TELEMETRY_PREVIEW")?1800:120);++i) {
            /* Synthetic advancing feed for the live-check and SSE freshness
             * gates: a new sample tick per transmit (sim time and sequence
             * advance; trace rings stay as seeded). */
            EnterCriticalSection(&g_telemetry_cs);
            g_telemetry.session_time += 0.1f;
            capture_time=GetTickCount(); ++capture_sequence;
            LeaveCriticalSection(&g_telemetry_cs);
            format_failed=0;
            int n=json_telemetry(data,sizeof data);
            CHECK(!format_failed);
            sse_broadcast(data,n); Sleep(100);
        }
        g_http_running=FALSE;
        CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0); CloseHandle(thread);
        serve_drop_archive();
    }
    return 0;
}
