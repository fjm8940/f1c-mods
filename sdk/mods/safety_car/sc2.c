/*
 * sc1.c — safety car DLL for F1 Challenge 99-02.
 *
 * Build:
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o sc1.dll sc1.c -lkernel32 -luser32
 * Inject:
 *   inject.bat   (uses ..\red-flag-dll\inject.exe)
 *
 * WHY A DLL AND NOT THE PYTHON SCRIPT
 *   The script had to land QualPos/retirement in the window between the driver
 *   structs existing and the grid being placed. An external process cannot hit
 *   that reliably. This DLL instead ENFORCES the retirement every tick, so the
 *   timing stops mattering: Bernd Maylander is retired at grid formation, after
 *   a restart, and through session changes, because he is retired continuously.
 *
 * HOTKEYS
 *   NUMPAD1  1st press = DEPLOY  (revive, field capped to 150 km/h, positions
 *                                 locked, player untouched)
 *            2nd press = RECALL  (called to the box via the game's own trigger,
 *                                 field released)
 *   NUMPAD3  toggle QualPos=200 enforcement (see PLACEMENT below)
 *
 * PLACEMENT — THE OPEN QUESTION
 *   Enforcing "retired" controls his STATE, not where the game parks him. Live
 *   2026-07-25: QualPos=200 + retired left the P1 grid slot empty and put him at
 *   his garage, but it also blanked the pre-race grid listing. Retirement alone
 *   is untested - he may be garaged, or may sit on pole blocking it. NUMPAD3
 *   flips this at runtime so both can be tried without a rebuild. Default OFF.
 *
 * HARD-WON RULES BAKED IN (each cost a live session)
 *   - NEVER revive outside SessionType==7. Doing so writes retired=0/finish=0,
 *     the game zeroes ActiveFlag, and that IS the permanent limbo state.
 *   - Re-resolve the vehicle pointer every tick. A session change reallocates
 *     the structs; writing through a cached pointer corrupts freed memory.
 *   - Never memorise a garage maxSpeed (22.22 = pit limiter) as a car's
 *     "original" - restoring it later caps that car at 80 km/h forever.
 *   - 0x4538 maxSpeed is m/s. 150 km/h = 41.67, not 150.
 *   - The revive MUST null 0x4354 (stale waypoint) or the car drives at a
 *     pre-retirement racing-line node, through the garage wall.
 *
 * Mechanisms: investigations/garage_exit_ignition/README.md,
 *             mods/safety_car/README.md
 */

#include <windows.h>
#include <stdio.h>

/* ---- globals ---------------------------------------------------------- */
#define VEHICLE_COUNT   (*(volatile int*)0x005C329C)
#define VEHICLE_ARRAY   ((volatile unsigned int*)0x007C54F0)
#define PLAYER_VEH      (*(volatile unsigned int*)0x007C54E8)
#define SESSION_TYPE    (*(volatile int*)0x007C56A4)
#define CONTROLLER      0x005EA618
#define EVENT_TABLE     0x005BF9A0

/* ---- vehicle offsets -------------------------------------------------- */
#define OFF_ANGVEL      0x00A0   /* EngineAngVel [FACT]  */
#define OFF_RWFLAG      0x0D58   /* RearWingIntactFlag   */
#define OFF_DRIVERVIS   0x105D   /* DriverModelVisible [FACT] */
#define OFF_PITTING     0x3450
#define OFF_RPM         0x39A0   /* EngineRPM */
#define OFF_CURLAP      0x403C
#define OFF_PITSTATE    0x4040
#define OFF_QUALPOS     0x4058
#define OFF_RACEPOS     0x4060
#define OFF_FINISH      0x4064
#define OFF_CURWP       0x4354   /* CurrentWaypointPtr [FACT] */
#define OFF_RETIRED     0x4410
#define OFF_SPEED       0x4440   /* m/s */
#define OFF_DRVSTAT     0x4510   /* 3 = approach */
#define OFF_MAXSPEED    0x4538   /* m/s */
#define OFF_CALLBOX     0x454C
#define OFF_ACTIVE      0x45F4
#define OFF_NAME        0x4620

#define SC_NAME         "Bernd Maylander"
#define CAP_MS          41.67f    /* 150 km/h */
#define UNCAPPED        99999.0f
#define ON_TRACK_MS     25.0f     /* clear of the 22.22 pit limiter */
#define CRANK_RPM       800.0f
#define SESSION_RACE    7
#define MAX_CARS        32

