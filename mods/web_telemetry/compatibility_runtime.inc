/* Only this callback may read live game memory. No game writes, native calls,
 * sockets, JSON formatting, disk IO, or blocking lock acquisition happen here. */
static void clear_mirrors(void) {
    memset(mirror_ADDR_VEHICLE_PTR_ARRAY.bytes, 0, sizeof mirror_ADDR_VEHICLE_PTR_ARRAY.bytes);
    memset(mirror_ADDR_VEHICLE_COUNT.bytes, 0, sizeof mirror_ADDR_VEHICLE_COUNT.bytes);
    memset(mirror_ADDR_RACE_ACTIVE.bytes, 0, sizeof mirror_ADDR_RACE_ACTIVE.bytes);
    memset(mirror_ADDR_AI_DB_BASE.bytes, 0, sizeof mirror_ADDR_AI_DB_BASE.bytes);
    memset(mirror_ADDR_STEWARD.bytes, 0, sizeof mirror_ADDR_STEWARD.bytes);
    memset(mirror_ADDR_AIW_PATH.bytes, 0, sizeof mirror_ADDR_AIW_PATH.bytes);
}

static int copy_indirect(uintptr_t owner, unsigned char *mirror, size_t count_off,
                         size_t pointer_off, unsigned limit, size_t stride, void *out) {
    int count; uintptr_t pointer=0;
    memcpy(&count, mirror+count_off, sizeof count);
    memcpy(&pointer, mirror+pointer_off, sizeof pointer);
    if (count<=0 || (unsigned)count>limit ||
        !copy_game(out,pointer,(size_t)count*stride)) {
        count=0; pointer=0;
    } else pointer=(uintptr_t)out;
    memcpy(mirror+count_off,&count,sizeof count);
    memcpy(mirror+pointer_off,&pointer,sizeof pointer);
    (void)owner;
    return count;
}

static void __cdecl capture_on_game_thread(void *unused) {
    F1CBrokerStats stats;
    int valid=1, count=0;
    uintptr_t pointers[MAX_CARS];
    LARGE_INTEGER ct0;
    (void)unused;
    QueryPerformanceCounter(&ct0);   /* BACKEND 4: native callback cost */
    memset(&stats,0,sizeof stats); stats.structure_size=sizeof stats;
    if (telemetry_broker.get_stats(&stats)!=F1C_BROKER_OK ||
        stats.hook_thread_id!=GetCurrentThreadId()) goto done;
    if (!TryEnterCriticalSection(&g_telemetry_cs)) goto done;
    for (size_t n=0;n<sizeof mirror_regions/sizeof mirror_regions[0];++n) {
        MirrorRegion *r=&mirror_regions[n];
        if (!copy_game(r->bytes,r->address,r->size)) { valid=0; break; }
    }
    if (valid) {
        memcpy(&count,mirror_ADDR_VEHICLE_COUNT.bytes,sizeof count);
        if (count<0 || count>MAX_CARS) valid=0;
    }
    if (valid && stats.session_active) {
        memcpy(pointers,mirror_ADDR_VEHICLE_PTR_ARRAY.bytes,sizeof pointers);
        for (int i=0;i<MAX_CARS;++i) {
            uintptr_t current=i<count?pointers[i]:0;
            if (last_roster[i] && last_roster[i]!=current) roster_changed=1;
            last_roster[i]=current;
        }
        memset(mirror_ADDR_VEHICLE_PTR_ARRAY.bytes,0,sizeof pointers);
        for (int i=0;i<count;++i) {
            uintptr_t address=pointers[i], local=0;
            /* The reviewed array contains interior-free records in the fixed
             * SDK vehicle pool. Never follow arbitrary/noncanonical pointers. */
            if (address>=VEH_BASE && address<VEH_BASE+(uintptr_t)VEH_STRIDE*VEH_SLOTS &&
                (address-VEH_BASE)%VEH_STRIDE==0 &&
                copy_game(mirror_vehicles[i].bytes,address,VEH_STRIDE))
                local=(uintptr_t)mirror_vehicles[i].bytes;
            else ++failed_captures;
            memcpy(mirror_ADDR_VEHICLE_PTR_ARRAY.bytes+i*sizeof local,&local,sizeof local);
        }
        /* The path is a fixed-size copied string; never strlen live memory. */
        mirror_ADDR_AIW_PATH.bytes[sizeof mirror_ADDR_AIW_PATH.bytes-1]=0;
        copy_indirect(GAME_ADDR_AI_DB_BASE,mirror_ADDR_AI_DB_BASE.bytes,
            OFF_WP_COUNT,OFF_WP_ARRAY,MAX_WAYPOINTS,WP_STRIDE,mirror_waypoints.bytes);
        copy_indirect(GAME_ADDR_STEWARD,mirror_ADDR_STEWARD.bytes,
            STW_OFF_SEG_COUNT,STW_OFF_SEG_ARRAY,MAX_SEGMENTS,8,mirror_flags.bytes);
    } else {
        clear_mirrors();
        if (!valid) ++failed_captures;
    }
    capture_generation=stats.session_generation;
    capture_time=GetTickCount(); ++capture_sequence;
    dcam_capture_locked(valid && stats.session_active);   /* TV Director phase 0: guarded reads only */
    LeaveCriticalSection(&g_telemetry_cs);
done:
    capture_cost_record(ct0);        /* BACKEND 4: record before the latch clears */
    InterlockedExchange(&capture_pending,0);
}

