/* Native UI release policy. Shared by the broker and x86 ABI fixture.
 * Evidence: UI_LEAVE_RESULTS_20260923.md. No keyboard polling or state spoofing. */
#pragma once
#include <string.h>

typedef struct BrokerGarageResultsContext {
    int button_kind, session_type, race_mode, race_phase, finish_state;
    const char *page;
    void *manager;
} BrokerGarageResultsContext;

typedef struct BrokerGarageResultsOps {
    void *(__attribute__((thiscall)) *find_page)(void *, const char *);
    void *(__attribute__((thiscall)) *get_grid)(void *, int);
    int (*grid_valid)(void *, void *);
    int (__attribute__((thiscall)) *build_grid)(void *, int);
    void (__attribute__((thiscall)) *arm_grid)(void *);
    int (__attribute__((thiscall)) *show_page)(void *, const char *, int);
} BrokerGarageResultsOps;

enum { BROKER_GARAGE_PASS = 0, BROKER_GARAGE_HANDLED = 1,
       BROKER_GARAGE_REFUSED = 2, BROKER_GARAGE_DUPLICATE = 3 };

static int broker_garage_results_eligible(const BrokerGarageResultsContext *c)
{
    return c && c->manager && c->button_kind == 56 &&
        c->session_type == 7 && c->race_mode >= 1 && c->race_mode <= 4 &&
        c->race_phase > 4 && (c->finish_state == 2 || c->finish_state == 3) &&
        c->page && (strcmp(c->page, "MONITORPAGE") == 0 ||
                    strcmp(c->page, "MONITOR MAXIMIZED") == 0);
}

/* PASS means execute the original native handler. Every other result consumes
 * the release (-1), including missing-resource refusal, to preserve the race.
 * Exactly mirrors sub_4A0250's race-grid block, not its already-run teardown. */
static inline int broker_garage_results_click(const BrokerGarageResultsContext *c,
                                      const BrokerGarageResultsOps *ops,
                                      int *pending)
{
    void *grid;
    if (!broker_garage_results_eligible(c)) return BROKER_GARAGE_PASS;
    if (*pending) return BROKER_GARAGE_DUPLICATE;
    if (!ops->find_page(c->manager, "POST RACE MONITOR"))
        return BROKER_GARAGE_REFUSED;
    grid = ops->get_grid(c->manager, 33);
    if (!grid || !ops->grid_valid(grid, c->manager)) return BROKER_GARAGE_REFUSED;
    *pending = 1;
    ops->build_grid(grid, 0);
    ops->arm_grid(grid);
    if (!ops->show_page(c->manager, "POST RACE MONITOR", -1)) {
        /* Keep pending latched: a repeated click must not arm another finish.
         * The original destructive leave action must not be used as fallback. */
        return BROKER_GARAGE_REFUSED;
    }
    return BROKER_GARAGE_HANDLED;
}