#define CARRIER_EID     0x83
#define SCORE_SCAR01    132
#define SCORE_SCAR02    133
#define SCORE_SCAR03    134
#define SCORE_SCAR04    135
#define SCORE_SCAR05    131

/* Seconds between capping the field and letting the safety car out. Live
 * 2026-07-25: revived at the same instant the cap engaged, he emerged from the
 * pit exit into 18 cars still travelling at racing pace and was wiped out
 * before he ever cleared the pit limiter. Slow the field FIRST, then release
 * him. */
#define ARM_DELAY_MS    5000

#define ST_IDLE      0
#define ST_ARMING    5   /* field capped, safety car still parked */
#define ST_OUTLAP    1
#define ST_ONTRACK   2
#define ST_RECALLING 3
#define ST_RETURNING 4

static const char* g_log = "sc1_log.txt";
static int   g_state = ST_IDLE;
static DWORD g_mark = 0;
static BOOL  g_enforce_qualpos = FALSE;

static float g_saved_speed[MAX_CARS];
static int   g_locked_pos[MAX_CARS];
static unsigned int g_ptr_at_engage[MAX_CARS];
static BOOL  g_capped = FALSE;

static void log_w(const char* s) {
    FILE* f = fopen(g_log, "a");
    if (f) { fprintf(f, "[%lu] %s\n", GetTickCount(), s); fclose(f); }
}

/* ---- commentary -------------------------------------------------------
 * The five PWS_SCAR clips exist in PC_RE_MAIN.SDT but NO event resolves to
 * them. TableLookupByIndex_004144c0 re-reads the score from the event table by
 * event id (dword_5BF9B0[5*eid]), so writing a score into the queue entry does
 * not steer the clip - the carrier's own score must be repointed. .rdata, so
 * VirtualProtect around it. Restored immediately after. */
static void play_score(int score) {
    DWORD old;
    volatile int* sa = (volatile int*)(EVENT_TABLE + CARRIER_EID * 0x14 + 0x10);
    int orig;
    if (!VirtualProtect((LPVOID)sa, 4, PAGE_EXECUTE_READWRITE, &old)) return;
    orig = *sa;
    *sa = score;

    {
        int pri = *(volatile int*)(EVENT_TABLE + CARRIER_EID * 0x14);
        uintptr_t q = CONTROLLER + 0x864;
        *(volatile int*)(q + 0x00) = CARRIER_EID;
        *(volatile int*)(q + 0x04) = score;
        *(volatile int*)(q + 0x08) = 0;
        *(volatile float*)(q + 0x0c) = -1.0f;
        *(volatile int*)(q + 0x10) = -1;
        *(volatile int*)(q + 0x14) = -1;
        *(volatile int*)(q + 0x18) = -1;
        *(volatile int*)(q + 0x1c) = pri;
        *(volatile float*)(CONTROLLER + 0xaf8 + CARRIER_EID * 4) = 0.0f;
        *(volatile float*)(CONTROLLER + 0xae8) = 0.0f;
        *(volatile int*)(CONTROLLER + 0x135c) = 1;
    }

    Sleep(300);            /* let the tick consume the queue entry */
    *sa = orig;
    VirtualProtect((LPVOID)sa, 4, old, &old);
}

/* ---- vehicles ---------------------------------------------------------- */
static BOOL name_is_sc(unsigned int veh) {
    const char* n = (const char*)(veh + OFF_NAME);
    int i;
    for (i = 0; i < 15; i++) {           /* "Bernd Maylander" */
        if (n[i] != SC_NAME[i]) return FALSE;
    }
    return TRUE;
}

/* Resolve BY NAME every time: enabling Maylander shifted every slot, and a
 * session change reallocates the structs. Never cache across ticks. */
static unsigned int find_sc(int* out_slot) {
    int n = VEHICLE_COUNT, i;
    if (n < 1 || n > MAX_CARS) return 0;
    for (i = 0; i < n; i++) {
        unsigned int v = VEHICLE_ARRAY[i];
        if (!v) continue;
        if (IsBadReadPtr((void*)(v + OFF_NAME), 16)) continue;
        if (name_is_sc(v)) { if (out_slot) *out_slot = i; return v; }
    }
    return 0;
}