static void update_queue_generation(unsigned generation,unsigned *previous) {
    if (generation!=*previous) {
        *previous=generation;
        InterlockedExchange(&capture_pending,0);
    }
}

/* C15: reviewed team identity (ABI 8 hash) and controller role from the
 * broker's own copied snapshots. Broker calls are made WITHOUT holding
 * g_telemetry_cs (the broker has its own lock; the capture callback takes
 * g_telemetry_cs on the game thread and must never be blocked behind us).
 * Read-only: this never touches game memory directly. */
static void identity_refresh(void) {
    F1CBrokerStats stats;
    F1CBrokerVehicleHandle handles[MAX_CARS];
    uint32_t count=0, i;
    static uint32_t team[MAX_CARS];
    static int role[MAX_CARS];
    static unsigned char have[MAX_CARS];
    if (!telemetry_broker.get_stats || !telemetry_broker.get_vehicle_handles ||
        !telemetry_broker.get_vehicle_snapshot) return;
    memset(&stats,0,sizeof stats); stats.structure_size=sizeof stats;
    if (telemetry_broker.get_stats(&stats)!=F1C_BROKER_OK || !stats.session_active ||
        !stats.session_generation) return;
    if (telemetry_broker.get_vehicle_handles(telemetry_token,stats.session_generation,
            handles,MAX_CARS,&count)!=F1C_BROKER_OK) return;
    if (count>MAX_CARS) count=MAX_CARS;
    memset(have,0,sizeof have);
    for (i=0;i<count;i++) {
        F1CBrokerVehicleSnapshot snap;
        if (handles[i].slot>=MAX_CARS) continue;
        memset(&snap,0,sizeof snap); snap.structure_size=sizeof snap;
        if (telemetry_broker.get_vehicle_snapshot(telemetry_token,handles[i],&snap)==F1C_BROKER_OK) {
            team[handles[i].slot]=trace_team_id(snap.team_id);
            role[handles[i].slot]=snap.controller_role;
            have[handles[i].slot]=1;
        }
    }
    EnterCriticalSection(&g_telemetry_cs);
    for (i=0;i<MAX_CARS;i++) if (have[i]) {
        g_telemetry.cars[i].team_id=team[i];
        g_telemetry.cars[i].controller_role=role[i];
    }
    LeaveCriticalSection(&g_telemetry_cs);
}

/* 1.2.2 (DVOA report 2026-10-17, "TV Director crashed telemetry"): after a callback over its severe limit the broker
 * quarantines the client and refuses every later post with STALE_TOKEN. 1.2.1 and older only cleared the capture latch and
 * posted again forever, so the page lost the game until a restart. Register again, at most once every 2 s; the broker
 * reclaims the old slot itself. Worker thread only. A watch step lost in the purge is released so it can give up. */
