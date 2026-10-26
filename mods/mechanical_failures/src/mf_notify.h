/* mf_notify.h - N2 (2026-10-06, DeepSeek N run): pure notification trigger
 * expressions for the engine family. No windows.h, no game addresses: every
 * function takes MfOut/MfEngine/MfInputs fields or plain values and returns
 * whether the notification's condition holds.
 *
 * The DLL context gates (pause/g_pump_frozen, live_drive, wrecked_or_stopped,
 * SESSION_OVER), the detection delay windows and the blink/reminder state stay
 * in engine_failure.c - this header owns only the condition that decides WHEN
 * a line is scheduled. tools/notify_test.c covers every function here: a
 * healthy car (all 0), a just-armed car with normal values (all 0 unless the
 * observable is already over threshold), a threshold-1 / threshold+1 pair per
 * function, failures-off cases, and the nine named history regression cases.
 *
 * Thresholds moved here from engine_failure.c so the header is self-contained
 * for the offline test (the values are unchanged, section B of the audit
 * prompt). */
#ifndef MF_NOTIFY_H
#define MF_NOTIFY_H

#include <string.h>
#include "mf_engine_model.h"

#define MFN_FN static __attribute__((unused))

/* moved from engine_failure.c (2026-10-06): notification thresholds only,
 * no model maths. Values unchanged. */
#define MF_MSG_BEARING_CRIT    0.30   /* h_b: bearings nearly gone */
#define MF_MSG_OIL_CRIT_FRAC   0.25   /* sump fraction: oil nearly gone */
#define MF_MSG_GASKET_CRIT     0.35   /* h_g: head gasket nearly gone */
#define MF_MSG_VALVE_CRIT      0.30   /* h_vt: valvetrain nearly gone */
#define MF_MSG_RING_CRIT       0.30   /* ring_s: rings nearly gone (stop at 0.15) */
#define MF_EXH_FLAG_POPS       3      /* spits before the marshals show the flag */
#define FUEL_LEAK_WARN_L       0.1    /* L: FUEL LEAK diagnosis (L2 review) */
#define OIL_WARN_SEVERITY      0.25f  /* blue smoke clearly visible to marshals */

/* ---- pump fault gate (was inlined in every pump) ------------------------- */
MFN_FN int mfn_active_fault(const MfEngine *e, int model_live, int fault)
{
    return model_live && e->onset && !e->culminated && !e->blown
        && !e->stopped && e->fault == fault;
}

/* ---- observables (R1) ---------------------------------------------------- */
MFN_FN int mfn_oil_gauge_low(const MfOut *o) { return o->oil_gauge_scale < 0.60; }
MFN_FN int mfn_oil_temp_warn(const MfOut *o) { return o->warn_cool_oil; }
MFN_FN int mfn_valve_air_low(const MfOut *o) { return o->warn_valve_air_low; }
MFN_FN int mfn_valve_float(const MfOut *o, int eng_running)
{ return o->warn_valve_float && eng_running; }
MFN_FN int mfn_shift_earlier(const MfOut *o) { return o->warn_shift_earlier; }
MFN_FN int mfn_water_temp(const MfOut *o) { return o->warn_water_temp; }
MFN_FN int mfn_cool_engine(const MfOut *o) { return o->warn_cool_engine; }
MFN_FN int mfn_coolant_low(const MfOut *o) { return o->warn_coolant_low; }
MFN_FN int mfn_rad_blocked(const MfOut *o, int cool_kind)
{ return cool_kind == 1 && o->warn_water_temp; }
MFN_FN int mfn_steam_bank(const MfOut *o) { return o->smoke_bank1 < 0.0; }

/* ---- instruction conditions (diagnosis shown / other line not asked) ----- */
MFN_FN int mfn_oil_cool_after_diag(const MfOut *o, int oilt_warn_on)
{ return o->warn_cool_oil && oilt_warn_on; }
MFN_FN int mfn_oil_cool_unasked(const MfOut *o, int oilp_instr_on)
{ return o->warn_cool_oil && !oilp_instr_on; }
MFN_FN int mfn_valve_shift_instr(int warn_on, double rev_limit, double shift_val)
{ return warn_on && rev_limit > shift_val; }
MFN_FN int mfn_shift_earlier_instr(const MfOut *o, int warn_on)
{ return o->warn_shift_earlier && warn_on; }
MFN_FN int mfn_water_instr(const MfOut *o, int warn_on)
{ return o->warn_cool_engine && warn_on; }
MFN_FN int mfn_blowby_instr(const MfEngine *e, int warn_on)
{ return (1.0 - e->ring_s) > 0.3 && warn_on; }
MFN_FN int mfn_fuel_instr(const MfEngine *e, int warn_on)
{ return e->fuel_cap < MF_FP_CUT_FULL && warn_on; }
MFN_FN int mfn_cooling_pre(int cool_kind, int warn_on)
{ return cool_kind == 2 && warn_on; }
MFN_FN int mfn_steam_shown(int cool_kind, int steam_on)
{ return cool_kind == 2 && steam_on; }
MFN_FN int mfn_coolant_lost_instr(const MfEngine *e, int cool_kind)
{ return cool_kind == 2 && e->v_w_l <= MF_WATER_START_L - 1.0; }

/* ---- internals allowed by section B / R2 criticals ----------------------- */
MFN_FN double mfn_blowby_k(const MfEngine *e) { return 1.0 - e->ring_s; }
MFN_FN int mfn_blowby_warn(const MfEngine *e) { return (1.0 - e->ring_s) > 0.3; }
/* N3 2026-10-06 (R1 blow-by): the diagnosis must be anchored on the visible
 * blow-by smoke (R2 visible effect), held for >= 5 s game time, ON TOP of the
 * internal k > 0.3. smoke_blue is the model's blue-smoke density. */