/* ---- retire / revive --------------------------------------------------- */
static void enforce_retired(unsigned int v) {
    *(volatile int*)(v + OFF_FINISH)     = 2;
    *(volatile char*)(v + OFF_RETIRED)   = 1;
    *(volatile char*)(v + OFF_ACTIVE)    = 0;
    *(volatile int*)(v + OFF_PITSTATE)   = 2;
    *(volatile char*)(v + OFF_DRIVERVIS) = 0;
    /* A retired car left holding its qualifying race position shows as LEADING
     * the race from inside its garage (live: QP=1 RP=1). Park him last. */
    {
        int n = VEHICLE_COUNT;
        if (n > 0 && n <= MAX_CARS) *(volatile int*)(v + OFF_RACEPOS) = n;
    }
    if (g_enforce_qualpos) {
        *(volatile int*)(v + OFF_QUALPOS) = 200;
        *(volatile int*)(v + OFF_CURLAP)  = 0;
    }
}

/* The garage-exit recipe. Order matters: disarm, revive, hand to the state
 * machine, and only then crank - the crank needs a running physics step, which
 * ActiveFlag=1 restores. */
static void revive(unsigned int v) {
    *(volatile char*)(v + OFF_RWFLAG)    = 1;   /* disarm or he re-retires */
    *(volatile int*)(v + OFF_CALLBOX)    = 0;
    *(volatile int*)(v + OFF_PITTING)    = 0;
    *(volatile int*)(v + OFF_FINISH)     = 0;
    *(volatile char*)(v + OFF_RETIRED)   = 0;
    *(volatile char*)(v + OFF_ACTIVE)    = 1;
    *(volatile int*)(v + OFF_DRVSTAT)    = 3;   /* approach - NOT 0 */
    *(volatile char*)(v + OFF_DRIVERVIS) = 1;   /* else an empty cockpit */
    *(volatile int*)(v + OFF_CURWP)      = 0;   /* the garage-wall fix */
    *(volatile float*)(v + OFF_ANGVEL)   = CRANK_RPM;
    *(volatile float*)(v + OFF_RPM)      = CRANK_RPM;
}

/* Arm the game's own trigger and hand off - it drives him in and garages him
 * itself. Do NOT forge the end-state out on track. */
static void send_to_box(unsigned int v) {
    *(volatile char*)(v + OFF_RWFLAG) = 0;
    *(volatile int*)(v + OFF_CALLBOX) = 1;
}

/* ---- field ------------------------------------------------------------- */
static void field_engage(int sc_slot) {
    int n = VEHICLE_COUNT, i;
    unsigned int player = PLAYER_VEH;
    char buf[96];
    int cnt = 0;
    if (n > MAX_CARS) n = MAX_CARS;
    for (i = 0; i < n; i++) {
        unsigned int v = VEHICLE_ARRAY[i];
        g_ptr_at_engage[i] = 0;
        if (!v || i == sc_slot || v == player) continue;
        if (*(volatile char*)(v + OFF_ACTIVE) != 1) continue;
        {
            float orig = *(volatile float*)(v + OFF_MAXSPEED);
            /* A garage reading (22.22 = pit limiter) must never be memorised as
             * the original, or the car is restored to a permanent 80 km/h. */
            if (orig < CAP_MS * 1.5f) orig = UNCAPPED;
            g_saved_speed[i] = orig;
        }
        g_locked_pos[i]    = *(volatile int*)(v + OFF_RACEPOS);
        g_ptr_at_engage[i] = v;
        cnt++;
    }
    g_capped = TRUE;
    wsprintfA(buf, "cap 150 km/h + position lock on %d cars (player free)", cnt);
    log_w(buf);
}

static void field_tick(void) {
    int n = VEHICLE_COUNT, i;
    if (!g_capped) return;
    if (n > MAX_CARS) n = MAX_CARS;
    for (i = 0; i < n; i++) {
        unsigned int v = VEHICLE_ARRAY[i];
        if (!v || !g_ptr_at_engage[i]) continue;
        if (v != g_ptr_at_engage[i]) continue;   /* struct moved - do not write */
        *(volatile float*)(v + OFF_MAXSPEED) = CAP_MS;
        *(volatile int*)(v + OFF_RACEPOS)    = g_locked_pos[i];
    }
}

static void field_release(void) {
    int n = VEHICLE_COUNT, i, cnt = 0;
    char buf[64];
    if (!g_capped) return;
    if (n > MAX_CARS) n = MAX_CARS;
    for (i = 0; i < n; i++) {
        unsigned int v = VEHICLE_ARRAY[i];
        if (!v || !g_ptr_at_engage[i]) continue;
        *(volatile float*)(v + OFF_MAXSPEED) = g_saved_speed[i];
        g_ptr_at_engage[i] = 0;
        cnt++;
    }
    g_capped = FALSE;
    wsprintfA(buf, "cap released, %d cars restored", cnt);
    log_w(buf);
}