static void telemetry_reregister(DWORD *last) {
    F1CBrokerToken fresh=0;
    F1CBrokerResult rr;
    DWORD now=GetTickCount();
    if (*last && now-*last<2000) return;
    *last=now ? now : 1;
    rr=telemetry_broker.register_client(telemetry_module,"web_telemetry",&fresh);
    if (rr!=F1C_BROKER_OK) {
        log_f("broker: telemetry callbacks were stopped by the broker; register again failed (%d), retry in 2 s",(int)rr);
        return;
    }
    InterlockedExchange((volatile LONG *)&telemetry_token,(LONG)fresh);
    InterlockedExchange(&capture_pending,0);
    EnterCriticalSection(&g_telemetry_cs);
    g_dcam_watch.posted=0;
    LeaveCriticalSection(&g_telemetry_cs);
    ++telemetry_reregisters;
    log_f("broker: telemetry callbacks were stopped by the broker (slow game-thread callback, see the broker log); "
          "registered again, count %u",telemetry_reregisters);
}

static DWORD WINAPI telemetry_worker(void *unused) {
    static char json_buf[262144];
    DWORD last_send=0,last_diag=0,last_identity=0,last_dcam=0,last_reregister=0;
    unsigned last_generation=0;
    unsigned queue_generation=0;
    HANDLE stop;
    int report_session=0,report_type=-1,report_cars=0;
    (void)unused;
    log_f("web_telemetry %s host verified; waiting for reviewed layout and broker",TELEMETRY_VERSION);
    /* Retail unpacking can finish after host activation. No live reads or
     * listener activation until the reviewed layout is recognizable. */
    for (int attempt=0;attempt<360;++attempt) {
        if (layout_compatible()) {
            wtu_note_layout(g_layout_mode,g_layout_anchors);
            if (g_layout_mode==2) log_f("layout: reviewed build (code probe match)");
            else log_f("layout: other build accepted, %d of %d data references found in game code",
                       g_layout_anchors,(int)(sizeof mirror_regions/sizeof mirror_regions[0]));
            break;
        }
        if (attempt==359) { log_f("REFUSED: game code does not reference the data this mod reads (%d of %d references found)",
                                  g_layout_anchors,(int)(sizeof mirror_regions/sizeof mirror_regions[0])); return 1; }
        Sleep(500);
    }
    telemetry_broker.structure_size=sizeof telemetry_broker;
    /* 4.3.0-rc.3: a transient QUERY_FAILED/NOT_FOUND at startup (seen live
     * 2026-10-11 11:17 and 2026-10-11 00:12, the broker still starting) used to
     * disable telemetry for the whole process. Retry for up to 120 s; only an
     * incompatible ABI or missing capability refuses at once. */
    F1CBrokerDiscoveryResult discovery=F1C_BROKER_DISCOVERY_NOT_FOUND;
    for (int attempt=0;attempt<240;++attempt) {
        memset(&telemetry_broker,0,sizeof telemetry_broker);
        telemetry_broker.structure_size=sizeof telemetry_broker;
        discovery=f1c_broker_discover(15,15,
            F1C_BROKER_CAP_LIFECYCLE|F1C_BROKER_CAP_VEHICLE_SNAPSHOTS|F1C_BROKER_CAP_SESSION_SNAPSHOT,
            &telemetry_broker);
        if (discovery==F1C_BROKER_DISCOVERY_OK ||
            discovery==F1C_BROKER_DISCOVERY_INCOMPATIBLE_ABI ||
            discovery==F1C_BROKER_DISCOVERY_MISSING_CAPABILITY ||
            discovery==F1C_BROKER_DISCOVERY_BAD_ARGUMENT) break;
        if (attempt==0) log_f("broker discovery %d; retrying for up to 120 s",discovery);
        Sleep(500);
    }
    if (discovery!=F1C_BROKER_DISCOVERY_OK) { log_f("REFUSED: broker discovery %d",discovery); return 1; }
    log_f("broker discovered");
    if (telemetry_broker.register_client(telemetry_module,"web_telemetry",&telemetry_token)!=F1C_BROKER_OK) {
        log_w("REFUSED: broker registration failed"); f1c_broker_release(&telemetry_broker); return 1;
    }
    telemetry_recovery_ok=f1c_broker_recovery_bind(&telemetry_broker,&telemetry_recovery);
    log_f("broker native budget for TV Director game calls: %s",telemetry_recovery_ok ? "available" : "not offered by this broker");
    telemetry_http_thread=CreateThread(NULL,0,http_thread,NULL,0,NULL);
    if (!telemetry_http_thread) {
        log_w("REFUSED: HTTP thread creation failed");
        if (telemetry_broker.unregister_client(telemetry_token,5000)==F1C_BROKER_OK)
            f1c_broker_release(&telemetry_broker);
        return 1;
    }
    /* 1.1.0 storage writer (spec 6.6: folder scan and cabinet load after registration). */
    if (sw_start()) log_f("storage: writer started; folder %s",sw_dir);
    else log_w("storage: writer not started (recording off for this run)");
    stop=CreateEventA(NULL,TRUE,FALSE,"Local\\f1c_web_telemetry_stop");
    log_f("active: read-only, broker ABI %u; log=%s; HTTP port requested %d; %s; archive %s (cap %d MB)",
          telemetry_broker.negotiated_abi,g_log_path,g_configured_port,
          g_bind_s_addr ? "remote on [Mod] BindAddress (Remote=1)" : (g_remote_access ? "this PC only (Remote=1 without a private BindAddress)" : "this PC only (Remote=0)"),
          g_ar_root[0] ? g_ar_root : "off", g_ar_max_mb);
    while (g_running && (!stop || WaitForSingleObject(stop,0)!=WAIT_OBJECT_0)) {
        F1CBrokerStats stats;
        memset(&stats,0,sizeof stats); stats.structure_size=sizeof stats;
        if (telemetry_broker.get_stats(&stats)==F1C_BROKER_OK &&
            stats.session_generation!=queue_generation) {
            /* Broker discards queued tasks from an old session. Their callback
             * cannot clear pending, so release that latch at an epoch change. */
            update_queue_generation(stats.session_generation,&queue_generation);
        }
        if (InterlockedCompareExchange(&capture_pending,1,0)==0) {
            F1CBrokerResult r=telemetry_broker.post(telemetry_token,capture_on_game_thread,NULL);
            if (r!=F1C_BROKER_OK) InterlockedExchange(&capture_pending,0);
            if (r==F1C_BROKER_STALE_TOKEN) telemetry_reregister(&last_reregister);
        }
        EnterCriticalSection(&g_telemetry_cs);
        if (capture_sequence!=consumed_sequence) {
            if (capture_generation!=last_generation || roster_changed) {
                int generation_changed=capture_generation!=last_generation;
                if (generation_changed) {
                    /* rc.17: one usage report per game session; written after the
                     * lock is released (no file I/O under g_telemetry_cs). */
                    report_session=1;
                    report_type=*(volatile int *)ADDR_SESSION_TYPE;
                    report_cars=*(volatile int *)ADDR_VEHICLE_COUNT;
                    /* 1.0.1: only a new broker generation is a new session. A
                     * roster pointer change alone (a car going to or leaving its
                     * garage) used to clear every driver's laps, pits and
                     * incidents mid-session (666 times in the owner's log). */
                    race_log_close(); session_history_clear("broker session changed");
                    memset(g_telemetry.cars,0,sizeof g_telemetry.cars);
                    g_player_slot=-1; g_player_slot_found=0;
                    remote_session_reset();   /* 1.2.3: CO-OP remote laps restart with the session */
                    g_aiw_seen[0]=0;
                    g_telemetry.sec1=g_telemetry.sec2=g_telemetry.lap_len=0;
                    g_telemetry.aiw_path[0]=g_telemetry.track_name[0]=0;
                }
                /* Preserve all unaffected drivers' trace laps; only a new broker
                 * generation clears the session-wide recorder. */
                trace_capture_epoch(generation_changed,
                                    *(volatile int*)ADDR_VEHICLE_COUNT);
                roster_changed=0;
                last_generation=capture_generation;
            }
            read_telemetry(); consumed_sequence=capture_sequence;
            /* 1.0.1 session archive: boundaries + in-memory documents (no IO here). */
            ar_tick_locked(GetTickCount()-capture_time<=1000);
            /* 1.1.0 storage: session state machine + one ring copy. No IO here (spec 6.1). */
            sw_capture_tick(GetTickCount());
        }
        if (capture_sequence && GetTickCount()-capture_time>1000) {
            g_telemetry.race_active=0;
            g_telemetry.vehicle_count=0;
            memset(g_telemetry.cars,0,sizeof g_telemetry.cars);
            sw_capture_idle(GetTickCount());
        }
        LeaveCriticalSection(&g_telemetry_cs);
        /* 1.1.0: the 1.0.1 archive file writer no longer runs on this thread. It is
         * superseded by the storage writer thread (batch 2); the 1.0.1 archive is
         * not initialized, so telemetry_data is never written. */
        if (report_session) {
            report_session=0;
            wtu_write("session_end",0);
            wtu_note_session(report_type,report_cars);
        }
        DWORD now=GetTickCount();
        if (now-last_send>=SSE_UPDATE_MS) {
            format_failed=0;
            int len=json_telemetry(json_buf,sizeof json_buf);
            if (!format_failed && len>0) sse_broadcast(json_buf,len);
            last_send=now;
        }
        if (now-last_diag>=10000) {
            double cp50,cp95,cmax,crate; int cn;
            capture_cost_snapshot(&cp50,&cp95,&cmax,&crate,&cn);
            EnterCriticalSection(&g_telemetry_cs);
            log_f("sample=%u age_ms=%lu generation=%u phase=%d cars=%d failed=%u pending=%ld "
                  "capture_us p50=%.1f p95=%.1f max=%.1f n=%d rate=%.1f/s",
                capture_sequence,capture_sequence ? now-capture_time : 0ul,capture_generation,
                g_telemetry.race_phase,g_telemetry.vehicle_count,failed_captures,capture_pending,
                cp50,cp95,cmax,cn,crate);
            LeaveCriticalSection(&g_telemetry_cs);
            last_diag=now;
        }
        if (now-last_identity>=2000) { identity_refresh(); last_identity=now; }
        /* TV Director phase 0: camera signature resolve, worker thread, every 2 s until resolved or refused. */
        if (dcam_pending() && now-last_dcam>=2000) { last_dcam=now; dcam_try_resolve(); }
        Sleep(dcam_watch_post_due() ? 2 : 50);   /* TV Director car change: one watch step per game frame while a change runs (2026-10-14) */
    }
    /* The module is pinned. Stop callers, wait for the HTTP worker, then drain
     * broker callbacks before releasing its API. No sleeps/unload in DllMain. */
    g_running=FALSE; g_http_running=FALSE;
    /* 1.1.0: no more capture ticks are possible now; drain and close the writer (spec 6.7, 5 s). */
    if (!sw_stop(5000)) log_w("storage: writer did not finish in 5 s; open .part files are repaired at next start");
    WaitForSingleObject(telemetry_http_thread,INFINITE);
    CloseHandle(telemetry_http_thread);
    F1CBrokerResult stopped=telemetry_broker.unregister_client(telemetry_token,5000);
    if (stopped==F1C_BROKER_OK) f1c_broker_release(&telemetry_broker);
    else log_f("broker unregister=%d; module/API retained safely until exit",stopped);
    race_log_close();
    /* 1.0.1: finalize the archived session (meta closed, last documents). */
    if (g_ar.active) { if (g_ar_done.active) ar_flush_io(1); g_ar_done=g_ar; memset(&g_ar,0,sizeof g_ar); }
    ar_flush_io(1);
    wtu_write("unload",0);
    if (stop) CloseHandle(stop);
    log_w("stopped; restart the game to replace this pinned candidate");
    return 0;
}