MFN_FN int mfn_blowby_visible(const MfOut *o) { return o->smoke_blue > 0.02; }
MFN_FN int mfn_for_ms(unsigned now, unsigned since, unsigned need)
{ return since != 0 && (int)(now - (since + need)) >= 0; }
MFN_FN int mfn_blowby_pre(const MfEngine *e, const MfOut *o,
                          int warn_on, int eng_running)
{ (void)e; return warn_on && o->smoke_blue > 0.02 && eng_running; }
MFN_FN int mfn_valve_crit(const MfEngine *e) { return e->h_vt < MF_MSG_VALVE_CRIT; }
MFN_FN int mfn_bearing_crit(const MfEngine *e) { return e->h_b < MF_MSG_BEARING_CRIT; }
MFN_FN int mfn_ring_crit(const MfEngine *e) { return e->ring_s < MF_MSG_RING_CRIT; }
MFN_FN int mfn_oil_crit(const MfEngine *e)
{
    return e->v_oil_l < MF_MSG_OIL_CRIT_FRAC * MF_OIL_START_L
        || e->h_b < MF_MSG_BEARING_CRIT;
}
MFN_FN int mfn_water_crit(const MfEngine *e, int cool_kind)
{
    return cool_kind == 2
        && (e->v_w_l < MF_WATER_SENSOR_FRAC * MF_WATER_START_L
            || e->h_g < MF_MSG_GASKET_CRIT);
}
MFN_FN int mfn_fuel_starve(const MfEngine *e) { return e->fp_floor_s > 0.0; }
MFN_FN int mfn_fuel_cap_stumble(const MfEngine *e) { return e->fuel_cap < MF_FP_STUMBLE; }
MFN_FN int mfn_fuel_leak_warn(const MfEngine *e)
{ return e->fuel_lost_l >= FUEL_LEAK_WARN_L; }
MFN_FN int mfn_debris_blown(const MfEngine *e) { return e->f_rad >= 0.999; }
MFN_FN int mfn_reason_gasket(const char *reason)
{ return reason && strstr(reason, "gasket") != 0; }

/* ---- plain values / radio fallbacks -------------------------------------- */
MFN_FN int mfn_exh_diag(int pops) { return pops >= 1; }
MFN_FN int mfn_exh_flag(int pops) { return pops >= MF_EXH_FLAG_POPS; }
MFN_FN int mfn_oil_smoke(double loss, int eng_running)
{ return loss >= (double)OIL_WARN_SEVERITY && eng_running; }
MFN_FN int mfn_oil77(double oil_k, double limit_k)
{ return limit_k > 300.0 && oil_k > limit_k; }
MFN_FN int mfn_water77(double water_k, double base_k)
{
    /* N3 2026-10-06 (N47): the vanilla-eid fallback keeps the model's absolute
     * floor too (water >= 110 C, MF_WATER_COOL_MIN_K) - was +18 K alone. */
    return water_k > base_k + MF_WATER_COOL_DK
        && water_k >= MF_WATER_COOL_MIN_K;
}
MFN_FN int mfn_overrev(double rpm, double limit, int eng_running, int failrate_on)
{
    return eng_running && failrate_on && limit >= 1.0
        && (float)(rpm / limit) > 1.04f;
}
MFN_FN int mfn_overrev_hurt(double hv_prev, double hv_now,
                            double hr_prev, double hr_now)
{ return hv_prev - hv_now > 0.002 || hr_prev - hr_now > 0.002; }
MFN_FN int mfn_fuel_save(int advice) { return advice == 1; }
MFN_FN int mfn_fuel_pit(int advice) { return advice == 2; }
/* N3 2026-10-06 (item 8, R6 ordering): a critical never fires before the
 * onset's diagnosis was shown/said; if the critical condition is already true
 * the caller shows the diagnosis first, then the critical >= 5 s later. */
MFN_FN int mfn_crit_needs_diag(int crit, int diagnosis_shown)
{ return crit && !diagnosis_shown; }
MFN_FN int mfn_crit_allowed(int crit, int diagnosis_shown,
                            unsigned now, unsigned diag_at, unsigned gap)
{
    return crit && diagnosis_shown && diag_at != 0u
        && (int)(now - (diag_at + gap)) >= 0;
}
/* N3 2026-10-06 (items 3+4): a clear/repair line is allowed only when some
 * diagnosis of the current onset was shown/said (HUD warn blink or the
 * fault's pre line). */
MFN_FN int mfn_diag_shown(int pre_line_said, int warn_blink_mask)
{ return pre_line_said || warn_blink_mask != 0; }

/* N6 2026-10-06 (owner-approved): the reason a mod fault is cleared by a stint
 * transition. 0 = keep, 1 = session left (RACE_PHASE <= 4), 2 = race
 * retirement. The pit lane / PIT_STATUS is deliberately NOT an input: a pit
 * stop or a pit-lane drive-through never clears a fault (only the panel
 * REPAIR, leaving the session, a race retirement, and the game re-seeding the
 * clock - detected in model_pump - do). */
MFN_FN int mfn_stint_clear_reason(int race_phase, int session_type,
                                  int finish_state)
{
    if (race_phase <= 4) return 1;
    if (session_type == 7 && finish_state >= 2) return 2;
    return 0;
}
MFN_FN int mfn_onset_unmapped(int fault)
{ return fault < MF_FAULT_VALVE_AIR || fault >= MF_FAULT_COUNT; }
MFN_FN int mfn_impact_armed(const MfOut *o)
{ return o->impact_fired && o->impact_fault > 0; }
MFN_FN int mfn_impact_only(const MfOut *o)
{ return o->impact_fired && o->impact_fault <= 0; }

#endif /* MF_NOTIFY_H */