/* ---- main tick --------------------------------------------------------- */
static DWORD WINAPI tick(LPVOID unused) {
    BOOL prev1 = FALSE, prev3 = FALSE;
    (void)unused;
    log_w("sc1.dll loaded. NUMPAD1 = deploy/recall, NUMPAD3 = toggle QualPos.");

    for (;;) {
        int slot = -1;
        unsigned int sc = find_sc(&slot);
        BOOL k1 = (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0;
        BOOL k3 = (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0;
        BOOL hit1 = k1 && !prev1;
        BOOL hit3 = k3 && !prev3;
        prev1 = k1; prev3 = k3;

        if (hit3) {
            g_enforce_qualpos = !g_enforce_qualpos;
            log_w(g_enforce_qualpos ? "QualPos=200 enforcement ON"
                                    : "QualPos=200 enforcement OFF");
        }

        if (!sc) { Sleep(200); continue; }

        if (hit1 && g_state == ST_IDLE) {
            if (SESSION_TYPE != SESSION_RACE) {
                log_w("REFUSED: not a race session. Reviving here creates the "
                      "permanent limbo state.");
            } else {
                /* Cap the field FIRST and hold him for ARM_DELAY_MS - see the
                 * comment on ARM_DELAY_MS. He is released in ST_ARMING. */
                log_w("DEPLOY: slowing the field before releasing the car");
                field_engage(slot);
                play_score(SCORE_SCAR01);
                g_state = ST_ARMING;
                g_mark = GetTickCount();
            }
        } else if (hit1 && (g_state == ST_ARMING ||
                            g_state == ST_OUTLAP || g_state == ST_ONTRACK)) {
            log_w("RECALL");
            play_score(SCORE_SCAR03);
            g_state = ST_RECALLING;
            g_mark = GetTickCount();
        }

        switch (g_state) {
        case ST_IDLE:
            /* The whole point of the DLL: he is retired CONTINUOUSLY, so grid
             * formation, restarts and session changes all see a retired car. */
            enforce_retired(sc);
            break;

        case ST_ARMING:
            /* Field is capped and slowing; keep him parked until it has. */
            enforce_retired(sc);
            if (GetTickCount() - g_mark > ARM_DELAY_MS) {
                log_w("field slowed - releasing the safety car");
                revive(sc);
                g_state = ST_OUTLAP;
                g_mark = GetTickCount();
            }
            break;

        case ST_OUTLAP:
            if (*(volatile float*)(sc + OFF_SPEED) > ON_TRACK_MS) {
                play_score(SCORE_SCAR02);
                g_state = ST_ONTRACK;
            } else if (GetTickCount() - g_mark > 3000 &&
                       (*(volatile char*)(sc + OFF_RETIRED) == 1 ||
                        *(volatile int*)(sc + OFF_FINISH) == 2)) {
                /* Retired again on the way out - almost certainly collected by
                 * traffic at the pit exit. Do not sit here for 45 s. */
                log_w("safety car was RETIRED on its out lap (hit at the pit "
                      "exit?) - releasing field");
                field_release();
                g_state = ST_IDLE;
            } else if (GetTickCount() - g_mark > 45000) {
                char buf[128];
                wsprintfA(buf, "ABORT: no motion in 45 s (ret=%d act=%d dStat=%d) "
                               "- releasing field",
                          *(volatile char*)(sc + OFF_RETIRED),
                          *(volatile char*)(sc + OFF_ACTIVE),
                          *(volatile int*)(sc + OFF_DRVSTAT));
                log_w(buf);
                field_release();
                g_state = ST_IDLE;
            }
            break;

        case ST_RECALLING:
            if (GetTickCount() - g_mark > 4000) {
                play_score(SCORE_SCAR05);
                send_to_box(sc);
                g_state = ST_RETURNING;
                g_mark = GetTickCount();
            }
            break;

        case ST_RETURNING:
            if (*(volatile char*)(sc + OFF_RETIRED) == 1 ||
                *(volatile char*)(sc + OFF_ACTIVE) == 0) {
                play_score(SCORE_SCAR04);
                field_release();
                log_w("safety car in. Green flag.");
                g_state = ST_IDLE;
            } else if (GetTickCount() - g_mark > 120000) {
                log_w("never reached the box - releasing field anyway");
                field_release();
                g_state = ST_IDLE;
            }
            break;
        }

        field_tick();
        Sleep(50);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, tick, NULL, 0, NULL);
    }
    return TRUE;
}
