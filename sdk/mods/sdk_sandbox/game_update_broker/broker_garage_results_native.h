/* Native UI bindings reviewed against the exact target, not a worker-thread API. */
#pragma once
#include "broker_garage_results.h"
#include "broker_garage_results_layout.h"

#define GARAGE_RELEASE_ADDRESS ((void *)0x00475050u)
typedef int (__attribute__((thiscall)) *GarageReleaseFn)(void *);
typedef const char *(__attribute__((thiscall)) *GaragePageNameFn)(void *);
static F1CDetour g_garage_release_detour;
static GarageReleaseFn g_garage_release_original;

/* Queue the GUI request; invoke the vanilla ESC leaf only after RestartGate
 * has returned from its UI dispatcher. 1=queued, 2=submitted to native ESC. */
static void *g_garage_escape_manager;
static uint8_t *g_garage_escape_player;

static int garage_context(void *receiver, BrokerGarageResultsContext *c)
{
    void *manager = *(void **)0x007CD8C0u;
    uint8_t *player = *(uint8_t **)0x007C54E8u;
    if (!g_ready || g_stopping || !manager || !player) return 0;
    if (receiver && *(void **)((uint8_t *)receiver + 0x40u) != manager) return 0;
    c->button_kind = receiver ? *(int32_t *)((uint8_t *)receiver + 0xE4u) : 56;
    c->session_type = *(volatile int32_t *)SESSION_TYPE_ADDRESS;
    c->race_mode = *(volatile int32_t *)0x007D0410u;
    c->race_phase = *(volatile int32_t *)0x007C56A8u;
    c->finish_state = *(volatile int32_t *)(player + 0x4064u);
    c->manager = manager;
    c->page = ((GaragePageNameFn)0x00497980u)(manager);
    return 1;
}

static int __attribute__((thiscall)) broker_garage_release_hook(void *receiver)
{
    int consume = 0;
    int result;
    BrokerGarageResultsContext c;
    InterlockedIncrement(&g_active_garage_release_calls);
    while (!g_garage_release_original) Sleep(0);
    if (receiver && garage_context(receiver, &c) && broker_garage_results_eligible(&c)) {
        consume = 1;
        if (!g_garage_results_pending) {
            g_garage_escape_manager = c.manager;
            g_garage_escape_player = *(uint8_t **)0x007C54E8u;
            g_garage_results_pending = 1;
            log_line("garage leave icon: queued native ESC exit page=%s thread=%lu",
                     c.page, (unsigned long)GetCurrentThreadId());
        } else {
            log_line("garage leave icon: native ESC request already pending=%d", g_garage_results_pending);
        }
    }
    result = consume ? -1 : g_garage_release_original(receiver);
    InterlockedDecrement(&g_active_garage_release_calls);
    return result;
}

static int broker_garage_escape_after_restart(int result)
{
    typedef void (__attribute__((thiscall)) *NativeEscapeFn)(void *);
    BrokerGarageResultsContext c;
    const char *results_page;
    uint8_t saved_ui_active;
    int32_t saved_ui_frame;
    if (g_garage_results_pending != 1) return result;
    if ((result != 5 && result != 6) || !garage_context(NULL, &c) ||
        !broker_garage_results_eligible(&c) || c.manager != g_garage_escape_manager ||
        *(uint8_t **)0x007C54E8u != g_garage_escape_player) {
        g_garage_results_pending = 0;
        log_line("garage leave icon: cancelled stale native ESC request; gate=%d", result);
        return result;
    }
    g_garage_results_pending = 2;
    saved_ui_active = *(volatile uint8_t *)0x007CE254u;
    saved_ui_frame = *(volatile int32_t *)0x007CE250u;
    /* Exact vanilla ESC leaf at 004A115C: same receiver, state write, call.
     * No key injection, manual grid reconstruction or main-menu button call. */
    log_line("garage leave icon: dispatching vanilla ESC transition; gate=%d", result);
    *(volatile int32_t *)0x007C569Cu = 6;
    ((NativeEscapeFn)0x00404F30u)((void *)0x005EA43Cu);
    /* The garage already ran the in-car teardown. Keep GamePostMainLoop alive
     * while the native results grid finishes its two-update countdown. Returning
     * 6 here runs Game_Post -> DynMan_Post -> SeasonScoringReset first, leaving
     * an empty results page. Native ACCEPT sets UI result 3; the original
     * RestartGate then returns 6 and owns the actual session exit. */
    results_page = ((GaragePageNameFn)0x00497980u)(c.manager);
    if (results_page && strcmp(results_page, "POST RACE MONITOR") == 0) {
        /* SystemOSAPICall inside ESC clears UI-active through GlobalStateSetter.
         * Continue the already-running garage UI, including its frame counter:
         * resetting the counter would rerun first-frame session initialization. */
        *(volatile uint8_t *)0x007CE254u = saved_ui_active;
        *(volatile int32_t *)0x007CE250u = saved_ui_frame;
        *(volatile int32_t *)0x007C569Cu = 5;
        log_line("garage leave icon: retaining results UI active=%u frame=%ld; ACCEPT owns exit",
            (unsigned)saved_ui_active, (long)saved_ui_frame);
    }
    log_line("garage leave icon: native ESC returned page=%s pending=%d",
        results_page ? results_page : "(none)", g_garage_results_pending);
    return *(volatile int32_t *)0x007C569Cu;
}

static const uint8_t garage_esc_leaf_bytes[] = {
    0xb9,0x3c,0xa4,0x5e,0x00,0xc7,0x05,0x9c,0x56,0x7c,0x00,
    0x06,0x00,0x00,0x00,0xe8,0xc0,0x3d,0xf6,0xff
};
static const uint8_t garage_esc_transition_bytes[] = {
    0x56,0x8b,0xf1,0xb9,0x4c,0xe2,0x7c,0x00,0xe8,0x13,0xb3,0x09,0x00
};

static int broker_garage_native_preflight(void)
{
    return g_results_supported && broker_garage_native_layout_valid() &&
        memcmp((void *)0x004A115Cu, garage_esc_leaf_bytes, sizeof garage_esc_leaf_bytes) == 0 &&
        memcmp((void *)0x00404F30u, garage_esc_transition_bytes, sizeof garage_esc_transition_bytes) == 0;
}

static void broker_garage_release_install(void)
{
    F1CDetourResult result;
    if (!broker_garage_native_preflight()) {
        log_line("garage leave icon hook refused: results dependency or reviewed code mismatch");
        return;
    }
    /* One complete MOV ECX,[ECX+E4]; no relative operands in stolen span. */
    result = f1c_detour_install_prevalidated_layout(&g_garage_release_detour,
        GARAGE_RELEASE_ADDRESS, broker_garage_release_hook, garage_release_bytes, 6u);
    if (result != F1C_DETOUR_OK) {
        log_line("garage leave icon hook refused: detour=%d", (int)result);
        return;
    }
    g_garage_release_original = (GarageReleaseFn)g_garage_release_detour.trampoline;
    g_garage_release_supported = 1;
    log_line("garage leave icon hook enabled: reviewed code layout, native ESC results with retained UI and ACCEPT-owned teardown");
}