static int verified_host(const LightLoaderProtectedHostV2 *host) {
    union { FARPROC raw; LightLoaderProofProc typed; } proof={0};
    if (!host || host->size<sizeof *host || host->magic!=LLP_ABI_MAGIC ||
        host->abi_version<LLP_MIN_EXCLUSIVE_ABI || host->product_id!=LLP_PRODUCT_ID ||
        (host->capabilities&(LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2))!=(LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2) ||
        host->compatibility_generation<LLP_HUB_2_GENERATION ||
        host->process_id!=GetCurrentProcessId() || !host->loader_module ||
        host->seal!=LightLoaderHostSeal(host)) return 0;
    proof.raw=GetProcAddress(host->loader_module,LLP_PROOF_NAME);
    return proof.typed && proof.typed(LightLoaderProofChallenge(host))==host->proof;
}

static BOOL refuse_activation(void) {
    InterlockedExchange(&telemetry_started,-1);
    if (telemetry_instance) { CloseHandle(telemetry_instance); telemetry_instance=NULL; }
    return FALSE;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(const LightLoaderProtectedHostV2 *host) {
    HMODULE pin=NULL;
    char mutex_name[96],path[MAX_PATH];
    if (!verified_host(host)) return FALSE;
    if (InterlockedCompareExchange(&telemetry_started,1,0)!=0) return telemetry_started==2;
    snprintf(mutex_name,sizeof mutex_name,"Local\\F1CWebTelemetry_%lu",GetCurrentProcessId());
    telemetry_instance=CreateMutexA(NULL,FALSE,mutex_name);
    if (!telemetry_instance || GetLastError()==ERROR_ALREADY_EXISTS) return refuse_activation();
    DWORD n=GetModuleFileNameA(telemetry_module,path,sizeof path);
    if (!n || n>=sizeof path) return refuse_activation();
    char *slash=strrchr(path,'\\');
    if (!slash) return refuse_activation();
    slash[1]=0;
    if (snprintf(g_log_path,sizeof g_log_path,"%sweb_telemetry_log.txt",path)>=(int)sizeof g_log_path) return refuse_activation();
    /* 1.1.0: the 1.0.1 archive (telemetry_data) is superseded and not initialized.
     * Storage path work only here (spec 6.6); the folder is <game dir>\Telemetry\. */
    {
        char why[200];
        int rc=sw_root_init_from_exe(why,sizeof why);
        if (rc==SW_OK) { sw_enabled=1; }
        else log_w(why[0] ? why : "storage: folder check failed");
    }
    InitializeCriticalSection(&g_telemetry_cs); InitializeCriticalSection(&g_sse_cs);
    wtu_start();   /* rc.17: crash capture + report identity (cheap, no I/O) */
    memset(&g_telemetry,0,sizeof g_telemetry);
    g_telemetry.fastest_lap_car=g_telemetry.dotd_car=g_telemetry.player_slot=-1;
    lap_history_init(); incidents_init();
    telemetry_settings_load(NULL);
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)rfe_1d3b7a91,&pin)) return refuse_activation();
    telemetry_worker_thread=CreateThread(NULL,0,telemetry_worker,NULL,0,NULL);
    if (!telemetry_worker_thread) { log_w("REFUSED: worker thread creation failed"); return refuse_activation(); }
    CloseHandle(telemetry_worker_thread);
    InterlockedExchange(&telemetry_started,2);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) { telemetry_module=module; DisableThreadLibraryCalls(module); }
    else if (reason==DLL_PROCESS_DETACH && telemetry_started==2) wtu_write("process_exit",1);
    /* 1.1.1 (T3): close an open recording on normal exit; bounded, no waits when the writer already ended. */
    if (reason==DLL_PROCESS_DETACH) sw_process_exit();
    return TRUE;
}
