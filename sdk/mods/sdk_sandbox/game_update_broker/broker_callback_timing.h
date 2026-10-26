#ifndef BROKER_CALLBACK_TIMING_H
#define BROKER_CALLBACK_TIMING_H
/* Ordinary <=20ms is diagnostic-only. Ten consecutive ordinary callbacks
 * over20ms indicate sustained trouble; any over100ms is severe.
 * Explicit native budget stays unchanged. Native work cannot erase streaks.
 * This is after-return containment, not a preemptive hang watchdog. */
static int broker_callback_quarantine(uint64_t elapsed, uint32_t budget,
                                      unsigned *small_overrun) {
    if (budget != F1C_BROKER_SLOW_CALLBACK_US) return elapsed > budget;
    if (elapsed <= 20000ull) { *small_overrun=0; return 0; }
    if (elapsed > 100000ull) return 1;
    if (*small_overrun < 10u) ++*small_overrun;
    return *small_overrun >= 10u;
}
#endif
