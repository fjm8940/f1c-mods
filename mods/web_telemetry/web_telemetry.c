/*
 * web_telemetry.dll — F1C Telemetry 1.0.1 (engine 4.4.0)
 *
 * Complete TV-style broadcast dashboard. Reads 48 unique vehicle fields per
 * driver + 10 session globals, serves via HTTP/SSE on a configurable port
 * (web_telemetry.ini [Mod] Port, default 8080, bind fallback +1..+5).
 * rc.5 added the legacy telemetry workbench; rc.6 the sidecar integrity
 * report and the import content-type guard; rc.7 fixes the sidecar staging
 * (byte-exact gate) and hardens the HTTP endpoint layer. rc.8 froze API
 * contract v1 (schemas + payload dir); rc.9 implemented the BACKEND 2 review
 * fixes (api_version 2) and the BACKEND 3 lifecycle/robustness gates; rc.10
 * is the BACKEND 4 final candidate: the native game-thread capture cost
 * (p50/p95/max, samples/s) and the process working set are published in
 * /health and the 10 s diagnostic log (api_version 3), with the live
 * checklist and read-only live-check script alongside. rc.11 is a
 * frontend-only release: the workbench_ui.js sidecar reaches old-viewer
 * parity (23 legacy layouts, lap strip, linked cursor) and its pinned hash
 * changes; no capture, endpoint or contract change (api_version 3).
 * rc.12 (frontend-only): the workbench records every car from page open,
 * so changing the A/B driver no longer erases collected laps.
 *
 * Features: driver standings with sector times, 2D track map with heading
 * arrows, radial speedo/tach gauges, weather, throttle/brake, tire compounds,
 * fuel rate, damage, track limits, DRS zone, DNF reasons, pit stop count,
 * RPM limiter, grid position, race clock, player indicator, penalty slots.
 *
 * Build:
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o web_telemetry.dll
 *     web_telemetry.c -I..\..\tools\sdk -lws2_32 -lkernel32 -luser32 -O2 -Wall
 */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <stdarg.h>
#include <psapi.h>
#include <math.h>
#include "f1c_broker.h"
#include "f1c_broker_recovery.h"
#include "f1c_target.h"
#include "f1c_sdk.h"
#include "lightloader_plugin_abi.h"
#include "legacy_formats.h"
#include "sha256.h"
#define TELEMETRY_VERSION "1.2.5"
/* Public product identity (Lightloader catalog "Telemetry"). TELEMETRY_VERSION
 * stays the engine/API build reported in /health and the frame. */
#define PRODUCT_LABEL "Telemetry 1.2.5"
/* Frozen API contract version: 3 adds the native capture-cost and process
 * memory report to /health (BACKEND 4 review). Bump together with any schema
 * change; schemas/version.json + the contract test enforce the pairing. */
#define API_VERSION "10"  /* 1.1.0: player chassis "pch", AI abt/aws, AI wheel order fixed; 7 = 1.0.1 session archive; 8 = pch/abt; 9 = recordings manager (/recordings*, /sessions removed); 10 = review index (GET /recordings/review, review_id on closed sessions) */
/* All output formatters return actual bytes written, even on old MSVCRT.
 * Failure is sticky per thread; HTTP sends an error, never truncated JSON. */
static __thread int format_failed;
static int bounded_format(char *out, size_t size, const char *fmt, ...) {
    int n; va_list ap;
    if (!size) { format_failed = 1; return 0; }
    va_start(ap, fmt); n = vsnprintf(out, size, fmt, ap); va_end(ap);
    out[size-1] = 0;
    if (n < 0 || (size_t)n >= size) { format_failed = 1; return (int)strlen(out); }
    return n;
}
#define _snprintf bounded_format
static void bounded_char(char *out, int size, int *pos, char ch) {
    if (*pos<0 || *pos>=size-1) { format_failed=1; return; }
    out[(*pos)++]=ch; out[*pos]=0;
}
static float finite_float(float v) { return isfinite(v) && fabsf(v)<1e12f ? v : 0.0f; }
static double finite_double(double v) { return isfinite(v) ? v : 0.0; }
static HMODULE telemetry_module;
static HANDLE telemetry_http_thread, telemetry_worker_thread, telemetry_instance;
static F1CBrokerApi telemetry_broker;
static F1CBrokerToken telemetry_token;
/* 1.2.2 (DVOA report 2026-10-17): TV Director runs native game code in broker tasks (camera chain, PGDN, HUD). One slow frame
 * over the broker's 100 ms severe limit quarantined telemetry for good. The ABI14 native budget (250 ms) covers the current
 * callback only; it is optional (older brokers: not bound, no change). */
static F1CBrokerRecoveryApi telemetry_recovery;
static int telemetry_recovery_ok;
static unsigned telemetry_reregisters;
/* Game thread, inside a telemetry broker task only (the broker refuses any other thread or token). */
static void telemetry_native_budget(void) {
    if (telemetry_recovery_ok && telemetry_recovery.native_budget)
        (void)telemetry_recovery.native_budget(telemetry_token);
}
static volatile LONG telemetry_started, capture_pending;
static DWORD capture_time;
static unsigned capture_sequence, capture_generation, consumed_sequence;
static unsigned failed_captures;
static int roster_changed;

/* BACKEND 4 (api_version 3): native game-thread capture cost. Measured with
 * QPC inside the broker callback (the only place game memory is read); a
 * bounded ring is published in /health and the 10 s diagnostic log, with the
 * samples-per-second rate. Checklist pass limit: p95 < 500 us and no
 * owner-reported frame hitch. */
#define CAPTURE_COST_SAMPLES 240
static double capture_cost_us[CAPTURE_COST_SAMPLES];
static volatile LONG capture_cost_epoch;   /* odd while the ring is written */
static unsigned capture_cost_head, capture_cost_count;
static volatile DWORD capture_rate_start;
static volatile unsigned capture_rate_count;
static volatile double capture_rate_per_s;
static LARGE_INTEGER capture_qpc_freq;

static void capture_cost_record(LARGE_INTEGER t0) {
    LARGE_INTEGER t1;
    double us; DWORD now, elapsed;
    QueryPerformanceCounter(&t1);
    if (!capture_qpc_freq.QuadPart) QueryPerformanceFrequency(&capture_qpc_freq);
    us=(double)(t1.QuadPart-t0.QuadPart)*1e6/(double)capture_qpc_freq.QuadPart;
    InterlockedIncrement(&capture_cost_epoch);
    capture_cost_us[capture_cost_head]=us;
    capture_cost_head=(capture_cost_head+1)%CAPTURE_COST_SAMPLES;
    if (capture_cost_count<CAPTURE_COST_SAMPLES) capture_cost_count++;
    now=GetTickCount();
    if (!capture_rate_start) { capture_rate_start=now; capture_rate_count=0; }
    capture_rate_count++;
    elapsed=now-capture_rate_start;
    if (elapsed>=5000) {
        capture_rate_per_s=(double)capture_rate_count*1000.0/(double)elapsed;
        capture_rate_start=now; capture_rate_count=0;
    }
    InterlockedIncrement(&capture_cost_epoch); /* publishes ring and 64-bit rate together */
}

static int compare_capture_cost(const void *a, const void *b) {
    double x=*(const double *)a, y=*(const double *)b;
    return (x>y)-(x<y);
}

static void capture_cost_snapshot(double *p50,double *p95,double *pmax,double *rate,int *count) {
    double copy[CAPTURE_COST_SAMPLES];
    unsigned head=0; int n=0; double copied_rate=0;
    for (int tries=0;tries<16;tries++) {
        LONG e1=InterlockedCompareExchange(&capture_cost_epoch,0,0);
        if (e1&1u) { Sleep(0); continue; }
        n=(int)capture_cost_count; head=capture_cost_head;
        if (n>CAPTURE_COST_SAMPLES) n=CAPTURE_COST_SAMPLES;
        for (int i=0;i<n;i++)
            copy[i]=capture_cost_us[(head-n+i+CAPTURE_COST_SAMPLES)%CAPTURE_COST_SAMPLES];
        copied_rate=capture_rate_per_s;
        if (InterlockedCompareExchange(&capture_cost_epoch,0,0)==e1) break;
        n=0;
        copied_rate=0;
    }
    qsort(copy,(size_t)n,sizeof copy[0],compare_capture_cost);
    *p50=n?copy[(int)((n-1)*0.50)]:0.0;
    *p95=n?copy[(int)((n-1)*0.95)]:0.0;
    *pmax=n?copy[n-1]:0.0;
    *rate=copied_rate;
    *count=n;
}
static uintptr_t last_roster[104];
static union { double align; unsigned char bytes[VEH_STRIDE]; } mirror_vehicles[VEH_SLOTS];
static union { double align; unsigned char bytes[3000*316]; } mirror_waypoints;
static union { double align; unsigned char bytes[128*8]; } mirror_flags;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_SESSION_TYPE;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_RACE_PHASE;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_RACE_ACTIVE;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_VEHICLE_COUNT;
static union { double align; unsigned char bytes[416]; } mirror_ADDR_VEHICLE_PTR_ARRAY;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_TOTAL_LAPS;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_TRACK_CONDITION;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_TRACK_WETNESS;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_CHAMP_MODE;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_PLAYER_INDEX;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_AI_DIFFICULTY;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_RACE_TIME_THRESH;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_SESSION_TIME;
static union { double align; unsigned char bytes[280]; } mirror_ADDR_STEWARD;
static union { double align; unsigned char bytes[260]; } mirror_ADDR_AIW_PATH;
static union { double align; unsigned char bytes[312]; } mirror_ADDR_AI_DB_BASE;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_AMBIENT_TEMP;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_TRACK_TEMP;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_ON_WETNESS;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_OFF_WETNESS;
static union { double align; unsigned char bytes[4496]; } mirror_ADDR_WHEEL_THERMAL;
static union { double align; unsigned char bytes[16]; } mirror_ADDR_BLISTER_FLAGS;
/* 4.3.0 RESEARCH: player W+796 (tyre load term), 4 wheels x 1124 stride. */
static union { double align; unsigned char bytes[3380]; } mirror_ADDR_WHEEL_LOAD;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_OIL_TEMP_HUD;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_WATER_TEMP_HUD;
static union { double align; unsigned char bytes[8]; } mirror_ADDR_PLAYER_ENGINE_LIFE;
/* 1.1.0: the whole player full-physics object (P = 0x7C7428 .. P+0x1E40). */
static union { double align; unsigned char bytes[0x1E40]; } mirror_ADDR_PLAYER_PHYS;
typedef struct { uintptr_t address; size_t size; unsigned char *bytes; } MirrorRegion;
static MirrorRegion mirror_regions[] = {
    { 0x007C56A4u, sizeof(mirror_ADDR_SESSION_TYPE.bytes), mirror_ADDR_SESSION_TYPE.bytes },
    { 0x007C56A8u, sizeof(mirror_ADDR_RACE_PHASE.bytes), mirror_ADDR_RACE_PHASE.bytes },
    { 0x007D0308u, sizeof(mirror_ADDR_RACE_ACTIVE.bytes), mirror_ADDR_RACE_ACTIVE.bytes },
    { 0x005C329Cu, sizeof(mirror_ADDR_VEHICLE_COUNT.bytes), mirror_ADDR_VEHICLE_COUNT.bytes },
    { 0x007C54F0u, sizeof(mirror_ADDR_VEHICLE_PTR_ARRAY.bytes), mirror_ADDR_VEHICLE_PTR_ARRAY.bytes },
    { 0x008ACF8Cu, sizeof(mirror_ADDR_TOTAL_LAPS.bytes), mirror_ADDR_TOTAL_LAPS.bytes },
    { 0x007CF3C8u, sizeof(mirror_ADDR_TRACK_CONDITION.bytes), mirror_ADDR_TRACK_CONDITION.bytes },
    { 0x007CF3A4u, sizeof(mirror_ADDR_TRACK_WETNESS.bytes), mirror_ADDR_TRACK_WETNESS.bytes },
    { 0x007D0410u, sizeof(mirror_ADDR_CHAMP_MODE.bytes), mirror_ADDR_CHAMP_MODE.bytes },
    { 0x007C54DCu, sizeof(mirror_ADDR_PLAYER_INDEX.bytes), mirror_ADDR_PLAYER_INDEX.bytes },
    { 0x007D01A0u, sizeof(mirror_ADDR_AI_DIFFICULTY.bytes), mirror_ADDR_AI_DIFFICULTY.bytes },
    { 0x008ACF94u, sizeof(mirror_ADDR_RACE_TIME_THRESH.bytes), mirror_ADDR_RACE_TIME_THRESH.bytes },
    { 0x008AFEA0u, sizeof(mirror_ADDR_SESSION_TIME.bytes), mirror_ADDR_SESSION_TIME.bytes },
    { 0x008ACEE0u, sizeof(mirror_ADDR_STEWARD.bytes), mirror_ADDR_STEWARD.bytes },
    { 0x008AC438u, sizeof(mirror_ADDR_AIW_PATH.bytes), mirror_ADDR_AIW_PATH.bytes },
    { 0x005EA2C0u, sizeof(mirror_ADDR_AI_DB_BASE.bytes), mirror_ADDR_AI_DB_BASE.bytes },
    { 0x008AFEF0u, sizeof(mirror_ADDR_AMBIENT_TEMP.bytes), mirror_ADDR_AMBIENT_TEMP.bytes },
    { 0x008AFEF4u, sizeof(mirror_ADDR_TRACK_TEMP.bytes), mirror_ADDR_TRACK_TEMP.bytes },
    { 0x008AFEE8u, sizeof(mirror_ADDR_ON_WETNESS.bytes), mirror_ADDR_ON_WETNESS.bytes },
    { 0x008AFEECu, sizeof(mirror_ADDR_OFF_WETNESS.bytes), mirror_ADDR_OFF_WETNESS.bytes },
    { 0x007C7A00u, sizeof(mirror_ADDR_WHEEL_THERMAL.bytes), mirror_ADDR_WHEEL_THERMAL.bytes },
    { 0x005EC3BCu, sizeof(mirror_ADDR_BLISTER_FLAGS.bytes), mirror_ADDR_BLISTER_FLAGS.bytes },
    { 0x007C79D0u, sizeof(mirror_ADDR_WHEEL_LOAD.bytes), mirror_ADDR_WHEEL_LOAD.bytes },
    { 0x007C9174u, sizeof(mirror_ADDR_OIL_TEMP_HUD.bytes), mirror_ADDR_OIL_TEMP_HUD.bytes },
    { 0x007C91B4u, sizeof(mirror_ADDR_WATER_TEMP_HUD.bytes), mirror_ADDR_WATER_TEMP_HUD.bytes },
    { (uintptr_t)G_EngineLifeSeconds, sizeof(mirror_ADDR_PLAYER_ENGINE_LIFE.bytes), mirror_ADDR_PLAYER_ENGINE_LIFE.bytes },
    { 0x007C7428u, sizeof(mirror_ADDR_PLAYER_PHYS.bytes), mirror_ADDR_PLAYER_PHYS.bytes },
};
static int copy_game(void *dst, uintptr_t address, size_t size) {
    SIZE_T got = 0;
    if (address < 0x10000u || size > UINTPTR_MAX-address) return 0;
#ifdef TELEMETRY_TEST
    extern void *telemetry_test_address(uintptr_t address,size_t size);
    address=(uintptr_t)telemetry_test_address(address,size);
#endif
    return ReadProcessMemory(GetCurrentProcess(), (const void *)address, dst, size, &got) && got == size;
}
/* rc.17 layout result: 2 = reviewed build (code probe), 1 = other build whose
 * code references the mirrored globals, 0 = refused. anchors = references
 * found (-1 when the probe matched and no scan ran). */
static int g_layout_mode = 0, g_layout_anchors = -1;
#define LAYOUT_ANCHOR_SLACK 2
#define LAYOUT_SCAN_CHUNK   65536u
static int layout_anchor_count(uintptr_t image, const IMAGE_NT_HEADERS32 *nt, uintptr_t lfanew) {
    enum { NREG = sizeof mirror_regions / sizeof mirror_regions[0] };
    static unsigned char buf[LAYOUT_SCAN_CHUNK + 3];
    unsigned char hit[NREG];
    int nsec = nt->FileHeader.NumberOfSections, found = 0;
    uintptr_t sh = image + lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + nt->FileHeader.SizeOfOptionalHeader;
    memset(hit, 0, sizeof hit);
    if (nsec <= 0 || nsec > 32) return 0;
    for (int s = 0; s < nsec; s++) {
        IMAGE_SECTION_HEADER sec;
        if (!copy_game(&sec, sh + (uintptr_t)s * sizeof sec, sizeof sec)) return 0;
        if (!(sec.Characteristics & IMAGE_SCN_CNT_CODE)) continue;
        uintptr_t a = image + sec.VirtualAddress;
        uintptr_t len = sec.Misc.VirtualSize ? sec.Misc.VirtualSize : sec.SizeOfRawData;
        if (len > 0x2000000u) len = 0x2000000u;
        for (uintptr_t off = 0; off < len; off += LAYOUT_SCAN_CHUNK) {
            size_t n = (size_t)((len - off) < LAYOUT_SCAN_CHUNK + 3 ? (len - off) : LAYOUT_SCAN_CHUNK + 3);
            if (n < 4 || !copy_game(buf, a + off, n)) continue;
            for (size_t i = 0; i + 4 <= n; i++) {
                uint32_t v = (uint32_t)buf[i] | ((uint32_t)buf[i+1] << 8) | ((uint32_t)buf[i+2] << 16) | ((uint32_t)buf[i+3] << 24);
                if (v < 0x00400000u || v > 0x00A00000u) continue;
                for (int r = 0; r < NREG; r++)
                    if (!hit[r] && v == (uint32_t)mirror_regions[r].address) { hit[r] = 1; found++; }
            }
        }
    }
    return found;
}
static int layout_for_image(uintptr_t image) {
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    unsigned char probe[F1C_TARGET_PROBE_LENGTH];
    /* Reviewed code fingerprint + bounded fixed-array layout, no EXE hash gate.
     * Broker additionally refuses unsupported game-update code layouts. */
    if (image!=F1C_TARGET_IMAGE_BASE || !copy_game(&dos,image,sizeof dos) ||
        dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<(LONG)sizeof dos || dos.e_lfanew>0x1000 ||
        !copy_game(&nt,image+(uintptr_t)dos.e_lfanew,sizeof nt) ||
        nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.ImageBase!=image || nt.OptionalHeader.SizeOfImage>UINTPTR_MAX-image)
        return 0;
    uintptr_t end=image+nt.OptionalHeader.SizeOfImage;
    if (VEH_BASE<image || VEH_BASE+(uintptr_t)VEH_STRIDE*VEH_SLOTS>end) return 0;
    for (size_t n=0;n<sizeof mirror_regions/sizeof mirror_regions[0];++n) {
        const MirrorRegion *r=&mirror_regions[n];
        if (r->address<image || r->address>=end || r->size>end-r->address) return 0;
    }
    if (F1C_TARGET_PROBE_ADDRESS>=image && F1C_TARGET_PROBE_ADDRESS+sizeof probe<=end &&
        copy_game(probe,F1C_TARGET_PROBE_ADDRESS,sizeof probe) &&
        !memcmp(probe,F1C_TARGET_PROBE_BYTES,sizeof probe)) {
        g_layout_mode = 2; g_layout_anchors = -1;
        return 1;
    }
    /* rc.17: any F1 Challenge build is accepted when the game's own code still
     * addresses the data this mod reads - no EXE name, hash or checksum test.
     * Every mirrored global is looked up as a 32-bit operand in the executable
     * sections; at most LAYOUT_ANCHOR_SLACK may be missing (some globals are
     * reached as base+offset, e.g. 0x7C79D0 in every known build). */
    {
        int found = layout_anchor_count(image, &nt, (uintptr_t)dos.e_lfanew);
        int total = (int)(sizeof mirror_regions / sizeof mirror_regions[0]);
        g_layout_anchors = found;
        if (found >= total - LAYOUT_ANCHOR_SLACK) { g_layout_mode = 1; return 1; }
    }
    g_layout_mode = 0;
    return 0;
}
static int layout_compatible(void) { return layout_for_image((uintptr_t)GetModuleHandleA(NULL)); }


/* ========================================================================
 * GAME ADDRESSES
 * ======================================================================== */
#define GAME_ADDR_SESSION_TYPE 0x007C56A4u
#define ADDR_SESSION_TYPE ((uintptr_t)mirror_ADDR_SESSION_TYPE.bytes)
#define GAME_ADDR_RACE_PHASE 0x007C56A8u
#define ADDR_RACE_PHASE ((uintptr_t)mirror_ADDR_RACE_PHASE.bytes)
#define GAME_ADDR_RACE_ACTIVE 0x007D0308u
#define ADDR_RACE_ACTIVE ((uintptr_t)mirror_ADDR_RACE_ACTIVE.bytes)
#define GAME_ADDR_VEHICLE_COUNT 0x005C329Cu
#define ADDR_VEHICLE_COUNT ((uintptr_t)mirror_ADDR_VEHICLE_COUNT.bytes)
#define GAME_ADDR_VEHICLE_PTR_ARRAY 0x007C54F0u
#define ADDR_VEHICLE_PTR_ARRAY ((uintptr_t)mirror_ADDR_VEHICLE_PTR_ARRAY.bytes)
#define GAME_ADDR_TOTAL_LAPS 0x008ACF8Cu
#define ADDR_TOTAL_LAPS ((uintptr_t)mirror_ADDR_TOTAL_LAPS.bytes)
#define GAME_ADDR_TRACK_CONDITION 0x007CF3C8u
#define ADDR_TRACK_CONDITION ((uintptr_t)mirror_ADDR_TRACK_CONDITION.bytes)
#define GAME_ADDR_TRACK_WETNESS 0x007CF3A4u
#define ADDR_TRACK_WETNESS ((uintptr_t)mirror_ADDR_TRACK_WETNESS.bytes)
#define GAME_ADDR_CHAMP_MODE 0x007D0410u
#define ADDR_CHAMP_MODE ((uintptr_t)mirror_ADDR_CHAMP_MODE.bytes)
#define GAME_ADDR_PLAYER_INDEX 0x007C54DCu
#define ADDR_PLAYER_INDEX ((uintptr_t)mirror_ADDR_PLAYER_INDEX.bytes)
#define GAME_ADDR_AI_DIFFICULTY 0x007D01A0u
#define ADDR_AI_DIFFICULTY ((uintptr_t)mirror_ADDR_AI_DIFFICULTY.bytes)
#define GAME_ADDR_RACE_TIME_THRESH 0x008ACF94u
#define ADDR_RACE_TIME_THRESH ((uintptr_t)mirror_ADDR_RACE_TIME_THRESH.bytes)
/* ---- YELLOW FLAGS / track-sector flag table (SOLVED 2026-07-25) -------------
 * The corpus previously recorded sector flag state as "confirmed absent". It is
 * not: `HUDFlagRendererMain` (0x4D82D0) draws flag byte 62 (yellow) when
 * `ShortComputation_bad0` (0x49BAD0) is true, and the disassembly of that
 * function shows it is a table lookup on the STEWARD object:
 *
 *   mov ecx, 0x8ACEE0        ; the Steward (Steward_Init/Setup/Post/Restart)
 *   call 0x4D1CF0            ; state at segment (idx - 1), wrapping
 *   cmp eax, 2               ; 2 == YELLOW
 *   ... call 0x4D1D40        ; state at segment (idx), clamped
 *   cmp eax, 2
 *
 * where idx = CRTFtol(distance * scale) and distance comes from
 * DistanceProjectionCalc(playerVeh + 12). docs/26 mislabels 0x8ACEE0 as
 * "ForceFeedbackBase"; the corpus's own Steward_* functions all take it.
 *
 * Layout, live-verified in-race with a yellow flag actually flying:
 *   +0x108 float  scale, 1/segment-length (0.0050162 => 199.35 units/segment)
 *   +0x10C int    segment count (29 here; 29 * 199.35 = 5781 = track length)
 *   +0x110 ptr    array of { int32 state; float end_distance }, stride 8
 *   +0x114 ptr    array of int32, the game's OWN previous-state copy, which it
 *                 diffs against to fire flag events
 * Observed: segment 4 held state 2 while every other segment held 0.
 *
 * WHO WRITES IT: sub_4D1730, called every frame from the scene update. It
 * CLEARS every segment to 0, then for each car that is not DNF computes a
 * minimum-speed threshold and, if the car's forward velocity is below it, sets
 * that car's segment (index - 1, wrapped) to 2. So yellow == "a car is going
 * too slowly here", recomputed from scratch every frame -- poking a segment
 * cannot persist for even one frame, which is exactly why this is read-only
 * tracking and not a mod surface.
 *
 * The state field is 6 bits: the change-notify path encodes each event as
 * `state | (segment << 6)` and ParseMidiBitstream (0x4D1A50) decodes it as
 * `segment = word >> 6`, `state = word & 0x3F`. Only 0 (green) and 2 (yellow)
 * have been observed; anything else is reported raw rather than guessed at. */
/* Session clock, seconds, double. Used by the flag writer sub_4D1730 itself
 * (`dbl_8AFEA0 < Steward+176 + 20.0` for its race-start relaxation) and by the
 * change-notify throttle (`dbl_8AFEA0 - Steward+280 > 5.0`). It RESETS when a
 * session restarts, which makes it the reliable restart signal: session type,
 * lap count, car count and the race-active flag are all unchanged across a
 * restart, so none of them can detect one. */
#define GAME_ADDR_SESSION_TIME 0x008AFEA0u
#define ADDR_SESSION_TIME ((uintptr_t)mirror_ADDR_SESSION_TIME.bytes)

#define GAME_ADDR_STEWARD 0x008ACEE0u
#define ADDR_STEWARD ((uintptr_t)mirror_ADDR_STEWARD.bytes)

/* ---- TIMING SECTORS (3) ---------------------------------------------------
 * Sectors are NOT in memory as boundaries; the game reads them from the
 * track's .AIW file, and so does F1CUILive. The path to the AIW currently
 * loaded is a plain NUL-terminated ASCII string in the exe's data segment,
 * relative to the game directory, e.g.
 *   SEASONDATA\CIRCUITS\Italy\2001_Monza\2001_Monza.AIW
 *
 * Address is FACT: F1CUILive's f1c_memoryReaderEngine.postProcessInput reads
 * `baseMemoryAddress + 4899896` as a string and tests `.endsWith(".AIW")` to
 * detect a track change, with baseMemoryAddress = findModuleBase(exe). The exe
 * is non-ASLR so that base is 0x400000, giving 0x400000 + 0x4AC438 = 0x8AC438
 * -- which the corpus already names AIWFilePathBuffer, and which read back
 * exactly the Monza path above live on 2026-07-28.
 *
 * The three keys inside the AIW are, despite the "_length" names, CUMULATIVE
 * distances from the start line (Monza: 1716.87 / 3605.15 / 5781.26). So the
 * sector boundaries are simply sector_1_length and sector_2_length, and
 * lap_length closes sector 3. This matches F1CUILive's
 * buildGapZoneArray(s1, s2, lap). */
#define GAME_ADDR_AIW_PATH 0x008AC438u
#define ADDR_AIW_PATH ((uintptr_t)mirror_ADDR_AIW_PATH.bytes)
#define STW_OFF_SEG_SCALE      0x108
#define STW_OFF_SEG_COUNT      0x10C
#define STW_OFF_SEG_ARRAY      0x110
#define STW_OFF_SEG_PREV       0x114
#define FLAG_GREEN             0
#define FLAG_YELLOW            2
#define MAX_SEGMENTS           128

#define GAME_ADDR_AI_DB_BASE 0x005EA2C0u
#define ADDR_AI_DB_BASE ((uintptr_t)mirror_ADDR_AI_DB_BASE.bytes)   /* dword_5EA2C0, "SetupScenePtr" AI database singleton */
#define OFF_WP_COUNT           0x130
#define OFF_WP_ARRAY           0x134
#define WP_STRIDE              316
#define WP_OFF_X               0x00
#define WP_OFF_Z               0x08
/* 4.4.0-rc.12 weather map: wp_path scalars at +0x18/+0x1C and wp_perp at
 * +0x20 (x,z) in the same 316-byte record; the game's active line is
 * wp_pos + lerp(pathA,pathB,1-grip)*perp (docs/52-aiw-navigation-static-
 * architecture.md section 4.5/4.6, endpoint verified live there). */
#define WP_OFF_PERP            0x20
#define WP_OFF_PATH            0x18
#define MAX_WAYPOINTS           3000
/* ---- weather globals -------------------------------------------------------
 * ABSOLUTE addresses, each proven by how the game's own decompiled code uses it.
 * They were previously read as base+offset from an assumed "weather struct" at
 * 0x8AFEC0 justified by a name in docs/26 -- but that name column is copied from
 * docs/12, which is Ghidra+LLM naming inherited from the deprecated
 * F1C_ghidra_project, i.e. exactly the tier this corpus says not to trust. Two of
 * those names are simply wrong: 0x8ACEE0 is the Steward, not "ForceFeedbackBase",
 * and 0x8AFEF4 is a temperature, not "Flt_PhysicsBrakeBalance". Grounded instead
 * in the decompiled functions:
 *
 *   0x8AFEF0, 0x8AFEF4  TEMPERATURES in degrees C. Every physics consumer adds
 *     273.16 to convert to Kelvin -- WheelTireForceUpdate, SuspensionAndWheelPhysics,
 *     EngineStressSimulation, VehicleDynamicsForceAccumulatorIntegration -- and
 *     PitRaceStatusMessageDispatcher tests `flt_8AFEF0 > 30.0` for a hot-weather
 *     message. Live in-race: 16.4 and 29.6, i.e. air and track.
 *   0x8AFEE8, 0x8AFEEC  WETNESS pair, 0..1. Consumed as a grip multiplier
 *     `1.0 - (a + b) * 0.5` in VehicleMainPhysicsUpdate, SurfaceHeightfieldCollision-
 *     Callback and MultiPhaseProcessingStateMachine; PitRaceStatusMessageDispatcher
 *     fires when the mean exceeds 0.7; AIPushLap gates on 0.01 / 0.2 / 0.33. Both
 *     read 0.0 in the dry session on screen.
 * This is why the dashboard uses these and not TrackWetnessFactor (0x7CF3A4),
 * which sat at a constant 0.50 in that same dry session. */
#define GAME_ADDR_AMBIENT_TEMP 0x008AFEF0u
#define ADDR_AMBIENT_TEMP ((uintptr_t)mirror_ADDR_AMBIENT_TEMP.bytes)
#define GAME_ADDR_TRACK_TEMP 0x008AFEF4u
#define ADDR_TRACK_TEMP ((uintptr_t)mirror_ADDR_TRACK_TEMP.bytes)
#define GAME_ADDR_ON_WETNESS 0x008AFEE8u
#define ADDR_ON_WETNESS ((uintptr_t)mirror_ADDR_ON_WETNESS.bytes)
#define GAME_ADDR_OFF_WETNESS 0x008AFEECu
#define ADDR_OFF_WETNESS ((uintptr_t)mirror_ADDR_OFF_WETNESS.bytes)

/* --- PLAYER WHEEL THERMAL BLOCK (brake + tyre temperatures) -------------
 * A flat global array of 4 per-wheel records, NOT part of the vehicle
 * struct — it mirrors the PLAYER car only, which is the same scope the
 * in-car HUD draws from.
 *
 * Base 0x007C7A00, stride 1124 (0x464) bytes, 4 wheels.
 *   +0x00  double  tyre temperature, carcass zone 0   (Kelvin)
 *   +0x08  double  tyre temperature, carcass zone 1   (Kelvin)
 *   +0x10  double  tyre temperature, carcass zone 2   (Kelvin)
 *   +0x48  double  tyre tread remaining, 1.0 fresh
 *   +0xB8  double  brake disc temperature             (Kelvin)
 * Both HUD temperature renderers subtract 273.16 before formatting, so we
 * do the same and publish Celsius.
 *
 * Evidence, three renderers over the same block (static, live-read):
 *   BrakeWaterTemperatureHUDDisplay_0049db80.c prints dbl_7C7AB8,
 *     dbl_7C7F1C, dbl_7C8380, dbl_7C87E4 each minus 273.16 under the
 *     "Brakes" label — exactly base + 0xB8 + 1124*n.
 *   SuspensionCornerCheck_0049e570.c is the 3-strip tyre-temp bar:
 *     `*(double *)(1124*wheel + 8*zone + 8157696) - 273.16`, zone 0..2,
 *     and 8157696 == 0x7C7A00 == the record head.
 *   sub_49E7B0_0049e7b0.c is the tyre-wear bar:
 *     `(*(double *)(&unk_7C7A48 + 1124*wheel) - 0.75) * 4.0`.
 * Live 2026-07-28: temps read 84 C / brakes 43-65 C / tread 0.99999, all
 * four wheels distinct and plausible.
 *
 * TREAD SCALE (matches the physics in
 * VehicleDynamicsForceAccumulatorIntegration_00438130.c, which uses the
 * identical (x-0.75)*4.0 normalization): 1.0 = fresh, 0.75 = destroyed.
 * The game's own gauge is that normalized 0..1 fraction, so we publish it
 * rather than the raw double. Its landmarks:
 *   1.00  fresh, blister flag cleared
 *   0.80  HUD bar turns green->yellow
 *   0.70  crossing DOWN fires the LIGHT blister line (eid 95/97/99/101,
 *         or 105 if 3+ corners already flagged)
 *   0.50  HUD bar turns yellow->red
 *   0.40  crossing DOWN fires the SEVERE blister line (eid 94/96/98/100,
 *         or 104)
 *   0.00  raw tread hit 0.75: blowout. Grip *0.18, and several suspension
 *         and load terms rescaled — this is the flat-tyre state.
 * Both commentary lines are EDGE events, one tick as the value crosses
 * down, not a band the value sits inside.
 *
 * Wheel ORDER is DERIVED, not proven: the brake HUD emits wheels 0,1 on
 * one row and 2,3 on the next, i.e. FL,FR then RL,RR. Zone order (inner
 * to outer, or the reverse) is likewise unproven — the renderer just
 * paints them left to right. Confirm both live before labelling corners
 * in any UI. */
#define GAME_ADDR_WHEEL_THERMAL 0x007C7A00u
#define ADDR_WHEEL_THERMAL ((uintptr_t)mirror_ADDR_WHEEL_THERMAL.bytes)
#define WHEEL_THERMAL_STRIDE    1124
#define WH_OFF_TIRE_TEMP        0x00   /* 3 consecutive doubles */
#define WH_OFF_TREAD            0x48
#define WH_OFF_BRAKE_TEMP       0xB8
#define WH_OFF_CARCASS          0x18   /* W+868, FACT (NATIVE_SIMULATION_MAP) */
#define GAME_ADDR_WHEEL_LOAD    0x007C79D0u   /* W(0)+796 */
#define ADDR_WHEEL_LOAD ((uintptr_t)mirror_ADDR_WHEEL_LOAD.bytes)
#define WHEEL_COUNT             4
#define TIRE_TEMP_ZONES         3
#define KELVIN_TO_C             273.16
#define TREAD_DEAD              0.75   /* raw tread at 0% life */

/* Per-corner blister flags: 4 int32, 1 = this corner has been flagged.
 * AISessionPhaseCounter_0041aa10.c writes `this[corner + 657] = 1` with
 * this = dword_5EB978, and the physics clears `dword_5EC3BC[corner] = 0`
 * on a fresh tyre — 0x5EB978 + 4*657 == 0x5EC3BC exactly, and the exe
 * contains 54 `mov ecx, 0x5EB978` sites (it is a this-pointer to an
 * object AT that address, not a loaded value), so the two agree.
 *
 * CAVEAT: this array is indexed by CORNER ONLY, with no car index, while
 * the physics that writes it runs for every car on track. It is therefore
 * shared/last-writer, NOT player-specific. Published for diagnostics; do
 * not present it as the player's tyre state. */
#define GAME_ADDR_BLISTER_FLAGS 0x005EC3BCu
#define ADDR_BLISTER_FLAGS ((uintptr_t)mirror_ADDR_BLISTER_FLAGS.bytes)

/* Oil and water temperature as the GAME'S OWN steering-wheel display reads
 * them: two Kelvin doubles, player car only, printed by
 * BrakeWaterTemperatureHUDDisplay_0049db80.c under the "Water"/"Oil"
 * labels alongside the brake temps above.
 *
 * FACT, screen-confirmed 2026-07-28: the in-car wheel display read
 * WATER 23 C / OIL 66 C while these globals read 22.67 / 65.95, with the
 * four brake temps matching in the same frame.
 *
 * These are NOT the same quantity as the per-car vehicle fields
 * veh+0x3D10/0x3D14 (name_index "OilTemp"/"WaterTemp", DERIVED, located
 * by drift analysis and never cross-checked against a display). In that
 * same frame those read 134.76 and 66.09 — neither matches the gauge.
 * The vehicle fields stay in the per-car feed because they are the only
 * per-AI signal available; these globals are the authoritative PLAYER
 * values and are published separately. */
#define GAME_ADDR_OIL_TEMP_HUD 0x007C9174u
#define ADDR_OIL_TEMP_HUD ((uintptr_t)mirror_ADDR_OIL_TEMP_HUD.bytes)
#define GAME_ADDR_WATER_TEMP_HUD 0x007C91B4u
#define ADDR_WATER_TEMP_HUD ((uintptr_t)mirror_ADDR_WATER_TEMP_HUD.bytes)
#define ADDR_PLAYER_ENGINE_LIFE ((uintptr_t)mirror_ADDR_PLAYER_ENGINE_LIFE.bytes)

/* ---- 1.1.0 PLAYER FULL-PHYSICS OBJECT (vanilla data only) ----------------
 * P = 0x7C7428, the single static object VehiclePhysicsModeDispatcher 0x4E4930
 * runs for the car with veh+0x32C8 == 0 (the player). AI cars have none of
 * these channels. Wheel record W(w) = P + 652 + 1124*w, order FL,FR,RL,RR.
 * Every offset below was read live on 2026-10-13 (COMMUNITY_CT_INTEGRATION
 * sections 9-19, ct_probe runs 1-7); tiers in findings.jsonl.
 *   W+188  wheel spin rad/s (speed/|spin| = rolling radius, live)
 *   W+488  suspension deflection m (the game's own .spt recorder channel)
 *   W+788  camber, rad (DERIVED unit)       W+804 tyre deflection m
 *   W+820  optimum tyre temp K               W+884 hot pressure kPa
 *   W+892  garage pressure kPa               W+924 grip 0..1
 *   W+1068 brake disc thickness m            W+1092 disc failure thickness m
 *   P+5872+8k ride height m (4 floor points), byte P+5912 bottoming latch
 *   P+1124w+1104/1105 byte detach / bent flags, P+1124w+1116 bend angle
 *   P+0x00/08/10/18 used steer/throttle/brake/clutch; P+0x40/48/50/20 raw
 *   P+0x19C fuel, P+5212 compound index, P+5476 brake bias (rear share)
 *   P+6876 body drag, P+7060 body lift (both move with aero damage),
 *   P+7068 drag in use (ride-height dependent), P+7540 oil pressure,
 *   P+0x1C5C engine rad/s, P+0x1E38 clutch rad/s, P+7604 engine life s. */
#define GAME_ADDR_PLAYER_PHYS   0x007C7428u
#define ADDR_PLAYER_PHYS ((uintptr_t)mirror_ADDR_PLAYER_PHYS.bytes)
#define PP_D(off) (*(const double *)((const uint8_t *)ADDR_PLAYER_PHYS + (off)))
#define PP_B(off) (*((const uint8_t *)ADDR_PLAYER_PHYS + (off)))
#define PP_I(off) (*(const int *)((const uint8_t *)ADDR_PLAYER_PHYS + (off)))
#define PW(w, off) PP_D(652 + 1124 * (w) + (off))
#define AI_WB_SPIN     0x30   /* AI wheel spin rad/s (live corr 0.996 with speed) */
#define AI_WB_BRAKE_K  0x5C   /* AI brake temperature K, one value per axle (live) */

#define SESSION_TYPE      (*(volatile int*)ADDR_SESSION_TYPE)
#define RACE_PHASE        (*(volatile int*)ADDR_RACE_PHASE)
#define RACE_ACTIVE       (*(volatile int*)ADDR_RACE_ACTIVE)
#define VEHICLE_COUNT     (*(volatile int*)ADDR_VEHICLE_COUNT)
#define VEHICLE_PTR(i)    ((volatile uint8_t*)((volatile void**)ADDR_VEHICLE_PTR_ARRAY)[i])
#define TOTAL_LAPS        (*(volatile int*)ADDR_TOTAL_LAPS)
/* TYPE CORRECTIONS, live-verified 2026-07-25 (Frida read of both interpretations):
 *   TrackConditionState 0x7CF3C8 is a FLOAT (0.2732), NOT the 0..4 enum it was
 *     read as -- as int it returned 1049353456, which the UI then indexed into a
 *     5-entry weather label table.
 *   TrackWetnessFactor 0x7CF3A4 read a flat 0.50 in a bone-dry session; docs/12
 *     calls it a grip factor consumed by FUN_004c8920, not a wetness percentage.
 *     Real wetness comes from the weather struct (+0x28/+0x2C), which read 0.0.
 *   AIDifficultySlider 0x7D01A0 is an INT percent (92). Read as float it was
 *     1.29e-43 and printed as "0.0". docs/12 confirms integer use:
 *     `(AIDifficultySlider - 70) * ...` and `AIDifficultySlider < 100`.
 *   0x7D0410 is RaceMode, not a 0/1 championship bool: docs/12 DAT_007d2db0 notes
 *     "championship DAT_007d0410 == 4". Live single Grand Prix reads 3. */
#define TRACK_CONDITION   finite_float(*(const float*)ADDR_TRACK_CONDITION)
#define TRACK_GRIP_FACTOR finite_float(*(const float*)ADDR_TRACK_WETNESS)
#define RACE_MODE         (*(volatile int*)ADDR_CHAMP_MODE)
#define RACE_MODE_CHAMPIONSHIP 4
#define PLAYER_INDEX      (*(volatile int*)ADDR_PLAYER_INDEX)
#define AI_DIFFICULTY     (*(volatile int*)ADDR_AI_DIFFICULTY)
#define RACE_TIME_THRESH  finite_float(*(const float*)ADDR_RACE_TIME_THRESH)

/* ---- vehicle field offsets ---- */
#define OFF_PHYS_X          0x001C
#define OFF_PHYS_Z          0x0024
#define OFF_HUMAN_DRS       0x00C4
#define OFF_TRACK_POS       0x06CC
/* Distance travelled along the lap, in metres, resetting at the start line.
 * FACT f1c-lapdist-4044-not-4048 proves +0x4044 for every car (live
 * 2026-07-31). The old +0x4048 claim was verified on the player only;
 * native acceptance 2026-10-09 showed it sticking/jumping on AI cars.
 *
 * OFF_TRACK_POS (0x6CC) is NOT this. It sat frozen at 1333.08 across 1052
 * samples while the car did a lap at up to 86 kph, so whatever it is, it is
 * not lap distance -- it must not be used for sector assignment. */
#define OFF_TRACK_PART_EXACT 0x4044 /* per-car distance: FACT f1c-lapdist-4044-not-4048 */
#define OFF_TRACK_X         0x0744   /* coordinateInTrack x (CONFIRMED) */
#define OFF_TRACK_Z         0x074C   /* coordinateInTrack y (CONFIRMED) */
#define OFF_DIR_X           0x0710
#define OFF_DIR_Y           0x0718
#define OFF_RPM_LIMITER     0x1068
#define OFF_HITS_RACE       0x12B4
#define OFF_PENALTY_0       0x32FC
#define OFF_PENALTY_1       0x3300
#define OFF_PENALTY_2       0x3304
#define OFF_NEXT_CHOSEN     0x3378
#define OFF_UNLOCK_TIRES    0x3428
#define OFF_PIT_STOP_SPEED  0x346C
#define OFF_PIT_TIME_2      0x3470
#define OFF_LAST_PIT_TIME   0x3474
#define OFF_PIT_TIME_1      0x3480
#define OFF_NEXT_STINT_FUEL 0x3484
#define OFF_PITTING_STATUS  0x3450
#define OFF_PIT_TIME_0      0x34A0
#define OFF_THROTTLE        0x357C
#define OFF_BRAKE           0x3584
/* ---- ENGINEERING COMPARISON CHANNELS (see CHANNEL_LEDGER_2026-10-09.md) ----
 * All of these are inside the 0x48C0-byte vehicle record the capture already
 * copies into owned mirrors, so no new live read path is introduced.
 *   OFF_STEER_INPUT 0x3580: AI steering input factor. FACT for AI
 *     (ai-accel-inputs-mass-live-2026-09-30; AIMainUpdate writes it every AI
 *     tick). The player-side writer is NOT proven, so the sample validity bit
 *     is only set for cars under AI control (see OFF_AI_CONTROL_FLAG).
 *   OFF_ACCEL_LAT 0x354C / OFF_ACCEL_VERT 0x3550 / OFF_ACCEL_LON 0x3554:
 *     car-local acceleration, m/s2. FACT both for AI and for the player block
 *     (ai-accel-inputs-mass-live-2026-09-30, player-live-telemetry-f2002).
 *     Forward longitudinal is NEGATIVE in the native sign; the UI converts.
 *   Heading is a yaw rotation matrix at 0x34BC..0x34E0
 *     (f1c-car-heading-is-a-rotation-matrix-not-the-yaw-field, FACT):
 *     +0x34BC cos, +0x34C4 sin, +0x34D4 -sin, +0x34DC cos. */
#define OFF_STEER_INPUT     0x3580
#define OFF_ACCEL_LAT       0x354C
#define OFF_ACCEL_VERT      0x3550
#define OFF_ACCEL_LON       0x3554
#define OFF_HEADING_COS     0x34BC
#define OFF_HEADING_SIN     0x34C4
/* 4.4.0-rc.5 (Batch 2h): race number, CONFIRMED finding
 * ai-team-order-unknowns-solved-4710-car-number-2026-10-03. */
#define OFF_RACE_NUMBER     0x4710
/* ---- 4.3.0 RESEARCH channels (CHANNEL_LEDGER section 1.3) ----
 * Vehicle wheel array veh+0x358C, stride 0xF8 (CONFIRMED,
 * vehicle-wheel-block-roles-corrected-20261009). Array index 0..3 is RR, RL,
 * FR, FL per F1CUI tread labels (order not independently proven). The fields
 * below are captured ONLY so one guarded owner session can prove or disprove
 * them; every one is tier RESEARCH and the viewer hides it unless the user
 * enables research channels. None is a FACT; none is published as a unit.
 *   +0x10 suspension deflection candidate (correlation, rises with speed)
 *   +0x44 vertical-load candidate (load-like, unit unproven)
 *   +0x38 slip-angle-like candidate (+-0.09)
 *   +0x50 tyre temperature candidate: SuspensionAndWheelPhysics_004e5f60
 *         initialises it to ambient + 273.16 (Kelvin) at session setup
 *   +0xD8 per-wheel force parameter (WheelForceParameterSet_004f1ba0; the
 *         player branch forwards the same four values to the rigid body) */
#define OFF_WHEEL_BASE      0x358C
#define WHEEL_BLOCK_STRIDE  0xF8
#define WB_OFF_SUSP         0x10
#define WB_OFF_SLIP         0x38
#define WB_OFF_LOAD         0x44
#define WB_OFF_TEMP_K       0x50
#define WB_OFF_FORCE        0xD8
#define OFF_TIRE_CHOICE     0x397C   /* INT32, live-verified (read as float it was 1.4e-45 -> cast to 0 -> blank compound) */
#define OFF_FUEL_CONS       0x3990
/* These are TIRE WEAR (remaining tread, 1.0 = fresh), not pressures. name_index.json
 * calls them tirePuncture_* but its own note says the field is a "tire wear-rate" and
 * that F1CUI fakes a puncture by driving it to minimum. Live 2026-07-25, lap 1:
 * player (stationary, out lap) 1.0000 on all four; Schumacher 0.99998 FL/FR,
 * 0.99999 RL/RR -- i.e. it does move, fronts faster than rears, exactly like wear.
 * Displayed as "pressure" it looked like a constant 1.00 and read as broken. */
/* 1.1.0 wheel-order correction (live 2026-10-13, COMMUNITY_CT_INTEGRATION
 * section 20): the vehicle wheel block is FL,FR,RL,RR from index 0 - the
 * player's block copies W(0..3) spin to 0.001 rad/s at indices 0..3, and on
 * AI cars only indices 0,1 steer (+0x28/+0xC0) while 2,3 carry the driven-
 * wheel inertia (+0x68). The former F1CUI labels (index 0 = RR) mirrored
 * every AI corner. */
#define OFF_TIRE_WEAR_FL    0x363C
#define OFF_TIRE_WEAR_FR    0x3734
#define OFF_TIRE_WEAR_RL    0x382C
#define OFF_TIRE_WEAR_RR    0x3924
/* ---- COMPONENT LIFE COUNTDOWNS (seconds remaining) -------------------------
 * FOUND + POKE-PROVEN 2026-07-25, see investigations/engine_lifetime/
 * LIFETIME_COUNTER_FOUND_2026-07-25.md. FuelConsumptionAndWear_004ed960 does
 * `this[i+3929] -= dt` for i=0..3 and, on the >=0 -> <0 transition, writes the
 * matching ReasonCode and fails that component.
 * OFFSET TRAP: the decompiled `this` is veh+0xC, not the vehicle base (proved by
 * `*(_DWORD*)this + 16488` landing on ReasonCode at veh+0x4068), so index 3929
 * is veh+0xC+3929*4 = veh+0x3D70. Reading the naive veh+0x3D64 gives
 * identical-looking garbage for every car.
 * Live: healthy cars 20000-41000 s; a just-blown engine reads -1.0.
 * These are the REAL health numbers -- OilTemp only feeds the burn rate, and
 * pinning it at 200 C for 24 s killed nothing. */
/* Wear-tick gate. MainGameTickUpdate_0042c690 runs the wear/countdown pass only
 * for cars whose veh+0x32C8 == 1: `if (*(_DWORD*)(veh+13000) == 1) FuelConsumptionAndWear(1.0)`.
 * LIVE 2026-07-25: every AI car reads 1, the HUMAN reads 0 — so the component
 * countdowns never tick for the player, and OilTemp (0x3D10) is likewise left
 * frozen at whatever it last held. Confirmed by watching a player engine blow
 * up: the counter sat at 23245.8 untouched throughout, then jumped straight to
 * -1.0 at the moment of failure. The player's engine died by the OVERHEAT path
 * (in-game high-temp warning), not by this countdown expiring. */
/* Scheduled-failure table, 5 entries of { int lap; int componentIdx } at stride 8.
 * AIMainUpdate_00424e70 walks it (`v22 = this + 16712`, 5 iterations, v22 += 2) and
 * when the car's current lap equals an entry's lap it OVERWRITES that component's
 * countdown with a small random value, so the part fails during that lap. This is
 * the game pre-scripting AI retirements. Every entry read lap = -1 (unarmed) in the
 * sessions sampled so far, so it is logged to find out when, if ever, it gets set. */
#define OFF_SCHED_TABLE     0x4144
#define SCHED_ENTRIES       5
#define OFF_WEAR_GATE       0x32C8
#define OFF_LIFE_ENGINE     0x3D70   /* -> ReasonCode 1 Engine */
#define OFF_LIFE_BRAKES     0x3D74   /* -> ReasonCode 5 Brakes */
#define OFF_LIFE_GEARBOX    0x3D78   /* -> ReasonCode 2 Gearbox */
#define OFF_LIFE_SUSPENSION 0x3D7C   /* -> ReasonCode 4 Suspension */
#define OFF_OIL_TEMP        0x3D10   /* DERIVED 2026-07-25: rises while running, frozen on a dead engine; baseline differs per engine manufacturer (each team engine.ini has its own OptimumOilTemp). engine.ini LifetimeOilTemp=(123.43,2.90) halves engine life per 2.9C above 123.43 */
#define OFF_WATER_TEMP      0x3D14   /* DERIVED: same drift pattern, lower magnitude */
#define OFF_FUEL_LEVEL      0x3984   /* DERIVED: decreases steadily while running, frozen when stopped -- litres remaining */
#define OFF_ENGINE_ANGVEL   0x00A0   /* EngineAngVel (FACT) - 0 when the engine is genuinely dead */
#define OFF_ENGINE_RPM      0x39A0
#define OFF_ENGINE_MAX_RPM  0x39A4
#define OFF_CURRENT_GEAR    0x39FC   /* INT32 (name_index: CurrentGear, type int32, FACT) -- was read as float, always cast to 0 -> "N" */
#define OFF_CAR_CONDITIONS  0x3D6C
/* ---- Per-part damage records (CONFIRMED, name_index.json) ----------------
 * Source: mods/investigations/part_detach_commentary/README.md (2026-07-24),
 * proven by a Frida hook on RepairServiceStateDispatcher_004ed250 logging the
 * raw partIdx argument, cross-checked with tools/wing_record_probe.py.
 *
 * Two records, stride 0x44. Flag semantics: 1 = intact, 0 = ARMED (detached).
 *   A = front wing: ptr 0xCD4, flag 0xD14 -- armed by partIdx 4
 *   B = rear  wing: ptr 0xD18, flag 0xD58 -- armed by partIdx 5
 *
 * The asymmetry that matters: PitStopBlockingCondition_0042aee0 tests ONLY
 * record B (`*(veh+0xD18) && !*(byte*)(veh+0xD58)`). So a detached FRONT wing
 * gets repaired and the car rejoins; a detached REAR wing makes the blocking
 * condition true at the pit box and PitStopDecisionTireCompound_0042a3d0
 * retires the car. That single fact lets us PREDICT a retirement before the
 * game performs it.
 *
 * IMPORTANT: these flags RESET to 1 once the retirement completes, so the
 * observable window is only detach -> pit box. They must be latched as they
 * happen (done in the 60Hz tick thread below), never polled after the fact --
 * post-hoc polling is exactly why earlier sessions saw nothing on retired cars.
 */
/* Engine seized on impact. DERIVED, single controlled trial 2026-07-25 -- set 0->1
 * when collision impulse exceeds Engine=10000.0 in SeasonData/damage.ini (~150 km/h
 * head-on). Control: a car that lost MORE parts in a lesser impact kept this at 0.
 * A seize is NOT a retirement: the car freezes (throttle/gear 0, rpm at idle) while
 * activeFlag stays 1 and finishState stays 0, so nothing else in the struct marks it.
 * See investigations/engine_seize/README.md. Not poke-proven; treat as provisional. */
/* DISPROVEN 2026-07-25. Live read at the START of a race, every car pristine,
 * damage.ini wings disabled: slots 3 (Coulthard) and 5 (TesterA) already read 1
 * while slots 0/1/2/4 read 0. A seize flag cannot be set on a stationary,
 * undamaged car on the grid. Whatever 0x4574 is, it is a static per-car property,
 * not impact state -- and because the DLL LATCHED it and fed it into `doomed`,
 * those cars were permanently reported DOOMED / ENGINE STOPPED from lap 0.
 * Kept as a raw passthrough field only; it no longer drives damage or prognosis. */
#define OFF_UNKNOWN_4574    0x4574
#define OFF_FRONTWING_PTR   0x0CD4
#define OFF_FRONTWING_FLAG  0x0D14
#define OFF_REARWING_PTR    0x0D18
#define OFF_REARWING_FLAG   0x0D58
#define OFF_LAP_NUMBER      0x403C
#define OFF_LAST_LAP        0x404C
#define OFF_LAST_S1         0x4050
#define OFF_LAST_S2         0x4054
#define OFF_PLANNED_POS     0x4058
#define OFF_QUALY_TIME      0x405C
#define OFF_POSITION        0x4060
/* 4.4.0-rc.5 garage order (DERIVED: findings f1c-pit-box-lookup-via-veh-48a4,
 * ai-team-order-rank-is-garage-bay-48a8). 0x48A4 = AIW team index, 0x48A8 = garage bay. */
#define OFF_GARAGE_TEAM     0x48A4
#define OFF_GARAGE_BAY      0x48A8
#define OFF_DNF_DSQ         0x4063
#define OFF_FINISH_STATE    0x4064
#define OFF_REASON_CODE     0x4068
#define OFF_PENALTIES       0x4070
#define OFF_LAP_OF_RUN      0x4094
#define OFF_TOTAL_TIME_SF   0x4098
#define OFF_BEST_LAP        0x409C
#define OFF_BEST_S1         0x40A0
#define OFF_BEST_S2         0x40A4
#define OFF_TRACK_LIMITS    0x4104
#define OFF_RETIRED_FLAG    0x4410
#define OFF_SPEED           0x4440
#define OFF_TRACK_FROM_PIT  0x4450
#define OFF_DRIVER_STATUS   0x4510
#define OFF_TIME_NEXT_EXIT  0x4534
#define OFF_MAX_SPEED       0x4538
#define OFF_CALL_TO_BOX     0x454C
#define OFF_ELAPSED_TIME    0x4570
/* MISNAMED. Live 2026-07-25, player driving in P1 at 79 km/h, finishState 0,
 * retired 0: this reads 0 for the PLAYER and 1 for every AI car. A retired AI
 * also drops to 0. So it tracks "this car is under AI control", not "this car is
 * on track". Everything that treated 0 as off-track was therefore wrong about the
 * player specifically -- the map skipped the player's dot entirely, and the pit
 * heuristic `!active && !retired && fs==0` marked the human as permanently in the
 * pits. Use on_track (below) for presence; keep this only as the AI-control bit. */
#define OFF_AI_CONTROL_FLAG 0x45F4
/* Controller mode veh+0x32C8 (FACT static, INVESTIGATION_NET_SYNC_2026-10-09):
 * -1 inactive, 0 local human, 1 AI, 2 remote (network), 3 replay. Mode 2 runs
 * no physics on this PC (0x4E4930); pose, velocity, wheels and rpm are
 * written from network records (0x4C73F0). Live 2026-10-11 CO-OP host read:
 * player 0, bots 1, and a retired bot with ai_control 0 still in mode 1, so
 * ai_control alone does not identify the local player. */
#define OFF_CONTROLLER_MODE 0x32C8
#define CTRL_LOCAL  0
#define CTRL_REMOTE 2
#define OFF_DRIVER_NAME     0x4620
#define OFF_CAR_NAME        0x4678
#define OFF_DRIVER_ID       0x489C   /* FACT (F1CVehicle.DriverID, roster index) */
#define OFF_NUMBER_OF_LAPS  0x4660
#define OFF_SPEED_CAP       0x453C
#define OFF_AI_PACE         0x457C
/* write-side offsets — race control actions, ported from mods/red_flag & mods/ai_pit_retire.
 * "Armed trigger" pattern (write 0xD58/0x454C, let the game's own pit/retire logic execute
 * it) is used instead of forging FinishState/RetiredFlag/ActiveFlag directly — forging
 * produces a car frozen mid-track or parked in the wrong spot (documented failure in
 * mods/ai_pit_retire/RETIRE_SOLVED_2026-07-24.md). UNTESTED in this DLL — ported from
 * mods that proved the mechanism works, but never fired from web_telemetry.c until now. */
#define OFF_REAR_WING_PTR    0x0D18
#define OFF_REAR_WING_INTACT 0x0D58

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */
#ifndef HTTP_PORT
#define HTTP_PORT         8080
#endif
/* Runtime HTTP port: web_telemetry.ini [Mod] Port (1024-65535). The compiled
 * HTTP_PORT stays the default. On bind failure the listener retries the next
 * up-to-5 ports; if none binds, HTTP is disabled and capture keeps running. */
#define HTTP_PORT_MIN     1024
#define HTTP_PORT_MAX     65535
#define HTTP_PORT_FALLBACK_ATTEMPTS 5
static int g_configured_port = HTTP_PORT;
/* rc.17 (owner 2026-10-12): listen on this PC only by default; [Mod] Remote=1
 * opens LAN / Tailscale access on [Mod] BindAddress only (security review
 * 2026-10-13, M1). Remote=1 without a private BindAddress keeps this PC only. */
static int g_remote_access = 0;
static char g_bind_cfg[32] = "";           /* [Mod] BindAddress as read from the ini */
static char g_bind_text[48] = "127.0.0.1"; /* effective listen address (Host allowlist, /health) */
static ULONG g_bind_s_addr = 0;            /* effective bind address (network order); 0 = loopback */
static volatile LONG g_http_bound_port;   /* 0 until the listener binds */
static int g_settings_loaded;
static void dcam_settings_read(const char *ini_path);   /* director_state.inc: [Director] Enabled (TV Director phase 0) */
#define SSE_UPDATE_MS     100
#define TICK_SLEEP_MS     16
#define MAX_SSE_CLIENTS   16
#define MAX_CARS          104
#define NAME_LEN          64

/* ========================================================================
 * TELEMETRY BUFFER
 * ======================================================================== */
typedef struct {
    char  name[NAME_LEN];
    char  car_name[NAME_LEN];
    int   position, lap_number, finish_state, gear;
    int   ai_control;              /* 0x45F4: 1 = under AI control. 0 for the human driver. */
    int   on_track;                /* derived presence: not classified out, not retired */
    int   segment;                 /* track sector this car is in */
    int   under_yellow;            /* the game's own HUD test applied to THIS car */
    float speed, rpm, max_rpm;
    float best_lap, last_lap, total_time;
    float best_s1, best_s2, last_s1, last_s2;
    int   tire_choice, penalties, call_to_box, retired;
    float throttle, brake, pit_time_0;
    int   garage_team, garage_bay;  /* 4.4.0-rc.5 additive, DERIVED garage order */
    /* pittingStatus: CONFIRMED int32 in name_index.json (0 none, 1 driving/
     * requested, 2 entering lane, 3 stopped in box, 4 exiting). Was read as
     * a FLOAT here (RF) instead of RI -- an int enum value like 3 reinterpreted
     * as float bit-pattern is a denormal near 4.2e-45, which is indistinguishable
     * from 0 in every `>= 2.0f` comparison and every %.0f print. CAUGHT LIVE
     * 2026-07-28: two real, confirmed pit stops (one tire+fuel, one
     * tire+fuel+wing) both showed pitting==0 across 1000+ continuous 100ms
     * samples with zero gaps, while tire_choice and fuel_level moved
     * correctly in the SAME struct read -- same bug class the corpus already
     * caught and fixed for tireChoice ("read as float it was 1.4e-45"). */
    int   pitting;
    float phys_x, phys_z, dir_x, dir_y;
    float track_pos;
    float lap_dist;                /* metres along the lap, OFF_TRACK_PART_EXACT */
    float track_x, track_z;

    /* v3 additions */
    float elapsed_time, max_speed;
    int   driver_status, planned_position, reason_code;
    float qualy_time;
    int   lap_of_run, car_conditions, track_limits;
    float fuel_consumption;
    int   hits_in_race, rpm_limiter, human_drs, dnf_dsq;
    float track_from_pit;
    float pit_time_1, pit_time_2, last_pit_time, pit_stop_speed;
    float next_stint_fuel;
    int   penalty_0, penalty_1, penalty_2;
    float tire_wear_fl, tire_wear_fr, tire_wear_rl, tire_wear_rr;
    float time_next_exit;
    int   number_of_laps;
    float next_chosen, unlock_tires;
    float speed_cap, ai_pace;
    /* per-part damage: live flags + latched history (flags are transient) */
    int   fw_flag, rw_flag;        /* 1 = intact, 0 = armed/detached, -1 = unreadable */
    int   fw_lost, rw_lost;        /* latched: this part came off at some point */
    int   raw_4574;                /* raw passthrough, meaning unknown — see OFF_UNKNOWN_4574 */
    float eng_angvel;              /* 0 = engine actually stopped */
    float oil_temp, water_temp, fuel_level;
    float life_eng, life_brk, life_gbx, life_sus;   /* seconds remaining */
    int   lifepct_eng, lifepct_brk, lifepct_gbx, lifepct_sus;
    int   wear_gate;               /* 1 = the countdown wear model runs for this car */
    int   eng_dead;                /* engine stopped while car still nominally racing */
    int   dmg_tier;                /* 0 none 1 bodywork 2 fwing 3 rwing 4 wheel 5 engine stopped */
    int   doomed;                  /* the game WILL retire this car */
    int   doom_cause;              /* DOOM_* — never report doomed without this */
    int   dmg_preexisting;         /* bit0 front wing, bit1 rear wing: already gone when we attached */

    /* ---- v4 additions: all computed by US from real per-tick reads, none
     * of these are game memory fields. Each block says exactly what it is
     * derived from so a wrong inference is traceable, not laundered into
     * a fact. ---- */

    /* RADAR: gap to the car immediately ahead/behind ON TRACK, in metres of
     * lap_dist, corrected for lap number so a lapped car doesn't show as
     * adjacent. REAL distance, not an interpolated time gap. -1 = none
     * found (alone on track, or gap too large to be meaningful). */
    float gap_ahead_m, gap_behind_m;
    int   gap_ahead_id, gap_behind_id;

    /* LIVE SECTOR DELTA: while sector N is in progress, elapsed time in that
     * sector so far minus this car's PERSONAL BEST time for sector N. Green
     * (negative) or red (positive), same convention as real F1 telemetry.
     * Needs a per-car sector-entry timestamp, tracked below in the top-level
     * telemetry_t (session-scoped state, not per-JSON-row). */
    float live_sector_delta;   /* seconds, 0 if no personal best yet for this sector */
    int   live_sector_num;     /* which sector this delta is for; 0 = none live */

    /* PIT ANTICIPATION: the REAL signal is call_to_box (already read above,
     * the game's own intention flag). This adds a labelled HEURISTIC on top
     * for cars the game hasn't called in yet, so the two are never
     * confused: box_soon_reason is empty unless we are guessing. */
    int   box_soon;                 /* 1 = our heuristic thinks a stop is close */
    char  box_soon_reason[32];      /* "low fuel" / "tyre life <15%" / "" */

    /* TOP SPEED: the game's own veh+0x4538 (max_speed, above) reads a
     * constant 99999 sentinel — NOT real telemetry — so this is tracked
     * independently as a running max of the real speed field. */
    float top_speed_lap, top_speed_session, top_speed_prev_lap;

    /* FUEL MODEL: rolling per-lap consumption (fuel at lap start minus fuel
     * at lap end, averaged over the last few completed laps) rather than
     * the instantaneous fuel_consumption rate above, plus the laps of
     * autonomy that average implies at the current fuel level. */
    float fuel_avg_per_lap;
    float fuel_autonomy_laps;

    /* CONTACT ATTRIBUTION: set for one tick when this car's damage changed
     * within CONTACT_RADIUS_M of another car's simultaneous damage change.
     * HEURISTIC (proximity + coincidence in time), not a decoded game flag —
     * the game exposes no guilty/victim record. contact_with is the other
     * car's id, -1 if none this tick. */
    int   contact_with;

    /* ---- v5 (4.2.0): engineering comparison channels ----
     * Every field here has an explicit evidence tier in
     * CHANNEL_LEDGER_2026-10-09.md. car_valid is a bitfield so the UI can say
     * "unavailable" instead of rendering a zero:
     *   1 speed, 2 throttle, 4 brake, 8 steering, 16 tread, 32 acceleration. */
    int   car_valid;
    float steer;                   /* AI input factor 0x3580; invalid for the player */
    float glat, glon, gvert;       /* m/s2, native signs (forward lon = negative) */
    float yaw;                     /* rad, derived from the heading matrix */
    int   race_num;                /* 4.4.0-rc.5 additive: veh+0x4710 race number, CONFIRMED (-1 = out of range) */
    float yaw_rate;                /* 4.4.0-rc.5 additive: rad/s, DERIVED from yaw between ticks (vf6 bit 2048) */
    uint32_t team_id;              /* broker ABI-8 team hash; 0 = unknown */
    int   controller_role;         /* broker: -1 inactive, 0 human, 1 AI, 2 remote */
    float sector_elapsed;          /* current sector elapsed seconds (C02) */
    float last_sector_delta;       /* last completed sector minus its PB (C02) */
    int   last_sector_num;         /* which sector last_sector_delta refers to */

    /* ---- v6 (4.3.0): viewer channels. Wheel order FL,FR,RL,RR. ----
     * ttemp: tyre surface temperature C, mean of the three zones (player:
     *   FACT thermal block; AI: RESEARCH wheel+0x50 Kelvin candidate).
     * tcarc: player carcass temperature C (FACT W+868); -999 otherwise.
     * rx_*: RESEARCH raw candidates (see OFF_WHEEL_BASE). AI only except
     *   rx_ld, which for the player is W+796 (the load term of the optimum
     *   pressure formula, unit unknown).
     * vf6 bits: 1 vertical accel, 2 yaw, 4 tyre temp FACT, 8 carcass,
     *   16 research susp, 32 research load, 64 research slip,
     *   128 research force, 256 research AI tyre temp, 512 CO-OP remote copy,
     *   1024 remote lap distance DERIVED, 2048 yaw rate, 4096 (1.2.2) remote brake on/off only. */
    int   vf6;
    float ttemp[4], tcarc[4];
    float rx_su[4], rx_ld[4], rx_sl[4], rx_fo[4];
    /* 1.1.0: AI brake temperature C per axle [front, rear] (wheel +0x5C,
     * -999 = none/player) and wheel spin rad/s FL,FR,RL,RR (+0x30). */
    float abt[2], aws[4];
} car_telemetry_t;

typedef struct {
    int   session_type, race_phase;
    int   race_active;             /* normalized 0/1 */
    int   race_active_raw;         /* raw 0x7D0308 — reads 3 on track (docs/38), never 1 */
    int   vehicle_count, total_laps;
    int   race_mode, player_index; /* race_mode: 4 = championship (docs/12), 3 = single GP */
    int   player_slot;             /* DERIVED vehicle slot of the human — see player_slot_update() */
    /* track-sector flags, read straight from the Steward every tick */
    int   seg_count;
    float seg_len;                 /* units per segment (1/scale) */
    float track_len;               /* seg_count * seg_len */
    int   seg_state[MAX_SEGMENTS];
    int   yellow_count;            /* how many segments are currently yellow */
    int   ai_difficulty;           /* integer percent, e.g. 92 */
    float track_condition;         /* float, NOT the 0..4 weather enum */
    float track_grip_factor;       /* 0x7CF3A4 — grip factor, not a wetness percentage */
    float race_time_threshold;
    double session_time;           /* 0x8AFEA0, resets on restart */
    DWORD tick;
    float ambient_temp, track_temp, on_wetness, off_wetness;
    /* PLAYER ONLY — the game keeps no per-AI thermal mirror. Celsius.
     * Index order is wheel index as the game stores it (see
     * ADDR_WHEEL_THERMAL): DERIVED as FL, FR, RL, RR. */
    float brake_temp[WHEEL_COUNT];
    float tire_temp[WHEEL_COUNT][TIRE_TEMP_ZONES];
    float tread[WHEEL_COUNT];          /* normalized 1.0 fresh .. 0.0 blown */
    int   blister[WHEEL_COUNT];        /* shared/last-writer, see ADDR_BLISTER_FLAGS */
    float oil_temp_hud, water_temp_hud; /* player, matches the in-car gauge */
    double player_engine_life_s;        /* player global G_EngineLifeSeconds (SDK FACT) */
    /* 1.1.0 player chassis, see PLAYER FULL-PHYSICS OBJECT. Wheel order
     * FL,FR,RL,RR. pch_valid = 0 outside a driven session (block zeroed). */
    int   pch_valid;
    float pch_ride_mm[4], pch_susp_mm[4], pch_tdefl_mm[4];
    float pch_press[4], pch_press_set[4], pch_grip[4], pch_opt_c[4], pch_camber_deg[4];
    float pch_spin[4], pch_roll[4], pch_disc_mm[4], pch_disc_fail_mm[4], pch_bend[4];
    int   pch_bent[4], pch_detach[4];
    int   pch_bottom, pch_compound;
    float pch_eng_wear;                 /* engine life lost per second (s/s), smoothed */
    float pch_fuel, pch_rpm, pch_clutch_rpm, pch_oil_press, pch_bias;
    float pch_drag, pch_lift, pch_drag_used;
    float pch_in_raw[4], pch_in_used[4];   /* throttle, brake, steering, clutch */
    /* timing sectors, from the track's .AIW -- cumulative metres from the
     * start line. 0 = not loaded / AIW unreadable. */
    float sec1, sec2, lap_len;
    char  aiw_path[260];
    char  track_name[64];

    /* Fastest lap of the SESSION seen so far, across every car. Whether this
     * game's ruleset awards a bonus point for it is unknown, so this is
     * published as plain information, not as a scored bonus. */
    float fastest_lap_time;
    int   fastest_lap_car;      /* -1 = none set yet */

    /* Driver of the Day: OUR OWN transparent composite score, NOT the
     * game's internal calc (f1c_driverOfTheDayEngine.calc(float) is a
     * private scoring formula we have not decoded). Recomputed periodically
     * from data we already have: net positions gained since the start,
     * clean laps (no incident attributed this lap), and holding fastest lap. */
    int   dotd_car;
    int   dotd_score;

    car_telemetry_t cars[MAX_CARS];
} telemetry_t;

/* Per-car session-scoped state that doesn't belong in the JSON row itself:
 * timestamps and rolling history needed to COMPUTE the v4 fields above. */
typedef struct {
    float sector_entry_t[4];      /* session time this car entered sector 1/2/3 (index 1..3) */
    float sector_pb[4];           /* personal best time for that sector, 0 = none yet */
    int   prev_sector;
    float fuel_at_lap_start;
    float fuel_hist[8];           /* per-lap consumption, ring buffer */
    int   fuel_hist_n, fuel_hist_i;
    int   prev_lap_number;
    int   start_position;         /* position on the first tick we saw this car, for DOTD */
    int   clean_lap;              /* 1 until an incident is attributed this lap */
    int   prev_dmg_tier;          /* for contact-attribution edge detection */
    int   prev_box_soon;
} car_state_t;
static car_state_t g_car_state[MAX_CARS];
static int g_car_state_init[MAX_CARS];

#define CONTACT_RADIUS_M 25.0f

/* clamp obviously-garbage reads from the unconfirmed weather base to a sentinel */
static float ws_sanity(float v) { return (v > -60.0f && v < 90.0f) ? v : -999.0f; }

/* Brake/tyre temps: plausible band is roughly ambient..1200 C. Anything
 * outside means the block is not being simulated right now. */
static float temp_sanity(double v) { return (v > -20.0 && v < 1500.0) ? (float)v : -999.0f; }

static telemetry_t      g_telemetry;
static CRITICAL_SECTION g_telemetry_cs;
static volatile BOOL    g_running = TRUE;
static char             g_log_path[MAX_PATH];
static char *esc_json(char *dst, int dsz, const char *src); /* fwd decl — full def is with the JSON builder below */

/* ========================================================================
 * PERSISTENT RACE LOG (JSONL, one line per event)
 *
 * Everything that happens during a race is captured as it happens: lap
 * completions (full per-lap detail, not just the time), incidents (same
 * feed as the live Race Control tab), periodic heartbeats (session/weather
 * state), and session start/end markers with a final classification.
 * Every line is flushed immediately — small writes, cheap fflush, safe
 * against the DLL being unloaded or the game crashing mid-race. Designed
 * for offline/LLM analysis: JSONL is trivially streamable/greppable/
 * line-parseable, no need to hold a whole file in memory to read it.
 * ======================================================================== */
/* 1.0.1: the journal is no longer one loose file per session reset beside the
 * DLL (1.0.0 left 798 files / 91 MB). Lines go to the session archive
 * (session_archive.inc: journal.jsonl inside the session folder), buffered in
 * memory and written by the worker outside g_telemetry_cs. g_race_log is now
 * only the "journal stream open" flag. */
static int   g_race_log = 0;
static DWORD g_last_heartbeat = 0;
static void ar_journal_line(const char *line, int len);

static void race_log_write(const char *fmt, ...) {
    static char line[131072];   /* worker thread only */
    int n;
    if (!g_race_log) return;
    va_list ap; va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof line) return;   /* never journal a truncated JSON line */
    ar_journal_line(line, n);
}

static void race_log_open(void) {
    g_race_log = 1;

    char cars[65536]; int p = 0, first = 1;
    p += _snprintf(cars + p, sizeof(cars) - p, "[");
    int cnt = g_telemetry.vehicle_count; if (cnt > MAX_CARS) cnt = MAX_CARS;
    for (int i = 0; i < cnt; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position <= 0) continue;
        char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
        if (!first) p += _snprintf(cars + p, sizeof(cars) - p, ",");
        first = 0;
        p += _snprintf(cars + p, sizeof(cars) - p, "{\"id\":%d,\"n\":\"%s\"}", i, esc);
    }
    p += _snprintf(cars + p, sizeof(cars) - p, "]");

    race_log_write("{\"t\":\"session_start\",\"tk\":%lu,\"ty\":%d,\"tl\":%d,\"vc\":%d,\"cars\":%s}",
        GetTickCount(), g_telemetry.session_type, g_telemetry.total_laps, cnt, cars);
}

static void race_log_close(void) {
    if (!g_race_log) return;
    char cls[8192]; int p = 0, first = 1;
    p += _snprintf(cls + p, sizeof(cls) - p, "[");
    int cnt = g_telemetry.vehicle_count; if (cnt > MAX_CARS) cnt = MAX_CARS;
    for (int pos = 1; pos <= cnt; pos++) {
        for (int i = 0; i < cnt; i++) {
            car_telemetry_t *c = &g_telemetry.cars[i];
            if (c->position != pos) continue;
            char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
            if (!first) p += _snprintf(cls + p, sizeof(cls) - p, ",");
            first = 0;
            p += _snprintf(cls + p, sizeof(cls) - p,
                "{\"id\":%d,\"n\":\"%s\",\"pos\":%d,\"fs\":%d,\"laps\":%d,\"best\":%.3f}",
                i, esc, c->position, c->finish_state, c->lap_number, c->best_lap);
            break;
        }
    }
    p += _snprintf(cls + p, sizeof(cls) - p, "]");
    race_log_write("{\"t\":\"session_end\",\"tk\":%lu,\"classification\":%s}", GetTickCount(), cls);
    g_race_log = 0;
}

static void race_log_lap(int i, car_telemetry_t *c) {
    if (!g_race_log) return;
    char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
    /* Exact per-lap values, full precision, so a finished race can be analysed
     * offline: tire wear per corner (moves ~1e-4 per lap, so 6 decimals), fuel in
     * the tank as well as the per-lap rate, engine temps against the engine.ini
     * LifetimeOilTemp threshold, and the raw damage flags rather than a UI tier. */
    race_log_write(
        "{\"t\":\"lap\",\"tk\":%lu,\"id\":%d,\"n\":\"%s\",\"lap\":%d,\"pos\":%d,"
        "\"time\":%.3f,\"s1\":%.3f,\"s2\":%.3f,\"tire\":%d,\"fuelrate\":%.4f,"
        "\"fuel\":%.3f,\"pit\":%d,\"cc\":%d,"
        "\"wear\":{\"fl\":%.6f,\"fr\":%.6f,\"rl\":%.6f,\"rr\":%.6f},"
        "\"eng\":{\"rpm\":%.1f,\"rpm_rad_s\":%.1f,\"angvel\":%.1f,\"oil\":%.3f,\"water\":%.3f,\"dead\":%d},"
        "\"life\":{\"eng\":%.1f,\"brk\":%.1f,\"gbx\":%.1f,\"sus\":%.1f},\"wg\":%d,"
        "\"dmg\":{\"tier\":%d,\"fw\":%d,\"rw\":%d,\"fwflag\":%d,\"rwflag\":%d,\"hits\":%d},"
        "\"pen\":%d,\"limits\":%d,\"rc\":%d}",
        GetTickCount(), i, esc, c->lap_number, c->position,
        c->last_lap, c->last_s1, c->last_s2, c->tire_choice, c->fuel_consumption,
        c->fuel_level, (c->pitting >= 2) ? 1 : 0, c->car_conditions,
        c->tire_wear_fl, c->tire_wear_fr, c->tire_wear_rl, c->tire_wear_rr,
        c->rpm * LF_RPM_FACTOR, c->rpm, c->eng_angvel, c->oil_temp, c->water_temp, c->eng_dead,
        c->life_eng, c->life_brk, c->life_gbx, c->life_sus, c->wear_gate,
        c->dmg_tier, c->fw_lost, c->rw_lost, c->fw_flag, c->rw_flag, c->hits_in_race,
        c->penalties, c->track_limits, c->reason_code);
}

static void race_log_incident(const char *type, int car_id, int lap, int a, int b) {
    if (!g_race_log) return;
    char esc[NAME_LEN * 6 + 1];
    esc_json(esc, sizeof(esc), (car_id >= 0 && car_id < MAX_CARS) ? g_telemetry.cars[car_id].name : "SESSION");
    race_log_write("{\"t\":\"incident\",\"tk\":%lu,\"ty\":\"%s\",\"id\":%d,\"n\":\"%s\",\"lap\":%d,\"a\":%d,\"b\":%d}",
        GetTickCount(), type, car_id, esc, lap, a, b);
}

/* ---- unattended wear trace -------------------------------------------------
 * One line every WEAR_SAMPLE_MS with EVERY car's component countdowns and tyre
 * wear, so a whole race can be left running and analysed afterwards. This is the
 * time series the live dashboard cannot give you, because nobody is watching it.
 *
 * `wg` is included per car on purpose: the countdown only ticks for cars with
 * veh+0x32C8 == 1 (AI), so a human car's `life`/`oil` values are frozen and must
 * not be read as wear. Recording the gate keeps that visible in the data itself
 * rather than as a footnote.
 */
#define WEAR_SAMPLE_MS 15000
static DWORD g_last_wear_sample = 0;

static void race_log_wear_sample(void) {
    if (!g_race_log) return;
    DWORD now = GetTickCount();
    if (g_last_wear_sample && now - g_last_wear_sample < WEAR_SAMPLE_MS) return;
    g_last_wear_sample = now;

    int cnt = g_telemetry.vehicle_count;
    if (cnt > MAX_CARS) cnt = MAX_CARS;

    for (int i = 0; i < cnt; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position <= 0 && !c->name[0]) continue;
        char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
        char ecn[NAME_LEN * 6 + 1]; esc_json(ecn, sizeof(ecn), c->car_name);
        /* the pre-scripted failure table, as [[lap,component],...] */
        char sched[512]; int sp = 0;
        {
            volatile uint8_t *veh = VEHICLE_PTR(i);
            sp += _snprintf(sched + sp, sizeof(sched) - sp, "[");
            if (veh && (uintptr_t)veh >= 0x10000) {
                for (int q = 0; q < SCHED_ENTRIES; q++) {
                    int lap  = *(volatile int *)(veh + OFF_SCHED_TABLE + q * 8);
                    int comp = *(volatile int *)(veh + OFF_SCHED_TABLE + q * 8 + 4);
                    sp += _snprintf(sched + sp, sizeof(sched) - sp,
                                    "%s[%d,%d]", q ? "," : "", lap, comp);
                }
            }
            sp += _snprintf(sched + sp, sizeof(sched) - sp, "]");
        }
        race_log_write(
            "{\"t\":\"wear\",\"tk\":%lu,\"st\":%.1f,\"id\":%d,\"n\":\"%s\",\"team\":\"%s\",\"lap\":%d,\"pos\":%d,\"sched\":%s,"
            "\"wg\":%d,\"life\":{\"eng\":%.1f,\"brk\":%.1f,\"gbx\":%.1f,\"sus\":%.1f},"
            "\"tyre\":{\"fl\":%.6f,\"fr\":%.6f,\"rl\":%.6f,\"rr\":%.6f},\"comp\":%d,"
            "\"eng\":{\"rpm\":%.0f,\"rpm_rad_s\":%.0f,\"oil\":%.3f,\"water\":%.3f,\"dead\":%d},"
            "\"fuel\":%.2f,\"rate\":%.4f,\"spd\":%.1f,\"pit\":%d,\"stops\":%d,"
            "\"fs\":%d,\"ret\":%d,\"rc\":%d,\"dt\":%d}",
            now, g_telemetry.session_time, i, esc, ecn, c->lap_number, c->position, sched,
            c->wear_gate,
            c->life_eng, c->life_brk, c->life_gbx, c->life_sus,
            c->tire_wear_fl, c->tire_wear_fr, c->tire_wear_rl, c->tire_wear_rr,
            c->tire_choice,
            c->rpm * LF_RPM_FACTOR, c->rpm, c->oil_temp, c->water_temp, c->eng_dead,
            c->fuel_level, c->fuel_consumption, c->speed, c->pitting,
            (c->pit_time_0 > 0 ? 1 : 0) + (c->pit_time_1 > 0 ? 1 : 0) + (c->pit_time_2 > 0 ? 1 : 0),
            c->finish_state, c->retired, c->reason_code, c->dmg_tier);
    }
}

static void race_log_heartbeat(void) {
    if (!g_race_log) return;
    DWORD now = GetTickCount();
    if (now - g_last_heartbeat < 10000) return;
    g_last_heartbeat = now;
    race_log_write(
        "{\"t\":\"heartbeat\",\"tk\":%lu,\"ph\":%d,\"ac\":%d,\"acr\":%d,\"tcf\":%.3f,"
        "\"grip\":%.3f,\"onw\":%.3f,\"offw\":%.3f,\"at\":%.1f,\"trkt\":%.1f}",
        now, g_telemetry.race_phase, g_telemetry.race_active,
        g_telemetry.race_active_raw, g_telemetry.track_condition,
        g_telemetry.track_grip_factor, g_telemetry.on_wetness, g_telemetry.off_wetness,
        g_telemetry.ambient_temp, g_telemetry.track_temp);
}

/* ========================================================================
 * PER-CAR LAP HISTORY (race pace grid)
 * C06 (UI_CORPUS_REVIEW_2026-10-08): the old 60-lap ring silently dropped
 * later laps in long races. It is now a bounded 120-lap ring with an explicit
 * per-car dropped counter, published as lhlim/hlost; the race JSONL journal
 * remains the durable full-session record.
 * ======================================================================== */
#define MAX_LAP_HISTORY 120

/* One row per completed lap, per car. The game's last_s1 and last_s2 are both
 * CUMULATIVE elapsed times from the line (last_s2 = S1 + S2). JSON keys (rc.2):
 * s2 keeps the cumulative value; s2d (new) = S2 split; s3 = S3 split = lap - s2
 * (rc.1 computed lap - s1 - s2, which subtracts S1 twice). Finding
 * web-telemetry-sectors-cumulative-s2-20261012. r->lap is the 1-based lap just
 * completed (game lap_number after the tick; rc.1 subtracted 1).
 * The tyre/fuel/wear
 * columns are sampled at the moment the lap ticks over, which is what makes
 * a stint readable after the fact: compound at the time, fuel remaining,
 * and how much tread was left. */
typedef struct {
    int   lap;                 /* lap number just completed */
    float t, s1, s2;           /* lap time, sector 1, sector 2 */
    int   pos;                 /* position at that moment */
    int   tire;                /* compound INDEX (see tbc note in tireLabel) */
    float fuel;
    float life[4];             /* normalized tyre life 0..1, FL FR RL RR */
    int   pitted;              /* this lap included a pit stop */
} lap_row_t;

static lap_row_t g_lap_rows[MAX_CARS][MAX_LAP_HISTORY];
static float g_lap_history[MAX_CARS][MAX_LAP_HISTORY];
static int   g_lap_history_count[MAX_CARS];
static int   g_lap_history_dropped[MAX_CARS];   /* laps past the ring (C06) */
static int   g_last_lap_seen[MAX_CARS];
/* 1.0.1: the driver a slot's lap history belongs to. A roster change no longer
 * wipes every car's history (it wiped practice/qualifying laps whenever a car
 * went to its garage); a slot whose driver changed restarts only its own. */
static char  g_lap_owner[MAX_CARS][NAME_LEN];
static void ar_on_lap_row(int slot, const car_telemetry_t *c, const lap_row_t *r);
static float  g_ll_prev[MAX_CARS];        /* last_lap value at the previous tick */
static double g_ll_changed_at[MAX_CARS];  /* sim time the value last changed */
static int    g_lap_pending[MAX_CARS];    /* lap number waiting for its time; 0 = none */
static double g_lap_pending_at[MAX_CARS];

static void lap_history_init(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        g_last_lap_seen[i] = -1;
        g_lap_history_dropped[i] = 0;
        g_lap_history_count[i] = 0;
        g_lap_owner[i][0] = 0;
        g_lap_pending[i] = 0;
        g_ll_prev[i] = 0.0f;
        g_ll_changed_at[i] = -1e9;
    }
}

/* 1.0.1 lap-time latch. The game raises the lap counter a few frames BEFORE it
 * writes the new last-lap time (and sector times), so 1.0.0 often stored the
 * PREVIOUS value: race lap 1 showed the qualifying time and pairs of laps
 * repeated (owner screenshot 2026-10-12, race pace L1 = L2, L3 = L4). Now a
 * counter increment opens a pending lap that is recorded when the last-lap
 * value actually changes (or changed in the last LAP_FRESH_S of sim time).
 * If it never changes within LAP_WAIT_S the lap is recorded with an unknown
 * time (-1, shown as "-") instead of a wrong one. */
#define LAP_FRESH_S 1.5
#define LAP_WAIT_S  8.0

static void lap_history_store(int i, car_telemetry_t *c, int lap_number, int time_known) {
    float last_lap = time_known ? c->last_lap : -1.0f;
    int idx = g_lap_history_count[i];
    {
        lap_row_t spill;                       /* ring full: still archived */
        {
            lap_row_t *r = idx < MAX_LAP_HISTORY ? &g_lap_rows[i][idx] : &spill;
            if (idx < MAX_LAP_HISTORY) g_lap_history[i][idx] = last_lap;
            r->lap    = lap_number;            /* the lap just finished, 1-based (rc.2) */
            r->t      = last_lap;
            r->s1     = time_known ? c->last_s1 : -1.0f;
            r->s2     = time_known ? c->last_s2 : -1.0f;
            r->pos    = c->position;
            r->tire   = c->tire_choice;
            r->fuel   = c->fuel_level;
            r->pitted = (c->pitting >= 2.0f) ? 1 : 0;
            /* Player-only: the thermal block carries real tread. For AI the
             * per-car mirror is all there is, normalized the same way. */
            if (i == g_telemetry.player_slot) {
                for (int w = 0; w < WHEEL_COUNT; w++) r->life[w] = g_telemetry.tread[w];
            } else {
                float mir[4] = { c->tire_wear_fl, c->tire_wear_fr,
                                 c->tire_wear_rl, c->tire_wear_rr };
                for (int w = 0; w < 4; w++) {
                    double n = (mir[w] - TREAD_DEAD) * 4.0;
                    if (!(n > 0.0)) n = 0.0;
                    if (n > 1.0) n = 1.0;
                    r->life[w] = (float)n;
                }
            }
            if (idx < MAX_LAP_HISTORY) g_lap_history_count[i] = idx + 1;
            else g_lap_history_dropped[i]++;
            ar_on_lap_row(i, c, r);
        }
    }
    if (time_known) race_log_lap(i, c);
    else {
        car_telemetry_t unknown = *c;
        unknown.last_lap = unknown.last_s1 = unknown.last_s2 = -1.0f;
        race_log_lap(i, &unknown);
    }
}

static void lap_history_record(int i, car_telemetry_t *c) {
    int lap_number = c->lap_number;
    double now = g_telemetry.session_time;
    if (c->name[0] && strcmp(g_lap_owner[i], c->name) != 0) {
        if (g_lap_owner[i][0]) {               /* another driver now holds this slot */
            g_lap_history_count[i] = 0;
            g_lap_history_dropped[i] = 0;
            g_last_lap_seen[i] = -1;
            g_lap_pending[i] = 0;
        }
        snprintf(g_lap_owner[i], NAME_LEN, "%s", c->name);
    }
    if (lap_number < g_last_lap_seen[i]) {
        /* new race/session for this slot: session restarted */
        g_lap_history_count[i] = 0;
        g_lap_history_dropped[i] = 0;
        g_lap_pending[i] = 0;
    }
    if (c->last_lap != g_ll_prev[i]) { g_ll_changed_at[i] = now; g_ll_prev[i] = c->last_lap; }
    if (lap_number > 0 && lap_number != g_last_lap_seen[i] && g_last_lap_seen[i] >= 0) {
        if (g_lap_pending[i]) lap_history_store(i, c, g_lap_pending[i], 0);   /* a lap passed with no time */
        g_lap_pending[i] = lap_number;
        g_lap_pending_at[i] = now;
    } else if (lap_number > 0 && g_last_lap_seen[i] < 0) {
        /* first sight of this car (attach mid-session): its current time is old */
        g_ll_changed_at[i] = -1e9;
    }
    if (g_lap_pending[i]) {
        int fresh = c->last_lap > 0 && g_ll_changed_at[i] >= g_lap_pending_at[i] - LAP_FRESH_S &&
                    g_ll_changed_at[i] <= now;
        if (fresh) {
            lap_history_store(i, c, g_lap_pending[i], 1);
            g_lap_pending[i] = 0;
        } else if (now - g_lap_pending_at[i] > LAP_WAIT_S || now < g_lap_pending_at[i]) {
            lap_history_store(i, c, g_lap_pending[i], 0);
            g_lap_pending[i] = 0;
        }
    }
    g_last_lap_seen[i] = lap_number;
}

/* ========================================================================
 * ENGINEERING TRACE (4.2.0): distance-indexed recording for the player/AI
 * and saved-run comparison. Channel evidence, units and validity rules are
 * specified in CHANNEL_LEDGER_2026-10-09.md; this code implements exactly
 * that ledger. Samples are native observed values; interpolation happens
 * only in the comparison builder (build_trace_compare_json) and is bounded
 * to valid continuous segments.
 * ======================================================================== */
/* Read helpers over owned mirror bytes. Defined here because the trace
 * recorder (worker thread, under g_telemetry_cs) uses them too. */
#define RF(veh, off)     finite_float(*(const float*)((const uint8_t*)(veh)+(off)))
/* Validate raw mirror values before sanitization can turn NaN into zero. */
#define RRAW(veh, off)   (*(const float*)((const uint8_t*)(veh)+(off)))
#define RI(veh, off)     (*(volatile int*)((volatile uint8_t*)(veh)+(off)))
#define RB(veh, off)     (*(volatile uint8_t*)((volatile uint8_t*)(veh)+(off)))
#define TRACE_CARS           2          /* selected-mode entries (fallback + A/B aliases) */
#define TRACE_LAPS           6
#define TRACE_LAP_SLOTS      (TRACE_LAPS+1) /* six completed plus one open */
/* 4.4.0 (owner rule 2026-10-12): every live car is traced all the time; the
 * ring is sized for a 2:30 lap at the measured capture rate. The rate is the
 * worker loop's Sleep(50) -> ~20 Hz (live rc.11 /health: rate 19.6 Hz;
 * LIVE_ACCEPTANCE_REVIEW_2026-10-09: 18.65 Hz; this is also what the old
 * comment claimed). 3200 samples = 2:40 at 20 Hz, headroom over the 2:30
 * target; the old 2400 (2 min) dropped samples and refused slow laps as
 * incomplete. NOT sized for a hypothetical 60 Hz: 9000 would be 10.1 MB per
 * car (~242 MB for 24 cars), unsafe for the 32-bit game and unsupported by
 * every rate measurement. */
#define TRACE_SAMPLES        3200
#define TRACE_SIM_GAP_S      1.0f      /* larger sim-time gap = segment break */
#define TRACE_REVERSE_M      2.0f      /* backwards step = segment break */
#define TRACE_TELEPORT_M     50.0f     /* forwards jump = segment break */
#define TRACE_MIN_LAP_SAMPLES 30       /* shorter "lap" = attach/jitter, not a lap */
#define TRACE_EVICT_MAX      8         /* recorded ring evictions kept per side */

/* Per-sample channel validity bits. A sample is only stored while the car is
 * on track, off the pit sequence, with a sane lap distance. */
#define TRF_DIST     (1u<<0)
#define TRF_SPEED    (1u<<1)
#define TRF_THROTTLE (1u<<2)
#define TRF_BRAKE    (1u<<3)
#define TRF_STEER    (1u<<4)
#define TRF_GLAT     (1u<<5)
#define TRF_GLON     (1u<<6)
#define TRF_GVERT    (1u<<7)
#define TRF_YAW      (1u<<8)
#define TRF_YAWRATE  (1u<<9)
#define TRF_TREAD    (1u<<10)
#define TRF_SEGMENT  (1u<<11) /* first sample after recording interruption */
/* rc.5 additions (workbench): converted RPM, gear, track coordinates. */
#define TRF_RPM      (1u<<12) /* rpm stored as display RPM, never raw rad/s */
#define TRF_GEAR     (1u<<13)
#define TRF_TRACK    (1u<<14) /* tx/tz from coordinatesInTrack (map frame) */
/* 4.3.0 additions (viewer). TTEMP is FACT for the player (thermal block mean
 * of three surface zones, C) and RESEARCH for AI (wheel+0x50 candidate);
 * TRF_TTEMP_RX marks the AI case. RX_* are RESEARCH raw candidates, AI only
 * except RX_LD (player W+796). Wheel order FL,FR,RL,RR. */
#define TRF_TTEMP    (1u<<15)
#define TRF_RX_SU    (1u<<16)
#define TRF_RX_LD    (1u<<17)
#define TRF_RX_SL    (1u<<18)
#define TRF_RX_FO    (1u<<19)
#define TRF_TTEMP_RX (1u<<20)
#define TRF_ALL      (TRF_DIST|TRF_SPEED|TRF_THROTTLE|TRF_BRAKE|TRF_STEER| \
                      TRF_GLAT|TRF_GLON|TRF_GVERT|TRF_YAW|TRF_YAWRATE|TRF_TREAD| \
                      TRF_RPM|TRF_GEAR|TRF_TRACK|TRF_TTEMP|TRF_RX_SU|TRF_RX_LD|                       TRF_RX_SL|TRF_RX_FO|TRF_TTEMP_RX)

/* Completed-lap status bits (published in /trace/status). */
#define TRL_COMPLETE (1<<0)
#define TRL_PITTED   (1<<1)
#define TRL_BREAKS   (1<<2)   /* an interpolation break occurred inside the lap */

typedef struct {
    float sim_t;        /* session time, s */
    float dist;         /* lap distance, m */
    float speed;        /* m/s */
    float throttle;     /* 0..1 */
    float brake;        /* 0..1 */
    float steer;        /* AI input factor (valid bit only when AI) */
    float glat, glon, gvert;   /* m/s2, native signs */
    float yaw;          /* rad */
    float yaw_rate;     /* rad/s */
    float tread[4];     /* normalized remaining tread 0..1 (FL,FR,RL,RR) */
    float rpm;          /* rc.5: display RPM (raw rad/s x 9.549296585513721) */
    int   gear;         /* rc.5: gear (0 = neutral) */
    float tx, tz;       /* rc.5: coordinatesInTrack (the map's own frame) */
    uint32_t flags;     /* TRF_* */
    float ttemp[4];     /* 4.3.0: tyre temperature C (FL,FR,RL,RR) */
    float rx[16];       /* 4.3.0 RESEARCH: susp x4, load x4, slip x4, force x4 */
} trace_sample_t;       /* 160 bytes */

/* Fix 2 (api_version 2): one explicit ring-eviction record. A completed lap
 * evicted by the bounded ring is reported in /trace/status.evicted[] instead
 * of vanishing silently; a client-pinned reference can always tell evicted
 * from never-recorded. */
typedef struct {
    int   lap;          /* lap number of the evicted completed lap */
    float t;            /* its lap time, s */
    float at;           /* session time when the ring evicted it, s */
} trace_evict_t;

typedef struct {
    int  used;                  /* trace slot assigned */
    int  slot;                  /* vehicle slot */
    int  user_selected;         /* 1 = manual selection, do not auto-follow */
    unsigned session_generation;
    int  driver_id;
    char driver_name[NAME_LEN];
    char car_name[NAME_LEN];
    int  is_ai;                 /* recorded AI control; NOT a physics-mode claim */
    int  is_remote;             /* 4.4.0-rc.1: controller mode 2 when recording started */
    unsigned revision;          /* changes whenever recording identity is reset */
    int start_seen;             /* witnessed a continuous start-line crossing */
    uint32_t team_id;           /* last broker team hash for this car */
    /* open lap */
    int  lap_open;
    int  lap_number;
    int  current_index;         /* ring index being filled */
    int  sample_count;
    unsigned dropped_samples;   /* samples past TRACE_SAMPLES in the open lap */
    int  open_flags;            /* TRL_* accumulated for the open lap */
    /* completed lap ring */
    int   lap_valid[TRACE_LAP_SLOTS];
    int   lap_num[TRACE_LAP_SLOTS];
    float lap_time[TRACE_LAP_SLOTS];
    float lap_start_sim[TRACE_LAP_SLOTS];
    int   lap_samples[TRACE_LAP_SLOTS];
    int   lap_head;             /* next ring slot to write */
    unsigned lap_total;         /* completed recorded laps this session */
    unsigned dropped_laps;      /* laps discarded or overwritten */
    unsigned total_samples;
    uint32_t channels_seen;     /* TRF_* ever valid this session */
    /* Fix 2: explicit eviction log + completed-lap range of this revision.
     * Both are cleared by every identity/session/generation reset, so a stale
     * client holding an earlier lid sees the revision mismatch instead of a
     * silent alias; within one revision the range separates "evicted" from
     * "never recorded" for /trace/data misses. */
    trace_evict_t evicted[TRACE_EVICT_MAX];
    int   evicted_head, evicted_count;
    int   first_lap_seen, last_lap_seen;
    float rec_lap_len;          /* lap length when this recording identity began;
                                 * a track change (lap_len moves) resets the
                                 * side so stale distances are never served */
    /* continuity state */
    int  have_prev;
    float prev_sim, prev_dist, prev_yaw;
    int  prev_lap;
    trace_sample_t laps[TRACE_LAP_SLOTS][TRACE_SAMPLES];
} trace_car_t;

static trace_car_t g_trace[TRACE_CARS];
static unsigned g_trace_revision;
static void ar_on_trace_lap(const trace_car_t *t, int idx);
static uint32_t trace_team_id(uint32_t team) {
    return team == 2166136261u ? 0 : team; /* empty FNV name is unknown */
}
static int g_trace_slot[TRACE_CARS] = { -1, -1 };   /* selected vehicle slots */
static int g_trace_auto[TRACE_CARS] = { 1, 1 };     /* 1 = follow default */

/* ---- all-cars store (4.4.0, owner rule 2026-10-12) ----------------------
 * One trace_car_t per vehicle slot, VirtualAlloc'd once per live car count
 * (capped at MAX_CARS) from the worker's session/roster-change path. When the
 * allocation fails the recorder keeps the old selected-only mode; /health
 * reports trace_mode = "all" | "selected" | "selected_fallback". */
#define TRACE_MODE_SELECTED 0
#define TRACE_MODE_ALL      1
static int g_trace_mode = TRACE_MODE_SELECTED;
static trace_car_t *g_trace_store = NULL;
static int g_trace_store_cap = 0;
static unsigned long long g_trace_store_bytes = 0;
static int g_trace_store_failed = 0;      /* an allocation attempt failed */
static int g_trace_alloc_test_fail = 0;   /* runtime_test hook only; never set in the product */
static void log_f(const char *fmt, ...);  /* defined below; store logs once per allocation */

/* Idempotent: grows the store when the live car count grows (older entries are
 * carried over), never shrinks. Returns 1 when all-cars mode is active. */
static int trace_store_ensure(int count) {
    if (count <= 0) return g_trace_mode == TRACE_MODE_ALL && g_trace_store != NULL;
    if (count > MAX_CARS) count = MAX_CARS;
    if (g_trace_store && count <= g_trace_store_cap) return 1;
    size_t bytes = (size_t)count * sizeof(trace_car_t);
    trace_car_t *p = NULL;
    if (!g_trace_alloc_test_fail)
        p = (trace_car_t *)VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) {
        if (!g_trace_store_failed) {
            g_trace_store_failed = 1;
            log_f("trace store allocation failed (%lu bytes for %d cars); %s",
                  (unsigned long)bytes, count,
                  g_trace_store ? "keeping the existing store (extra cars not traced)"
                                : "selected-only traces (fallback)");
        }
        /* 1.0.1: a failed GROWTH keeps the store that already holds every
         * recorded lap (1.0.0 dropped to selected mode and made all of them
         * unreachable while keeping the memory). Cars past the old capacity
         * are simply not traced. */
        if (g_trace_store) { g_trace_mode = TRACE_MODE_ALL; return 1; }
        g_trace_mode = TRACE_MODE_SELECTED;
        return 0;
    }
    if (g_trace_store) {
        int keep = g_trace_store_cap < count ? g_trace_store_cap : count;
        memcpy(p, g_trace_store, (size_t)keep * sizeof(trace_car_t));
        VirtualFree(g_trace_store, 0, MEM_RELEASE);
    }
    g_trace_store = p;
    g_trace_store_cap = count;
    g_trace_store_bytes = (unsigned long long)bytes;
    g_trace_mode = TRACE_MODE_ALL;
    log_f("trace store: all cars traced (%d x %u bytes = %llu bytes)", count,
          (unsigned)sizeof(trace_car_t), g_trace_store_bytes);
    return 1;
}

/* Resolve a request selector to a trace entry. which: 0/1 = side A/B;
 * 1000+N = explicit vehicle slot N. Returns NULL when unknown. letter_out:
 * 0/1 when the entry is one of the selected sides, -1 otherwise. Callers hold
 * g_telemetry_cs. */
static trace_car_t *trace_pick(int which, int *letter_out) {
    if (letter_out) *letter_out = -1;
    if (which >= 1000) {
        int slot = which - 1000;
        if (slot < 0 || slot >= MAX_CARS) return NULL;
        if (g_trace_mode == TRACE_MODE_ALL && g_trace_store) {
            if (slot >= g_trace_store_cap) return NULL;
            if (letter_out) *letter_out = (slot == g_trace_slot[0]) ? 0
                                      : (slot == g_trace_slot[1]) ? 1 : -1;
            return &g_trace_store[slot];
        }
        for (int i = 0; i < TRACE_CARS; i++)
            if (g_trace[i].slot == slot) { if (letter_out) *letter_out = i; return &g_trace[i]; }
        return NULL;
    }
    if (which < 0 || which >= TRACE_CARS) return NULL;
    if (g_trace_mode == TRACE_MODE_ALL && g_trace_store) {
        int slot = g_trace_slot[which];
        if (slot < 0 || slot >= g_trace_store_cap) return NULL;
        if (letter_out) *letter_out = which;
        return &g_trace_store[slot];
    }
    if (letter_out) *letter_out = which;
    return &g_trace[which];
}

static void trace_session_clear(void) {
    for (int k = 0; k < TRACE_CARS; k++) {
        int slot = g_trace_slot[k];
        memset(&g_trace[k], 0, sizeof(g_trace[k]));
        g_trace[k].slot = slot;
        g_trace[k].lap_number = -1;
        g_trace[k].prev_lap = -1;
        g_trace[k].current_index = 0;
        g_trace[k].first_lap_seen = -1;
        g_trace[k].last_lap_seen = -1;
    }
    /* Owner rule 2026-10-12: a session/roster change clears the all-cars store
     * too; the allocation itself survives and is reused by the next session. */
    for (int i = 0; g_trace_store && i < g_trace_store_cap; i++) {
        memset(&g_trace_store[i], 0, sizeof(g_trace_store[i]));
        g_trace_store[i].slot = i;
        g_trace_store[i].lap_number = -1;
        g_trace_store[i].prev_lap = -1;
        g_trace_store[i].current_index = 0;
        g_trace_store[i].first_lap_seen = -1;
        g_trace_store[i].last_lap_seen = -1;
    }
}

/* A roster pointer can change when a car goes to or leaves its garage. That
 * does not start a new session and must not discard every driver's laps.
 * trace_entry_identity handles a genuinely new occupant of one slot. */
static void trace_capture_epoch(int generation_changed, int live_count) {
    if (generation_changed) trace_session_clear();
    trace_store_ensure(live_count);
}

/* Explicit (HTTP /trace/select) or default selection. slot < 0 clears. */
static void trace_select(int which, int slot, int user) {
    if (which < 0 || which >= TRACE_CARS) return;
    if (slot >= MAX_CARS) return;
    if (slot < 0) {
        memset(&g_trace[which], 0, sizeof(g_trace[which]));
        g_trace[which].slot = -1;
        g_trace[which].lap_number = -1;
        g_trace[which].prev_lap = -1;
        g_trace[which].first_lap_seen = -1;
        g_trace[which].last_lap_seen = -1;
        g_trace_slot[which] = -1;
        g_trace_auto[which] = user ? 0 : 1;
        return;
    }
    /* Batch 2c: the same slot never clears the ring (laps stay), used or not yet sampled. */
    if (g_trace[which].slot == slot) {
        g_trace[which].user_selected = user;
        g_trace_auto[which] = user ? 0 : 1;
        return;
    }
    memset(&g_trace[which], 0, sizeof(g_trace[which]));
    g_trace[which].slot = slot;
    g_trace[which].lap_number = -1;
    g_trace[which].prev_lap = -1;
    g_trace[which].first_lap_seen = -1;
    g_trace[which].last_lap_seen = -1;
    g_trace[which].user_selected = user;
    g_trace_slot[which] = slot;
    g_trace_auto[which] = user ? 0 : 1;
}

/* Batch 2c (task 1): a car the trace may follow by default is running and
 * classified: on track, not retired, finish_state 0, classified position. */
static int trace_live_car(const car_telemetry_t *c) {
    return c->position > 0 && c->on_track && !c->retired && c->finish_state == 0;
}

/* Default selection: A = player when running, else the race leader (lowest
 * live position); B = teammate by broker team hash, else the first AI car.
 * A stopped or retired player is never auto-selected. Called after
 * player_slot_update / identity refresh. */
static void trace_default_selection(int count) {
    if (g_trace_auto[0]) {
        int ps = g_telemetry.player_slot;
        int pick = -1, best = 1 << 30;
        if (ps >= 0 && ps < count && trace_live_car(&g_telemetry.cars[ps])) pick = ps;
        if (pick < 0) {
            for (int i = 0; i < count; i++) {
                car_telemetry_t *c = &g_telemetry.cars[i];
                if (!trace_live_car(c)) continue;
                if (c->position < best) { best = c->position; pick = i; }
            }
        }
        if (pick >= 0) {
            if (g_trace[0].slot != pick || !g_trace[0].used) trace_select(0, pick, 0);
        } else if (g_trace[0].used && !g_trace[0].user_selected) {
            trace_select(0, -1, 0);
        }
    }
    if (g_trace_auto[1]) {
        int ps = g_telemetry.player_slot;
        uint32_t team = (ps >= 0 && ps < count) ? trace_team_id(g_telemetry.cars[ps].team_id) : 0;
        int pick = -1;
        for (int i = 0; i < count; i++) {
            car_telemetry_t *c = &g_telemetry.cars[i];
            if (i == ps || !trace_live_car(c)) continue;
            if ((team && trace_team_id(c->team_id) == team) ||
                (!team && ps >= 0 && ps < count && c->car_name[0] &&
                 !strcmp(c->car_name,g_telemetry.cars[ps].car_name))) { pick = i; break; }
        }
        if (pick < 0) {
            for (int i = 0; i < count; i++) {
                car_telemetry_t *c = &g_telemetry.cars[i];
                if (i == ps || !trace_live_car(c)) continue;
                if (c->ai_control) { pick = i; break; }
            }
        }
        if (pick >= 0) {
            if (g_trace[1].slot != pick || !g_trace[1].used) trace_select(1, pick, 0);
        } else if (g_trace[1].used && !g_trace[1].user_selected) {
            trace_select(1, -1, 0);
        }
    }
}

static float trace_channel_ok(float v, float limit) {
    return isfinite(v) && fabsf(v) <= limit;
}

/* Fix 2: record the completed lap the ring is about to overwrite so the
 * eviction is explicit in /trace/status (never a silent switch). */
static void trace_evict_push(trace_car_t *t, int slot_index, float now) {
    trace_evict_t *e = &t->evicted[t->evicted_head];
    e->lap = t->lap_num[slot_index];
    e->t = t->lap_time[slot_index];
    e->at = now;
    t->evicted_head = (t->evicted_head + 1) % TRACE_EVICT_MAX;
    if (t->evicted_count < TRACE_EVICT_MAX) t->evicted_count++;
}

/* 1.2.3 (DVOA report 2026-10-17): the ring kept only the newest six laps, so a fastest lap from an early run was
 * overwritten and the lap strip showed the best of the last six. The slot holding this car's fastest clean lap is never
 * the next one written: the ring keeps the fastest lap plus the newest laps. */
static int trace_best_slot(const trace_car_t *t) {
    int best = -1;
    for (int k = 0; k < TRACE_LAP_SLOTS; k++)
        if ((t->lap_valid[k] & TRL_COMPLETE) && !(t->lap_valid[k] & (TRL_PITTED|TRL_BREAKS)) && t->lap_time[k] > 0.0f &&
            (best < 0 || t->lap_time[k] < t->lap_time[best] ||
             (t->lap_time[k] == t->lap_time[best] && t->lap_num[k] > t->lap_num[best]))) best = k;   /* tie: newest */
    return best;
}
static int trace_next_head(const trace_car_t *t, int from) {
    int next = (from + 1) % TRACE_LAP_SLOTS;
    if (next == trace_best_slot(t)) next = (next + 1) % TRACE_LAP_SLOTS;
    return next;
}

/* 4.4.0-rc.5 (Batch 2h task A): the game lap counter is 0 for the whole first
 * racing lap. A lap-0 car is recorded only while it moves (speed above 1) or its
 * lap distance advances after a recorded sample; a grid standstill is refused. */
static int trace_lap0_moving(const car_telemetry_t *c, const trace_car_t *t) {
    return c->speed > 1.0f || (t->have_prev && c->lap_dist > t->prev_dist + 0.01f);
}

/* Build one sample for one trace entry; returns 1 when stored. Runs under
 * g_telemetry_cs (called from read_telemetry) and only reads owned mirrors. */
static void trace_sample_car_at(trace_car_t *t, int count, float session_time) {
    if (!t->used || t->slot < 0 || t->slot >= count) return;
    car_telemetry_t *c = &g_telemetry.cars[t->slot];
    volatile uint8_t *veh = VEHICLE_PTR(t->slot);

    /* Recording gate: a real on-track lap sample only. */
    int recording = veh && (uintptr_t)veh >= 0x10000 && c->position > 0 &&
                    c->on_track && c->pitting < 2 &&
                    (c->lap_number > 0 || trace_lap0_moving(c, t)) &&
                    g_telemetry.lap_len > 0.0f &&
                    isfinite(RRAW(veh, OFF_TRACK_PART_EXACT)) &&
                    isfinite(c->lap_dist) && c->lap_dist >= 0.0f && c->lap_dist <= g_telemetry.lap_len;
    if (!recording) {
        if (t->lap_open) t->open_flags |= TRL_BREAKS;
        if (t->lap_open && c->pitting >= 2) t->open_flags |= TRL_PITTED;
        t->have_prev = 0;
        return;
    }

    /* Repeated captures during pause are not new simulation samples. */
    if (t->lap_open && t->sample_count > 0 &&
        session_time <= t->laps[t->current_index][t->sample_count-1].sim_t) {
        if (session_time < t->laps[t->current_index][t->sample_count-1].sim_t) {
            t->open_flags |= TRL_BREAKS;
            t->start_seen = 0;
            t->have_prev = 0;
        }
        return;
    }
    /* Native Hockenheim capture: the lap counter advances before distance
     * wraps (6775.659 -> 2.341 on a 6784.1 m lap). Keep those end-of-lap
     * samples in the old lap; only the witnessed distance wrap closes it. */
    int sample_lap = c->lap_number;
    if (t->lap_open && t->have_prev && sample_lap == t->lap_number + 1 &&
        t->prev_dist >= g_telemetry.lap_len - TRACE_TELEPORT_M &&
        c->lap_dist >= g_telemetry.lap_len - TRACE_TELEPORT_M &&
        c->lap_dist >= t->prev_dist &&
        session_time - t->prev_sim > 0.0f &&
        session_time - t->prev_sim <= TRACE_SIM_GAP_S)
        sample_lap = t->lap_number;
    /* A complete lap requires observed continuous crossings at both ends. */
    float wrap_dt = session_time - t->prev_sim;
    float wrap_ds = g_telemetry.lap_len - t->prev_dist + c->lap_dist;
    int crossing = t->have_prev && sample_lap == t->lap_number + 1 &&
        wrap_dt > 0.0f && wrap_dt <= TRACE_SIM_GAP_S &&
        t->prev_dist >= g_telemetry.lap_len - TRACE_TELEPORT_M &&
        c->lap_dist <= TRACE_TELEPORT_M && wrap_ds > 0.0f &&
        wrap_ds <= TRACE_TELEPORT_M;
    float crossing_time = crossing ? t->prev_sim + wrap_dt *
        (g_telemetry.lap_len - t->prev_dist) / wrap_ds : session_time;
    /* Lap lifecycle. */
    if (!t->lap_open || sample_lap < t->lap_number) {
        if (t->lap_open && sample_lap < t->lap_number) {
            t->dropped_laps++;          /* restart/rewind: open lap discarded */
        }
        t->lap_open = 1;
        t->lap_number = sample_lap;
        t->current_index = t->lap_head;
        if (t->lap_valid[t->current_index]) {
            trace_evict_push(t, t->current_index, session_time);
            t->dropped_laps++;
        }
        t->lap_valid[t->current_index] = 0;
        t->sample_count = 0;
        t->dropped_samples = 0;
        t->open_flags = 0;
        t->start_seen = 0;
        t->have_prev = 0;
    } else if (sample_lap != t->lap_number) {
        /* Lap closed. Only a +1 increment is a complete start-line-to-line
         * lap; anything else is a reset/teleport and is discarded. */
        if (crossing && t->start_seen && !t->dropped_samples &&
            t->sample_count >= TRACE_MIN_LAP_SAMPLES) {
            trace_sample_t *lap = t->laps[t->current_index];
            int breaks = 0;
            for (int k = 1; k < t->sample_count; k++) {
                float ds = lap[k].dist - lap[k-1].dist;
                float dt = lap[k].sim_t - lap[k-1].sim_t;
                if ((lap[k].flags & TRF_SEGMENT) || !(dt > 0.0f) || dt > TRACE_SIM_GAP_S || ds < 0.0f ||
                    ds > TRACE_TELEPORT_M) { breaks = 1; break; }
            }
            int idx = t->current_index;
            t->lap_valid[idx] = TRL_COMPLETE | (t->open_flags & (TRL_PITTED|TRL_BREAKS)) |
                                (breaks ? TRL_BREAKS : 0);
            t->lap_num[idx] = t->lap_number;
            t->lap_time[idx] = crossing_time - t->lap_start_sim[idx];
            t->lap_samples[idx] = t->sample_count;
            t->lap_head = trace_next_head(t, t->lap_head);   /* 1.2.3: never the fastest clean lap */
            t->lap_total++;
            if (t->first_lap_seen < 0) t->first_lap_seen = t->lap_number;
            t->last_lap_seen = t->lap_number;
            ar_on_trace_lap(t, idx);   /* 1.0.1: every completed lap also goes to disk */

        } else {
            t->dropped_laps++;
        }
        /* Open the new lap immediately with the current sample. */
        t->lap_open = 1;
        t->lap_number = sample_lap;
        t->current_index = t->lap_head;
        if (t->lap_valid[t->current_index]) {
            trace_evict_push(t, t->current_index, session_time);
            t->dropped_laps++;
        }
        t->lap_valid[t->current_index] = 0;
        t->sample_count = 0;
        t->dropped_samples = 0;
        t->open_flags = 0;
        t->start_seen = crossing;
        t->lap_start_sim[t->current_index] = crossing_time;
        t->have_prev = 0;
    }
    if (!t->lap_open) return;
    if (t->sample_count == 0 && !t->start_seen)
        t->lap_start_sim[t->current_index] = session_time;

    if (t->sample_count >= TRACE_SAMPLES) { t->dropped_samples++; t->have_prev=0; return; }

    trace_sample_t *s = &t->laps[t->current_index][t->sample_count];
    memset(s, 0, sizeof(*s));
    s->sim_t = session_time;
    s->dist = c->lap_dist;
    s->flags = TRF_DIST;
    if (t->sample_count && !t->have_prev) s->flags |= TRF_SEGMENT;

    /* CO-OP remote cars: only the gated per-car values (car_valid/vf6) count;
     * their raw inputs, accelerations and tyres are not simulated here. */
    int remote = c->controller_role == CTRL_REMOTE;
    if (remote) {
        if ((c->car_valid & 1) && trace_channel_ok(c->speed, 150.0f)) { s->speed = c->speed; s->flags |= TRF_SPEED; }
        /* 1.2.2: network brake (on/off) and steer, as gated by remote_gate */
        if ((c->car_valid & 4) && trace_channel_ok(c->brake, 1.5f)) { s->brake = c->brake; s->flags |= TRF_BRAKE; }
        if ((c->car_valid & 8) && trace_channel_ok(c->steer, 1.5f)) { s->steer = c->steer; s->flags |= TRF_STEER; }
    } else {
    if (c->speed >= 0 && trace_channel_ok(RRAW(veh, OFF_SPEED), 150.0f) && trace_channel_ok(c->speed, 150.0f)) { s->speed = c->speed; s->flags |= TRF_SPEED; }
    if (trace_channel_ok(RRAW(veh, OFF_THROTTLE), 1.5f) && trace_channel_ok(c->throttle, 1.5f)) { s->throttle = c->throttle; s->flags |= TRF_THROTTLE; }
    if (trace_channel_ok(RRAW(veh, OFF_BRAKE), 1.5f) && trace_channel_ok(c->brake, 1.5f)) { s->brake = c->brake; s->flags |= TRF_BRAKE; }

    /* Steering: AI-only per the ledger. The player-side writer of 0x3580 is
     * not proven, so the bit stays clear for non-AI cars. */
    if (c->ai_control) {
        float st = RRAW(veh, OFF_STEER_INPUT);
        if (trace_channel_ok(st, 1.5f)) { s->steer = st; s->flags |= TRF_STEER; }
    }
    {
        float glat = RRAW(veh, OFF_ACCEL_LAT), glon = RRAW(veh, OFF_ACCEL_LON),
              gvert = RRAW(veh, OFF_ACCEL_VERT);
        if (trace_channel_ok(glat, 100.0f)) { s->glat = glat; s->flags |= TRF_GLAT; }
        if (trace_channel_ok(glon, 100.0f)) { s->glon = glon; s->flags |= TRF_GLON; }
        if (trace_channel_ok(gvert, 100.0f)) { s->gvert = gvert; s->flags |= TRF_GVERT; }
    }
    } /* !remote */
    {
        float co = RRAW(veh, OFF_HEADING_COS), si = RRAW(veh, OFF_HEADING_SIN);
        float norm = co * co + si * si;
        if (trace_channel_ok(co, 1.1f) && trace_channel_ok(si, 1.1f) &&
            norm > 0.9f && norm < 1.1f) {
            s->yaw = atan2f(si, co);
            s->flags |= TRF_YAW;
            if (t->have_prev) {
                float dt = session_time - t->prev_sim;
                if (dt > 0.001f && dt <= TRACE_SIM_GAP_S && t->sample_count > 0 &&
                    (t->laps[t->current_index][t->sample_count-1].flags & TRF_YAW) &&
                    s->dist >= t->prev_dist && s->dist - t->prev_dist <= TRACE_TELEPORT_M) {
                    const float pi_f = 3.14159265358979323846f;
                    float d = s->yaw - t->prev_yaw;
                    while (d > pi_f) d -= 2.0f * pi_f;
                    while (d < -pi_f) d += 2.0f * pi_f;
                    s->yaw_rate = d / dt;
                    s->flags |= TRF_YAWRATE;
                }
            }
        }
    }
    /* Tread: player from the thermal mirror (already normalized), AI from the
     * vehicle wheel block +0xB0 (FuelConsumptionAndWear, floor 0.75). Both
     * published as remaining usable tread 0..1. */
    {
        int all = 1;
        float v[4];
        if (t->slot == g_telemetry.player_slot) {
            for (int w = 0; w < 4; w++) {
                double raw = *(const double *)(ADDR_WHEEL_THERMAL + (size_t)w * WHEEL_THERMAL_STRIDE + WH_OFF_TREAD);
                if (!isfinite(raw) || raw < 0.5 || raw > 1.001) all = 0;
                v[w] = g_telemetry.tread[w];
            }
        } else {
            float mir[4] = { c->tire_wear_fl, c->tire_wear_fr,
                             c->tire_wear_rl, c->tire_wear_rr };
            for (int w = 0; w < 4; w++) {
                const int offsets[4] = { OFF_TIRE_WEAR_FL, OFF_TIRE_WEAR_FR, OFF_TIRE_WEAR_RL, OFF_TIRE_WEAR_RR };
                float raw = RRAW(veh, offsets[w]);
                if (!isfinite(raw) || raw < 0.5f || raw > 1.001f) all = 0;
                float n = (mir[w] - TREAD_DEAD) * 4.0f;
                if (!(n > 0.0f)) n = 0.0f;
                if (n > 1.0f) n = 1.0f;
                v[w] = n;
            }
        }
        for (int w = 0; w < 4; w++) {
            if (!isfinite(v[w]) || v[w] < 0.0f || v[w] > 1.0001f) all = 0;
            s->tread[w] = v[w];
        }
        if (all && !remote) s->flags |= TRF_TREAD;
    }

    /* rc.5 workbench channels. RPM is stored converted (raw rad/s x the
     * proven factor) so no consumer can label the raw scale; the live
     * /telemetry field c.r stays raw for schema compatibility. */
    if (c->rpm >= 0.0f && trace_channel_ok(RRAW(veh, OFF_ENGINE_RPM), 2500.0f) && trace_channel_ok(c->rpm, 2500.0f)) {
        s->rpm = (float)((double)c->rpm * LF_RPM_FACTOR);
        s->flags |= TRF_RPM;
    }
    if (c->gear >= 0 && c->gear <= 16) {
        s->gear = c->gear;
        s->flags |= TRF_GEAR;
    }
    if (trace_channel_ok(RRAW(veh, OFF_TRACK_X), 1.0e6f) && trace_channel_ok(RRAW(veh, OFF_TRACK_Z), 1.0e6f) &&
        trace_channel_ok(c->track_x, 1.0e6f) && trace_channel_ok(c->track_z, 1.0e6f)) {
        s->tx = c->track_x;
        s->tz = c->track_z;
        s->flags |= TRF_TRACK;
    }

    /* 4.3.0 viewer channels, from the per-car values already validated in
     * the frame builder (car_telemetry_t v6 block). */
    {
        for (int w = 0; w < 4; w++) {
            s->ttemp[w] = c->ttemp[w];
            s->rx[w] = c->rx_su[w]; s->rx[4 + w] = c->rx_ld[w];
            s->rx[8 + w] = c->rx_sl[w]; s->rx[12 + w] = c->rx_fo[w];
        }
        if (c->vf6 & 4) s->flags |= TRF_TTEMP;
        else if (c->vf6 & 256) s->flags |= TRF_TTEMP | TRF_TTEMP_RX;
        if (c->vf6 & 16) s->flags |= TRF_RX_SU;
        if (c->vf6 & 32) s->flags |= TRF_RX_LD;
        if (c->vf6 & 64) s->flags |= TRF_RX_SL;
        if (c->vf6 & 128) s->flags |= TRF_RX_FO;
    }

    t->channels_seen |= s->flags;
    t->have_prev = 1;
    t->prev_sim = session_time;
    t->prev_dist = s->dist;
    t->prev_yaw = s->yaw;
    t->prev_lap = c->lap_number;
    t->sample_count++;
    t->total_samples++;
}

/* Selected-mode wrapper (kept for the A/B aliases and the runtime tests). */
static void trace_sample_car(int which, int count, float session_time) {
    if (which < 0 || which >= TRACE_CARS) return;
    trace_sample_car_at(&g_trace[which], count, session_time);
}

/* Shared by the selected-pair loop and the all-cars loop: (re)binds one entry
 * to its vehicle slot when the recording identity changed, zeroing the ring. */
static void trace_entry_identity(trace_car_t *t, int slot, int count) {
    car_telemetry_t *current = &g_telemetry.cars[slot];
    volatile uint8_t *current_veh = VEHICLE_PTR(slot);
    int current_id = current_veh ? RI(current_veh, OFF_DRIVER_ID) : -1;
    if (!t->used || t->session_generation != capture_generation ||
        t->slot != slot ||
        (current_veh && t->driver_id != current_id) ||
        /* CO-OP: a slot handed between a bot and a remote human resets */
        (t->used && t->is_remote != (current->controller_role == CTRL_REMOTE)) ||
        (current->name[0] && strcmp(t->driver_name,current->name)) ||
        (current->car_name[0] && strcmp(t->car_name,current->car_name)) ||
        /* Track change inside one generation (defensive): a moved lap
         * length invalidates the recorded distances, so the side resets
         * with a revision bump instead of serving stale laps. */
        (t->used && t->rec_lap_len > 1.0f && g_telemetry.lap_len > 1.0f &&
         fabsf(g_telemetry.lap_len - t->rec_lap_len) > 1.0f)) {
        int user = t->user_selected;
        memset(t, 0, sizeof(*t));
        t->slot = slot;
        t->user_selected = user;
        t->session_generation = capture_generation;
        t->lap_number = -1;
        t->prev_lap = -1;
        t->current_index = 0;
        t->first_lap_seen = -1;
        t->last_lap_seen = -1;
        if (slot >= count) return;
        car_telemetry_t *c = &g_telemetry.cars[slot];
        volatile uint8_t *veh = VEHICLE_PTR(slot);
        t->used = 1;
        t->revision = ++g_trace_revision;
        t->rec_lap_len = g_telemetry.lap_len;
        if (veh && (uintptr_t)veh >= 0x10000)
            t->driver_id = RI(veh, OFF_DRIVER_ID);
        snprintf(t->driver_name, sizeof t->driver_name, "%s", c->name);
        snprintf(t->car_name, sizeof t->car_name, "%s", c->car_name);
        t->is_ai = c->ai_control ? 1 : 0;
        t->is_remote = c->controller_role == CTRL_REMOTE;
        t->team_id = trace_team_id(c->team_id);
    }
}

static void trace_update(int count) {
    if (count > MAX_CARS) count = MAX_CARS;
    trace_default_selection(count);
    if (g_trace_mode == TRACE_MODE_ALL && g_trace_store) {
        /* Owner rule 2026-10-12: record EVERY live car slot from session start,
         * independent of which sides A-F the console shows. Selecting a driver
         * only points the console at the already-recorded entry. */
        int n = count < g_trace_store_cap ? count : g_trace_store_cap;
        for (int slot = 0; slot < n; slot++) {
            trace_car_t *t = &g_trace_store[slot];
            trace_entry_identity(t, slot, count);
            if (!t->used) continue;
            car_telemetry_t *c = &g_telemetry.cars[slot];
            t->team_id = trace_team_id(c->team_id);
            trace_sample_car_at(t, count, (float)g_telemetry.session_time);
        }
        return;
    }
    for (int k = 0; k < TRACE_CARS; k++) {
        trace_car_t *t = &g_trace[k];
        if (t->slot < 0 || t->slot >= count) continue;
        trace_entry_identity(t, t->slot, count);
        if (!t->used) continue;
        car_telemetry_t *c = &g_telemetry.cars[t->slot];
        t->team_id = trace_team_id(c->team_id);
        trace_sample_car(k, count, (float)g_telemetry.session_time);
    }
}

/* ========================================================================
 * SESSION WEATHER RING (rc.11 weather): the session-level weather history for
 * the Weather console page. The values are the same ones the frame already
 * publishes (RESEARCH_WEATHER_2026-10-12 section 1b), so no new game memory
 * is read here: at/trkt in C (CONFIRMED), onw/offw 0..1 (CONFIRMED), tgf raw
 * (DERIVED, ~0.50 in the dry). Cadence: a row every WEATHER_INTERVAL_S of
 * session time, or on a wetness/temperature change; never while the session
 * clock is unchanged (a paused frame is not a new sample). The ring is
 * cleared when capture_generation changes, the same rule the trace ring uses.
 * Owned by the capture side (read_telemetry, under g_telemetry_cs); the HTTP
 * thread builds its document under the same lock.
 * ======================================================================== */
#define WEATHER_RING       2048        /* a 3 h session at 10 s = 1080 rows */
#define WEATHER_INTERVAL_S 10.0f       /* cadence when nothing changes */
#define WEATHER_WET_STEP   0.01f       /* onw/offw append threshold */
#define WEATHER_TEMP_STEP  0.1f        /* at/trkt append threshold, C */
#define WEATHER_JSON_BYTES 196608      /* bounded output buffer for /weather (rc.12: ring + GDB script) */

/* ---- GDB session weather script (rc.12) -----------------------------------
 * The track GDB's Weather { } block is the game's per-session weather plan:
 * Conditions / TrackWetness / OnPathWetness / OffPathWetness / AmbientTemp /
 * TrackTemp per session, then Minute = / Lap = steps with nested value blocks.
 * Parsed once per track load from <track>.gdb next to the AIW (read-only,
 * bounded). Values are raw file tokens (words or numbers); the viewer labels
 * them with the documented tables. FILE DATA ONLY: the live values follow the
 * weekend timeline the game generates at weekend load and writes to
 * LOG\weather.txt (Forecast pane, rc.14); how this script feeds that
 * generator (sub_4FD8B0) is OPEN. */
#define GDB_MAX_SESSIONS 12
#define GDB_MAX_STEPS    32
typedef struct {
    char  kind;                       /* 'm' minute, 'l' lap, 0 = none */
    float at;                         /* step position (minute or lap) */
    char  conditions[16], tw[16], onw[16], offw[16], ambient[16], tracktemp[16];
} gdb_step_t;
typedef struct {
    char        name[16];
    char        conditions[16], tw[16], onw[16], offw[16], ambient[16], tracktemp[16];
    int         step_n;
    gdb_step_t  steps[GDB_MAX_STEPS];
} gdb_session_t;
static gdb_session_t g_gdb[GDB_MAX_SESSIONS];
static int           g_gdb_n = 0;
static int           g_gdb_truncated = 0;
static char          g_gdb_file[64];

/* "key = value" with any indentation; value stops at whitespace; one trailing
 * ')' or ',' is stripped. Returns the key id (0 = not a key line). The token
 * match demands '=' right after the key, so TrackWetness and TrackTemp (and
 * OnPathWetness vs OffPathWetness) stay distinct. */
enum { GDB_K_NONE = 0, GDB_K_COND, GDB_K_TW, GDB_K_ONW, GDB_K_OFFW, GDB_K_AT,
       GDB_K_TT, GDB_K_MINUTE, GDB_K_LAP };
static int gdb_kv(const char *line, char *val, size_t sz) {
    static const struct { const char *k; int id; } tab[] = {
        { "Conditions",     GDB_K_COND   }, { "TrackWetness", GDB_K_TW  },
        { "OnPathWetness",  GDB_K_ONW    }, { "OffPathWetness", GDB_K_OFFW },
        { "AmbientTemp",    GDB_K_AT     }, { "TrackTemp",    GDB_K_TT  },
        { "Minute",         GDB_K_MINUTE }, { "Lap",          GDB_K_LAP }
    };
    while (*line == ' ' || *line == '\t') line++;
    for (int i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++) {
        size_t n = strlen(tab[i].k);
        if (_strnicmp(line, tab[i].k, n) != 0) continue;
        const char *p = line + n;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        size_t j = 0;
        while (p[j] && p[j] != '\r' && p[j] != '\n' && p[j] != ' ' && p[j] != '\t' && j < sz - 1) {
            val[j] = p[j]; j++;
        }
        val[j] = 0;
        size_t L = strlen(val);
        while (L && (val[L - 1] == ')' || val[L - 1] == ',')) val[--L] = 0;
        return tab[i].id;
    }
    return GDB_K_NONE;
}

static int gdb_is_session_name(const char *s) {
    return _stricmp(s, "TestDay") == 0 || _stricmp(s, "Practice1") == 0 ||
           _stricmp(s, "Practice2") == 0 || _stricmp(s, "Practice3") == 0 ||
           _stricmp(s, "Practice4") == 0 || _stricmp(s, "Qualify") == 0 ||
           _stricmp(s, "Warmup") == 0 || _stricmp(s, "Race") == 0;
}

/* One value into the target record; kind picks the field. */
static void gdb_store(char *cond, char *tw, char *onw, char *offw, char *at, char *tt,
                      int kind, const char *val) {
    switch (kind) {
        case GDB_K_COND: snprintf(cond, 16, "%s", val); break;
        case GDB_K_TW:   snprintf(tw, 16, "%s", val); break;
        case GDB_K_ONW:  snprintf(onw, 16, "%s", val); break;
        case GDB_K_OFFW: snprintf(offw, 16, "%s", val); break;
        case GDB_K_AT:   snprintf(at, 16, "%s", val); break;
        case GDB_K_TT:   snprintf(tt, 16, "%s", val); break;
        default: break;
    }
}

static void gdb_script_parse(const char *full_aiw) {
    g_gdb_n = 0; g_gdb_truncated = 0; g_gdb_file[0] = 0;
    char path[MAX_PATH];
    size_t L = strlen(full_aiw);
    if (L < 5 || L + 1 >= sizeof path) return;
    memcpy(path, full_aiw, L + 1);
    char *dot = strrchr(path, '.');
    if (!dot || dot[4] != 0) return;   /* expect ".AIW" */
    strcpy(dot + 1, "gdb");
    const char *bn = strrchr(path, '\\');
    bn = bn ? bn + 1 : path;
    size_t bl = strlen(bn);
    if (bl >= sizeof g_gdb_file) bl = sizeof g_gdb_file - 1;
    memcpy(g_gdb_file, bn, bl); g_gdb_file[bl] = 0;

    FILE *f = fopen(path, "r");
    if (!f) { g_gdb_file[0] = 0; return; }

    int in_weather = 0, depth = 0, sess_pending = 0, sess = -1, step = -1;
    int pend_kind = 0; float pend_at = 0.0f;
    char pend_name[16] = "";
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        char *e = s + strlen(s);
        while (e > s && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (!in_weather) {
            if (_stricmp(s, "Weather") == 0) { in_weather = 1; depth = 0; }
            continue;
        }
        if (!*s) continue;
        if (s[0] == '{' && s[1] == 0) {
            depth++;
            if (depth == 2 && sess_pending) {
                sess_pending = 0;
                if (g_gdb_n < GDB_MAX_SESSIONS) {
                    sess = g_gdb_n++;
                    memset(&g_gdb[sess], 0, sizeof g_gdb[sess]);
                    snprintf(g_gdb[sess].name, sizeof g_gdb[sess].name, "%s", pend_name);
                } else { g_gdb_truncated = 1; sess = -1; }
            } else if (depth == 3 && pend_kind && sess >= 0) {
                if (g_gdb[sess].step_n < GDB_MAX_STEPS) {
                    step = g_gdb[sess].step_n++;
                    memset(&g_gdb[sess].steps[step], 0, sizeof g_gdb[sess].steps[step]);
                    g_gdb[sess].steps[step].kind = (char)pend_kind;
                    g_gdb[sess].steps[step].at = pend_at;
                } else { g_gdb_truncated = 1; step = -1; }
                pend_kind = 0;
            }
            continue;
        }
        if (s[0] == '}' && s[1] == 0) {
            if (depth == 3) step = -1;
            if (depth >= 1) depth--;
            if (depth <= 0) break;
            continue;
        }
        if (depth == 1) {
            if (gdb_is_session_name(s)) {
                sess_pending = 1;
                size_t nl = strlen(s);
                if (nl >= sizeof pend_name) nl = sizeof pend_name - 1;
                memcpy(pend_name, s, nl); pend_name[nl] = 0;
            }
            continue;
        }
        if (depth == 2 || depth == 3) {
            char v[16];
            int kind = gdb_kv(s, v, sizeof v);
            if (!kind) continue;
            if (kind == GDB_K_MINUTE || kind == GDB_K_LAP) {
                if (depth == 2) { pend_kind = kind == GDB_K_MINUTE ? 'm' : 'l'; pend_at = (float)atof(v); }
                continue;
            }
            if (depth == 3 && step >= 0 && sess >= 0)
                gdb_store(g_gdb[sess].steps[step].conditions, g_gdb[sess].steps[step].tw,
                          g_gdb[sess].steps[step].onw, g_gdb[sess].steps[step].offw,
                          g_gdb[sess].steps[step].ambient, g_gdb[sess].steps[step].tracktemp, kind, v);
            else if (depth == 2 && sess >= 0)
                gdb_store(g_gdb[sess].conditions, g_gdb[sess].tw, g_gdb[sess].onw,
                          g_gdb[sess].offw, g_gdb[sess].ambient, g_gdb[sess].tracktemp, kind, v);
            continue;
        }
    }
    fclose(f);
}

typedef struct {
    float st, at, trkt, onw, offw, tgf;   /* NAN = missing, published as null */
} weather_sample_t;

static weather_sample_t g_weather[WEATHER_RING];
static int      g_weather_head;      /* next write slot */
static int      g_weather_count;     /* valid rows, <= WEATHER_RING */
static unsigned g_weather_evicted;   /* oldest rows the ring dropped */
static unsigned g_weather_gen = 0xFFFFFFFFu;

static void weather_ring_clear(void) {
    g_weather_head = 0;
    g_weather_count = 0;
    g_weather_evicted = 0;
}

/* -900 is the console's "missing" sentinel (telemetry_viewer.js sessionPane);
 * anything non-finite is stored as NAN and published as JSON null. */
static float weather_value(float v) {
    return (isfinite(v) && v > -900.0f) ? v : NAN;
}

/* NAN-aware "changed by >= step": a null <-> value edge counts as changed. */
static int weather_changed(float a, float b, float step) {
    int an = !isfinite(a), bn = !isfinite(b);
    if (an != bn) return 1;
    if (an && bn) return 0;
    return fabsf(a - b) >= step;
}

static void weather_update(void) {
    if (g_weather_gen != capture_generation) {
        g_weather_gen = capture_generation;
        weather_ring_clear();
    }
    float st = (float)g_telemetry.session_time;
    if (!isfinite(st)) return;
    float at = weather_value(g_telemetry.ambient_temp);
    float trkt = weather_value(g_telemetry.track_temp);
    float onw = weather_value(g_telemetry.on_wetness);
    float offw = weather_value(g_telemetry.off_wetness);
    float tgf = weather_value(g_telemetry.track_grip_factor);
    if (g_weather_count) {
        weather_sample_t *p = &g_weather[(g_weather_head - 1 + WEATHER_RING) % WEATHER_RING];
        if (st == p->st) return;   /* paused frame: never append */
        /* rc.11 weather (Claude review): the session clock went back without a new
         * generation (restart/rewind): start the history again instead of stalling. */
        if (st < p->st) weather_ring_clear();
        else if (!(st > p->st && st - p->st >= WEATHER_INTERVAL_S) &&
            !weather_changed(onw, p->onw, WEATHER_WET_STEP) &&
            !weather_changed(offw, p->offw, WEATHER_WET_STEP) &&
            !weather_changed(at, p->at, WEATHER_TEMP_STEP) &&
            !weather_changed(trkt, p->trkt, WEATHER_TEMP_STEP))
            return;
    }
    weather_sample_t *s = &g_weather[g_weather_head];
    s->st = st; s->at = at; s->trkt = trkt; s->onw = onw; s->offw = offw; s->tgf = tgf;
    g_weather_head = (g_weather_head + 1) % WEATHER_RING;
    if (g_weather_count < WEATHER_RING) g_weather_count++;
    else g_weather_evicted++;
}

/* /weather document: one array row per sample ([st,at,trkt,onw,offw,tgf]);
 * a missing value is a JSON null. Bounded buffer: on overflow _snprintf sets
 * format_failed and http_send answers with the capacity-exceeded error. */
static int build_weather_json(char *buf, int bufsz) {
    EnterCriticalSection(&g_telemetry_cs);
    char trk[sizeof(g_telemetry.track_name) * 6 + 1];
    esc_json(trk, sizeof trk, g_telemetry.track_name);
    int p = _snprintf(buf, bufsz,
        "{\"api_version\":\"%s\",\"session_generation\":%u,\"session_type\":%d,\"track\":\"%s\","
        "\"interval_s\":%.1f,\"capacity\":%d,\"evicted\":%u,"
        "\"fields\":{\"st\":\"s\",\"at\":\"C\",\"trkt\":\"C\",\"onw\":\"0..1\",\"offw\":\"0..1\",\"tgf\":\"raw\"},"
        "\"tiers\":{\"at\":\"CONFIRMED\",\"trkt\":\"CONFIRMED\",\"onw\":\"CONFIRMED\",\"offw\":\"CONFIRMED\",\"tgf\":\"DERIVED\"},",
        API_VERSION, capture_generation, g_telemetry.session_type, trk,
        WEATHER_INTERVAL_S, WEATHER_RING, g_weather_evicted);
    /* rc.12: the track GDB session weather script (file data; raw word/number
     * tokens). Bounded: GDB_MAX_SESSIONS x GDB_MAX_STEPS; the emission stops
     * early if the ring budget would be at risk (the ring keeps its space). */
    {
        char escf[sizeof g_gdb_file * 6 + 1];
        esc_json(escf, sizeof escf, g_gdb_file);
        p += _snprintf(buf + p, bufsz - p, "\"gdb\":{\"file\":\"%s\",\"truncated\":%s,\"sessions\":[",
                       escf, g_gdb_truncated ? "true" : "false");
        for (int si = 0; si < g_gdb_n && p < bufsz - 90112; si++) {
            gdb_session_t *g = &g_gdb[si];
            char n1[128], c1[128], c2[128], c3[128], c4[128], c5[128], c6[128];
            esc_json(n1, sizeof n1, g->name);         esc_json(c1, sizeof c1, g->conditions);
            esc_json(c2, sizeof c2, g->tw);           esc_json(c3, sizeof c3, g->onw);
            esc_json(c4, sizeof c4, g->offw);         esc_json(c5, sizeof c5, g->ambient);
            esc_json(c6, sizeof c6, g->tracktemp);
            p += _snprintf(buf + p, bufsz - p,
                "%s{\"name\":\"%s\",\"conditions\":\"%s\",\"track_wetness\":\"%s\","
                "\"on_path_wetness\":\"%s\",\"off_path_wetness\":\"%s\","
                "\"ambient\":\"%s\",\"tracktemp\":\"%s\",\"steps\":[",
                si ? "," : "", n1, c1, c2, c3, c4, c5, c6);
            for (int k = 0; k < g->step_n; k++) {
                gdb_step_t *st = &g->steps[k];
                char s1[128], s2[128], s3[128], s4[128];
                esc_json(s1, sizeof s1, st->conditions); esc_json(s2, sizeof s2, st->tw);
                esc_json(s3, sizeof s3, st->ambient);    esc_json(s4, sizeof s4, st->tracktemp);
                if (p >= bufsz - 90112) break;
                p += _snprintf(buf + p, bufsz - p,
                    "%s{\"%s\":%.1f,\"conditions\":\"%s\",\"track_wetness\":\"%s\","
                    "\"ambient\":\"%s\",\"tracktemp\":\"%s\"}",
                    k ? "," : "", st->kind == 'l' ? "lap" : "minute", (double)st->at,
                    s1, s2, s3, s4);
            }
            p += _snprintf(buf + p, bufsz - p, "]}");
        }
        p += _snprintf(buf + p, bufsz - p, "]},");   /* close the sessions array even when empty */
    }
    p += _snprintf(buf + p, bufsz - p, "\"samples\":[");
    for (int i = 0; i < g_weather_count; i++) {
        int idx = (g_weather_head - g_weather_count + i + WEATHER_RING * 2) % WEATHER_RING;
        weather_sample_t *s = &g_weather[idx];
        if (i) bounded_char(buf, bufsz, &p, ',');
        p += _snprintf(buf + p, bufsz - p, "[%.1f,", (double)s->st);
        p += _snprintf(buf + p, bufsz - p, isfinite(s->at) ? "%.1f" : "null", (double)s->at);
        p += _snprintf(buf + p, bufsz - p, isfinite(s->trkt) ? ",%.1f" : ",null", (double)s->trkt);
        p += _snprintf(buf + p, bufsz - p, isfinite(s->onw) ? ",%.3f" : ",null", (double)s->onw);
        p += _snprintf(buf + p, bufsz - p, isfinite(s->offw) ? ",%.3f" : ",null", (double)s->offw);
        p += _snprintf(buf + p, bufsz - p, isfinite(s->tgf) ? ",%.3f]" : ",null]", (double)s->tgf);
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* ========================================================================
 * INCIDENT LOG (overtakes, track limits, penalties, damage, retirements,
 * pit in/out — built by diffing each car's snapshot tick to tick, since
 * the game keeps no event history of its own)
 * ======================================================================== */
#define MAX_INCIDENTS 400
typedef struct {
    DWORD tick;
    int   car_id, lap, a, b;
    char  type[12];
} incident_t;
static incident_t g_incidents[MAX_INCIDENTS];
static int g_incident_head = 0, g_incident_count = 0;

static int g_prev_position[MAX_CARS], g_prev_track_limits[MAX_CARS];
static int g_prev_penalties[MAX_CARS], g_prev_car_conditions[MAX_CARS];
static int g_prev_finish_state[MAX_CARS], g_prev_inpit[MAX_CARS];

static int g_prev_race_active = -1, g_half_fired = 0, g_chequered_fired = 0;
static void incident_push(const char *type, int car_id, int lap, int a, int b);
static void ar_on_incident(const incident_t *e, const char *name);

/* ---- per-part damage latch -------------------------------------------------
 * The wing flags are only 0 between the part detaching and the car reaching the
 * pit box; they reset to 1 afterwards. Sampling here in the 60Hz tick thread and
 * latching means a detach is never missed, whereas the 10Hz SSE client would.
 */
static int g_fw_lost[MAX_CARS], g_rw_lost[MAX_CARS], g_seized_seen[MAX_CARS];
/* have we ever seen this car's wing record INTACT? without it, a 0 read on the
 * first sample after injection is indistinguishable from a live detach. */
static int g_fw_seen_ok[MAX_CARS], g_rw_seen_ok[MAX_CARS];
/* consecutive 60Hz ticks each record has read 0, to debounce the teardown blip */
static int g_fw_zero_ticks[MAX_CARS], g_rw_zero_ticks[MAX_CARS];
/* first settled observation taken for this car? distinguishes 'already broken
 * when we attached' from 'broke while we were watching' */
static int g_wing_baseline[MAX_CARS];
/* highest life value seen per car per component, for the % readout */
static float g_life_max[MAX_CARS][4];
static int g_fw_pre[MAX_CARS], g_rw_pre[MAX_CARS];
/* a real detach needs ~1/6s of persistence; the restart blip is far shorter */
#define DAMAGE_CONFIRM_TICKS 10
/* nothing is believed about damage in the opening seconds of a session */
#define DAMAGE_ARM_AFTER_SEC 8.0
/* why a car is going to retire — sent as `dc` so the UI never has to say just
 * "DOOMED" */
#define DOOM_NONE     0
#define DOOM_REARWING 1   /* rear wing armed: PitStopBlockingCondition retires it at the box */
#define DOOM_WHEEL    2   /* carConditions >= 2, wheel loss */
#define DOOM_ENGINE   3   /* rpm 0 and EngineAngVel 0 in a live session */

static void damage_reset(void) {
    for (int i = 0; i < MAX_CARS; i++) { g_fw_lost[i] = 0; g_rw_lost[i] = 0; g_seized_seen[i] = 0;
                                         g_fw_seen_ok[i] = 0; g_rw_seen_ok[i] = 0;
                                         g_fw_zero_ticks[i] = 0; g_rw_zero_ticks[i] = 0;
                                         g_wing_baseline[i] = 0; g_fw_pre[i] = 0; g_rw_pre[i] = 0;
                                         g_life_max[i][0] = g_life_max[i][1] = 0.0f;
                                         g_life_max[i][2] = g_life_max[i][3] = 0.0f; }
}

static void damage_update(int i, car_telemetry_t *c, volatile uint8_t *veh);

static void incidents_init(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        g_prev_position[i] = -1; g_prev_track_limits[i] = -1;
        g_prev_penalties[i] = -1; g_prev_car_conditions[i] = -1;
        g_prev_finish_state[i] = -1; g_prev_inpit[i] = -1;
    }
    g_prev_race_active = -1; g_half_fired = 0; g_chequered_fired = 0;
    damage_reset();
}

/* session-wide milestones (car_id=-1); call once per tick after the per-car loop */
static void incident_scan_session(int race_active, int total_laps, int leader_lap, int leader_finished) {
    if (race_active && g_prev_race_active == 0) {
        race_log_open();
        incident_push("START", -1, 0, 0, 0);
        g_half_fired = 0; g_chequered_fired = 0;
    }
    /* Injected (or hot-swapped) into a race already under way: the 0 -> 1 edge
     * happened before we existed, so the log would never open and an unattended
     * trace would silently record nothing. Open it on the spot instead — no START
     * bulletin, because we did not witness the start. */
    else if (race_active && !g_race_log && g_prev_race_active == -1) {
        race_log_open();
    }
    if (!race_active && g_prev_race_active == 1) {
        g_half_fired = 0; g_chequered_fired = 0;
        race_log_close();
    }
    race_log_heartbeat();
    race_log_wear_sample();
    if (!g_half_fired && total_laps > 0 && leader_lap > 0 && leader_lap * 2 >= total_laps) {
        incident_push("HALF", -1, leader_lap, total_laps, 0);
        g_half_fired = 1;
    }
    if (!g_chequered_fired && leader_finished) {
        incident_push("CHEQUERED", -1, leader_lap, 0, 0);
        g_chequered_fired = 1;
    }
    if (race_active >= 0) g_prev_race_active = race_active;
}

static void incident_push(const char *type, int car_id, int lap, int a, int b) {
    incident_t *e = &g_incidents[g_incident_head];
    e->tick = GetTickCount();
    strncpy(e->type, type, sizeof(e->type) - 1); e->type[sizeof(e->type) - 1] = '\0';
    e->car_id = car_id; e->lap = lap; e->a = a; e->b = b;
    g_incident_head = (g_incident_head + 1) % MAX_INCIDENTS;
    if (g_incident_count < MAX_INCIDENTS) g_incident_count++;
    /* 1.0.1: the archive keeps every incident (the ring keeps 400) with the
     * driver name at the time, so a later slot reuse cannot rename it. */
    ar_on_incident(e, (car_id >= 0 && car_id < MAX_CARS) ? g_telemetry.cars[car_id].name : "");
    race_log_incident(type, car_id, lap, a, b);
}

/* call once per car per tick, after all fields for that car are populated */
static void incident_scan(int i, car_telemetry_t *c) {
    if (c->position <= 0) return;
    /* pitting >= 2 is the reliable signal. The old fallback `!active && !retired &&
     * fs == 0` used the AI-control bit, so the human driver matched it for the whole
     * race and was reported permanently in the pits. An AI car that has been
     * deactivated but not yet classified still qualifies. */
    /* Only pittingStatus is trustworthy here. The old fallback
     * `!active && !retired && fs == 0` was meant to catch a car parked in its box
     * before it is formally classified out, but 0x45F4 is an AI-control bit, so the
     * ONLY car it ever matched was the human — flagged in the pits all race long
     * while driving. Every other car reading 0 there is already retired, which the
     * !retired term excluded. The fallback could not fire for its intended case. */
    int inpit = (c->pitting >= 2);

    if (g_prev_position[i] >= 0 && c->position != g_prev_position[i])
        incident_push("POS", i, c->lap_number, g_prev_position[i], c->position);
    if (g_prev_track_limits[i] >= 0 && c->track_limits > g_prev_track_limits[i])
        incident_push("LIMITS", i, c->lap_number, g_prev_track_limits[i], c->track_limits);
    if (g_prev_penalties[i] >= 0 && c->penalties > g_prev_penalties[i]) {
        /* HEURISTIC classification: the game exposes no penalty-reason field
         * (checked — sub_4BCCC0's StopGoPenaltyHandler renders a reason
         * string straight to the HUD and never writes it to any offset), so
         * the only signal available is correlation with something else that
         * changed the SAME tick. Track limits incrementing in the same tick
         * as the penalty count is a strong, honest correlation; anything
         * else is reported as unclassified rather than guessed. */
        int tl_same_tick = (g_prev_track_limits[i] >= 0 && c->track_limits > g_prev_track_limits[i]);
        incident_push(tl_same_tick ? "PENALTY_TL" : "PENALTY", i, c->lap_number,
                      g_prev_penalties[i], c->penalties);
        g_car_state[i].clean_lap = 0;
    }
    if (g_prev_car_conditions[i] >= 0 && c->car_conditions > g_prev_car_conditions[i]) {
        incident_push("DAMAGE", i, c->lap_number, g_prev_car_conditions[i], c->car_conditions);
        g_car_state[i].clean_lap = 0;   /* feeds the Driver of the Day score */
    }
    if (g_prev_finish_state[i] >= 0 && c->finish_state != g_prev_finish_state[i] && c->finish_state != 0)
        incident_push("RETIRE", i, c->lap_number, g_prev_finish_state[i], c->finish_state);
    if (g_prev_inpit[i] == 0 && inpit)
        incident_push("PIT_IN", i, c->lap_number, 0, 0);
    if (g_prev_inpit[i] == 1 && !inpit)
        incident_push("PIT_OUT", i, c->lap_number, 0, 0);

    g_prev_position[i] = c->position;
    g_prev_track_limits[i] = c->track_limits;
    g_prev_penalties[i] = c->penalties;
    g_prev_car_conditions[i] = c->car_conditions;
    g_prev_finish_state[i] = c->finish_state;
    g_prev_inpit[i] = inpit;
}

/* ========================================================================
 * TRACK WAYPOINT PATH (static circuit outline, read once per track load)
 * ======================================================================== */
static int   g_wp_count = -1;   /* last-seen waypoint count; -1 = not read yet */
/* rc.12: AIW header groove widths (GrooveWidth, GrooveWidthWet), read by
 * sectors_update's one-per-track file scan; 0 = not read. Used only as map
 * annotation, never as physics. */
static float g_groove_w = 0.0f, g_groove_ww = 0.0f;
static char *esc_json(char *dst, int dsz, const char *src);

static int build_track_json(char *buf, int bufsz) {
    volatile uint8_t *base = (volatile uint8_t *)ADDR_AI_DB_BASE;
    int count = *(volatile int *)(base + OFF_WP_COUNT);
    volatile uint8_t *arr = *(volatile uint8_t **)(base + OFF_WP_ARRAY);

    if (count <= 0 || count > MAX_WAYPOINTS || !arr || (uintptr_t)arr < 0x10000) {
        g_wp_count = count;
        return _snprintf(buf, bufsz, "{\"wc\":%d,\"pts\":[]}", count > 0 ? count : 0);
    }

    int p = _snprintf(buf, bufsz, "{\"wc\":%d,\"pts\":[", count);
    for (int i = 0; i < count; i++) {
        volatile uint8_t *wp = arr + (size_t)i * WP_STRIDE;
        float wx = finite_float(*(const float *)(wp + WP_OFF_X));
        float wz = finite_float(*(const float *)(wp + WP_OFF_Z));
        p += _snprintf(buf + p, bufsz - p, "%s[%.1f,%.1f]", i ? "," : "", wx, wz);
        if (p >= bufsz - 32) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]");
    /* 4.4.0-rc.12 weather map (additive; DERIVED): the dry and wet driving
     * lines from the live AI database: line = wp_pos + wp_path[n] * wp_perp
     * (docs/52 section 4.6). Emitted only when every record passes sanity so
     * the Weather map never draws a wrong line; the viewer also checks the
     * length against wc. Groove widths annotate the band. */
    int have = 1;
    for (int i = 0; i < count && have; i++) {
        volatile uint8_t *wp = arr + (size_t)i * WP_STRIDE;
        float nx = finite_float(*(const float *)(wp + WP_OFF_PERP + 0));
        float nz = finite_float(*(const float *)(wp + WP_OFF_PERP + 8));
        float a  = finite_float(*(const float *)(wp + WP_OFF_PATH + 0));
        float b  = finite_float(*(const float *)(wp + WP_OFF_PATH + 4));
        float n2 = nx * nx + nz * nz;
        if (!(n2 > 0.25f && n2 < 2.25f) || fabsf(a) > 100.0f || fabsf(b) > 100.0f) have = 0;
    }
    p += _snprintf(buf + p, bufsz - p, ",\"gw\":%.3f,\"gww\":%.3f",
                   (double)g_groove_w, (double)g_groove_ww);
    for (int pass = 0; pass < 2; pass++) {
        if (!have) break;
        const char *key = pass ? ",\"wl\":[" : ",\"dl\":[";
        const int   fld = pass ? 4 : 0;   /* wp_path[1] wet, [0] dry */
        if (p >= bufsz - 64) break;       /* fixed margin: bufsz can be small in tests */
        p += _snprintf(buf + p, bufsz - p, key);
        for (int i = 0; i < count; i++) {
            volatile uint8_t *wp = arr + (size_t)i * WP_STRIDE;
            float wx = finite_float(*(const float *)(wp + WP_OFF_X));
            float wz = finite_float(*(const float *)(wp + WP_OFF_Z));
            float nx = finite_float(*(const float *)(wp + WP_OFF_PERP + 0));
            float nz = finite_float(*(const float *)(wp + WP_OFF_PERP + 8));
            float o  = finite_float(*(const float *)(wp + WP_OFF_PATH + fld));
            if (p >= bufsz - 48) { have = 0; break; }   /* partial array is valid JSON; the viewer drops it */
            p += _snprintf(buf + p, bufsz - p, "%s[%.1f,%.1f]", i ? "," : "", wx + o * nx, wz + o * nz);
        }
        p += _snprintf(buf + p, bufsz - p, "]");
    }
    p += _snprintf(buf + p, bufsz - p, "}");
    g_wp_count = count;
    return p;
}

static int build_laps_json(char *buf, int bufsz) {
    EnterCriticalSection(&g_telemetry_cs);
    int p = _snprintf(buf, bufsz, "{\"gen\":%u,\"cars\":[", capture_generation);
    int first = 1;
    int cnt = g_telemetry.vehicle_count;
    if (cnt > MAX_CARS) cnt = MAX_CARS;
    for (int i = 0; i < cnt; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position <= 0) continue;
        char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
        if (!first) bounded_char(buf,bufsz,&p,',');
        first = 0;
        p += _snprintf(buf + p, bufsz - p, "{\"id\":%d,\"p\":%d,\"n\":\"%s\",\"laps\":[",
            i, c->position, esc);
        int lc = g_lap_history_count[i];
        for (int j = 0; j < lc; j++)
            p += _snprintf(buf + p, bufsz - p, "%s%.3f", j ? "," : "", g_lap_history[i][j]);
        p += _snprintf(buf + p, bufsz - p, "]}");
        if (p >= bufsz - 64) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* ========================================================================
 * ENGINEERING TRACE ENDPOINTS (4.2.0)
 * ======================================================================== */
#define TRACE_GRID_MAX 1000

/* Grid channel mask bits (per grid point). */
#define TGF_SPEED   (1u<<0)
#define TGF_THROTTLE (1u<<1)
#define TGF_BRAKE   (1u<<2)
#define TGF_STEER   (1u<<3)
#define TGF_GLAT    (1u<<4)
#define TGF_GLON    (1u<<5)
#define TGF_GVERT   (1u<<6)
#define TGF_YAW     (1u<<7)
#define TGF_YAWRATE (1u<<8)
#define TGF_TREAD   (1u<<9)
#define TGF_ELAPSED (1u<<10)
#define TGF_RPM     (1u<<11) /* rc.5 */
#define TGF_TTEMP   (1u<<12) /* 4.3.0 */
#define TGF_RX_SU   (1u<<13) /* 4.3.0 RESEARCH */
#define TGF_RX_LD   (1u<<14)
#define TGF_RX_SL   (1u<<15)
#define TGF_RX_FO   (1u<<16)

typedef struct {
    float dist[TRACE_GRID_MAX];
    float speed[TRACE_GRID_MAX], throttle[TRACE_GRID_MAX], brake[TRACE_GRID_MAX];
    float steer[TRACE_GRID_MAX];
    float glat[TRACE_GRID_MAX], glon[TRACE_GRID_MAX], gvert[TRACE_GRID_MAX];
    float yaw[TRACE_GRID_MAX], yaw_rate[TRACE_GRID_MAX];
    float tread[4][TRACE_GRID_MAX];
    float elapsed[TRACE_GRID_MAX];
    float rpm[TRACE_GRID_MAX]; /* rc.5: display RPM */
    float ttemp[4][TRACE_GRID_MAX];   /* 4.3.0 */
    float rx[16][TRACE_GRID_MAX];     /* 4.3.0 RESEARCH */
    unsigned int have[TRACE_GRID_MAX];
    int  breaks;
    float break_dist[64];
    int  break_n;
} trace_grid_t;

static trace_grid_t g_grid_a, g_grid_b;

static int trace_find_lap(const trace_car_t *t, int lap) {
    for (int i = 0; i < TRACE_LAP_SLOTS; i++)
        if (t->lap_num[i] == lap && (t->lap_valid[i] & TRL_COMPLETE))
            return i;
    return -1;
}

/* Interpolate one recorded lap onto a distance grid. Only spans between two
 * consecutive samples that are both valid for a channel and that do not cross
 * a break (sim gap, reverse travel, teleport) are filled; everything else
 * stays a hole. Never bridges segments. The samples variant also serves
 * imported laps (fix 2: /trace/compare accepts an import on either side);
 * start_sim is the lap's own time origin for the elapsed channel. */
static void trace_grid_fill_samples(const trace_sample_t *s, int count, float start_sim,
                                    float step, int n, trace_grid_t *g) {
    memset(g, 0, sizeof(*g));
    for (int k = 0; k + 1 < count; k++) {
        float d0 = s[k].dist, d1 = s[k + 1].dist;
        float dt = s[k + 1].sim_t - s[k].sim_t;
        int broken = !!(s[k+1].flags & TRF_SEGMENT);
        if (!(dt > 0.0f) || dt > TRACE_SIM_GAP_S) broken = 1;
        if (!(d1 > d0)) broken = 1;                 /* reverse/duplicate */
        if (d1 - d0 > TRACE_TELEPORT_M) broken = 1; /* teleport/reset */
        if (broken) {
            g->breaks++;
            if (g->break_n < 64) g->break_dist[g->break_n++] = d0;
            continue;
        }
        int i0 = (int)ceilf(d0 / step), i1 = (int)floorf(d1 / step);
        if (i0 < 0) i0 = 0;
        if (i1 >= n) i1 = n - 1;
        uint32_t both = s[k].flags & s[k + 1].flags;
        for (int i = i0; i <= i1; i++) {
            float d = i * step;
            float f = (d - d0) / (d1 - d0);
            if (f < 0.0f) f = 0.0f;
            if (f > 1.0f) f = 1.0f;
            g->dist[i] = d;
#define TGI(tbit, gbit, field) \
            if ((both & (tbit)) && !(g->have[i] & (gbit))) { \
                g->field[i] = s[k].field + (s[k+1].field - s[k].field) * f; \
                g->have[i] |= (gbit); \
            }
            TGI(TRF_SPEED, TGF_SPEED, speed)
            TGI(TRF_THROTTLE, TGF_THROTTLE, throttle)
            TGI(TRF_BRAKE, TGF_BRAKE, brake)
            TGI(TRF_STEER, TGF_STEER, steer)
            TGI(TRF_GLAT, TGF_GLAT, glat)
            TGI(TRF_GLON, TGF_GLON, glon)
            TGI(TRF_GVERT, TGF_GVERT, gvert)
            TGI(TRF_YAWRATE, TGF_YAWRATE, yaw_rate)
            TGI(TRF_RPM, TGF_RPM, rpm)
#undef TGI
            if ((both & TRF_YAW) && !(g->have[i] & TGF_YAW)) {
                float y1 = s[k + 1].yaw;
                const float pi_f = 3.14159265358979323846f;
                while (y1 - s[k].yaw > pi_f) y1 -= 2.0f * pi_f;
                while (y1 - s[k].yaw < -pi_f) y1 += 2.0f * pi_f;
                g->yaw[i] = s[k].yaw + (y1 - s[k].yaw) * f;
                g->have[i] |= TGF_YAW;
            }
            if ((both & TRF_TREAD) && !(g->have[i] & TGF_TREAD)) {
                for (int w = 0; w < 4; w++)
                    g->tread[w][i] = s[k].tread[w] + (s[k+1].tread[w] - s[k].tread[w]) * f;
                g->have[i] |= TGF_TREAD;
            }
            if ((both & TRF_TTEMP) && !(g->have[i] & TGF_TTEMP)) {
                for (int w = 0; w < 4; w++)
                    g->ttemp[w][i] = s[k].ttemp[w] + (s[k+1].ttemp[w] - s[k].ttemp[w]) * f;
                g->have[i] |= TGF_TTEMP;
            }
            {
                static const uint32_t rtb[4] = { TRF_RX_SU, TRF_RX_LD, TRF_RX_SL, TRF_RX_FO };
                static const unsigned rgb[4] = { TGF_RX_SU, TGF_RX_LD, TGF_RX_SL, TGF_RX_FO };
                for (int q = 0; q < 4; q++) {
                    if (!(both & rtb[q]) || (g->have[i] & rgb[q])) continue;
                    for (int w = 0; w < 4; w++) {
                        int r = q * 4 + w;
                        g->rx[r][i] = s[k].rx[r] + (s[k+1].rx[r] - s[k].rx[r]) * f;
                    }
                    g->have[i] |= rgb[q];
                }
            }
            if (!(g->have[i] & TGF_ELAPSED)) {
                g->elapsed[i] = s[k].sim_t + (s[k + 1].sim_t - s[k].sim_t) * f - start_sim;
                g->have[i] |= TGF_ELAPSED;
            }
        }
    }
}

static void trace_grid_fill(const trace_car_t *t, int lap, float step, int n,
                            trace_grid_t *g) {
    trace_grid_fill_samples(t->laps[lap], t->lap_samples[lap], t->lap_start_sim[lap],
                            step, n, g);
}

static int trace_emit_channels(char *buf, int bufsz, int p, const trace_grid_t *g,
                               int n, int last) {
    const char *names[] = { "speed","throttle","brake","steer","glat","glon",
                            "gvert","yaw","yaw_rate","rpm" };
    const unsigned short bits[] = { TGF_SPEED,TGF_THROTTLE,TGF_BRAKE,TGF_STEER,
                                    TGF_GLAT,TGF_GLON,TGF_GVERT,TGF_YAW,TGF_YAWRATE,
                                    TGF_RPM };
    const float *arrays[] = { g->speed,g->throttle,g->brake,g->steer,g->glat,
                              g->glon,g->gvert,g->yaw,g->yaw_rate,g->rpm };
    for (int c = 0; c < 10; c++) {
        p += _snprintf(buf + p, bufsz - p, "\"%s\":[", names[c]);
        for (int i = 0; i < n; i++) {
            if (g->have[i] & bits[c]) {
                if (c == 7 || c == 8)
                    p += _snprintf(buf + p, bufsz - p, "%s%.3f", i ? "," : "", arrays[c][i]);
                else
                    p += _snprintf(buf + p, bufsz - p, "%s%.2f", i ? "," : "", arrays[c][i]);
            } else {
                p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
            }
            if (p >= bufsz - 64) return p;
        }
        p += _snprintf(buf + p, bufsz - p, "],");
    }
    for (int w = 0; w < 4; w++) {
        p += _snprintf(buf + p, bufsz - p, "\"tread%d\":[", w);
        for (int i = 0; i < n; i++) {
            if (g->have[i] & TGF_TREAD)
                p += _snprintf(buf + p, bufsz - p, "%s%.4f", i ? "," : "", g->tread[w][i]);
            else
                p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
            if (p >= bufsz - 64) return p;
        }
        p += _snprintf(buf + p, bufsz - p, "],");
    }
    for (int w = 0; w < 4; w++) {
        p += _snprintf(buf + p, bufsz - p, "\"ttemp%d\":[", w);
        for (int i = 0; i < n; i++) {
            if (g->have[i] & TGF_TTEMP)
                p += _snprintf(buf + p, bufsz - p, "%s%.1f", i ? "," : "", g->ttemp[w][i]);
            else
                p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
            if (p >= bufsz - 64) return p;
        }
        p += _snprintf(buf + p, bufsz - p, "],");
    }
    {
        static const char *rn[4] = { "rx_su", "rx_ld", "rx_sl", "rx_fo" };
        static const unsigned rgb[4] = { TGF_RX_SU, TGF_RX_LD, TGF_RX_SL, TGF_RX_FO };
        for (int r = 0; r < 16; r++) {
            p += _snprintf(buf + p, bufsz - p, "\"%s%d\":[", rn[r / 4], r % 4);
            for (int i = 0; i < n; i++) {
                if (g->have[i] & rgb[r / 4])
                    p += _snprintf(buf + p, bufsz - p, "%s%.4g", i ? "," : "", g->rx[r][i]);
                else
                    p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
                if (p >= bufsz - 64) return p;
            }
            p += _snprintf(buf + p, bufsz - p, "],");
        }
    }
    p += _snprintf(buf + p, bufsz - p, "\"elapsed\":[");
    for (int i = 0; i < n; i++) {
        if (g->have[i] & TGF_ELAPSED)
            p += _snprintf(buf + p, bufsz - p, "%s%.3f", i ? "," : "", g->elapsed[i]);
        else
            p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
        if (p >= bufsz - 64) return p;
    }
    p += _snprintf(buf + p, bufsz - p, "]%s", last ? "" : ",");
    return p;
}

static int trace_grid_avail(const trace_grid_t *g, int n) {
    unsigned int seen = 0;
    for (int i = 0; i < n; i++) seen |= g->have[i];
    return seen;
}

static int build_trace_status_json(char *buf, int bufsz) {
    EnterCriticalSection(&g_telemetry_cs);
    int all = g_trace_mode == TRACE_MODE_ALL && g_trace_store != NULL;
    int n = all ? g_trace_store_cap : TRACE_CARS;
    int p = _snprintf(buf, bufsz,
        "{\"gen\":%u,\"lap_len\":%.1f,\"laps_max\":%d,\"samples_max\":%d,\"mode\":\"%s\",\"cars\":[",
        capture_generation, g_telemetry.lap_len, TRACE_LAPS, TRACE_SAMPLES,
        all ? "all" : "selected");
    int emitted = 0;
    for (int k = 0; k < n; k++) {
        trace_car_t *t = all ? &g_trace_store[k] : &g_trace[k];
        char which_text[2] = { 0, 0 };
        if (all) {
            if (!t->used && k >= g_telemetry.vehicle_count) continue;
            which_text[0] = (k == g_trace_slot[0]) ? 'A' : (k == g_trace_slot[1]) ? 'B' : 0;
        } else {
            which_text[0] = (char)('A' + k);
        }
        char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
        esc_json(escn, sizeof escn, t->driver_name);
        esc_json(escc, sizeof escc, t->car_name);
        if (emitted) bounded_char(buf, bufsz, &p, ',');
        emitted++;
        p += _snprintf(buf + p, bufsz - p,
            "{\"rev\":%u,\"which\":\"%s\",\"slot\":%d,\"used\":%d,\"auto\":%d,\"n\":\"%s\","
            "\"cn\":\"%s\",\"ai\":%d,\"remote\":%d,\"team\":%u,\"dropped_laps\":%u,"
            "\"open\":%d,\"open_lap\":%d,\"open_samples\":%d,\"open_dropped\":%u,"
            "\"channels_seen\":%u,\"total_samples\":%u,\"laps\":[",
            t->revision, which_text, t->slot, t->used,
            all ? (which_text[0] ? g_trace_auto[which_text[0] - 'A'] : 0) : g_trace_auto[k],
            escn, escc,
            t->is_ai, t->is_remote, t->team_id, t->dropped_laps,
            t->lap_open, t->lap_number, t->sample_count, t->dropped_samples,
            t->channels_seen, t->total_samples);
        int first = 1;
        for (int i = 0; i < TRACE_LAP_SLOTS; i++) {
            if (!(t->lap_valid[i] & TRL_COMPLETE)) continue;
            if (!first) bounded_char(buf, bufsz, &p, ',');
            first = 0;
            /* Fix 2: every completed lap carries its stable identity
             * (generation + revision + slot + lap + source). */
            p += _snprintf(buf + p, bufsz - p,
                "{\"lap\":%d,\"t\":%.3f,\"samples\":%d,\"flags\":%d,\"start\":%.3f,"
                "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"live\"}}",
                t->lap_num[i], t->lap_time[i], t->lap_samples[i],
                t->lap_valid[i], t->lap_start_sim[i],
                capture_generation, t->revision, t->slot, t->lap_num[i]);
        }
        p += _snprintf(buf + p, bufsz - p, "],\"unavailable\":[");
        const char *names[] = { "speed","throttle","brake","steering",
            "lateral_g","longitudinal_g","vertical_g","yaw","yaw_rate","tread",
            "rpm","gear","track","tyre_temp" };
        const unsigned bits[] = { TRF_SPEED,TRF_THROTTLE,TRF_BRAKE,TRF_STEER,
            TRF_GLAT,TRF_GLON,TRF_GVERT,TRF_YAW,TRF_YAWRATE,TRF_TREAD,
            TRF_RPM,TRF_GEAR,TRF_TRACK,TRF_TTEMP };
        {
            int f2 = 1;
            for (int c = 0; c < 14; c++) {
                if (t->channels_seen & bits[c]) continue;
                if (!f2) bounded_char(buf, bufsz, &p, ',');
                f2 = 0;
                p += _snprintf(buf + p, bufsz - p, "\"%s\"", names[c]);
            }
        }
        /* Fix 3 (api_version 2): every unavailable channel carries a
         * machine-readable reason; player steering names the pending live
         * check, AI steering its own channel. */
        p += _snprintf(buf + p, bufsz - p, "],\"unavailable_why\":{");
        {
            int f3 = 1;
            for (int c = 0; c < 14; c++) {
                if (t->channels_seen & bits[c]) continue;
                if (!f3) bounded_char(buf, bufsz, &p, ',');
                f3 = 0;
                const char *why = t->is_remote && strcmp(names[c], "track") != 0 &&
                    strcmp(names[c], "rpm") != 0 && strcmp(names[c], "yaw") != 0
                    ? "remote car (CO-OP controller mode 2): simulated on the other PC; not available here"
                    : strcmp(names[c], "steering") == 0
                    ? (t->is_ai ? "AI steering channel invalid this session"
                                : "player steering unavailable: live steering check pending (veh+0x3580 writer unproven)")
                    : strcmp(names[c], "tyre_temp") == 0
                    ? (t->is_ai ? "AI tyre temperature is a RESEARCH candidate (wheel+0x50) and read no plausible Kelvin value"
                                : "player tyre thermal block not simulated or out of range")
                    : "no valid samples recorded this session";
                p += _snprintf(buf + p, bufsz - p, "\"%s\":\"%s\"", names[c], why);
            }
        }
        /* Fix 2: explicit ring-eviction log (bounded, oldest first). */
        p += _snprintf(buf + p, bufsz - p, "},\"evicted\":[");
        {
            int f4 = 1;
            int n = t->evicted_count;
            for (int j = 0; j < n; j++) {
                int idx = (t->evicted_head - n + j + TRACE_EVICT_MAX * 2) % TRACE_EVICT_MAX;
                trace_evict_t *e = &t->evicted[idx];
                if (!f4) bounded_char(buf, bufsz, &p, ',');
                f4 = 0;
                p += _snprintf(buf + p, bufsz - p,
                    "{\"lap\":%d,\"t\":%.3f,\"at\":%.3f,"
                    "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"live\"}}",
                    e->lap, e->t, e->at,
                    capture_generation, t->revision, t->slot, e->lap);
            }
        }
        p += _snprintf(buf + p, bufsz - p, "]}");
        if (p >= bufsz - 64) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* Serialize one recorded sample as an f1c.trace/1 JSON row. Shared by the
 * completed-lap, open-lap and imported-lap builders so every surface carries
 * the same fields (rc.5 adds rpm/ge/tx/tz with per-field nulls). */
static int trace_emit_sample_json(char *buf, int bufsz, int p,
                                  const trace_sample_t *s, int first) {
    char stb[24], glb[24], gob[24], gvb[24], yb[24], yrb[24], rb[24], gb[8];
    char txb[24], tzb[24], ttb[80], rxb[288];
    if (s->flags & TRF_TTEMP)
        snprintf(ttb, sizeof ttb, "[%.1f,%.1f,%.1f,%.1f]", s->ttemp[0], s->ttemp[1], s->ttemp[2], s->ttemp[3]);
    else strcpy(ttb, "null");
    if (s->flags & (TRF_RX_SU | TRF_RX_LD | TRF_RX_SL | TRF_RX_FO)) {
        int q = snprintf(rxb, sizeof rxb, "[");
        for (int k = 0; k < 16 && q < (int)sizeof rxb - 20; k++)
            q += snprintf(rxb + q, sizeof rxb - q, "%s%.4g", k ? "," : "", isfinite(s->rx[k]) ? s->rx[k] : 0.0f);
        snprintf(rxb + q, sizeof rxb - q, "]");
    } else strcpy(rxb, "null");
    if (s->flags & TRF_STEER) snprintf(stb, sizeof stb, "%.4f", s->steer);
    else strcpy(stb, "null");
    if (s->flags & TRF_GLAT) snprintf(glb, sizeof glb, "%.2f", s->glat);
    else strcpy(glb, "null");
    if (s->flags & TRF_GLON) snprintf(gob, sizeof gob, "%.2f", s->glon);
    else strcpy(gob, "null");
    if (s->flags & TRF_GVERT) snprintf(gvb, sizeof gvb, "%.2f", s->gvert);
    else strcpy(gvb, "null");
    if (s->flags & TRF_YAW) snprintf(yb, sizeof yb, "%.3f", s->yaw);
    else strcpy(yb, "null");
    if (s->flags & TRF_YAWRATE) snprintf(yrb, sizeof yrb, "%.3f", s->yaw_rate);
    else strcpy(yrb, "null");
    if (s->flags & TRF_RPM) snprintf(rb, sizeof rb, "%.1f", s->rpm);
    else strcpy(rb, "null");
    if (s->flags & TRF_GEAR) snprintf(gb, sizeof gb, "%d", s->gear);
    else strcpy(gb, "null");
    if (s->flags & TRF_TRACK) {
        snprintf(txb, sizeof txb, "%.1f", s->tx);
        snprintf(tzb, sizeof tzb, "%.1f", s->tz);
    } else {
        strcpy(txb, "null"); strcpy(tzb, "null");
    }
    return p + _snprintf(buf + p, bufsz - p,
        "%s{\"t\":%.3f,\"d\":%.1f,\"s\":%.2f,\"th\":%.2f,\"br\":%.2f,"
        "\"st\":%s,\"gl\":%s,\"go\":%s,\"gv\":%s,\"y\":%s,\"yr\":%s,"
        "\"rpm\":%s,\"ge\":%s,\"tx\":%s,\"tz\":%s,"
        "\"tr\":[%.4f,%.4f,%.4f,%.4f],\"tt\":%s,\"rx\":%s,\"f\":%u}",
        first ? "" : ",", s->sim_t, s->dist, s->speed, s->throttle, s->brake,
        stb, glb, gob, gvb, yb, yrb, rb, gb, txb, tzb,
        s->tread[0], s->tread[1], s->tread[2], s->tread[3], ttb, rxb, s->flags);
}

/* Returns p>=0 on success; the handler maps the negatives instead of serving
 * a fabricated empty trace (fix 2, api_version 2):
 *   -1 lap never recorded (evicted:false)
 *   -2 lap was evicted from the bounded ring (evicted:true)
 *   -3 gen mismatch, session changed (409)
 *   -4 rev mismatch, recorder identity reset (409, evicted:true)
 * gen/rev are optional identity pins (pass -1 to skip the check). */
static int build_trace_data_json_id(char *buf, int bufsz, int which, int lap,
                                    long gen, long rev) {
    EnterCriticalSection(&g_telemetry_cs);
    int letter = -1;
    trace_car_t *t = trace_pick(which, &letter);
    if (!t) { LeaveCriticalSection(&g_telemetry_cs); return -1; }
    if (gen >= 0 && (unsigned long)gen != capture_generation) {
        LeaveCriticalSection(&g_telemetry_cs); return -3;
    }
    if (rev >= 0 && (unsigned long)rev != t->revision) {
        LeaveCriticalSection(&g_telemetry_cs); return -4;
    }
    int idx = trace_find_lap(t, lap);
    if (idx < 0) {
        /* Within this revision the completed-lap range separates an evicted
         * reference (lap number already seen and since overwritten) from a lap
         * that was never recorded. */
        int evicted = t->first_lap_seen >= 0 && lap >= t->first_lap_seen
                      && lap <= t->last_lap_seen;
        LeaveCriticalSection(&g_telemetry_cs);
        return evicted ? -2 : -1;
    }
    const trace_sample_t *s = t->laps[idx];
    int count = t->lap_samples[idx];
    char track[sizeof(g_telemetry.track_name)*6+1];
    esc_json(track,sizeof track,g_telemetry.track_name);
    char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
    esc_json(escn, sizeof escn, t->driver_name);
    esc_json(escc, sizeof escc, t->car_name);
    char which_text[2] = { letter >= 0 ? (char)('A' + letter) : 0, 0 };
    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.trace/1\",\"source\":\"live\",\"open\":false,"
        "\"gen\":%u,\"rev\":%u,\"which\":\"%s\",\"slot\":%d,"
        "\"driver\":\"%s\",\"car\":\"%s\",\"ai\":%d,\"team\":%u,\"lap\":%d,"
        "\"lap_time\":%.3f,\"start_sim\":%.3f,\"lap_len\":%.1f,\"count\":%d,"
        "\"flags\":%d,\"channels_seen\":%u,\"track\":\"%s\","
        "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"live\"},"
        "\"samples\":[",
        capture_generation, t->revision, which_text, t->slot, escn, escc,
        t->is_ai, t->team_id, t->lap_num[idx], t->lap_time[idx],
        t->lap_start_sim[idx], g_telemetry.lap_len, count,
        t->lap_valid[idx], t->channels_seen, track,
        capture_generation, t->revision, t->slot, t->lap_num[idx]);
    int truncated = 0;
    for (int i = 0; i < count; i++) {
        p = trace_emit_sample_json(buf, bufsz, p, &s[i], i == 0);
        if (p >= bufsz - 256) { truncated = 1; break; }
    }
    p += _snprintf(buf + p, bufsz - p, "]%s}", truncated ? ",\"truncated\":true" : "");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* Compatibility wrapper (no identity pin); also used by the C test builders. */
static int build_trace_data_json(char *buf, int bufsz, int which, int lap) {
    return build_trace_data_json_id(buf, bufsz, which, lap, -1, -1);
}

/* rc.5: serve the open lap as it is being driven (bounded by TRACE_SAMPLES),
 * so the workbench draws partial laps without waiting for a completed lap.
 * Returns -1 when this side has no open lap with samples. */
static int build_trace_open_json(char *buf, int bufsz, int which) {
    EnterCriticalSection(&g_telemetry_cs);
    int letter = -1;
    trace_car_t *t = trace_pick(which, &letter);
    if (!t || !t->used || !t->lap_open || t->sample_count <= 0) {
        LeaveCriticalSection(&g_telemetry_cs);
        return -1;
    }
    const trace_sample_t *s = t->laps[t->current_index];
    int count = t->sample_count;
    char track[sizeof(g_telemetry.track_name)*6+1];
    esc_json(track,sizeof track,g_telemetry.track_name);
    char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
    esc_json(escn, sizeof escn, t->driver_name);
    esc_json(escc, sizeof escc, t->car_name);
    char which_text[2] = { letter >= 0 ? (char)('A' + letter) : 0, 0 };
    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.trace/1\",\"source\":\"live\",\"open\":true,"
        "\"gen\":%u,\"rev\":%u,\"which\":\"%s\",\"slot\":%d,"
        "\"driver\":\"%s\",\"car\":\"%s\",\"ai\":%d,\"team\":%u,\"lap\":%d,"
        "\"lap_time\":0.000,\"start_sim\":%.3f,\"lap_len\":%.1f,\"count\":%d,"
        "\"flags\":%d,\"channels_seen\":%u,\"track\":\"%s\","
        "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"live\"},"
        "\"samples\":[",
        capture_generation, t->revision, which_text, t->slot, escn, escc,
        t->is_ai, t->team_id, t->lap_number,
        t->lap_start_sim[t->current_index], g_telemetry.lap_len, count,
        t->open_flags, t->channels_seen, track,
        capture_generation, t->revision, t->slot, t->lap_number);
    int truncated = 0;
    for (int i = 0; i < count; i++) {
        p = trace_emit_sample_json(buf, bufsz, p, &s[i], i == 0);
        if (p >= bufsz - 256) { truncated = 1; break; }
    }
    p += _snprintf(buf + p, bufsz - p, "]%s}", truncated ? ",\"truncated\":true" : "");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* ========================================================================
 * LEGACY IMPORT (rc.5): POST /trace/import/legacy converts a .lap or .spt
 * body into an f1c.trace/1 document with the proven C decoders
 * (legacy_formats.c). Units follow the accepted STOP 3R conversion table
 * with the parallel-C corrections (PCODE_SEMANTICS_2026-10-09):
 *  - viewer velocity is mph -> m/s via 1/2.2371365 (DM#8 constant);
 *  - .lap acceleration channels are g -> m/s2 via x9.8 (DM#8 g-conversion;
 *    numeric check on the real lap: max lateral 3.32 g, rest vertical ~1.0);
 *  - yaw is degrees -> radians;
 *  - spt has no distance/speed channel: both are derived exactly per DM#8
 *    (cumulative sqrt(dx^2+dz^2) over x/z, y unused; v = d/dt with the
 *    1e-7 floor and the dt>0.01 hold rule; the mph scale cancels to m/s);
 *  - tyre/damper derived formulas (FileReading#0) are cited but not carried:
 *    the f1c.trace/1 model has no temperature/damper channels.
 * The converted document is cached per side (A/B) for GET /trace/import.
 * ======================================================================== */
#define IMPORT_MPH_PER_MPS 2.2371365
#define IMPORT_ACCEL_G 9.8
#define IMPORT_BODY_MAX (16u * 1024u * 1024u)
#define TRACE_JSON_BYTES 2097152 /* 4.4.0: worst-case 3200 rows with tt/rx ~1.5 MB + margin (was 1572864 for 2400 rows) */
static char g_import_json[2][TRACE_JSON_BYTES];
static int  g_import_len[2];
/* Fix 2 (api_version 2): the decoded import is kept beside its JSON so
 * /trace/compare can use an imported lap as either side, and every import
 * surface carries a stable lid (gen + rev + slot + lap + src:"import") plus a
 * per-side serial that makes a replacement detectable. */
static trace_sample_t g_import_samples[2][TRACE_SAMPLES];
static int   g_import_count[2];
static float g_import_start_sim[2];
static float g_import_lap_time[2];
static char  g_import_driver[2][NAME_LEN];
static unsigned g_import_serial[2];

static int import_fail(char *err, size_t err_cap, const char *msg) {
    if (err && err_cap) snprintf(err, err_cap, "%s", msg);
    return -1;
}

static void import_copy_str(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (!cap) return;
    if (src) while (src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static float import_lap_channel(const lf_lap_t *lap, int channel, int sample) {
    return lap->samples[(size_t)sample * (size_t)lap->channel_count + channel];
}

static int import_emit_finish(char *buf, int bufsz, int p, int emitted, int total,
                              int stride, int truncated, uint32_t channels_seen) {
    if (stride > 1)
        p += _snprintf(buf + p, bufsz - p,
            "]%s,\"reduction\":{\"from\":%d,\"to\":%d,\"stride\":%d},\"channels_seen\":%u}",
            truncated ? ",\"truncated\":true" : "", total, emitted, stride, channels_seen);
    else
        p += _snprintf(buf + p, bufsz - p, "]%s,\"channels_seen\":%u}",
            truncated ? ",\"truncated\":true" : "", channels_seen);
    return p;
}

/* .lap -> f1c.trace/1. Stored channels are the viewer's own derived output;
 * only the unit corrections above are applied, each one recorded in
 * "conversions". */
static int import_lap_json(char *buf, int bufsz, const lf_lap_t *lap,
                           size_t file_bytes, char which, char *err, size_t err_cap) {
    int c_time = lf_lap_channel_index(lap, "time");
    int c_dist = lf_lap_channel_index(lap, "distance");
    int c_vel  = lf_lap_channel_index(lap, "velocity");
    int c_th   = lf_lap_channel_index(lap, "throttle");
    int c_br   = lf_lap_channel_index(lap, "brake");
    int c_st   = lf_lap_channel_index(lap, "steering");
    int c_rpm  = lf_lap_channel_index(lap, "engine rpm");
    int c_gear = lf_lap_channel_index(lap, "gear");
    int c_yaw  = lf_lap_channel_index(lap, "yaw");
    int c_lat  = lf_lap_channel_index(lap, "lat accel");
    int c_lon  = lf_lap_channel_index(lap, "long accel");
    int c_vert = lf_lap_channel_index(lap, "vert accel");
    int c_x    = lf_lap_channel_index(lap, "x");
    int c_z    = lf_lap_channel_index(lap, "z");
    int c_wear[4];
    c_wear[0] = lf_lap_channel_index(lap, "fl tyre wear");
    c_wear[1] = lf_lap_channel_index(lap, "fr tyre wear");
    c_wear[2] = lf_lap_channel_index(lap, "rl tyre wear");
    c_wear[3] = lf_lap_channel_index(lap, "rr tyre wear");
    if (c_time < 0 || c_dist < 0) {
        return import_fail(err, err_cap, "lap import needs the time and distance channels");
    }
    int total = lap->sample_count;
    if (total <= 0) return import_fail(err, err_cap, "lap import has no samples");
    int stride = (total + TRACE_SAMPLES - 1) / TRACE_SAMPLES;
    if (stride < 1) stride = 1;
    int partial = !(import_lap_channel(lap, c_dist, 0) <= 1.0f);
    char drv[NAME_LEN * 6 + 1];
    const char *driver = lf_pairs_get(&lap->metadata.pairs, "DRIVER");
    esc_json(drv, sizeof drv, driver ? driver : "");
    const char *conv = "\"velocity mph->m/s (1/2.2371365)\",\"accel g->m/s2 (x9.8)\","
                       "\"yaw deg->rad\",\"throttle/brake 0..100->0..1\","
                       "\"gear float->int\",\"tyre wear->tread 0..1\"";
    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.trace/1\",\"source\":\"legacy\",\"open\":false,\"format\":\"lap\",\"partial\":%s,"
        "\"gen\":%u,\"rev\":0,\"which\":\"%c\",\"slot\":-1,\"driver\":\"%s\",\"car\":\"\","
        "\"ai\":0,\"team\":0,\"lap\":0,\"lap_time\":%.3f,\"start_sim\":%.3f,\"lap_len\":0.0,"
        "\"count\":%d,\"flags\":0,\"track\":\"\",\"file\":{\"bytes\":%lu},"
        "\"lid\":{\"gen\":%u,\"rev\":0,\"slot\":-1,\"lap\":0,\"src\":\"import\",\"serial\":%u},"
        "\"formulas\":\"PCODE_SEMANTICS_2026-10-09 (parallel C): distance/velocity DM#8, "
        "delta DM#14, tyre/damper FileReading#0 (not carried by f1c.trace/1)\","
        "\"conversions\":[%s],\"samples\":[",
        partial ? "true" : "false", capture_generation, which, drv,
        (double)lap->field_float_08[0], (double)import_lap_channel(lap, c_time, 0),
        (total + stride - 1) / stride, (unsigned long)file_bytes,
        capture_generation, g_import_serial[which - 'A'], conv);
    uint32_t seen = 0;
    int emitted = 0, truncated = 0;
    for (int k = 0; k < total; k += stride) {
        trace_sample_t s;
        memset(&s, 0, sizeof s);
        float v;
        s.sim_t = import_lap_channel(lap, c_time, k);
        v = import_lap_channel(lap, c_dist, k);
        if (trace_channel_ok(v, 1.0e6f)) { s.dist = v; s.flags |= TRF_DIST; }
        if (c_vel >= 0) {
            v = import_lap_channel(lap, c_vel, k);
            if (trace_channel_ok(v, 500.0f)) {
                s.speed = v / (float)IMPORT_MPH_PER_MPS;
                s.flags |= TRF_SPEED;
            }
        }
        if (c_th >= 0) {
            v = import_lap_channel(lap, c_th, k) / 100.0f;
            if (v >= -0.01f && v <= 1.01f) { s.throttle = v; s.flags |= TRF_THROTTLE; }
        }
        if (c_br >= 0) {
            v = import_lap_channel(lap, c_br, k) / 100.0f;
            if (v >= -0.01f && v <= 1.01f) { s.brake = v; s.flags |= TRF_BRAKE; }
        }
        if (c_st >= 0) {
            v = import_lap_channel(lap, c_st, k);
            if (trace_channel_ok(v, 1500.0f)) { s.steer = v; s.flags |= TRF_STEER; }
        }
        if (c_rpm >= 0) {
            v = import_lap_channel(lap, c_rpm, k);
            if (v >= 0.0f && v <= 25000.0f) { s.rpm = v; s.flags |= TRF_RPM; }
        }
        if (c_gear >= 0) {
            v = import_lap_channel(lap, c_gear, k);
            if (v >= -0.5f && v <= 16.5f) { s.gear = (int)lroundf(v); s.flags |= TRF_GEAR; }
        }
        if (c_yaw >= 0) {
            v = import_lap_channel(lap, c_yaw, k);
            if (trace_channel_ok(v, 720.0f)) {
                s.yaw = v * 3.14159265358979323846f / 180.0f;
                s.flags |= TRF_YAW;
            }
        }
        if (c_lat >= 0) {
            v = import_lap_channel(lap, c_lat, k) * (float)IMPORT_ACCEL_G;
            if (trace_channel_ok(v, 100.0f)) { s.glat = v; s.flags |= TRF_GLAT; }
        }
        if (c_lon >= 0) {
            v = import_lap_channel(lap, c_lon, k) * (float)IMPORT_ACCEL_G;
            if (trace_channel_ok(v, 100.0f)) { s.glon = v; s.flags |= TRF_GLON; }
        }
        if (c_vert >= 0) {
            v = import_lap_channel(lap, c_vert, k) * (float)IMPORT_ACCEL_G;
            if (trace_channel_ok(v, 100.0f)) { s.gvert = v; s.flags |= TRF_GVERT; }
        }
        if (c_x >= 0 && c_z >= 0) {
            float tx = import_lap_channel(lap, c_x, k), tz = import_lap_channel(lap, c_z, k);
            if (trace_channel_ok(tx, 1.0e6f) && trace_channel_ok(tz, 1.0e6f)) {
                s.tx = tx; s.tz = tz; s.flags |= TRF_TRACK;
            }
        }
        if (c_wear[0] >= 0 && c_wear[1] >= 0 && c_wear[2] >= 0 && c_wear[3] >= 0) {
            int ok = 1;
            for (int w = 0; w < 4; w++) {
                v = import_lap_channel(lap, c_wear[w], k);
                if (!(v >= 0.0f && v <= 1.0001f)) { ok = 0; break; }
                s.tread[w] = v;
            }
            if (ok) s.flags |= TRF_TREAD;
        }
        /* A stride-reduced import must not imply continuity between kept
         * samples: every kept sample starts a new segment except the first. */
        if (stride > 1 && emitted > 0) s.flags |= TRF_SEGMENT;
        if (emitted < TRACE_SAMPLES) g_import_samples[which - 'A'][emitted] = s;
        p = trace_emit_sample_json(buf, bufsz, p, &s, emitted == 0);
        seen |= s.flags;
        emitted++;
        if (p >= bufsz - 256) { truncated = 1; break; }
    }
    int slot = which - 'A';
    g_import_count[slot] = emitted;
    g_import_start_sim[slot] = emitted > 0 ? g_import_samples[slot][0].sim_t : 0.0f;
    g_import_lap_time[slot] = (float)lap->field_float_08[0];
    import_copy_str(g_import_driver[slot], sizeof g_import_driver[slot], driver);
    return import_emit_finish(buf, bufsz, p, emitted, total, stride, truncated, seen);
}

/* .spt -> f1c.trace/1. Distance and speed are derived with the viewer's own
 * DM#8 formulas (there is no distance channel in an spt); every derived
 * channel is listed in "conversions". */
static int import_spt_json(char *buf, int bufsz, const lf_spt_t *spt,
                           size_t file_bytes, char which, char *err, size_t err_cap) {
    int total = (int)spt->player_records;
    if (total <= 0) return import_fail(err, err_cap, "spt import has no player records");
    int stride = (total + TRACE_SAMPLES - 1) / TRACE_SAMPLES;
    if (stride < 1) stride = 1;
    int has_gear = spt->row_stride > 54;
    int has_wear = spt->row_stride > 52;
    char drv[NAME_LEN * 6 + 1], trk[sizeof(spt->track) * 6 + 1];
    esc_json(drv, sizeof drv, spt->driver);
    esc_json(trk, sizeof trk, spt->track);
    const char *conv = "\"distance/velocity derived from x/z deltas (DM#8; mph scale cancels)\","
                       "\"accel native m/s2 (viewer g-conversion not applied)\","
                       "\"yaw rad (as stored)\",\"tyre wear->tread 0..1\"";
    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.trace/1\",\"source\":\"legacy\",\"open\":false,\"format\":\"spt\",\"partial\":true,"
        "\"gen\":%u,\"rev\":0,\"which\":\"%c\",\"slot\":-1,\"driver\":\"%s\",\"car\":\"\","
        "\"ai\":0,\"team\":0,\"lap\":0,\"lap_time\":0.000,\"start_sim\":%.3f,\"lap_len\":0.0,"
        "\"count\":%d,\"flags\":0,\"track\":\"%s\",\"file\":{\"bytes\":%lu},"
        "\"lid\":{\"gen\":%u,\"rev\":0,\"slot\":-1,\"lap\":0,\"src\":\"import\",\"serial\":%u},"
        "\"formulas\":\"PCODE_SEMANTICS_2026-10-09 (parallel C): distance/velocity DM#8, "
        "delta DM#14, tyre/damper FileReading#0 (not carried by f1c.trace/1)\","
        "\"conversions\":[%s],\"samples\":[",
        capture_generation, which, drv, (double)spt->first_elapsed_s,
        (total + stride - 1) / stride, trk, (unsigned long)file_bytes,
        capture_generation, g_import_serial[which - 'A'], conv);
    uint32_t seen = 0;
    int emitted = 0, truncated = 0;
    float prev_x = 0.0f, prev_z = 0.0f, prev_t = 0.0f, prev_v = 0.0f;
    float dist_acc = 0.0f;
    int have_prev = 0;
    for (int k = 0; k < total; k += stride) {
        const float *row = spt->rows + (size_t)k * (size_t)spt->row_stride;
        trace_sample_t s;
        memset(&s, 0, sizeof s);
        float x = row[1], z = row[3];
        s.sim_t = row[0];
        if (have_prev) {
            float dx = x - prev_x, dz = z - prev_z;
            float d = sqrtf(dx * dx + dz * dz);
            float dt = s.sim_t - prev_t;
            if (dt < 1e-7f) dt = 1e-7f;
            if (dt > 0.01f) prev_v = d / dt; /* mph factor cancels: m/s */
            dist_acc += d;
            s.speed = prev_v;
            s.flags |= TRF_SPEED;
        }
        prev_x = x; prev_z = z; prev_t = s.sim_t; have_prev = 1;
        s.dist = dist_acc;
        s.flags |= TRF_DIST;
        {
            float v = row[5];        /* lat, native m/s2 */
            if (trace_channel_ok(v, 100.0f)) { s.glat = v; s.flags |= TRF_GLAT; }
            v = row[7];              /* long */
            if (trace_channel_ok(v, 100.0f)) { s.glon = v; s.flags |= TRF_GLON; }
            v = row[6];              /* vert */
            if (trace_channel_ok(v, 100.0f)) { s.gvert = v; s.flags |= TRF_GVERT; }
        }
        {
            float v = row[11] / 1.0f; /* filtered throttle (0..1) */
            if (v >= -0.01f && v <= 1.01f) { s.throttle = v; s.flags |= TRF_THROTTLE; }
            v = row[13];              /* filtered brake */
            if (v >= -0.01f && v <= 1.01f) { s.brake = v; s.flags |= TRF_BRAKE; }
            v = row[15];              /* filtered steering */
            if (trace_channel_ok(v, 1.5f)) { s.steer = v; s.flags |= TRF_STEER; }
        }
        {
            float v = row[8]; /* engine RPM (writer already applied the factor) */
            if (v >= 0.0f && v <= 25000.0f) { s.rpm = v; s.flags |= TRF_RPM; }
            v = row[4];       /* yaw rad */
            if (trace_channel_ok(v, 10.0f)) { s.yaw = v; s.flags |= TRF_YAW; }
            if (has_gear) {
                v = row[54];
                if (v >= -0.5f && v <= 16.5f) { s.gear = (int)lroundf(v); s.flags |= TRF_GEAR; }
            }
            if (trace_channel_ok(x, 1.0e6f) && trace_channel_ok(z, 1.0e6f)) {
                s.tx = x; s.tz = z; s.flags |= TRF_TRACK;
            }
            if (has_wear) {
                int ok = 1;
                const int wi[4] = { 25, 34, 43, 52 };
                for (int w = 0; w < 4; w++) {
                    float wv = row[wi[w]];
                    if (!(wv >= 0.0f && wv <= 1.0001f)) { ok = 0; break; }
                    s.tread[w] = wv;
                }
                if (ok) s.flags |= TRF_TREAD;
            }
        }
        if (stride > 1 && emitted > 0) s.flags |= TRF_SEGMENT;
        if (emitted < TRACE_SAMPLES) g_import_samples[which - 'A'][emitted] = s;
        p = trace_emit_sample_json(buf, bufsz, p, &s, emitted == 0);
        seen |= s.flags;
        emitted++;
        if (p >= bufsz - 256) { truncated = 1; break; }
    }
    int slot = which - 'A';
    g_import_count[slot] = emitted;
    g_import_start_sim[slot] = emitted > 0 ? g_import_samples[slot][0].sim_t : 0.0f;
    g_import_lap_time[slot] = 0.0f;
    import_copy_str(g_import_driver[slot], sizeof g_import_driver[slot], spt->driver);
    return import_emit_finish(buf, bufsz, p, emitted, total, stride, truncated, seen);
}

/* POST /trace/import/legacy dispatcher: sniff, decode, convert, cache. */
static int build_import_json(int slot, const unsigned char *body, size_t len,
                             char *err, size_t err_cap) {
    if (slot < 0 || slot > 1) return -1;
    /* Single HTTP writer. Keep the previous import intact if conversion or
     * bounded JSON formatting fails after decoding succeeded. */
    static char previous_json[sizeof g_import_json[0]];
    static trace_sample_t previous_samples[TRACE_SAMPLES];
    char previous_driver[NAME_LEN];
    int previous_len=g_import_len[slot], previous_count=g_import_count[slot];
    float previous_start=g_import_start_sim[slot], previous_time=g_import_lap_time[slot];
    memcpy(previous_json,g_import_json[slot],sizeof previous_json);
    memcpy(previous_samples,g_import_samples[slot],sizeof previous_samples);
    memcpy(previous_driver,g_import_driver[slot],sizeof previous_driver);
    char *buf = g_import_json[slot];
    int bufsz = (int)sizeof(g_import_json[0]);
    int p = -1;
    /* Fix 2: the per-side serial makes a replacement detectable; it is only
     * consumed by a successful import. */
    g_import_serial[slot]++;
    if (len >= 9 && !memcmp(body, "CHECKSUM=", 9)) {
        lf_lap_t lap;
        if (lf_decode_lap_mem(body, len, &lap, err, err_cap) != 0) { g_import_serial[slot]--; return -1; }
        p = import_lap_json(buf, bufsz, &lap, len, (char)('A' + slot), err, err_cap);
        lf_lap_free(&lap);
    } else {
        lf_spt_t spt;
        if (lf_decode_spt_mem(body, len, &spt, err, err_cap) != 0) { g_import_serial[slot]--; return -1; }
        p = import_spt_json(buf, bufsz, &spt, len, (char)('A' + slot), err, err_cap);
        lf_spt_free(&spt);
    }
    if (p < 0 || format_failed) {
        g_import_serial[slot]--;
        memcpy(g_import_json[slot],previous_json,sizeof previous_json);
        memcpy(g_import_samples[slot],previous_samples,sizeof previous_samples);
        memcpy(g_import_driver[slot],previous_driver,sizeof previous_driver);
        g_import_len[slot]=previous_len; g_import_count[slot]=previous_count;
        g_import_start_sim[slot]=previous_start; g_import_lap_time[slot]=previous_time;
        if (format_failed) { format_failed=0; import_fail(err,err_cap,"converted import exceeds response capacity"); }
        return -1;
    }
    g_import_len[slot] = p;
    return p;
}

/* Same identity/miss semantics as build_trace_data_json_id (fix 2). */
static int build_trace_data_csv_id(char *buf, int bufsz, int which, int lap,
                                   long gen, long rev) {
    EnterCriticalSection(&g_telemetry_cs);
    int letter = -1;
    trace_car_t *t = trace_pick(which, &letter);
    if (!t) { LeaveCriticalSection(&g_telemetry_cs); return -1; }
    if (gen >= 0 && (unsigned long)gen != capture_generation) {
        LeaveCriticalSection(&g_telemetry_cs); return -3;
    }
    if (rev >= 0 && (unsigned long)rev != t->revision) {
        LeaveCriticalSection(&g_telemetry_cs); return -4;
    }
    int idx = trace_find_lap(t, lap);
    if (idx < 0) {
        int evicted = t->first_lap_seen >= 0 && lap >= t->first_lap_seen
                      && lap <= t->last_lap_seen;
        LeaveCriticalSection(&g_telemetry_cs);
        return evicted ? -2 : -1;
    }
    const trace_sample_t *s = t->laps[idx];
    int count = t->lap_samples[idx];
    char track[sizeof(g_telemetry.track_name)*6+1];
    esc_json(track,sizeof track,g_telemetry.track_name);
    char escn[NAME_LEN + 1], escc[NAME_LEN + 1];
    int n = 0;
    while (t->driver_name[n] && n < NAME_LEN - 1) {
        char ch = t->driver_name[n];
        escn[n] = (ch == ',' || ch == '\n' || ch == '\r') ? ' ' : ch;
        n++;
    }
    escn[n] = 0;
    n = 0;
    while (t->car_name[n] && n < NAME_LEN - 1) {
        char ch = t->car_name[n];
        escc[n] = (ch == ',' || ch == '\n' || ch == '\r') ? ' ' : ch;
        n++;
    }
    escc[n] = 0;
    char which_text[2] = { letter >= 0 ? (char)('A' + letter) : '-', 0 };
    int p = _snprintf(buf, bufsz,
        "# f1c.trace/1 gen=%u rev=%u which=%s slot=%d src=live driver=%s car=%s ai=%d team=%u lap=%d lap_time=%.3f start_sim=%.3f lap_len=%.1f flags=%d\n"
        "sim_t,dist_m,speed_mps,throttle,brake,steer,glat_mps2,glon_mps2,gvert_mps2,yaw_rad,yaw_rate_rps,tread_fl,tread_fr,tread_rl,tread_rr,flags,rpm,gear,tx,tz\n",
        capture_generation, t->revision, which_text, t->slot, escn, escc, t->is_ai,
        t->team_id, t->lap_num[idx], t->lap_time[idx], t->lap_start_sim[idx],
        g_telemetry.lap_len, t->lap_valid[idx]);
    for (int i = 0; i < count; i++) {
        char stb[24], glb[24], gob[24], gvb[24], yb[24], yrb[24], rb[24], gb[8];
        char txb[24], tzb[24];
        if (s[i].flags & TRF_STEER) snprintf(stb, sizeof stb, "%.4f", s[i].steer);
        else stb[0] = 0;
        if (s[i].flags & TRF_GLAT) snprintf(glb, sizeof glb, "%.2f", s[i].glat);
        else glb[0] = 0;
        if (s[i].flags & TRF_GLON) snprintf(gob, sizeof gob, "%.2f", s[i].glon);
        else gob[0] = 0;
        if (s[i].flags & TRF_GVERT) snprintf(gvb, sizeof gvb, "%.2f", s[i].gvert);
        else gvb[0] = 0;
        if (s[i].flags & TRF_YAW) snprintf(yb, sizeof yb, "%.3f", s[i].yaw);
        else yb[0] = 0;
        if (s[i].flags & TRF_YAWRATE) snprintf(yrb, sizeof yrb, "%.3f", s[i].yaw_rate);
        else yrb[0] = 0;
        if (s[i].flags & TRF_RPM) snprintf(rb, sizeof rb, "%.1f", s[i].rpm);
        else rb[0] = 0;
        if (s[i].flags & TRF_GEAR) snprintf(gb, sizeof gb, "%d", s[i].gear);
        else gb[0] = 0;
        if (s[i].flags & TRF_TRACK) {
            snprintf(txb, sizeof txb, "%.1f", s[i].tx);
            snprintf(tzb, sizeof tzb, "%.1f", s[i].tz);
        } else {
            txb[0] = 0; tzb[0] = 0;
        }
        p += _snprintf(buf + p, bufsz - p,
            "%.3f,%.1f,%.2f,%.2f,%.2f,%s,%s,%s,%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%u,%s,%s,%s,%s\n",
            s[i].sim_t, s[i].dist, s[i].speed, s[i].throttle, s[i].brake,
            stb, glb, gob, gvb, yb, yrb,
            s[i].tread[0], s[i].tread[1], s[i].tread[2], s[i].tread[3], s[i].flags,
            rb, gb, txb, tzb);
        if (p >= bufsz - 256) break;
    }
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* Compatibility wrapper (no identity pin); also used by the C test builders. */
static int build_trace_data_csv(char *buf, int bufsz, int which, int lap) {
    return build_trace_data_csv_id(buf, bufsz, which, lap, -1, -1);
}

/* Distance-aligned comparison. Returns -1 when a requested lap is not
 * recorded (live) or no import is cached for a requested import side, or the
 * distance frame is unknown. Fix 2 (api_version 2): either side may be the
 * cached legacy import (src 0 = live ring, 1 = import cache) and every side
 * carries its stable lid. */
typedef struct {
    int  is_import;
    const trace_sample_t *samples;
    int  count;
    float start_sim;
    float lap_time;
    int  flags;
    int  slot;
    int  ai;
    uint32_t team;
    int  lap_num;
    unsigned rev;
    unsigned serial;     /* import lid serial (0 for live) */
    char driver[NAME_LEN];
    char car[NAME_LEN];
} trace_cmp_side_t;

static int trace_cmp_live(trace_cmp_side_t *o, int which, int lap) {
    trace_car_t *t = trace_pick(which, NULL);
    if (!t) return -1;
    int idx = trace_find_lap(t, lap);
    if (idx < 0) return -1;
    memset(o, 0, sizeof *o);
    o->samples = t->laps[idx];
    o->count = t->lap_samples[idx];
    o->start_sim = t->lap_start_sim[idx];
    o->lap_time = t->lap_time[idx];
    o->flags = t->lap_valid[idx];
    o->slot = t->slot; o->ai = t->is_ai; o->team = t->team_id;
    o->lap_num = t->lap_num[idx];
    o->rev = t->revision;
    snprintf(o->driver, sizeof o->driver, "%s", t->driver_name);
    snprintf(o->car, sizeof o->car, "%s", t->car_name);
    return 0;
}

static int trace_cmp_import(trace_cmp_side_t *o, int which) {
    if (which < 0 || which >= 2 || g_import_count[which] <= 0) return -1;
    memset(o, 0, sizeof *o);
    o->is_import = 1;
    o->samples = g_import_samples[which];
    o->count = g_import_count[which];
    o->start_sim = g_import_start_sim[which];
    o->lap_time = g_import_lap_time[which];
    o->slot = -1;
    o->serial = g_import_serial[which];
    snprintf(o->driver, sizeof o->driver, "%s", g_import_driver[which]);
    return 0;
}

/* Emit one compare side up to and including its break_dist opener. */
static int trace_cmp_emit_open(char *buf, int bufsz, int p, char key, char value,
                               const trace_cmp_side_t *o, unsigned gen,
                               const trace_grid_t *grid, int n) {
    char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
    esc_json(escn, sizeof escn, o->driver);
    esc_json(escc, sizeof escc, o->car);
    p += _snprintf(buf + p, bufsz - p,
        "\"%c\":{\"which\":\"%c\",\"slot\":%d,\"driver\":\"%s\",\"car\":\"%s\",\"ai\":%d,"
        "\"team\":%u,\"lap\":%d,\"lap_time\":%.3f,\"start_sim\":%.3f,\"flags\":%d,"
        "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"%s\"",
        key, value, o->slot, escn, escc, o->ai, o->team,
        o->lap_num, o->lap_time, o->start_sim, o->flags,
        gen, o->rev, o->slot, o->lap_num, o->is_import ? "import" : "live");
    if (o->is_import)
        p += _snprintf(buf + p, bufsz - p, ",\"serial\":%u", o->serial);
    p += _snprintf(buf + p, bufsz - p, "},\"avail\":%d,\"breaks\":%d,\"break_dist\":[",
                   trace_grid_avail(grid, n), grid->breaks);
    return p;
}

static int build_trace_compare_json(char *buf, int bufsz,
                                    int which_a, int lap_a, int src_a,
                                    int which_b, int lap_b, int src_b, float step) {
    EnterCriticalSection(&g_telemetry_cs);
    trace_cmp_side_t sa, sb;
    int miss = src_a ? trace_cmp_import(&sa, which_a)
                     : trace_cmp_live(&sa, which_a, lap_a);
    if (!miss)
        miss = src_b ? trace_cmp_import(&sb, which_b)
                     : trace_cmp_live(&sb, which_b, lap_b);
    float lap_len = g_telemetry.lap_len;
    if (!miss && !(lap_len > 0.0f)) {
        /* Import-only or pre-track compare: derive the distance frame from
         * the recorded distances instead of refusing. */
        float maxd = 0.0f;
        if (sa.count > 0 && sa.samples[sa.count - 1].dist > maxd)
            maxd = sa.samples[sa.count - 1].dist;
        if (sb.count > 0 && sb.samples[sb.count - 1].dist > maxd)
            maxd = sb.samples[sb.count - 1].dist;
        lap_len = maxd;
    }
    if (miss || !(lap_len > 0.0f)) {
        LeaveCriticalSection(&g_telemetry_cs);
        return miss ? -1 : -2;   /* -2: distance frame unknown */
    }
    if (step < 1.0f) step = 1.0f;
    if (step > 50.0f) step = 50.0f;
    if (lap_len / step > (float)(TRACE_GRID_MAX - 1))
        step = lap_len / (float)(TRACE_GRID_MAX - 1);
    int n = (int)(lap_len / step) + 1;
    if (n > TRACE_GRID_MAX) n = TRACE_GRID_MAX;
    trace_grid_fill_samples(sa.samples, sa.count, sa.start_sim, step, n, &g_grid_a);
    trace_grid_fill_samples(sb.samples, sb.count, sb.start_sim, step, n, &g_grid_b);

    char track_escaped[sizeof(g_telemetry.track_name) * 6 + 1];
    esc_json(track_escaped, sizeof track_escaped, g_telemetry.track_name);

    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.compare/1\",\"gen\":%u,\"lap_len\":%.1f,\"step\":%.2f,\"n\":%d,"
        "\"conditions\":{\"ty\":%d,\"ph\":%d,\"trk\":\"%s\",\"at\":%.1f,\"trkt\":%.1f,"
        "\"onw\":%.3f,\"offw\":%.3f,\"ai\":%d,\"sc1\":%.1f,\"sc2\":%.1f,\"lhlim\":%d},",
        capture_generation, lap_len, step, n,
        g_telemetry.session_type, g_telemetry.race_phase, track_escaped,
        g_telemetry.ambient_temp, g_telemetry.track_temp,
        g_telemetry.on_wetness, g_telemetry.off_wetness, g_telemetry.ai_difficulty,
        g_telemetry.sec1, g_telemetry.sec2, MAX_LAP_HISTORY);
    p = trace_cmp_emit_open(buf, bufsz, p, 'a', 'A', &sa, capture_generation, &g_grid_a, n);
    for (int i = 0; i < g_grid_a.break_n; i++)
        p += _snprintf(buf + p, bufsz - p, "%s%.1f", i ? "," : "", g_grid_a.break_dist[i]);
    p += _snprintf(buf + p, bufsz - p, "]},");
    p = trace_cmp_emit_open(buf, bufsz, p, 'b', 'B', &sb, capture_generation, &g_grid_b, n);
    for (int i = 0; i < g_grid_b.break_n; i++)
        p += _snprintf(buf + p, bufsz - p, "%s%.1f", i ? "," : "", g_grid_b.break_dist[i]);
    p += _snprintf(buf + p, bufsz - p, "]},\"dist\":[");
    for (int i = 0; i < n; i++)
        p += _snprintf(buf + p, bufsz - p, "%s%.1f", i ? "," : "", i * step);
    p += _snprintf(buf + p, bufsz - p, "],\"a_ch\":{");
    p = trace_emit_channels(buf, bufsz, p, &g_grid_a, n, 1);
    p += _snprintf(buf + p, bufsz - p, "},\"delta\":[");
    for (int i = 0; i < n; i++) {
        int ea = (g_grid_a.have[i] & TGF_ELAPSED) ? 1 : 0;
        int eb = (g_grid_b.have[i] & TGF_ELAPSED) ? 1 : 0;
        if (ea && eb)
            p += _snprintf(buf + p, bufsz - p, "%s%.3f", i ? "," : "",
                           g_grid_b.elapsed[i] - g_grid_a.elapsed[i]);
        else
            p += _snprintf(buf + p, bufsz - p, "%snull", i ? "," : "");
        if (p >= bufsz - 64) break;
    }
    p += _snprintf(buf + p, bufsz - p, "],\"b_ch\":{");
    p = trace_emit_channels(buf, bufsz, p, &g_grid_b, n, 1);
    p += _snprintf(buf + p, bufsz - p, "}}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

static int build_incidents_json(char *buf, int bufsz) {
    EnterCriticalSection(&g_telemetry_cs);
    int p = _snprintf(buf, bufsz, "{\"gen\":%u,\"ev\":[", capture_generation);
    int n = g_incident_count;
    int start = (g_incident_head - n + MAX_INCIDENTS) % MAX_INCIDENTS;
    int first = 1;
    for (int k = 0; k < n; k++) {
        int idx = (start + k) % MAX_INCIDENTS;
        incident_t *e = &g_incidents[idx];
        char esc[NAME_LEN * 6 + 1];
        esc_json(esc, sizeof(esc), (e->car_id >= 0) ? g_telemetry.cars[e->car_id].name : "SESSION");
        if (!first) bounded_char(buf,bufsz,&p,',');
        first = 0;
        p += _snprintf(buf + p, bufsz - p,
            "{\"tk\":%lu,\"id\":%d,\"n\":\"%s\",\"ty\":\"%s\",\"lp\":%d,\"a\":%d,\"b\":%d}",
            e->tick, e->car_id, esc, e->type, e->lap, e->a, e->b);
        if (p >= bufsz - 128) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* ========================================================================
 * LOGGING
 * ======================================================================== */
/* rc.17: every log line also feeds the consent-gated usage report. */
#include "web_telemetry_usage.h"
/* 1.0.1: the log is capped. Past LOG_ROTATE_BYTES it moves to
 * web_telemetry_log.old.txt (replacing the previous one), so at most two
 * files of about 2 MB exist (1.0.0 grew one file forever). */
#define LOG_ROTATE_BYTES (2u * 1024u * 1024u)
static int g_ar_max_mb = 3000;   /* session archive cap, [Mod] MaxStorageMB */
static void log_rotate_maybe(void) {
    static volatile LONG calls;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    char old[MAX_PATH];
    if ((InterlockedIncrement(&calls) & 63) != 1 || !g_log_path[0]) return;
    if (!GetFileAttributesExA(g_log_path, GetFileExInfoStandard, &fad) ||
        (!fad.nFileSizeHigh && fad.nFileSizeLow < LOG_ROTATE_BYTES)) return;
    size_t n = strlen(g_log_path);
    if (n < 4 || n + 5 >= sizeof old) return;
    memcpy(old, g_log_path, n - 4);
    strcpy(old + n - 4, ".old.txt");
    MoveFileExA(g_log_path, old, MOVEFILE_REPLACE_EXISTING);
}
static void log_w(const char *s) {
    log_rotate_maybe();
    FILE *f = fopen(g_log_path, "a");
    if (f) { fprintf(f, "[%lu] %s\n", GetTickCount(), s); fclose(f); }
    wtu_tap(s);
}
static void log_f(const char *fmt, ...) {
    char line[512];
    va_list ap; va_start(ap, fmt); _vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
    line[sizeof line - 1] = 0;
    log_rotate_maybe();
    FILE *f = fopen(g_log_path, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
    wtu_tap(line);
}

/* ========================================================================
 * SETTINGS — web_telemetry.ini beside the DLL (MOD_STANDARD R10)
 * ========================================================================
 * [Mod] Port selects the HTTP listen port, range 1024-65535. A missing file
 * or key, or an invalid value, keeps the compiled default (8080 in the
 * shipped build) and the reason is logged. Read once, on the component init
 * path, before the HTTP thread starts. */

static int settings_module_ini_path(char *out, size_t size) {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(telemetry_module ? telemetry_module : NULL,
                                 path, sizeof path);
    if (!n || n >= sizeof path) return 0;
    char *slash = strrchr(path, '\\');
    if (!slash) return 0;
    slash[1] = 0;
    return snprintf(out, size, "%sweb_telemetry.ini", path) < (int)size;
}

/* Strict decimal parse: full-token digits, optional surrounding spaces and a
 * trailing ; or # comment. Returns -1 when the value is absent or invalid. */
static int settings_parse_port_value(const char *value) {
    char *end = NULL;
    long port;
    while (*value == ' ' || *value == '\t') value++;
    if (!*value) return -1;
    port = strtol(value, &end, 10);
    if (end == value) return -1;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end && *end != ';' && *end != '#') return -1;
    if (port < HTTP_PORT_MIN || port > HTTP_PORT_MAX) return -1;
    return (int)port;
}

static void settings_read_ini(const char *ini_path) {
    char line[256];
    int in_mod = 0, found = 0, section_seen = 0;
    g_bind_cfg[0] = 0;   /* re-read on every load: a missing key means no BindAddress */
    FILE *f = (ini_path && ini_path[0]) ? fopen(ini_path, "r") : NULL;
    if (!f) {
        g_configured_port = HTTP_PORT;
        log_f("settings: no web_telemetry.ini beside the DLL; [Mod] Port default %d", g_configured_port);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *eq, *key, *value;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            char *close = strchr(p, ']');
            section_seen = 1;
            in_mod = close && close - p == 4 && !strnicmp(p + 1, "Mod", 3);
            continue;
        }
        if (!in_mod || *p == ';' || *p == '#' || *p == '\r' || *p == '\n') continue;
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        key = p;
        value = eq + 1;
        while (key[0] == ' ' || key[0] == '\t') key++;
        for (char *kend = key + strlen(key); kend > key && (kend[-1] == ' ' || kend[-1] == '\t'); ) *--kend = 0;
        if (!strnicmp(key, "remote", 6) && !key[6]) {
            while (*value == ' ' || *value == '	') value++;
            g_remote_access = (value[0] == '1');
            log_f("settings: [Mod] Remote=%d", g_remote_access);
            continue;
        }
        if (!strnicmp(key, "bindaddress", 11) && !key[11]) {
            /* M1 (2026-10-13): the one address Remote=1 may listen on; http_bind_resolve checks it. */
            size_t k = 0;
            while (*value == ' ' || *value == '\t') value++;
            while (k < sizeof g_bind_cfg - 1 && value[k] && value[k] != ' ' && value[k] != '\t' &&
                   value[k] != '\r' && value[k] != '\n' && value[k] != ';' && value[k] != '#') {
                g_bind_cfg[k] = value[k];
                k++;
            }
            g_bind_cfg[k] = 0;
            log_f("settings: [Mod] BindAddress=%s", g_bind_cfg[0] ? g_bind_cfg : "(not set)");
            continue;
        }
        if (!strnicmp(key, "maxstoragemb", 12) && !key[12]) {
            /* 1.0.1 session archive cap in MB; 0 = no cap. Invalid keeps the default. */
            char *end = NULL;
            long mb = strtol(value, &end, 10);
            if (end != value && mb >= 0 && mb <= 1000000) g_ar_max_mb = (int)mb;
            log_f("settings: [Mod] MaxStorageMB=%d", g_ar_max_mb);
            continue;
        }
        if (strnicmp(key, "port", 4) != 0 || key[4] || found) continue;
        int port = settings_parse_port_value(value);
        if (port < 0) {
            g_configured_port = HTTP_PORT;
            log_f("settings: invalid [Mod] Port in %s; using %d", ini_path, g_configured_port);
        } else { g_configured_port = port; log_f("settings: [Mod] Port=%d from %s", port, ini_path); }
        found = 1;
    }
    fclose(f);
    if (!found && section_seen) {
        g_configured_port = HTTP_PORT;
        log_f("settings: [Mod] Port missing in %s; using default %d", ini_path, g_configured_port);
    }
}

static void telemetry_settings_load(const char *ini_override) {
    char path[MAX_PATH];
    const char *use = NULL;
    if (g_settings_loaded) return;
    g_settings_loaded = 1;
    if (ini_override && ini_override[0]) use = ini_override;
    else if (settings_module_ini_path(path, sizeof path)) use = path;
    settings_read_ini(use);
    dcam_settings_read(use);
}

/* ========================================================================
 * RACE CONTROL ACTIONS (write-side — UNTESTED in this DLL as of authoring;
 * mechanism ported verbatim from mods/red_flag and mods/ai_pit_retire, which
 * DO have confirmed in-game logs for the same technique. Player car (slot
 * PLAYER_INDEX) is always excluded — these actions only ever touch AI cars.)
 * ======================================================================== */

static void damage_update(int i, car_telemetry_t *c, volatile uint8_t *veh) {
    /* Guard the record pointers: a null ptr means the record isn't live and the
     * flag byte would be meaningless (the game's own test requires ptr != 0). */
    unsigned fwp = (unsigned)RI(veh, OFF_FRONTWING_PTR);
    unsigned rwp = (unsigned)RI(veh, OFF_REARWING_PTR);
    c->fw_flag = fwp ? (int)RB(veh, OFF_FRONTWING_FLAG) : -1;
    c->rw_flag = rwp ? (int)RB(veh, OFF_REARWING_FLAG)  : -1;

    /* A DETACH is the 1 -> 0 EDGE, not the mere sight of a 0, and it has to be
     * an edge that happens during green-flag running. Two ways this armed on
     * garbage before:
     *   - injecting mid-race instantly "detected" wings on cars that had been
     *     wrecked for laps (five bogus bulletins in one tick);
     *   - a session RESTART tears these records down and rebuilds them, and the
     *     flags read 0 in the middle of that. Barrichello was reported DOOMED on
     *     the formation lap of a fresh race while his wings read intact.
     * So: require having seen the record intact first, ignore the opening
     * seconds of a session entirely, and require the 0 to persist rather than
     * trusting a single sample. */
    /* Believe nothing about the wing records in the opening seconds of a session:
     * a restart tears them down and rebuilds them, and they read 0 mid-rebuild. */
    int settled = (g_telemetry.session_time > DAMAGE_ARM_AFTER_SEC);
    if (!settled) {
        /* leave every latch untouched */
    } else if (!g_wing_baseline[i]) {
        /* FIRST settled look at this car. Whatever the records say NOW is the
         * pre-existing state, not an event: the DLL can be injected (or
         * hot-swapped) into a race already in progress, where a car may have lost
         * a wing laps ago. Record it as lost so the UI tells the truth, but emit
         * NO bulletin — we did not witness it happen.
         * Getting this wrong is why Webber showed "bodywork" while visibly
         * missing his rear wing: the previous rule required seeing the record
         * intact first, which for an already-damaged car never happens. */
        g_wing_baseline[i] = 1;
        if (c->fw_flag == 0) { g_fw_lost[i] = 1; g_fw_pre[i] = 1; }
        else if (c->fw_flag == 1) g_fw_seen_ok[i] = 1;
        if (c->rw_flag == 0) { g_rw_lost[i] = 1; g_rw_pre[i] = 1; }
        else if (c->rw_flag == 1) g_rw_seen_ok[i] = 1;
    } else {
        /* Steady state: a detach is the 1 -> 0 edge, debounced, and THAT is an
         * event worth announcing. */
        if (c->fw_flag == 1) { g_fw_seen_ok[i] = 1; g_fw_zero_ticks[i] = 0; }
        else if (c->fw_flag == 0 && g_fw_zero_ticks[i] < 1000) g_fw_zero_ticks[i]++;
        if (c->rw_flag == 1) { g_rw_seen_ok[i] = 1; g_rw_zero_ticks[i] = 0; }
        else if (c->rw_flag == 0 && g_rw_zero_ticks[i] < 1000) g_rw_zero_ticks[i]++;

        /* REPAIRED: the record goes back to 1 after a pit stop (corpus: observed
         * 1->0 on detach, 0->1 after the stop, car rejoined). The latch was
         * write-once, so a repaired car showed "front wing detached" for the rest
         * of the race — Sarrazin pitted, was fixed, and the dashboard never
         * noticed. A front wing is repairable by design, so this is the normal
         * end of the story, not an edge case. */
        if (g_fw_lost[i] && c->fw_flag == 1 && !c->retired && c->finish_state == 0) {
            g_fw_lost[i] = 0;
            g_fw_pre[i] = 0;
            incident_push("FIXEDWING", i, c->lap_number, 0, 1);
        }
        /* Rear wing can also read 1 again, but the game resets it that way once a
         * retirement COMPLETES. Only treat it as a repair while the car is still
         * classified as running, so a genuine terminal case is never erased. */
        if (g_rw_lost[i] && c->rw_flag == 1 && !c->retired && c->finish_state == 0
            && c->pitting < 2) {
            g_rw_lost[i] = 0;
            g_rw_pre[i] = 0;
            incident_push("FIXEDWING", i, c->lap_number, 1, 1);
        }

        if (g_fw_seen_ok[i] && !g_fw_lost[i]
            && g_fw_zero_ticks[i] >= DAMAGE_CONFIRM_TICKS) {
            g_fw_lost[i] = 1;
            incident_push("FRONTWING", i, c->lap_number, 0, 1);
        }
        if (g_rw_seen_ok[i] && !g_rw_lost[i]
            && g_rw_zero_ticks[i] >= DAMAGE_CONFIRM_TICKS) {
            g_rw_lost[i] = 1;
            /* terminal: the blocking condition will retire this car at the box */
            incident_push("REARWING", i, c->lap_number, 0, 1);
        }
    }
    c->dmg_preexisting = (g_fw_pre[i] ? 1 : 0) | (g_rw_pre[i] ? 2 : 0);
    c->fw_lost = g_fw_lost[i];
    c->rw_lost = g_rw_lost[i];

    /* Predictive: ONLY a detached rear wing means a guaranteed retirement, because
     * PitStopBlockingCondition_0042aee0 tests only that record. The old version also
     * OR'd in the 0x4574 "seized" flag, which is not a seize flag at all (see
     * OFF_UNKNOWN_4574) -- that is what put DOOMED / WILL RETIRE on healthy cars.
     * Also requires the record pointer to be readable: rw_flag == -1 means we could
     * not read it, which must never be mistaken for "detached". */
    /* rc.17: only a car ON TRACK (pitting 0) can have a dead engine. Live
     * 2026-10-12 Warm-up: every car parked in its garage reads pitting 4,
     * rpm 0, angvel 0 and was flagged ENGINE / doomed (owner: "game shows that
     * I am dead/retired"). A parked car is not a blown engine. */
    c->eng_dead = (g_telemetry.race_active && c->position > 0 && c->pitting == 0
                   && c->finish_state != 1 && c->rpm <= 0.5f
                   && c->eng_angvel > -0.5f && c->eng_angvel < 0.5f) ? 1 : 0;
    c->doomed = ((g_rw_lost[i] || c->car_conditions >= 2 || c->eng_dead)
                 && !c->retired && c->finish_state == 0) ? 1 : 0;

    /* WHY it is doomed. "DOOMED" with no cause is useless on screen — these are
     * three different stories: a car driving to the box with a detached rear
     * wing, a car stopped with a wheel off, and a car whose engine has died.
     * Precedence is most-immediate-cause first. carCond 2 = wheel loss (FACT). */
    c->doom_cause = DOOM_NONE;
    if (c->doomed) {
        if (c->eng_dead)                 c->doom_cause = DOOM_ENGINE;
        else if (c->car_conditions >= 2) c->doom_cause = DOOM_WHEEL;
        else if (g_rw_lost[i])           c->doom_cause = DOOM_REARWING;
    }

    c->raw_4574 = (int)RB(veh, OFF_UNKNOWN_4574);

    /* Engine actually stopped. VERIFIED live 2026-07-25 (Hakkinen, smoking engine):
     * a dead engine reads rpm 0 AND EngineAngVel 0 while a healthy car has angvel
     * tracking rpm (Ralf Schumacher: 1803.7 vs 1804). Paired with ReasonCode==1
     * (Engine) this is the real engine-destruction signal -- the transient 0x4574
     * flag did NOT fire for it (200Hz poller caught zero transitions).
     * NOTE: do NOT gate this on activeFlag -- a car whose engine dies gets
     * DEACTIVATED, so requiring active excluded the exact case it should catch
     * (Hakkinen read active=0 with a dead engine). Gate on "hasn't finished
     * normally" instead. */
    /* Gated on the session actually being live: OUT of a session every car reads
     * rpm 0 / angvel 0, which is a parked car, not a blown engine. Without this gate
     * the whole grid showed ENGINE STOPPED in the garage and in the menus. */


    /* Tier, most severe wins. car_conditions 2 == wheel loss (FACT, name_index). */
    if (c->eng_dead)                 c->dmg_tier = 5;   /* engine stopped - terminal */
    else if (c->car_conditions >= 2) c->dmg_tier = 4;   /* wheel - terminal */
    else if (g_rw_lost[i])           c->dmg_tier = 3;   /* rear wing - terminal */
    else if (g_fw_lost[i])           c->dmg_tier = 2;   /* front wing - repairable */
    else if (c->car_conditions == 1) c->dmg_tier = 1;   /* bodywork - cosmetic */
    else                             c->dmg_tier = 0;
}

/* ---- PIT STOP LOG ----------------------------------------------------------
 * The game records only three stop DURATIONS per car (pt0/p1/p2) and nothing
 * about what was done or when. Everything else has to be assembled by watching
 * the car across the stop: snapshot on entry, snapshot on exit, diff.
 *
 * `pittingStatus` (+0x3450) is the state machine: 0 none, 1 requested, 2 in the
 * pit lane, 3 stopped in the box, 4 exiting. Entry is >= 2, exit is the fall
 * back below 2.
 *
 * The diff yields: lap in/out, duration from the session clock, tire compound
 * before/after (so a CHANGE is visible, not just the current tire), fuel
 * before/after (a rise means fuel went in), and which wings were replaced --
 * caught by a wing record flipping back to 1 while the car is in the box.
 */
#define MAX_PIT_EVENTS 128

/* pittingStatus enum, CONFIRMED FACT (findings.jsonl, 2026-07-24):
 * 0 none, 1 requested, 2 in pit lane, 3 stopped in box, 4 exiting. */
#define PIT_NONE 0
#define PIT_REQUESTED 1
#define PIT_LANE 2
#define PIT_BOX 3
#define PIT_EXITING 4

typedef struct {
    int    car_id;
    char   name[NAME_LEN];
    int    lap_in, lap_out;
    double t_in, t_out;          /* session clock, pit LANE entry/exit (status 2..0) */
    /* Full breakdown, tracking every pittingStatus edge instead of just
     * lane-in/lane-out. 0 = that edge hasn't happened (yet, or the stop
     * skipped straight past it, which the game's state machine allows). */
    double t_requested;          /* 0 -> 1 */
    double t_box_stop;           /* 2 -> 3, actually stopped */
    double t_exit_start;         /* 3 -> 4, releasing */
    int    tire_in, tire_out;
    float  fuel_in, fuel_out;
    int    fixed_fw, fixed_rw;
    int    dmg_in;               /* damage tier on entry */
    int    completed;
} pit_event_t;

static pit_event_t g_pits[MAX_PIT_EVENTS];
static void ar_on_pit(int slot, const pit_event_t *e);
static int g_pit_head = 0, g_pit_count = 0;
static int g_pit_open[MAX_CARS];      /* index+1 of this car's in-progress stop */
static int g_pit_was_in[MAX_CARS];
static int g_pit_prev_status[MAX_CARS];

static void pits_reset(void) {
    g_pit_head = 0; g_pit_count = 0;
    memset(g_pits, 0, sizeof(g_pits));
    for (int i = 0; i < MAX_CARS; i++) {
        g_pit_open[i] = 0; g_pit_was_in[i] = 0; g_pit_prev_status[i] = PIT_NONE;
    }
}

static void pit_track(int i, car_telemetry_t *c) {
    int status = c->pitting;   /* already an exact int32 enum, see field comment */
    int prev = g_pit_prev_status[i];
    int in_pit = (c->pitting >= PIT_LANE) ? 1 : 0;

    if (in_pit && !g_pit_was_in[i]) {                 /* entered the lane */
        pit_event_t *e = &g_pits[g_pit_head];
        memset(e, 0, sizeof(*e));
        e->car_id = i;
        memcpy(e->name, c->name, NAME_LEN);
        e->name[NAME_LEN-1]=0;
        e->lap_in   = c->lap_number;
        e->t_in     = g_telemetry.session_time;
        e->tire_in  = c->tire_choice;
        e->fuel_in  = c->fuel_level;
        e->dmg_in   = c->dmg_tier;
        e->tire_out = -1;
        /* the REQUEST (status 1) precedes lane entry and is the real
         * "the driver was told to box" moment -- capture it if we saw it. */
        if (prev == PIT_REQUESTED) e->t_requested = g_telemetry.session_time;
        g_pit_open[i] = g_pit_head + 1;
        g_pit_head = (g_pit_head + 1) % MAX_PIT_EVENTS;
        if (g_pit_count < MAX_PIT_EVENTS) g_pit_count++;
    }

    if (g_pit_open[i]) {
        pit_event_t *e = &g_pits[g_pit_open[i] - 1];
        if (e->car_id!=i || e->completed) { g_pit_open[i]=0; return; }
        /* a repair is only observable while the car is in the box */
        if (e->dmg_in >= 2 && c->fw_flag == 1) e->fixed_fw = 1;
        if (e->dmg_in >= 3 && c->rw_flag == 1) e->fixed_rw = 1;

        if (prev != PIT_BOX && status == PIT_BOX) e->t_box_stop = g_telemetry.session_time;
        if (prev != PIT_EXITING && status == PIT_EXITING) e->t_exit_start = g_telemetry.session_time;

        if (!in_pit && g_pit_was_in[i]) {             /* exited */
            e->lap_out   = c->lap_number;
            e->t_out     = g_telemetry.session_time;
            e->tire_out  = c->tire_choice;
            e->fuel_out  = c->fuel_level;
            e->completed = 1;
            g_pit_open[i] = 0;
            ar_on_pit(i, e);
            incident_push("PITSTOP", i, e->lap_in,
                          (int)((e->t_out - e->t_in) * 10.0),   /* tenths */
                          e->tire_out);
            char escaped_name[NAME_LEN*6+1];
            esc_json(escaped_name,sizeof escaped_name,e->name);
            race_log_write("{\"t\":\"pitstop\",\"tk\":%lu,\"id\":%d,\"n\":\"%s\","
                "\"lap_in\":%d,\"lap_out\":%d,\"dur\":%.2f,\"boxdur\":%.2f,"
                "\"tire_in\":%d,\"tire_out\":%d,\"fuel_in\":%.2f,\"fuel_out\":%.2f,"
                "\"fixed_fw\":%d,\"fixed_rw\":%d,\"dmg_in\":%d}",
                GetTickCount(), i, escaped_name, e->lap_in, e->lap_out,
                e->t_out - e->t_in,
                (e->t_box_stop > 0 && e->t_exit_start > 0) ? (e->t_exit_start - e->t_box_stop) : 0.0,
                e->tire_in, e->tire_out,
                e->fuel_in, e->fuel_out, e->fixed_fw, e->fixed_rw, e->dmg_in);
        }
    }
    g_pit_prev_status[i] = status;
    g_pit_was_in[i] = in_pit;
}

static int build_pits_json(char *buf, int bufsz) {
    EnterCriticalSection(&g_telemetry_cs);
    int p = _snprintf(buf, bufsz, "{\"gen\":%u,\"pits\":[", capture_generation);
    int first = 1;
    for (int k = 0; k < g_pit_count; k++) {
        pit_event_t *e = &g_pits[k];
        if (!e->name[0]) continue;
        char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), e->name);
        /* boxdur = time actually stationary in the box (status 3 window,
         * box_stop -> exit_start). d = the old lane-to-lane figure, kept for
         * compatibility with anything already reading it. 0 = that edge was
         * never observed (attach mid-stop, or the stop is still open). */
        double boxdur = (e->t_box_stop > 0 && e->t_exit_start > 0)
                       ? (e->t_exit_start - e->t_box_stop) : 0.0;
        double reqdelay = (e->t_requested > 0) ? (e->t_in - e->t_requested) : 0.0;
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"id\":%d,\"n\":\"%s\",\"li\":%d,\"lo\":%d,\"d\":%.2f,\"boxdur\":%.2f,\"reqdelay\":%.2f,"
            "\"ti\":%d,\"to\":%d,\"fi\":%.1f,\"fo\":%.1f,"
            "\"rfw\":%d,\"rrw\":%d,\"dmg\":%d,\"done\":%d}",
            first ? "" : ",", e->car_id, esc, e->lap_in, e->lap_out,
            e->completed ? (e->t_out - e->t_in) : 0.0, boxdur, reqdelay,
            e->tire_in, e->tire_out, e->fuel_in, e->fuel_out,
            e->fixed_fw, e->fixed_rw, e->dmg_in, e->completed);
        first = 0;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* Per-driver dossier: every completed lap in full detail plus that driver's
 * pit stops. Deliberately a SEPARATE endpoint from /laps -- /laps polls all
 * cars every 3 s for the pace grid and must stay small, while this is fetched
 * only for the one driver whose card is open. */
static int build_driver_json(char *buf, int bufsz, int id) {
    EnterCriticalSection(&g_telemetry_cs);
    int p = 0;
    if (id < 0 || id >= MAX_CARS) {
        p = _snprintf(buf, bufsz, "{\"err\":\"bad id\"}");
        LeaveCriticalSection(&g_telemetry_cs);
        return p;
    }
    car_telemetry_t *c = &g_telemetry.cars[id];
    char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
    char ecn[NAME_LEN * 6 + 1]; esc_json(ecn, sizeof(ecn), c->car_name);
    p += _snprintf(buf + p, bufsz - p,
        "{\"gen\":%u,\"id\":%d,\"n\":\"%s\",\"cn\":\"%s\",\"p\":%d,\"laps\":[",
        capture_generation, id, esc, ecn, c->position);

    int lc = g_lap_history_count[id];
    for (int j = 0; j < lc; j++) {
        lap_row_t *r = &g_lap_rows[id][j];
        /* rc.2: the game's s1 and s2 are cumulative elapsed times (s2 = S1+S2).
         * "s2" keeps that cumulative value (unchanged key and meaning, documented
         * here). "s2d" = S2 split = s2 - s1; "s3" = S3 split = lap - s2 (rc.1
         * computed lap - s1 - s2, which subtracts S1 twice). -1 = not derivable. */
        float s2d = (r->s1 > 0 && r->s2 > r->s1) ? (r->s2 - r->s1) : -1.0f;
        float s3 = (r->t > 0 && r->s2 > 0 && r->t > r->s2) ? (r->t - r->s2) : -1.0f;
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"l\":%d,\"t\":%.3f,\"s1\":%.3f,\"s2\":%.3f,\"s2d\":%.3f,\"s3\":%.3f,"
            "\"p\":%d,\"tc\":%d,\"f\":%.1f,\"pit\":%d,"
            "\"lf\":[%.4f,%.4f,%.4f,%.4f]}",
            j ? "," : "", r->lap, r->t, r->s1, r->s2, s2d, s3,
            r->pos, r->tire, r->fuel, r->pitted,
            r->life[0], r->life[1], r->life[2], r->life[3]);
        if (p >= bufsz - 256) break;
    }
    p += _snprintf(buf + p, bufsz - p, "],\"pits\":[");

    int firstp = 1;
    for (int k = 0; k < g_pit_count; k++) {
        pit_event_t *e = &g_pits[k];
        if (e->car_id != id || !e->name[0]) continue;
        double boxdur = (e->t_box_stop > 0 && e->t_exit_start > 0)
                       ? (e->t_exit_start - e->t_box_stop) : 0.0;
        double reqdelay = (e->t_requested > 0) ? (e->t_in - e->t_requested) : 0.0;
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"li\":%d,\"lo\":%d,\"d\":%.2f,\"boxdur\":%.2f,\"reqdelay\":%.2f,\"ti\":%d,\"to\":%d,"
            "\"fi\":%.1f,\"fo\":%.1f,\"add\":%.1f,"
            "\"rfw\":%d,\"rrw\":%d,\"dmg\":%d,\"done\":%d}",
            firstp ? "" : ",", e->lap_in, e->lap_out,
            e->completed ? (e->t_out - e->t_in) : 0.0, boxdur, reqdelay,
            e->tire_in, e->tire_out, e->fuel_in, e->fuel_out,
            e->completed ? (e->fuel_out - e->fuel_in) : 0.0,
            e->fixed_fw, e->fixed_rw, e->dmg_in, e->completed);
        firstp = 0;
        if (p >= bufsz - 256) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}



/* ========================================================================
 * TIMING SECTORS — parsed from the current track's .AIW
 *
 * Re-parsed only when the game's AIW path string changes, i.e. once per
 * track load, not per tick. The file is ~400 KB and only three numbers in
 * its header are wanted, so the scan stops as soon as all three are found.
 * ======================================================================== */
static char g_aiw_seen[260];

/* "key=value", no quoting, plain float values. */
static int aiw_key(const char *line, const char *key, float *out) {
    size_t n = strlen(key);
    if (_strnicmp(line, key, n) != 0) return 0;
    const char *p = line + n;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return 0;
    *out = (float)atof(p + 1);
    return 1;
}

static void sectors_update(void) {
    const char *rel = (const char *)ADDR_AIW_PATH;
    if (!rel[0]) {
        g_aiw_seen[0]=g_telemetry.aiw_path[0]=g_telemetry.track_name[0]=0;
        g_telemetry.sec1=g_telemetry.sec2=g_telemetry.lap_len=0;
        g_gdb_n = 0; g_gdb_truncated = 0; g_gdb_file[0] = 0;
        return;
    }
    size_t rl = strlen(rel);
    if (rl < 5 || rl >= sizeof(g_telemetry.aiw_path)) return;
    if (_stricmp(rel + rl - 4, ".AIW") != 0) return;    /* same test F1CUI uses */
    if (strcmp(rel, g_aiw_seen) == 0) return;           /* unchanged track */

    memcpy(g_aiw_seen, rel, rl + 1);
    memcpy(g_telemetry.aiw_path, rel, rl + 1);
    /* An unreadable new AIW must never inherit the previous circuit. */
    g_telemetry.sec1=g_telemetry.sec2=g_telemetry.lap_len=0.0f;
    g_groove_w = g_groove_ww = 0.0f;   /* rc.12: a stale groove width is worse than none */
    g_gdb_n = 0; g_gdb_truncated = 0; g_gdb_file[0] = 0;   /* rc.12: no stale weather script */

    /* Track name = the AIW's own filename, minus directory and extension. */
    const char *slash = strrchr(rel, '\\');
    const char *base = slash ? slash + 1 : rel;
    size_t bl = strlen(base);
    if (bl > 4) bl -= 4;
    if (bl >= sizeof(g_telemetry.track_name)) bl = sizeof(g_telemetry.track_name) - 1;
    memcpy(g_telemetry.track_name, base, bl);
    g_telemetry.track_name[bl] = '\0';

    /* The AIW path is relative to the game directory, and we are injected
     * INTO the game, so its own module path is the anchor — no config file
     * and no guessing at an install location. */
    char full[MAX_PATH];
    if (!GetModuleFileNameA(NULL, full, sizeof(full))) return;
    char *cut = strrchr(full, '\\');
    if (!cut) return;
    cut[1] = '\0';
    if (strlen(full) + rl + 1 >= sizeof(full)) return;
    strcat(full, rel);

    FILE *f = fopen(full, "r");
    if (!f) { log_f("SECTORS: cannot open AIW '%s'\n", full); return; }

    float s1 = 0, s2 = 0, ll = 0;
    float gw = 0, gww = 0;   /* rc.12 groove widths (map annotation only) */
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (s1 <= 0) aiw_key(line, "sector_1_length", &s1);
        if (s2 <= 0) aiw_key(line, "sector_2_length", &s2);
        if (ll <= 0) aiw_key(line, "lap_length", &ll);
        if (gw <= 0) aiw_key(line, "GrooveWidth", &gw);          /* aiw_key demands '=' directly
                                                                  * after the key, so the
                                                                  * GrooveWidthWet line cannot
                                                                  * satisfy this test */
        if (gww <= 0) aiw_key(line, "GrooveWidthWet", &gww);
        if (s1 > 0 && s2 > 0 && ll > 0 && gw > 0 && gww > 0) break;
    }
    fclose(f);

    /* rc.12: plausible groove widths for the Weather map band; silent on junk. */
    if (isfinite(gw) && isfinite(gww) && gw > 1.0f && gw < 60.0f &&
        gww > 0.5f && gww <= gw) {
        g_groove_w = gw;
        g_groove_ww = gww;
    }

    /* rc.12: the same once-per-track scan reads the GDB session weather script. */
    gdb_script_parse(full);

    /* Despite the "_length" names these are CUMULATIVE distances, so they
     * must come out strictly increasing. If they don't, the file is not what
     * we think it is — publish nothing rather than a wrong sector map. */
    if (!(isfinite(s1) && isfinite(s2) && isfinite(ll) && s1 > 0 && s2 > s1 && ll > s2)) {
        log_f("SECTORS: implausible AIW values s1=%.1f s2=%.1f lap=%.1f\n", s1, s2, ll);
        g_telemetry.sec1 = g_telemetry.sec2 = g_telemetry.lap_len = 0.0f;
        return;
    }
    g_telemetry.sec1 = s1;
    g_telemetry.sec2 = s2;
    g_telemetry.lap_len = ll;
    log_f("SECTORS: %s  S1<%.1f  S2<%.1f  lap %.1f m\n",
          g_telemetry.track_name, s1, s2, ll);
}

/* Which of the three timing sectors a track position falls in. 0 = unknown. */
static int sector_of_t(const telemetry_t *t, float track_pos) {
    if (t->lap_len <= 0.0f || track_pos < 0.0f) return 0;
    if (track_pos < t->sec1) return 1;
    if (track_pos < t->sec2) return 2;
    return 3;
}
static int sector_of(float track_pos) { return sector_of_t(&g_telemetry, track_pos); }

/* ---- marshal-zone flags ----------------------------------------------------
 * These are the Steward's flag-posting segments (~20 per track), NOT the
 * three timing sectors above — different concept, different source.
 * Read-only. The Steward rebuilds this table from scratch every frame (see
 * ADDR_STEWARD), so there is nothing here to write; the value is in reporting
 * it, which the game itself never does beyond a flag sprite on the HUD.
 */
static int g_prev_seg_state[MAX_SEGMENTS];
static int g_seg_state_known = 0;

static void flags_reset(void) {
    for (int i = 0; i < MAX_SEGMENTS; i++) g_prev_seg_state[i] = 0;
    g_seg_state_known = 0;
}

static void flags_read(int leader_lap) {
    volatile uint8_t *st = (volatile uint8_t *)ADDR_STEWARD;
    float scale = finite_float(*(const float *)(st + STW_OFF_SEG_SCALE));
    int   n     = *(volatile int   *)(st + STW_OFF_SEG_COUNT);
    volatile uint8_t *arr = *(volatile uint8_t **)(st + STW_OFF_SEG_ARRAY);

    if (n <= 0 || n > MAX_SEGMENTS || !arr || (uintptr_t)arr < 0x10000
        || scale <= 0.000001f || scale > 100.0f) {
        g_telemetry.seg_count = 0;
        g_telemetry.yellow_count = 0;
        return;
    }

    g_telemetry.seg_count = n;
    g_telemetry.seg_len   = 1.0f / scale;
    g_telemetry.track_len = n * g_telemetry.seg_len;
    g_telemetry.yellow_count = 0;

    for (int i = 0; i < n; i++) {
        int s = *(volatile int *)(arr + i * 8);
        g_telemetry.seg_state[i] = s;
        if (s == FLAG_YELLOW) g_telemetry.yellow_count++;

        /* Transition -> Race Control bulletin. Only edges: the table is rebuilt
         * every frame, so a stationary car would otherwise emit an event at 60Hz. */
        if (g_seg_state_known && s != g_prev_seg_state[i]) {
            incident_push(s == FLAG_YELLOW ? "YELLOW" : (s == FLAG_GREEN ? "GREEN" : "FLAG"),
                          -1, leader_lap, i, s);
        }
        g_prev_seg_state[i] = s;
    }
    g_seg_state_known = 1;
}

/* The game's own yellow test, applied per car instead of only to the player:
 * HUDFlagRendererMain shows yellow when the segment the car is in, OR the one
 * behind it (index - 1, wrapping), holds state 2. */
static void flags_apply_to_car(car_telemetry_t *c) {
    c->segment = -1;
    c->under_yellow = 0;
    if (g_telemetry.seg_count <= 0 || g_telemetry.seg_len <= 0.0f) return;

    int n = g_telemetry.seg_count;
    int idx = (int)(c->track_pos / g_telemetry.seg_len);
    if (idx < 0) idx = 0;
    if (idx > n - 1) idx = n - 1;
    c->segment = idx;

    int prev = idx - 1;
    if (prev < 0) prev += n;
    c->under_yellow = (g_telemetry.seg_state[idx] == FLAG_YELLOW
                       || g_telemetry.seg_state[prev] == FLAG_YELLOW) ? 1 : 0;
}

/* ---- who is the human? ----------------------------------------------------
 * `CurrentPlayerIndex` (0x7C54DC) is NOT the player's vehicle slot. Live
 * 2026-07-25 it read 0, then 2, then a stable 6 during one race in which the
 * human was slot 0 (Matias) throughout — it behaves like a camera/focus target.
 * Worse, the client was comparing it against each car's POSITION, not its slot.
 *
 * Derive it instead from the one signal that is unambiguous: 0x45F4 is an
 * AI-control bit, so the human is the car that is NOT under AI control while
 * still racing. Every other car reading 0 there is a retired one (fs=2/ret=1),
 * which the guards below exclude. Sticky: if the human retires, no car matches
 * any more, and the last known slot is kept rather than silently jumping to 0.
 */
static int g_player_slot = 0, g_player_slot_found = 0;

/* ---- 1.1.0 player chassis capture (vanilla full-physics object) ---- */
static float pch_f(double v, double lo, double hi, double scale) {
    return (isfinite(v) && v >= lo && v <= hi) ? (float)(v * scale) : -999.0f;
}
static double g_pch_prev_life = 0.0, g_pch_prev_t = -1.0;
static float g_pch_radius[4];
static void player_chassis_capture(void) {
    double opt0 = PW(0, 820), st = g_telemetry.session_time;
    /* Zeroed block (menus, session end where sim time jumps to ~1e9) = no data. */
    int valid = isfinite(opt0) && opt0 > 300.0 && opt0 < 450.0 && st > 0.0 && st < 1.0e8;
    g_telemetry.pch_valid = valid;
    if (!valid) { g_pch_prev_t = -1.0; g_telemetry.pch_eng_wear = -999.0f; return; }
    for (int w = 0; w < 4; w++) {
        g_telemetry.pch_ride_mm[w]   = pch_f(PP_D(5872 + 8 * w), -0.5, 1.0, 1000.0);
        g_telemetry.pch_susp_mm[w]   = pch_f(PW(w, 488), -0.5, 0.5, 1000.0);
        g_telemetry.pch_tdefl_mm[w]  = pch_f(PW(w, 804), -0.5, 0.5, 1000.0);
        g_telemetry.pch_press[w]     = pch_f(PW(w, 884), 0.0, 1000.0, 1.0);
        g_telemetry.pch_press_set[w] = pch_f(PW(w, 892), 0.0, 1000.0, 1.0);
        g_telemetry.pch_grip[w]      = pch_f(PW(w, 924), 0.0, 2.0, 1.0);
        g_telemetry.pch_opt_c[w]     = temp_sanity(PW(w, 820) - KELVIN_TO_C);
        g_telemetry.pch_camber_deg[w]= pch_f(PW(w, 788), -1.0, 1.0, 57.29577951308232);
        g_telemetry.pch_spin[w]      = pch_f(PW(w, 188), -2000.0, 2000.0, 1.0);
        g_telemetry.pch_disc_mm[w]   = pch_f(PW(w, 1068), 0.0, 0.2, 1000.0);
        g_telemetry.pch_disc_fail_mm[w] = pch_f(PW(w, 1092), 0.0, 0.2, 1000.0);
        g_telemetry.pch_bend[w]      = pch_f(PP_D(1124 * w + 1116), -3.2, 3.2, 1.0);
        g_telemetry.pch_detach[w]    = PP_B(1124 * w + 1104) ? 1 : 0;
        g_telemetry.pch_bent[w]      = PP_B(1124 * w + 1105) ? 1 : 0;
    }
    static const int raw_off[4] = { 0x48, 0x50, 0x40, 0x20 }, used_off[4] = { 0x08, 0x10, 0x00, 0x18 };
    for (int k = 0; k < 4; k++) {
        g_telemetry.pch_in_raw[k]  = pch_f(PP_D(raw_off[k]), -1.5, 1.5, 1.0);
        g_telemetry.pch_in_used[k] = pch_f(PP_D(used_off[k]), -1.5, 1.5, 1.0);
    }
    g_telemetry.pch_bottom    = PP_B(5912) ? 1 : 0;
    g_telemetry.pch_compound  = PP_I(5212);
    g_telemetry.pch_fuel      = pch_f(PP_D(0x19C), 0.0, 1000.0, 1.0);
    g_telemetry.pch_rpm       = pch_f(PP_D(0x1C5C), -100.0, 5000.0, 9.549296585513721);
    g_telemetry.pch_clutch_rpm= pch_f(PP_D(0x1E38), -100.0, 5000.0, 9.549296585513721);
    g_telemetry.pch_oil_press = pch_f(PP_D(7540), 0.0, 10000.0, 1.0);
    g_telemetry.pch_bias      = pch_f(PP_D(5476), 0.0, 1.0, 1.0);
    g_telemetry.pch_drag      = pch_f(PP_D(6876), -10.0, 10.0, 1.0);
    g_telemetry.pch_lift      = pch_f(PP_D(7060), -10.0, 10.0, 1.0);
    g_telemetry.pch_drag_used = pch_f(PP_D(7068), -10.0, 10.0, 1.0);
    /* Engine wear rate = engine life consumed per simulated second
     * ((rpm curve + oil curve)/2 + stress, EngineStressSimulation 0x43EF20).
     * Live 2026-10-13: 0.0006 s/s at idle, up to 2.06 s/s at 17,500 rpm. */
    double life = g_telemetry.player_engine_life_s;
    if (g_pch_prev_t >= 0.0 && st > g_pch_prev_t && st - g_pch_prev_t <= 2.0 && life <= g_pch_prev_life) {
        float r = (float)((g_pch_prev_life - life) / (st - g_pch_prev_t));
        float prev = g_telemetry.pch_eng_wear;
        g_telemetry.pch_eng_wear = (prev < 0.0f) ? r : prev + 0.3f * (r - prev);
    } else if (g_pch_prev_t < 0.0 || st < g_pch_prev_t || life > g_pch_prev_life) {
        g_telemetry.pch_eng_wear = -999.0f;   /* start, restart or engine reset */
    }
    if (st != g_pch_prev_t) { g_pch_prev_t = st; g_pch_prev_life = life; }
}
/* Wheel roll ratio = wheel surface speed / car speed (1 rolling, 0 locked,
 * >1 wheelspin). The rolling radius is learned per wheel while coasting
 * (no brake, light throttle, > 20 m/s); live 0.345 m front, 0.333 m rear. */
static void player_chassis_roll(void) {
    for (int w = 0; w < 4; w++) g_telemetry.pch_roll[w] = -999.0f;
    if (!g_telemetry.pch_valid || g_player_slot < 0 || g_player_slot >= MAX_CARS) return;
    float v = g_telemetry.cars[g_player_slot].speed;
    float brk = g_telemetry.pch_in_used[1], thr = g_telemetry.pch_in_used[0];
    for (int w = 0; w < 4; w++) {
        float sp = fabsf(g_telemetry.pch_spin[w]);
        if (g_telemetry.pch_spin[w] <= -999.0f) continue;
        if (v > 20.0f && brk >= 0.0f && brk < 0.01f && thr >= 0.0f && thr < 0.6f && sp > 1.0f) {
            float r = v / sp;
            if (r > 0.2f && r < 0.5f)
                g_pch_radius[w] = g_pch_radius[w] > 0.0f ? g_pch_radius[w] + 0.05f * (r - g_pch_radius[w]) : r;
        }
        if (g_pch_radius[w] > 0.0f && v > 5.0f) g_telemetry.pch_roll[w] = sp * g_pch_radius[w] / v;
    }
}

/* ---- v6 viewer channels (FL,FR,RL,RR). Runs after player_slot_update()
 * so the player test uses THIS tick's slot (the vehicle loop still sees the
 * previous tick's g_player_slot). Wheel-block array index for FL,FR,RL,RR is
 * 0,1,2,3 (1.1.0 correction; was 3,2,1,0 from F1CUI labels). ---- */
static void fill_v6_channels(int i, car_telemetry_t *c, volatile uint8_t *veh) {
    static const int wb_index[4] = { 0, 1, 2, 3 };   /* 1.1.0: FL,FR,RL,RR (see OFF_TIRE_WEAR_FL) */
    int ok_su = 1, ok_ld = 1, ok_sl = 1, ok_fo = 1, ok_tk = 1, ok_tt = 1, ok_tc = 1;
    for (int w = 0; w < 4; w++) {
        const uint8_t *wb = (const uint8_t *)veh + OFF_WHEEL_BASE + WHEEL_BLOCK_STRIDE * wb_index[w];
        c->ttemp[w] = c->tcarc[w] = -999.0f;
        c->rx_su[w] = c->rx_ld[w] = c->rx_sl[w] = c->rx_fo[w] = 0.0f;
        if (i == g_player_slot) {
            float sum = 0.0f;
            for (int z = 0; z < TIRE_TEMP_ZONES; z++) {
                if (g_telemetry.tire_temp[w][z] <= -999.0f) ok_tt = 0;
                sum += g_telemetry.tire_temp[w][z];
            }
            c->ttemp[w] = sum / (float)TIRE_TEMP_ZONES;
            const uint8_t *wh = (const uint8_t *)ADDR_WHEEL_THERMAL + (size_t)w * WHEEL_THERMAL_STRIDE;
            c->tcarc[w] = temp_sanity(*(const double *)(wh + WH_OFF_CARCASS) - KELVIN_TO_C);
            if (c->tcarc[w] <= -999.0f) ok_tc = 0;
            double ld = *(const double *)((const uint8_t *)ADDR_WHEEL_LOAD + (size_t)w * WHEEL_THERMAL_STRIDE);
            /* A raw 0 is an unloaded block, not a measurement. */
            if (isfinite(ld) && fabs(ld) > 1.0e-9 && fabs(ld) < 1.0e7) c->rx_ld[w] = (float)ld; else ok_ld = 0;
        } else {
            float tk = *(const float *)(wb + WB_OFF_TEMP_K);
            float t = (isfinite(tk) && tk > 200.0f && tk < 700.0f) ? (float)(tk - KELVIN_TO_C) : -999.0f;
            c->ttemp[w] = t;
            if (t <= -999.0f) ok_tk = 0;
            float su = *(const float *)(wb + WB_OFF_SUSP), ld = *(const float *)(wb + WB_OFF_LOAD);
            float sl = *(const float *)(wb + WB_OFF_SLIP), fo = *(const float *)(wb + WB_OFF_FORCE);
            if (trace_channel_ok(su, 10.0f)) c->rx_su[w] = su; else ok_su = 0;
            if (trace_channel_ok(ld, 1.0e6f)) c->rx_ld[w] = ld; else ok_ld = 0;
            if (trace_channel_ok(sl, 10.0f)) c->rx_sl[w] = sl; else ok_sl = 0;
            if (trace_channel_ok(fo, 1.0e6f)) c->rx_fo[w] = fo; else ok_fo = 0;
        }
    }
    /* 1.1.0 AI brake temperature (per axle) and wheel spin, vanilla AI path. */
    c->abt[0] = c->abt[1] = -999.0f;
    for (int w = 0; w < 4; w++) c->aws[w] = -999.0f;
    if (i != g_player_slot && c->controller_role != CTRL_REMOTE) {
        const uint8_t *wb0 = (const uint8_t *)veh + OFF_WHEEL_BASE;
        for (int a = 0; a < 2; a++) {
            float k = *(const float *)(wb0 + WHEEL_BLOCK_STRIDE * (2 * a) + AI_WB_BRAKE_K);
            c->abt[a] = temp_sanity((double)k - KELVIN_TO_C);
        }
        for (int w = 0; w < 4; w++) {
            float sp = *(const float *)(wb0 + WHEEL_BLOCK_STRIDE * w + AI_WB_SPIN);
            if (isfinite(sp) && fabsf(sp) < 2000.0f) c->aws[w] = sp;
        }
    }
    if (i == g_player_slot) {
        if (ok_tt) c->vf6 |= 4;
        if (ok_tc) c->vf6 |= 8;
        if (ok_ld) c->vf6 |= 32;
    } else {
        if (ok_su) c->vf6 |= 16;
        if (ok_ld) c->vf6 |= 32;
        if (ok_sl) c->vf6 |= 64;
        if (ok_fo) c->vf6 |= 128;
        if (ok_tk) c->vf6 |= 256;
    }
}

/* ---- CO-OP remote cars (controller mode 2) ----
 * Simulated on the other PC: inputs, accelerations, steering, tread, tyre
 * temperatures and every RESEARCH field in this process are network copies or
 * stale, so their validity bits are cleared. The raw speed field is not
 * written by the network apply path (host copy read 0 on 2026-10-10 before
 * coop RemoteCarTracking), so speed is DERIVED from the interpolated pose:
 * horizontal distance / sim time, EMA-smoothed, updated at most every 50 ms.
 * Pose, yaw, rpm, lap and lap distance stay as read. */
static float g_rem_x[MAX_CARS], g_rem_z[MAX_CARS], g_rem_v[MAX_CARS];
static double g_rem_t[MAX_CARS];
static unsigned char g_rem_have[MAX_CARS];

/* Remote lap distance (4.4.0-rc.1). Live 2026-10-11 CO-OP host: a remote
 * car's lap distance (veh+0x4044) and lap number stay frozen on the host copy
 * (5044.8 m, lap 1 while driving). Its interpolated pose is projected onto the
 * AIW waypoint polyline; only the main loop (cumulative <= lap length + 30 m)
 * is searched so the pit-lane tail never captures it. Live check against 13
 * simulated bots: projection = native lap distance - 6..11 m (DERIVED). */
static float g_wp_cum[MAX_WAYPOINTS + 1];
static int g_wp_cum_n = 0;
static uintptr_t g_wp_cum_arr = 0;
static int g_rem_wp[MAX_CARS];
static int g_rem_lap[MAX_CARS];
static float g_rem_dist[MAX_CARS];
/* 1.2.3 (owner CO-OP report 2026-10-17: "lap times come from qualifying and they have a lot more laps"): the remote lap
 * counter was never reset at a session change, so race laps continued the qualifying count. On a client the game's best
 * and last lap of a remote car can keep the earlier session's values; until the game changes them, they come from the
 * line crossings seen here (DERIVED from pose, about one sample of error). */
static double g_rem_dt[MAX_CARS], g_rem_lap_t0[MAX_CARS];
static float g_rem_last[MAX_CARS], g_rem_best[MAX_CARS], g_rem_game_best[MAX_CARS], g_rem_game_last[MAX_CARS];
static unsigned char g_rem_game_seen[MAX_CARS];
/* 1.2.0 (owner report "friend data has some delay", 2026-10-14): the AIW polyline starts at its first waypoint, not at the
 * game's lap-distance origin. Owner CO-OP data, Albert Park 1997: the remote trace sat a constant +150.6 m from the player's
 * at the same place (141 matched points, p10..p90 145.8..159.7 m; speed at the same place equal, -0.5 m/s), so it was an
 * origin offset, not a time delay. The offset is learned from locally simulated cars on track (game lap distance minus their
 * own projection, median per frame, smoothed) and added to every remote projection. */
static float g_rem_origin = 0.0f;
static int g_rem_origin_ok = 0;
static float g_rem_origin_len = 0.0f;
static unsigned g_rem_origin_frame = 0;

static int remote_wp_prepare(volatile uint8_t **arr_out) {
    volatile uint8_t *base = (volatile uint8_t *)ADDR_AI_DB_BASE;
    int count = *(volatile int *)(base + OFF_WP_COUNT);
    volatile uint8_t *arr = *(volatile uint8_t **)(base + OFF_WP_ARRAY);
    if (count < 3 || count > MAX_WAYPOINTS || !arr || (uintptr_t)arr < 0x10000) return 0;
    if ((uintptr_t)arr != g_wp_cum_arr || count != g_wp_cum_n) {
        g_wp_cum[0] = 0.0f;
        for (int k = 1; k < count; k++) {
            float ax = *(volatile float *)(arr + (size_t)(k - 1) * WP_STRIDE + WP_OFF_X);
            float az = *(volatile float *)(arr + (size_t)(k - 1) * WP_STRIDE + WP_OFF_Z);
            float bx = *(volatile float *)(arr + (size_t)k * WP_STRIDE + WP_OFF_X);
            float bz = *(volatile float *)(arr + (size_t)k * WP_STRIDE + WP_OFF_Z);
            float d = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
            g_wp_cum[k] = g_wp_cum[k - 1] + (isfinite(d) && d < 1000.0f ? d : 0.0f);
        }
        g_wp_cum_n = count; g_wp_cum_arr = (uintptr_t)arr;
        for (int k = 0; k < MAX_CARS; k++) g_rem_wp[k] = -1;
    }
    *arr_out = arr;
    return count;
}

static float remote_project(int i, float x, float z, float lap_len) {
    volatile uint8_t *arr;
    int count = remote_wp_prepare(&arr);
    if (!count || !(lap_len > 100.0f)) return -1.0f;
    int last = count - 1;
    while (last > 1 && g_wp_cum[last] > lap_len + 30.0f) last--;
    int lo = 0, hi = last;
    if (g_rem_wp[i] >= 0 && g_rem_wp[i] <= last) { lo = g_rem_wp[i] - 80; hi = g_rem_wp[i] + 80; }
    float best = 1e30f, bestd = -1.0f; int besti = -1;
    for (int n = lo; n <= hi; n++) {
        int k = ((n % (last + 1)) + (last + 1)) % (last + 1);
        int k2 = k == last ? 0 : k + 1;
        float ax = *(volatile float *)(arr + (size_t)k * WP_STRIDE + WP_OFF_X);
        float az = *(volatile float *)(arr + (size_t)k * WP_STRIDE + WP_OFF_Z);
        float bx = *(volatile float *)(arr + (size_t)k2 * WP_STRIDE + WP_OFF_X);
        float bz = *(volatile float *)(arr + (size_t)k2 * WP_STRIDE + WP_OFF_Z);
        float dx = bx - ax, dz = bz - az, l2 = dx * dx + dz * dz;
        if (!(l2 > 1e-6f)) continue;
        float t = ((x - ax) * dx + (z - az) * dz) / l2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float px = ax + t * dx - x, pz = az + t * dz - z, e = px * px + pz * pz;
        if (e < best) { best = e; besti = k; bestd = g_wp_cum[k] + t * sqrtf(l2); }
    }
    if (besti < 0 || best > 60.0f * 60.0f) { g_rem_wp[i] = -1; return -1.0f; }
    g_rem_wp[i] = besti;
    if (bestd > lap_len) bestd -= lap_len;
    return bestd;
}

static float remote_wrap(float d, float len) {
    while (d < 0.0f) d += len;
    while (d >= len) d -= len;
    return d;
}

/* Capture thread, once per frame before remote_gate. Up to 4 local cars per frame (every frame until 20 good frames, then
 * every 10th): moving (> 20 m/s), not in the pit lane, valid game lap distance. Signed offset wrapped to +-lap/2. */
static void remote_origin_learn(int count) {
    float lap = g_telemetry.lap_len, v[4];
    int n = 0;
    if (!(lap > 100.0f)) { g_rem_origin_ok = 0; return; }
    if (lap != g_rem_origin_len) { g_rem_origin_len = lap; g_rem_origin_ok = 0; g_rem_origin = 0.0f; }
    if (g_rem_origin_ok >= 20 && (++g_rem_origin_frame % 10u) != 0u) return;
    for (int i = 0; i < count && n < 4; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position <= 0 || c->controller_role == CTRL_REMOTE || c->pitting != 0) continue;
        if (!(c->speed > 20.0f) || !(c->lap_dist >= 0.0f && c->lap_dist <= lap)) continue;
        float d = remote_project(i, c->phys_x, c->phys_z, lap);
        g_rem_wp[i] = -1;   /* the remote cache is for remote cars only */
        if (d < 0.0f) continue;
        float off = c->lap_dist - d;
        if (off > lap * 0.5f) off -= lap;
        if (off < -lap * 0.5f) off += lap;
        v[n++] = off;
    }
    if (!n) return;
    for (int a = 1; a < n; a++) for (int b = a; b > 0 && v[b - 1] > v[b]; b--) { float t = v[b]; v[b] = v[b - 1]; v[b - 1] = t; }
    {
        float med = v[n / 2];
        if (!g_rem_origin_ok) g_rem_origin = med;
        else {
            float diff = med - g_rem_origin;
            if (diff > lap * 0.5f) diff -= lap;
            if (diff < -lap * 0.5f) diff += lap;
            if (fabsf(diff) < 60.0f) g_rem_origin += 0.1f * diff;   /* outliers (wrong segment) are ignored */
        }
        if (g_rem_origin_ok < 1000) g_rem_origin_ok++;
    }
}

/* 1.2.3: one remote car's lap state back to "nothing seen". */
static void remote_car_reset(int i) {
    g_rem_have[i] = 0; g_rem_wp[i] = -1; g_rem_lap[i] = 0; g_rem_dist[i] = 0.0f;
    g_rem_dt[i] = g_rem_lap_t0[i] = 0.0;
    g_rem_last[i] = g_rem_best[i] = 0.0f; g_rem_game_seen[i] = 0;
}
/* 1.2.3: every remote car's lap state; called on a new broker session (worker, under g_telemetry_cs). */
static void remote_session_reset(void) {
    for (int k = 0; k < MAX_CARS; k++) remote_car_reset(k);
}
/* 1.2.3: the first lap number of a remote car. The game's copy can be stale from the earlier session, so it is used only
 * when it is not beyond the locally simulated field (+1); else the field's highest lap. Pure. */
static int remote_seed_lap(int game_lap, int field_max) {
    if (field_max <= 0) return game_lap > 0 ? game_lap : 1;
    if (game_lap > 0 && game_lap <= field_max + 1) return game_lap;
    return field_max;
}
static int remote_field_max_lap(void) {
    int m = 0, n = g_telemetry.vehicle_count;
    if (n > MAX_CARS) n = MAX_CARS;
    for (int k = 0; k < n; k++)
        if (g_telemetry.cars[k].position > 0 && g_telemetry.cars[k].controller_role != CTRL_REMOTE &&
            g_telemetry.cars[k].lap_number > m) m = g_telemetry.cars[k].lap_number;
    return m;
}

static void remote_gate(int i, car_telemetry_t *c) {
    if (i < 0 || i >= MAX_CARS) return;
    if (c->controller_role != CTRL_REMOTE) { remote_car_reset(i); return; }
    if (!g_rem_game_seen[i]) { g_rem_game_seen[i] = 1; g_rem_game_best[i] = c->best_lap; g_rem_game_last[i] = c->last_lap; }
    {
        float d = remote_project(i, c->phys_x, c->phys_z, g_telemetry.lap_len);
        if (d >= 0.0f && g_rem_origin_ok) d = remote_wrap(d + g_rem_origin, g_telemetry.lap_len);
        if (d >= 0.0f) {
            double now = g_telemetry.session_time;
            if (g_rem_lap[i] <= 0) g_rem_lap[i] = remote_seed_lap(c->lap_number, remote_field_max_lap());
            else if (g_rem_dist[i] > g_telemetry.lap_len - 150.0f && d < 150.0f) {
                double tc = now;
                float span = g_telemetry.lap_len - g_rem_dist[i] + d;
                if (span > 0.0f && now > g_rem_dt[i])
                    tc = g_rem_dt[i] + (now - g_rem_dt[i]) * (g_telemetry.lap_len - g_rem_dist[i]) / span;
                if (g_rem_lap_t0[i] > 0.0 && tc > g_rem_lap_t0[i]) {
                    g_rem_last[i] = (float)(tc - g_rem_lap_t0[i]);
                    if (g_rem_best[i] <= 0.0f || g_rem_last[i] < g_rem_best[i]) g_rem_best[i] = g_rem_last[i];
                }
                g_rem_lap_t0[i] = tc;
                g_rem_lap[i]++;
            }
            g_rem_dist[i] = d;
            g_rem_dt[i] = now;
            c->lap_dist = d;
            c->track_pos = d;            /* metres, as for local cars (sector_of, mini-sectors); was a fraction before 1.2.0 */
            c->track_from_pit = d;       /* JSON tp2 = the lap distance the viewer plots; the host copy keeps it frozen (owner live 2026-10-14: 5179.3 m all lap) */
            c->lap_number = g_rem_lap[i];
            c->vf6 |= 1024;              /* lap distance/lap DERIVED from pose */
            /* 1.2.3: while the game still shows the values it had when this session started, use the crossings seen here */
            if (c->best_lap == g_rem_game_best[i] && c->last_lap == g_rem_game_last[i]) {
                c->best_lap = g_rem_best[i];
                c->last_lap = g_rem_last[i];
            } else g_rem_game_best[i] = -1.0f;   /* the game updates them: trust the game from now on */
        }
    }
    /* 1.2.2: brake (4) and steer (8) stay. The client receiver writes brake 0x3584 as 0 or 1.0 from one packet flag and
     * steer 0x3580 from the packet; it never writes throttle 0x357C (findings coop-remote-brake-binary-receiver-2026-10-16,
     * coop-remote-throttle-not-applied-2026-10-16, DERIVED; live CO-OP check pending). Throttle, tread and accel stay off. */
    c->car_valid &= ~(2 | 16 | 32 | 64);
    c->vf6 &= 2 | 1024;                  /* keep yaw (pose) and derived lap distance */
    c->vf6 |= 512;                       /* remote copy, derived speed */
    if (c->car_valid & 4) c->vf6 |= 4096;   /* brake is on/off only (network flag), not a pedal position */
    double t = g_telemetry.session_time;
    float x = c->phys_x, z = c->phys_z;
    if (!g_rem_have[i] || !(t > g_rem_t[i]) || t - g_rem_t[i] > 1.0) {
        g_rem_have[i] = 1; g_rem_t[i] = t; g_rem_x[i] = x; g_rem_z[i] = z; g_rem_v[i] = -1.0f;
    } else if (t - g_rem_t[i] >= 0.05) {
        float dx = x - g_rem_x[i], dz = z - g_rem_z[i];
        float v = (float)(sqrtf(dx * dx + dz * dz) / (t - g_rem_t[i]));
        if (isfinite(v) && v < 150.0f)
            g_rem_v[i] = g_rem_v[i] < 0.0f ? v : g_rem_v[i] + 0.4f * (v - g_rem_v[i]);
        g_rem_t[i] = t; g_rem_x[i] = x; g_rem_z[i] = z;
    }
    if (g_rem_v[i] >= 0.0f) { c->speed = g_rem_v[i]; c->car_valid |= 1; }
    else c->car_valid &= ~1;
}

static void player_slot_update(int count) {
    int previous_slot = g_player_slot, previous_found = g_player_slot_found;
    g_player_slot = -1;
    g_player_slot_found = 0;
    /* CO-OP (4.4.0-rc.1): the local human is controller mode 0. Remote
     * humans (mode 2) and retired bots can also read ai_control 0, so the
     * mode decides first, even for a retired local player. */
    for (int i = 0; i < count; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position > 0 && c->controller_role == CTRL_LOCAL && c->ai_control == 0) {
            if (previous_slot != i)
                log_f("[player] slot resolved to %d (%s, controller mode 0)", i, c->name);
            g_player_slot = i;
            g_player_slot_found = 1;
            return;
        }
    }
    for (int i = 0; i < count; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position > 0 && c->ai_control == 0 && c->finish_state == 0 && !c->retired) {
            if (previous_slot != i)
                log_f("[player] slot resolved to %d (%s)", i, c->name);
            g_player_slot = i;
            g_player_slot_found = 1;
            return;
        }
    }
    if (previous_found && previous_slot >= 0 && previous_slot < count &&
        g_telemetry.cars[previous_slot].position > 0 &&
        (g_telemetry.cars[previous_slot].finish_state != 0 ||
         g_telemetry.cars[previous_slot].retired)) {
        g_player_slot = previous_slot;
        g_player_slot_found = 1;
    }
}

/* ---- session boundary detection ------------------------------------------
 * The DLL is injected once and outlives any number of sessions, but every
 * history buffer here (incidents, damage latches, lap history, milestone
 * flags) used to be initialised only at DLL load. Combined with the
 * race_active edge never firing (it reads 3, was compared to 1), a second
 * race inherited the first one's retirements, damage and Race Control feed.
 *
 * Anything that identifies a session changing is a reset: the session type,
 * the scheduled lap count, the car count, or the race-active edge. Leader lap
 * going backwards catches a restart of the SAME session type, which none of
 * the others would see.
 */
static int g_ar_restart_pending;   /* session archive: a restart starts a new archived session */
static int g_sess_type_prev = -1, g_sess_laps_prev = -1, g_sess_cars_prev = -1;
static int g_sess_active_prev = -1, g_sess_leader_lap_prev = 0;
static double g_sess_time_prev = -1.0;

static void session_history_clear(const char *why) {
    g_incident_head = 0;
    g_incident_count = 0;
    incidents_init();     /* also calls damage_reset() */
    lap_history_init();
    flags_reset();
    pits_reset();
    g_last_wear_sample = 0;
    /* incidents_init() parks g_prev_race_active at -1, but the START edge and the
     * race-log rotation both require a 0 -> active transition. Left at -1 the new
     * race would never open a log file or emit a START bulletin. */
    g_prev_race_active = 0;
    memset(g_car_state, 0, sizeof(g_car_state));
    memset(g_car_state_init, 0, sizeof(g_car_state_init));
    g_telemetry.fastest_lap_time = 0.0f;
    g_telemetry.fastest_lap_car = -1;
    for (int i = 0; i < MAX_CARS; i++) {
        g_telemetry.cars[i].top_speed_lap = 0.0f;
        g_telemetry.cars[i].top_speed_session = 0.0f;
        g_telemetry.cars[i].top_speed_prev_lap = 0.0f;
    }
    log_f("[session] history cleared (%s)", why);
}

static void session_reset_check(void) {
    int ty = g_telemetry.session_type;
    int tl = g_telemetry.total_laps;
    int vc = g_telemetry.vehicle_count;
    int ac = g_telemetry.race_active;
    double st = g_telemetry.session_time;

    if (g_sess_type_prev == -1) {          /* first tick after injection */
        g_sess_type_prev = ty; g_sess_laps_prev = tl;
        g_sess_cars_prev = vc; g_sess_active_prev = ac;
        g_sess_time_prev = st;
        return;
    }

    const char *why = NULL;
    /* The session clock going BACKWARDS is the only thing that reliably catches
     * a restart of the same race: type, lap count, car count and race-active are
     * all identical before and after one. Tolerance so ordinary float jitter or a
     * paused clock can't trip it.
     * 1.0.1: a car count change is NOT a new session (CO-OP join/leave, cars
     * entering the session); it wiped every lap mid-session (178 times in the
     * owner's log). Per-slot driver changes are handled in lap_history_record.
     * vc is still tracked for the log. */
    if (st < g_sess_time_prev - 0.5)                   { why = "session clock reset (restart)"; g_ar_restart_pending = 1; }
    else if (ty != g_sess_type_prev)                   why = "session type changed";
    else if (tl != g_sess_laps_prev)                   why = "total laps changed";
    else if (ac && !g_sess_active_prev)                why = "race went active";
    if (vc != g_sess_cars_prev && g_sess_cars_prev > 0 && vc > 0)
        log_f("[session] car count %d -> %d (history kept)", g_sess_cars_prev, vc);

    g_sess_type_prev = ty; g_sess_laps_prev = tl;
    g_sess_cars_prev = vc; g_sess_active_prev = ac;
    g_sess_time_prev = st;

    if (why) {
        if (g_race_log) race_log_close();   /* don't leak the old race's file */
        session_history_clear(why);
        g_sess_leader_lap_prev = 0;
    }
}

/* Called at the end of the per-car loop, where the leader lap is known. A leader
 * lap that decreases can only mean a fresh/restarted race. */
static void session_restart_check(int leader_lap) {
    /* 1.0.1: race only. In practice, qualifying and warm-up P1 is the fastest
     * lap, and a new P1 with fewer laps run made the "leader lap" go down: that
     * wiped the whole session's laps mid-qualifying (69 times in the owner's
     * log). A real restart also resets the session clock (session_reset_check). */
    if (g_telemetry.session_type != 7) { g_sess_leader_lap_prev = leader_lap; return; }
    if (leader_lap < g_sess_leader_lap_prev) {
        g_ar_restart_pending = 1;
        if (g_race_log) race_log_close();
        session_history_clear("leader lap went backwards (restart)");
    }
    g_sess_leader_lap_prev = leader_lap;
}

/* ========================================================================
 * V4 DERIVED FEATURES — radar, live sector delta, pit anticipation,
 * top speed, fuel model, contact attribution, fastest lap, driver of the day
 *
 * A second pass over all cars, run once the main per-car loop has filled
 * every real memory field for this tick. Everything here is COMPUTED from
 * fields already read above; nothing here reads new game memory. Each
 * block says plainly whether it's exact (radar distance, top speed, fuel
 * average) or a labelled heuristic (box_soon, contact attribution).
 * ======================================================================== */
static void compute_v4_features(int count) {
    /* ---- FASTEST LAP OF THE SESSION: scan every car's own personal best.
     * Whether this ruleset awards a bonus point for it is unknown, so this
     * is published as information, not as a scored bonus. ---- */
    {
        float best = 1e9f; int who = -1;
        for (int i = 0; i < count; i++) {
            car_telemetry_t *c = &g_telemetry.cars[i];
            if (c->position > 0 && c->best_lap > 0.0f && c->best_lap < best) {
                best = c->best_lap; who = i;
            }
        }
        if (who >= 0) { g_telemetry.fastest_lap_time = best; g_telemetry.fastest_lap_car = who; }
    }

    for (int i = 0; i < count; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        if (c->position <= 0) continue;
        car_state_t *st = &g_car_state[i];

        if (!g_car_state_init[i]) {
            memset(st, 0, sizeof(*st));
            st->start_position = c->position;
            st->clean_lap = 1;
            st->prev_lap_number = c->lap_number;
            st->fuel_at_lap_start = c->fuel_level;
            st->prev_dmg_tier = c->dmg_tier;
            g_car_state_init[i] = 1;
        }

        /* ---- TOP SPEED: real, running max of the actual speed field.
         * veh+0x4538 (max_speed, read earlier as c->max_speed) is a fixed
         * 99999 sentinel in every live sample so far — not usable. ---- */
        if (c->lap_number != st->prev_lap_number) {
            c->top_speed_prev_lap = c->top_speed_lap;
            c->top_speed_lap = 0.0f;

            /* ---- FUEL MODEL: consumption over the lap just completed,
             * rolling average over the last 8 laps, laps-of-autonomy at the
             * CURRENT fuel level and that average. Different from the
             * instantaneous fuel_consumption rate already read above. ---- */
            if (st->fuel_at_lap_start > 0.0f && c->fuel_level >= 0.0f &&
                st->fuel_at_lap_start >= c->fuel_level) {
                st->fuel_hist[st->fuel_hist_i] = st->fuel_at_lap_start - c->fuel_level;
                st->fuel_hist_i = (st->fuel_hist_i + 1) % 8;
                if (st->fuel_hist_n < 8) st->fuel_hist_n++;
            }
            st->fuel_at_lap_start = c->fuel_level;

            st->clean_lap = 1;     /* new lap, nothing attributed to it yet */
            st->prev_lap_number = c->lap_number;
        }
        if (c->speed > c->top_speed_lap)     c->top_speed_lap = c->speed;
        if (c->speed > c->top_speed_session) c->top_speed_session = c->speed;

        if (st->fuel_hist_n > 0) {
            float sum = 0.0f;
            for (int k = 0; k < st->fuel_hist_n; k++) sum += st->fuel_hist[k];
            c->fuel_avg_per_lap = sum / st->fuel_hist_n;
            c->fuel_autonomy_laps = (c->fuel_avg_per_lap > 0.01f)
                                   ? (c->fuel_level / c->fuel_avg_per_lap) : -1.0f;
        } else {
            c->fuel_avg_per_lap = 0.0f;
            c->fuel_autonomy_laps = -1.0f;
        }

        /* ---- LIVE SECTOR DELTA: real elapsed time in the sector the car is
         * CURRENTLY in, minus that car's own personal best for that sector.
         * Needs the AIW-derived sector boundaries (sector_of) and per-car
         * sector-entry timestamps tracked here. Sector 0 = boundaries not
         * loaded yet, or the car hasn't completed a lap so has no PB. ---- */
        int sec = sector_of(c->lap_dist);
        c->sector_elapsed = 0.0f;
        if (sec >= 1 && sec <= 3) {
            if (sec != st->prev_sector) {
                if (st->prev_sector >= 1 && st->prev_sector <= 3 &&
                    st->sector_entry_t[st->prev_sector] > 0.0f) {
                    float dur = (float)(g_telemetry.session_time - st->sector_entry_t[st->prev_sector]);
                    /* C02: report the completed sector against the PB that
                     * existed BEFORE this run; never subtract a PB from a
                     * still-running sector. */
                    if (dur > 3.0f && st->sector_pb[st->prev_sector] > 0.0f) {
                        c->last_sector_delta = dur - st->sector_pb[st->prev_sector];
                        c->last_sector_num = st->prev_sector;
                    }
                    if (dur > 3.0f && (st->sector_pb[st->prev_sector] <= 0.0f ||
                                       dur < st->sector_pb[st->prev_sector]))
                        st->sector_pb[st->prev_sector] = dur;
                }
                st->sector_entry_t[sec] = (float)g_telemetry.session_time;
                st->prev_sector = sec;
            }
            if (st->sector_entry_t[sec] > 0.0f)
                c->sector_elapsed = (float)(g_telemetry.session_time - st->sector_entry_t[sec]);
            if (st->sector_entry_t[sec] > 0.0f && st->sector_pb[sec] > 0.0f) {
                c->live_sector_delta = (float)(g_telemetry.session_time - st->sector_entry_t[sec])
                                      - st->sector_pb[sec];
                c->live_sector_num = sec;
            } else {
                c->live_sector_delta = 0.0f;
                c->live_sector_num = 0;
            }
        } else {
            c->live_sector_num = 0;
        }

        /* ---- PIT ANTICIPATION HEURISTIC: the REAL signal is call_to_box,
         * already read above from the game's own intention flag and never
         * touched here. This ADDS a clearly separate guess for cars the
         * game hasn't called in yet, so a UI never confuses the two.
         * Thresholds are deliberately conservative (used elsewhere in this
         * file: 70%/40% are the blister-call tyre-life edges). ---- */
        c->box_soon = 0;
        c->box_soon_reason[0] = '\0';
        if (!c->call_to_box && c->pitting < 1.0f) {
            if (c->fuel_autonomy_laps >= 0.0f && c->fuel_autonomy_laps < 1.3f) {
                c->box_soon = 1;
                strncpy(c->box_soon_reason, "low fuel", sizeof(c->box_soon_reason) - 1);
            } else if (i == g_telemetry.player_slot && g_telemetry.tread[0] > 0.0f) {
                float minlife = g_telemetry.tread[0];
                for (int w = 1; w < WHEEL_COUNT; w++)
                    if (g_telemetry.tread[w] < minlife) minlife = g_telemetry.tread[w];
                if (minlife < 0.15f) {
                    c->box_soon = 1;
                    strncpy(c->box_soon_reason, "tyre life <15%", sizeof(c->box_soon_reason) - 1);
                }
            }
        }
        if (c->box_soon && !st->prev_box_soon)
            incident_push("BOXSOON", i, c->lap_number, 0, 0);
        st->prev_box_soon = c->box_soon;

        /* ---- CONTACT ATTRIBUTION: this car's damage tier just increased.
         * Look for another car whose damage ALSO just increased this same
         * tick, within CONTACT_RADIUS_M of track position. HEURISTIC
         * (proximity + time coincidence) — the game keeps no guilty/victim
         * record, so this can misfire for two unrelated incidents at
         * different parts of a long straight in the same tick. ---- */
        c->contact_with = -1;
        if (c->dmg_tier > st->prev_dmg_tier) {
            for (int j = 0; j < count; j++) {
                if (j == i) continue;
                car_telemetry_t *o = &g_telemetry.cars[j];
                car_state_t *ost = &g_car_state[j];
                if (o->position <= 0 || !g_car_state_init[j]) continue;
                if (o->dmg_tier <= ost->prev_dmg_tier) continue;
                float dx = c->track_x - o->track_x, dz = c->track_z - o->track_z;
                if (dx * dx + dz * dz <= CONTACT_RADIUS_M * CONTACT_RADIUS_M) {
                    c->contact_with = j;
                    incident_push("CONTACT", i, c->lap_number, j, c->dmg_tier);
                    break;
                }
            }
        }
        st->prev_dmg_tier = c->dmg_tier;
    }

    /* ---- RADAR: distance in metres to the nearest car ahead/behind ON
     * TRACK, using real lap_dist (+ a full lap per lap_number difference so
     * a car a lap down doesn't read as adjacent). O(n^2) over <=104 cars,
     * once per tick — cheap. ---- */
    float lap_len = g_telemetry.lap_len;
    for (int i = 0; i < count; i++) {
        car_telemetry_t *c = &g_telemetry.cars[i];
        c->gap_ahead_m = -1.0f; c->gap_behind_m = -1.0f;
        c->gap_ahead_id = -1;   c->gap_behind_id = -1;
        if (c->position <= 0 || lap_len <= 0.0f || !c->on_track) continue;
        float my_total = c->lap_number * lap_len + c->lap_dist;
        float best_ahead = 1e12f, best_behind = 1e12f;
        for (int j = 0; j < count; j++) {
            if (j == i) continue;
            car_telemetry_t *o = &g_telemetry.cars[j];
            /* CAUGHT LIVE 2026-07-28: position>0 alone let a retired/garaged
             * car with a stale lap_dist skew the "nearest" search — one gap
             * read 12374m in a 5781m lap, more than two lap-lengths away.
             * on_track (finish_state==0 && !retired) is the real filter. */
            if (o->position <= 0 || !o->on_track) continue;
            float their_total = o->lap_number * lap_len + o->lap_dist;
            float d = their_total - my_total;
            if (d > 0.0f && d < best_ahead)  { best_ahead = d;  c->gap_ahead_id = j; }
            if (d < 0.0f && -d < best_behind) { best_behind = -d; c->gap_behind_id = j; }
        }
        if (c->gap_ahead_id >= 0)  c->gap_ahead_m  = best_ahead;
        if (c->gap_behind_id >= 0) c->gap_behind_m = best_behind;
    }

    /* ---- DRIVER OF THE DAY: OUR OWN transparent composite score, NOT the
     * game's f1c_driverOfTheDayEngine.calc(float) — that formula is private
     * and was not recovered from the bytecode signatures alone. Score =
     * positions gained since this DLL first saw the car, +10 for holding
     * the session's fastest lap, +2 per clean lap run (no incident
     * attributed). Recomputed every tick; cheap, all inputs already
     * derived above. ---- */
    {
        int best_car = -1, best_score = -1000000;
        for (int i = 0; i < count; i++) {
            car_telemetry_t *c = &g_telemetry.cars[i];
            if (c->position <= 0 || !g_car_state_init[i]) continue;
            car_state_t *st = &g_car_state[i];
            int gained = st->start_position - c->position;   /* positive = moved up */
            int score = gained * 3;
            if (g_telemetry.fastest_lap_car == i) score += 10;
            if (st->clean_lap) score += 2;
            if (score > best_score) { best_score = score; best_car = i; }
        }
        g_telemetry.dotd_car = best_car;
        g_telemetry.dotd_score = (best_car >= 0) ? best_score : 0;
    }
}

static void read_telemetry(void) {
    EnterCriticalSection(&g_telemetry_cs);

    g_telemetry.session_type       = SESSION_TYPE;
    g_telemetry.race_phase         = RACE_PHASE;
    g_telemetry.race_active_raw    = RACE_ACTIVE;
    /* docs/38: this reads 3 on track, "nonzero = on track". It was compared against
     * 1 for the start/end edges, so the edges never fired: the race log never
     * rotated, incidents from the PREVIOUS race were never cleared, and every
     * damage latch stayed set forever. Normalize to 0/1 here. */
    g_telemetry.race_active        = (g_telemetry.race_active_raw != 0) ? 1 : 0;
    g_telemetry.vehicle_count      = VEHICLE_COUNT;
    g_telemetry.total_laps         = TOTAL_LAPS;
    g_telemetry.track_condition    = TRACK_CONDITION;
    g_telemetry.track_grip_factor  = TRACK_GRIP_FACTOR;
    g_telemetry.race_mode          = RACE_MODE;
    g_telemetry.player_index       = PLAYER_INDEX;   /* raw; NOT the player's slot */
    g_telemetry.player_slot        = g_player_slot;  /* derived last tick, refreshed below */
    g_telemetry.ai_difficulty      = AI_DIFFICULTY;
    g_telemetry.race_time_threshold= RACE_TIME_THRESH;
    g_telemetry.session_time       = finite_double(*(const double *)ADDR_SESSION_TIME);
    g_telemetry.tick               = GetTickCount();

    session_reset_check();

    {
        g_telemetry.ambient_temp = ws_sanity(*(volatile float *)ADDR_AMBIENT_TEMP);
        g_telemetry.track_temp   = ws_sanity(*(volatile float *)ADDR_TRACK_TEMP);
        g_telemetry.on_wetness   = finite_float(*(const float *)ADDR_ON_WETNESS);
        g_telemetry.off_wetness  = finite_float(*(const float *)ADDR_OFF_WETNESS);
    }

    /* Player wheel thermals. Read as doubles, publish Celsius. Outside a
     * session the block is stale/zero, so a raw 0 K would print -273.2 —
     * clamp anything implausible to the same -999 sentinel the weather
     * reads use, and let the dashboard hide it. */
    for (int w = 0; w < WHEEL_COUNT; w++) {
        volatile uint8_t *wh =
            (volatile uint8_t *)ADDR_WHEEL_THERMAL + (size_t)w * WHEEL_THERMAL_STRIDE;
        g_telemetry.brake_temp[w] =
            temp_sanity(*(volatile double *)(wh + WH_OFF_BRAKE_TEMP) - KELVIN_TO_C);
        for (int z = 0; z < TIRE_TEMP_ZONES; z++)
            g_telemetry.tire_temp[w][z] =
                temp_sanity(*(volatile double *)(wh + WH_OFF_TIRE_TEMP + 8 * z) - KELVIN_TO_C);

        /* Same normalization and clamp the game's own wear bar applies. */
        double raw = *(volatile double *)(wh + WH_OFF_TREAD);
        double n = (raw - TREAD_DEAD) * 4.0;
        if (!(n > 0.0)) n = 0.0;        /* also catches NaN */
        if (n > 1.0) n = 1.0;
        g_telemetry.tread[w] = (float)n;

        g_telemetry.blister[w] = *(volatile int *)((volatile uint8_t *)ADDR_BLISTER_FLAGS + 4 * w);
    }
    sectors_update();   /* no-op unless the track changed */
    g_telemetry.oil_temp_hud   = temp_sanity(*(volatile double *)ADDR_OIL_TEMP_HUD - KELVIN_TO_C);
    g_telemetry.water_temp_hud = temp_sanity(*(volatile double *)ADDR_WATER_TEMP_HUD - KELVIN_TO_C);
    g_telemetry.player_engine_life_s = finite_double(*(volatile double *)ADDR_PLAYER_ENGINE_LIFE);
    player_chassis_capture();

    int count = VEHICLE_COUNT;
    if (count < 0) count = 0;
    if (count > MAX_CARS) count = MAX_CARS;

    for (int i = 0; i < count; i++) {
        volatile uint8_t *veh = VEHICLE_PTR(i);
        car_telemetry_t *c = &g_telemetry.cars[i];

        if (!veh || (uintptr_t)veh < 0x10000) {
            memset(c, 0, sizeof(*c));
            continue;
        }

        const char *src = (const char *)(veh + OFF_DRIVER_NAME);
        int j;
        for (j = 0; j < NAME_LEN - 1 && src[j]; j++)
            c->name[j] = src[j];
        c->name[j] = '\0';

        /* car/team name */
        src = (const char *)(veh + OFF_CAR_NAME);
        for (j = 0; j < NAME_LEN - 1 && src[j]; j++)
            c->car_name[j] = src[j];
        c->car_name[j] = '\0';

        c->position          = RI(veh, OFF_POSITION);
        c->lap_number        = RI(veh, OFF_LAP_NUMBER);
        c->finish_state      = RI(veh, OFF_FINISH_STATE);
        c->ai_control        = RI(veh, OFF_AI_CONTROL_FLAG);
        c->controller_role   = RI(veh, OFF_CONTROLLER_MODE);
        c->speed             = RF(veh, OFF_SPEED);
        c->rpm               = RF(veh, OFF_ENGINE_RPM);
        c->eng_angvel        = RF(veh, OFF_ENGINE_ANGVEL);
        c->oil_temp          = RF(veh, OFF_OIL_TEMP);
        c->wear_gate         = RI(veh, OFF_WEAR_GATE);
        c->life_eng          = RF(veh, OFF_LIFE_ENGINE);
        c->life_brk          = RF(veh, OFF_LIFE_BRAKES);
        c->life_gbx          = RF(veh, OFF_LIFE_GEARBOX);
        c->life_sus          = RF(veh, OFF_LIFE_SUSPENSION);
        /* No "initial life" is exposed anywhere, so percentage is against the
         * highest value seen for this car this session. Attach mid-race and the
         * baseline is simply that moment — the percentage then reads high, which
         * is why the raw seconds are shown alongside it and not hidden. */
        {
            float lv[4] = { c->life_eng, c->life_brk, c->life_gbx, c->life_sus };
            int  *pc[4] = { &c->lifepct_eng, &c->lifepct_brk, &c->lifepct_gbx, &c->lifepct_sus };
            for (int q = 0; q < 4; q++) {
                if (lv[q] > g_life_max[i][q]) g_life_max[i][q] = lv[q];
                *pc[q] = (g_life_max[i][q] > 1.0f && lv[q] >= 0.0f)
                         ? (int)((lv[q] / g_life_max[i][q]) * 100.0f + 0.5f) : (lv[q] < 0.0f ? 0 : -1);
            }
        }
        c->water_temp        = RF(veh, OFF_WATER_TEMP);
        c->fuel_level        = RF(veh, OFF_FUEL_LEVEL);
        c->max_rpm           = RF(veh, OFF_ENGINE_MAX_RPM);
        c->gear              = RI(veh, OFF_CURRENT_GEAR);
        c->best_lap          = RF(veh, OFF_BEST_LAP);
        c->last_lap          = RF(veh, OFF_LAST_LAP);
        c->total_time        = RF(veh, OFF_TOTAL_TIME_SF);
        c->track_pos         = RF(veh, OFF_TRACK_POS);
        c->lap_dist          = RF(veh, OFF_TRACK_PART_EXACT);
        c->pitting           = RI(veh, OFF_PITTING_STATUS);
        c->tire_choice       = RI(veh, OFF_TIRE_CHOICE);
        c->throttle          = RF(veh, OFF_THROTTLE);
        c->brake             = RF(veh, OFF_BRAKE);
        c->retired           = RI(veh, OFF_RETIRED_FLAG);
        /* Presence on track, independent of WHO is driving. The old proxy for this
         * was the 0x45F4 flag, which is an AI-control bit and reads 0 for the human
         * -- so the player was treated as absent everywhere. */
        c->on_track          = (c->finish_state == 0 && !c->retired) ? 1 : 0;
        c->best_s1           = RF(veh, OFF_BEST_S1);
        c->best_s2           = RF(veh, OFF_BEST_S2);
        c->last_s1           = RF(veh, OFF_LAST_S1);
        c->last_s2           = RF(veh, OFF_LAST_S2);
        c->phys_x            = RF(veh, OFF_PHYS_X);
        c->phys_z            = RF(veh, OFF_PHYS_Z);
        c->track_x           = RF(veh, OFF_TRACK_X);
        c->track_z           = RF(veh, OFF_TRACK_Z);
        c->dir_x             = RF(veh, OFF_DIR_X);
        c->dir_y             = RF(veh, OFF_DIR_Y);
        c->penalties         = RI(veh, OFF_PENALTIES);
        c->call_to_box       = RI(veh, OFF_CALL_TO_BOX);
        c->pit_time_0        = RF(veh, OFF_PIT_TIME_0);
        c->garage_team       = RI(veh, OFF_GARAGE_TEAM);
        c->garage_bay        = RI(veh, OFF_GARAGE_BAY);

        /* v3 additions */
        c->elapsed_time      = RF(veh, OFF_ELAPSED_TIME);
        c->max_speed         = RF(veh, OFF_MAX_SPEED);
        c->driver_status     = RI(veh, OFF_DRIVER_STATUS);
        c->planned_position  = RI(veh, OFF_PLANNED_POS);
        c->reason_code       = RI(veh, OFF_REASON_CODE);
        c->qualy_time        = RF(veh, OFF_QUALY_TIME);
        c->lap_of_run        = RI(veh, OFF_LAP_OF_RUN);
        c->car_conditions    = RI(veh, OFF_CAR_CONDITIONS);
        c->track_limits      = RI(veh, OFF_TRACK_LIMITS);
        damage_update(i, c, veh);
        c->fuel_consumption  = RF(veh, OFF_FUEL_CONS);
        c->hits_in_race      = RI(veh, OFF_HITS_RACE);
        c->rpm_limiter       = RI(veh, OFF_RPM_LIMITER);
        c->human_drs         = RI(veh, OFF_HUMAN_DRS);
        c->dnf_dsq           = (int)RB(veh, OFF_DNF_DSQ);
        c->track_from_pit    = RF(veh, OFF_TRACK_FROM_PIT);
        c->pit_time_1        = RF(veh, OFF_PIT_TIME_1);
        c->pit_time_2        = RF(veh, OFF_PIT_TIME_2);
        c->last_pit_time     = RF(veh, OFF_LAST_PIT_TIME);
        c->pit_stop_speed    = RF(veh, OFF_PIT_STOP_SPEED);
        c->next_stint_fuel   = RF(veh, OFF_NEXT_STINT_FUEL);
        c->penalty_0         = RI(veh, OFF_PENALTY_0);
        c->penalty_1         = RI(veh, OFF_PENALTY_1);
        c->penalty_2         = RI(veh, OFF_PENALTY_2);
        c->tire_wear_fl         = RF(veh, OFF_TIRE_WEAR_FL);
        c->tire_wear_fr         = RF(veh, OFF_TIRE_WEAR_FR);
        c->tire_wear_rl         = RF(veh, OFF_TIRE_WEAR_RL);
        c->tire_wear_rr         = RF(veh, OFF_TIRE_WEAR_RR);
        c->time_next_exit    = RF(veh, OFF_TIME_NEXT_EXIT);
        c->number_of_laps    = RI(veh, OFF_NUMBER_OF_LAPS);
        c->next_chosen       = RF(veh, OFF_NEXT_CHOSEN);
        c->unlock_tires      = RF(veh, OFF_UNLOCK_TIRES);
        c->speed_cap         = RF(veh, OFF_SPEED_CAP);
        c->ai_pace           = RF(veh, OFF_AI_PACE);
        {
            int rn = RI(veh, OFF_RACE_NUMBER);
            c->race_num = (rn >= 0 && rn <= 999) ? rn : -1;
        }

        /* ---- v5 engineering channels: read + validate per the ledger. ---- */
        c->car_valid = 0;
        c->steer = 0.0f; c->glat = c->glon = c->gvert = 0.0f; c->yaw = 0.0f;
        if (trace_channel_ok(RRAW(veh, OFF_SPEED), 150.0f) && c->speed >= 0.0f && c->speed <= 150.0f)
            c->car_valid |= 1;
        if (trace_channel_ok(RRAW(veh, OFF_THROTTLE), 1.5f)) c->car_valid |= 2;
        if (trace_channel_ok(RRAW(veh, OFF_BRAKE), 1.5f))    c->car_valid |= 4;
        /* 1.2.2: a CO-OP remote car (mode 2) also carries steer: the network receiver 0x4C73F0 writes 0x3580 from the
         * packet (finding coop-remote-brake-binary-receiver-2026-10-16, DERIVED). No ai_control gate for mode 2. */
        if (c->ai_control || c->controller_role == CTRL_REMOTE) {
            float st = RRAW(veh, OFF_STEER_INPUT);
            if (trace_channel_ok(st, 1.5f)) { c->steer = st; c->car_valid |= 8; }
        }
        if (i == g_player_slot) {
            /* Player tread comes from the thermal block (already read and
             * normalized above), not the vehicle mirror. */
            if (g_telemetry.tread[0] > 0.0f || g_telemetry.tread[1] > 0.0f ||
                g_telemetry.tread[2] > 0.0f || g_telemetry.tread[3] > 0.0f)
                c->car_valid |= 16;
        } else if (c->tire_wear_fl >= 0.5f && c->tire_wear_fl <= 1.001f &&
                   c->tire_wear_fr >= 0.5f && c->tire_wear_fr <= 1.001f &&
                   c->tire_wear_rl >= 0.5f && c->tire_wear_rl <= 1.001f &&
                   c->tire_wear_rr >= 0.5f && c->tire_wear_rr <= 1.001f) {
            c->car_valid |= 16;
        }
        {
            float glat = RRAW(veh, OFF_ACCEL_LAT), glon = RRAW(veh, OFF_ACCEL_LON),
                  gvert = RRAW(veh, OFF_ACCEL_VERT);
            float co = RRAW(veh, OFF_HEADING_COS), si = RRAW(veh, OFF_HEADING_SIN);
            if (trace_channel_ok(glat, 100.0f)) { c->glat = glat; c->car_valid |= 32; }
            if (trace_channel_ok(glon, 100.0f)) { c->glon = glon; c->car_valid |= 64; }
            if (trace_channel_ok(gvert, 100.0f))  c->gvert = gvert;
            if (trace_channel_ok(co, 1.1f) && trace_channel_ok(si, 1.1f) &&
                co * co + si * si > 0.9f && co * co + si * si < 1.1f)
                c->yaw = atan2f(si, co);
            c->vf6 = 0;
            if (trace_channel_ok(gvert, 100.0f)) c->vf6 |= 1;
            if (trace_channel_ok(co, 1.1f) && trace_channel_ok(si, 1.1f) &&
                co * co + si * si > 0.9f && co * co + si * si < 1.1f) c->vf6 |= 2;
            /* 4.4.0-rc.5 (Batch 2h task B): yaw rate DERIVED from the heading angle
             * change between two ticks on the sim clock. Pauses (dt 0) and gaps
             * over 0.5 s give no rate. Valid bit vf6 2048. */
            {
                static float yr_prev[MAX_CARS];
                static double yr_t[MAX_CARS];
                static int yr_ok[MAX_CARS];
                double now = g_telemetry.session_time;
                c->yaw_rate = 0.0f;
                if ((c->vf6 & 2) && yr_ok[i]) {
                    double dt = now - yr_t[i];
                    float d = yr_prev[i];
                    float dy = atan2f(sinf(c->yaw - d), cosf(c->yaw - d));
                    if (dt > 0.0 && dt <= 0.5) { c->yaw_rate = (float)(dy / dt); c->vf6 |= 2048; }
                }
                if (c->vf6 & 2) { yr_prev[i] = c->yaw; yr_t[i] = now; yr_ok[i] = 1; }
                else yr_ok[i] = 0;
            }
        }
        pit_track(i, c);
        incident_scan(i, c);
    }

    for (int i = count; i < MAX_CARS; i++)
        memset(&g_telemetry.cars[i], 0, sizeof(car_telemetry_t));

    player_slot_update(count);
    g_telemetry.player_slot = g_player_slot;
    remote_origin_learn(count);
    for (int i = 0; i < count; i++) {
        volatile uint8_t *veh = VEHICLE_PTR(i);
        if (!veh || (uintptr_t)veh < 0x10000 || g_telemetry.cars[i].position <= 0) continue;
        fill_v6_channels(i, &g_telemetry.cars[i], veh);
        remote_gate(i, &g_telemetry.cars[i]);
    }
    player_chassis_roll();
    for (int i=0;i<count;i++)
        if (g_telemetry.cars[i].position > 0) lap_history_record(i,&g_telemetry.cars[i]);
    compute_v4_features(count);   /* radar, live sector delta, fuel model, DOTD, ... */

    {
        int leader_lap = 0, leader_finished = 0;
        for (int i = 0; i < count; i++) {
            car_telemetry_t *c = &g_telemetry.cars[i];
            if (c->position == 1) {
                leader_lap = c->lap_number;
                leader_finished = (c->finish_state == 1);
            }
        }
        session_restart_check(leader_lap);
        /* Sector flags after the car loop (needs leader_lap for the bulletin),
         * then fold the result back onto each car. */
        flags_read(leader_lap);
        for (int i = 0; i < count; i++)
            flags_apply_to_car(&g_telemetry.cars[i]);
        incident_scan_session(g_telemetry.race_active, g_telemetry.total_laps, leader_lap, leader_finished);
    }

    /* Engineering trace recording (owned mirrors only; see ledger). */
    trace_update(count);
    /* rc.11 weather: session weather history for the Weather console page. */
    weather_update();

    LeaveCriticalSection(&g_telemetry_cs);
}

/* ========================================================================
 * JSON BUILDER
 * ======================================================================== */
static char *esc_json(char *dst, int dsz, const char *src) {
    int p = 0;
    const char hex[] = "0123456789abcdef";
    if (dsz <= 0) return dst;
    for (int k=0; src[k]; ++k) {
        unsigned char ch = (unsigned char)src[k];
        if (ch>=128) {
            /* Preserve valid UTF-8. Legacy game/file byte strings also carry
             * Windows-1252 names: escape invalid UTF-8 bytes rather than
             * emitting malformed JSON (e.g. H\xE4kkinen). */
            int span=ch>=0xc2 && ch<=0xdf ? 2 : ch>=0xe0 && ch<=0xef ? 3 : ch>=0xf0 && ch<=0xf4 ? 4 : 0;
            for (int j=1;j<span;j++) if (((unsigned char)src[k+j]&0xc0)!=0x80) { span=0; break; }
            unsigned char next=(unsigned char)src[k+1];
            if ((ch==0xe0 && next<0xa0) || (ch==0xed && next>=0xa0) ||
                (ch==0xf0 && next<0x90) || (ch==0xf4 && next>=0x90)) span=0;
            if (span) {
                if (p+span>=dsz) { format_failed=1; break; }
                memcpy(dst+p,src+k,(size_t)span); p+=span; k+=span-1; continue;
            }
            static const unsigned short cp1252[32]={
                0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
                0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
                0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
            unsigned cp=ch<160 ? cp1252[ch-128] : ch;
            if (p+6>=dsz) { format_failed=1; break; }
            dst[p++]='\\'; dst[p++]='u';
            for (int shift=12;shift>=0;shift-=4) dst[p++]=hex[(cp>>shift)&15];
            continue;
        }
        int need = ch < 32 ? 6 : ((ch=='"' || ch=='\\') ? 2 : 1);
        if (p+need >= dsz) { format_failed=1; break; }
        if (ch<32) { dst[p++]='\\'; dst[p++]='u'; dst[p++]='0'; dst[p++]='0'; dst[p++]=hex[ch>>4]; dst[p++]=hex[ch&15]; }
        else { if (need==2) dst[p++]='\\'; dst[p++]=(char)ch; }
    }
    dst[p]=0; return dst;
}

/* ---- 4.4.0-rc.7 (Batch 2j task 1): tyre compound names, DERIVED ----
 * Chain, all read-only, run on the HTTP side and cached per process:
 *   live car: n = driver name, cn = car name, num = race number
 *   SEASONDATA\Vehicles\<team>\<car>\*.veh: Driver= (a trailing " (2002)" is
 *     ignored), Team=, Number=, HDVehicle= (file in the same folder)
 *   that .hdv: TireBrand=<brand> (no extension)
 *   SEASONDATA\Vehicles\<brand>.tbc: [COMPOUND] Name="..." in order; index = tc
 * Two candidate brands for one car give no name, and the viewer falls back to
 * C<tc>. The file layout is FACT; the index-to-name mapping is DERIVED until a
 * live tyre-choice check (Claude, game closed). */
#define TYRE_MAX_VEH     600
#define TYRE_MAX_BRANDS  24
#define TYRE_MAX_NAMES   8
#define TYRE_NAME_LEN    48
#define TYRE_FILE_CAP    65536
#define TYRE_MAX_DIRS    400

typedef struct {
    char nd[NAME_LEN];      /* normalised driver name */
    char nt[NAME_LEN];      /* normalised team name */
    int  number;
    int  year;              /* rc.9: season year from the car folder (2002_EJ12), 0 = unknown */
    int  brand;             /* index into g_tyre_brand, -1 = none */
} tyre_veh_t;

typedef struct {
    char brand[64];
    int  n;
    char name[TYRE_MAX_NAMES][TYRE_NAME_LEN];
} tyre_brand_t;

static tyre_veh_t   g_tyre_veh[TYRE_MAX_VEH];
static int          g_tyre_veh_n = -1;          /* -1 = not scanned yet */
static tyre_brand_t g_tyre_brand[TYRE_MAX_BRANDS];
static int          g_tyre_brand_n = 0;
static int          g_tyre_car_brand[MAX_CARS]; /* -1 = no names for this car */
static char         g_tyre_snap[MAX_CARS][2][NAME_LEN];
static int          g_tyre_snap_num[MAX_CARS];
static int          g_tyre_snap_n = -1;         /* car count of the last mapping */

/* Full path = folder of the game executable + rel (same anchor as the AIW read). */
static int tyre_game_path(const char *rel, char *out, size_t sz) {
    char full[MAX_PATH];
    if (!GetModuleFileNameA(NULL, full, sizeof full)) return 0;
    char *cut = strrchr(full, '\\');
    if (!cut) return 0;
    cut[1] = '\0';
    if (strlen(full) + strlen(rel) + 1 >= sizeof full) return 0;
    strcat(full, rel);
    if (strlen(full) + 1 > sz) return 0;
    memcpy(out, full, strlen(full) + 1);
    return 1;
}

/* Reads a small text file (capped) into a malloc'd NUL-terminated buffer. */
static char *tyre_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = (char *)malloc(TYRE_FILE_CAP + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, TYRE_FILE_CAP, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* "Key = value" with a case-insensitive key. Quoted values keep the text
 * inside the quotes; unquoted values stop at "//", CR, LF or the end. */
static int tyre_kv(const char *line, const char *key, char *out, size_t sz) {
    const char *p = line;
    size_t kl = strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    if (_strnicmp(p, key, kl) != 0) return 0;
    p += kl;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    size_t k = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && *p != '\r' && *p != '\n' && k + 1 < sz) out[k++] = *p++;
    } else {
        while (*p && !(p[0] == '/' && p[1] == '/') && *p != '\r' && *p != '\n' && k + 1 < sz) out[k++] = *p++;
        while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '\t')) k--;
    }
    out[k] = '\0';
    return 1;
}

/* First line of a whole text buffer that matches tyre_kv. */
static int tyre_text_kv(const char *txt, const char *key, char *out, size_t sz) {
    const char *p = txt;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        char line[512];
        size_t n = (size_t)(e - p);
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        if (tyre_kv(line, key, out, sz)) return 1;
        p = *e ? e + 1 : e;
    }
    return 0;
}

/* Lower-case letters and digits only. A trailing " (2002)" is dropped first. */
static void tyre_norm(const char *src, char *out, size_t sz) {
    size_t n = strlen(src);
    if (n > 0 && src[n - 1] == ')') {
        size_t i = n - 1;
        while (i > 0 && src[i - 1] >= '0' && src[i - 1] <= '9') i--;
        if (i < n - 1 && i > 0 && src[i - 1] == '(') n = i - 1;
    }
    size_t k = 0;
    for (size_t j = 0; j < n && k + 1 < sz; j++) {
        char ch = src[j];
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[k++] = ch;
    }
    out[k] = '\0';
}

/* Index of a brand in g_tyre_brand, loading its [COMPOUND] names once. */
static int tyre_brand_get(const char *brand) {
    for (int b = 0; b < g_tyre_brand_n; b++)
        if (strcmp(g_tyre_brand[b].brand, brand) == 0) return b;
    if (g_tyre_brand_n >= TYRE_MAX_BRANDS || !brand[0]) return -1;
    char rel[200], path[MAX_PATH];
    snprintf(rel, sizeof rel, "SEASONDATA\\Vehicles\\%s.tbc", brand);
    if (!tyre_game_path(rel, path, sizeof path)) return -1;
    int b = g_tyre_brand_n++;
    tyre_brand_t *tb = &g_tyre_brand[b];
    snprintf(tb->brand, sizeof tb->brand, "%s", brand);
    tb->n = 0;
    char *txt = tyre_read_file(path);
    if (!txt) return b;
    int in = 0;
    const char *p = txt;
    while (*p && tb->n < TYRE_MAX_NAMES) {
        char line[512];
        size_t k = 0;
        while (*p && *p != '\n' && k + 1 < sizeof line) line[k++] = *p++;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        line[k] = '\0';
        const char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '[') { in = _strnicmp(s, "[COMPOUND]", 10) == 0; continue; }
        if (in) {
            char v[TYRE_NAME_LEN];
            if (tyre_kv(s, "Name", v, sizeof v)) {
                snprintf(tb->name[tb->n], TYRE_NAME_LEN, "%s", v);
                tb->n++;
                in = 0;
            }
        }
    }
    free(txt);
    return b;
}

/* One .veh file: Driver, Team, Number, its .hdv (same folder) and TireBrand. */
static void tyre_read_veh(const char *path) {
    if (g_tyre_veh_n >= TYRE_MAX_VEH) return;
    char *txt = tyre_read_file(path);
    if (!txt) return;
    char drv[NAME_LEN] = "", team[NAME_LEN] = "", hd[MAX_PATH] = "", num[32] = "";
    tyre_text_kv(txt, "Driver", drv, sizeof drv);
    tyre_text_kv(txt, "Team", team, sizeof team);
    tyre_text_kv(txt, "Number", num, sizeof num);
    tyre_text_kv(txt, "HDVehicle", hd, sizeof hd);
    free(txt);
    if (!drv[0] || !hd[0]) return;
    char hdv[2048];
    snprintf(hdv, sizeof hdv, "%s", path);
    char *slash = strrchr(hdv, '\\');
    if (!slash) return;
    slash[1] = '\0';
    if (strlen(hdv) + strlen(hd) + 1 >= sizeof hdv) return;
    strcat(hdv, hd);
    char *hx = tyre_read_file(hdv);
    if (!hx) return;
    char brand[64] = "";
    tyre_text_kv(hx, "TireBrand", brand, sizeof brand);
    free(hx);
    if (!brand[0]) return;
    tyre_veh_t *v = &g_tyre_veh[g_tyre_veh_n++];
    tyre_norm(drv, v->nd, sizeof v->nd);
    tyre_norm(team, v->nt, sizeof v->nt);
    v->number = atoi(num);
    /* rc.9: the same driver often appears in several seasons (Alonso 2001
     * Minardi #21 and 2002 Renault #21), so keep the folder year to break ties. */
    v->year = 0;
    {
        char *e = strrchr(hdv, '\\');
        if (e) {
            *e = '\0';
            char *f = strrchr(hdv, '\\');
            f = f ? f + 1 : hdv;
            if (f[0] >= '1' && f[0] <= '2' && f[1] >= '0' && f[1] <= '9' &&
                f[2] >= '0' && f[2] <= '9' && f[3] >= '0' && f[3] <= '9')
                v->year = (f[0]-'0')*1000 + (f[1]-'0')*100 + (f[2]-'0')*10 + (f[3]-'0');
        }
    }
    v->brand = tyre_brand_get(brand);
}

/* Scans SEASONDATA\Vehicles\<team>\<car>\*.veh once per process (bounded). */
static void tyre_walk(void) {
    char base[1024], pat[2048];
    g_tyre_veh_n = 0;
    if (!tyre_game_path("SEASONDATA\\Vehicles\\", base, sizeof base)) return;
    snprintf(pat, sizeof pat, "%s*", base);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    int dirs = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        char team[MAX_PATH];
        snprintf(team, sizeof team, "%s", fd.cFileName);
        snprintf(pat, sizeof pat, "%s%s\\*", base, team);
        WIN32_FIND_DATAA cd;
        HANDLE ch = FindFirstFileA(pat, &cd);
        if (ch == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(cd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || cd.cFileName[0] == '.') continue;
            if (++dirs > TYRE_MAX_DIRS) break;
            snprintf(pat, sizeof pat, "%s%s\\%s\\*.veh", base, team, cd.cFileName);
            WIN32_FIND_DATAA vf;
            HANDLE vh = FindFirstFileA(pat, &vf);
            if (vh == INVALID_HANDLE_VALUE) continue;
            do {
                if (vf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                if (g_tyre_veh_n >= TYRE_MAX_VEH) break;
                char vpath[2048];
                snprintf(vpath, sizeof vpath, "%s%s\\%s\\%s", base, team, cd.cFileName, vf.cFileName);
                tyre_read_veh(vpath);
            } while (FindNextFileA(vh, &vf));
            FindClose(vh);
        } while (FindNextFileA(ch, &cd));
        FindClose(ch);
        if (dirs > TYRE_MAX_DIRS || g_tyre_veh_n >= TYRE_MAX_VEH) break;
        snprintf(pat, sizeof pat, "%s*", base);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* Brand for one live car: driver name first (several years may share it, so
 * the race number narrows them); team plus number as the fallback. -1 = none. */
static int tyre_car_match(const char *n, const char *cn, int num) {
    char nd[NAME_LEN], nt[NAME_LEN];
    int cand[TYRE_MAX_VEH];
    int nc = 0;
    tyre_norm(n, nd, sizeof nd);
    tyre_norm(cn, nt, sizeof nt);
    if (nd[0])
        for (int v = 0; v < g_tyre_veh_n && nc < TYRE_MAX_VEH; v++)
            if (strcmp(g_tyre_veh[v].nd, nd) == 0) cand[nc++] = v;
    if (nc > 1 && num > 0) {
        int k = 0;
        for (int j = 0; j < nc; j++)
            if (g_tyre_veh[cand[j]].number == num) cand[k++] = cand[j];
        if (k > 0) nc = k;
    }
    /* rc.9: the live car name carries the season ("2002 Jordan"); drop
     * candidates from other seasons, which also rejects a player whose name
     * matches an old car (Matthias Steppe in 1998_jordan08.veh). */
    int yr = (cn[0] >= '1' && cn[0] <= '2') ? atoi(cn) : 0;
    if (yr >= 1900 && yr <= 2100 && nc > 0) {
        int k = 0;
        for (int j = 0; j < nc; j++)
            if (g_tyre_veh[cand[j]].year == yr) cand[k++] = cand[j];
        nc = k;
    }
    if (nc == 0 && nt[0] && num > 0)
        for (int v = 0; v < g_tyre_veh_n && nc < TYRE_MAX_VEH; v++)
            if (g_tyre_veh[v].number == num &&
                (strcmp(g_tyre_veh[v].nt, nt) == 0 ||
                 (yr >= 1900 && g_tyre_veh[v].year == yr &&
                  strstr(nt, g_tyre_veh[v].nt) != NULL && g_tyre_veh[v].nt[0])))
                cand[nc++] = v;
    /* A custom player name/number may have no .veh record. The team's
     * same-season cars still establish its tyre brand when they all agree.
     * Keep the existing ambiguity refusal below if they do not. */
    if (nc == 0 && nt[0] && yr >= 1900)
        for (int v = 0; v < g_tyre_veh_n && nc < TYRE_MAX_VEH; v++)
            if (g_tyre_veh[v].year == yr && g_tyre_veh[v].nt[0] &&
                strstr(nt, g_tyre_veh[v].nt) != NULL)
                cand[nc++] = v;
    if (nc == 0) return -1;
    int brand = g_tyre_veh[cand[0]].brand;
    for (int j = 1; j < nc; j++)
        if (g_tyre_veh[cand[j]].brand != brand) return -1;
    return brand;
}

/* ---- TAPE2 N1 (2026-10-15): driver names of a season from its .veh files (DERIVED: race number + season year) ----
 * The folders are those of tyre_walk (SEASONDATA\Vehicles\<team>\<year>_<car>\*.veh), but every file with a Driver and a
 * Number is kept (tyre_walk needs a TireBrand). Read-only, once per process. TAPE2_SEASON_ROOT (environment variable; the
 * runtime test and the review harness only) names a SEASONDATA folder to read instead of the game's. A race number with
 * two different driver names in one season gives no name: the game files do this (Tyrrell 1997 has Verstappen and Salo
 * both on Number=18). Keys are case-insensitive; a trailing " (1997)" is dropped from the driver. */
#define VEHN_MAX 800
#define VEHN_DIG(c) ((c) >= '0' && (c) <= '9')
typedef struct { int year, number; char name[NAME_LEN], team[NAME_LEN]; } vehn_t;
static vehn_t g_vehn[VEHN_MAX];
static int    g_vehn_n = -1;      /* -1 = not scanned yet */

/* Copies src into out (cap bytes, always terminated). */
static void vehn_copy(char *out, size_t cap, const char *src) {
    size_t L = strnlen(src, cap - 1);
    memcpy(out, src, L);
    out[L] = '\0';
}

/* Driver or team text without a trailing " (1997)" and without trailing blanks. */
static void vehn_clean(const char *src, char *out, size_t cap) {
    size_t n = strlen(src);
    if (n > 0 && src[n - 1] == ')') {
        size_t i = n - 1;
        while (i > 0 && VEHN_DIG(src[i - 1])) i--;
        if (i < n - 1 && i >= 2 && src[i - 1] == '(' && src[i - 2] == ' ') n = i - 2;
    }
    while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\t')) n--;
    if (n > cap - 1) n = cap - 1;
    memcpy(out, src, n);
    out[n] = '\0';
}

/* Season year from a car folder name ("1997_Jordan197"): four digits, then "_" or the end. 0 when it is not one. */
static int vehn_folder_year(const char *f) {
    int y;
    if (!(VEHN_DIG(f[0]) && VEHN_DIG(f[1]) && VEHN_DIG(f[2]) && VEHN_DIG(f[3]))) return 0;
    if (f[4] != '\0' && f[4] != '_') return 0;
    y = (f[0] - '0') * 1000 + (f[1] - '0') * 100 + (f[2] - '0') * 10 + (f[3] - '0');
    return (y >= 1950 && y <= 2099) ? y : 0;
}

/* One .veh file: kept when it has a driver, a race number and a season year. */
static void vehn_read(const char *path, int year) {
    char *txt;
    char drv[NAME_LEN * 2] = "", team[NAME_LEN * 2] = "", num[32] = "";
    vehn_t t;
    if (g_vehn_n >= VEHN_MAX || year <= 0) return;
    txt = tyre_read_file(path);
    if (!txt) return;
    tyre_text_kv(txt, "Driver", drv, sizeof drv);
    tyre_text_kv(txt, "Team", team, sizeof team);
    tyre_text_kv(txt, "Number", num, sizeof num);
    free(txt);
    memset(&t, 0, sizeof t);
    t.number = atoi(num);
    vehn_clean(drv, t.name, sizeof t.name);
    if (t.number <= 0 || !t.name[0]) return;
    vehn_clean(team, t.team, sizeof t.team);
    t.year = year;
    g_vehn[g_vehn_n++] = t;
}

/* Scans SEASONDATA\Vehicles\<team>\<year>_<car>\*.veh once per process (bounded). */
static void vehn_walk(void) {
    char base[1024], pat[2048], root[MAX_PATH], team[MAX_PATH], vpath[2048];
    WIN32_FIND_DATAA fd, cd, vf;
    HANDLE h, ch, vh;
    DWORD got;
    int dirs = 0;
    g_vehn_n = 0;
    got = GetEnvironmentVariableA("TAPE2_SEASON_ROOT", root, sizeof root);
    if (got > 0 && got < sizeof root && root[0]) {
        size_t L = strlen(root);
        if (L + 16 >= sizeof base) return;
        memcpy(base, root, L);
        base[L] = '\0';
        if (root[L - 1] != '\\') strcat(base, "\\");
        strcat(base, "Vehicles\\");
    } else if (!tyre_game_path("SEASONDATA\\Vehicles\\", base, sizeof base)) {
        return;
    }
    snprintf(pat, sizeof pat, "%s*", base);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        snprintf(team, sizeof team, "%s", fd.cFileName);
        snprintf(pat, sizeof pat, "%s%s\\*", base, team);
        ch = FindFirstFileA(pat, &cd);
        if (ch == INVALID_HANDLE_VALUE) continue;
        do {
            int year;
            if (!(cd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || cd.cFileName[0] == '.') continue;
            if (++dirs > TYRE_MAX_DIRS) break;
            year = vehn_folder_year(cd.cFileName);
            snprintf(pat, sizeof pat, "%s%s\\%s\\*.veh", base, team, cd.cFileName);
            vh = FindFirstFileA(pat, &vf);
            if (vh == INVALID_HANDLE_VALUE) continue;
            do {
                if (vf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                snprintf(vpath, sizeof vpath, "%s%s\\%s\\%s", base, team, cd.cFileName, vf.cFileName);
                vehn_read(vpath, year);
            } while (g_vehn_n < VEHN_MAX && FindNextFileA(vh, &vf));
            FindClose(vh);
        } while (FindNextFileA(ch, &cd));
        FindClose(ch);
        if (dirs > TYRE_MAX_DIRS) break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* Season year of a recording stem: the first four-digit run that starts with 19 or 20 and stands alone
 * ("AUSTRALIA 1997 - CO-OP - Race - 2026-10-14 20-17_2" gives 1997). 0 when there is none. */
static int vehn_stem_year(const char *stem) {
    size_t n = strlen(stem);
    for (size_t i = 0; i + 4 <= n; i++) {
        int ok = (i == 0 || !VEHN_DIG(stem[i - 1])) && (i + 4 == n || !VEHN_DIG(stem[i + 4]));
        for (size_t k = 0; ok && k < 4; k++) ok = VEHN_DIG(stem[i + k]) ? 1 : 0;
        if (ok && ((stem[i] == '1' && stem[i + 1] == '9') || (stem[i] == '2' && stem[i + 1] == '0')))
            return (stem[i] - '0') * 1000 + (stem[i + 1] - '0') * 100 + (stem[i + 2] - '0') * 10 + (stem[i + 3] - '0');
    }
    return 0;
}

/* Name and team of race number num in season year. 1 when every file with that number and year has the same driver name
 * (the team is the first one's); 0 otherwise: no year, no number, no file, or two different driver names. */
static int vehn_lookup(int year, int num, char *name, size_t nsz, char *team, size_t tsz) {
    int found = -1;
    if (year <= 0 || num <= 0 || nsz == 0 || tsz == 0) return 0;
    if (g_vehn_n < 0) vehn_walk();
    for (int i = 0; i < g_vehn_n; i++) {
        if (g_vehn[i].year != year || g_vehn[i].number != num) continue;
        if (found < 0) found = i;
        else if (strcmp(g_vehn[i].name, g_vehn[found].name) != 0) return 0;
    }
    if (found < 0) return 0;
    vehn_copy(name, nsz, g_vehn[found].name);
    vehn_copy(team, tsz, g_vehn[found].team);
    return 1;
}

/* Forces the next lookup to scan again (the runtime test changes TAPE2_SEASON_ROOT). */
static void vehn_reset(void) { g_vehn_n = -1; }

/* Called at the top of json_telemetry (HTTP thread only, never the game thread).
 * Snapshots the car names under g_telemetry_cs and re-maps only when they change. */
/* REPLAY 2.3: the mapping of the tyre brands from the car names. The live path snapshots the names (tyre_refresh); a tape
 * frame passes the names of the recording. No lock here: the names are copies. */
static void tyre_refresh_names(int cnt, char nm[MAX_CARS][NAME_LEN], char cn[MAX_CARS][NAME_LEN], const int *num);
static void tyre_refresh(void) {
    char nm[MAX_CARS][NAME_LEN], cn[MAX_CARS][NAME_LEN];
    int num[MAX_CARS];
    int cnt;
    EnterCriticalSection(&g_telemetry_cs);
    cnt = g_telemetry.vehicle_count;
    if (cnt > MAX_CARS) cnt = MAX_CARS;
    if (cnt < 0) cnt = 0;
    for (int i = 0; i < cnt; i++) {
        snprintf(nm[i], NAME_LEN, "%s", g_telemetry.cars[i].name);
        snprintf(cn[i], NAME_LEN, "%s", g_telemetry.cars[i].car_name);
        num[i] = g_telemetry.cars[i].race_num;
    }
    LeaveCriticalSection(&g_telemetry_cs);
    tyre_refresh_names(cnt, nm, cn, num);
}

static void tyre_refresh_names(int cnt, char nm[MAX_CARS][NAME_LEN], char cn[MAX_CARS][NAME_LEN], const int *num) {
    if (g_tyre_veh_n < 0) tyre_walk();
    if (g_tyre_snap_n < 0)
        for (int i = 0; i < MAX_CARS; i++) g_tyre_car_brand[i] = -1;
    int same = (cnt == g_tyre_snap_n);
    for (int i = 0; same && i < cnt; i++)
        if (num[i] != g_tyre_snap_num[i] || strcmp(nm[i], g_tyre_snap[i][0]) != 0 ||
            strcmp(cn[i], g_tyre_snap[i][1]) != 0)
            same = 0;
    if (same) return;
    for (int i = 0; i < cnt; i++) {
        /* bounded memcpy copies: gcc -Werror flags snprintf/strncpy here as
         * possible truncation once tyre_car_match is inlined (rc.9). */
        for (int f = 0; f < 2; f++) {
            const char *src = f ? cn[i] : nm[i];
            size_t L = strnlen(src, NAME_LEN - 1);
            memcpy(g_tyre_snap[i][f], src, L); g_tyre_snap[i][f][L] = 0;
        }
        g_tyre_snap_num[i] = num[i];
        g_tyre_car_brand[i] = tyre_car_match(nm[i], cn[i], num[i]);
    }
    g_tyre_snap_n = cnt;
}

/* Escaped JSON text of the compound name for car `car` and index `tc`, or "". */
static void tyre_name_for(int car, int tc, char *out, size_t sz) {
    out[0] = '\0';
    if (car < 0 || car >= MAX_CARS) return;
    int b = g_tyre_car_brand[car];
    if (b < 0 || b >= g_tyre_brand_n || tc < 0 || tc >= g_tyre_brand[b].n) return;
    esc_json(out, (int)sz, g_tyre_brand[b].name[tc]);
}

/* 1.1.0 "pch" object: player chassis channels (vanilla full physics). Wheel
 * arrays FL,FR,RL,RR; -999 = not available. v=0 means no driven session. */
static int json_pch_from(char *o, int n, const telemetry_t *t) {
    int q = 0;
#define PCH4(name, a, fmt) q += _snprintf(o + q, n - q, "\"" name "\":[" fmt "," fmt "," fmt "," fmt "],", a[0], a[1], a[2], a[3]); if (q < 0 || q >= n) return -1;
    q += _snprintf(o + q, n - q, "{\"v\":%d,", t->pch_valid); if (q < 0 || q >= n) return -1;
    PCH4("rh", t->pch_ride_mm, "%.2f")
    PCH4("sus", t->pch_susp_mm, "%.2f")
    PCH4("tdf", t->pch_tdefl_mm, "%.2f")
    PCH4("pr", t->pch_press, "%.1f")
    PCH4("prs", t->pch_press_set, "%.1f")
    PCH4("gr", t->pch_grip, "%.4f")
    PCH4("opt", t->pch_opt_c, "%.1f")
    PCH4("cam", t->pch_camber_deg, "%.3f")
    PCH4("ws", t->pch_spin, "%.2f")
    PCH4("roll", t->pch_roll, "%.3f")
    PCH4("disc", t->pch_disc_mm, "%.4f")
    PCH4("discf", t->pch_disc_fail_mm, "%.4f")
    PCH4("bend", t->pch_bend, "%.4f")
    PCH4("bent", t->pch_bent, "%d")
    PCH4("det", t->pch_detach, "%d")
    PCH4("inr", t->pch_in_raw, "%.3f")
    PCH4("inu", t->pch_in_used, "%.3f")
#undef PCH4
    q += _snprintf(o + q, n - q,
        "\"bot\":%d,\"cmp\":%d,\"ewr\":%.4f,\"fuel\":%.3f,\"rpm\":%.0f,\"crpm\":%.0f,"
        "\"oilp\":%.1f,\"bias\":%.4f,\"drag\":%.4f,\"lift\":%.4f,\"dragu\":%.4f}",
        t->pch_bottom, t->pch_compound, t->pch_eng_wear, t->pch_fuel, t->pch_rpm, t->pch_clutch_rpm,
        t->pch_oil_press, t->pch_bias, t->pch_drag, t->pch_lift, t->pch_drag_used);
    if (q < 0 || q >= n) return -1;
    return q;
}

/* REPLAY 2.3 (TAPE 2.3): the body of the live /telemetry document. Reads only its arguments and t (no lock, no
 * global capture state): seq = capture_sequence, gen = capture_generation, age_ms = sample age, wp_count = the
 * memory-read waypoint count, lap_dropped = g_lap_history_dropped. json_telemetry() supplies them under the lock. */
static int json_telemetry_from(char *buf, int bufsz, const telemetry_t *t, unsigned seq, unsigned gen,
                               unsigned long age_ms, int wp_count, const int *lap_dropped) {
    int p = 0;
    char track_escaped[sizeof(t->track_name)*6+1];
    esc_json(track_escaped,sizeof track_escaped,t->track_name);

    p += _snprintf(buf,bufsz,"{\"version\":\"%s\",\"sample_sequence\":%u,\"sample_age_ms\":%lu,\"session_generation\":%u,",TELEMETRY_VERSION,seq,age_ms,gen);

    /* Sector flag strip: one digit per track segment, index 0 = start line,
     * digits are the raw 6-bit state values (0 green, 2 yellow). Compact on
     * purpose — this ships 10x/second. */
    char sg[MAX_SEGMENTS + 1];
    {
        int n = t->seg_count;
        if (n < 0) n = 0;
        if (n > MAX_SEGMENTS) n = MAX_SEGMENTS;
        for (int i = 0; i < n; i++) {
            int s = t->seg_state[i];
            sg[i] = (s >= 0 && s <= 9) ? (char)('0' + s) : '?';
        }
        sg[n] = '\0';
    }

    char pchbuf[3072];
    if (json_pch_from(pchbuf, sizeof pchbuf, t) < 0) strcpy(pchbuf, "{\"v\":0}");
    p += _snprintf(buf + p, bufsz - p,
        /* tw is now REAL wetness (weather struct on-line wetness), not the 0x7CF3A4
         * grip factor that sat at a constant 0.50 in dry sessions. tcf/tgf are the
         * raw floats, exposed so the Full Debug tab can show them unmassaged. */
        "\"ty\":%d,\"ph\":%d,\"ac\":%d,\"acr\":%d,\"tcf\":%.3f,\"tgf\":%.3f,\"tw\":%.3f,"
        "\"tl\":%d,\"vc\":%d,\"cm\":%d,\"rm\":%d,\"pi\":%d,\"ps\":%d,\"ai\":%d,\"rt\":%.1f,"
        "\"st\":%.1f,\"tk\":%lu,\"wc\":%d,\"at\":%.1f,\"trkt\":%.1f,\"onw\":%.3f,\"offw\":%.3f,"
        /* btmp = player brake temps (C), one per wheel, order FL,FR,RL,RR
         * (DERIVED). ttmp = player tyre temps (C), 4 wheels x 3 carcass
         * zones, same wheel order. -999 = not simulated / no session. */
        "\"sg\":\"%s\",\"sgn\":%d,\"sgl\":%.2f,\"trkl\":%.1f,\"yc\":%d,"
        "\"btmp\":[%.1f,%.1f,%.1f,%.1f],"
        "\"ttmp\":[[%.1f,%.1f,%.1f],[%.1f,%.1f,%.1f],[%.1f,%.1f,%.1f],[%.1f,%.1f,%.1f]],"
        "\"trd\":[%.4f,%.4f,%.4f,%.4f],\"bls\":[%d,%d,%d,%d],"
        /* player oil/water as the in-car gauge shows them — see
         * ADDR_OIL_TEMP_HUD. The per-car "ot"/"wt" below are a different,
         * unverified quantity; prefer these for the player. */
        "\"otp\":%.1f,\"wtp\":%.1f,\"pel\":%.1f,\"pch\":%s,"
        /* Timing sectors: cumulative metres from the start line, straight out
         * of the track's .AIW. sc1/sc2 are the two boundaries, lapm the lap
         * length. All zero = AIW not read, and the UI must then say "unknown"
         * rather than fall back to equal thirds. */
        "\"sc1\":%.1f,\"sc2\":%.1f,\"lapm\":%.1f,\"trk\":\"%s\",\"lhlim\":%d,"
        /* Fastest lap of the session (any car) and Driver of the Day —
         * both computed in compute_v4_features, see its header comment for
         * exactly what each is and is not. flapid/dotdid are -1 when unset. */
        "\"flapt\":%.3f,\"flapid\":%d,\"dotdid\":%d,\"dotdsc\":%d,"
        "\"c\":[",
        t->session_type, t->race_phase,
        t->race_active, t->race_active_raw,
        t->track_condition, t->track_grip_factor,
        t->on_wetness, t->total_laps,
        t->vehicle_count,
        (t->race_mode == RACE_MODE_CHAMPIONSHIP) ? 1 : 0,
        t->race_mode,
        t->player_index, t->player_slot, t->ai_difficulty,
        t->race_time_threshold, t->session_time, t->tick,
        wp_count,
        t->ambient_temp, t->track_temp,
        t->on_wetness, t->off_wetness,
        sg, t->seg_count, t->seg_len,
        t->track_len, t->yellow_count,
        t->brake_temp[0], t->brake_temp[1],
        t->brake_temp[2], t->brake_temp[3],
        t->tire_temp[0][0], t->tire_temp[0][1], t->tire_temp[0][2],
        t->tire_temp[1][0], t->tire_temp[1][1], t->tire_temp[1][2],
        t->tire_temp[2][0], t->tire_temp[2][1], t->tire_temp[2][2],
        t->tire_temp[3][0], t->tire_temp[3][1], t->tire_temp[3][2],
        t->tread[0], t->tread[1], t->tread[2], t->tread[3],
        t->blister[0], t->blister[1],
        t->blister[2], t->blister[3],
        t->oil_temp_hud, t->water_temp_hud, t->player_engine_life_s, pchbuf,
        t->sec1, t->sec2, t->lap_len,
        track_escaped, MAX_LAP_HISTORY,
        t->fastest_lap_time, t->fastest_lap_car,
        t->dotd_car, t->dotd_score);

    int leader_lap = 0, cnt = t->vehicle_count;
    if (cnt > MAX_CARS) cnt = MAX_CARS;
    int first = 1;

    for (int i = 0; i < cnt; i++) {
        const car_telemetry_t *c = &t->cars[i];
        if (c->position <= 0) continue;
        if (c->lap_number > leader_lap) leader_lap = c->lap_number;

        if (!first) bounded_char(buf,bufsz,&p,',');
        first = 0;

        char esc[NAME_LEN * 6 + 1]; esc_json(esc, sizeof(esc), c->name);
        char ecn[NAME_LEN * 6 + 1]; esc_json(ecn, sizeof(ecn), c->car_name);
        char etcn[TYRE_NAME_LEN * 6 + 1];
        tyre_name_for(i, c->tire_choice, etcn, sizeof etcn);

        /* Fix 3 (BACKEND 2 review, api_version 2): player steering stays
         * explicitly unavailable with a machine-readable reason until the
         * live steering check passes; AI steering (0x3580 AI input factor)
         * keeps its documented valid path. */
        const char *steerwhy = (c->car_valid & 8) ? "" :
            c->controller_role == CTRL_REMOTE ? "remote car (CO-OP): no valid network steer in this sample" :
            (c->ai_control ? "AI steering channel invalid in this sample"
                           : "player steering unavailable: live steering check pending (veh+0x3580 writer unproven)");

        /* compact JSON. Fix 1 (api_version 2): r/mr stay RAW rad/s
         * (documented as such); rpm/max_rpm are the converted display
         * values, so no RPM-labelled field ever carries rad/s. */
        p += _snprintf(buf + p, bufsz - p,
            "{\"id\":%d,\"l\":%d,\"p\":%d,\"n\":\"%s\",\"s\":%.1f,\"r\":%.0f,\"mr\":%.0f,"
            "\"rpm\":%.0f,\"max_rpm\":%.0f,"
            "\"g\":%d,\"bl\":%.3f,\"ll\":%.3f,\"tt\":%.3f,\"tp\":%.4f,"
            "\"bs1\":%.3f,\"bs2\":%.3f,\"ls1\":%.3f,\"ls2\":%.3f,"
            "\"fs\":%d,\"a\":%d,\"ot2\":%d,\"sgi\":%d,\"yf\":%d,\"pt\":%d,\"tc\":%d,"
            "\"th\":%.2f,\"br\":%.2f,\"rt\":%d,\"cs\":%d,"
            "\"px\":%.1f,\"pz\":%.1f,\"mx\":%.1f,\"mz\":%.1f,\"dx\":%.3f,\"dy\":%.3f,"
            "\"pn\":%d,\"cb\":%d,\"pt0\":%.1f,\"gar_team\":%d,\"gar_bay\":%d,"
            "\"et\":%.1f,\"ms\":%.1f,\"ds\":%d,\"gp\":%d,\"rc\":%d,"
            "\"qt\":%.3f,\"lr\":%d,\"cc\":%d,\"tl\":%d,"
            "\"fc\":%.4f,\"hr\":%d,\"rl\":%d,\"hd\":%d,\"dd\":%d,"
            "\"tp2\":%.4f,"
            "\"p1\":%.1f,\"p2\":%.1f,\"lp\":%.1f,\"ps\":%.1f,"
            "\"nf\":%.1f,\"pe0\":%d,\"pe1\":%d,\"pe2\":%d,"
            /* tire wear, 1.0 = fresh. 5 decimals: a whole lap only moves it ~0.0002,
             * so %.2f printed a flat 1.00 for the entire race. */
            "\"wfl\":%.5f,\"wfr\":%.5f,\"wrl\":%.5f,\"wrr\":%.5f,"
            "\"nx\":%.1f,\"nl\":%d,\"nc\":%.0f,\"ut\":%.0f,"
            "\"cn\":\"%s\",\"sc\":%.2f,\"ap\":%.4f,"
            "\"dt\":%d,\"fw\":%d,\"rw\":%d,\"dm\":%d,\"dc\":%d,\"dpr\":%d,\"u4574\":%d,\"ed\":%d,\"eav\":%.1f,"
            "\"ot\":%.1f,\"wt\":%.1f,\"fl\":%.1f,"
            "\"lfe\":%.0f,\"lfb\":%.0f,\"lfg\":%.0f,\"lfs\":%.0f,"
            "\"lpe\":%d,\"lpb\":%d,\"lpg\":%d,\"lps\":%d,\"wg\":%d,"
            /* ---- v4: all computed by us, see compute_v4_features ----
             * ga/gb = real metres to the car ahead/behind on track, gai/gbi
             *   its car id, -1 = none. lsd/lsn = live delta to personal-best
             *   in the CURRENT sector (lsn), 0 if no PB yet for that sector.
             * boxsoon/boxwhy = HEURISTIC guess only (empty why = not guessing;
             *   the REAL intention flag is cb/call_to_box above).
             * topl/toppl/tops = this-lap / previous-lap / session top speed,
             *   since the game's own mx (max_speed) field is a dead 99999
             *   sentinel. favg/faut = rolling per-lap fuel use and the laps
             *   of autonomy it implies at the current fuel level.
             * cw = car id this car's damage was attributed to this tick,
             *   HEURISTIC (proximity + same-tick damage), -1 = none. */
            "\"ga\":%.1f,\"gb\":%.1f,\"gai\":%d,\"gbi\":%d,"
            "\"lsd\":%.3f,\"lsn\":%d,"
            "\"boxsoon\":%d,\"boxwhy\":\"%s\","
            "\"topl\":%.1f,\"toppl\":%.1f,\"tops\":%.1f,"
            "\"favg\":%.3f,\"faut\":%.2f,\"cw\":%d,"
            /* v5: per-field validity (C07) and engineering channels. vf bit
             * meanings: 1 speed, 2 throttle, 4 brake, 8 steering (AI only),
             * 16 tread, 32 acceleration. steer is an input factor, not
             * degrees, and is only meaningful when bit 8 is set. tid is the
             * broker ABI-8 team hash (C15; 0 = unknown). secel/lspb/lspn are
             * the C02 corrected sector values. hlost counts laps past the
             * bounded history ring. */
            "\"vf\":%d,\"steer\":%.3f,\"steerwhy\":\"%s\",\"glat\":%.2f,\"glon\":%.2f,"
            "\"tid\":%u,\"cr\":%d,\"secel\":%.2f,\"lspb\":%.3f,\"lspn\":%d,"
            /* v6 (4.3.0) viewer channels, wheel order FL,FR,RL,RR. gv
             * vertical accel m/s2, yaw rad; ttc tyre temp C, tcc player
             * carcass C (-999 = none); vf6 validity bits (see car_telemetry_t).
             * rx = RESEARCH raw candidates [susp x4, load x4, slip x4,
             * force x4], unproven meaning and unit: never label as physics. */
            "\"vf6\":%d,\"gv\":%.2f,\"yaw\":%.4f,"
            "\"ttc\":[%.1f,%.1f,%.1f,%.1f],\"tcc\":[%.1f,%.1f,%.1f,%.1f],"
            "\"rx\":[%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g],"
            /* 1.1.0: abt = AI brake temp C [front, rear axle], aws = AI wheel
             * spin rad/s FL,FR,RL,RR; -999 for the player/remote cars. */
            "\"abt\":[%.1f,%.1f],\"aws\":[%.2f,%.2f,%.2f,%.2f],"
            "\"num\":%d,\"yr\":%.4f,\"hlost\":%d,\"tcn\":\"%s\"}",
            i, c->lap_number, c->position, esc, c->speed, c->rpm, c->max_rpm,
            c->rpm * LF_RPM_FACTOR, c->max_rpm * LF_RPM_FACTOR,
            c->gear, c->best_lap, c->last_lap, c->total_time, c->track_pos,
            c->best_s1, c->best_s2, c->last_s1, c->last_s2,
            c->finish_state, c->ai_control, c->on_track, c->segment, c->under_yellow,
            c->pitting, c->tire_choice,
            c->throttle, c->brake, c->retired, sector_of_t(t, c->lap_dist),
            c->phys_x, c->phys_z, c->track_x, c->track_z,
            c->dir_x, c->dir_y,
            c->penalties, c->call_to_box, c->pit_time_0, c->garage_team, c->garage_bay,
            c->elapsed_time, c->max_speed, c->driver_status, c->planned_position,
            c->reason_code, c->qualy_time, c->lap_of_run, c->car_conditions,
            c->track_limits, c->fuel_consumption, c->hits_in_race, c->rpm_limiter,
            c->human_drs, c->dnf_dsq,
            c->track_from_pit,
            c->pit_time_1, c->pit_time_2, c->last_pit_time, c->pit_stop_speed,
            c->next_stint_fuel, c->penalty_0, c->penalty_1, c->penalty_2,
            c->tire_wear_fl, c->tire_wear_fr, c->tire_wear_rl, c->tire_wear_rr,
            c->time_next_exit, c->number_of_laps, c->next_chosen, c->unlock_tires,
            ecn, c->speed_cap, c->ai_pace,
            c->dmg_tier, c->fw_lost, c->rw_lost, c->doomed, c->doom_cause, c->dmg_preexisting, c->raw_4574, c->eng_dead, c->eng_angvel, c->oil_temp, c->water_temp, c->fuel_level,
            c->life_eng, c->life_brk, c->life_gbx, c->life_sus,
            c->lifepct_eng, c->lifepct_brk, c->lifepct_gbx, c->lifepct_sus, c->wear_gate,
            c->gap_ahead_m, c->gap_behind_m, c->gap_ahead_id, c->gap_behind_id,
            c->live_sector_delta, c->live_sector_num,
            c->box_soon, c->box_soon_reason,
            c->top_speed_lap, c->top_speed_prev_lap, c->top_speed_session,
            c->fuel_avg_per_lap, c->fuel_autonomy_laps, c->contact_with,
            c->car_valid, c->steer, steerwhy, c->glat, c->glon,
            c->team_id, c->controller_role, c->sector_elapsed,
            c->last_sector_delta, c->last_sector_num,
            c->vf6, c->gvert, c->yaw,
            c->ttemp[0], c->ttemp[1], c->ttemp[2], c->ttemp[3],
            c->tcarc[0], c->tcarc[1], c->tcarc[2], c->tcarc[3],
            c->rx_su[0], c->rx_su[1], c->rx_su[2], c->rx_su[3],
            c->rx_ld[0], c->rx_ld[1], c->rx_ld[2], c->rx_ld[3],
            c->rx_sl[0], c->rx_sl[1], c->rx_sl[2], c->rx_sl[3],
            c->rx_fo[0], c->rx_fo[1], c->rx_fo[2], c->rx_fo[3],
            c->abt[0], c->abt[1], c->aws[0], c->aws[1], c->aws[2], c->aws[3],
            c->race_num, c->yaw_rate,
            lap_dropped[i], etcn);
    }

    p += _snprintf(buf + p, bufsz - p, "],\"ll\":%d,"
        "\"ch_tiers\":{\"num\":\"CONFIRMED race number veh+0x4710 (-1 = out of range)\","
        "\"yr\":\"DERIVED yaw rate from heading between ticks; vf6 bit 2048\","
        "\"steer\":\"AI FACT; CO-OP remote DERIVED (network steer, live check pending); player OPEN (no player writer proven; car_valid bit 8 stays clear)\","
        "\"br\":\"FACT pedal 0..1; CO-OP remote DERIVED on/off only (0 or 1.0 from one network flag; vf6 bit 4096)\","
        "\"glat\":\"FACT accel block 0x354C (player and AI); car_valid bit 32\","
        "\"glon\":\"FACT accel block 0x3554 (player and AI); car_valid bit 64\","
        "\"gv\":\"FACT AI, DERIVED player; vf6 bit 1\","
        "\"rx\":\"RESEARCH raw candidates; units unproven, not physics\"}}", leader_lap);
    return p;
}

/* The live /telemetry document: the capture counters, the waypoint count (memory read) and the lap history drops are
 * read under g_telemetry_cs, then the body runs on g_telemetry. Output is the same bytes as before the split (golden). */
static int json_telemetry(char *buf, int bufsz) {
    unsigned seq, gen;
    unsigned long age;
    int wp, dropped[MAX_CARS], i, p;
    tyre_refresh();
    EnterCriticalSection(&g_telemetry_cs);
    seq = capture_sequence;
    gen = capture_generation;
    age = capture_sequence ? GetTickCount() - capture_time : 0ul;
    wp = *(volatile int *)((volatile uint8_t *)ADDR_AI_DB_BASE + OFF_WP_COUNT);
    for (i = 0; i < MAX_CARS; i++) dropped[i] = g_lap_history_dropped[i];
    p = json_telemetry_from(buf, bufsz, &g_telemetry, seq, gen, age, wp, dropped);
    LeaveCriticalSection(&g_telemetry_cs);
    return p;
}

/* ========================================================================
 * EMBEDDED HTML DASHBOARD v3.1 — focus selector + delta + car name + AI
 * ======================================================================== */
#if 0 /* 4.4.0 batch 1: /classic is retired (404). Source kept, compiled out. */
static char *g_html_bound;   /* page copy with the bound port in the footer */
static const char *g_html =
"<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>F1C Live Telemetry</title>\n"
"<style>\n"
"*{margin:0;padding:0;box-sizing:border-box}\n"
":root{--bg:#0a0a0f;--pn:#111115;--cd:#18181f;--bd:#252530;\n"
"--red:#e10600;--gn:#00cc44;--yw:#fc0;--pu:#b000ff;\n"
"--bl:#3498db;--gy:#666;--tx:#d0d0d5;--t2:#888;--t3:#555}\n"
"body{background:var(--bg);color:var(--tx);font:13px 'Segoe UI',Arial,sans-serif;\n"
"overflow:hidden;height:100vh;width:100vw;max-width:100vw;display:flex;flex-direction:column}\n"
".hb{background:var(--pn);border-bottom:2px solid var(--red);\n"
"padding:4px 12px;display:flex;justify-content:space-between;align-items:center;\n"
"flex-wrap:wrap;min-height:36px;z-index:10;gap:8px}\n"
".hb .t{font-size:15px;font-weight:800;color:var(--red);letter-spacing:2px}\n"
".hb .i{font-size:10.5px;color:var(--t2);display:flex;gap:10px;align-items:center;flex-wrap:wrap}\n"
".hb .i b{color:var(--tx);font-weight:600}\n"
".hb .i .clk{color:var(--tx);font-family:Consolas,monospace;font-weight:600;font-size:11px}\n"
".d{display:inline-block;width:7px;height:7px;border-radius:50%;margin-right:2px}\n"
".d.lv{background:var(--gn);box-shadow:0 0 5px var(--gn)}\n"
".d.rc{background:var(--red);box-shadow:0 0 5px var(--red);animation:pls 1s infinite}\n"
".d.of{background:var(--t3)}\n"
"@keyframes pls{0%,100%{opacity:1}50%{opacity:.25}}\n"
".tag{display:inline-block;padding:1px 5px;border-radius:2px;font-size:9px;font-weight:700;margin:0 1px}\n"
".tag.drs{background:var(--gn);color:#000}\n"
".tag.dr{background:var(--red);color:#fff}\n"
".tag.pl{background:var(--bl);color:#fff}\n"
".tag.rl{background:#f0f;color:#000}\n"
".tag.go{background:#ff6600;color:#000}\n"
".mn{flex:1;display:flex;overflow:hidden}\n"
".sw{flex:2.5;min-width:520px;overflow:auto}\n"
".sw table{width:100%;max-width:1500px;margin:0 auto;border-collapse:collapse;font-size:11px;font-variant-numeric:tabular-nums}\n"
".sw th{background:var(--pn);color:var(--t2);font-weight:400;font-size:8.5px;\n"
"text-transform:uppercase;letter-spacing:.7px;padding:4px 3px;\n"
"position:sticky;top:0;z-index:2;border-bottom:1px solid var(--bd)}\n"
".sw td{padding:2px 3px;border-bottom:1px solid var(--bd);white-space:nowrap}\n"
".sw tr:hover{background:#1a1a28}\n"
".sw tr.foc{background:#0a1a2a!important;outline:1px solid var(--bl)}\n"
".sw tr.dn{opacity:.3}\n"
".sw tr.pt{background:#2a1a00}\n"
".sw tr.plr{background:#0a1a28}\n"
".sw tr.exp{background:#0f0f18}\n"
".sw tr.exp td{padding:3px 8px;font-size:9.5px;color:var(--t2)}\n"
".sw tr.exp td b{color:var(--tx)}\n"
".sw tr.exp td .row{display:flex;flex-wrap:wrap;gap:8px 16px}\n"
".sw tr.exp td .row .fi{display:flex;gap:3px}\n"
".nml{text-align:right}\n"
".sct{color:var(--pu);font-weight:600}\n"
".gn2{color:var(--gn)}\n"
".rd2{color:var(--red)}\n"
".tire{display:inline-block;padding:1px 5px;border-radius:2px;font-size:8.5px;font-weight:700}\n"
".tire.S{background:var(--red);color:#fff}\n"
".tire.M{background:var(--yw);color:#000}\n"
".tire.H{background:#fff;color:#000}\n"
".tire.W{background:var(--bl);color:#fff}\n"
".tire.I{background:#27ae60;color:#fff}\n"
".rw{flex:1;min-width:270px;max-width:420px;background:var(--pn);\n"
"border-left:1px solid var(--bd);overflow-y:auto;overflow-x:hidden;padding:10px;\n"
"display:flex;flex-direction:column;gap:8px}\n"
".foc-label{font-size:9px;color:var(--tx);text-transform:uppercase;letter-spacing:1.2px;\n"
"font-weight:700;background:var(--cd);border:1px solid var(--bd);border-left:3px solid var(--red);\n"
"border-radius:3px;padding:7px 9px}\n"
".foc-label b{color:var(--red)}\n"
".gg{display:flex;gap:8px;justify-content:center;background:var(--cd);\n"
"border:1px solid var(--bd);border-radius:5px;padding:8px 4px}\n"
".gg canvas{width:130px;height:130px}\n"
".brs{padding:8px 10px;background:var(--cd);border:1px solid var(--bd);border-radius:5px}\n"
".br{display:flex;align-items:center;margin:5px 0;font-size:9.5px}\n"
".br .l{width:30px;color:var(--t2);font-weight:700;letter-spacing:.5px}\n"
".br .fw{flex:1;height:10px;background:#0d0d12;border-radius:6px;\n"
"overflow:hidden;margin:0 7px;border:1px solid var(--bd)}\n"
".br .fl{height:100%;border-radius:6px;transition:width .2s}\n"
".fl.th{background:linear-gradient(90deg,#27ae60,#2ecc71)}\n"
".fl.br{background:linear-gradient(90deg,#c0392b,#e74c3c)}\n"
".fl.fu{background:linear-gradient(90deg,#f39c12,#e67e22)}\n"
".br .v{width:38px;text-align:right;color:var(--tx);font-family:Consolas,monospace;font-weight:600}\n"
".lps{display:flex;gap:6px;flex-wrap:wrap}\n"
".lc{flex:1;min-width:52px;background:var(--cd);border:1px solid var(--bd);border-radius:4px;\n"
"padding:6px 4px;text-align:center}\n"
".lc .vl{font-size:16px;font-weight:700;font-family:Consolas,monospace;color:var(--tx)}\n"
".lc .lb{font-size:7.5px;color:var(--t2);text-transform:uppercase;letter-spacing:.6px;margin-top:1px}\n"
".sec{font-size:9.5px;background:var(--cd);border:1px solid var(--bd);border-radius:4px;padding:8px}\n"
".sec h4{font-size:8.5px;color:var(--t2);text-transform:uppercase;\n"
"letter-spacing:1.2px;margin-bottom:5px;font-weight:700}\n"
".sr{display:flex;justify-content:space-between;margin:3px 0;color:var(--tx);\n"
"padding-bottom:3px;border-bottom:1px solid rgba(255,255,255,0.04)}\n"
".sr:last-child{border-bottom:none}\n"
".sr.pu{color:var(--pu);font-weight:700}\n"
".sr .sl{color:var(--t2);font-size:8.5px;font-weight:600;letter-spacing:.5px}\n"
".det{font-size:9.5px;background:var(--cd);border:1px solid var(--bd);border-radius:4px;padding:8px}\n"
".det h4{font-size:8.5px;color:var(--t2);text-transform:uppercase;\n"
"letter-spacing:1.2px;margin-bottom:6px;font-weight:700}\n"
".det .dr{display:flex;flex-wrap:wrap;gap:6px 14px}\n"
".det .dr .di{display:flex;gap:4px;font-size:9.5px}\n"
".det .dr .di .lk{color:var(--t3);font-weight:600}\n"
".det .dr .di .lv2{color:var(--tx);font-weight:700;font-family:Consolas,monospace}\n"
".wtr{background:var(--cd);border:1px solid var(--bd);border-radius:4px;padding:9px;display:flex;\n"
"align-items:center;gap:9px;font-size:10px}\n"
".wtr .ic{font-size:22px}\n"
".wtr .dt{color:var(--t2);line-height:1.6}\n"
".wtr .dt b{color:var(--tx);font-family:Consolas,monospace}\n"
".mw{background:var(--cd);border-radius:4px;overflow:hidden;\n"
"position:relative;height:380px}\n"
".mw canvas{cursor:grab}\n"
".mw canvas:active{cursor:grabbing}\n"
".mw canvas{width:100%;height:100%;display:block}\n"
".mw .ml{position:absolute;top:3px;left:7px;font-size:8px;color:var(--t3);\n"
"text-transform:uppercase;letter-spacing:1px}\n"
".ft{text-align:center;font-size:8.5px;color:var(--t3);padding:2px;\n"
"border-top:1px solid var(--bd)}\n"
"#cn{display:flex;justify-content:center;align-items:center;height:100vh;\n"
"color:var(--t2);font-size:13px}\n"
"#cn span{animation:pls 1.2s infinite}\n"
".cur{cursor:pointer}\n"
".cur:hover{background:#1a1a30!important}\n"
".tbar{display:flex;gap:2px;background:var(--pn);border-bottom:1px solid var(--bd);padding:0 8px}\n"
".tbar button{background:none;border:none;color:var(--t2);padding:7px 14px;font-size:10.5px;\n"
"font-weight:700;letter-spacing:.5px;text-transform:uppercase;cursor:pointer;\n"
"border-bottom:2px solid transparent}\n"
".tbar button.on{color:var(--tx);border-bottom-color:var(--red)}\n"
".rpw{padding:8px;overflow:auto;display:none;flex:1 1 auto;min-width:0}\n"
".rpw table{border-collapse:collapse;font-size:10.5px;font-variant-numeric:tabular-nums}\n"
".rpw th{background:var(--pn);color:var(--t2);font-weight:600;padding:3px 6px;\n"
"position:sticky;top:0;text-align:left;white-space:nowrap}\n"
".rpw td{padding:3px 8px;text-align:right;white-space:nowrap;font-family:Consolas,monospace}\n"
".rpw td.ln{color:var(--t2);text-align:left;font-family:inherit;position:sticky;left:0;\n"
"background:var(--pn);z-index:1;font-weight:600}\n"
"#swPace tbody tr:nth-child(even) td{background:#141419}\n"
"#swPace tbody tr:nth-child(even) td.ln{background:#1a1a20}\n"
".cardrow{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:8px}\n"
".pitboard{background:#0a0a0a;border:2px solid #333;border-radius:2px;padding:8px 12px;\n"
"font-family:Consolas,monospace;color:#ffb000;margin-bottom:8px;max-width:280px}\n"
".pitboard .pbrow{padding:2px 0;font-size:13px;letter-spacing:1px}\n"
".pitboard .pbbig{font-size:22px;font-weight:800}\n"
".cardrow>.c1{flex:1;min-width:190px;background:var(--cd);border-radius:5px;padding:8px}\n"
".cardrow>.c1 h4{font-size:8.5px;color:var(--t2);text-transform:uppercase;\n"
"letter-spacing:1px;margin-bottom:5px}\n"
".inc{font-size:9.5px;padding:3px 4px;border-bottom:1px solid var(--bd);display:flex;gap:6px}\n"
".inc .tk{color:var(--t3);font-family:Consolas,monospace;width:60px;flex-shrink:0}\n"
".inc .ty{width:60px;flex-shrink:0;font-weight:700}\n"
".inc .ty.POS{color:var(--bl)}.inc .ty.LIMITS{color:var(--yw)}\n"
".inc .ty.PENALTY{color:var(--red)}.inc .ty.DAMAGE{color:var(--red)}\n"
".inc .ty.RETIRE{color:var(--t3)}.inc .ty.PIT_IN,.inc .ty.PIT_OUT{color:var(--gn)}\n"
".inc .ty.PENALTY_TL{color:var(--red)}.inc .ty.CONTACT{color:var(--yw)}.inc .ty.BOXSOON{color:var(--bl)}\n"
".dbgraw{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:4px}\n"
".dbgraw div{background:var(--cd);border-radius:3px;padding:4px 6px;font-size:9px}\n"
".dbgraw div b{display:block;color:var(--t2);font-size:7.5px;text-transform:uppercase}\n"
".warn{background:#2a1a00;border-radius:4px;padding:6px 8px;font-size:9.5px;color:var(--yw);margin-bottom:8px}\n"
".rpw button{background:var(--cd);border:1px solid var(--bd);color:var(--tx);padding:3px 8px;\n"
"border-radius:3px;font-size:9px;cursor:pointer;margin:1px}\n"
".rpw button:hover{background:#242430;border-color:var(--red)}\n"
".rcHdr{display:flex;justify-content:space-between;align-items:center;padding:6px 8px;\n"
"background:var(--pn);border-bottom:2px solid var(--red);margin-bottom:8px;border-radius:3px 3px 0 0}\n"
".rcHdr b{color:var(--red);letter-spacing:1px;font-size:11px}\n"
".rcBul{display:flex;gap:8px;padding:6px 8px;border-bottom:1px solid var(--bd);font-size:10px;align-items:baseline}\n"
".rcBul .no{color:var(--t3);font-family:Consolas,monospace;width:28px;flex-shrink:0}\n"
".rcBul .tm{color:var(--t3);font-family:Consolas,monospace;width:46px;flex-shrink:0}\n"
".rcBul .bd{flex:1}\n"
".rcBul .bd b{color:var(--tx)}\n"
".rcBul.sev1{border-left:3px solid var(--yw)}\n"
".rcBul.sev2{border-left:3px solid var(--red)}\n"
".rcBul.sev0{border-left:3px solid var(--gn)}\n"
".rcBul.sevI{border-left:3px solid var(--bl)}\n"
"/* Tab panes: cap content width so columns hug their data instead of being smeared\n"
"   across a 2000px monitor with acres of dead space between them. */\n"
".rpw>*{max-width:1500px}\n"
".rpw .sec,.rpw .cardrow,.rpw .warn,.rpw .rcHdr{max-width:1500px}\n"
"table.dtab{width:auto;min-width:460px;max-width:1500px;border-collapse:collapse;font-size:9.5px}\n"
"table.dtab th{text-align:right;padding:4px 12px;color:var(--t2);font-size:8px;\n"
"text-transform:uppercase;letter-spacing:.8px;white-space:nowrap;border-bottom:1px solid var(--bd)}\n"
"table.dtab td{text-align:right;padding:3px 12px;white-space:nowrap;\n"
"font-family:Consolas,monospace;border-bottom:1px solid #1c1c24}\n"
"table.dtab th:first-child,table.dtab td:first-child{text-align:left;\n"
"font-family:'Segoe UI',Arial,sans-serif;font-weight:600}\n"
"table.dtab tr.cur:hover td{background:#1c1c28}\n"
"table.dtab tr.foc td{background:#0d2033}\n"
"/* driver column absorbs slack so numeric columns stay tight together */\n"
"table.tw td:nth-child(2),table.tw th:nth-child(2){width:99%}\n"
"/* ---- PIT WALL: fixed multi-pane grid, everything visible at once ---- */\n"
".pw{display:none;flex:1;min-height:0;gap:5px;padding:5px;background:#07070b;\n"
"grid-template-columns:330px 1.25fr 1fr;\n"
/* The telemetry row carries a REAL minimum, not minmax(0,...). Race Control
   grows with its message list, and against a zero-minimum neighbour the
   browser is free to collapse the telemetry pane to nothing — which is
   exactly what happened: at ~1430px wide the whole car-detail pane vanished
   between SESSION and RACE CONTROL. Panes already scroll internally
   (.pwp>.bd2 is overflow:auto), so a floor here costs nothing. */
"grid-template-rows:minmax(0,auto) minmax(240px,1fr) minmax(0,1.15fr);\n"
"grid-template-areas:'sess tower pace' 'tele tower map' 'rc tower map'}\n"
".pw.shown{display:grid}\n"
"/* maximize: one pane fills the whole grid (replaces the old separate tabs) */\n"
".pw.maxed>.pwp{display:none}\n"
".pw.maxed>.pwp.onMax{display:flex;grid-column:1/-1;grid-row:1/-1}\n"
".pwp>.hd{cursor:pointer;user-select:none}\n"
".pwp>.hd:hover{background:#1c1c24;color:var(--tx)}\n"
".pwp>.hd .xp{color:var(--t3);font-size:11px;margin-left:6px}\n"
".pwp>.hd:hover .xp{color:var(--red)}\n"
".pwp{background:var(--pn);border:1px solid var(--bd);border-radius:3px;\n"
"display:flex;flex-direction:column;min-height:0;min-width:0;overflow:hidden}\n"
".pwp>.hd{background:#141419;border-bottom:1px solid var(--bd);padding:4px 8px;\n"
"font-size:8.5px;font-weight:800;letter-spacing:1.4px;text-transform:uppercase;\n"
"color:var(--t2);display:flex;justify-content:space-between;align-items:center;flex-shrink:0}\n"
".pwp>.hd .hv{color:var(--red);font-family:Consolas,monospace}\n"
".pwp>.bd2{flex:1;min-height:0;overflow:auto;padding:5px}\n"
".pwp>.bd2.nopad{padding:0}\n"
".pw-sess{grid-area:sess}.pw-tele{grid-area:tele}.pw-rc{grid-area:rc}\n"
".pw-tower{grid-area:tower}.pw-pace{grid-area:pace}.pw-map{grid-area:map}\n"
".kv{display:grid;grid-template-columns:auto 1fr;gap:2px 8px;font-size:10px}\n"
".kv .k{color:var(--t3);font-weight:600;letter-spacing:.4px}\n"
".kv .v{color:var(--tx);font-family:Consolas,monospace;font-weight:600;text-align:right}\n"
".tstat{padding:3px 7px;border-radius:2px;font-weight:800;font-size:9px;letter-spacing:1px}\n"
".tstat.g{background:#0a3d1a;color:#2ecc71;border:1px solid #176630}\n"
".tstat.y{background:#3d3208;color:#fc0;border:1px solid #665313}\n"
".tstat.r{background:#3d0a0a;color:#ff4136;border:1px solid #661717}\n"
"table.tw{width:100%;border-collapse:collapse;font-size:10.5px;font-variant-numeric:tabular-nums}\n"
"table.tw th{background:#141419;color:var(--t3);font-size:8px;font-weight:700;\n"
"text-transform:uppercase;letter-spacing:.8px;padding:4px 4px;position:sticky;top:0;z-index:2;\n"
"border-bottom:1px solid var(--bd);text-align:right}\n"
"table.tw th:nth-child(1),table.tw th:nth-child(2){text-align:left}\n"
"table.tw td{padding:3px 4px;border-bottom:1px solid #1c1c24;text-align:right;\n"
"font-family:Consolas,monospace;white-space:nowrap}\n"
"table.tw td:nth-child(2){text-align:left;font-family:'Segoe UI',Arial,sans-serif;font-weight:600}\n"
"table.tw tr.foc td{background:#0d2033}\n"
"table.tw tr.plr td{background:#101d2e}\n"
"table.tw tr.vw td{box-shadow:inset 0 0 0 1px rgba(46,204,113,.55)}\n"
"table.tw tr.dn td{opacity:.35}\n"
"table.tw tr.pit td{background:#2a1c00}\n"
"table.tw tr:hover td{background:#1c1c28}\n"
"table.tw td .pos{display:inline-block;min-width:17px;text-align:center;font-weight:800;\n"
"border-radius:2px;padding:0 3px}\n"
".pcell{font-size:9.5px;padding:2px 4px;text-align:right;font-family:Consolas,monospace;\n"
"border-bottom:1px solid #1c1c24;white-space:nowrap}\n"
".rcfeed .rcline{font-size:9px;padding:3px 5px;border-bottom:1px solid #1c1c24;\n"
"display:flex;gap:6px;line-height:1.35}\n"
".rcfeed .rcline .lp{color:var(--t3);font-family:Consolas,monospace;flex-shrink:0;width:26px}\n"
".rcfeed .rcline.s2{border-left:2px solid var(--red)}\n"
".rcfeed .rcline.s1{border-left:2px solid var(--yw)}\n"
".rcfeed .rcline.s0{border-left:2px solid var(--gn)}\n"
".rcfeed .rcline.sI{border-left:2px solid var(--bl)}\n"
".dtag{display:inline-block;padding:0 4px;border-radius:2px;font-size:8px;font-weight:800;\n"
"color:#000;letter-spacing:.5px;vertical-align:middle}\n"
"table.tw tr.doomed td{background:#2a0808}\n"
".miniG{display:flex;gap:4px;justify-content:center}\n"
".miniG canvas{width:104px;height:104px}\n"
/* Pit Wall responsive ladder. The 3-column grid needs ~1600px: at 1438px the
   timing tower was squeezed to the point of growing its own horizontal
   scrollbar. Note these are WIDTH breakpoints, not `orientation:portrait` —
   a browser window on a portrait monitor is still a landscape VIEWPORT
   (1438x1157), so the orientation query never matched and the old rule below
   only ever restyled the legacy .mn layout, never .pw. */
/* Timing tower has 16 columns: 1 P, 2 Driver, 3 Gap, 4 Int, 5 Last, 6 Best,
   7 delta, 8 S1, 9 S2, 10 S3, 11 Spd, 12 Grd, 13 Tyr, 14 Fuel, 15 Stp, 16 St.
   Rather than let it scroll sideways (which hid Tyr/Stp/St — tire and status,
   the things you actually watch — behind a scrollbar), shed the least useful
   columns as width drops. Delta is derivable from Gap, Grd is static, Fuel is a
   rate you rarely read mid-race. */
"@media(max-width:1600px){\n"
".pw table.tw td:nth-child(7),.pw table.tw th:nth-child(7),\n"
".pw table.tw td:nth-child(12),.pw table.tw th:nth-child(12),\n"
".pw table.tw td:nth-child(14),.pw table.tw th:nth-child(14){display:none}\n"
"}\n"
/* Tighter still: sector splits go, keeping Last/Best plus tire and status. */
"@media(max-width:1150px){\n"
".pw table.tw td:nth-child(8),.pw table.tw th:nth-child(8),\n"
".pw table.tw td:nth-child(9),.pw table.tw th:nth-child(9),\n"
".pw table.tw td:nth-child(10),.pw table.tw th:nth-child(10),\n"
".pw table.tw td:nth-child(11),.pw table.tw th:nth-child(11){display:none}\n"
"}\n"
/* Same fix in the two-column layout, and Race Control loses its `auto` row:
   as `auto` it sized to its full message list and starved everything below
   it. Bounded to a fraction, it scrolls instead. */
"@media(max-width:1600px){.pw{grid-template-columns:300px minmax(0,1fr);\n"
"grid-template-rows:minmax(0,auto) minmax(240px,1.1fr) minmax(0,0.9fr) minmax(0,1fr);\n"
"grid-template-areas:'sess tower' 'tele tower' 'rc tower' 'pace map'}}\n"
/* Single column: the whole board scrolls, each pane gets full width. Tower and
   map get viewport-relative heights so they stay usable on a tall screen. */
"@media(max-width:1150px){.pw{grid-template-columns:minmax(0,1fr);\n"
"grid-template-rows:auto minmax(60vh,auto) auto minmax(45vh,auto) auto auto;\n"
"grid-template-areas:'sess' 'tower' 'tele' 'map' 'pace' 'rc';\n"
"overflow-y:auto}\n"
".pw>.pwp{min-height:0}\n"
"table.tw{font-size:12px}\n"
"table.tw td{padding:5px 4px}table.tw th{font-size:9px;padding:6px 4px}\n"
"}\n"
"@media(max-width:800px),(orientation:portrait){.mn{flex-direction:column}\n"
".rw{max-width:none;border-left:none;border-top:1px solid var(--bd);flex:none}\n"
".sw{flex:1 1 auto;min-width:0}\n"
".sw table{font-size:14px}\n"
".sw td{padding:6px 5px}.sw th{font-size:10.5px;padding:7px 5px}\n"
".gg canvas{width:170px;height:170px}\n"
".mw{height:320px}\n"
".lc .vl{font-size:20px}\n"
"}\n"
/* ---- Engineering tab (4.2.0) ---- */
".eng-bar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;font-size:10px;margin-bottom:8px}\n"
".eng-bar select,.eng-bar button{background:var(--pn);color:var(--t1);border:1px solid var(--bd);border-radius:3px;font-size:10px;padding:2px 5px}\n"
".eng-bar button{cursor:pointer}\n"
".eng-bar select,.eng-bar input{max-width:100%;min-width:0;box-sizing:border-box}\n"
".eng-stats{overflow-x:auto;overflow-wrap:anywhere}\n"
".eng-canvas{width:100%;background:#0b0e12;border:1px solid var(--bd);border-radius:4px;display:block}\n"
".eng-stats{display:flex;flex-wrap:wrap;gap:10px;margin-top:8px;font-size:10px}\n"
".eng-stats table{border-collapse:collapse;font-size:10px}\n"
".eng-stats td,.eng-stats th{border:1px solid var(--bd);padding:2px 6px;text-align:right}\n"
".eng-note{font-size:9.5px;color:var(--t3);margin:6px 0}\n"
".doss{display:none;background:var(--pn);border-top:1px solid var(--bd);padding:8px;max-height:46vh;overflow:auto}\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div id=\"sampleStatus\" style=\"position:fixed;bottom:0;left:0;z-index:9999;background:#131a22;color:#ddd;padding:3px 8px;font:12px monospace\">Web Telemetry " TELEMETRY_VERSION " - waiting for game data</div>\n"
"<div id=\"cn\"><span>Connecting...</span></div>\n"
"<div id=\"ap\" style=\"display:none;flex-direction:column;height:100vh\">\n"
"<div class=\"hb\">\n"
"<div><span class=\"t\">F1 CHALLENGE LIVE</span></div>\n"
"<div class=\"i\">\n"
"<span class=\"d of\" id=\"sd\"></span>\n"
"<span id=\"sl\">---</span>\n"
"<b>L</b><span id=\"ln\">-</span>/<span id=\"tlv\">-</span>\n"
"<span id=\"wx\"></span>\n"
"<b>C</b><span id=\"cnv\">0</span>\n"
"<span class=\"clk\" id=\"clk\"></span>\n"
"</div>\n"
"</div>\n"
"<div class=\"tbar\">\n"
"<button id=\"tbPW\" class=\"on\" onclick=\"swTab('pw')\">Pit Wall</button>\n"
"<button id=\"tbTP\" onclick=\"swTab('tp')\">Team Principal</button>\n"
"<button id=\"tbRC\" onclick=\"swTab('rc')\">Race Control</button>\n"
"<button id=\"tbEng\" onclick=\"swTab('eng')\">Engineering</button>\n"
"<a id=\"tbTV\" href=\"/viewer\" target=\"_blank\" style=\"text-decoration:none;padding:5px 11px;border:0;border-radius:4px;background:#0e7490;color:#ecfeff;font:inherit;font-weight:600;cursor:pointer\">Telemetry Viewer</a>\n"
"<a id=\"tbWB\" href=\"/workbench\" target=\"_blank\" style=\"text-decoration:none;padding:5px 11px;border:0;border-radius:4px;background:#2f4d6d;color:#e8f1ff;font:inherit;cursor:pointer\">Workbench</a>\n"
"<button id=\"tbDbg\" onclick=\"swTab('debug')\">Full Debug</button>\n"
"</div>\n"
"<div class=\"pw shown\" id=\"swPW\">\n"
"<div class=\"pwp pw-sess\" data-pane=\"sess\"><div class=\"hd\" onclick=\"pwMax('sess')\">\n"
"<span>Session</span><span class=\"hv\" id=\"pwSessT\"></span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2\" id=\"pwSess\"></div></div>\n"
"<div class=\"pwp pw-tele\" data-pane=\"tele\"><div class=\"hd\" onclick=\"pwMax('tele')\">\n"
"<span>Car Telemetry</span><span class=\"hv\" id=\"pwTeleN\"></span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2\" id=\"pwTele\"></div></div>\n"
"<div class=\"pwp pw-rc\" data-pane=\"rc\"><div class=\"hd\" onclick=\"pwMax('rc')\">\n"
"<span>Race Control</span><span class=\"hv\" id=\"pwRcN\"></span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2 nopad rcfeed\" id=\"pwRC\"></div></div>\n"
"<div class=\"pwp pw-tower\" data-pane=\"tower\"><div class=\"hd\" onclick=\"pwMax('tower')\">\n"
"<span>Timing Tower</span><span class=\"hv\" id=\"pwTowN\"></span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2 nopad\"><table class=\"tw\"><thead id=\"pwTowH\"></thead><tbody id=\"pwTowB\"></tbody></table></div></div>\n"
"<div class=\"pwp pw-pace\" data-pane=\"pace\"><div class=\"hd\" onclick=\"pwMax('pace')\">\n"
"<span>Race Pace</span><span class=\"hv\" id=\"pwPaceN\"></span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2 nopad\"><table class=\"tw\" id=\"pwPaceT\"><thead id=\"pwPaceH\"></thead><tbody id=\"pwPaceB\"></tbody></table></div></div>\n"
"<div class=\"pwp pw-map\" data-pane=\"map\"><div class=\"hd\" onclick=\"pwMax('map')\">\n"
"<span>Track Map</span><span class=\"hv\">scroll zoom &middot; drag pan &middot; click car</span><span class=\"xp\">&#9974;</span></div>\n"
"<div class=\"bd2 nopad\"><div class=\"mw\" id=\"mWPw\" style=\"height:100%;border-radius:0\">\n"
"<canvas id=\"mCPw\"></canvas></div></div></div>\n"
"</div>\n"
"<div class=\"mn\" id=\"mainArea\" style=\"display:none\">\n"
"<div class=\"rpw\" id=\"swTP\"></div>\n"
"<div class=\"rpw\" id=\"swDebug\"></div>\n"
"<div class=\"rpw\" id=\"swRC\"></div>\n"
"<div class=\"rpw\" id=\"swEng\"></div>\n"
"<div class=\"doss\" id=\"drvDoss\"></div>\n"
"<div class=\"rw\" id=\"rp\">\n"
"<div class=\"foc-label\">FOCUS: <b id=\"flbl\">P1</b> (click row to change)</div>\n"
"<div class=\"gg\">\n"
"<canvas id=\"gS\" width=\"260\" height=\"260\"></canvas>\n"
"<canvas id=\"gT\" width=\"260\" height=\"260\"></canvas>\n"
"</div>\n"
"<div class=\"brs\">\n"
"<div class=\"br\"><div class=\"l\">THR</div>\n"
"<div class=\"fw\"><div class=\"fl th\" id=\"bth\"></div></div>\n"
"<div class=\"v\" id=\"vth\">0%</div></div>\n"
"<div class=\"br\"><div class=\"l\">BRK</div>\n"
"<div class=\"fw\"><div class=\"fl br\" id=\"bbk\"></div></div>\n"
"<div class=\"v\" id=\"vbk\">0%</div></div>\n"
"<div class=\"br\"><div class=\"l\">FUEL</div>\n"
"<div class=\"fw\"><div class=\"fl fu\" id=\"bfu\"></div></div>\n"
"<div class=\"v\" id=\"vfu\">-</div></div>\n"
"</div>\n"
"<div class=\"lps\">\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vl\">-</div><div class=\"lb\">Lap</div></div>\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vg\">N</div><div class=\"lb\">Gear</div></div>\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vb\">--</div><div class=\"lb\">Best</div></div>\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vla\">--</div><div class=\"lb\">Last</div></div>\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vms\">-</div><div class=\"lb\">Top km/h</div></div>\n"
"<div class=\"lc\"><div class=\"vl\" id=\"vet\">--</div><div class=\"lb\">Elapsed</div></div>\n"
"</div>\n"
"<div class=\"sec\"><h4>Focus Sectors</h4>\n"
"<div class=\"sr\"><span class=\"sl\">S1</span><span id=\"ss1\">-</span></div>\n"
"<div class=\"sr\"><span class=\"sl\">S2</span><span id=\"ss2\">-</span></div>\n"
"<div class=\"sr\"><span class=\"sl\">S3</span><span id=\"ss3\">-</span></div>\n"
"</div>\n"
"<div class=\"det\"><h4>Focus Details</h4>\n"
"<div class=\"dr\">\n"
"<div class=\"di\"><span class=\"lk\">Car:</span><span class=\"lv2\" id=\"dcn\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Grid:</span><span class=\"lv2\" id=\"dgp\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Stops:</span><span class=\"lv2\" id=\"dst\">0</span></div>\n"
"<div class=\"di\"><span class=\"lk\">LastPit:</span><span class=\"lv2\" id=\"dlp\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Damage:</span><span class=\"lv2\" id=\"ddm\">0</span></div>\n"
"<div class=\"di\"><span class=\"lk\">TrkLim:</span><span class=\"lv2\" id=\"dtl\">0</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Hits:</span><span class=\"lv2\" id=\"dhr\">0</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Fuel:</span><span class=\"lv2\" id=\"dfc\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Stint:</span><span class=\"lv2\" id=\"dlr\">0</span></div>\n"
"<div class=\"di\"><span class=\"lk\">SpeedCap:</span><span class=\"lv2\" id=\"dsc\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">PaceMult:</span><span class=\"lv2\" id=\"dap\">-</span></div>\n"
"<div class=\"di\"><span class=\"lk\">Reason:</span><span class=\"lv2\" id=\"drc\">-</span></div>\n"
"</div></div>\n"
"<div class=\"wtr\">\n"
"<div class=\"ic\" id=\"wi\">&#9728;</div>\n"
"<div class=\"dt\">\n"
"<div id=\"wt\">Dry</div>\n"
"<div>Wet: <b id=\"ww\">0%</b> | AI: <b id=\"wai\">-</b> | <span id=\"wcm\"></span></div>\n"
"<div>Track: <b id=\"wtt\">-</b> | Air: <b id=\"wat\">-</b></div>\n"
"</div>\n"
"</div>\n"
"<div class=\"mw\" id=\"mW\">\n"
"<div class=\"ml\">Track Map &mdash; scroll to zoom, drag to pan, dbl-click to reset</div>\n"
"<canvas id=\"mC\"></canvas>\n"
"</div>\n"
"</div>\n"
"</div>\n"
"<div class=\"ft\">F1C Web Telemetry " TELEMETRY_VERSION " — port 8080 - Tailscale / LAN enabled</div>\n"
"</div>\n"
"<script>\n"
"var ap=document.getElementById('ap'),cn=document.getElementById('cn');\n"
"var LD=null;\n"
/* tireChoice is an INDEX into the compound list of the TRACK'S OWN .tbc file.
   Re-checked against F1CUILive's bytecode 2026-07-28: f1c_tbcTires parses the
   .tbc linked to the current track, reads each [COMPOUND] section's Name="..."
   and WetWeather=0/1, and getTiresDataByCompound(int) is a plain list.get(idx)
   into that per-track list.

   So there is NO fixed S/M/H/W/I enum. The old TIRE=['','S','M','H','W','I']
   table here was a guess: on any mod or track whose .tbc lists compounds in a
   different order it printed a confidently wrong letter. Removed. Until this
   DLL parses the .tbc itself, the honest render is the raw index, and the
   tooltip says where the real name lives. */
"function tireName(tc){return (tc===undefined||tc===null||tc<0)?'-':('C'+tc);}\n"
"/* Damage tiers. Backed by CONFIRMED per-part records, not guesswork:\n"
"   front wing detach is repairable (car pits and rejoins), rear wing detach is\n"
"   TERMINAL because PitStopBlockingCondition tests only the rear record and the\n"
"   game retires the car at the box. See part_detach_commentary in the corpus. */\n"
"var DMG=[{t:'',c:'',d:''},\n"
"{t:'BODYWORK',c:'#888',d:'Panel damage \\u2014 cosmetic, no stop required'},\n"
"{t:'FRONT WING',c:'#fc0',d:'Front wing detached \\u2014 will pit and REJOIN (repairable)'},\n"
"{t:'REAR WING',c:'#ff4136',d:'Rear wing detached \\u2014 car WILL RETIRE at the pit box'},\n"
"{t:'WHEEL',c:'#ff4136',d:'Wheel loss \\u2014 terminal'},\n"
"{t:'ENGINE STOP',c:'#ff4136',d:'Engine stopped: RPM 0 AND EngineAngVel 0 while the car has not finished, during a live session. The old source for this tier was the 0x4574 flag, which is NOT a seize flag \\u2014 it reads 1 on pristine cars on the grid.'}];\n"
"function dmgTag(c2){var t=c2.dt||0;if(!t)return'';var D=DMG[t];\n"
"return'<span class=\"dtag\" style=\"background:'+D.c+'\" title=\"'+D.d+'\">'+D.t+'</span>';}\n"
/* ---- track-sector flags ----------------------------------------------------
   `sg` is one digit per track segment (0 green, 2 yellow), straight from the
   game's Steward table -- the same state its own HUD flag sprite is drawn from.
   The strip is laid out start-line first; a car's own segment is boxed, and the
   segment behind it counts too because that is the game's own display rule
   (HUDFlagRendererMain tests segment idx and idx-1). */
"var FLAGZONES=0;\n"
"function flagStrip(d,fc){var sg=d.sg||'';if(!sg.length)return'';\n"
"FLAGZONES=sg.length;\n"
"var cells='';\n"
"for(var i=0;i<sg.length;i++){var st=sg.charAt(i);\n"
"var bg=st=='0'?'#1c1c24':(st=='2'?'#f5c400':'#c0392b');\n"
"var mine=(fc&&fc.sgi===i);\n"
"cells+='<span title=\"Flag zone '+(i+1)+' of '+sg.length+' (approx '+Math.round(d.sgl||0)+'m each)'+' \\u2014 state '+st+\n"
"(st=='2'?' (YELLOW)':(st=='0'?' (green)':' (unmapped)'))+'\" style=\"display:inline-block;width:9px;height:11px;'+\n"
"'margin-right:1px;background:'+bg+';'+(mine?'outline:1px solid #fff;':'')+'\"></span>';}\n"
"var hdr=(d.yc>0)?('<span style=\"color:#f5c400;font-weight:700\">YELLOW \\u00d7'+d.yc+'</span>')\n"
":'<span style=\"color:#2ecc71\">ALL GREEN</span>';\n"
"var own=(fc&&fc.yf)?'<div style=\"grid-column:1/3;color:#f5c400;font-weight:700;text-align:center;padding:2px 0\">\\u26A0 '+\n"
"code3(fc.n)+' IS UNDER A YELLOW FLAG</div>':'';\n"
/* Label the strip for what it IS. These are the Steward's MARSHAL ZONES --
   the flag-posting segments the game's own HUD flag sprite is driven from --
   and a track has as many of them as it has marshal posts. They are NOT
   timing sectors. A lap has exactly three timing sectors (F1CUILive builds a
   3-element gapSectorZone from the AIW keys lap_length / sector_1_length /
   sector_2_length; the game parses those same three keys itself in
   sub_565870). Showing ~20 unlabelled boxes next to "FLAGS" read as "way too
   many sectors", which is a naming failure, not a data one. */
/* The three real timing sectors, from the track's .AIW. Rendered as a
   proportional bar so it is visually obvious this is a different thing from
   the marshal-zone strip below it: 3 blocks, sized by actual sector length,
   with the focused car's current sector lit. */
"var sect='';\n"
"if(d.lapm>0){var b=[0,d.sc1,d.sc2,d.lapm],cs=(fc&&fc.cs)||0;\n"
"for(var si=0;si<3;si++){var w=(b[si+1]-b[si])/d.lapm*100;\n"
"sect+='<span title=\"Sector '+(si+1)+': '+b[si].toFixed(0)+'\\u2013'+b[si+1].toFixed(0)+\n"
"'m of '+d.lapm.toFixed(0)+'m (from '+es(d.trk||'')+'.AIW)\" style=\"display:inline-block;'+\n"
"'width:'+w.toFixed(2)+'%;height:13px;box-sizing:border-box;font-size:8px;text-align:center;'+\n"
"'line-height:13px;border-right:1px solid var(--bg);background:'+(cs===si+1?'#2ecc71':'#2a2a33')+';'+\n"
"'color:'+(cs===si+1?'#000':'var(--t3)')+'\">S'+(si+1)+'</span>';}\n"
"sect='<div class=\"k\" title=\"Timing sectors, read from the track\\'s .AIW (lap_length / sector_1_length / sector_2_length). Exactly three, always.\">SECTORS</div>'+\n"
"'<div class=\"v\" style=\"font-size:9px\">'+d.lapm.toFixed(0)+'m'+(d.trk?' \\u00b7 '+es(d.trk):'')+'</div>'+\n"
"'<div style=\"grid-column:1/3;padding:2px 0;line-height:0\">'+sect+'</div>';}\n"
"else sect='<div class=\"k\">SECTORS</div><div class=\"v\" style=\"opacity:.6\">AIW not read</div>';\n"
"return sect+'<div class=\"k\">FLAGS</div><div class=\"v\">'+hdr+'</div>'+\n"
"'<div style=\"grid-column:1/3;padding:3px 0;line-height:0\" title=\"'+sg.length+\n"
"' marshal zones (flag-posting segments), start line first. NOT timing sectors \\u2014 a lap has 3 of those.\">'+cells+'</div>'+\n"
"'<div style=\"grid-column:1/3;color:var(--t3);font-size:9px;text-align:center\">'+\n"
"sg.length+' marshal zones \\u00b7 '+Math.round(d.sgl||0)+'m each \\u00b7 not timing sectors</div>'+own;}\n"
/* ---- why a car is going to retire ------------------------------------------
   A bare "DOOMED" on every affected car says nothing: a rear wing hanging off on
   the way to the box, a wheel torn off at a standstill and a dead engine are
   three different events. `dc` carries the cause, and the impact count (`hr`)
   plus damage severity (`cc`) turn it into a sentence. */
"var DOOMWHY={1:{tag:'REAR WING',txt:'Rear wing detached. The pit-box blocking condition tests only this record, so the game will retire the car when it reaches the box \\u2014 it cannot be repaired.'},\n"
"2:{tag:'WHEEL',txt:'Wheel lost (damage severity 2). Terminal \\u2014 the car cannot continue.'},\n"
"3:{tag:'ENGINE',txt:'Engine has stopped: RPM and crankshaft angular velocity are both zero while the car has not finished.'}};\n"
"function doomTag(c2){var w=DOOMWHY[c2.dc];if(!w)return'OUT';\n"
"return(c2.hr>=3?'ACCIDENT / ':'')+w.tag;}\n"
"function doomWhy(c2){var w=DOOMWHY[c2.dc];\n"
"var base=w?w.txt:'Terminal damage, cause not resolved.';\n"
"if(c2.dpr&2)base+=' (Rear wing was already gone when telemetry attached, so no bulletin was raised for it.)';\n"
"else if(c2.dpr&1)base+=' (Front wing was already gone when telemetry attached.)';\n"
"if(c2.hr>0)base+=' Impacts this race: '+c2.hr+'.';\n"
"if(c2.rc>0)base+=' Failure category: '+(RC[c2.rc]||('code '+c2.rc))+'.';\n"
"if(c2.cb)base+=' Called to the pits.';\n"
"return base;}\n"
/* ---- component life ---------------------------------------------------------
   Real remaining seconds from the game's own countdowns (veh+0x3D70/74/78/7C),
   not inferred from oil temp. A negative value means that component has already
   failed — the counter is parked at a negative sentinel after it fires. */
/* Sign carries meaning, live-verified: a counter that actually EXPIRED parks at a
   negative sentinel (a blown engine read -1.0), whereas a car retired by some
   other cause has its counters ZEROED (two accident retirements, ReasonCode 6,
   both read exactly 0.00 with brakes/gearbox untouched). So negative = this part
   killed the car; zero = not applicable, the car went out another way. */
"function lifeShort(sec){if(sec===undefined)return'-';\n"
"if(sec<0)return'FAILED';\n"
"if(sec===0)return'\\u2014';\n"
"if(sec>=3600)return(sec/3600).toFixed(1)+'h';\n"
"return Math.round(sec/60)+'m';}\n"
"function lifeCol(pct){if(pct===undefined||pct<0)return'';\n"
"return pct<=15?'color:#ff4136;font-weight:700':(pct<=35?'color:#fc0':'');}\n"
"/* Laps-remaining is the number a race engineer actually wants: seconds of life\n"
"   divided by the driver's last lap time. Only shown when we have a real lap. */\n"
"function lifeTxt(sec,pct,lastlap){if(sec===undefined)return'-';\n"
"if(sec<0)return'FAILED';\n"
"if(sec===0)return'\\u2014 (out for another reason)';\n"
"var t=lifeShort(sec);\n"
"if(pct>=0)t+=' ('+pct+'%)';\n"
"if(lastlap>0&&sec>0)t+=' \\u2248'+Math.floor(sec/lastlap)+' laps';\n"
"return t;}\n"
"function tireLabel(tc){return tireName(tc);}\n"
/* Tyre wear. CORRECTED 2026-07-28: the usable range is 1.0 (fresh) down to
   0.75 (destroyed), NOT down to 0. The game's own physics and its wear bar
   both normalize with (tread - 0.75) * 4, so the previous (1 - v) * 1000
   per-mille reading was against a scale four times too wide and could never
   approach its own maximum -- a tyre at the blowout threshold showed 250
   per-mille, i.e. "25% worn", when it was in fact finished.
   life() returns percent remaining on the game's scale: 100 fresh, 0 blown,
   with 70/40 the light/severe blister call thresholds. */
"function life(v){if(v===undefined||v===null)return null;\n"
"var n=(v-0.75)*4;return Math.max(0,Math.min(1,n))*100;}\n"
"function lifeCol2(p){return p===null?'':(p<=40?'color:#ff4136':(p<=70?'color:#fc0':''));}\n"
/* Life remaining per corner on the game's own scale. For the PLAYER the
   thermal block (d.trd) is authoritative; the per-car wfl..wrr mirror is the
   only thing available for AI. */
"function wearTxt(c2,d){\n"
"var a=null;\n"
"if(d&&d.trd&&c2&&c2.id===d.ps)a=d.trd.map(function(v){return Math.max(0,Math.min(1,v))*100;});\n"
"else if(c2&&c2.wfl!==undefined)a=[c2.wfl,c2.wfr,c2.wrl,c2.wrr].map(life);\n"
"if(!a)return'-';\n"
"var mn=Math.min.apply(null,a);\n"
"return'<span style=\"'+lifeCol2(mn)+'\" title=\"Tyre life remaining, percent, FL/FR/RL/RR. 100 = fresh, 0 = blowout (raw tread 0.75). 70 and 40 are the light and severe blister call thresholds.\">'+\n"
"a.map(function(x){return x.toFixed(1)}).join(' / ')+'</span>';}\n"
"var TRK=[],TRK_MAX=500;var MB=null;var FOCUS=null;var FOCUS_LOCK=0;\n"
"var RPMF=9.549296585513721;/* live c.r/c.mr are rad/s (engine-rpm-live-scale-20261009) */\n"
"var TRKPATH=[],LAST_WC=null,LAST_TRACK_KEY=null;\n"
"function loadTrack(){fetch('/track').then(function(r){return r.json()})\n"
".then(function(j){TRKPATH=(j.pts||[]).map(function(p2){return{x:p2[0],z:p2[1]}});})\n"
".catch(function(){});}\n"
"/* Retirement classification text. Taken verbatim from the game's own lookup,\n"
"   FailureCategoryNameLookup_004bb730 (0x004BB730) — a plain switch returning the\n"
"   string constants aEngine/aGearbox/aClutch/aSuspension/aBrakes/aAccident/\n"
"   aElectronics/aFuel, with 3 and 7 both mapping to Clutch. This is static proof\n"
"   from decompiled code and supersedes the conflicting enum in name_index.json\n"
"   (which had 2/4/5 permuted and 6 as 'puncture').\n"
"   NOTE: this code exists only to label AI mechanical failures on the final\n"
"   classification screen. It is not a live damage signal — a genuine AI pit\n"
"   retirement leaves it at 0 — so it is only shown once a car is actually out. */\n"
"var RC={0:'N/A',1:'Engine',2:'Gearbox',3:'Clutch',4:'Suspension',\n"
"5:'Brakes',6:'Accident',7:'Clutch',8:'Electronics',9:'Fuel'};\n"
"/* CORRECTED 2026-07-25: this was previously suppressed until a car was classified\n"
"   out, on the assumption the code only mattered at final classification. Wrong --\n"
"   observed live changing 6 (Accident) -> 1 (Engine) on Hakkinen while he was still\n"
"   running (finishState 0, retired 0) with a dead, smoking engine. Suppressing it\n"
"   hid exactly the failure it exists to describe. Now shown whenever it is set. */\n"
"function reasonTxt(c2){if(!c2)return'-';\n"
"if(c2.rc>0)return RC[c2.rc]||('code '+c2.rc);\n"
"var out=(c2.fs==2||c2.fs==3||c2.rt);\n"
"return out?'Not classified':'-';}\n"
"var WL={0:{t:'Dry',i:'&#9728;'},1:{t:'Damp',i:'&#9925;'},\n"
"2:{t:'Wet',i:'&#9730;'},3:{t:'Rain',i:'&#9748;'},4:{t:'Storm',i:'&#9928;'}};\n"
"/* Condition is now DERIVED from the weather struct's on-line wetness (0.0 in a dry\n"
"   session, live-verified), because TrackConditionState is a float and was being\n"
"   read as an int and used as an index into this table \\u2014 it produced garbage. */\n"
"function condIdx(d){var w=d.onw||0;\n"
"return w<=0.02?0:(w<0.15?1:(w<0.40?2:(w<0.70?3:4)));}\n"
"var SL={0:'Menu',1:'P1',2:'P2',3:'P3',4:'Q1',5:'Q2',6:'Wup',7:'Race',8:'Champ'};\n"
"/* RacePhase is documented 0..8 (docs/12), so a 5-entry table showed '?' for the\n"
"   most common in-race value. Unmapped codes now print as 'ph N' instead of '?'. */\n"
"var PL={0:'?',1:'Grid',2:'Race',3:'Fin',4:'Pod'};\n"
"function phLbl(p){return PL[p]!==undefined?PL[p]:('ph '+p);}\n"
"\n"
"/* ---- formatters ---- */\n"
"function fm(s){if(!s||s<=0||s>99999)return' -';\n"
"var m=Math.floor(s/60),sc=Math.floor(s%60),ms=Math.floor((s%1)*1000);\n"
"return m+':'+(sc<10?'0':'')+sc+'.'+(ms<100?(ms<10?'00':'0'):'')+ms;}\n"
"function fs(s){if(!s||s<=0||s>99999)return'-';var m=Math.floor(s/60),\n"
"sc=Math.floor(s%60),ms=Math.floor((s%1)*1000);\n"
"return m+':'+(sc<10?'0':'')+sc+'.'+(ms<100?'0':'')+ms;}\n"
"function fet(v){if(!v||v<=0)return'--:--';var m=Math.floor(v/60),s=Math.floor(v%60);\n"
"return (m<10?'0':'')+m+':'+(s<10?'0':'')+s;}\n"
"function gm(tt,ld){if(!tt||!ld||tt<=0||ld<=0)return'-';\n"
"var g=tt-ld;if(Math.abs(g)<.001)return'LEADER';return'+'+g.toFixed(3);}\n"
"/* Gap/interval from total_time (+0x4098). VERIFIED live 2026-07-25 via frida:\n"
"   tt IS per-car cumulative race time (SCH 2961.78 / HAK 2964.54 / COU 2965.35 on\n"
"   the same lap = real gaps), while elapsed_time (+0x4570) reads 0.0 for every car.\n"
"   The old README claim (tt shared/identical, use et) is inverted and made the whole\n"
"   GAP column render '-'. Lap-aware: a lapped car shows +NL, not a bogus seconds gap. */\n"
"function gapStr(c,ref){if(!c||!ref)return'-';\n"
"if(c.id===ref.id)return'LEADER';\n"
"var dl=(ref.l||0)-(c.l||0);\n"
"if(dl>=1)return'+'+dl+'L';\n"
"if(!c.tt||!ref.tt||c.tt<=0||ref.tt<=0)return'-';\n"
"var g=c.tt-ref.tt;if(g<-0.001)return'-';\n"
"return'+'+Math.abs(g).toFixed(3);}\n"
"function intStr(c,ahead){if(!c||!ahead)return'-';\n"
"var dl=(ahead.l||0)-(c.l||0);\n"
"if(dl>=1)return'+'+dl+'L';\n"
"if(!c.tt||!ahead.tt||c.tt<=0||ahead.tt<=0)return'-';\n"
"var g=c.tt-ahead.tt;if(g<-0.001)return'-';\n"
"return'+'+Math.abs(g).toFixed(3);}\n"
"function fdelta(bl,ll){if(!bl||!ll||bl<=0||ll<=0)return'';\n"
"var d=bl-ll;if(Math.abs(d)<.001)return'<span class=\"nml\">0.000</span>';\n"
"var cls=d>0?'rd2':'gn2';var sgn=d>0?'+':'';\n"
"return'<span class=\"nml '+cls+'\">'+sgn+d.toFixed(3)+'</span>';}\n"
"function fSpd(v){if(!v||v<=0||v>130)return'-';return(v*3.6).toFixed(0);}\n"
"function es(s){s=(s||'').replace(/\\s*\\(\\d{4}\\)\\s*$/,'');\n"
"return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')\n"
".replace(/>/g,'&gt;').replace(/\"/g,'&quot;');}\n"
"function code3(s){var p=(s||'').replace(/\\s*\\(\\d{4}\\)\\s*$/,'').trim().split(/\\s+/);\n"
"return(p[p.length-1]||'').substring(0,3).toUpperCase();}\n"
"\n"
"/* ---- clock ---- */\n"
"setInterval(function(){var n=new Date();\n"
"document.getElementById('clk').textContent=\n"
"n.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',second:'2-digit'});},1000);\n"
"\n"
"/* ---- gauges ---- */\n"
"var gC=document.getElementById('gS'),gX=gC.getContext('2d');\n"
"var tC=document.getElementById('gT'),tX=tC.getContext('2d');\n"
"function dg(ctx,w,h,val,max,unit,lo,hi,rz,dec){\n"
"var cx=w/2,cy=h/2,r=Math.min(w,h)/2-18;\n"
"ctx.clearRect(0,0,w,h);\n"
"ctx.beginPath();ctx.arc(cx,cy,r+6,0.75*Math.PI,2.25*Math.PI);\n"
"ctx.strokeStyle='#1a1a24';ctx.lineWidth=12;ctx.stroke();\n"
"var range=hi-lo,a0=0.75*Math.PI,as=1.5*Math.PI;\n"
"function a(v){return a0+(v-lo)/range*as;}\n"
"var ge=Math.min(rz,hi);\n"
"ctx.beginPath();ctx.arc(cx,cy,r+6,a(lo),a(ge));\n"
"ctx.strokeStyle='#1a3a1a';ctx.lineWidth=12;ctx.stroke();\n"
"if(rz<hi){ctx.beginPath();ctx.arc(cx,cy,r+6,a(rz),a(hi));\n"
"ctx.strokeStyle='#3a1a1a';ctx.lineWidth=12;ctx.stroke();}\n"
"var v=Math.max(lo,Math.min(hi,val)),va=a(v);\n"
"ctx.beginPath();ctx.arc(cx,cy,r+6,a0,va);\n"
"var g=ctx.createLinearGradient(cx-r,cy,cx+r,cy);\n"
"g.addColorStop(0,'#00cc44');g.addColorStop(0.7,'#fc0');g.addColorStop(1,'#e10600');\n"
"ctx.strokeStyle=g;ctx.lineWidth=10;ctx.stroke();\n"
"for(var i=0;i<=8;i++){var tv=lo+(hi-lo)*i/8,ta=a(tv);\n"
"var ir=r-6,orr=r+16;\n"
"ctx.beginPath();ctx.moveTo(cx+ir*Math.cos(ta),cy+ir*Math.sin(ta));\n"
"ctx.lineTo(cx+orr*Math.cos(ta),cy+orr*Math.sin(ta));\n"
"ctx.strokeStyle='#444';ctx.lineWidth=1;ctx.stroke();\n"
"var lr=r+22,lx=cx+lr*Math.cos(ta),ly=cy+lr*Math.sin(ta);\n"
"ctx.fillStyle='#666';ctx.font='7px monospace';ctx.textAlign='center';\n"
"ctx.textBaseline='middle';ctx.fillText(Math.round(tv),lx,ly);}\n"
"var nr=r-28;\n"
"ctx.beginPath();ctx.moveTo(cx,cy);\n"
"ctx.lineTo(cx+nr*Math.cos(va),cy+nr*Math.sin(va));\n"
"ctx.strokeStyle=v>=rz?'#e10600':'#fff';ctx.lineWidth=2.5;ctx.stroke();\n"
"ctx.beginPath();ctx.arc(cx,cy,10,0,2*Math.PI);ctx.fillStyle='#111';ctx.fill();\n"
"ctx.beginPath();ctx.arc(cx,cy,5,0,2*Math.PI);\n"
"ctx.fillStyle=v>=rz?'#e10600':'#333';ctx.fill();\n"
"ctx.fillStyle='#fff';ctx.font='bold 20px Consolas,monospace';ctx.textAlign='center';\n"
"ctx.textBaseline='bottom';ctx.fillText(dec?v.toFixed(0):Math.round(v),cx,cy-16);\n"
"ctx.fillStyle='#666';ctx.font='8px sans-serif';ctx.fillText(unit,cx,cy+2);}\n"
"\n"
"/* ---- track map ---- */\n"
"var PC=['#ffd700','#c0c0c0','#cd7f32','#fff','#fff','#fff',\n"
"'#fff','#fff','#fff','#fff','#e10600','#3498db','#27ae60','#ff6600',\n"
"'#ff69b4','#0ff','#ff0','#f0f','#0f0','#aaa','#aaa','#aaa','#aaa'];\n"
"var mapFlipZ=true;\n"
"function mapView(name,cvsId,wrapId){\n"
"var cvs=document.getElementById(cvsId),ctx=cvs.getContext('2d');\n"
"var st={zoom:1,panX:0,panY:0,drag:null,hits:[],mb:null};\n"
"cvs.addEventListener('wheel',function(e){e.preventDefault();\n"
"var f=e.deltaY<0?1.15:0.87;st.zoom=Math.max(0.5,Math.min(8,st.zoom*f));},{passive:false});\n"
"cvs.addEventListener('mousedown',function(e){st.drag={x:e.clientX,y:e.clientY,px:st.panX,py:st.panY};});\n"
"window.addEventListener('mousemove',function(e){if(st.drag){\n"
"var ddx=e.clientX-st.drag.x,ddy=e.clientY-st.drag.y;\n"
"if(Math.abs(ddx)>3||Math.abs(ddy)>3){st.panX=st.drag.px+ddx;st.panY=st.drag.py+ddy;}}});\n"
"window.addEventListener('mouseup',function(){st.drag=null;});\n"
"cvs.addEventListener('dblclick',function(){st.zoom=1;st.panX=0;st.panY=0;});\n"
"cvs.addEventListener('click',function(e){\n"
"var rc=cvs.getBoundingClientRect(),mx=e.clientX-rc.left,my=e.clientY-rc.top,best=null,bd=1e9;\n"
"for(var i=0;i<st.hits.length;i++){var hp=st.hits[i];\n"
"var dd=(mx-hp.x)*(mx-hp.x)+(my-hp.y)*(my-hp.y);\n"
"if(dd<(hp.r+12)*(hp.r+12)&&dd<bd){bd=dd;best=hp;}}\n"
"if(best)focusCar(best.id);});\n"
"return function(ca){var mw=document.getElementById(wrapId);\n"
"var w=mw.clientWidth||280,h=mw.clientHeight||180;if(w<10||h<10)return;\n"
"cvs.width=w;cvs.height=h;ctx.clearRect(0,0,w,h);st.hits=[];\n"
"if(!ca||ca.length===0)return;\n"
"var mnX=1e9,mxX=-1e9,mnZ=1e9,mxZ=-1e9,ps=[];\n"
"var haveTrackPath=TRKPATH.length>1;\n"
/* `a` (0x45F4) is an AI-CONTROL bit, not presence: it reads 0 for the human
   driver, so filtering the map on it silently dropped the player's own dot every
   frame. `ot2` is the server-side presence flag (not out, not retired). */
"var onTrack=function(c3){return c3.p>0&&(c3.ot2!==undefined?c3.ot2:1);};\n"
"var useTrack=haveTrackPath||ca.some(function(c3){return onTrack(c3)&&(Math.abs(c3.mx||0)>0.01||Math.abs(c3.mz||0)>0.01)});\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(!onTrack(c2))continue;\n"
"var x=useTrack?(c2.mx||0):(c2.px||0),z=useTrack?(c2.mz||0):(c2.pz||0);\n"
"if(x<mnX)mnX=x;if(x>mxX)mxX=x;if(z<mnZ)mnZ=z;if(z>mxZ)mxZ=z;ps.push(c2);}\n"
"if(haveTrackPath)for(var i=0;i<TRKPATH.length;i++){var tp2=TRKPATH[i];\n"
"if(tp2.x<mnX)mnX=tp2.x;if(tp2.x>mxX)mxX=tp2.x;if(tp2.z<mnZ)mnZ=tp2.z;if(tp2.z>mxZ)mxZ=tp2.z;}\n"
"if(ps.length===0&&!haveTrackPath)return;\n"
"var rx=mxX-mnX,rz=mxZ-mnZ;\n"
"if(rx<100){var ex=(100-rx)/2;mnX-=ex;mxX+=ex;rx=100;}\n"
"if(rz<100){var ez=(100-rz)/2;mnZ-=ez;mxZ+=ez;rz=100;}\n"
"if(!st.mb)st.mb={mx:mnX,Mx:mxX,mz:mnZ,Mz:mxZ};\n"
"else{var f=.06;var mb=st.mb;\n"
"mb.mx+=Math.max(0,mnX-mb.mx)*f+Math.min(0,mnX-mb.mx)*.3;\n"
"mb.Mx+=Math.max(0,mxX-mb.Mx)*f+Math.min(0,mxX-mb.Mx)*.3;\n"
"mb.mz+=Math.max(0,mnZ-mb.mz)*f+Math.min(0,mnZ-mb.mz)*.3;\n"
"mb.Mz+=Math.max(0,mxZ-mb.Mz)*f+Math.min(0,mxZ-mb.Mz)*.3;}\n"
"var b=st.mb,pad=25,rrx=b.Mx-b.mx||1,rrz=b.Mz-b.mz||1;\n"
"var sc=Math.min((w-2*pad)/rrx,(h-2*pad)/rrz);\n"
"var offX=(w-rrx*sc)/2,offZ=(h-rrz*sc)/2;\n"
"function tx(v){return offX+(v-b.mx)*sc*st.zoom+st.panX;}\n"
"function tz(v){var vv=mapFlipZ?(b.mz+b.Mz-v):v;return offZ+(vv-b.mz)*sc*st.zoom+st.panY;}\n"
"if(haveTrackPath){ctx.beginPath();\n"
"ctx.moveTo(tx(TRKPATH[0].x),tz(TRKPATH[0].z));\n"
"for(var i=1;i<TRKPATH.length;i++)ctx.lineTo(tx(TRKPATH[i].x),tz(TRKPATH[i].z));\n"
"ctx.closePath();ctx.strokeStyle='rgba(255,255,255,0.35)';ctx.lineWidth=3;ctx.stroke();}\n"
"else if(TRK.length>1){ctx.beginPath();\n"
"ctx.moveTo(tx(TRK[0].x),tz(TRK[0].z));\n"
"for(var i=1;i<TRK.length;i++)ctx.lineTo(tx(TRK[i].x),tz(TRK[i].z));\n"
"ctx.strokeStyle='rgba(255,255,255,0.07)';ctx.lineWidth=2;ctx.stroke();}\n"
"for(var i=0;i<ps.length;i++){var c2=ps[i];\n"
"var dx=useTrack?(c2.mx||0):(c2.px||0),dz=useTrack?(c2.mz||0):(c2.pz||0);\n"
"var ppx=tx(dx),ppz=tz(dz);\n"
"var r2=Math.max(5,Math.min(12,w/40));\n"
"st.hits.push({x:ppx,y:ppz,r:r2,id:c2.id});\n"
"var co=(c2.id===FOCUS)?'#fff':(PC[Math.min(c2.p-1,22)]||'#aaa');\n"
"ctx.beginPath();ctx.arc(ppx,ppz,r2+2,0,2*Math.PI);\n"
"ctx.fillStyle='rgba(0,0,0,0.6)';ctx.fill();\n"
"ctx.beginPath();ctx.arc(ppx,ppz,r2,0,2*Math.PI);\n"
"ctx.fillStyle=co;ctx.fill();\n"
"ctx.strokeStyle=(c2.id===FOCUS)?'#3498db':'#000';ctx.lineWidth=(c2.id===FOCUS)?2:1;ctx.stroke();\n"
"if(useTrack&&(c2.dx||c2.dy)){var hx=c2.dx||0,hy=c2.dy||0,hl=Math.sqrt(hx*hx+hy*hy)||1;\n"
"hx/=hl;hy/=hl;\n"
"ctx.beginPath();ctx.moveTo(ppx,ppz);ctx.lineTo(ppx+hx*(r2+8),ppz+hy*(r2+8));\n"
"ctx.strokeStyle='rgba(255,255,255,0.4)';ctx.lineWidth=1.3;ctx.stroke();}\n"
"if(r2>=6){ctx.fillStyle=c2.p<=3?'#000':'#fff';\n"
"ctx.font='bold '+(r2-1)+'px sans-serif';ctx.textAlign='center';\n"
"ctx.textBaseline='middle';ctx.fillText(c2.p,ppx,ppz+1);}\n"
"if(r2>=8&&c2.n){ctx.fillStyle='rgba(255,255,255,0.55)';ctx.font='7px sans-serif';\n"
"ctx.textAlign='left';ctx.textBaseline='bottom';\n"
"ctx.fillText(code3(c2.n),ppx+r2+2,ppz-2);}}\n"
"ctx.fillStyle='rgba(255,255,255,0.15)';ctx.font='7px sans-serif';\n"
"ctx.textAlign='right';ctx.fillText(useTrack?'Track coords':'World coords',w-5,12);};}\n"
"var uM=mapView('small','mC','mW');\n"
"var uMPw=mapView('pw','mCPw','mWPw');\n"
"/* generic keyed row-diff: keeps <tr> nodes alive across ticks so a click target is\n"
"   never destroyed mid-click (full innerHTML rebuilds were eating clicks). */\n"
"function diffRows(tbody,store,ids,htmlFn,clsFn){var prev=null;\n"
"for(var i=0;i<ids.length;i++){var id=ids[i];var tr=store.els[id];\n"
"if(!tr){tr=document.createElement('tr');tr.setAttribute('data-fid',id);\n"
"tr.setAttribute('onclick','focusCar('+id+')');tr.className='cur';\n"
"store.els[id]=tr;store.hash[id]='';}\n"
"var cls=clsFn(id);if(tr.className!==cls)tr.className=cls;\n"
"var hh=htmlFn(id);if(store.hash[id]!==hh){tr.innerHTML=hh;store.hash[id]=hh;}\n"
"var want=prev?prev.nextSibling:tbody.firstChild;\n"
"if(want!==tr)tbody.insertBefore(tr,want);prev=tr;}\n"
"for(var k in store.els)if(store.els.hasOwnProperty(k)&&ids.indexOf(parseInt(k,10))<0){\n"
"if(store.els[k].parentNode)store.els[k].parentNode.removeChild(store.els[k]);\n"
"delete store.els[k];delete store.hash[k];}}\n"
"\n"
"/* ---- expand state ---- */\n"
"var EX={},RL={},GV={};\n"
"window.tg=function(i){EX[i]=!EX[i];up(LD);}\n"
"var rowEls={},rowHash={};\n"
"window.focusCar=function(id,tab){FOCUS=id;FOCUS_LOCK=1;if(tab)swTab(tab);\n"
"loadDriver(id);if(LD)up(LD);}\n"
/* ---- per-driver dossier: every lap this driver has completed, with sector
   splits and the stint context (compound, fuel, tyre life at that lap), plus
   every pit stop with what was actually done to the car. Fetched on focus and
   refreshed on a slow timer -- it is history, it does not need 10 Hz. */
"var DRV_CACHE=null,drvTimer=null;\n"
"function loadDriver(id){if(id===null||id===undefined)return;\n"
"fetch('/driver?id='+id).then(function(r){return r.json()})\n"
".then(function(j){if(j.gen!==undefined&&LASTGEN!==null&&j.gen!==LASTGEN)return;if(id!==FOCUS)return;DRV_CACHE=j;renderDriver();}).catch(function(){});}\n"
"function closeDoss(){var el=document.getElementById('drvDoss');if(el){el.style.display='none';el.innerHTML='';}}\n"
"function fmS(v){return(v===undefined||v===null||v<=0)?'-':v.toFixed(3);}\n"
"function renderDriver(){var el=document.getElementById('drvDoss');if(!el)return;\n"
"var j=DRV_CACHE;\n"
"if(!j||j.err){el.innerHTML='<div class=\"inc\">Select a driver.</div>';el.style.display='block';return;}\n"
"el.style.display='block';\n"
"var lp=j.laps||[],pt=j.pits||[];\n"
"var h='<div class=\"rcHdr\"><b>'+es(j.n)+'</b> \\u2014 P'+j.p+' \\u2014 '+es(j.cn)+'</div>';\n"
"h+='<div style=\"text-align:right\"><button onclick=\"closeDoss()\" style=\"background:var(--pn);color:var(--t1);border:1px solid var(--bd);border-radius:3px;font-size:10px;cursor:pointer\">close</button></div>';\n"
/* best sector / best lap highlighting, computed over this driver's own laps */
"var bl=1e9,b1=1e9,b2=1e9,b3=1e9;\n"
"for(var i=0;i<lp.length;i++){var r=lp[i];\n"
"if(r.t>0&&r.t<bl)bl=r.t;if(r.s1>0&&r.s1<b1)b1=r.s1;\n"
"if(r.s2>0&&r.s2<b2)b2=r.s2;if(r.s3>0&&r.s3<b3)b3=r.s3;}\n"
"h+='<div class=\"sec\"><h4>Laps ('+lp.length+')</h4><table class=\"dtab\">';\n"
"h+='<tr><th>Lap</th><th>Time</th><th>S1</th><th>S2</th><th>S3</th><th>Pos</th>'+\n"
"'<th>Tyre</th><th>Fuel</th><th>Tyre life FL/FR/RL/RR</th><th>Pit</th></tr>';\n"
"for(var i2=lp.length-1;i2>=0;i2--){var r2=lp[i2];\n"
"var bst=function(v,b){return(v>0&&v<=b+1e-4)?'color:#b000ff;font-weight:700':'';};\n"
"var lf=(r2.lf||[]).map(function(v){return(v*100).toFixed(1)});\n"
"var mnl=Math.min.apply(null,(r2.lf||[1]).map(function(v){return v*100}));\n"
"h+='<tr><td class=\"nml\">'+r2.l+'</td>'+\n"
"'<td class=\"nml\" style=\"'+bst(r2.t,bl)+'\">'+fmLap(r2.t)+'</td>'+\n"
"'<td class=\"nml\" style=\"'+bst(r2.s1,b1)+'\">'+fmS(r2.s1)+'</td>'+\n"
"'<td class=\"nml\" style=\"'+bst(r2.s2,b2)+'\">'+fmS(r2.s2)+'</td>'+\n"
"'<td class=\"nml\" style=\"'+bst(r2.s3,b3)+'\">'+fmS(r2.s3)+'</td>'+\n"
"'<td class=\"nml\">'+r2.p+'</td><td class=\"nml\">'+tireName(r2.tc)+'</td>'+\n"
"'<td class=\"nml\">'+(r2.f>0?r2.f.toFixed(1):'-')+'</td>'+\n"
"'<td class=\"nml\" style=\"'+lifeCol2(mnl)+'\">'+lf.join(' / ')+'</td>'+\n"
"'<td class=\"nml\">'+(r2.pit?'PIT':'')+'</td></tr>';}\n"
"if(lp.length===0)h+='<tr><td colspan=\"10\">No completed laps yet.</td></tr>';\n"
"h+='</table></div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Pit stops ('+pt.length+')</h4><table class=\"dtab\">';\n"
"h+='<tr><th>Lap in</th><th>Lap out</th><th>Stationary</th><th>Tyres</th>'+\n"
"'<th>Fuel in</th><th>Fuel out</th><th>Added</th><th>Repairs</th><th>State</th></tr>';\n"
"for(var k=0;k<pt.length;k++){var e2=pt[k];\n"
/* fuel "added" is measured across the stop; a negative delta just means the
   car burned more crossing the pit lane than it took on, so clamp the label */
"var add=e2.add>0.05?('+'+e2.add.toFixed(1)+' L'):'none';\n"
"var rep=[];if(e2.rfw)rep.push('front wing');if(e2.rrw)rep.push('rear wing');\n"
"var tyr=(e2.ti===e2.to)?(tireName(e2.to)+' (same)'):(tireName(e2.ti)+' \\u2192 '+tireName(e2.to));\n"
"h+='<tr><td class=\"nml\">'+e2.li+'</td><td class=\"nml\">'+(e2.done?e2.lo:'-')+'</td>'+\n"
"'<td class=\"nml\">'+(e2.done?e2.d.toFixed(2)+' s':'in box')+'</td>'+\n"
"'<td class=\"nml\">'+tyr+'</td>'+\n"
"'<td class=\"nml\">'+e2.fi.toFixed(1)+'</td><td class=\"nml\">'+(e2.done?e2.fo.toFixed(1):'-')+'</td>'+\n"
"'<td class=\"nml\">'+(e2.done?add:'-')+'</td>'+\n"
"'<td class=\"nml\">'+(rep.length?rep.join(' + '):(e2.dmg?'none (damaged)':'none'))+'</td>'+\n"
"'<td class=\"nml\">'+(e2.done?'complete':'IN PROGRESS')+'</td></tr>';}\n"
"if(pt.length===0)h+='<tr><td colspan=\"9\">No pit stops yet.</td></tr>';\n"
"h+='</table>';\n"
"h+='<div class=\"inc\">Stationary time is measured pit-entry to pit-exit (the game exposes no box-only clock), so it includes the pit lane run. Repairs are only observable while the car is in the box: a part that was already gone on entry and intact on exit is reported as repaired.</div>';\n"
"h+='</div>';\n"
"el.innerHTML=h;}\n"
"\n"
"/* ---- main update ---- */\n"
"function up(d){\n"
"LD=d;ap.style.display='flex';cn.style.display='none';\n"
"var trackKey=d.session_generation+':'+d.trk+':'+d.wc;\n"
"if(trackKey!==LAST_TRACK_KEY){LAST_TRACK_KEY=trackKey;LAST_WC=d.wc;TRKPATH=[];loadTrack();}\n"
"var ca=d.c||[];ca.sort(function(a,b){return(a.p||999)-(b.p||999)});\n"
"var isQual=(d.ty==4||d.ty==5);\n"
"\n"
"/* header */\n"
"var sd=document.getElementById('sd');\n"
"sd.className='d '+(d.ac?'rc':(d.ty==7?'lv':'of'));\n"
"document.getElementById('sl').textContent=(SL[d.ty]||'?')+'/'+phLbl(d.ph);\n"
"document.getElementById('ln').textContent=d.ll||0;\n"
"document.getElementById('tlv').textContent=d.tl||0;\n"
"document.getElementById('cnv').textContent=d.vc||0;\n"
"var w=WL[condIdx(d)];\n"
"document.getElementById('wx').textContent=w.t;\n"
"document.getElementById('wi').innerHTML=w.i;\n"
"document.getElementById('wt').textContent=w.t;\n"
"document.getElementById('ww').textContent=((d.tw||0)*100).toFixed(0)+'%';\n"
"document.getElementById('wai').textContent=(d.ai||0)+'%';\n"
"document.getElementById('wcm').textContent=d.cm?'CHAMP':('GP (mode '+d.rm+')');\n"
"document.getElementById('wtt').textContent=(d.trkt>-900)?d.trkt.toFixed(1)+'\\u00b0C':'-';\n"
"document.getElementById('wat').textContent=(d.at>-900)?d.at.toFixed(1)+'\\u00b0C':'-';\n"
"document.getElementById('wat').title='Air temp, weather struct +0x30 \\u2014 that address IS the corpus TemperatureOffset (0x8AFEF0), so the base is cross-referenced. Previously read from +0x04, which is a 0..1 factor.';\n"
"\n"
"/* Session-best sectors, shared by the timing tower and the focus panel.\n"
"   (The old standalone Standings tab was folded into the Pit Wall timing tower.) */\n"
"var bS1=999,bS2=999,s3Best=999;\n"
"for(var i=0;i<ca.length;i++){var cx=ca[i];if(cx.p<=0)continue;\n"
"if(cx.ls1>0&&cx.ls1<bS1)bS1=cx.ls1;\n"
"if(cx.ls2>0&&cx.ls2<bS2)bS2=cx.ls2;\n"
"var s3x=(cx.ll>0&&cx.ls1>0&&cx.ls2>0)?(cx.ll-cx.ls1-cx.ls2):0;\n"
"if(s3x>0&&s3x<s3Best)s3Best=s3x;}\n"
"\n"
"/* find focus car + leader — FOCUS keys by stable car id, not position */\n"
"var ld=null,fc=null,meCar=null;\n"
"for(var i=0;i<ca.length;i++){\n"
"if(ca[i].p==1)ld=ca[i];\n"
"if(ca[i].id===d.ps)meCar=ca[i];\n"
"if(FOCUS_LOCK&&FOCUS!==null&&ca[i].id==FOCUS)fc=ca[i];\n"
"}\n"
/* No explicit click yet: follow whatever the in-game camera is on (`pi`, a slot),
   which is what a spectator actually wants, rather than defaulting to the leader. */
"if(!fc&&d.pi!==undefined)for(var i=0;i<ca.length;i++)if(ca[i].id===d.pi){fc=ca[i];break;}\n"
"if(!fc)fc=ld;if(!fc&&ca.length>0&&ca[0].p>0)fc=ca[0];\n"
"FOCUS=fc?fc.id:null;\n"
"document.getElementById('flbl').textContent=(fc?'P'+fc.p:'-')+' '+((fc&&fc.n)?fc.n:'');\n"
"\n"
"/* focus side panel */\n"
"if(fc){\n"
"var s3fc=(fc.ll>0&&fc.ls1>0&&fc.ls2>0)?(fc.ll-fc.ls1-fc.ls2):0;\n"
"document.getElementById('ss1').textContent=fm(fc.ls1).replace(' ','');\n"
"document.getElementById('ss2').textContent=fm(fc.ls2).replace(' ','');\n"
"document.getElementById('ss3').textContent=(s3fc>0?fm(s3fc):' -').replace(' ','');\n"
"var s1e=document.getElementById('ss1'),s2e=document.getElementById('ss2'),\n"
"s3e=document.getElementById('ss3');\n"
"s1e.className=(fc.ls1>0&&fc.ls1<=bS1+0.0001)?'sct':'';\n"
"s2e.className=(fc.ls2>0&&fc.ls2<=bS2+0.0001)?'sct':'';\n"
"s3e.className=(s3fc>0&&s3fc<=s3Best+0.0001)?'sct':'';\n"
"\n"
"var spd=fc.s*3.6;dg(gX,260,260,spd,360,'km/h',0,360,280,1);\n"
"var mrp=fc.mr>0?fc.mr*RPMF:19000;\n"
"dg(tX,260,260,fc.r*RPMF,mrp,'RPM',0,mrp,mrp*.85,1);\n"
"document.getElementById('vg').textContent=fc.g>0?fc.g:'N';\n"
"document.getElementById('vl').textContent=fc.l||0;\n"
"document.getElementById('vb').textContent=fs(fc.bl);\n"
"document.getElementById('vla').textContent=fs(fc.ll);\n"
"document.getElementById('vms').textContent=fSpd(fc.ms);\n"
"document.getElementById('vet').textContent=fc.et?fet(fc.et):'--:--';\n"
"\n"
"var tp=Math.min(100,Math.max(0,fc.th*100));\n"
"var bp=Math.min(100,Math.max(0,fc.br*100));\n"
"document.getElementById('bth').style.width=tp+'%';\n"
"document.getElementById('vth').textContent=tp.toFixed(0)+'%';\n"
"document.getElementById('bbk').style.width=bp+'%';\n"
"document.getElementById('vbk').textContent=bp.toFixed(0)+'%';\n"
"var fp=fc.fc?Math.min(100,fc.fc*20):0;\n"
"document.getElementById('bfu').style.width=fp+'%';\n"
"document.getElementById('vfu').textContent=fc.fc?fc.fc.toFixed(2):'-';\n"
"\n"
"/* focus details */\n"
"var pc=(fc.pt0>0?1:0)+(fc.p1>0?1:0)+(fc.p2>0?1:0);\n"
"document.getElementById('dcn').textContent=es(fc.cn||'-');\n"
"document.getElementById('dgp').textContent=fc.gp>0?fc.gp:'-';\n"
"document.getElementById('dst').textContent=pc;\n"
"var lpt=fc.lp>0?fc.lp.toFixed(1)+'s':'-';\n"
"document.getElementById('dlp').textContent=lpt;\n"
"document.getElementById('ddm').textContent=fc.cc||0;\n"
"document.getElementById('dtl').textContent=fc.tl||0;\n"
"document.getElementById('dhr').textContent=fc.hr||0;\n"
"document.getElementById('dfc').textContent=fc.fc?fc.fc.toFixed(3):'-';\n"
"document.getElementById('dlr').textContent='L'+(fc.lr||0);\n"
"document.getElementById('dsc').textContent=fc.sc>0.01?fc.sc.toFixed(3):'-';\n"
"document.getElementById('dap').textContent=fc.ap>0.001?fc.ap.toFixed(4):'-';\n"
"document.getElementById('drc').textContent=reasonTxt(fc);\n"
"}\n"
"\n"
/* Sensor min/max and the cornering buckets must accumulate on EVERY poll,
   not only while the Full Debug tab happens to be open -- otherwise the
   driver does the checklist lap with the wrong tab showing and the panel
   has nothing to report. Cheap: a few dozen comparisons. */
"if(d&&fc&&fc.id===d.ps)verifUpdate(d,fc);\n"
"if(CURTAB=='pw')renderPitWall(d,ca,fc);\n"
"if(CURTAB=='tp')renderTeamPrincipal(d,ca);\n"
"if(CURTAB=='debug')renderDebug(d,ca,fc);\n"
"if(CURTAB=='rc')renderRaceControl(d,ca);\n"
"\n"
"/* track map */\n"
"if(ld&&(ld.ot2!==undefined?ld.ot2:ld.a)){var utk=(Math.abs(ld.mx||0)>0.01||Math.abs(ld.mz||0)>0.01);\n"
"var tmx=utk?ld.mx:(ld.px||0),tmz=utk?ld.mz:(ld.pz||0);\n"
"if(tmx||tmz)TRK.push({x:tmx,z:tmz});\n"
"if(TRK.length>TRK_MAX)TRK.shift();}\n"
"uM(ca);\n"
"}\n"
"\n"
"/* ================= ENGINEERING TAB (4.2.0) =================\n"
"   Distance-aligned comparison of two recorded laps: live player/AI traces\n"
"   or exported files. Channel availability comes from the server validity\n"
"   flags; unavailable channels are shown as unavailable, never as zeros.\n"
"   The interpolation below is the client mirror of the C builder\n"
"   (build_trace_compare_json) and is exercised by the offline Node test. */\n"
"var ENG_STATUS=null,ENG_DATA=null,ENG_FILE={a:null,b:null},ENG_BUILT=0;\n"
"var ENG_SEL={a:-1,b:-1,la:null,lb:null};\n"
"var ENG_ZOOM=null,ENG_INT=[null,null],ENG_EPOCH=0,engTimer=null;\n"
"var ENG_PENDING={a:false,b:false},ENG_REFRESHING=false;\n"
"/*CMP-BEGIN*/\n"
"function cmpGrid(samples,lapLen,step,startSim){\n"
"step=Math.max(1,Math.min(50,step));step=Math.max(step,lapLen/999);\n"
"var n=Math.min(1000,Math.floor(lapLen/step)+1);\n"
"if(!Number.isFinite(startSim))startSim=samples.length?samples[0].t:0;\n"
"var keys=['s','th','br','st','gl','go','gv','y','yr','tr0','tr1','tr2','tr3','elapsed'];\n"
"var ch={},have=[];for(var k=0;k<keys.length;k++){ch[keys[k]]=[];}\n"
"for(var i=0;i<n;i++){for(var k=0;k<keys.length;k++)ch[keys[k]][i]=null;have[i]=0;}\n"
"var F={s:2,th:4,br:8,st:16,gl:32,go:64,gv:128,y:256,yr:512,tr:1024};\n"
"var breaks=0,breakDist=[];\n"
"for(var s=0;s+1<samples.length;s++){\n"
"var a=samples[s],b=samples[s+1];\n"
"var dt=b.t-a.t,dd=b.d-a.d;\n"
"if((b.f&2048)||!(dt>0)||dt>1.0||!(dd>0)||dd>50){breaks++;if(breakDist.length<64)breakDist.push(a.d);continue;}\n"
"var i0=Math.ceil(a.d/step),i1=Math.floor(b.d/step);\n"
"if(i0<0)i0=0;if(i1>n-1)i1=n-1;\n"
"var both=a.f&b.f;\n"
"for(var i=i0;i<=i1;i++){\n"
"var d=i*step,f=(d-a.d)/dd;if(f<0)f=0;if(f>1)f=1;\n"
"function lerp(key,bit){if((both&bit)&&!(have[i]&bit)){ch[key][i]=a[key]+(b[key]-a[key])*f;have[i]|=bit;}}\n"
"lerp('s',F.s);lerp('th',F.th);lerp('br',F.br);lerp('st',F.st);\n"
"lerp('gl',F.gl);lerp('go',F.go);lerp('gv',F.gv);lerp('yr',F.yr);\n"
"if((both&F.y)&&!(have[i]&F.y)){var y1=b.y;while(y1-a.y>Math.PI)y1-=2*Math.PI;while(y1-a.y<-Math.PI)y1+=2*Math.PI;ch.y[i]=a.y+(y1-a.y)*f;have[i]|=F.y;}\n"
"if((both&F.tr)&&!(have[i]&F.tr)){for(var w=0;w<4;w++)ch['tr'+w][i]=a.tr[w]+(b.tr[w]-a.tr[w])*f;have[i]|=F.tr;}\n"
"if(!(have[i]&2048)){ch.elapsed[i]=a.t+(b.t-a.t)*f-startSim;have[i]|=2048;}\n"
"}}\n"
"return {n:n,step:step,ch:ch,have:have,breaks:breaks,breakDist:breakDist};\n"
"}\n"
"/*CMP-END*/\n"
"function engOpen(){if(!ENG_BUILT){\n"
"document.getElementById('swEng').innerHTML=\n"
"'<div class=\"eng-bar\">A: <select id=\"engCA\"></select> lap <select id=\"engLA\"></select>'+\n"
"' B: <select id=\"engCB\"></select> lap <select id=\"engLB\"></select>'+\n"
"' step <select id=\"engStep\"><option>5</option><option>10</option><option>2</option><option>20</option></select>'+\n"
"' <button onclick=\"engLoad()\">Load</button>'+\n"
"' <button onclick=\"engZoomReset()\">Reset zoom</button>'+\n"
"' <button onclick=\"engSector(1)\">S1</button><button onclick=\"engSector(2)\">S2</button><button onclick=\"engSector(3)\">S3</button>'+\n"
"' <button onclick=\"engIntClear()\">Clear interval</button>'+\n"
"' file A <input type=\"file\" id=\"engFA\" accept=\".json\" onchange=\"engFile(this,\\'a\\')\">'+\n"
"' file B <input type=\"file\" id=\"engFB\" accept=\".json\" onchange=\"engFile(this,\\'b\\')\">'+\n"
"'</div>'+\n"
"'<div class=\"eng-note\" id=\"engAvail\"></div>'+\n"
"'<div class=\"eng-stats\" id=\"engLive\"></div><div class=\"eng-note\" id=\"engProgress\"></div>'+\n"
"'<div id=\"engMsg\" class=\"warn\" style=\"display:none\"></div>'+\n"
"'<canvas id=\"engC\" class=\"eng-canvas\" height=\"560\" style=\"display:none;height:560px\"></canvas>'+\n"
"'<div class=\"eng-note\" id=\"engCursor\">Move the cursor over the charts; click twice to mark an interval.</div>'+\n"
"'<div class=\"eng-stats\" id=\"engStats\"></div>'+\n"
"'<div class=\"eng-stats\" id=\"engMeta\"></div>';\n"
"var cv=document.getElementById('engC');\n"
"cv.addEventListener('mousemove',engMove);\n"
"cv.addEventListener('mousedown',engClick);\n"
"cv.addEventListener('wheel',engWheel,{passive:false});\n"
"cv.addEventListener('dblclick',engZoomReset);\n"
"ENG_BUILT=1;}\n"
"engRefresh();if(!engTimer)engTimer=setInterval(engRefresh,2000);\n"
"}\n"
"function engMsg(t){var el=document.getElementById('engMsg');if(!el)return;\n"
"if(!t){el.style.display='none';el.textContent='';return;}el.style.display='block';el.textContent=t;}\n"
"function engRefresh(){\n"
"if(CURTAB!=='eng'||ENG_REFRESHING||ENG_PENDING.a||ENG_PENDING.b)return;\n"
"var epoch=ENG_EPOCH;ENG_REFRESHING=true;\n"
"fetch('/trace/status',{cache:'no-store'}).then(function(r){if(!r.ok)throw Error('Trace status unavailable');return r.json()}).then(function(j){\n"
"if(epoch!==ENG_EPOCH||CURTAB!=='eng')return;\n"
"if(j.gen!==undefined&&LASTGEN!==null&&j.gen!==LASTGEN)return;\n"
"ENG_STATUS=j;engPopulate();}).catch(function(e){if(epoch===ENG_EPOCH)engMsg(e.message);}).finally(function(){ENG_REFRESHING=false;});}\n"
"function engChoose(which,slot){\n"
"if(ENG_PENDING[which])return;\n"
"ENG_EPOCH++;ENG_DATA=null;engDraw();ENG_FILE[which]=null;ENG_SEL['l'+which]=null;\n"
"ENG_PENDING[which]=true;var epoch=ENG_EPOCH,gen=LASTGEN;\n"
"var sel=document.getElementById(which==='a'?'engCA':'engCB');sel.disabled=true;\n"
"fetch('/trace/select?which='+which.toUpperCase()+'&car='+slot+'&gen='+gen,{cache:'no-store'})\n"
".then(function(r){if(!r.ok)throw Error('Recorder selection failed; refresh and retry');return r.json()})\n"
".then(function(j){if(gen!==LASTGEN||j.gen!==gen)return;ENG_STATUS=j;\n"
"engMsg('Recorder '+which.toUpperCase()+' changed; wait for a full lap.');})\n"
".catch(function(e){engMsg(e.message);})\n"
".finally(function(){ENG_PENDING[which]=false;sel.disabled=false;engPopulate();engRefresh();});}\n"
"function engPopulate(){\n"
"var j=ENG_STATUS;if(!j||!j.cars)return;\n"
"if(ENG_DATA&&((!ENG_DATA.fileA&&ENG_DATA.metaA.rev!==j.cars[0].rev)||(!ENG_DATA.fileB&&ENG_DATA.metaB.rev!==j.cars[1].rev))){ENG_EPOCH++;ENG_DATA=null;engDraw();engMsg('Recording identity changed; reload completed laps.');}\n"
"var sa=document.getElementById('engCA'),sb=document.getElementById('engCB');\n"
"var ca=(LD&&LD.c)?LD.c.filter(function(c){return c.p>0}):[];\n"
"function fillSel(sel,which){if(ENG_PENDING[which])return;var t=j.cars[which==='a'?0:1];var cur=t&&t.slot>=0?t.slot:-1;ENG_SEL[which]=cur;var h='<option value=\"-1\">Automatic / waiting</option>';\n"
"for(var i=0;i<ca.length;i++)h+='<option value=\"'+ca[i].id+'\">P'+ca[i].p+' '+es(ca[i].n)+'</option>';\n"
"sel.innerHTML=h;\n"
"if(cur>=0&&ca.some(function(c){return c.id===cur}))sel.value=cur;\n"
"else sel.value=-1;\n"
"sel.onchange=function(){engChoose(which,parseInt(sel.value,10));};}\n"
"fillSel(sa,'a');fillSel(sb,'b');\n"
"engFillLaps('a');engFillLaps('b');\n"
"var avail=[];for(var k=0;k<j.cars.length;k++){var t=j.cars[k];\n"
"avail.push((k?'B':'A')+' '+es(t.n||'-')+': '+(t.unavailable&&t.unavailable.length?('unavailable: '+t.unavailable.join(', ')):'all ledger channels seen'));}\n"
"document.getElementById('engAvail').textContent=avail.join(' | ');engLive();}\n"
"function engLive(){\n"
"var progress=[],live=[];\n"
"function val(v,scale){return Number.isFinite(v)?(v*(scale||1)).toFixed(2):'unavailable';}\n"
"ENG_STATUS.cars.forEach(function(t,i){\n"
"var c=LD&&LD.c&&LD.c.find(function(c){return c.id===t.slot;});\n"
"var name=(i?'B':'A')+' '+(t.n||'waiting');\n"
"var pct=c&&ENG_STATUS.lap_len>0?Math.max(0,Math.min(100,c.tp2/ENG_STATUS.lap_len*100)):0;\n"
"progress.push(name+': '+(t.open?'recording lap '+t.open_lap+' ('+pct.toFixed(0)+'%, '+t.open_samples+' samples)':'waiting for moving on-track samples')+'; '+(t.laps?t.laps.length:0)+' completed laps.');\n"
"if(c){var vf=c.vf||0;\n"
"live.push('<div><b>'+es(name)+'</b> — LIVE snapshot · speed '+((vf&1)?val(c.s,3.6):'unavailable')+' km/h · throttle '+((vf&2)?val(c.th,100):'unavailable')+'% · brake '+((vf&4)?val(c.br,100):'unavailable')+'% · steering '+((vf&8)?val(c.steer):'unavailable')+' · lateral '+((vf&32)?val(c.glat):'unavailable')+' m/s² · longitudinal '+((vf&64)?val(c.glon):'unavailable')+' m/s²</div>');}\n"
"});\n"
"document.getElementById('engLive').innerHTML=live.join('');\n"
"document.getElementById('engProgress').textContent=progress.join(' | ')+' Changing a car restarts its recorder: finish the current partial lap, then one full lap. Choose completed laps and Load for distance charts.';\n"
"}\n"
"function engFillLaps(which){\n"
"var j=ENG_STATUS;if(!j||!j.cars)return;\n"
"var idx=which=='a'?0:1;var t=j.cars[idx];\n"
"var sel=document.getElementById(which=='a'?'engLA':'engLB');\n"
"var h='';var laps=(t&&t.used&&t.slot===ENG_SEL[which]&&t.laps)?t.laps.filter(function(l){return l.flags&1}).sort(function(a,b){return a.lap-b.lap}):[];\n"
"for(var i=0;i<laps.length;i++)h+='<option value=\"'+laps[i].lap+'\">L'+laps[i].lap+' '+fmLap(laps[i].t)+(laps[i].flags&2?' (pit)':'')+(laps[i].flags&4?' (breaks)':'')+'</option>';\n"
"sel.innerHTML=h;\n"
"if(!laps.length){sel.innerHTML='<option value=\"\">no completed laps recorded yet</option>';ENG_SEL['l'+which]=null;return;}\n"
"var cur=ENG_SEL['l'+which];\n"
"if(cur!==null&&laps.some(function(l){return l.lap===cur}))sel.value=cur;else sel.value=laps[laps.length-1].lap;\n"
"ENG_SEL['l'+which]=parseInt(sel.value,10);\n"
"sel.onchange=function(){ENG_SEL['l'+which]=parseInt(sel.value,10);};}\n"
"function engSector(n){\n"
"if(!ENG_STATUS||!LD)return;if(ENG_DATA&&ENG_DATA.metaA.track!==LD.trk){engMsg('Sector boundaries unavailable for this imported track.');return;}var sc1=LD.sc1,sc2=LD.sc2,ll=ENG_DATA?ENG_DATA.lapLen:LD.lapm;\n"
"if(!(sc1>0&&sc2>sc1&&ll>sc2)){engMsg('Sector boundaries are not loaded (.AIW unread).');return;}\n"
"if(n==1)ENG_INT=[0,sc1];else if(n==2)ENG_INT=[sc1,sc2];else ENG_INT=[sc2,ll];\n"
"engDraw();engStats();}\n"
"function engIntClear(){ENG_INT=[null,null];engDraw();engStats();}\n"
"function engZoomReset(){ENG_ZOOM=null;engDraw();}\n"
"function engWheel(e){if(!ENG_DATA)return;e.preventDefault();\n"
"var cv=document.getElementById('engC');var r=cv.getBoundingClientRect();\n"
"var x=(e.clientX-r.left)/r.width;var v0=ENG_ZOOM?ENG_ZOOM[0]:0,v1=ENG_ZOOM?ENG_ZOOM[1]:ENG_DATA.lapLen;\n"
"var d=v0+x*(v1-v0);var f=e.deltaY>0?1.15:1/1.15;\n"
"var n0=d-(d-v0)*f,n1=d+(v1-d)*f;\n"
"if(n1-n0<50){n0=d-25;n1=d+25;}\n"
"if(n0<0)n0=0;if(n1>ENG_DATA.lapLen)n1=ENG_DATA.lapLen;\n"
"ENG_ZOOM=[n0,n1];engDraw();}\n"
"function engXtoD(x,W){var v0=ENG_ZOOM?ENG_ZOOM[0]:0,v1=ENG_ZOOM?ENG_ZOOM[1]:ENG_DATA.lapLen;return v0+x/W*(v1-v0);}\n"
"function engClick(e){if(!ENG_DATA)return;var cv=document.getElementById('engC');var r=cv.getBoundingClientRect();\n"
"var d=engXtoD((e.clientX-r.left),r.width);\n"
"if(ENG_INT[0]===null||ENG_INT[1]!==null)ENG_INT=[d,null];else ENG_INT[1]=d;\n"
"engDraw();engStats();}\n"
"function engMove(e){if(!ENG_DATA)return;var cv=document.getElementById('engC');var r=cv.getBoundingClientRect();\n"
"var d=engXtoD((e.clientX-r.left),r.width);\n"
"var i=Math.round(d/ENG_DATA.step);if(i<0)i=0;if(i>=ENG_DATA.n)i=ENG_DATA.n-1;\n"
"function gv(side,k){var v=ENG_DATA[side].ch[k][i];return v===null?'-':v.toFixed(2);}\n"
"function dv(k){var a=ENG_DATA.a.ch[k][i],b=ENG_DATA.b.ch[k][i];if(a===null||b===null)return '-';return (b-a).toFixed(3);}\n"
"document.getElementById('engCursor').textContent='d='+d.toFixed(0)+'m  A/B speed '+gv('a','s')+'/'+gv('b','s')+\n"
"'  thr '+gv('a','th')+'/'+gv('b','th')+'  brk '+gv('a','br')+'/'+gv('b','br')+\n"
"'  steer '+gv('a','st')+'/'+gv('b','st')+'  lat '+gv('a','gl')+'/'+gv('b','gl')+\n"
"'  lon '+gv('a','go')+'/'+gv('b','go')+'  yawrate '+gv('a','yr')+'/'+gv('b','yr')+\n"
"'  time delta '+dv('elapsed')+'s';}\n"
"function engFile(input,side){var f=input.files&&input.files[0];if(!f)return;\n"
"var rd=new FileReader();rd.onload=function(){\n"
"try{var j=JSON.parse(rd.result);\n"
"if(!j||j.schema!=='f1c.trace/1'||!j.samples){engMsg('Not a f1c.trace/1 export.');return;}\n"
"engValidateTrace(j);ENG_EPOCH++;ENG_FILE[side]={samples:j.samples,meta:j};\n"
"engMsg('File loaded as '+side.toUpperCase()+': '+j.driver+' lap '+j.lap+'.');engLoad();\n"
"}catch(ex){engMsg('File rejected: '+ex.message);}};\n"
"rd.readAsText(f);}\n"
"function engValidateTrace(j){\n"
"if(j.schema!=='f1c.trace/1'||!Array.isArray(j.samples)||j.samples.length<30||j.samples.length>2400||\n"
"!(j.flags&1)||!Number.isFinite(j.start_sim)||!(j.lap_len>0)||typeof j.track!=='string'||!j.track)\n"
"throw Error('Trace requires complete-lap, timing and track metadata; re-export with rc.2.');\n"
"var prev=-Infinity;\n"
"j.samples.forEach(function(s){if(!Number.isFinite(s.t)||!Number.isFinite(s.d)||s.t<=prev||s.d<0||s.d>j.lap_len||!Number.isInteger(s.f)||!(s.f&1))throw Error('Invalid trace sample');prev=s.t;\n"
"[['s',2],['th',4],['br',8],['st',16],['gl',32],['go',64],['gv',128],['y',256],['yr',512]].forEach(function(k){if((s.f&k[1])&&!Number.isFinite(s[k[0]]))throw Error('Invalid channel');});\n"
"if((s.f&1024)&&(!Array.isArray(s.tr)||s.tr.length!==4||s.tr.some(function(v){return !Number.isFinite(v)||v<0||v>1})))throw Error('Invalid tread');});\n"
"if(j.samples[0].d>50||j.samples[j.samples.length-1].d<j.lap_len-50||j.start_sim>j.samples[0].t)throw Error('Partial lap');\n"
"}\n"
"function engLoad(){\n"
"var epoch=++ENG_EPOCH;ENG_DATA=null;if(ENG_PENDING.a||ENG_PENDING.b){engMsg('Recorder selection pending.');return;}\n"
"var st=ENG_STATUS;if(!st){engMsg('No trace status yet - is a session running?');return;}\n"
"var step=parseFloat(document.getElementById('engStep').value)||5;\n"
"var lapLen=(ENG_FILE.a&&ENG_FILE.a.meta.lap_len)||(ENG_FILE.b&&ENG_FILE.b.meta.lap_len)||st.lap_len;\n"
"if(!(lapLen>0)){engMsg('Lap length unknown (no track loaded).');return;}\n"
"function side(name){if(ENG_FILE[name]){var m=ENG_FILE[name].meta;\n"
"return Promise.resolve({g:cmpGrid(ENG_FILE[name].samples,m.lap_len||lapLen,step,m.start_sim),meta:m,file:1});}\n"
"var lap=ENG_SEL['l'+name];if(lap===null||lap===undefined)return Promise.resolve(null);\n"
"var which=name=='a'?'A':'B',expected=st.cars[name=='a'?0:1];\n"
"return fetch('/trace/data?car='+which+'&lap='+lap).then(function(r){return r.json()}).then(function(j){\n"
"if(j.error)return null;if(j.gen!==LASTGEN||j.gen!==st.gen||j.slot!==ENG_SEL[name]||j.rev!==expected.rev)return null;\n"
"engValidateTrace(j);return {g:cmpGrid(j.samples,j.lap_len,step,j.start_sim),meta:j,file:0};});}\n"
"engMsg('');\n"
"Promise.all([side('a'),side('b')]).then(function(res){\n"
"if(epoch!==ENG_EPOCH)return;\n"
"if(!res[0]||!res[1]){engMsg('Load two recorded laps (or files) first. Completed laps appear after a lap crossing.');return;}\n"
"if(res[0].meta.track!==res[1].meta.track||Math.abs(res[0].meta.lap_len-res[1].meta.lap_len)>0.5)throw Error('Traces are from different tracks.');\n"
"var gA=res[0].g,gB=res[1].g;step=gA.step;\n"
"var n=Math.min(gA.n,gB.n);\n"
"ENG_DATA={lapLen:lapLen,step:step,n:n,a:gA,b:gB,metaA:res[0].meta,metaB:res[1].meta,fileA:res[0].file,fileB:res[1].file,\n"
"delta:[]};\n"
"for(var i=0;i<n;i++){var ea=gA.ch.elapsed[i],eb=gB.ch.elapsed[i];\n"
"ENG_DATA.delta[i]=(ea===null||eb===null)?null:(eb-ea);}\n"
"engDraw();engStats();}).catch(function(e){if(epoch===ENG_EPOCH)engMsg('Comparison failed: '+e.message);});}\n"
"function engSeries(){return [\n"
"['s','speed m/s'],['th','throttle'],['br','brake'],['st','steer (AI input)'],\n"
"['gl','lateral accel m/s2'],['go','longitudinal accel m/s2'],['yr','yaw rate rad/s'],['tmin','tread min (0..1)']];}\n"
"function engVal(side,i,key){\n"
"if(key=='tmin'){var g=ENG_DATA[side];var v=null;\n"
"for(var w=0;w<4;w++){var t=g.ch['tr'+w][i];if(t!==null&&(v===null||t<v))v=t;}return v;}\n"
"return ENG_DATA[side].ch[key][i];}\n"
"function engDraw(){var cv=document.getElementById('engC');if(!cv)return;\n"
"if(!ENG_DATA){cv.style.display='none';cv.getContext('2d').clearRect(0,0,cv.width,cv.height);return;}cv.style.display='block';\n"
"var W=cv.width=cv.clientWidth||900,H=cv.height=560,ctx=cv.getContext('2d');\n"
"ctx.clearRect(0,0,W,H);ctx.font='10px monospace';\n"
"var rows=engSeries(),rh=Math.floor((H-24)/rows.length);\n"
"var v0=ENG_ZOOM?ENG_ZOOM[0]:0,v1=ENG_ZOOM?ENG_ZOOM[1]:ENG_DATA.lapLen;\n"
"function X(d){return (d-v0)/(v1-v0)*W;}\n"
"for(var r=0;r<rows.length;r++){\n"
"var key=rows[r][0],y0=8+r*rh;\n"
"var lo=null,hi=null;\n"
"for(var i=0;i<ENG_DATA.n;i++){var a=engVal('a',i,key),b=engVal('b',i,key);\n"
"if(a!==null&&(lo===null||a<lo))lo=a;if(a!==null&&(hi===null||a>hi))hi=a;\n"
"if(b!==null&&(lo===null||b<lo))lo=b;if(b!==null&&(hi===null||b>hi))hi=b;}\n"
"if(lo===null){lo=0;hi=1;}\n"
"if(hi-lo<1e-6){hi=lo+1;}\n"
"var pad=(hi-lo)*0.1;lo-=pad;hi+=pad;\n"
"function Y(v){return y0+rh-4-(v-lo)/(hi-lo)*(rh-14);}\n"
"ctx.strokeStyle='#1d242e';ctx.beginPath();ctx.moveTo(0,y0+rh-1);ctx.lineTo(W,y0+rh-1);ctx.stroke();\n"
"ctx.fillStyle='#8a97a6';ctx.fillText(rows[r][1]+'  ['+lo.toFixed(1)+'..'+hi.toFixed(1)+']',4,y0+10);\n"
"function line(side,color,dash){ctx.strokeStyle=color;ctx.setLineDash(dash?[4,3]:[]);ctx.beginPath();var pen=0;\n"
"for(var i=0;i<ENG_DATA.n;i++){var d=i*ENG_DATA.step;if(d<v0||d>v1){pen=0;continue;}\n"
"var v=engVal(side,i,key);if(v===null){pen=0;continue;}\n"
"var x=X(d),y=Y(v);if(!pen){ctx.moveTo(x,y);pen=1;}else ctx.lineTo(x,y);}\n"
"ctx.stroke();ctx.setLineDash([]);}\n"
"line('a','#3fb6ff',0);line('b','#ffa43f',1);\n"
"}\n"
"if(ENG_INT[0]!==null){var i0=ENG_INT[0],i1=ENG_INT[1]===null?ENG_INT[0]:ENG_INT[1];\n"
"ctx.fillStyle='rgba(63,182,255,0.12)';ctx.fillRect(X(i0),0,Math.max(1,X(i1)-X(i0)),H);}\n"
"if(ENG_DATA.delta){ctx.fillStyle='#8a97a6';ctx.fillText('A solid blue / B dashed orange. Bottom band shows time delta (B-A) at the cursor.',4,H-4);}\n"
"}\n"
"function median(a){if(!a.length)return 0;var s=a.slice().sort(function(x,y){return x-y});var m=Math.floor(s.length/2);return s.length%2?s[m]:(s[m-1]+s[m])/2;}\n"
"function engStats(){\n"
"var el=document.getElementById('engStats');if(!el)return;if(!ENG_DATA){el.innerHTML='';return;}\n"
"var i0=ENG_INT[0],i1=ENG_INT[1];\n"
"function at(side,key,i){var v=engVal(side,i,key);return v===null?null:v;}\n"
"function intervalStats(side){if(i0===null)return null;\n"
"if(i1===null)i1=i0;\n"
"var ia=Math.max(0,Math.round(Math.min(i0,i1)/ENG_DATA.step)),ib=Math.min(ENG_DATA.n-1,Math.round(Math.max(i0,i1)/ENG_DATA.step));\n"
"var entry=null,mn=null,exit=null,brake=null,thr=null;\n"
"for(var i=ia;i<=ib;i++){var v=at(side,'s',i);\n"
"if(v!==null){if(entry===null)entry=v;if(mn===null||v<mn)mn=v;exit=v;}\n"
"var b=at(side,'br',i);if(b!==null&&b>0.05&&brake===null)brake=i*ENG_DATA.step;\n"
"var t=at(side,'th',i);if(t!==null&&t>0.5&&thr===null)thr=i*ENG_DATA.step;}\n"
"return {entry:entry,mn:mn,exit:exit,brake:brake,thr:thr,ia:ia,ib:ib};}\n"
"var sa=intervalStats('a'),sb=intervalStats('b');\n"
"var h='<table><tr><th>interval</th><th>entry m/s</th><th>min m/s</th><th>exit m/s</th><th>brake@m</th><th>thr>50%@m</th><th>lap time</th></tr>';\n"
"function row(nm,st,meta){if(!st)return '<tr><td>'+nm+'</td><td colspan=\"6\">no interval selected</td></tr>';\n"
"return '<tr><td>'+nm+'</td><td>'+(st.entry===null?'-':st.entry.toFixed(1))+'</td><td>'+(st.mn===null?'-':st.mn.toFixed(1))+'</td><td>'+(st.exit===null?'-':st.exit.toFixed(1))+'</td><td>'+(st.brake===null?'-':st.brake.toFixed(0))+'</td><td>'+(st.thr===null?'-':st.thr.toFixed(0))+'</td><td>'+(meta&&meta.lap_time?fmLap(meta.lap_time):'-')+'</td></tr>';}\n"
"h+=row('A',sa,ENG_DATA.metaA);h+=row('B',sb,ENG_DATA.metaB);\n"
"h+='</table>';\n"
"if(i0!==null){var ii=Math.min(ENG_DATA.n-1,Math.round((i1===null?i0:i1)/ENG_DATA.step));\n"
"var dd=ENG_DATA.delta[ii];\n"
"h+='<div>time delta at interval end (B-A): '+(dd===null?'unavailable':dd.toFixed(3)+' s')+'</div>';}\n"
"el.innerHTML=h;\n"
"var meta=document.getElementById('engMeta');\n"
"var c=LD||{};\n"
"var lines=['current session conditions (not recorded lap conditions): '+((SL[c.ty]||'?')+' '+phLbl(c.ph))+' | track '+(c.trk||'-')+' | air '+((c.at>-900)?c.at.toFixed(1)+'C':'-')+' | track '+((c.trkt>-900)?c.trkt.toFixed(1)+'C':'-')+' | wetness '+((c.tw||0)*100).toFixed(0)+'% | AI '+((c.ai||0))+'%'];\n"
"function sideMeta(tag,m,file){if(!m)return tag+': -';\n"
"return tag+': '+es(m.driver||m.n||'-')+' lap '+m.lap+' | car '+es(m.car||m.cn||'-')+' | '+(m.ai?'AI control':'non-AI control')+' | lap '+(m.lap_time?fmLap(m.lap_time):'-')+(file?' (file)':'');}\n"
"lines.push(sideMeta('A',ENG_DATA.metaA,ENG_DATA.fileA));\n"
"lines.push(sideMeta('B',ENG_DATA.metaB,ENG_DATA.fileB));\n"
"var mismatch=[];\n"
"if(ENG_DATA.metaA&&ENG_DATA.metaB){\n"
"var ca=(ENG_DATA.metaA.car||ENG_DATA.metaA.cn||''),cb=(ENG_DATA.metaB.car||ENG_DATA.metaB.cn||'');\n"
"if(ca&&cb&&ca!==cb)mismatch.push('different cars');\n"
"if(ENG_DATA.metaA.lap_len&&ENG_DATA.metaB.lap_len&&Math.abs(ENG_DATA.metaA.lap_len-ENG_DATA.metaB.lap_len)>1)mismatch.push('different track length');}\n"
"lines.push(mismatch.length?('WARNING: '+mismatch.join(', ')+' - a causal comparison needs matched conditions.'):'Car and track metadata match. Weather, fuel, tyres, AI settings and mod/setup identity are not verified as matched; record them in run notes.');\n"
"meta.innerHTML=lines.map(function(s){return '<div>'+s+'</div>'}).join('');\n"
"var sums=[];\n"
"function runSummary(name,traceCars,idx){if(!traceCars||!traceCars[idx])return null;\n"
"var laps=(traceCars[idx].laps||[]).slice();if(!laps.length)return null;\n"
"var clean=laps.filter(function(l){return !(l.flags&6)});\n"
"var times=clean.map(function(l){return l.t});\n"
"return {name:name,n:laps.length,clean:times.length,median:median(times),spread:times.length?(Math.max.apply(null,times)-Math.min.apply(null,times)):0,\n"
"excluded:laps.length-times.length,list:laps};}\n"
"var r1=runSummary('A session',ENG_STATUS&&ENG_STATUS.cars,0);\n"
"var r2=runSummary('B session',ENG_STATUS&&ENG_STATUS.cars,1);\n"
"function rs(x){if(!x)return '<div>'+'run summary: no completed laps yet'+'</div>';\n"
"return '<div>'+x.name+': '+x.n+' laps, '+x.clean+' clean; median '+fmLap(x.median)+', spread '+x.spread.toFixed(3)+' s; excluded (pit/breaks) '+x.excluded+'</div>';}\n"
"meta.innerHTML+=rs(r1)+rs(r2);\n"
"}\n"
"/* ---- race pace tab ---- */\n"
"var CURTAB='pw',rpTimer=null,incTimer=null;\n"
"function swTab(t){CURTAB=t;\n"
"document.getElementById('tbPW').className=t=='pw'?'on':'';\n"
"document.getElementById('tbTP').className=t=='tp'?'on':'';\n"
"document.getElementById('tbDbg').className=t=='debug'?'on':'';\n"
"document.getElementById('tbRC').className=t=='rc'?'on':'';\n"
"document.getElementById('tbEng').className=t=='eng'?'on':'';\n"
"document.getElementById('swPW').className=t=='pw'?(PWMAX?'pw shown maxed':'pw shown'):'pw';\n"
"document.getElementById('mainArea').style.display=t=='pw'?'none':'flex';\n"
"document.getElementById('swTP').style.display=t=='tp'?'block':'none';\n"
"document.getElementById('swDebug').style.display=t=='debug'?'block':'none';\n"
"document.getElementById('swRC').style.display=t=='rc'?'block':'none';\n"
"document.getElementById('swEng').style.display=t=='eng'?'block':'none';\n"
"if(t=='pw'){loadRacePace();if(!rpTimer)rpTimer=setInterval(loadRacePace,3000);}\n"
"else if(rpTimer){clearInterval(rpTimer);rpTimer=null;}\n"
"if(t=='debug'||t=='rc'||t=='pw'){loadIncidents();if(!incTimer)incTimer=setInterval(loadIncidents,3000);}\n"
"else if(incTimer){clearInterval(incTimer);incTimer=null;}\n"
"document.getElementById('rp').style.display=t=='eng'?'none':'';\n"
"if(t=='eng')engOpen();else {if(engTimer){clearInterval(engTimer);engTimer=null;}ENG_EPOCH++;}\n"
"if(LD)up(LD);}\n"
"/* Pane maximize: replaces the old separate Standings / Race Pace / Track Map tabs.\n"
"   Click a pane header (or its expand glyph) to blow it up to the full grid. */\n"
"var PWMAX=null;\n"
"window.pwMax=function(name){PWMAX=(PWMAX===name)?null:name;\n"
"var g=document.getElementById('swPW');\n"
"g.className=PWMAX?'pw shown maxed':'pw shown';\n"
"var ps=g.getElementsByClassName('pwp');\n"
"for(var i=0;i<ps.length;i++){\n"
"var isOn=PWMAX&&ps[i].getAttribute('data-pane')===PWMAX;\n"
"ps[i].className=ps[i].className.replace(/ ?onMax/,'')+(isOn?' onMax':'');}\n"
"if(LD)up(LD);}\n"
"function fmLap(s){if(!s||s<=0)return'-';var m=Math.floor(s/60),sc=(s%60).toFixed(3);\n"
"return m+':'+(sc<10?'0':'')+sc;}\n"
"function loadRacePace(){fetch('/laps').then(function(r){return r.json()})\n"
".then(function(j){if(j.gen!==undefined&&LASTGEN!==null&&j.gen!==LASTGEN)return;PACE_CACHE=j.cars||[];\n"
"if(CURTAB=='pw')renderPWPace();}).catch(function(){});}\n"
"\n"
"/* ---- PIT WALL: all panes rendered every tick, nothing hidden behind a tab ---- */\n"
"var pwTow={els:{},hash:{}},PACE_CACHE=[],pwGx=null,pwTx=null;\n"
"function pwTrackStatus(){var m=0;\n"
"for(var i=0;i<INC_CACHE.length;i++)if(INC_CACHE[i].ty=='RC_MODE')m=INC_CACHE[i].b;\n"
"return m;}\n"
"function renderPitWall(d,ca,fc){\n"
"if(!d)return;\n"
"var mode=pwTrackStatus();\n"
"var stTxt=['GREEN','VSC','RED FLAG'][mode]||'GREEN';\n"
"var stCls=mode==2?'r':mode==1?'y':'g';\n"
"/* --- session pane --- */\n"
"document.getElementById('pwSessT').textContent=(SL[d.ty]||'?')+' / '+phLbl(d.ph);\n"
"var w2=WL[condIdx(d)];\n"
"var sh='<div style=\"text-align:center;margin-bottom:7px\"><span class=\"tstat '+stCls+'\">'+stTxt+'</span></div>';\n"
"sh+='<div class=\"kv\">';\n"
"sh+='<div class=\"k\">LAP</div><div class=\"v\">'+(d.ll||0)+' / '+(d.tl||0)+'</div>';\n"
"sh+='<div class=\"k\">CARS</div><div class=\"v\">'+(d.vc||0)+'</div>';\n"
"sh+='<div class=\"k\">RACE</div><div class=\"v\">'+(d.ac?'RUNNING':'STANDBY')+'</div>';\n"
"sh+=flagStrip(d,fc);\n"
"sh+='<div class=\"k\">WEATHER</div><div class=\"v\">'+w2.t+'</div>';\n"
"sh+='<div class=\"k\">WETNESS</div><div class=\"v\">'+((d.tw||0)*100).toFixed(0)+'%</div>';\n"
"sh+='<div class=\"k\">AIR T</div><div class=\"v\">'+((d.at>-900)?d.at.toFixed(1)+'\\u00b0C':'-')+'</div>';\n"
"sh+='<div class=\"k\">TRACK T</div><div class=\"v\">'+((d.trkt>-900)?d.trkt.toFixed(1)+'\\u00b0C':'-')+'</div>';\n"
"sh+='<div class=\"k\">AI</div><div class=\"v\">'+(d.ai||0)+'%</div>';\n"
"sh+='<div class=\"k\">MODE</div><div class=\"v\">'+(d.cm?'CHAMP':'SINGLE GP')+'</div>';\n"
"if(d.flapid>=0){var flDrv=ca.filter(function(x){return x.id===d.flapid;})[0];\n"
"sh+='<div class=\"k\" style=\"color:#b000ff\">FASTEST LAP</div><div class=\"v\" style=\"color:#b000ff\">'+\n"
"fs(d.flapt)+' \\u2014 '+(flDrv?code3(flDrv.n):'?')+'</div>';}\n"
"if(d.dotdid>=0){var doDrv=ca.filter(function(x){return x.id===d.dotdid;})[0];\n"
"sh+='<div class=\"k\" title=\"Our own composite score, not the game\\'s internal calculation.\">DRIVER OF DAY</div><div class=\"v\">'+\n"
"(doDrv?code3(doDrv.n):'?')+' (score '+d.dotdsc+')</div>';}\n"
"sh+='</div>';\n"
"document.getElementById('pwSess').innerHTML=sh;\n"
"/* --- telemetry pane (focus car) --- */\n"
"document.getElementById('pwTeleN').textContent=fc?('P'+fc.p+' '+code3(fc.n)):'-';\n"
"var te=document.getElementById('pwTele');\n"
"if(!pwGx){te.innerHTML='<div class=\"miniG\"><canvas id=\"pwGS\" width=\"208\" height=\"208\"></canvas>'+\n"
"'<canvas id=\"pwGT\" width=\"208\" height=\"208\"></canvas></div>'+\n"
"'<div class=\"brs\" style=\"margin-top:6px;padding:6px 8px\">'+\n"
"'<div class=\"br\"><div class=\"l\">THR</div><div class=\"fw\"><div class=\"fl th\" id=\"pwTh\"></div></div><div class=\"v\" id=\"pwThV\">0%</div></div>'+\n"
"'<div class=\"br\"><div class=\"l\">BRK</div><div class=\"fw\"><div class=\"fl br\" id=\"pwBk\"></div></div><div class=\"v\" id=\"pwBkV\">0%</div></div>'+\n"
"'</div><div class=\"kv\" id=\"pwTeleKV\" style=\"margin-top:7px\"></div>';\n"
"pwGx=document.getElementById('pwGS').getContext('2d');\n"
"pwTx=document.getElementById('pwGT').getContext('2d');}\n"
"if(fc){\n"
"dg(pwGx,208,208,fc.s*3.6,360,'km/h',0,360,280,1);\n"
"var mrp2=fc.mr>0?fc.mr*RPMF:19000;dg(pwTx,208,208,fc.r*RPMF,mrp2,'RPM',0,mrp2,mrp2*.85,1);\n"
"var tp2=Math.min(100,Math.max(0,(fc.th||0)*100)),bp2=Math.min(100,Math.max(0,(fc.br||0)*100));\n"
"document.getElementById('pwTh').style.width=tp2+'%';\n"
"document.getElementById('pwThV').textContent=tp2.toFixed(0)+'%';\n"
"document.getElementById('pwBk').style.width=bp2+'%';\n"
"document.getElementById('pwBkV').textContent=bp2.toFixed(0)+'%';\n"
"var s3f=(fc.ll>0&&fc.ls1>0&&fc.ls2>0)?(fc.ll-fc.ls1-fc.ls2):0;\n"
"var kh='<div class=\"k\">GEAR</div><div class=\"v\">'+(fc.g>0?fc.g:'N')+'</div>';\n"
"kh+='<div class=\"k\">LAP</div><div class=\"v\">'+(fc.l||0)+'</div>';\n"
"kh+='<div class=\"k\">LAST</div><div class=\"v\">'+fs(fc.ll)+'</div>';\n"
"kh+='<div class=\"k\">BEST</div><div class=\"v\">'+fs(fc.bl)+'</div>';\n"
"kh+='<div class=\"k\">S1 / S2 / S3</div><div class=\"v\">'+fs(fc.ls1)+' / '+fs(fc.ls2)+' / '+(s3f>0?fs(s3f):'-')+'</div>';\n"
/* C02 fix: the old row subtracted a COMPLETE personal-best sector from a
   PARTIAL current sector, which reads about -30 s at sector entry. Now the
   running sector shows its plain elapsed time and the delta is only shown
   for a sector that actually COMPLETED (against the PB that existed before
   it). The true distance-aligned live delta lives in the Engineering tab. */
"if(fc.secel>0&&fc.lsn)kh+='<div class=\"k\" title=\"Elapsed time in the sector this car is in right now. No reference is subtracted from a running sector.\">SECTOR S'+fc.lsn+' ELAPSED</div>'+\n"
"'<div class=\"v\">'+fc.secel.toFixed(3)+'</div>';\n"
"if(fc.lspn)kh+='<div class=\"k\" title=\"Completed sector time minus the personal best that existed before it. Green = faster.\">LAST S'+fc.lspn+' vs PB</div>'+\n"
"'<div class=\"v\" style=\"'+(fc.lspb<=0?'color:#2ecc71':'color:#ff4136')+'\">'+(fc.lspb<=0?'':'+' )+fc.lspb.toFixed(3)+'</div>';\n"
"kh+='<div class=\"k\">TIRE</div><div class=\"v\">'+tireLabel(fc.tc)+'</div>';\n"
"kh+='<div class=\"k\">FUEL/LAP</div><div class=\"v\">'+(fc.fc?fc.fc.toFixed(2):'-')+'</div>';\n"
"/* fuel projection, folded in from the old Engineer tab */\n"
"var lapsLeft=Math.max(0,(d.tl||0)-(fc.l||0));\n"
"var need=fc.fc?fc.fc*lapsLeft:0;\n"
"kh+='<div class=\"k\">LAPS LEFT</div><div class=\"v\">'+lapsLeft+'</div>';\n"
"kh+='<div class=\"k\">FUEL NEEDED</div><div class=\"v\">'+(need>0?need.toFixed(1):'-')+'</div>';\n"
"if(fc.nf>0){var shortF=need>fc.nf;\n"
"kh+='<div class=\"k\">NEXT STINT</div><div class=\"v\" style=\"'+(shortF?'color:#ff4136':'color:#2ecc71')+'\">'+\n"
"fc.nf.toFixed(1)+(shortF?' SHORT':' OK')+'</div>';}\n"
/* FUEL MODEL: rolling per-lap consumption (last up to 8 laps) and the laps
   of autonomy it implies, distinct from the instantaneous FUEL/LAP rate and
   the single-lap projection above. compute_v4_features 'favg'/'faut'. */
"if(fc.favg>0)kh+='<div class=\"k\" title=\"Rolling average fuel used per lap over the last up to 8 completed laps, and how many more laps that pace can go at the current fuel level. Different from the instantaneous FUEL/LAP rate above.\">FUEL AVG/AUTONOMY</div>'+\n"
"'<div class=\"v\">'+fc.favg.toFixed(2)+'/lap \\u00b7 '+(fc.faut>=0?fc.faut.toFixed(1)+' laps':'-')+'</div>';\n"
/* PIT ANTICIPATION: cb (call_to_box) is the game's OWN intention flag --
   shown first and unconditionally. boxsoon/boxwhy is OUR heuristic guess
   for cars the game hasn't called in yet; the reason string makes clear
   it is a guess, never confused with the real flag. */
"kh+='<div class=\"k\" title=\"The game\\'s own pit-intention flag (veh call_to_box).\">CALLED TO BOX</div>'+\n"
"'<div class=\"v\" style=\"'+(fc.cb?'color:#fc0':'')+'\">'+(fc.cb?'YES':'no')+'</div>';\n"
"if(fc.boxsoon)kh+='<div class=\"k\" title=\"Heuristic only -- not a game flag. Based on fuel autonomy or (player only) tyre life crossing a conservative threshold.\">BOX SOON?</div>'+\n"
"'<div class=\"v\" style=\"color:#fc0\">likely \\u2014 '+es(fc.boxwhy)+'</div>';\n"
/* TOP SPEED: running max of the REAL speed field. veh max_speed (fc.ms
   above) is a fixed 99999 sentinel in every live sample so far, so it is
   never usable as a top-speed readout -- this replaces it. */
"kh+='<div class=\"k\" title=\"Running max of the real speed field. The game\\'s own max_speed field reads a fixed 99999 sentinel and cannot be used.\">TOP SPEED (lap/prev/session)</div>'+\n"
"'<div class=\"v\">'+(fc.topl*3.6).toFixed(0)+' / '+(fc.toppl*3.6).toFixed(0)+' / '+(fc.tops*3.6).toFixed(0)+' km/h</div>';\n"
/* RADAR: real metres to the nearest car ahead/behind on track (lap_dist
   plus a full lap per lap-number difference), not an interpolated time gap. */
"if(fc.gai>=0||fc.gbi>=0){\n"
"var ah=fc.gai>=0?ca.filter(function(x){return x.id===fc.gai;})[0]:null;\n"
"var bh=fc.gbi>=0?ca.filter(function(x){return x.id===fc.gbi;})[0]:null;\n"
"kh+='<div class=\"k\" title=\"Real distance along the track to the nearest car ahead/behind, corrected for lap number. Not derived from time gaps.\">RADAR</div>'+\n"
"'<div class=\"v\" style=\"font-size:11px\">'+\n"
"(ah?('\\u2191 '+code3(ah.n)+' '+fc.ga.toFixed(0)+'m'):'\\u2191 \\u2014')+'<br>'+\n"
"(bh?('\\u2193 '+code3(bh.n)+' '+fc.gb.toFixed(0)+'m'):'\\u2193 \\u2014')+'</div>';}\n"
/* Session-wide badges: fastest lap holder and Driver of the Day, both only
   rendered on whichever car currently holds them. DOTD is explicitly OUR
   OWN transparent composite score, not the game's own hidden calculation --
   see compute_v4_features for the exact formula. */
"if(d.flapid===fc.id)kh+='<div class=\"k\" style=\"color:#b000ff\">FASTEST LAP</div><div class=\"v\" style=\"color:#b000ff\">'+fs(d.flapt)+' \\u2014 SESSION BEST</div>';\n"
"if(d.dotdid===fc.id)kh+='<div class=\"k\" title=\"Our own composite score (positions gained + fastest lap + clean laps), NOT the game\\'s internal Driver of the Day calculation, which is a private formula we have not decoded.\">DRIVER OF THE DAY</div>'+\n"
"'<div class=\"v\">score '+d.dotdsc+' (dashboard metric)</div>';\n"
"var dT=DMG[fc.dt||0];\n"
"kh+='<div class=\"k\">DAMAGE</div><div class=\"v\" style=\"'+(fc.dt?'color:'+dT.c:'')+'\">'+\n"
"(fc.dt?dT.t:'NONE')+'</div>';\n"
"kh+='<div class=\"k\">FRONT WING</div><div class=\"v\" style=\"'+(fc.fw?'color:#fc0':'')+'\">'+\n"
"(fc.fw?'DETACHED':'intact')+'</div>';\n"
"kh+='<div class=\"k\">REAR WING</div><div class=\"v\" style=\"'+(fc.rw?'color:#ff4136':'')+'\">'+\n"
"(fc.rw?'DETACHED':'intact')+'</div>';\n"
"kh+='<div class=\"k\">ENGINE</div><div class=\"v\" style=\"'+((fc.ed||fc.rc==1)?'color:#ff4136':'')+'\">'+\n"
"(fc.ed?('DEAD'+(fc.rc==1?' (ENGINE)':'')):'running')+'</div>';\n"
"/* EngineAngVel (veh+0xA0, FACT): the crankshaft's angular velocity. On a healthy\n"
"   engine it tracks RPM almost exactly (1803.7 vs 1804), so it is not extra\n"
"   information while running \\u2014 its value is as a CROSS-CHECK. RPM alone reads 0\n"
"   for a car that is merely stopped; RPM 0 AND angvel 0 means the crank is not\n"
"   turning at all, i.e. the engine is genuinely dead. Shown next to RPM so the\n"
"   two can be compared: a divergence is the interesting case. */\n"
"kh+='<div class=\"k\" title=\"Crankshaft angular velocity (veh+0xA0). Mirrors RPM on a healthy engine; both at 0 = engine truly dead.\">ENG ANGVEL</div>'+\n"
"'<div class=\"v\">'+(fc.eav!==undefined?fc.eav.toFixed(0):'-')+' <span style=\"opacity:.5\">(rpm '+(fc.r!==undefined?(fc.r*RPMF).toFixed(0):'-')+')</span></div>';\n"
"/* engine.ini LifetimeOilTemp=(123.43,2.90): engine life HALVES per 2.9C above 123.43,\n"
"   so oil temp is a real reliability lever, not decoration. Baseline differs per\n"
"   engine manufacturer, hence no fixed 'normal' colour band. */\n"
"kh+='<div class=\"k\">OIL TEMP</div><div class=\"v\" style=\"'+(fc.ot>123?'color:#ff4136':(fc.ot>115?'color:#fc0':''))+'\">'+\n"
"(fc.ot!==undefined?fc.ot.toFixed(1)+'\\u00b0':'-')+'</div>';\n"
"kh+='<div class=\"k\">WATER TEMP</div><div class=\"v\">'+(fc.wt!==undefined?fc.wt.toFixed(1)+'\\u00b0':'-')+'</div>';\n"
/* For the player the game's own gauge values are available and are NOT the
   same numbers as the per-car veh+0x3D10/0x3D14 fields above (screen-confirmed
   2026-07-28: wheel display said 66/23 while those fields said 134.8/66.1).
   Show both rather than silently picking one. */
"if(fc.id===d.ps&&d.otp!==undefined)\n"
"kh+='<div class=\"k\" title=\"Oil and water as the in-car steering-wheel display reads them (globals 0x7C9174 / 0x7C91B4, Kelvin). Screen-confirmed. The OIL/WATER rows above come from per-car vehicle fields that do NOT match this gauge.\">GAUGE OIL / WATER</div>'+\n"
"'<div class=\"v\">'+(d.otp>-900?d.otp.toFixed(1)+'\\u00b0':'-')+' / '+(d.wtp>-900?d.wtp.toFixed(1)+'\\u00b0':'-')+'</div>';\n"
"kh+='<div class=\"k\">FUEL LEFT</div><div class=\"v\">'+(fc.fl>0?fc.fl.toFixed(1)+' L':'-')+'</div>';\n"
/* Brake and tyre temps come from a PLAYER-ONLY global block (0x7C7AB8, stride
   1124) — the game keeps no per-AI thermal mirror, so these rows only appear
   when the focused car IS the player. -999 = block not being simulated.
   Tyre temp is 3 carcass zones per wheel; the spread across the three is the
   camber/pressure signal, so all three are shown, not an average. */
"if(fc.id===d.ps&&d.btmp){\n"
"var wn=['FL','FR','RL','RR'];\n"
"var tOk=function(v){return v!==undefined&&v>-900;};\n"
"var bcol=function(v){return v>750?'color:#ff4136':(v>600?'color:#fc0':'');};\n"
"var bh='';for(var w=0;w<4;w++)bh+=(w?' / ':'')+'<span style=\"'+bcol(d.btmp[w])+'\">'+(tOk(d.btmp[w])?d.btmp[w].toFixed(0):'-')+'</span>';\n"
"kh+='<div class=\"k\" title=\"Brake disc temperature per wheel, Celsius. Global 0x7C7AB8 + 1124*wheel, Kelvin in memory. Wheel order FL,FR,RL,RR is DERIVED from the HUD draw order, not poke-proven.\">BRAKE TEMP</div>'+\n"
"'<div class=\"v\" style=\"font-size:11px\">'+wn.join(' / ')+'<br>'+bh+'</div>';\n"
"var th='';for(var w2=0;w2<4;w2++){var z=d.ttmp[w2]||[];\n"
"th+='<div>'+wn[w2]+' '+[0,1,2].map(function(i){return tOk(z[i])?z[i].toFixed(0):'-';}).join(' ')+'</div>';}\n"
"kh+='<div class=\"k\" title=\"Tyre temperature, Celsius, 3 carcass zones per wheel (same 3-strip the in-car HUD draws). Global 0x7C7A00 + 1124*wheel + 8*zone.\">TYRE TEMP</div>'+\n"
"'<div class=\"v\" style=\"font-size:11px\">'+th+'</div>';\n"
/* Same colour bands the game's own wear bar uses (sub_49E7B0): green above
   0.8, yellow 0.5-0.8, red below. The two blister thresholds are marked
   because they are the commentary trigger edges, not just cosmetic. */
"var wcol=function(v){return v>0.8?'':(v>0.5?'color:#fc0':'color:#ff4136');};\n"
"var dh='';for(var w3=0;w3<4;w3++){var t=d.trd[w3];\n"
"dh+=(w3?' / ':'')+'<span style=\"'+wcol(t)+'\">'+(t*100).toFixed(1)+'</span>';}\n"
"kh+='<div class=\"k\" title=\"Tyre life remaining, percent. Raw tread is 1.0 fresh / 0.75 destroyed; this is the game\\'s own (tread-0.75)*4 normalization. 70% = light blister call, 40% = severe blister call, 0% = blowout.\">TYRE LIFE</div>'+\n"
"'<div class=\"v\" style=\"font-size:11px\">'+wn.join(' / ')+'<br>'+dh+'%</div>';\n"
"if(d.bls&&(d.bls[0]||d.bls[1]||d.bls[2]||d.bls[3]))\n"
"kh+='<div class=\"k\" title=\"Per-corner blister flag. WARNING: this array is indexed by corner with no car index and the physics that sets it runs for every car, so it is shared/last-writer, not necessarily the player.\">BLISTER FLAGS</div>'+\n"
"'<div class=\"v\" style=\"font-size:11px;opacity:.7\">'+wn.filter(function(_,i){return d.bls[i];}).join(' ')+' <span style=\"opacity:.6\">(shared, not per-car)</span></div>';\n"
"}\n"
"kh+='<div class=\"k\">CLASSIFICATION</div><div class=\"v\">'+reasonTxt(fc)+'</div>';\n"
"kh+='<div class=\"k\">TYRE LIFE %</div><div class=\"v\">'+wearTxt(fc,d)+'</div>';\n"
/* The countdown only ticks for cars with veh+0x32C8 == 1. Live: every AI reads 1,
   the HUMAN reads 0 — a player engine blew up with its counter untouched at
   23245.8 the whole time. Showing that frozen number as "engine life" would be a
   lie, so say so instead. */
"kh+='<div class=\"k\" title=\"Seconds of engine life left (veh+0x3D70). Only meaningful for cars the wear countdown runs for: the tick is gated on veh+0x32C8==1, true for AI, false for the human driver.\">ENGINE LIFE</div>'+\n"
"'<div class=\"v\" style=\"'+(fc.wg===1?lifeCol(fc.lpe):'')+'\">'+\n"
"(fc.wg===1?lifeTxt(fc.lfe,fc.lpe,fc.ll)\n"
":'<span style=\"opacity:.6\" title=\"veh+0x32C8='+fc.wg+' so the wear pass never runs on this car: its countdown is frozen and its OilTemp readout is stale. A human car fails by the overheat path instead.\">n/a \\u2014 not simulated</span>')+'</div>';\n"
"kh+='<div class=\"k\">BRK / GBX / SUS</div><div class=\"v\">'+\n"
"'<span style=\"'+lifeCol(fc.lpb)+'\">'+lifeShort(fc.lfb)+'</span> / '+\n"
"'<span style=\"'+lifeCol(fc.lpg)+'\">'+lifeShort(fc.lfg)+'</span> / '+\n"
"'<span style=\"'+lifeCol(fc.lps)+'\">'+lifeShort(fc.lfs)+'</span></div>';\n"
"if(fc.dm)kh+='<div class=\"k\" style=\"color:#ff4136\">PROGNOSIS</div>'+\n"
"'<div class=\"v\" style=\"color:#ff4136\" title=\"'+doomWhy(fc)+'\">WILL RETIRE \\u2014 '+doomTag(fc)+'</div>';\n"
"kh+='<div class=\"k\">TRK LIMITS</div><div class=\"v\">'+(fc.tl||0)+'</div>';\n"
"kh+='<div class=\"k\">PENALTIES</div><div class=\"v\">'+(fc.pn||0)+'</div>';\n"
"document.getElementById('pwTeleKV').innerHTML=kh;}\n"
"/* --- timing tower --- */\n"
"var bs1=999,bs2=999,bs3=999,byId={},ids=[];\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;\n"
"if(c2.ls1>0&&c2.ls1<bs1)bs1=c2.ls1;if(c2.ls2>0&&c2.ls2<bs2)bs2=c2.ls2;\n"
"var x3=(c2.ll>0&&c2.ls1>0&&c2.ls2>0)?(c2.ll-c2.ls1-c2.ls2):0;\n"
"if(x3>0&&x3<bs3)bs3=x3;}\n"
"var ldCar=null,ahead={};\n"
"var ordered=ca.filter(function(x){return x.p>0}).sort(function(a,b){return a.p-b.p});\n"
"for(var i=0;i<ordered.length;i++){if(i===0)ldCar=ordered[i];else ahead[ordered[i].id]=ordered[i-1];}\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;byId[c2.id]=c2;ids.push(c2.id);}\n"
"document.getElementById('pwTowH').innerHTML='<tr><th>P</th><th>Driver</th><th>Gap</th><th>Int</th>'+\n"
"'<th>Last</th><th>Best</th><th>&Delta;</th><th>S1</th><th>S2</th><th>S3</th>'+\n"
"'<th>Spd</th><th>Grd</th><th>Tyr</th><th>Fuel</th><th>Stp</th><th>St</th></tr>';\n"
"diffRows(document.getElementById('pwTowB'),pwTow,ids,function(id){var c2=byId[id];\n"
"var s3v=(c2.ll>0&&c2.ls1>0&&c2.ls2>0)?(c2.ll-c2.ls1-c2.ls2):0;\n"
"var pc2=(c2.pt0>0?1:0)+(c2.p1>0?1:0)+(c2.p2>0?1:0);\n"
"var inPit2=c2.pt>=2;\n"
"var stv=c2.fs==1?'FIN':c2.fs==2?'DNF':c2.fs==3?'DSQ':c2.rt?'RET':inPit2?'PIT':c2.cb?'BOX':\n"
"(c2.pn>0?'P'+c2.pn:'');\n"
"if(c2.ed)stv='<span class=\"dtag\" style=\"background:#ff4136\" title=\"Engine stopped: rpm 0 and EngineAngVel 0 while still on track'+(c2.rc==1?' \\u2014 classified ENGINE failure':'')+'\">ENGINE</span> '+stv;\n"
"else if(c2.dm)stv='<span class=\"dtag\" style=\"background:#ff4136\" title=\"'+doomWhy(c2)+'\">'+doomTag(c2)+'</span> '+stv;\n"
"else if(c2.dt)stv=dmgTag(c2)+' '+stv;\n"
"var pcol=c2.p==1?'background:#ffd700;color:#000':c2.p==2?'background:#c0c0c0;color:#000':\n"
"c2.p==3?'background:#cd7f32;color:#000':'';\n"
"var hh='<td><span class=\"pos\" style=\"'+pcol+'\">'+c2.p+'</span></td><td>'+es(c2.n)+'</td>';\n"
"hh+='<td>'+gapStr(c2,ldCar)+'</td><td>'+intStr(c2,ahead[c2.id])+'</td>';\n"
"hh+='<td>'+fs(c2.ll)+'</td><td>'+fs(c2.bl)+'</td><td>'+fdelta(c2.bl,c2.ll)+'</td>';\n"
"hh+='<td'+(c2.ls1>0&&c2.ls1<=bs1+1e-4?' class=\"sct\"':'')+'>'+fs(c2.ls1)+'</td>';\n"
"hh+='<td'+(c2.ls2>0&&c2.ls2<=bs2+1e-4?' class=\"sct\"':'')+'>'+fs(c2.ls2)+'</td>';\n"
"hh+='<td'+(s3v>0&&s3v<=bs3+1e-4?' class=\"sct\"':'')+'>'+(s3v>0?fs(s3v):'-')+'</td>';\n"
"hh+='<td>'+(c2.s?(c2.s*3.6).toFixed(0):'-')+'</td><td>'+(c2.gp>0?c2.gp:'-')+'</td>';\n"
"hh+='<td><span class=\"tire '+tireLabel(c2.tc)+'\">'+tireLabel(c2.tc)+'</span></td>';\n"
"hh+='<td>'+(c2.fc?c2.fc.toFixed(2):'-')+'</td><td>'+(pc2>0?pc2:'-')+'</td><td>'+stv+'</td>';\n"
"return hh;},function(id){var c2=byId[id];\n"
"var inPit3=c2.pt>=2;\n"
"var cl='cur';if(c2.fs==2||c2.fs==3||c2.rt)cl+=' dn';else if(c2.dm)cl+=' doomed';else if(inPit3)cl+=' pit';\n"
/* Two different cars, two different markers, and neither is a position:
     plr = the HUMAN's car (derived slot `ps`)
     vw  = the car the in-game camera is on (`pi`, the spectated SLOT)
   `pi` was previously compared against each car's POSITION. Those agree only when
   a car's slot happens to equal its position -- e.g. Barrichello, slot 4 and P4,
   which is why it looked correct -- and diverge everywhere else: R.Schumacher is
   slot 6 but P5, so spectating him highlighted P6 (TesterA, slot 5). */
"if(c2.id===FOCUS)cl+=' foc';if(c2.id===d.ps)cl+=' plr';if(c2.id===d.pi)cl+=' vw';return cl;});\n"
"document.getElementById('pwTowN').textContent=ids.length+' cars';\n"
"/* --- race control feed --- */\n"
"var rel=INC_CACHE.filter(function(e){return e.ty!=='POS';});\n"
"var rh2='';var lst=rel.slice().reverse().slice(0,60);\n"
"for(var i=0;i<lst.length;i++){var m2=rcMsg(lst[i]);if(!m2)continue;\n"
"rh2+='<div class=\"rcline '+m2.sev.replace('sev','s')+'\"><span class=\"lp\">L'+(lst[i].lp||0)+'</span>'+\n"
"'<span>'+m2.t+'</span></div>';}\n"
"if(!rh2)rh2='<div class=\"rcline\"><span>No bulletins yet this session.</span></div>';\n"
"document.getElementById('pwRC').innerHTML=rh2;\n"
"document.getElementById('pwRcN').textContent=rel.length+' msg';\n"
"/* --- pace + map --- */\n"
"renderPWPace();uMPw(ca);}\n"
"/* Transposed vs the full Race Pace tab: drivers as ROWS, last 6 laps as columns.\n"
"   22 driver-columns don't fit a narrow pit-wall pane; 6 lap-columns do. */\n"
"function renderPWPace(){\n"
"var cars=PACE_CACHE.slice().sort(function(a,b){return(a.p||999)-(b.p||999)});\n"
"var hEl=document.getElementById('pwPaceH'),bEl=document.getElementById('pwPaceB');\n"
"if(cars.length===0){hEl.innerHTML='';\n"
"bEl.innerHTML='<tr><td class=\"pcell\" style=\"text-align:left\">No completed laps yet</td></tr>';return;}\n"
"var maxL=0;for(var i=0;i<cars.length;i++)maxL=Math.max(maxL,(cars[i].laps||[]).length);\n"
"var NL=(PWMAX==='pace')?40:6,from=Math.max(0,maxL-NL);\n"
"document.getElementById('pwPaceN').textContent='last '+Math.min(NL,maxL)+' laps';\n"
"var overallBest=1e9;\n"
"for(var i=0;i<cars.length;i++)(cars[i].laps||[]).forEach(function(v){if(v>0&&v<overallBest)overallBest=v;});\n"
"var hh='<tr><th>Drv</th>';\n"
"for(var l=from;l<maxL;l++)hh+='<th>L'+(l+1)+'</th>';\n"
"hh+='<th>Best</th></tr>';hEl.innerHTML=hh;\n"
"var bd='';\n"
"for(var i=0;i<cars.length;i++){var c2=cars[i],lp=c2.laps||[];\n"
"var mn=1e9;lp.forEach(function(v){if(v>0&&v<mn)mn=v;});\n"
"bd+='<tr class=\"cur\" onclick=\"focusCar('+c2.id+')\">';\n"
"bd+='<td class=\"pcell\" style=\"text-align:left\" title=\"'+es(c2.n)+'\">'+code3(c2.n)+'</td>';\n"
"for(var l=from;l<maxL;l++){var v=lp[l];\n"
"if(v===undefined||v<=0){bd+='<td class=\"pcell\">-</td>';continue;}\n"
"var col=v<=overallBest+1e-4?'color:#b000ff;font-weight:700':\n"
"(v<=mn+1e-4?'color:#2ecc71;font-weight:700':'');\n"
"bd+='<td class=\"pcell\" style=\"'+col+'\">'+fmLap(v)+'</td>';}\n"
"bd+='<td class=\"pcell\" style=\"color:var(--t2)\">'+(mn<1e9?fmLap(mn):'-')+'</td></tr>';}\n"
"bEl.innerHTML=bd;}\n"
"\n"
"/* ---- team principal tab: both team cars + immediate rivals' strategy only \\u2014\n"
"   no full-field telemetry, matched by car/team name (cn field) ---- */\n"
"function tpCard(me,ca){\n"
"if(!me)return'<div class=\"c1\"><h4>\\u2014</h4>No car found</div>';\n"
"var ahead=null,behind=null;\n"
"for(var i=0;i<ca.length;i++){if(ca[i].p===me.p-1)ahead=ca[i];if(ca[i].p===me.p+1)behind=ca[i];}\n"
"var pitCount=(me.pt0>0?1:0)+(me.p1>0?1:0)+(me.p2>0?1:0);\n"
"return '<div class=\"c1 cur\" onclick=\"focusCar('+me.id+')\"><h4>P'+me.p+' \\u2014 '+es(me.n)+'</h4>'+\n"
"'Tire: '+(tireLabel(me.tc))+' | Fuel: '+(me.fc?me.fc.toFixed(2)+'/lap':'-')+\n"
"' | Stops: '+pitCount+'<br>'+\n"
"'Status: '+(me.pt>=2?'IN PIT':me.cb?'BOX CALLED':me.fs==2?'DNF':me.fs==3?'DSQ':'On track')+\n"
"' | Damage: '+(me.cc==0?'None':me.cc==1?'Minor':'Severe')+'<br>'+\n"
"'Ahead: '+(ahead?es(ahead.n)+' ('+intStr(me,ahead)+')':'\\u2014 leader \\u2014')+'<br>'+\n"
"'Behind: '+(behind?es(behind.n)+' ('+intStr(behind,me)+')':'\\u2014 last car \\u2014')+\n"
"'</div>';}\n"
/* PIT BOARD: dashboard equivalent of the crew's hand-held board
   (f1c_gridSignEngine.doTheDraw draws an actual bitmap sign onto the track
   model — that has no meaning in a web page, so this is a from-scratch
   panel styled like one, built from data we already have: real position,
   real gap ahead/behind in metres (compute_v4_features radar), laps
   remaining, and the game's own pit-intention flag). */
"function pitBoardHTML(me,d){if(!me)return'';\n"
"var lapsLeft=Math.max(0,(d.tl||0)-(me.l||0));\n"
"var gA=me.gai>=0?me.ga.toFixed(0)+'M':'\\u2014';\n"
"var gB=me.gbi>=0?me.gb.toFixed(0)+'M':'\\u2014';\n"
"return '<div class=\"pitboard\" title=\"Dashboard equivalent of a pit board \\u2014 not a recreation of the game\\'s own sign graphics, built from real telemetry (position, radar gap, laps left, box status)\">'+\n"
"'<div class=\"pbrow pbbig\">P'+(me.p||'-')+'</div>'+\n"
"'<div class=\"pbrow\">GAP F '+gA+' &nbsp; GAP R '+gB+'</div>'+\n"
"'<div class=\"pbrow\">LAPS '+lapsLeft+' &nbsp; '+tireLabel(me.tc)+'</div>'+\n"
"'<div class=\"pbrow\" style=\"'+(me.cb?'color:#ff4136':'')+'\">'+(me.cb?'BOX BOX BOX':(me.boxsoon?'BOX SOON?':'STAY OUT'))+'</div>'+\n"
"'</div>';}\n"
"function renderTeamPrincipal(d,ca){\n"
"var me=null;for(var i=0;i<ca.length;i++)if(ca[i].id===d.ps){me=ca[i];break;}\n"
"if(!me){document.getElementById('swTP').innerHTML='<div class=\"warn\">No player car detected this session.</div>';return;}\n"
"var mate=null;\n"
/* C15: prefer the reviewed broker team identity (tid, ABI-8 FNV hash of the
   roster team name) and only fall back to the display-name match; never take
   an empty-string match. */
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.id===me.id||c2.p<=0)continue;\n"
"if(me.tid&&me.tid!==2166136261&&c2.tid&&c2.tid===me.tid){mate=c2;break;}}\n"
"if(!mate)for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.id===me.id||c2.p<=0)continue;\n"
"if(me.cn&&c2.cn&&c2.cn===me.cn){mate=c2;break;}}\n"
"var h='<div class=\"foc-label\">TEAM: '+es(me.cn||'\\u2014')+'</div>';\n"
"if(!mate)h+='<div class=\"warn\">No teammate found (broker team identity, then car/team name match against \\u201c'+\n"
"es(me.cn||'')+'\\u201d) \\u2014 showing your car only.</div>';\n"
"if(!me.fc||(mate&&!mate.fc))h+='<div class=\"warn\">Fuel/tire detail can read stale or \\u2019-\\u2019 for a car the game hasn\\'t rendered closely recently (off-camera AI LOD) \\u2014 engine data-availability limit, not a UI bug. Updates once that car is back on-screen or is the focus car. Tire compound labels (S/M/H/W/I) are best-effort: tireChoice is an index into a per-session tire list (per F1CUILive source), not a fixed enum \\u2014 unrecognized indices show as \\u2019C&lt;n&gt;\\u2019 instead of a guessed letter.</div>';\n"
"h+='<div class=\"cardrow\">'+tpCard(me,ca)+(mate?tpCard(mate,ca):'')+'</div>';\n"
"h+=pitBoardHTML(me,d);\n"
"var teamPos=[me.p,mate?mate.p:null].filter(function(x){return x;});\n"
"var rivals=ca.filter(function(c2){return c2.p>0&&teamPos.indexOf(c2.p)<0&&\n"
"(Math.abs(c2.p-me.p)<=2||(mate&&Math.abs(c2.p-mate.p)<=2));})\n"
".sort(function(a,b){return a.p-b.p;});\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Rivals in Strategic Range</h4>';\n"
"h+='<table class=\"dtab\"><tr><th style=\"text-align:left\">Drv</th>'+\n"
"'<th>Pos</th><th>Tire</th><th>Fuel</th><th>Stops</th><th>Status</th></tr>';\n"
"for(var i=0;i<rivals.length;i++){var c2=rivals[i];\n"
"var pc=(c2.pt0>0?1:0)+(c2.p1>0?1:0)+(c2.p2>0?1:0);\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"text-align:left\">'+es(c2.n)+'</td><td class=\"nml\">P'+c2.p+'</td>'+\n"
"'<td class=\"nml\">'+(tireLabel(c2.tc))+'</td><td class=\"nml\">'+(c2.fc?c2.fc.toFixed(2):'-')+\n"
"'</td><td class=\"nml\">'+pc+'</td><td class=\"nml\">'+\n"
"(c2.pt>=2?'PIT':c2.fs==2?'DNF':c2.fs==3?'DSQ':'-')+'</td></tr>';}\n"
"if(rivals.length===0)h+='<tr><td>No rivals within strategic range right now.</td></tr>';\n"
"h+='</table></div>';\n"
"document.getElementById('swTP').innerHTML=h;}\n"
"\n"
"/* ---- full debug tab: everything, all cars, raw values ---- */\n"
"var INC_CACHE=[];\n"
"function loadIncidents(){fetch('/incidents').then(function(r){return r.json()})\n"
".then(function(j){if(j.gen!==undefined&&LASTGEN!==null&&j.gen!==LASTGEN)return;INC_CACHE=j.ev||[];\n"
"if(CURTAB=='debug')renderDebug(LD,(LD.c||[]),null);\n"
"if(CURTAB=='rc')renderRaceControl(LD,(LD.c||[]));}).catch(function(){});}\n"
/* ---- SENSOR VERIFICATION ------------------------------------------------
   Everything in the wheel thermal block was derived from decompiled render
   code and read back live; only the wheel ORDER and the oil/water gauge are
   screen-confirmed so far. This panel exists to close the rest by driving:
   it tracks every signal's session min/max so a frozen or dead signal is obvious,
   and it runs the two checks that need vehicle motion (brake heating under
   braking, and which tyre-temp ZONE index is the outboard shoulder).

   The zone check needs no knowledge of which way is left: during cornering
   the two hottest wheels ARE the outer pair, and on a loaded outer tyre the
   outboard shoulder is the hottest zone. So the winning zone index on those
   wheels is the outboard one. Turn direction is still bucketed by yaw sign
   because the sign convention of the game's heading vector is unproven --
   the checklist asks the driver which bucket was a left-hander. */
"var WN=['FL','FR','RL','RR'];\n"
"function vfmt(v){return (typeof v!=='number')?'-':(Math.abs(v)>=1000?v.toFixed(0):v.toFixed(2));}\n"
"function verifUpdate(d,fc){\n"
"if(!window.VERIF)window.VERIF={mm:{},ord:[],buk:{},pd:null,pt:0,brk:{hot:0,cold:0}};\n"
"var V=window.VERIF,S={};\n"
"if(d.ttmp)for(var a=0;a<4;a++)for(var z=0;z<3;z++)S['TyreTemp '+WN[a]+' z'+z]=d.ttmp[a][z];\n"
"if(d.trd)for(var b=0;b<4;b++)S['TyreLife% '+WN[b]]=d.trd[b]*100;\n"
"if(d.btmp)for(var c=0;c<4;c++)S['BrakeTemp '+WN[c]]=d.btmp[c];\n"
"S['Gauge Oil']=d.otp;S['Gauge Water']=d.wtp;\n"
"if(fc){S['veh OilTemp']=fc.ot;S['veh WaterTemp']=fc.wt;S['RPM']=fc.r*RPMF;S['Speed']=fc.s;\n"
"S['Throttle']=fc.th;S['Brake']=fc.br;S['Fuel']=fc.fl;S['EngLife']=fc.lfe;S['BrkLife']=fc.lfb;}\n"
"for(var k in S){var v=S[k];if(typeof v!=='number'||v<-900)continue;\n"
"var m=V.mm[k];if(!m){V.mm[k]=m={lo:v,hi:v};V.ord.push(k);}\n"
"if(v<m.lo)m.lo=v;if(v>m.hi)m.hi=v;m.cur=v;}\n"
/* brake-input vs brake-temp: remember the hottest disc seen while actually
   on the brakes, and while off them, so the pair proves causation not drift */
"if(fc&&d.btmp){var mx=Math.max(d.btmp[0],d.btmp[1],d.btmp[2],d.btmp[3]);\n"
"if(fc.br>0.5&&mx>V.brk.hot)V.brk.hot=mx;\n"
"if(fc.br<0.05&&fc.s>50&&(V.brk.cold===0||mx<V.brk.cold))V.brk.cold=mx;}\n"
/* yaw rate from the heading vector, bucketed by sign */
"if(fc&&fc.dx!==undefined&&d.ttmp){var ang=Math.atan2(fc.dy,fc.dx);\n"
"if(V.pd!==null&&d.tk>V.pt){var da=ang-V.pd;\n"
"while(da>Math.PI)da-=2*Math.PI;while(da<-Math.PI)da+=2*Math.PI;\n"
"var yr=da/((d.tk-V.pt)/1000);\n"
"if(fc.s>60&&Math.abs(yr)>0.15){var sg=yr>0?'yaw+':'yaw-';\n"
"var bk=V.buk[sg];if(!bk)V.buk[sg]=bk={n:0,wh:[0,0,0,0],zn:[0,0,0]};\n"
"bk.n++;\n"
/* mean tyre temp per wheel this sample, to find the loaded (outer) pair */
"var wm=[];for(var w2=0;w2<4;w2++)wm.push((d.ttmp[w2][0]+d.ttmp[w2][1]+d.ttmp[w2][2])/3);\n"
"for(var w3=0;w3<4;w3++)bk.wh[w3]+=wm[w3];\n"
/* the two hottest wheels are the outer pair; on those, which zone leads? */
"var idx=[0,1,2,3].sort(function(p,q){return wm[q]-wm[p];}).slice(0,2);\n"
"for(var ii=0;ii<2;ii++){var zz=d.ttmp[idx[ii]],best=0;\n"
"for(var z2=1;z2<3;z2++)if(zz[z2]>zz[best])best=z2;bk.zn[best]++;}\n"
"}}\n"
"V.pd=ang;V.pt=d.tk;}\n"
"return V;}\n"
"function renderVerif(d,fc){\n"
"var V=verifUpdate(d,fc),h='';\n"
"h+='<div class=\"sec\"><h4>Sensor Verification \\u2014 Player</h4>';\n"
"h+='<div class=\"warn\">Wheel order FL,FR,RL,RR and the oil/water gauge are screen-confirmed. Tyre-temp ZONE order is NOT \\u2014 the checks below settle it. A signal whose min and max are identical after a lap is either frozen or not simulated.</div>';\n"
"h+='<table class=\"dtab\"><tr><th style=\"text-align:left\">Signal</th><th>Now</th><th>Min</th><th>Max</th><th>Range</th></tr>';\n"
"for(var i=0;i<V.ord.length;i++){var k=V.ord[i],m=V.mm[k],rg=m.hi-m.lo;\n"
"h+='<tr><td style=\"text-align:left\">'+k+'</td><td class=\"nml\">'+vfmt(m.cur)+'</td><td class=\"nml\">'+vfmt(m.lo)+\n"
"'</td><td class=\"nml\">'+vfmt(m.hi)+'</td><td class=\"nml\" style=\"'+(rg>1e-6?'':'color:#ff4136')+'\">'+\n"
"(rg>1e-6?vfmt(rg):'FROZEN')+'</td></tr>';}\n"
"h+='</table>';\n"
"h+='<h4 style=\"margin-top:8px\">Cornering \\u2014 zone polarity</h4>';\n"
"var bks=['yaw+','yaw-'],any=0;\n"
"h+='<table class=\"dtab\"><tr><th style=\"text-align:left\">Bucket</th><th>Samples</th><th>Mean tyre temp FL / FR / RL / RR</th><th>Hottest zone on the outer pair</th></tr>';\n"
"for(var bi=0;bi<2;bi++){var bk=V.buk[bks[bi]];if(!bk)continue;any=1;\n"
"var mw=[];for(var w=0;w<4;w++)mw.push((bk.wh[w]/bk.n).toFixed(1));\n"
"var tot=bk.zn[0]+bk.zn[1]+bk.zn[2],zs=[];\n"
"for(var z=0;z<3;z++)zs.push('z'+z+' '+(tot?Math.round(100*bk.zn[z]/tot):0)+'%');\n"
"h+='<tr><td style=\"text-align:left\">'+bks[bi]+'</td><td class=\"nml\">'+bk.n+'</td><td class=\"nml\">'+mw.join(' / ')+\n"
"'</td><td class=\"nml\">'+zs.join('  ')+'</td></tr>';}\n"
"h+='</table>';\n"
"if(!any)h+='<div class=\"inc\">No cornering samples yet \\u2014 needs sustained turns above 60 kph.</div>';\n"
"else h+='<div class=\"inc\">The zone with the highest percentage is the OUTBOARD shoulder. Which bucket was your left-hander tells you which wheels the game calls left.</div>';\n"
"h+='<h4 style=\"margin-top:8px\">Brake heating</h4>';\n"
"h+='<div class=\"inc\">Hottest disc while ON the brakes: <b>'+vfmt(V.brk.hot)+'</b> C &nbsp;|&nbsp; coolest seen at speed OFF the brakes: <b>'+vfmt(V.brk.cold)+'</b> C. '+\n"
"(V.brk.hot>V.brk.cold+10?'<span class=\"gn2\">PASS \\u2014 discs track brake input</span>':'<span style=\"opacity:.6\">inconclusive so far</span>')+'</div>';\n"
"h+='<h4 style=\"margin-top:8px\">Driving checklist</h4>';\n"
"var CK=[\n"
"['Idle 30 s in the pits, engine on','Gauge Oil and Gauge Water both climb; Range column non-zero'],\n"
"['Compare the in-car wheel display to the GAUGE OIL / WATER row on the Pit Wall tab','Same numbers. Already confirmed once \\u2014 repeat after any rebuild'],\n"
"['One hard stop from top speed','BrakeTemp Max jumps, and FL/FR read hotter than RL/RR (front bias)'],\n"
"['Three sustained LEFT corners, then note which bucket above filled','Outer (right) wheels FR+RR show the higher mean temp in that bucket'],\n"
"['Three sustained RIGHT corners','The other bucket fills, and FL+RL lead instead \\u2014 this is what proves the left/right labels'],\n"
"['Drive 3+ full laps','TyreLife% Min drops below its Max; rears usually wear first'],\n"
"['Lock a wheel / long flat-spot slide','One corner TyreTemp spikes well clear of the other three'],\n"
"['Pit for fresh tyres','TyreLife% returns to 100 and the blister flags clear'],\n"
"['Watch the tyre temps during the cornering runs','Whichever zone index wins the outer-pair column is the outboard shoulder \\u2014 that settles z0/z1/z2']\n"
"];\n"
"h+='<table class=\"dtab\"><tr><th style=\"text-align:left\">Do this</th><th style=\"text-align:left\">Expect</th></tr>';\n"
"for(var ci=0;ci<CK.length;ci++)h+='<tr><td style=\"text-align:left\">'+CK[ci][0]+'</td><td style=\"text-align:left\">'+CK[ci][1]+'</td></tr>';\n"
"h+='</table></div>';\n"
"return h;}\n"
"function renderDebug(d,ca,fc){\n"
"if(!d)return;\n"
"ca=ca.slice().sort(function(a,b){return(a.p||999)-(b.p||999)});\n"
"var h=(fc&&fc.id===d.ps)?renderVerif(d,fc):'<div class=\"warn\">Sensor Verification needs the PLAYER car focused \\u2014 the thermal block is player-only.</div>';\n"
"h+='<div class=\"warn\">Pit history caps at 3 stops per car (game only keeps 3 slots) \\u2014 a 4th stop overwrites nothing but has no readable slot.</div>';\n"
"h+='<div class=\"sec\"><h4>Reliability / Damage \\u2014 All Cars</h4><table class=\"dtab\">';\n"
"h+='<tr><th style=\"text-align:left\">Drv</th><th>Dmg</th><th>Reason</th><th title=\"Engine life remaining, veh+0x3D70\">Eng</th><th>Brk</th><th>Gbx</th><th>Sus</th><th>Hits</th><th>TrkLim</th><th>Pen</th><th>Status</th></tr>';\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"text-align:left\">'+es(c2.n)+'</td><td class=\"nml\">'+\n"
"(c2.dt?(DMG[c2.dt].t+(c2.dm?' / WILL RETIRE ('+doomTag(c2)+')':'')):'-')+'</td><td class=\"nml\">'+reasonTxt(c2)+\n"
"'</td><td class=\"nml\" style=\"'+lifeCol(c2.lpe)+'\">'+lifeShort(c2.lfe)+\n"
"'</td><td class=\"nml\" style=\"'+lifeCol(c2.lpb)+'\">'+lifeShort(c2.lfb)+\n"
"'</td><td class=\"nml\" style=\"'+lifeCol(c2.lpg)+'\">'+lifeShort(c2.lfg)+\n"
"'</td><td class=\"nml\" style=\"'+lifeCol(c2.lps)+'\">'+lifeShort(c2.lfs)+\n"
"'</td><td class=\"nml\">'+(c2.hr||0)+'</td><td class=\"nml\">'+(c2.tl||0)+'</td><td class=\"nml\">'+\n"
"(c2.pn||0)+'</td><td class=\"nml\">'+(c2.fs==1?'FIN':c2.fs==2?'DNF':c2.fs==3?'DSQ':c2.rt?'RET':'-')+'</td></tr>';}\n"
"h+='</table></div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Fuel Strategy Projection \\u2014 All Cars</h4><table class=\"dtab\">';\n"
"h+='<tr><th style=\"text-align:left\">Drv</th><th>Rate</th><th>LapsLeft</th><th>ProjNeed</th><th>NextStint</th><th>OK?</th></tr>';\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;\n"
"var ll2=(d.tl||0)-(c2.l||0),need=c2.fc?c2.fc*Math.max(0,ll2):0;\n"
"var ok=(c2.nf>0)?(need<=c2.nf?'<span class=\"gn2\">OK</span>':'<span class=\"rd2\">SHORT</span>'):'-';\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"text-align:left\">'+es(c2.n)+'</td><td class=\"nml\">'+(c2.fc?c2.fc.toFixed(2):'-')+\n"
"'</td><td class=\"nml\">'+ll2+'</td><td class=\"nml\">'+need.toFixed(1)+'</td><td class=\"nml\">'+\n"
"(c2.nf>0?c2.nf.toFixed(1):'-')+'</td><td class=\"nml\">'+ok+'</td></tr>';}\n"
"h+='</table></div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>AI Driver Setups &amp; Stats \\u2014 All Cars</h4>'+\n"
"'<table class=\"dtab\"><tr><th style=\"text-align:left\">Drv</th>'+\n"
"'<th>Grid</th><th>PaceMult</th><th>SpeedCap</th><th>Limiter</th><th>DrvStatus</th></tr>';\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"text-align:left\">'+es(c2.n)+'</td><td class=\"nml\">'+(c2.gp>0?c2.gp:'-')+\n"
"'</td><td class=\"nml\">'+(c2.ap>0.001?c2.ap.toFixed(4):'-')+'</td><td class=\"nml\">'+\n"
"(c2.sc>0.01?c2.sc.toFixed(3):'-')+'</td><td class=\"nml\">'+(c2.rl?'ON':'off')+\n"
"'</td><td class=\"nml\">'+(c2.ds||'-')+'</td></tr>';}\n"
"h+='</table></div>';\n"
/* TOP SPEED / RADAR / FUEL AUTONOMY: real running-max speed (the game's own
   max_speed field is a dead 99999 sentinel, unusable), real on-track gap to
   the car ahead, and the rolling fuel-consumption model, for every car. */
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Top Speed / Radar / Fuel \\u2014 All Cars</h4>'+\n"
"'<table class=\"dtab\"><tr><th style=\"text-align:left\">Drv</th>'+\n"
"'<th title=\"Running max of the real speed field this lap / previous lap / session\">Top (lap/prev/sess)</th>'+\n"
"'<th title=\"Real distance to the car ahead on track\">Gap ahead</th>'+\n"
"'<th title=\"Rolling average fuel per lap, last 8 laps\">Fuel/lap</th>'+\n"
"'<th title=\"Laps of autonomy at current fuel and average consumption\">Autonomy</th></tr>';\n"
"var tsSorted=ca.filter(function(c2){return c2.p>0;}).slice().sort(function(a,b){return(b.tops||0)-(a.tops||0);});\n"
"for(var i=0;i<tsSorted.length;i++){var c2=tsSorted[i];\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"text-align:left\">'+es(c2.n)+'</td>'+\n"
"'<td class=\"nml\">'+(c2.topl*3.6).toFixed(0)+' / '+(c2.toppl*3.6).toFixed(0)+' / '+(c2.tops*3.6).toFixed(0)+'</td>'+\n"
"'<td class=\"nml\">'+(c2.gai>=0?c2.ga.toFixed(0)+'m':'-')+'</td>'+\n"
"'<td class=\"nml\">'+(c2.favg>0?c2.favg.toFixed(2):'-')+'</td>'+\n"
"'<td class=\"nml\">'+(c2.faut>=0?c2.faut.toFixed(1):'-')+'</td></tr>';}\n"
"h+='</table></div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Incident Log (overtakes, limits, penalties, damage, retire, pit, contact)</h4>';\n"
"var evs=INC_CACHE.slice().reverse().slice(0,80);\n"
"for(var i=0;i<evs.length;i++){var e=evs[i];\n"
"var desc=e.ty=='POS'?(e.b<e.a?'gained P'+e.a+'\\u2192P'+e.b:'lost P'+e.a+'\\u2192P'+e.b):\n"
"e.ty=='LIMITS'?'track limits '+e.a+'\\u2192'+e.b:\n"
"e.ty=='PENALTY'?'penalty count '+e.a+'\\u2192'+e.b+' (reason not exposed by the game)':\n"
"e.ty=='PENALTY_TL'?'penalty count '+e.a+'\\u2192'+e.b+' \\u2014 same tick as a track-limits infringement (inferred)':\n"
"e.ty=='CONTACT'?'damage coincided with '+(ca.filter(function(x){return x.id===e.b;})[0]||{n:'car '+e.b}).n+' within 25m (heuristic, not a decoded guilty/victim record)':\n"
"e.ty=='BOXSOON'?'our heuristic now expects a pit stop soon (not the game\\'s own call_to_box flag)':\n"
"e.ty=='DAMAGE'?'damage '+e.a+'\\u2192'+e.b:\n"
"e.ty=='FRONTWING'?'front wing detached (repairable)':\n"
"e.ty=='REARWING'?'rear wing detached (terminal \\u2014 will retire at box)':\n"
"e.ty=='RETIRE'?'retired (state '+e.b+')':\n"
"e.ty=='YELLOW'?'YELLOW raised in flag zone '+(e.a+1):\n"
"e.ty=='GREEN'?'flag zone '+(e.a+1)+' back to green':\n"
"e.ty=='PIT_IN'?'entered pit':e.ty=='PIT_OUT'?'exited pit':e.ty;\n"
"h+='<div class=\"inc\"><span class=\"tk\">L'+e.lp+'</span><span class=\"ty '+e.ty+'\">'+e.ty+'</span>'+\n"
"'<span>'+es(e.n)+' \\u2014 '+desc+'</span></div>';}\n"
"if(evs.length===0)h+='<div class=\"inc\">No events yet this session.</div>';\n"
"h+='</div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>All Fields \\u2014 All Cars (raw)</h4><div style=\"overflow-x:auto\"><table style=\"font-size:8.5px;border-collapse:collapse\">';\n"
"var akeys=[];if(ca.length>0)for(var k in ca[0])if(ca[0].hasOwnProperty(k))akeys.push(k);\n"
"h+='<tr><th style=\"position:sticky;left:0;background:var(--pn);text-align:left;padding:2px 6px\">Drv</th>';\n"
"for(var ki=0;ki<akeys.length;ki++)h+='<th style=\"padding:2px 5px\">'+akeys[ki]+'</th>';h+='</tr>';\n"
"for(var i=0;i<ca.length;i++){var c2=ca[i];if(c2.p<=0)continue;\n"
"h+='<tr class=\"cur\" data-fid=\"'+c2.id+'\" onclick=\"focusCar('+c2.id+')\"><td style=\"position:sticky;left:0;background:var(--bg);text-align:left;padding:2px 6px\">'+es(c2.n)+'</td>';\n"
"for(var ki=0;ki<akeys.length;ki++){var av=c2[akeys[ki]];\n"
"h+='<td class=\"nml\" style=\"padding:2px 5px\">'+(typeof av==='number'?(Math.abs(av)<1000?av.toFixed(3):av.toFixed(0)):String(av).substring(0,14))+'</td>';}\n"
"h+='</tr>';}\n"
"h+='</table></div></div>';\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Raw Session Fields (unfiltered)</h4><div class=\"dbgraw\">';\n"
"for(var k in d)if(d.hasOwnProperty(k)&&k!=='c')h+='<div><b>'+k+'</b>'+String(d[k]).substring(0,20)+'</div>';\n"
"h+='</div></div>';\n"
"if(fc){h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Raw Focus-Car Fields (unfiltered, for offset debugging)</h4><div class=\"dbgraw\">';\n"
"for(var k in fc)if(fc.hasOwnProperty(k))h+='<div><b>'+k+'</b>'+String(fc[k]).substring(0,20)+'</div>';\n"
"h+='</div></div>';}\n"
"document.getElementById('swDebug').innerHTML=h;}\n"
"\n"
"/* ---- FIA-style race control bulletin feed (built from our own incident log \\u2014\n"
"   the game exposes no race-control message data at all, this reformats what we\n"
"   already derive server-side into stewards-bulletin language) ---- */\n"
"function rcMsg(e){\n"
/* Sector flags are the one bulletin here that is NOT derived from our own
   snapshot diffing: the Steward publishes this state itself and the game draws
   the same value as the yellow sprite on its own HUD. */
/* ZONE, not SECTOR. A lap has three timing sectors (S1/S2/S3, already shown in
   the tower); this table is 29 marshalling zones of ~199m. Calling them sectors
   collides with the sector times right next to them on screen. */
"if(e.ty=='YELLOW')return{sev:'sev2',t:'YELLOW FLAG \\u2014 ZONE '+(e.a+1)+'/'+(FLAGZONES||'?')+' \\u2014 SLOW OR STOPPED CAR (Lap '+e.lp+')'};\n"
"if(e.ty=='GREEN')return{sev:'sev0',t:'GREEN FLAG \\u2014 ZONE '+(e.a+1)+'/'+(FLAGZONES||'?')+' CLEAR (Lap '+e.lp+')'};\n"
"if(e.ty=='FLAG')return{sev:'sev1',t:'FLAG STATE '+e.b+' \\u2014 ZONE '+(e.a+1)+' (Lap '+e.lp+') \\u2014 unmapped state, shown raw'};\n"
"if(e.ty=='START')return{sev:'sev0',t:'RACE CONTROL \\u2014 LIGHTS OUT, RACE STARTED'};\n"
"if(e.ty=='HALF')return{sev:'sev0',t:'RACE CONTROL \\u2014 HALFWAY POINT REACHED (Lap '+e.lp+'/'+e.a+')'};\n"
"if(e.ty=='CHEQUERED')return{sev:'sev0',t:'RACE CONTROL \\u2014 CHEQUERED FLAG, LEADER HAS FINISHED (Lap '+e.lp+')'};\n"
"if(e.ty=='LIMITS')return{sev:'sev1',t:'TRACK LIMITS INFRINGEMENT \\u2014 '+es(e.n)+' \\u2014 NOTED (Lap '+e.lp+')'};\n"
"if(e.ty=='PENALTY')return{sev:'sev2',t:'TIME PENALTY \\u2014 '+es(e.n)+' \\u2014 PENALTY COUNT NOW '+e.b+' \\u2014 reason not exposed by the game (Lap '+e.lp+')'};\n"
"if(e.ty=='PENALTY_TL')return{sev:'sev2',t:'TIME PENALTY \\u2014 '+es(e.n)+' \\u2014 PENALTY COUNT NOW '+e.b+' \\u2014 inferred TRACK LIMITS (same-tick correlation) (Lap '+e.lp+')'};\n"
"if(e.ty=='CONTACT')return{sev:'sev1',t:'CONTACT (heuristic) \\u2014 '+es(e.n)+' damage coincided with car '+e.b+' within 25m \\u2014 NOT a decoded guilty/victim record (Lap '+e.lp+')'};\n"
"if(e.ty=='BOXSOON')return{sev:'sevI',t:'PIT WATCH (heuristic) \\u2014 '+es(e.n)+' \\u2014 our own guess, not the game\\'s call_to_box flag (Lap '+e.lp+')'};\n"
"if(e.ty=='ENGINE')return{sev:'sev2',t:'ENGINE \\u2014 '+es(e.n)+\n"
"' \\u2014 SEIZED ON IMPACT, CAR IS OUT (Lap '+e.lp+')'};\n"
"if(e.ty=='FRONTWING')return{sev:'sev1',t:'FRONT WING \\u2014 '+es(e.n)+\n"
"' \\u2014 DETACHED, CAR REPAIRABLE (Lap '+e.lp+')'};\n"
"if(e.ty=='REARWING')return{sev:'sev2',t:'REAR WING \\u2014 '+es(e.n)+\n"
"' \\u2014 DETACHED IN CONTACT, CANNOT BE REPAIRED, RETIREMENT EXPECTED AT PIT BOX (Lap '+e.lp+')'};\n"
"if(e.ty=='DAMAGE')return{sev:e.b>=2?'sev2':'sev1',t:'CAR DAMAGE \\u2014 '+es(e.n)+' \\u2014 '+\n"
"(e.b>=2?'SEVERE / WHEEL LOSS':'DAMAGE REPORTED')+' (Lap '+e.lp+')'};\n"
"if(e.ty=='RETIRE')return e.b==1?{sev:'sev0',t:'CLASSIFICATION \\u2014 '+es(e.n)+' \\u2014 CLASSIFIED FINISHER (Lap '+e.lp+')'}:\n"
"e.b==3?{sev:'sev2',t:'DISQUALIFICATION \\u2014 '+es(e.n)+' \\u2014 DSQ (Lap '+e.lp+')'}:\n"
"{sev:'sev2',t:'RETIREMENT \\u2014 '+es(e.n)+' \\u2014 DID NOT FINISH (Lap '+e.lp+')'};\n"
"if(e.ty=='PIT_IN')return{sev:'sevI',t:'PIT LANE \\u2014 '+es(e.n)+' \\u2014 ENTERED PIT LANE (Lap '+e.lp+')'};\n"
"if(e.ty=='PIT_OUT')return{sev:'sevI',t:'PIT LANE \\u2014 '+es(e.n)+' \\u2014 EXITED PIT LANE (Lap '+e.lp+')'};\n"
"if(e.ty=='RC_PIT')return{sev:'sevI',t:'RACE CONTROL \\u2014 '+es(e.n)+' \\u2014 SUMMONED TO PITS (Lap '+e.lp+')'};\n"
"if(e.ty=='RC_RETIRE')return{sev:'sev2',t:'RACE CONTROL \\u2014 '+es(e.n)+' \\u2014 BLACK-FLAGGED / FORCED RETIREMENT (Lap '+e.lp+')'};\n"
"if(e.ty=='RC_MODE'){var nm=['GREEN','VSC','RED FLAG'];\n"
"return{sev:e.b==2?'sev2':e.b==1?'sev1':'sev0',t:'RACE CONTROL \\u2014 TRACK STATUS: '+(nm[e.b]||'?')};}\n"
"return null;}\n"
"function doAction(type,id,val){var q='type='+type+(id!==undefined?'&id='+id:'')+(val!==undefined?'&val='+val:'');\n"
"fetch('/action?'+q).then(function(r){return r.json()}).then(function(j){\n"
"if(!j.ok)alert('Action failed: '+(j.err||'unknown'));loadIncidents();}).catch(function(){});}\n"
"function renderRaceControl(d,ca){\n"
"ca=(ca||[]).slice().sort(function(a,b){return(a.p||999)-(b.p||999)});\n"
"var curMode=0;\n"
"for(var i=0;i<INC_CACHE.length;i++)if(INC_CACHE[i].ty=='RC_MODE')curMode=INC_CACHE[i].b;\n"
"var h='<div class=\"rcHdr\"><b>FIA RACE CONTROL</b><span style=\"color:var(--t3);font-size:9px\">'+\n"
"'TRACK STATUS: '+['GREEN','VSC','RED FLAG'][curMode]+'</span></div>';\n"
"h+='<div class=\"warn\">Read-only compatibility candidate. Race-control commands are unavailable.</div>';\n"
"var relevant=INC_CACHE.filter(function(e){return e.ty!=='POS';});\n"
"h+='<div class=\"sec\" style=\"margin-top:8px\"><h4>Bulletins</h4>';\n"
"if(relevant.length===0){h+='<div class=\"rcBul\"><span class=\"bd\">No bulletins issued yet this session.</span></div>';}\n"
"var list=relevant.slice().reverse();\n"
"for(var i=0;i<list.length;i++){var e=list[i];var m=rcMsg(e);if(!m)continue;\n"
"var num=relevant.length-i;\n"
"h+='<div class=\"rcBul '+m.sev+'\"><span class=\"no\">#'+num+'</span>'+\n"
"'<span class=\"bd\">'+m.t+'</span></div>';}\n"
"h+='</div>';\n"
"document.getElementById('swRC').innerHTML=h;}\n"
"\n"
"var LASTMSG=0,LASTGEN=null,TRANSPORT_LOST=0;\n"
"function conStatus(d){var age=d.sample_age_ms||0;var t=Date.now()-LASTMSG;\n"
"var txt=!d.sample_sequence?'WAITING FOR GAME':TRANSPORT_LOST?'TRANSPORT STALE (no frames '+Math.round(t/1000)+'s)':age>1000?'STALE DATA':'LIVE';\n"
"document.getElementById('sampleStatus').textContent='Web Telemetry '+d.version+' - '+txt+' - frame '+(age/1000).toFixed(1)+'s / transport '+(t/1000).toFixed(1)+'s - read only';}\n"
"function con(){\n"
"var es=new EventSource('/events');\n"
"es.onmessage=function(e){try{var d=JSON.parse(e.data);LASTMSG=Date.now();TRANSPORT_LOST=0;\n"
"if(LASTGEN===null||d.session_generation!==LASTGEN){LASTGEN=d.session_generation;DRV_CACHE=null;PACE_CACHE=[];INC_CACHE=[];ENG_EPOCH++;ENG_DATA=null;ENG_STATUS=null;ENG_FILE={a:null,b:null};ENG_SEL={a:-1,b:-1,la:null,lb:null};}\n"
"conStatus(d);up(d)}catch(ex){}}\n"
"es.onerror=function(){document.getElementById('sampleStatus').textContent='Telemetry disconnected';es.close();MB=null;TRK=[];EX={};RL={};GV={};\n"
"var tower=document.getElementById('tb');if(tower)tower.innerHTML='';rowEls={};rowHash={};setTimeout(con,2000)}\n"
"}\n"
"con();\n"
/* C08: a connected-but-silent EventSource never fires onerror, so the page
   could show LIVE forever. Age the last accepted frame independently. */
"setInterval(function(){if(LASTMSG&&!TRANSPORT_LOST&&Date.now()-LASTMSG>2500){TRANSPORT_LOST=1;\n"
"var el=document.getElementById('sampleStatus');if(el&&el.textContent.indexOf('disconnected')<0){el.textContent='Web Telemetry - TRANSPORT STALE (no frames '+Math.round((Date.now()-LASTMSG)/1000)+'s) - read only';}}},1000);\n"
/* BUGFIX 2026-07-28: the page opens on the Pit Wall tab, but the Race Pace
   and Race Control panes are fed by loadRacePace()/loadIncidents(), and the
   ONLY place those are kicked off (and their 3 s timers started) was inside
   swTab(). swTab never runs at load -- the Pit Wall button is marked active
   in the markup instead -- so both panes sat empty until the user clicked
   any tab and came back, which is exactly the reported symptom. Calling
   swTab for the default tab on boot starts the timers the same way a click
   would, rather than duplicating the fetch calls here. */
"swTab(CURTAB);\n"
"document.body.addEventListener('click',function(e){\n"
"var fEl=e.target.closest('[data-fid]');if(!fEl)return;\n"
"FOCUS=parseInt(fEl.getAttribute('data-fid'),10);FOCUS_LOCK=1;\n"
"var g=fEl.getAttribute('data-goto');if(g)swTab(g);\n"
"loadDriver(FOCUS);\n"
"if(LD)up(LD);});\n"
"window.addEventListener('resize',function(){\n"
"if(LD){var cc=(LD.c||[]).filter(function(c2){return c2.p>0&&c2.a});uM(cc);\n"
"if(CURTAB=='pw')uMPw(cc);}});\n"
"</script>\n"
"</body>\n"
"</html>\r\n";
#endif /* 4.4.0 batch 1: g_html (/classic) compiled out */

/* ========================================================================
 * HTTP SERVER
 * ======================================================================== */
/* 1.0.1 session archive (disk store, review documents, data manager). */
/* 1.1.0 storage v2 core (batch 1): pure encoding, reader, compression adapter, naming. */
#include "director_state.inc"
#include "storage_v2.inc"
#include "session_archive.inc"
#include "storage_writer.inc"
#include "storage_jobs.inc"
#include "storage_api.inc"

/* 1.0.1: the trace endpoints shared five static 2 MB buffers (10 MB of the
 * 32-bit game's address space); the HTTP thread serves one request at a time. */
static char g_http_big[TRACE_JSON_BYTES];

static SOCKET         g_listen_sock = INVALID_SOCKET;
/* M6 (security review 2026-10-13): SSE clients get a per-client byte queue. The telemetry
 * worker only sends with non-blocking calls. A client is dropped when its queue would pass
 * SSE_QUEUE_MAX or when it accepts no byte for SSE_STALL_MS. The queue holds at least two
 * full frames (a frame can be close to 256 KB). */
#define SSE_QUEUE_MAX    (512 * 1024)
#define SSE_STALL_MS     5000
/* HTTP responses (dispatcher thread): progress-based, so a reader that keeps taking bytes
 * is served in full. The absolute cap bounds a reader that drips one byte at a time. */
#define HTTP_SEND_IDLE_MS 5000
#define HTTP_SEND_ABS_MS  120000

typedef struct SseClient {
    SOCKET s;
    char  *pend;      /* bytes not yet accepted by the kernel */
    int    plen;
    DWORD  progress;  /* last tick at which bytes were accepted, or the queue was empty */
} SseClient;

static SseClient      g_sse_clients[MAX_SSE_CLIENTS];
static int            g_sse_count = 0;
static volatile BOOL  g_http_running = TRUE;
static CRITICAL_SECTION g_sse_cs;

static int send_complete(SOCKET s, const char *buf, int len) {
    int done = 0;
    DWORD start = GetTickCount(), progress = start;
    if (len < 0) return 0;
    while (done < len) {
        DWORD now = GetTickCount();
        if (now - progress >= HTTP_SEND_IDLE_MS || now - start >= HTTP_SEND_ABS_MS) return 0;
        int n = send(s, buf + done, len - done, 0);
        if (n > 0) { done += n; progress = GetTickCount(); continue; }
        if (!n || WSAGetLastError() != WSAEWOULDBLOCK) return 0;
        Sleep(1);
    }
    return 1;
}
static void http_send(SOCKET s, const char *status, const char *ctype,
                      const char *body, int blen) {
    char hdr[512];
    if (format_failed || blen<0) {
        status="500 Internal Server Error"; ctype="application/json";
        body="{\"error\":\"response capacity exceeded\"}"; blen=(int)strlen(body);
        format_failed=0;
    }
    int hl=_snprintf(hdr,sizeof hdr,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        /* 4.4.0-rc.10: no-cache so a reinstall is picked up without Ctrl+F5
         * (owner saw an rc.8 viewer.js cached while rc.9 was served). */
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",status,ctype,blen);
    if (!format_failed && send_complete(s,hdr,hl) && body && blen>0) send_complete(s,body,blen);
    wtu_note_http(atoi(status), g_sse_count);
}

static void sse_add(SOCKET s) {
    EnterCriticalSection(&g_sse_cs);
    if (g_sse_count < MAX_SSE_CLIENTS) {
        SseClient *c = &g_sse_clients[g_sse_count];
        c->pend = (char *)malloc(SSE_QUEUE_MAX);
        if (c->pend) {
            c->s = s; c->plen = 0; c->progress = GetTickCount();
            g_sse_count++;
        } else {
            closesocket(s);
        }
    } else {
        closesocket(s);
    }
    LeaveCriticalSection(&g_sse_cs);
}

static void sse_broadcast(const char *data, int len) {
    format_failed=0;
    static char buf[262160];
    int hl = _snprintf(buf, sizeof(buf), "data: %.*s\r\n\r\n", len, data);
    if (format_failed) return;
    EnterCriticalSection(&g_sse_cs);
    for (int i = g_sse_count - 1; i >= 0; i--) {
        SseClient *c = &g_sse_clients[i];
        int drop = 0;
        if (c->plen + hl > SSE_QUEUE_MAX) {
            drop = 1;
        } else {
            memcpy(c->pend + c->plen, buf, (size_t)hl);
            c->plen += hl;
            while (c->plen > 0) {
                int n = send(c->s, c->pend, c->plen, 0);
                if (n > 0) {
                    memmove(c->pend, c->pend + n, (size_t)(c->plen - n));
                    c->plen -= n;
                    c->progress = GetTickCount();
                    continue;
                }
                if (n < 0 && WSAGetLastError() == WSAEWOULDBLOCK) break;
                drop = 1;   /* n == 0 or a socket error */
                break;
            }
            if (!drop && c->plen == 0) c->progress = GetTickCount();
            else if (!drop && GetTickCount() - c->progress >= SSE_STALL_MS) drop = 1;
        }
        if (drop) {
            closesocket(c->s);
            free(c->pend);
            g_sse_clients[i] = g_sse_clients[--g_sse_count];
        }
    }
    LeaveCriticalSection(&g_sse_cs);
}

/* finds "key=" in a query string, copies the value (up to '&' or end) into out.
 * The key must start the query or follow '&': substring matches such as
 * "xcar=A" no longer alias "car=A" (endpoint-audit hardening, rc.7). */
static __attribute__((unused)) int qs_get(const char *query, const char *key, char *out, int outsz) {
    int klen = (int)strlen(key);
    const char *p = query;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=' &&
            (p == query || p[-1] == '&')) {
            p += klen + 1;
            int i = 0;
            while (p[i] && p[i] != '&' && i < outsz - 1) { out[i] = p[i]; i++; }
            /* Never accept a valid prefix of an oversized parameter. */
            if (p[i] && p[i]!='&') { out[0]='!'; out[1]=0; return 1; }
            out[i] = '\0';
            return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return 0;
}

/* 4.4.0 all-cars: `car=` accepts a side letter (A|B, the selected aliases) or
 * an explicit vehicle slot (0..MAX_CARS-1) so any driver's recorded trace is
 * addressable. Encodes a slot as 1000+N for trace_pick. Returns 0 when the
 * parameter is absent or malformed. */
static __attribute__((unused)) int http_car_select(const char *query, int *which) {
    char text[24];
    if (!qs_get(query, "car", text, sizeof text)) return 0;
    if (text[0] && !text[1] &&
        (text[0] == 'A' || text[0] == 'a' || text[0] == 'B' || text[0] == 'b')) {
        *which = (text[0] == 'B' || text[0] == 'b') ? 1 : 0;
        return 1;
    }
    {
        char *end = NULL;
        errno = 0;
        long v = strtol(text, &end, 10);
        if (errno == ERANGE || !end || end == text || *end || v < 0 || v >= MAX_CARS) return 0;
        *which = 1000 + (int)v;
        return 1;
    }
}

#if 0 /* 4.4.0 multi-driver: /workbench page retired; the console at / is the only UI. Source kept, compiled out. */
/* ========================================================================
 * WORKBENCH PAGE (rc.5) — the product serves /workbench and /workbench.js.
 * The renderer is the canonical sidecar workbench_ui.js beside the DLL
 * (bounded, loaded once); the page bootstrap wires it to the existing
 * read-only endpoints. Missing sidecar: 404, capture unaffected.
 * ======================================================================== */
static const char *g_workbench_html =
"<!DOCTYPE html>\n"
"<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
"<title>F1C Workbench — live player/AI telemetry</title>\n"
"<style>\n"
"body{margin:0;background:#0d1117;color:#c9d4e0;font:13px/1.45 system-ui,Segoe UI,sans-serif}\n"
"header{padding:10px 14px;background:#161b22;border-bottom:1px solid #263041;display:flex;gap:12px;align-items:baseline}\n"
"header h1{margin:0;font-size:15px;color:#e6edf3}\n"
"header a{color:#7fb2ff;font-size:12px}\n"
"header .w{color:#ffbb55;font-size:12px;margin-left:auto}\n"
"#app{padding:12px 14px;display:grid;grid-template-columns:260px 1fr;gap:10px}\n"
".wb-bar{grid-column:1/-1;display:flex;flex-wrap:wrap;gap:8px;align-items:center}\n"
".wb-bar select,.wb-bar button{background:#21283a;color:#d7e0ea;border:1px solid #38445a;border-radius:4px;padding:3px 8px;font:inherit}\n"
".wb-bar button{cursor:pointer}\n"
".wb-chart{background:#101722;border:1px solid #263041;border-radius:6px;padding:6px}\n"
".wb-chart.wb-track{grid-column:1}.wb-chart.wb-gg{grid-column:1}\n"
".wb-chart.wb-speed,.wb-chart.wb-rpm,.wb-chart.wb-inputs{grid-column:2}\n"
".wb-title{font-size:11px;color:#8fa3b8;margin-bottom:4px}\n"
"canvas{display:block}\n"
".wb-readout,.wb-reasons{grid-column:1/-1;background:#101722;border:1px solid #263041;border-radius:6px;padding:6px 8px;font-size:12px;color:#b9c6d4}\n"
".wb-status{margin-left:auto;color:#67d9a0;font-size:12px}\n"
"</style></head>\n"
"<body>\n"
"<header><h1>F1C Workbench</h1><a href=\"/\">&larr; dashboard</a>\n"
"<span class=\"w\">Read-only. Live player/AI traces; legacy .lap/.spt import. RPM is converted from rad/s (\u00d7 9.549296585513721).</span></header>\n"
"<div id=\"app\"></div>\n"
"<script src=\"/workbench.js\"></script>\n"
"<script>\n"
"if(typeof Stop4Workbench==='undefined'){document.getElementById('app').innerHTML='<p style=\"color:#ffbb55;padding:12px\">workbench_ui.js is missing beside the DLL \\u2014 reinstall the sidecar (see PRIVATE_TEST_README).</p>';}\n"
"else{(function(){'use strict';\n"
"var app=document.getElementById('app'),wb=new Stop4Workbench.Workbench(),ui=null,roster=[],rosterSig='',context=null,trackKey='',trackToken=0;\n"
"function onFrame(j){\n"
" wb.frame(j);\n"
" var next=(j.c||[]).filter(function(c){return c.p>0});\n"
" var sig=JSON.stringify(next.map(function(c){return [c.id,c.n,c.cn,c.a,c.tid]}));\n"
" var ctx=JSON.stringify([j.session_generation,j.trk,j.lapm]);\n"
" if(!ui&&next.length)ui=Stop4Workbench.mount(app,wb,{roster:next,label:'LIVE WORKBENCH'});\n"
" if(!ui)return;\n"
" if(sig!==rosterSig){roster=next;rosterSig=sig;ui.setRoster(roster);}\n"
" if(ctx!==context){context=ctx;var pl=roster.filter(function(c){return !c.a})[0]||roster[0];var ai=roster.filter(function(c){return c.a})[0];if(pl)wb.select('A',pl.id,pl);if(ai)wb.select('B',ai.id,ai);ui.refreshSelection();}\n"
" var tk=JSON.stringify([j.session_generation,j.trk,j.wc]);\n"
" if(tk!==trackKey){trackKey=tk;var token=++trackToken;ui.setWaypoints(null);fetch('/track').then(function(r){if(!r.ok)throw Error('track unavailable');return r.json()}).then(function(d){if(token===trackToken)ui.setWaypoints(d)}).catch(function(){});}\n"
" ui.render();\n"
"}\n"
"var es=new EventSource('/events');\n"
"es.onmessage=function(e){try{onFrame(JSON.parse(e.data))}catch(x){}};\n"
"})();}\n"
"</script></body></html>\n";
#endif /* 4.4.0 multi-driver: g_workbench_html (/workbench page) compiled out */

#if 0 /* 4.4.0 batch 1: /viewer is retired (404). Source kept, compiled out. */
/* ========================================================================
 * TELEMETRY VIEWER PAGE (4.3.0) - /viewer loads the shared model sidecar
 * (/workbench.js, Stop4Workbench) and then the viewer sidecar
 * (/viewer.js, telemetry_viewer.js). Both are SHA-pinned like the workbench.
 * ======================================================================== */
static const char *g_viewer_html =
"<!DOCTYPE html>\n"
"<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>F1C Telemetry Viewer</title>\n"
"<style>html,body{margin:0;background:#07090d;color:#e6edf5;font:13px system-ui,Segoe UI,sans-serif}</style>\n"
"</head><body><div id=\"app\"></div>\n"
"<script src=\"/workbench.js\"></script>\n"
"<script src=\"/viewer.js\"></script>\n"
"<script>\n"
"if(typeof Stop4Workbench==='undefined'||typeof F1CViewer==='undefined'){document.getElementById('app').innerHTML='<p style=\"color:#fbbf24;padding:16px\">telemetry_viewer.js or workbench_ui.js is missing beside the DLL \\u2014 reinstall the sidecars (see PRIVATE_TEST_README).</p>';}\n"
"else{(function(){'use strict';\n"
"var v=F1CViewer.mount(document.getElementById('app'),{fetchTrack:function(){return fetch('/track').then(function(r){if(!r.ok)throw Error('track unavailable');return r.json()})}});\n"
"function health(){fetch('/health').then(function(r){return r.json()}).then(function(h){v.health=h;v.invalidate()}).catch(function(){})}\n"
"health();setInterval(health,10000);\n"
"function con(){var es=new EventSource('/events');es.onmessage=function(e){try{v.onFrame(JSON.parse(e.data))}catch(x){}};es.onerror=function(){es.close();setTimeout(con,2000)}}\n"
"con();window.viewer=v;\n"
"})();}\n"
"</script></body></html>\n";
#endif /* 4.4.0 batch 1: g_viewer_html (/viewer) compiled out */

/* ========================================================================
 * UNIFIED CONSOLE PAGE (4.4.0) - "/" mounts F1CViewer.mountConsole from the
 * viewer sidecar: race pages, analysis views and the legacy layouts in one
 * window styled after the old f1_telemetry.exe. The previous dashboard is
 * /classic and /viewer are retired (404; source compiled out); /workbench is unchanged.
 * ======================================================================== */
static const char *g_console_html =
"<!DOCTYPE html>\n"
"<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>F1 Challenge 99-02 Telemetry</title>\n"
"<meta name=\"generator\" content=\"F1C " PRODUCT_LABEL " (engine " TELEMETRY_VERSION ")\">\n"
"<style>html,body{margin:0;background:#000;color:#dfe7ff;font:11px Tahoma,Verdana,Arial,sans-serif}</style>\n"
"</head><body><div id=\"app\"></div>\n"
"<script src=\"/workbench.js\"></script>\n"
"<script src=\"/viewer.js\"></script>\n"
"<script>\n"
"if(typeof Stop4Workbench==='undefined'||typeof F1CViewer==='undefined'||!F1CViewer.mountConsole){document.getElementById('app').innerHTML='<p style=\"color:#ffd84a;padding:16px\">" PRODUCT_LABEL ": telemetry_viewer.js or workbench_ui.js is missing or outdated beside the DLL \u2014 reinstall Telemetry in Lightloader.</p>';}\n"
"else{(function(){'use strict';\n"
/* 1.0.1 review mode: /?session=<id> opens an archived session; the console
 * reads the archive (same endpoints + session=) and the live feed stays off. */
"var m=/[?&]session=([A-Za-z0-9_-]{8,47})(?:&|$)/.exec(location.search),S=m?m[1]:null,Q=S?'?session='+S:'';\n"
"var v=F1CViewer.mountConsole(document.getElementById('app'),{review:S,fetchTrack:function(){return fetch('/track'+Q).then(function(r){if(!r.ok)throw Error('track unavailable');return r.json()})}});\n"
"function health(){fetch('/health').then(function(r){return r.json()}).then(function(h){v.health=h;v.invalidate()}).catch(function(){})}\n"
"health();setInterval(health,10000);\n"
"function con(){var es=new EventSource('/events');es.onmessage=function(e){try{v.onFrame(JSON.parse(e.data))}catch(x){}};es.onerror=function(){es.close();setTimeout(con,2000)}}\n"
"if(S){fetch('/telemetry'+Q).then(function(r){if(!r.ok)throw Error('no frame');return r.json()}).then(function(j){v.onFrame(j)}).catch(function(){if(v.reviewFailed)v.reviewFailed()})}else con();\n"
"window.telemetryConsole=v;\n"
"})();}\n"
"</script></body></html>\n";

/* 1.0.1: the two UI scripts are built INTO the DLL. The Lightloader manager
 * installs only .dll/.asi/.ini files from a mod package, so with 1.0.0 every
 * manager install lacked telemetry_viewer.js and workbench_ui.js and the page
 * showed "missing or outdated beside the DLL" (player report 2026-10-12).
 * Files beside the DLL are no longer read at all, so a stale copy from an
 * older private build cannot break the page either. The bytes come from the
 * source files at build time (.incbin, compiled from this directory); the
 * pinned hashes below still prove which scripts the DLL carries (/health). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".balign 4\n"
    ".globl _g_embed_viewer_js\n"
    "_g_embed_viewer_js:\n"
    ".incbin \"telemetry_viewer.js\"\n"
    ".globl _g_embed_viewer_js_end\n"
    "_g_embed_viewer_js_end:\n"
    ".byte 0\n"
    ".balign 4\n"
    ".globl _g_embed_workbench_js\n"
    "_g_embed_workbench_js:\n"
    ".incbin \"workbench_ui.js\"\n"
    ".globl _g_embed_workbench_js_end\n"
    "_g_embed_workbench_js_end:\n"
    ".byte 0\n"
    ".text\n");
extern const char g_embed_viewer_js[], g_embed_viewer_js_end[];
extern const char g_embed_workbench_js[], g_embed_workbench_js_end[];

static char *g_viewer_js_cache = NULL;
static int g_viewer_js_len = 0;
static char g_viewer_js_sha[65];

/* Manifest-pinned viewer sidecar hash (VERIFICATION_MANIFEST.json
 * sidecar_viewer.sha256), written by pin_sidecars.py and gated by
 * verify_sidecar.py. */
#define VIEWER_UI_EXPECTED_SHA256 \
    "BB6335C40504A3E07747DF854F550205BE730B15C4BA391C67A9B79B3CEAEC7B"

/* Binds one embedded script once and hashes it. */
static int sidecar_load(const char *start, const char *end, char **cache, int *len, char sha[65]) {
    if (*cache) return 1;
    size_t n = (size_t)(end - start);
    if (!n || n > 4u * 1048576u) return 0;
    {
        unsigned char digest[32];
        sha256_buf(start, n, digest);
        sha256_hex(digest, sha);
    }
    *cache = (char *)start;   /* read-only; served as is */
    *len = (int)n;
    return 1;
}

static int viewer_js_load(void) {
    return sidecar_load(g_embed_viewer_js, g_embed_viewer_js_end, &g_viewer_js_cache, &g_viewer_js_len, g_viewer_js_sha);
}

static char *g_workbench_js_cache = NULL;
static int g_workbench_js_len = 0;
static char g_workbench_js_sha[65];

/* Manifest-pinned sidecar hash (VERIFICATION_MANIFEST.json sidecar.sha256).
 * The build gate (verify_sidecar.py + test_telemetry.py) keeps the source,
 * the staged copy and this constant in sync; /health reports expected and
 * matches so the scripts built into the DLL can be verified in the browser. */
#define WORKBENCH_UI_EXPECTED_SHA256 \
    "29ECD1A638AAD455E72A4726BF2B948D8E56419E73953C6D993B9033763938AD"

static int workbench_js_load(void) {
    return sidecar_load(g_embed_workbench_js, g_embed_workbench_js_end, &g_workbench_js_cache, &g_workbench_js_len, g_workbench_js_sha);
}

/* ---- 4.4.0-rc.8 (Batch 2k task 1 and 2): read-only MF host probe and LOG\weather.txt forecast ----
 * Both run on the HTTP thread only. No game memory is written. The MF probe
 * opens the MF host mapping read-only and reads its 32-byte MfHostCtl layout
 * (mechanical_failures.c:76-88, magic 'MFHM'), then closes the handles.
 * The forecast parses <game>\LOG\weather.txt. 4.4.0-rc.14: the GAME writes
 * that file itself - Game_Init 0x42D6B0 -> weather init sub_4FCAA0 builds the
 * weekend timeline (sub_4FD8B0(0, 432000)) and dumps it with
 * WeatherConditionsFileWriter 0x4FE820 ("%sweather.txt", 8 blocks named by
 * SessionTypeNameLookup 0..7). Live 2026-10-12: written 14 s after the game
 * started; Practice1 start AmbientTemp 16.88 / TrackTemp 23.24 = live
 * at/trkt 16.9 / 23.3. It is written once per weekend load, so it is always
 * OLDER than the session start; the freshness gate is "written during this
 * game run" (file mtime >= this process's creation time). No other mod is
 * involved or required. */
#define MF_PROBE_SHM   "F1C_MECHFAIL_CTL"
#define MF_PROBE_MAGIC 0x4D48464Du

typedef struct {
    volatile unsigned magic;
    volatile int abi_version;
    volatile int struct_size;
    volatile int host_verified;
    volatile int dev_injector;
    volatile int families_started;
    volatile int game_pid;
    volatile int activated;
} mf_probe_view_t;

static void mechfail_probe(int *present, int *host_verified, int *activated,
                           int *families_started) {
    HANDLE h;
    const mf_probe_view_t *v;
    *present = 0; *host_verified = 0; *activated = 0; *families_started = 0;
    h = OpenFileMappingA(FILE_MAP_READ, FALSE, MF_PROBE_SHM);
    if (!h) return;
    v = (const mf_probe_view_t *)MapViewOfFile(h, FILE_MAP_READ, 0, 0,
                                               sizeof(mf_probe_view_t));
    if (v) {
        if (v->magic == MF_PROBE_MAGIC) {
            *present = 1;
            *host_verified = (v->host_verified == 1);
            *activated = v->activated;
            *families_started = v->families_started;
        }
        UnmapViewOfFile((LPCVOID)v);
    }
    CloseHandle(h);
}

#define FC_MAX_STEPS 96
#define FC_JSON_CAP  12288
#define FC_TYPES     8

typedef struct { double minute, cond, amb; } fc_step_t;
typedef struct {
    int found, has_start, nsteps, truncated;
    double cond, onw, offw, amb, trk;
    fc_step_t step[FC_MAX_STEPS];
} fc_block_t;

/* Game ty (conditions block) -> weather.txt block name. 6 (Warmup) is an
 * inference, see RESEARCH_WEATHER; 0 TestDay and 1-4 Practice, 5 Qualify, 7
 * Race follow the console mapping (findings test-day-session-type-0). */
static const char *fc_type_name(int ty) {
    static const char *names[FC_TYPES] = {
        "TestDay", "Practice1", "Practice2", "Practice3", "Practice4",
        "Qualify", "Warmup", "Race"
    };
    if (ty < 0 || ty >= FC_TYPES) return NULL;
    return names[ty];
}

static char *fc_trim(char *s) {
    size_t n;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
        s[--n] = '\0';
    return s;
}

/* Line-based parse of the block named `want`. Depth 1 = session names,
 * depth 2 = start values and Minute= headers, depth 3 = one step's values. */
static void fc_parse(char *text, const char *want, fc_block_t *out) {
    int depth = 0, active = 0, step_open = 0;
    char cur[32] = "";
    char *p = text;
    memset(out, 0, sizeof *out);
    while (*p) {
        char *nl = strchr(p, '\n');
        char *s;
        if (nl) *nl = '\0';
        s = fc_trim(p);
        if (*s == '\0') {
            /* blank line */
        } else if (strcmp(s, "{") == 0) {
            depth++;
            if (depth == 2) {
                active = (strcmp(cur, want) == 0);
                if (active) out->found = 1;
            }
        } else if (strcmp(s, "}") == 0) {
            if (depth == 2) active = 0;
            if (depth == 3) step_open = 0;
            if (depth > 0) depth--;
        } else if (strchr(s, '=') == NULL) {
            if (depth == 1) {
                size_t L = strlen(s);
                if (L >= sizeof cur) L = sizeof cur - 1;
                memcpy(cur, s, L);
                cur[L] = '\0';
            }
        } else if (active) {
            char *eq = strchr(s, '=');
            char *k, *v;
            double dv;
            *eq = '\0';
            k = fc_trim(s);
            v = fc_trim(eq + 1);
            dv = strtod(v, NULL);
            if (depth == 2) {
                if (strcmp(k, "Conditions") == 0)          { out->cond = dv; out->has_start = 1; }
                else if (strcmp(k, "OnPathWetness") == 0)  out->onw = dv;
                else if (strcmp(k, "OffPathWetness") == 0) out->offw = dv;
                else if (strcmp(k, "AmbientTemp") == 0)    out->amb = dv;
                else if (strcmp(k, "TrackTemp") == 0)      out->trk = dv;
                else if (strcmp(k, "Minute") == 0) {
                    if (out->nsteps < FC_MAX_STEPS) {
                        out->step[out->nsteps].minute = dv;
                        out->step[out->nsteps].cond = 0.0;
                        out->step[out->nsteps].amb = 0.0;
                        out->nsteps++;
                        step_open = 1;
                    } else {
                        out->truncated = 1;
                        step_open = 0;
                    }
                }
            } else if (depth == 3 && step_open && out->nsteps > 0) {
                fc_step_t *st = &out->step[out->nsteps - 1];
                if (strcmp(k, "Conditions") == 0)       st->cond = dv;
                else if (strcmp(k, "AmbientTemp") == 0) st->amb = dv;
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
}

static double fc_filetime_unix(const FILETIME *ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return ((double)u.QuadPart - 116444736000000000.0) / 10000000.0;
}

/* rc.14: game-run start = this process's creation time (the DLL runs inside
 * the game). A weather.txt older than that is from an earlier run and stays
 * hidden; any weekend loaded during this run rewrites the file. */
static FILETIME g_fc_start_ft;
static int g_fc_start_set = 0;

static void fc_note_run_start(void) {
    FILETIME created, exited, kern, user;
    if (g_fc_start_set) return;
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kern, &user)) {
        g_fc_start_ft = created;
        g_fc_start_set = 1;
    }
}

/* Writes the forecast JSON object. Returns chars written, or -1 on overflow. */
static int forecast_json(char *buf, int bufsz, int ty) {
    static const char *src = "LOG\\weather.txt";
    char path[MAX_PATH + 32];
    WIN32_FILE_ATTRIBUTE_DATA fad;
    const char *tname = fc_type_name(ty);
    int n = 0;
    char *text;
    fc_block_t blk;
    if (!tyre_game_path(src, path, sizeof path)) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"available\":false,\"reason\":\"no game path\"}");
    }
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"available\":false,\"reason\":\"file missing\"}");
    }
    if (!tname) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"available\":false,\"reason\":\"no session type\"}");
    }
    if (fad.nFileSizeHigh || fad.nFileSizeLow > TYRE_FILE_CAP) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"session\":\"%s\",\"available\":false,\"reason\":\"file too large\"}", tname);
    }
    if (!g_fc_start_set || CompareFileTime(&fad.ftLastWriteTime, &g_fc_start_ft) < 0) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"session\":\"%s\",\"available\":false,\"fresh\":false,\"reason\":\"file is from an earlier game run\",\"file_mtime\":%.0f,\"run_start\":%.0f}",
            tname, fc_filetime_unix(&fad.ftLastWriteTime), g_fc_start_set ? fc_filetime_unix(&g_fc_start_ft) : 0.0);
    }
    text = tyre_read_file(path);
    if (!text) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"session\":\"%s\",\"available\":false,\"reason\":\"read failed\"}", tname);
    }
    fc_parse(text, tname, &blk);
    free(text);
    if (!blk.found || blk.nsteps == 0) {
        return _snprintf(buf, bufsz, "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"session\":\"%s\",\"available\":false,\"fresh\":true,\"reason\":\"no block for this session\"}", tname);
    }
    n = _snprintf(buf, bufsz,
        "{\"source\":\"LOG\\\\weather.txt\",\"tier\":\"DERIVED\",\"written_by\":\"game\","
        "\"session\":\"%s\",\"ty\":%d,\"available\":true,\"fresh\":true,"
        "\"file_mtime\":%.0f,\"run_start\":%.0f,"
        "\"start\":{\"conditions\":%.2f,\"on_path_wetness\":%.2f,\"off_path_wetness\":%.2f,\"ambient_temp\":%.2f,\"track_temp\":%.2f},"
        "\"steps\":[",
        tname, ty, fc_filetime_unix(&fad.ftLastWriteTime), fc_filetime_unix(&g_fc_start_ft),
        blk.cond, blk.onw, blk.offw, blk.amb, blk.trk);
    if (n < 0) return -1;
    for (int i = 0; i < blk.nsteps; i++) {
        int w = _snprintf(buf + n, bufsz - n,
            "%s{\"minute\":%.2f,\"conditions\":%.2f,\"ambient_temp\":%.2f}",
            i ? "," : "", blk.step[i].minute, blk.step[i].cond, blk.step[i].amb);
        if (w < 0) return -1;
        n += w;
    }
    if (n + 40 >= bufsz) return -1;
    n += _snprintf(buf + n, bufsz - n, "],\"truncated\":%s}", blk.truncated ? "true" : "false");
    return n;
}

/* /health body: build identity, capture state, the sidecar integrity report
 * and (BACKEND 4, api_version 3) the native game-thread capture cost measured
 * in the broker callback plus the process working set. */
static int build_health_json(char *buf, int bufsz) {
    unsigned gen_now; int ty_now;
    int mf_present, mf_verified, mf_activated, mf_families;
    char *fc = NULL;
    int fc_len = -1;
    int present = workbench_js_load();
    int matches = present &&
        strcmp(g_workbench_js_sha, WORKBENCH_UI_EXPECTED_SHA256) == 0;
    int vpresent = viewer_js_load();
    int vmatches = vpresent &&
        strcmp(g_viewer_js_sha, VIEWER_UI_EXPECTED_SHA256) == 0;
    double cp50, cp95, cmax, crate; int cn;
    unsigned long working_set_kb = 0;
    PROCESS_MEMORY_COUNTERS pmc;
    capture_cost_snapshot(&cp50, &cp95, &cmax, &crate, &cn);
    /* Read-only, outside g_telemetry_cs: no file or mapping IO under the game lock. */
    EnterCriticalSection(&g_telemetry_cs);
    gen_now = capture_generation;
    ty_now = g_telemetry.session_type;
    LeaveCriticalSection(&g_telemetry_cs);
    (void)gen_now;
    fc_note_run_start();
    mechfail_probe(&mf_present, &mf_verified, &mf_activated, &mf_families);
    fc = (char *)malloc(FC_JSON_CAP);
    if (fc) fc_len = forecast_json(fc, FC_JSON_CAP, ty_now);
    memset(&pmc, 0, sizeof pmc); pmc.cb = sizeof pmc;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, (DWORD)sizeof pmc))
        working_set_kb = (unsigned long)(pmc.WorkingSetSize / 1024);
    EnterCriticalSection(&g_telemetry_cs);
    int len = _snprintf(buf, bufsz,
        "{\"version\":\"%s\",\"api_version\":\"%s\",\"broker_abi\":%u,\"sample_sequence\":%u,"
        "\"sample_age_ms\":%lu,\"session_generation\":%u,\"failed_captures\":%u,\"read_only\":true,"
        "\"listen_address\":\"%s\",\"port\":%d,"
        "\"capture_cost_us\":{\"samples\":%d,\"p50\":%.1f,\"p95\":%.1f,\"max\":%.1f,\"rate_per_s\":%.1f},"
        "\"memory\":{\"working_set_kb\":%lu},"
        "\"workbench_ui\":{\"present\":%s,\"bytes\":%d,\"sha256\":\"%s\","
        "\"expected\":\"%s\",\"matches\":%s},"
        "\"viewer_ui\":{\"present\":%s,\"bytes\":%d,\"sha256\":\"%s\","
        "\"expected\":\"%s\",\"matches\":%s}",
        TELEMETRY_VERSION, API_VERSION, telemetry_broker.negotiated_abi, capture_sequence,
        capture_sequence ? GetTickCount()-capture_time : 0ul, capture_generation,
        /* schemas/health.json is frozen at API 9 (enum 127.0.0.1 or 0.0.0.0): the primary loopback listener is reported;
         * the Remote=1 BindAddress listener is not in this field (owner decision for API 10). */
        failed_captures, "127.0.0.1", (int)g_http_bound_port,
        cn, cp50, cp95, cmax, crate, working_set_kb,
        present ? "true" : "false", present ? g_workbench_js_len : 0,
        present ? g_workbench_js_sha : "", WORKBENCH_UI_EXPECTED_SHA256,
        matches ? "true" : "false",
        vpresent ? "true" : "false", vpresent ? g_viewer_js_len : 0,
        vpresent ? g_viewer_js_sha : "", VIEWER_UI_EXPECTED_SHA256,
        vmatches ? "true" : "false");
    int trace_mode_now = g_trace_mode;
    int trace_failed_now = g_trace_store_failed;
    int trace_cars_now = g_trace_store_cap;
    unsigned long long trace_bytes_now = g_trace_store_bytes;
    LeaveCriticalSection(&g_telemetry_cs);
    /* Additive keys (api_version 4: mods.mechfail, forecast; api_version 5:
     * trace_mode/trace_bytes/trace_cars for the all-cars recorder). */
    char archive_json[MAX_PATH * 6 + 512];
    if (ar_health_json(archive_json, sizeof archive_json) <= 0) strcpy(archive_json, "{\"enabled\":false}");
    char storage_json[512];
    if (sa_health_json(storage_json, sizeof storage_json) <= 0) strcpy(storage_json, "{\"enabled\":false}");
    if (len > 0 && len < bufsz - 1) {
        int more = _snprintf(buf + len, bufsz - len,
            ",\"mods\":{\"mechfail\":{\"present\":%s,\"host_verified\":%s,\"activated\":%d,\"families_started\":%d}},\"forecast\":%s,"
            "\"trace_mode\":\"%s\",\"trace_bytes\":%llu,\"trace_cars\":%d,\"product\":\"" PRODUCT_LABEL "\",\"archive\":%s,\"storage\":%s}",
            mf_present ? "true" : "false", mf_verified ? "true" : "false",
            mf_activated, mf_families,
            (fc && fc_len > 0) ? fc : "{\"available\":false,\"reason\":\"forecast unavailable\"}",
            trace_mode_now == TRACE_MODE_ALL ? "all"
                : trace_failed_now ? "selected_fallback" : "selected",
            trace_bytes_now, trace_cars_now, archive_json, storage_json);
        len = more > 0 ? len + more : -1;
    } else {
        len = -1;
    }
    free(fc);
    return len;
}

/* Content-Length from the request headers; -1 when absent/malformed. */
static long http_content_length(const char *req) {
    const char *p = req;
    long length=-1;
    while ((p = strchr(p, '\n')) != NULL) {
        p++;
        if (!strncmp(p, "\r\n", 2) || !*p) break;
        if (!strncasecmp(p,"transfer-encoding:",18)) return -1;
        if (!strncasecmp(p, "content-length:", 15)) {
            if (length>=0) return -1;
            const char *v=p+15; char *end;
            while (*v==' ' || *v=='\t') v++;
            if (*v<'0' || *v>'9') return -1;
            errno=0; length=strtol(v,&end,10);
            if (errno==ERANGE) return -1;
            while (*end==' ' || *end=='\t') end++;
            if (end[0]!='\r' || end[1]!='\n') return -1;
        }
    }
    return length;
}

/* True when the request carries `name: value` (name and value compared
 * case-insensitively; value parameters after ';' are ignored). Used to force
 * a CORS preflight on the import POST: application/octet-stream is not a
 * CORS-simple content type, so a foreign page cannot fire it no-preflight. */
static int http_header_is(const char *req, const char *name, const char *value) {
    size_t nlen = strlen(name);
    const char *p = req;
    while ((p = strchr(p, '\n')) != NULL) {
        p++;
        if (!strncasecmp(p, name, nlen) && p[nlen] == ':') {
            const char *v = p + nlen + 1;
            while (*v == ' ' || *v == '\t') v++;
            size_t vlen = strlen(value);
            return !strncasecmp(v, value, vlen) &&
                   (v[vlen] == '\0' || v[vlen] == '\r' || v[vlen] == '\n' || v[vlen] == ';');
        }
        if (!strncmp(p, "\r\n", 2) || !*p) break;
    }
    return 0;
}

/* Read a bounded POST body; bytes already buffered after the header block are
 * used first. Returns 1 on a complete body (caller frees *out). */
static int http_read_body(SOCKET client, const char *req, int received, long length,
                          unsigned char **out) {
    if (length <= 0 || length > (long)IMPORT_BODY_MAX) return 0;
    unsigned char *body = (unsigned char *)malloc((size_t)length);
    if (!body) return 0;
    const char *hdr_end = strstr(req, "\r\n\r\n");
    if (!hdr_end) { free(body); return 0; }
    int have = received - (int)(hdr_end + 4 - req);
    if (have > 0) {
        if (have > (int)length) have = (int)length;
        memcpy(body, hdr_end + 4, (size_t)have);
    }
    DWORD start = GetTickCount();
    while (have < (int)length) {
        if (GetTickCount() - start > 3000) break;
        int got = recv(client, (char *)body + have, (int)length - have, 0);
        if (got > 0) { have += got; continue; }
        if (got == 0) break;
        if (WSAGetLastError() != WSAEWOULDBLOCK) break;
        if (GetTickCount() - start > 3000) break;
        Sleep(5);
    }
    if (have != (int)length) { free(body); return 0; }
    *out = body;
    return 1;
}

/* Graceful close (endpoint-audit rc.7): half-close the send side and briefly
 * drain unread request bytes so the client receives the response instead of a
 * TCP reset — a refused POST whose body is still in flight would otherwise
 * trigger RST before the client reads the error. Bounded to 200 ms. */
static void http_close(SOCKET s) {
    char sink[512];
    DWORD start;
    shutdown(s, SD_SEND);
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    start = GetTickCount();
    while (GetTickCount() - start < 200) {
        int n = recv(s, sink, sizeof sink, 0);
        if (n > 0) continue;
        if (n == 0) break;
        if (WSAGetLastError() != WSAEWOULDBLOCK) break;
        Sleep(5);
    }
    closesocket(s);
}

/* 1.0.1: data-manager deletes are accepted only from this PC, even with
 * Remote=1 (LAN/Tailscale viewers can read, never delete). */
static int http_peer_is_local(SOCKET s) {
    struct sockaddr_in a;
    int al = sizeof a;
    if (getpeername(s, (struct sockaddr *)&a, &al) != 0 || a.sin_family != AF_INET) return 0;
    return (ntohl(a.sin_addr.s_addr) >> 24) == 127;
}

/* M4 (security review 2026-10-13): a request that a proxy forwarded is not local, even when the
 * peer is 127.x: tailscale serve, cloudflared, nginx and SSH tunnels on this PC all connect from
 * loopback. Header names are case-insensitive; values are ignored. */
static int http_has_header(const char *req, const char *name) {
    size_t nlen = strlen(name);
    const char *p = strchr(req, '\n');
    while (p) {
        const char *h = p + 1;
        if (!h[0] || h[0] == '\r' || h[0] == '\n') break;
        if (!strncasecmp(h, name, nlen) && h[nlen] == ':') return 1;
        p = strchr(h, '\n');
    }
    return 0;
}
static int http_has_proxy_header(const char *req) {
    return http_has_header(req, "Forwarded") || http_has_header(req, "X-Forwarded-For") ||
           http_has_header(req, "Via") || http_has_header(req, "X-Real-IP") ||
           http_has_header(req, "CF-Connecting-IP") || http_has_header(req, "True-Client-IP");
}
/* Local-only actions (director commands, recordings changes, the director viewer flag):
 * peer 127.x and no proxy header. The remaining M4 item (owner secret) is deferred. */
static int http_request_is_local(SOCKET s, const char *req) {
    return http_peer_is_local(s) && !http_has_proxy_header(req);
}

static void http_send_json_error(SOCKET client, const char *status, const char *msg) {
    char body[256];
    _snprintf(body, sizeof body, "{\"error\":\"%s\"}", msg);
    http_send(client, status, "application/json", body, (int)strlen(body));
    http_close(client);
}

/* R1-B (API 10, spec 5.3): 503 for a review index that is still building, with Retry-After: 1. The body is the
 * adapter's JSON. Never waits: the viewer asks again. */
static void http_send_retry_after(SOCKET client, const char *body, int blen) {
    char hdr[256];
    int hl = _snprintf(hdr, sizeof hdr,
        "HTTP/1.1 503 Service Unavailable\r\nRetry-After: 1\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n", blen);
    if (hl > 0 && send_complete(client, hdr, hl)) send_complete(client, body, blen);
    http_close(client);
}

/* 1.0.1 review mode: the console's endpoints with ?session=<id> answer from
 * the archived session on disk instead of live memory. Returns 1 when the
 * request was answered. */
static int http_archive_route(SOCKET client, const char *path, const char *query, const char *id) {
    char *buf = g_http_big;
    int len = -1;
    const char *raw = NULL;
    if (!strcmp(path, "/telemetry")) raw = "frame.json";
    else if (!strcmp(path, "/weather")) raw = "weather.json";
    else if (!strcmp(path, "/track")) raw = "track.json";
    if (raw) {
        size_t n = 0;
        char *doc;
        AcquireSRWLockExclusive(&g_ar_lock);
        doc = ar_read_file(id, raw, &n);
        ReleaseSRWLockExclusive(&g_ar_lock);
        if (!doc || !n) { free(doc); http_send_json_error(client, "404 Not Found", "not recorded for this session"); return 1; }
        http_send(client, "200 OK", "application/json", doc, (int)n);
        free(doc);
        http_close(client);
        return 1;
    }
    AcquireSRWLockExclusive(&g_ar_lock);
    format_failed = 0;
    if (!strcmp(path, "/laps")) len = ar_build_laps_json(id, buf, TRACE_JSON_BYTES);
    else if (!strcmp(path, "/pits")) len = ar_build_pits_json(id, buf, TRACE_JSON_BYTES);
    else if (!strcmp(path, "/incidents")) len = ar_build_incidents_json(id, buf, TRACE_JSON_BYTES);
    else if (!strcmp(path, "/trace/status") || !strcmp(path, "/trace/select"))
        len = ar_build_trace_status_json(id, buf, TRACE_JSON_BYTES);
    else if (!strcmp(path, "/driver")) {
        char id_text[32] = {0}, *end = NULL;
        long slot = -1; errno = 0;
        if (qs_get(query, "id", id_text, sizeof id_text)) slot = strtol(id_text, &end, 10);
        if (errno == ERANGE || !end || *end || !id_text[0] || slot < 0 || slot >= MAX_CARS) len = -2;
        else len = ar_build_driver_json(id, (int)slot, buf, TRACE_JSON_BYTES);
    } else if (!strcmp(path, "/trace/data")) {
        int which = -1, lap = -1;
        char lap_text[24], *end = NULL;
        if (http_car_select(query, &which) && which >= 1000 && qs_get(query, "lap", lap_text, sizeof lap_text)) {
            errno = 0; long v = strtol(lap_text, &end, 10);
            if (errno != ERANGE && end && end != lap_text && !*end && v >= 0 && v <= 100000) lap = (int)v;
        }
        if (which < 1000 || lap < 0) len = -2;
        else len = ar_build_trace_data_json(id, which - 1000, lap, buf, TRACE_JSON_BYTES);
    } else if (!strcmp(path, "/trace/open") || !strcmp(path, "/trace/compare") || !strcmp(path, "/trace/export")) {
        len = -3;
    } else {
        ReleaseSRWLockExclusive(&g_ar_lock);
        return 0;   /* e.g. "/" itself: the page is the same in review mode */
    }
    ReleaseSRWLockExclusive(&g_ar_lock);
    if (len == -2) { http_send_json_error(client, "400 Bad Request", "expected car=<slot> and lap=N, or id=<slot>"); return 1; }
    if (len == -3) { http_send_json_error(client, "404 Not Found", "not available for an archived session"); return 1; }
    if (len < 0) { http_send_json_error(client, "404 Not Found", "lap not recorded in this session"); return 1; }
    http_send(client, "200 OK", "application/json", buf, len);
    http_close(client);
    return 1;
}

/* Owner option switch from the TV Director page: POST /director/options {"hide_hud": true|false}. Same guards as the commands
 * (Enabled=1, Host allowlist, this PC only, X-Telemetry-Action: director, JSON, 4 KB cap). Turning hide_hud off gives the HUD
 * back on the game thread (dcam_hud_option_task). The ini value is the start value; the page setting lasts until the game closes. */
static void director_options_route(SOCKET client, const char *req, int received)
{
    long length = http_content_length(req);
    unsigned char *body = NULL;
    char *text, out[128];
    const char *v;
    int on;
    if (!g_dcam_enabled) { http_send_json_error(client, "404 Not Found", "TV Director writes are off ([Director] Enabled=0)"); return; }
    if (!dcam_host_ok(req)) { http_send_json_error(client, "403 Forbidden", "Host header not allowed (use localhost)"); return; }
    if (!http_request_is_local(client, req)) { http_send_json_error(client, "403 Forbidden", "director options only from the game PC"); return; }
    if (!http_header_is(req, "x-telemetry-action", "director")) {
        http_send_json_error(client, "403 Forbidden", "missing X-Telemetry-Action: director");
        return;
    }
    if (!http_header_is(req, "content-type", "application/json")) {
        http_send_json_error(client, "415 Unsupported Media Type", "Content-Type application/json required");
        return;
    }
    if (length <= 0 || length > DCAM_BODY_MAX) {
        http_send_json_error(client, length > DCAM_BODY_MAX ? "413 Payload Too Large" : "400 Bad Request",
                             "the JSON body must be 1 to 4096 bytes");
        return;
    }
    if (!http_read_body(client, req, received, length, &body)) {
        http_send_json_error(client, "400 Bad Request", "incomplete body");
        return;
    }
    text = (char *)malloc((size_t)length + 1);
    if (!text) { free(body); http_send_json_error(client, "500 Internal Server Error", "out of memory"); return; }
    memcpy(text, body, (size_t)length);
    text[length] = 0;
    free(body);
    v = strstr(text, "\"hide_hud\"");
    if (v) v = strchr(v + 10, ':');
    if (v) { v++; while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n') v++; }
    if (!v || (strncmp(v, "true", 4) && strncmp(v, "false", 5))) {
        free(text);
        http_send_json_error(client, "400 Bad Request", "hide_hud must be true or false, for example {\"hide_hud\": false}");
        return;
    }
    on = !strncmp(v, "true", 4);
    free(text);
    InterlockedExchange(&g_dcam_hide_hud, on ? 1 : 0);
    log_f("director: option hide_hud=%d from the page", on);
    if (!on && telemetry_broker.post) (void)telemetry_broker.post(telemetry_token, dcam_hud_option_task, NULL);
    snprintf(out, sizeof out, "{\"hide_hud\":%s}", on ? "true" : "false");
    http_send(client, "200 OK", "application/json", out, (int)strlen(out));
}

/* TV Director phase 1 POST routes (spec 6.2, 6.3): /director/cut, /director/angle, /director/restore. Guards in the order of
 * the recordings routes: Enabled=1, Host allowlist, this PC only, X-Telemetry-Action: director, JSON, 4 KB cap. The write
 * itself runs on the game thread (dcam_cmd_task, posted through the telemetry broker). This handler never calls game code. */
static void director_post_route(SOCKET client, const char *req, int received, const char *path)
{
    static const char *const kinds[3] = { "cut", "angle", "restore" };
    long length = http_content_length(req);
    unsigned char *body = NULL;
    char *text;
    char out[DCAM_RESULT_REASON * 2 + 512];
    char driver[NAME_LEN], angle[DCAM_NAME_MAX + 8];
    DcamCmd cmd;
    DcamResult res;
    uint32_t seq = 0, index = 0;
    int kind, mode = 0, waited;

    if (!strcmp(path, "/director/cut")) kind = DCAM_CMD_CUT;
    else if (!strcmp(path, "/director/angle")) kind = DCAM_CMD_ANGLE;
    else if (!strcmp(path, "/director/restore")) kind = DCAM_CMD_RESTORE;
    else { http_send_json_error(client, "404 Not Found", "unknown director route"); return; }
    if (!g_dcam_enabled) { http_send_json_error(client, "404 Not Found", "TV Director writes are off ([Director] Enabled=0)"); return; }
    if (!dcam_host_ok(req)) { http_send_json_error(client, "403 Forbidden", "Host header not allowed (use localhost)"); return; }
    if (!http_request_is_local(client, req)) { http_send_json_error(client, "403 Forbidden", "director commands only from the game PC"); return; }
    if (!http_header_is(req, "x-telemetry-action", "director")) {
        http_send_json_error(client, "403 Forbidden", "missing X-Telemetry-Action: director");
        return;
    }
    if (!http_header_is(req, "content-type", "application/json")) {
        http_send_json_error(client, "415 Unsupported Media Type", "Content-Type application/json required");
        return;
    }
    if (length <= 0 || length > DCAM_BODY_MAX) {
        http_send_json_error(client, length > DCAM_BODY_MAX ? "413 Payload Too Large" : "400 Bad Request",
                             "the JSON body must be 1 to 4096 bytes");
        return;
    }
    if (!http_read_body(client, req, received, length, &body)) {
        http_send_json_error(client, "400 Bad Request", "incomplete body");
        return;
    }
    text = (char *)malloc((size_t)length + 1);
    if (!text) { free(body); http_send_json_error(client, "500 Internal Server Error", "out of memory"); return; }
    memcpy(text, body, (size_t)length);
    text[length] = 0;
    free(body);

    memset(&cmd, 0, sizeof cmd);
    cmd.kind = kind;
    if (kind == DCAM_CMD_CUT) {
        /* 2026-10-14: a Cut takes {"position": N} (race position 1..104, resolved on the game thread) or {"driver": "name"}. */
        int position = 0;
        if (dcam_json_int(text, "position", &position)) {
            if (position < 1 || position > VEH_SLOTS) {
                free(text);
                http_send_json_error(client, "400 Bad Request", "position must be a whole number from 1 to 104");
                return;
            }
            cmd.position = position;
        } else if (strstr(text, "\"position\"") != NULL) {
            free(text);
            http_send_json_error(client, "400 Bad Request", "position must be a whole number, for example {\"position\": 3}");
            return;
        } else if (!dcam_json_string(text, "driver", driver, sizeof driver)) {
            free(text);
            http_send_json_error(client, "400 Bad Request", "driver (exact driver name) or position (1..104) is required");
            return;
        } else {
            snprintf(cmd.driver, sizeof cmd.driver, "%s", driver);
        }
    }
    if (kind != DCAM_CMD_RESTORE) {
        if (!dcam_json_string(text, "angle", angle, sizeof angle)) {
            free(text);
            http_send_json_error(client, "400 Bad Request", "angle is required: NOSECAM, COCKPIT, TVCOCKPIT, SWINGMAN or ONBOARD:<name>");
            return;
        }
        dcam_names_refresh();
        if (!dcam_angle_key(angle, &mode, &index)) {
            free(text);
            http_send_json_error(client, "400 Bad Request", "unknown angle (not one of the names in GET /director/state)");
            return;
        }
        cmd.mode = mode;
        cmd.index = index;
    }
    free(text);

    if (!dcam_cmd_submit(&cmd, &seq)) {
        http_send_json_error(client, "409 Conflict", "another director command is still pending");
        return;
    }
    if (!telemetry_broker.post || telemetry_broker.post(telemetry_token, dcam_cmd_task, NULL) != F1C_BROKER_OK)
        dcam_cmd_abandon(seq, "game broker unavailable; nothing was written");
    waited = dcam_cmd_wait(seq, &res);
    if (waited == DCAM_WAIT_RUNNING) {
        memset(&res, 0, sizeof res);
        res.seq = seq;
        dcam_res(&res, "queued", "the game thread has the command; the result arrives in GET /director/state (last_result)");
    } else {
        EnterCriticalSection(&g_telemetry_cs);
        res = g_dcam_result;
        LeaveCriticalSection(&g_telemetry_cs);
    }
    dcam_cmd_json(out, sizeof out, kinds[kind - 1], &res);
    http_send(client, "200 OK", "application/json", out, (int)strlen(out));
}

/* M5 (security review 2026-10-13): one reader thread per connection reads the request header
 * (idle 5 s, absolute 30 s). Admission: 32 connections in total and 4 per non-loopback peer.
 * Loopback is exempt from the per-peer cap because the local console polls in parallel.
 * Requests are then handed to ONE dispatcher thread, which runs every route as before: the
 * route code shares static buffers (g_http_big, director and archive state) and is not
 * re-entrant, so it never runs on two threads. Response sends run on the dispatcher. */
#define HTTP_MAX_CONNECTIONS  32
#define HTTP_MAX_PER_IP       4
#define HTTP_HEADER_IDLE_MS   5000
#define HTTP_HEADER_ABS_MS    30000

typedef struct HttpJob {
    SOCKET s;
    int    slot;
    int    n;
    int    complete;
    char   req[2048];
} HttpJob;

static CRITICAL_SECTION g_conn_cs;
static int   g_conn_used[HTTP_MAX_CONNECTIONS];
static DWORD g_conn_ip[HTTP_MAX_CONNECTIONS];
static int   g_conn_active = 0;
static HttpJob *g_job_queue[HTTP_MAX_CONNECTIONS];
static int   g_job_count = 0;
static HANDLE g_job_sem = NULL;
static volatile LONG g_http_dispatch_stop = 0;

static int http_conn_admit(DWORD ip) {
    int slot = -1, same = 0;
    int local = (ntohl(ip) >> 24) == 127;
    EnterCriticalSection(&g_conn_cs);
    for (int i = 0; i < HTTP_MAX_CONNECTIONS; i++) {
        if (g_conn_used[i]) { if (g_conn_ip[i] == ip) same++; }
        else if (slot < 0) slot = i;
    }
    if (slot < 0 || (!local && same >= HTTP_MAX_PER_IP)) {
        slot = -1;
    } else {
        g_conn_used[slot] = 1;
        g_conn_ip[slot] = ip;
        g_conn_active++;
    }
    LeaveCriticalSection(&g_conn_cs);
    return slot;
}

static void http_conn_release(int slot) {
    EnterCriticalSection(&g_conn_cs);
    g_conn_used[slot] = 0;
    g_conn_active--;
    LeaveCriticalSection(&g_conn_cs);
}

static DWORD WINAPI http_reader_thread(LPVOID param) {
    HttpJob *job = (HttpJob *)param;
    DWORD start = GetTickCount(), last = start;
    u_long mode = 1;
    ioctlsocket(job->s, FIONBIO, &mode);
    while (job->n < (int)sizeof job->req - 1) {
        DWORD now = GetTickCount();
        int received;
        if (now - start >= HTTP_HEADER_ABS_MS || now - last >= HTTP_HEADER_IDLE_MS) break;
        received = recv(job->s, job->req + job->n, (int)sizeof job->req - 1 - job->n, 0);
        if (received > 0) {
            job->n += received;
            job->req[job->n] = 0;
            last = GetTickCount();
            if (strstr(job->req, "\r\n\r\n")) { job->complete = 1; break; }
            continue;
        }
        if (received == 0) break;
        if (WSAGetLastError() != WSAEWOULDBLOCK) break;
        Sleep(5);
    }
    EnterCriticalSection(&g_conn_cs);
    g_job_queue[g_job_count++] = job;
    LeaveCriticalSection(&g_conn_cs);
    ReleaseSemaphore(g_job_sem, 1, NULL);
    return 0;
}

static void http_serve(HttpJob *job) {
    SOCKET client = job->s;
    char *req = job->req;
    int n = job->n, complete = job->complete;
    format_failed=0;
    if (!complete) {
        const char *error = "{\"error\":\"incomplete request\"}";
        http_send(client,"400 Bad Request","application/json",error,(int)strlen(error));
        http_close(client); return;
    }
    req[n] = '\0';
    char method[16] = {0}, path[256] = {0}, protocol[16] = {0};
    int used=0;
    const char *line_end=strstr(req,"\r\n");
    int fields=sscanf(req, "%15s %255s %15s%n", method, path, protocol, &used);
    while (req[used]==' ' || req[used]=='\t') used++;
    if (fields!=3 || !line_end || req+used!=line_end ||
        (strcmp(protocol,"HTTP/1.1") && strcmp(protocol,"HTTP/1.0"))) {
        const char *error="{\"error\":\"malformed request line\"}";
        http_send(client,"400 Bad Request","application/json",error,(int)strlen(error));
        http_close(client); return;
    }
    /* M3 (security review 2026-10-13): the Host allowlist applies to every route, reads included. */
    if (!dcam_host_ok(req)) {
        http_send_json_error(client, "403 Forbidden", "Host header not allowed (use the listener address and port)");
        return;
    }
    int is_post = strcmp(method, "POST") == 0;
    int is_head = strcmp(method, "HEAD") == 0;
    if (!is_post && strcmp(method, "GET") != 0) {
        const char *error = "{\"error\":\"method not allowed\"}";
        http_send(client, "405 Method Not Allowed", "application/json",
                  is_head ? "" : error, is_head ? 0 : (int)strlen(error));
        http_close(client); return;
    }

    char *query = strchr(path, '?');
    if (query) { *query = '\0'; query++; } else { query = ""; }

    /* Batch 4 (API 9): recordings manager (STORAGE_MANAGER_SPEC 8.1-8.3). Reads are GET. Every state-changing route
     * is POST with the 1.0.1 guards: this PC only (http_peer_is_local), a custom X-Telemetry-Action header that names
     * the action (a foreign page cannot send it without a preflight this server never approves), and a JSON body of
     * at most 4 KB (M8). The 1.0.1 routes GET /sessions and POST /sessions/delete (hard delete) are removed in API 9:
     * deleting moves files to the Recycle Bin through storage_jobs.inc only. */
    if (!is_post && !strcmp(path, "/recordings")) {
        size_t len = 0;
        int code = sa_api_recordings(g_http_big, TRACE_JSON_BYTES, &len);
        http_send(client, sa_status_line(code), "application/json", g_http_big, (int)len);
        http_close(client); return;
    }
    if (!is_post && !strcmp(path, "/recordings/jobs")) {
        size_t len = 0;
        int code = sa_api_jobs(g_http_big, TRACE_JSON_BYTES, &len);
        http_send(client, sa_status_line(code), "application/json", g_http_big, (int)len);
        http_close(client); return;
    }
    if (is_post && (!strcmp(path, "/recordings/load") || !strcmp(path, "/recordings/archive") ||
                    !strcmp(path, "/recordings/delete"))) {
        const char *action = path + 12;   /* after "/recordings/" */
        long length = http_content_length(req);
        unsigned char *body = NULL;
        size_t len = 0;
        int code;
        if (!http_request_is_local(client, req)) { http_send_json_error(client, "403 Forbidden", "recordings can be changed only from the game PC"); return; }
        if (!http_header_is(req, "x-telemetry-action", action)) {
            char msg[96];
            _snprintf(msg, sizeof msg, "missing X-Telemetry-Action: %s", action);
            http_send_json_error(client, "403 Forbidden", msg);
            return;
        }
        if (!http_header_is(req, "content-type", "application/json")) { http_send_json_error(client, "415 Unsupported Media Type", "Content-Type application/json required"); return; }
        if (length <= 0 || length > (long)SA_BODY_MAX) {
            http_send_json_error(client, length > (long)SA_BODY_MAX ? "413 Payload Too Large" : "400 Bad Request", "the JSON body must be 1 to 4096 bytes");
            return;
        }
        if (!http_read_body(client, req, n, length, &body)) { http_send_json_error(client, "400 Bad Request", "incomplete body"); return; }
        code = sa_api_action(action, (const char *)body, (size_t)length, g_http_big, TRACE_JSON_BYTES, &len);
        free(body);
        log_f("recordings: %s answered %d", action, code);
        http_send(client, sa_status_line(code), "application/json", g_http_big, (int)len);
        http_close(client); return;
    }
    /* R1-A (API 10, spec 5.3): review index state of a recorded session. Read-only; this PC only (S-R2, decision D4:
     * the 1.0.1 review routes below have no peer check). The v2 review ids never reach ar_session_exists. */
    if (!is_post && !strcmp(path, "/recordings/review")) {
        char rid[32] = {0};
        size_t len = 0;
        int code;
        if (!http_request_is_local(client, req)) { http_send_json_error(client, "403 Forbidden", "the review can be opened only from the game PC"); return; }
        if (!qs_get(query, "session", rid, sizeof rid)) { http_send_json_error(client, "400 Bad Request", "session=<review_id> is required"); return; }
        code = sa_api_review(rid, g_http_big, TRACE_JSON_BYTES, &len);
        http_send(client, sa_status_line(code), "application/json", g_http_big, (int)len);
        http_close(client); return;
    }
    /* R1-B (API 10): a v2 review id goes to the review adapter (storage_api.inc) for the review routes. Any other path
     * (the console page with ?session=v2_...) falls through to its normal handler; the 1.0.1 guard below never sees a
     * v2 id, so it cannot 404 it. */
    int v2_session = 0;
    if (!is_post) {
        char v2_id[32] = {0};
        if (qs_get(query, "session", v2_id, sizeof v2_id) && !strncmp(v2_id, "v2_", 3)) {
            v2_session = 1;
            if (sa_rv_is_api_route(path)) {
                char car_s[16] = {0}, lap_s[16] = {0};
                char id_s[16] = {0}, t_s[40] = {0}, t0_s[40] = {0}, t1_s[40] = {0};   /* REPLAY 2.1/2.2: /driver id, t; /replay t0, t1 */
                int car = -1, lap = -1, code;
                size_t len = 0;
                if (!http_request_is_local(client, req)) { http_send_json_error(client, "403 Forbidden", "the review can be opened only from the game PC"); return; }
                if (qs_get(query, "car", car_s, sizeof car_s)) car = atoi(car_s);
                if (qs_get(query, "lap", lap_s, sizeof lap_s)) lap = atoi(lap_s);
                code = sa_api_review_route(path, v2_id, car, lap,
                                           qs_get(query, "id", id_s, sizeof id_s) ? id_s : NULL,
                                           qs_get(query, "t", t_s, sizeof t_s) ? t_s : NULL,
                                           qs_get(query, "t0", t0_s, sizeof t0_s) ? t0_s : NULL,
                                           qs_get(query, "t1", t1_s, sizeof t1_s) ? t1_s : NULL,
                                           g_http_big, TRACE_JSON_BYTES, &len);
                if (code == 503) http_send_retry_after(client, g_http_big, (int)len);
                else {
                    http_send(client, sa_status_line(code), "application/json", g_http_big, (int)len);
                    http_close(client);
                }
                return;
            }
        }
    }
    if (!is_post && !v2_session) {
        char review_id[AR_ID_LEN + 8] = {0};
        if (qs_get(query, "session", review_id, sizeof review_id)) {
            if (!ar_id_valid(review_id) || !ar_session_exists(review_id)) {
                http_send_json_error(client, "404 Not Found", "unknown or deleted session");
                return;
            }
            if (http_archive_route(client, path, query, review_id)) return;
        }
    }

    /* TV Director phase 1: POST commands (spec 6.2/6.3). Writes need [Director] Enabled=1; see director_post_route. */
    if (is_post && (!strcmp(path, "/director/cut") || !strcmp(path, "/director/angle") ||
                    !strcmp(path, "/director/restore"))) {
        director_post_route(client, req, n, path);
        http_close(client); return;
    }
    if (is_post && !strcmp(path, "/director/options")) {
        director_options_route(client, req, n);
        http_close(client); return;
    }
    /* The only other write-shaped route is the bounded legacy import; it never
     * touches game state (decoder + conversion run on this HTTP thread). */
    if (is_post) {
        if (strcmp(path, "/trace/import/legacy") != 0) {
            const char *error = "{\"error\":\"method not allowed\"}";
            http_send(client, "405 Method Not Allowed", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        char car_text[8] = {0};
        int slot = 0;
        if (qs_get(query, "car", car_text, sizeof car_text)) {
            if (car_text[1] || (car_text[0]!='A' && car_text[0]!='a' && car_text[0]!='B' && car_text[0]!='b')) {
                const char *error="{\"error\":\"expected car=A|B\"}";
                http_send(client,"400 Bad Request","application/json",error,(int)strlen(error));
                http_close(client); return;
            }
            slot = (car_text[0] == 'B' || car_text[0] == 'b') ? 1 : 0;
        }
        /* Cross-origin guard: octet-stream is not a CORS-simple content type,
         * so a foreign page triggers a preflight (which this server never
         * answers with an allow header) instead of a silent no-preflight
         * POST. Same-origin pages and CLI clients are unaffected. */
        if (!http_header_is(req, "content-type", "application/octet-stream")) {
            const char *error = "{\"error\":\"Content-Type application/octet-stream required\"}";
            http_send(client, "415 Unsupported Media Type", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        long length = http_content_length(req);
        const char *error = NULL;
        const char *status = "400 Bad Request";
        unsigned char *body = NULL;
        if (length < 0) error = "{\"error\":\"Content-Length required\"}";
        else if (length == 0) error = "{\"error\":\"empty import body\"}";
        else if (length > (long)IMPORT_BODY_MAX) {
            error = "{\"error\":\"import body exceeds the 16 MB ceiling\"}";
            status = "413 Payload Too Large";
        } else if (!http_read_body(client, req, n, length, &body))
            error = "{\"error\":\"incomplete import body\"}";
        if (error) {
            http_send(client, status, "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        char decode_err[LF_ERR_CAP];
        decode_err[0] = 0;
        int len = build_import_json(slot, body, (size_t)length, decode_err, sizeof decode_err);
        free(body);
        if (len < 0) {
            char esc[LF_ERR_CAP * 6 + 1];
            esc_json(esc, sizeof esc, decode_err[0] ? decode_err : "decode refused");
            char msg[LF_ERR_CAP * 6 + 32];
            _snprintf(msg, sizeof msg, "{\"error\":\"%s\"}", esc);
            http_send(client, "400 Bad Request", "application/json", msg, (int)strlen(msg));
        } else {
            http_send(client, "200 OK", "application/json", g_import_json[slot], len);
        }
        http_close(client); return;
    }

    if (strcmp(path, "/action") == 0) {
        const char *body = "{\"ok\":false,\"err\":\"Race control is unavailable in this read-only compatibility candidate\"}";
        http_send(client, "403 Forbidden", "application/json", body, (int)strlen(body));
        http_close(client); return;
    }
    if (strcmp(path, "/telemetry") == 0) {
        static char data[262144];
        int len=json_telemetry(data,sizeof data);
        http_send(client,"200 OK","application/json",data,len);
        http_close(client); return;
    }
    if (strcmp(path, "/health") == 0) {
        static char health[16384];
        int len = build_health_json(health, sizeof health);
        http_send(client, "200 OK", "application/json", health, len);
        http_close(client); return;
    }
    /* TV Director GET /director/state: reads always on (the write gate is [Director] Enabled, reported as write_enabled). */
    if (!is_post && !strcmp(path, "/director/state")) {
        static char director_json[8192];
        int len;
        if (!dcam_host_ok(req)) {
            const char *bad = "{\"error\":\"Host header not allowed (use localhost)\"}";
            http_send(client, "403 Forbidden", "application/json", bad, (int)strlen(bad));
        } else {
            len = dcam_state_json(director_json, sizeof director_json, http_request_is_local(client, req));
            if (len < 0) {
                const char *cap = "{\"error\":\"response capacity exceeded\"}";
                http_send(client, "500 Internal Server Error", "application/json", cap, (int)strlen(cap));
            } else {
                http_send(client, "200 OK", "application/json", director_json, len);
            }
        }
        http_close(client); return;
    }
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        http_send(client, "200 OK", "text/html; charset=utf-8",
                  g_console_html, (int)strlen(g_console_html));
        http_close(client); return;
    }
#if 0 /* 4.4.0 multi-driver: /workbench page retired (404). Source kept, compiled out. */
    if (strcmp(path, "/workbench") == 0) {
        http_send(client, "200 OK", "text/html; charset=utf-8",
                  g_workbench_html, (int)strlen(g_workbench_html));
        http_close(client); return;
    }
#endif
    if (strcmp(path, "/viewer.js") == 0) {
        if (!viewer_js_load()) {
            const char *missing = "telemetry_viewer.js missing beside the DLL";
            http_send(client, "404 Not Found", "text/plain", missing, (int)strlen(missing));
        } else {
            http_send(client, "200 OK", "application/javascript; charset=utf-8",
                      g_viewer_js_cache, g_viewer_js_len);
        }
        http_close(client); return;
    }
    if (strcmp(path, "/workbench.js") == 0) {
        if (!workbench_js_load()) {
            const char *missing = "workbench_ui.js missing beside the DLL";
            http_send(client, "404 Not Found", "text/plain", missing, (int)strlen(missing));
        } else {
            http_send(client, "200 OK", "application/javascript; charset=utf-8",
                      g_workbench_js_cache, g_workbench_js_len);
        }
        http_close(client); return;
    }
    if (strcmp(path, "/incidents") == 0) {
        static char inc_buf[262144];
        int len = build_incidents_json(inc_buf, sizeof(inc_buf));
        http_send(client, "200 OK", "application/json", inc_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/pits") == 0) {
        static char pits_buf[262144];
        int len = build_pits_json(pits_buf, sizeof(pits_buf));
        http_send(client, "200 OK", "application/json", pits_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/laps") == 0) {
        static char laps_buf[262144];
        int len = build_laps_json(laps_buf, sizeof(laps_buf));
        http_send(client, "200 OK", "application/json", laps_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/driver") == 0) {
        /* The query string was already split off `path` above, so it must be
         * read from `query` — matching against `path` finds nothing. */
        char id_text[32] = {0}, *end=NULL;
        long id=-1; errno=0;
        if (qs_get(query,"id",id_text,sizeof id_text)) id=strtol(id_text,&end,10);
        if (errno==ERANGE || !end || *end || !id_text[0] || id<0 || id>=MAX_CARS) {
            const char *error="{\"error\":\"invalid driver id\"}";
            http_send(client,"400 Bad Request","application/json",error,(int)strlen(error));
            http_close(client); return;
        }
        static char drv_buf[131072];
        int len = build_driver_json(drv_buf, sizeof(drv_buf), id);
        http_send(client, "200 OK", "application/json", drv_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/track") == 0) {
        static char track_buf[262144];   /* rc.12: pts + dl + wl for 3000 waypoints */
        EnterCriticalSection(&g_telemetry_cs);
        int len = build_track_json(track_buf, sizeof(track_buf));
        LeaveCriticalSection(&g_telemetry_cs);
        http_send(client, "200 OK", "application/json", track_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/events") == 0) {
        const char *hdr = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
            /* 1.0.1: no Access-Control-Allow-Origin: other web sites open in
             * the same browser must not read the live feed. */
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n\r\n";
        if (!send_complete(client, hdr, (int)strlen(hdr))) { http_close(client); return; }
        sse_add(client);
        return;
    }
    /* ---- Session weather history (rc.11 weather; read-only). ---- */
    if (strcmp(path, "/weather") == 0) {
        static char weather_buf[WEATHER_JSON_BYTES];
        int len = build_weather_json(weather_buf, sizeof(weather_buf));
        http_send(client, "200 OK", "application/json", weather_buf, len);
        http_close(client); return;
    }
    /* ---- Engineering trace endpoints (read-only; selection is internal
     * state, never a game write). ---- */
    if (strcmp(path, "/trace/status") == 0) {
        char *trace_buf = g_http_big;   /* 1.0.1: one shared 2 MB buffer (HTTP is single-threaded) */
        int len = build_trace_status_json(trace_buf, TRACE_JSON_BYTES);
        http_send(client, "200 OK", "application/json", trace_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/trace/open") == 0) {
        int which = -1;
        if (!http_car_select(query, &which)) {
            const char *error = "{\"error\":\"expected car=A|B or a vehicle slot 0..103\"}";
            http_send(client, "400 Bad Request", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        char *open_buf = g_http_big;
        int len = build_trace_open_json(open_buf, TRACE_JSON_BYTES, which);
        if (len < 0) {
            const char *error = "{\"error\":\"no open lap with samples for this side\"}";
            http_send(client, "404 Not Found", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        http_send(client, "200 OK", "application/json", open_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/trace/import") == 0) {
        char car_text[8] = {0};
        int slot = 0;
        if (qs_get(query, "car", car_text, sizeof car_text)) {
            if (car_text[1] || (car_text[0]!='A' && car_text[0]!='a' && car_text[0]!='B' && car_text[0]!='b')) {
                const char *error="{\"error\":\"expected car=A|B\"}";
                http_send(client,"400 Bad Request","application/json",error,(int)strlen(error));
                http_close(client); return;
            }
            slot = (car_text[0] == 'B' || car_text[0] == 'b') ? 1 : 0;
        }
        if (g_import_len[slot] <= 0) {
            const char *error = "{\"error\":\"no imported lap for this side\"}";
            http_send(client, "404 Not Found", "application/json", error, (int)strlen(error));
        } else {
            http_send(client, "200 OK", "application/json", g_import_json[slot], g_import_len[slot]);
        }
        http_close(client); return;
    }
    if (strcmp(path, "/trace/select") == 0) {
        char which_text[8], car_text[24], *end = NULL;
        int which = -1, car = -1;
        if (qs_get(query, "which", which_text, sizeof which_text) && which_text[0] && !which_text[1])
            which = (which_text[0] == 'B' || which_text[0] == 'b') ? 1
                  : (which_text[0] == 'A' || which_text[0] == 'a') ? 0 : -1;
        if (qs_get(query, "car", car_text, sizeof car_text)) {
            errno=0; long v = strtol(car_text, &end, 10);
            if (errno==ERANGE || !end || end==car_text || *end || v < -1 || v >= MAX_CARS) which = -1;
            else car = (int)v;
        } else {
            which = -1;
        }
        if (which < 0) {
            const char *error = "{\"error\":\"expected which=A|B and car=0..103 or -1\"}";
            http_send(client, "400 Bad Request", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        EnterCriticalSection(&g_telemetry_cs);
        char gen_text[24];
        if (qs_get(query,"gen",gen_text,sizeof gen_text)) {
            char *gen_end=NULL;
            errno=0; unsigned long gen=strtoul(gen_text,&gen_end,10);
            if (errno==ERANGE || gen_text[0]<'0' || gen_text[0]>'9' || gen_end==gen_text || *gen_end || gen!=capture_generation) {
                LeaveCriticalSection(&g_telemetry_cs);
                const char *error="{\"error\":\"session changed\"}";
                http_send(client,"409 Conflict","application/json",error,(int)strlen(error));
                http_close(client); return;
            }
        }
        trace_select(which, car, car >= 0 ? 1 : 0);
        LeaveCriticalSection(&g_telemetry_cs);
        char *trace_buf = g_http_big;   /* 1.0.1: one shared 2 MB buffer (HTTP is single-threaded) */
        int len = build_trace_status_json(trace_buf, TRACE_JSON_BYTES);
        http_send(client, "200 OK", "application/json", trace_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/trace/data") == 0 || strcmp(path, "/trace/export") == 0) {
        char lap_text[24], fmt_text[8], gen_text[24], rev_text[24], *end = NULL;
        int which = -1, lap = -1, csv = strcmp(path,"/trace/export")==0;
        long gen = -1, rev = -1;
        if (!http_car_select(query, &which)) which = -1;
        if (qs_get(query, "lap", lap_text, sizeof lap_text)) {
            errno=0; long v = strtol(lap_text, &end, 10);
            if (errno==ERANGE || !end || end==lap_text || *end || v < 0 || v > 100000) which = -1;
            else lap = (int)v;
        } else {
            which = -1;
        }
        /* Optional identity pins (fix 2): a pinned client detects eviction or
         * a recorder reset explicitly instead of a silent lap-number alias. */
        if (qs_get(query, "gen", gen_text, sizeof gen_text)) {
            errno=0; long v = strtol(gen_text, &end, 10);
            if (errno==ERANGE || !end || end==gen_text || *end || v < 0) which = -1; else gen = v;
        }
        if (qs_get(query, "rev", rev_text, sizeof rev_text)) {
            errno=0; long v = strtol(rev_text, &end, 10);
            if (errno==ERANGE || !end || end==rev_text || *end || v < 0) which = -1; else rev = v;
        }
        if (qs_get(query, "fmt", fmt_text, sizeof fmt_text)) {
            if (strcmp(fmt_text,"csv") && strcmp(fmt_text,"json")) which=-1;
            csv = strcmp(fmt_text,"csv")==0;
        }
        if (which < 0 || lap < 0) {
            const char *error = "{\"error\":\"expected car=A|B|<slot> and lap=N[&gen=G][&rev=R]\"}";
            http_send(client, "400 Bad Request", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        char *trace_buf = g_http_big;   /* 1.0.1: one shared 2 MB buffer (HTTP is single-threaded) */
        int len = csv ? build_trace_data_csv_id(trace_buf, TRACE_JSON_BYTES, which, lap, gen, rev)
                      : build_trace_data_json_id(trace_buf, TRACE_JSON_BYTES, which, lap, gen, rev);
        /* 1.0.1: a lap the 6-lap memory ring no longer holds is served from this
         * session's archive on disk (JSON, explicit vehicle slot, no rev pin). */
        if ((len == -1 || len == -2) && !csv && which >= 1000 && rev < 0) {
            int alen = ar_live_fallback_trace(which - 1000, lap, trace_buf, TRACE_JSON_BYTES);
            if (alen > 0) len = alen;
        }
        if (len < 0) {
            char msg[192];
            const char *status = "404 Not Found";
            const char *error = "lap not recorded";
            const char *evicted = "false";
            if (len == -2) { error = "lap evicted from the recorder ring"; evicted = "true"; }
            else if (len == -3) { error = "session changed"; status = "409 Conflict"; evicted = "true"; }
            else if (len == -4) { error = "lap identity changed (recorder reset)"; status = "409 Conflict"; evicted = "true"; }
            _snprintf(msg, sizeof msg, "{\"error\":\"%s\",\"evicted\":%s}", error, evicted);
            http_send(client, status, "application/json", msg, (int)strlen(msg));
            http_close(client); return;
        }
        http_send(client, "200 OK",
                  csv ? "text/csv; charset=utf-8" : "application/json",
                  trace_buf, len);
        http_close(client); return;
    }
    if (strcmp(path, "/trace/compare") == 0) {
        char a_text[8] = {0}, b_text[8] = {0}, la_text[24], lb_text[24], step_text[16], gen_text[24], *end = NULL;
        int wa = 0, wb = 1, la = -1, lb = -1, ia = 0, ib = 0;
        float step = 5.0f;
        int ok = 1;
        if (qs_get(query, "carA", a_text, sizeof a_text)) {
            if (a_text[1] || (a_text[0]!='A' && a_text[0]!='a' && a_text[0]!='B' && a_text[0]!='b')) ok=0;
            wa = (a_text[0] == 'B' || a_text[0] == 'b') ? 1 : 0;
        }
        if (qs_get(query, "carB", b_text, sizeof b_text)) {
            if (b_text[1] || (b_text[0]!='A' && b_text[0]!='a' && b_text[0]!='B' && b_text[0]!='b')) ok=0;
            wb = (b_text[0] == 'B' || b_text[0] == 'b') ? 1 : 0;
        }
        /* lap tokens are a lap number or the literal 'import' (fix 2): the
         * cached legacy import for that side is used as the reference lap. */
        if (qs_get(query, "lapA", la_text, sizeof la_text)) {
            if (!strcmp(la_text, "import")) ia = 1;
            else {
                errno=0; long v = strtol(la_text, &end, 10);
                if (errno==ERANGE || !end || end==la_text || *end || v < 0) ok = 0; else la = (int)v;
            }
        } else ok = 0;
        if (qs_get(query, "lapB", lb_text, sizeof lb_text)) {
            if (!strcmp(lb_text, "import")) ib = 1;
            else {
                errno=0; long v = strtol(lb_text, &end, 10);
                if (errno==ERANGE || !end || end==lb_text || *end || v < 0) ok = 0; else lb = (int)v;
            }
        } else ok = 0;
        if (qs_get(query, "step", step_text, sizeof step_text)) {
            errno=0; char *step_end=NULL; float v=strtof(step_text,&step_end);
            /* Keep the frozen v3 fallback rule for invalid optional steps. */
            if (errno!=ERANGE && step_end!=step_text && !*step_end && isfinite(v) && v>=1.0f && v<=50.0f) step=v;
        }
        if (!ok) {
            const char *error = "{\"error\":\"expected carA, lapA, carB, lapB (lap is N or import)\"}";
            http_send(client, "400 Bad Request", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        if (qs_get(query, "gen", gen_text, sizeof gen_text)) {
            errno=0; long v = strtol(gen_text, &end, 10);
            if (errno==ERANGE || !end || end==gen_text || *end || v < 0 ||
                (unsigned long)v != capture_generation) {
                const char *error = "{\"error\":\"session changed\"}";
                http_send(client, "409 Conflict", "application/json", error, (int)strlen(error));
                http_close(client); return;
            }
        }
        char *trace_buf = g_http_big;   /* 1.0.1: one shared 2 MB buffer (HTTP is single-threaded) */
        int len = build_trace_compare_json(trace_buf, TRACE_JSON_BYTES, wa, la, ia, wb, lb, ib, step);
        if (len < 0) {
            const char *error = (len == -2)
                ? "{\"error\":\"lap distances unknown\"}"
                : "{\"error\":\"lap not recorded or import missing for this side\"}";
            http_send(client, "404 Not Found", "application/json", error, (int)strlen(error));
            http_close(client); return;
        }
        http_send(client, "200 OK", "application/json", trace_buf, len);
        http_close(client); return;
    }
    /* 4.4.0 batch 1: unknown paths (including the retired /classic and /viewer) close like every other response. */
    http_send(client, "404 Not Found", "application/json", "{\"error\":\"not found\"}", 21);
    http_close(client);
}

/* M1 (security review 2026-10-13): Remote=1 listens on [Mod] BindAddress only, and only when it is a
 * private IPv4 address: 10/8, 172.16/12, 192.168/16, 100.64/10 (Tailscale), 25/8 (Hamachi) or 127/8.
 * 0.0.0.0, public addresses and malformed text are refused and the listener stays on this PC.
 * No override exists in this release. */
static int http_bind_private_ipv4(const char *text, ULONG *s_addr_out) {
    unsigned a[4] = { 0, 0, 0, 0 };
    const char *p = text;
    for (int i = 0; i < 4; i++) {
        unsigned v = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (unsigned)(*p - '0');
            if (++digits > 3) return 0;
            p++;
        }
        if (!digits || v > 255) return 0;
        a[i] = v;
        if (i < 3) { if (*p != '.') return 0; p++; }
    }
    if (*p) return 0;
    if (a[0] == 10 || a[0] == 127 || a[0] == 25 || (a[0] == 192 && a[1] == 168) ||
        (a[0] == 172 && a[1] >= 16 && a[1] <= 31) || (a[0] == 100 && a[1] >= 64 && a[1] <= 127)) {
        if (s_addr_out) *s_addr_out = htonl((ULONG)((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]));
        return 1;
    }
    return 0;
}

static void http_bind_resolve(void) {
    ULONG net_addr = 0;
    unsigned host;
    g_bind_s_addr = 0;
    strcpy(g_bind_text, "127.0.0.1");
    if (!g_remote_access) return;
    if (!g_bind_cfg[0]) {
        log_w("Remote=1 needs [Mod] BindAddress=<private IPv4>; listening on this PC only");
        return;
    }
    if (!http_bind_private_ipv4(g_bind_cfg, &net_addr)) {
        log_f("[Mod] BindAddress=%s refused (not a private LAN/VPN IPv4); listening on this PC only", g_bind_cfg);
        return;
    }
    g_bind_s_addr = net_addr;
    host = (unsigned)ntohl(net_addr);
    _snprintf(g_bind_text, sizeof g_bind_text, "%u.%u.%u.%u",
              (host >> 24) & 255u, (host >> 16) & 255u, (host >> 8) & 255u, host & 255u);
    log_f("HTTP listener: Remote=1 on [Mod] BindAddress %s", g_bind_text);
}

/* ========================================================================
 * HTTP LISTENER — configurable port with bounded fallback
 * ========================================================================
 * Tries the configured port, then the next up-to-5 ports. Returns the bound
 * port, or 0 when every candidate is occupied; in that case the caller logs
 * "HTTP disabled: no port available" and capture keeps running (this runs on
 * the HTTP thread only, never the game callback). */
static int http_listen_open(int preferred) {
    for (int attempt = 0; attempt <= HTTP_PORT_FALLBACK_ATTEMPTS; attempt++) {
        int candidate = preferred + attempt;
        if (candidate > HTTP_PORT_MAX) break;
        g_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_listen_sock == INVALID_SOCKET) { log_w("socket failed"); return 0; }
        BOOL exclusive = TRUE;
        setsockopt(g_listen_sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
        u_long mode = 1;
        ioctlsocket(g_listen_sock, FIONBIO, &mode);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        /* This PC's own listener is always loopback. Remote=1 adds a second listener on the
         * private [Mod] BindAddress only (http_bind_listener_open). Never 0.0.0.0 (M1). */
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((u_short)candidate);
        if (bind(g_listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            log_f("bind port %d failed: %d", candidate, WSAGetLastError());
            closesocket(g_listen_sock);
            g_listen_sock = INVALID_SOCKET;
            continue;
        }
        if (listen(g_listen_sock, SOMAXCONN) == SOCKET_ERROR) {
            log_w("listen failed"); closesocket(g_listen_sock);
            g_listen_sock = INVALID_SOCKET; return 0;
        }
        return candidate;
    }
    return 0;
}

#if 0 /* 4.4.0 batch 1: only the retired /classic page used this */
/* Build the served page once per HTTP start, splicing the bound port into the
 * footer marker "port 8080". Falls back to the static page on failure. */
static void http_page_prepare(int port) {
    const char *marker = strstr(g_html, "port 8080");
    size_t total = strlen(g_html), head;
    char number[8], *page;
    int n;
    if (!marker || g_html_bound) return;
    page = malloc(total + 16);
    if (!page) return;
    head = (size_t)(marker - g_html);
    memcpy(page, g_html, head + 5);                    /* includes "port " */
    n = snprintf(number, sizeof number, "%d", port);
    if (n < 0 || n >= (int)sizeof number) { free(page); return; }
    memcpy(page + head + 5, number, (size_t)n);
    memcpy(page + head + 5 + n, marker + 9, total - head - 9 + 1); /* skip "port 8080" */
    g_html_bound = page;
}
#endif /* http_page_prepare */

/* M1: second listener on the resolved [Mod] BindAddress (Remote=1 only). Same port as this PC's
 * listener. Returns INVALID_SOCKET when the address cannot be bound; the loopback listener still runs. */
static SOCKET g_listen_sock2 = INVALID_SOCKET;
static SOCKET http_bind_listener_open(int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    BOOL exclusive = TRUE;
    u_long mode = 1;
    struct sockaddr_in addr;
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
    ioctlsocket(s, FIONBIO, &mode);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = g_bind_s_addr;
    addr.sin_port = htons((u_short)port);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(s, SOMAXCONN) == SOCKET_ERROR) {
        log_f("BindAddress %s:%d not bound (%d); this PC only", g_bind_text, port, WSAGetLastError());
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static DWORD WINAPI http_dispatch_thread(LPVOID param) {
    (void)param;
    for (;;) {
        HttpJob *job = NULL;
        WaitForSingleObject(g_job_sem, 100);
        EnterCriticalSection(&g_conn_cs);
        if (g_job_count > 0) {
            job = g_job_queue[0];
            memmove(g_job_queue, g_job_queue + 1, (size_t)(g_job_count - 1) * sizeof g_job_queue[0]);
            g_job_count--;
        }
        LeaveCriticalSection(&g_conn_cs);
        if (job) {
            http_serve(job);
            http_conn_release(job->slot);
            free(job);
            continue;
        }
        if (InterlockedCompareExchange(&g_http_dispatch_stop, 0, 0)) break;
    }
    return 0;
}

static DWORD WINAPI http_thread(LPVOID param) {
    (void)param;
    WSADATA wsa;
    int bound;
    HANDLE dispatcher = NULL;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { log_w("WSAStartup failed"); return 1; }
#ifdef TELEMETRY_TEST
    /* The env override exists only in the synthetic harness; the shipped DLL
     * reads the ini beside the module only. */
    telemetry_settings_load(getenv("TELEMETRY_INI"));
#else
    telemetry_settings_load(NULL);
#endif
    http_bind_resolve();
    InitializeCriticalSection(&g_conn_cs);
    memset(g_conn_used, 0, sizeof g_conn_used);
    g_conn_active = 0;
    g_job_count = 0;
    InterlockedExchange(&g_http_dispatch_stop, 0);
    g_job_sem = CreateSemaphoreA(NULL, 0, HTTP_MAX_CONNECTIONS, NULL);
    if (g_job_sem) dispatcher = CreateThread(NULL, 512 * 1024, http_dispatch_thread, NULL, 0, NULL);
    if (!dispatcher) {
        log_w("HTTP disabled: dispatcher thread not created");
        if (g_job_sem) { CloseHandle(g_job_sem); g_job_sem = NULL; }
        DeleteCriticalSection(&g_conn_cs);
        WSACleanup();
        return 1;
    }
    bound = http_listen_open(g_configured_port);
    if (!bound) {
        log_w("HTTP disabled: no port available");
        InterlockedExchange(&g_http_dispatch_stop, 1);
        WaitForSingleObject(dispatcher, 5000);
        CloseHandle(dispatcher);
        CloseHandle(g_job_sem); g_job_sem = NULL;
        DeleteCriticalSection(&g_conn_cs);
        WSACleanup();
        return 1;
    }
    InterlockedExchange(&g_http_bound_port, bound);
    wtu_note_listen(bound, g_listen_sock2 != INVALID_SOCKET);
    if (bound != g_configured_port)
        log_f("HTTP fallback: port %d occupied; bound %d instead", g_configured_port, bound);
    if (g_bind_s_addr) g_listen_sock2 = http_bind_listener_open(bound);
    log_f("HTTP server listening on 127.0.0.1:%d%s", bound,
          g_listen_sock2 != INVALID_SOCKET ? " and the Remote=1 BindAddress" : " (this PC only)");
    while (g_http_running) {
        for (int li = 0; li < 2; li++) {
            struct sockaddr_in ca; int al = sizeof(ca);
            SOCKET listener = li == 0 ? g_listen_sock : g_listen_sock2;
            SOCKET client;
            if (listener == INVALID_SOCKET) continue;
            client = accept(listener, (struct sockaddr*)&ca, &al);
            if (client != INVALID_SOCKET) {
                int slot = http_conn_admit(ca.sin_addr.s_addr);
                HttpJob *job = slot >= 0 ? (HttpJob *)calloc(1, sizeof(HttpJob)) : NULL;
                HANDLE reader = NULL;
                if (job) {
                    job->s = client;
                    job->slot = slot;
                    reader = CreateThread(NULL, 256 * 1024, http_reader_thread, job, 0, NULL);
                }
                if (reader) {
                    CloseHandle(reader);
                } else {
                    if (slot >= 0) http_conn_release(slot);
                    free(job);
                    closesocket(client);
                }
            }
        }
        Sleep(10);
    }
    InterlockedExchange(&g_http_bound_port, 0);
    /* Readers end within the 30 s header cap; the dispatcher then drains the queue and exits. */
    for (int waited = 0; waited < 40000; waited += 10) {
        int active;
        EnterCriticalSection(&g_conn_cs);
        active = g_conn_active;
        LeaveCriticalSection(&g_conn_cs);
        if (!active) break;
        Sleep(10);
    }
    InterlockedExchange(&g_http_dispatch_stop, 1);
    WaitForSingleObject(dispatcher, 10000);
    CloseHandle(dispatcher);
    EnterCriticalSection(&g_sse_cs);
    for (int i = 0; i < g_sse_count; i++) {
        closesocket(g_sse_clients[i].s);
        free(g_sse_clients[i].pend);
    }
    g_sse_count = 0;
    LeaveCriticalSection(&g_sse_cs);
    closesocket(g_listen_sock);
    g_listen_sock = INVALID_SOCKET;
    if (g_listen_sock2 != INVALID_SOCKET) closesocket(g_listen_sock2);
    g_listen_sock2 = INVALID_SOCKET;
    CloseHandle(g_job_sem);
    g_job_sem = NULL;
    DeleteCriticalSection(&g_conn_cs);
    WSACleanup();
    log_w("HTTP server stopped");
    return 0;
}

#include "compatibility_runtime.inc"
