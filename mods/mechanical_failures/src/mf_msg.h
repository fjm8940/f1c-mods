/* mf_msg.h - message arbiter for the engine family's HUD line and radio
 * calls (Claude, 2026-10-01, docs/MESSAGING_AUDIT_2026-10-01.md).
 *
 * Pure logic, no game memory: the DLL feeds it the sim clock (ms) and draws
 * whatever mfm_current() returns; tools/msg_test.c tests it offline.
 *
 * HUD (one mod line at a time):
 *   tier 0 diagnosis (white), 1 instruction (yellow), 2 critical (red),
 *   3 end (ENGINE FAILURE / STOP THE CAR). A posted line shows for
 *   MFM_HOLD_MS; a higher tier preempts and the preempted line is queued
 *   again; an equal/lower tier waits in the queue instead of overwriting
 *   (R3c rising edges are no longer lost). Queued lines older than
 *   MFM_STALE_MS are dropped (old news). A LIVE line (e.g. SHIFT NOW) shows
 *   only while its condition holds and ends MFM_LIVE_OFF_MS after it goes
 *   false - no 4 s hold, no fade (owner 2026-10-01).
 * Radio (the game's commentary queue has one head entry):
 *   one request queue, at least MFR_GAP_MS between two mod calls (death
 *   calls MFR_GAP_DEATH_MS), a death request drops every pending
 *   lower-priority call, requests older than MFR_STALE_MS are dropped. */
#ifndef MF_MSG_H
#define MF_MSG_H

#include <stdint.h>
#include <string.h>

#define MFM_FN static __attribute__((unused))
#define MFM_TEXT_MAX     31
#define MFM_QMAX         8
#define MFM_HOLD_MS      4000u
#define MFM_STALE_MS     10000u
#define MFM_LIVE_OFF_MS  150u

#define MFM_T_DIAG  0
#define MFM_T_INSTR 1
#define MFM_T_CRIT  2
#define MFM_T_END   3

/* Tier from the text. Keep in step with mfh_colour (mf_hud.h). */
MFM_FN int mfm_tier(const char *t)
{
    static const char *const end[] = { "ENGINE FAILURE", "STOP THE CAR", NULL };
    static const char *const crit[] = { "SHIFT NOW", "ENGINE CRITICAL",
        "PIT AND RETIRE", "FUEL STARVATION", NULL };
    static const char *const instr[] = { "SHIFT EARLIER", "SHORT SHIFT",
        "COOL THE OIL", "COOL THE ENGINE", "BOX - DEBRIS", "EASE OFF",
        "SAVE FUEL", "BOX FOR FUEL", "WATCH THE REAR", NULL };
    int i;
    for (i = 0; end[i]; i++)   if (!strcmp(t, end[i]))   return MFM_T_END;
    for (i = 0; crit[i]; i++)  if (!strcmp(t, crit[i]))  return MFM_T_CRIT;
    for (i = 0; instr[i]; i++) if (!strcmp(t, instr[i])) return MFM_T_INSTR;
    return MFM_T_DIAG;
}

typedef struct {
    char     text[MFM_TEXT_MAX + 1];
    int      tier;
    int      live;
    uint32_t posted;     /* sim ms when (first) posted */
    uint32_t until;      /* sim ms the line ends (shown lines) */
} MfMsgItem;

typedef struct {
    int       has;
    MfMsgItem cur;
    int       n;
    MfMsgItem q[MFM_QMAX];
} MfMsgArb;

MFM_FN void mfm_clear(MfMsgArb *a) { memset(a, 0, sizeof *a); }

static inline int mfm_due(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

MFM_FN int mfm_find_q(const MfMsgArb *a, const char *t)
{
    int i;
    for (i = 0; i < a->n; i++) if (!strcmp(a->q[i].text, t)) return i;
    return -1;
}

MFM_FN void mfm_qdel(MfMsgArb *a, int i)
{
    for (; i + 1 < a->n; i++) a->q[i] = a->q[i + 1];
    a->n--;
}

/* queue an item (dedupe; full -> drop the lowest tier, oldest first) */
MFM_FN void mfm_qput(MfMsgArb *a, const MfMsgItem *it)
{
    int i, low = -1;
    if (mfm_find_q(a, it->text) >= 0) return;
    if (a->n >= MFM_QMAX) {
        for (i = 0; i < a->n; i++)
            if (low < 0 || a->q[i].tier < a->q[low].tier) low = i;
        if (a->q[low].tier > it->tier) return;   /* everything queued matters more */
        mfm_qdel(a, low);
    }
    a->q[a->n++] = *it;
}

MFM_FN void mfm_show(MfMsgArb *a, const MfMsgItem *it, uint32_t now)
{
    if (a->has && strcmp(a->cur.text, it->text) != 0) {
        MfMsgItem old = a->cur;
        /* a preempted line comes back later; a live one re-posts itself */
        if (!old.live && !mfm_due(now, old.until)) mfm_qput(a, &old);
    }
    a->cur = *it;
    a->has = 1;
    a->cur.until = now + (it->live ? MFM_LIVE_OFF_MS : MFM_HOLD_MS);
}

MFM_FN void mfm_offer(MfMsgArb *a, const char *text, int live, uint32_t now)
{
    MfMsgItem it;
    int qi;
    if (!text || !text[0]) return;
    memset(&it, 0, sizeof it);
    strncpy(it.text, text, MFM_TEXT_MAX);
    it.tier = mfm_tier(it.text);
    it.live = live;
    it.posted = now;
    if (a->has && !strcmp(a->cur.text, it.text)) {
        /* same line again: refresh its hold */
        a->cur.until = now + (live ? MFM_LIVE_OFF_MS : MFM_HOLD_MS);
        return;
    }
    qi = mfm_find_q(a, it.text);
    if (qi >= 0) {                      /* already waiting: keep its age */
        if (live) a->q[qi].posted = now;
        return;
    }
    if (!a->has || it.tier > a->cur.tier) mfm_show(a, &it, now);
    else mfm_qput(a, &it);
}

/* a one-shot line (diagnosis, instruction, death...) */
MFM_FN void mfm_post(MfMsgArb *a, const char *text, uint32_t now)
{
    mfm_offer(a, text, 0, now);
}

/* a live line: call every pass with its condition */
MFM_FN void mfm_live(MfMsgArb *a, const char *text, int cond, uint32_t now)
{
    int qi;
    if (cond) { mfm_offer(a, text, 1, now); return; }
    qi = mfm_find_q(a, text);
    if (qi >= 0) mfm_qdel(a, qi);
    if (a->has && a->cur.live && !strcmp(a->cur.text, text)) a->has = 0;
}

/* every pass: end the shown line, then pick the next (highest tier, oldest) */
MFM_FN void mfm_tick(MfMsgArb *a, uint32_t now)
{
    int i, best;
    if (a->has && mfm_due(now, a->cur.until)) a->has = 0;
    for (i = 0; i < a->n; ) {
        uint32_t stale = a->q[i].live ? MFM_LIVE_OFF_MS : MFM_STALE_MS;
        if (mfm_due(now, a->q[i].posted + stale)) mfm_qdel(a, i);
        else i++;
    }
    if (a->has || !a->n) return;
    best = 0;
    for (i = 1; i < a->n; i++) if (a->q[i].tier > a->q[best].tier) best = i;
    {
        MfMsgItem it = a->q[best];
        mfm_qdel(a, best);
        mfm_show(a, &it, now);
    }
}

MFM_FN const char *mfm_current(const MfMsgArb *a) { return a->has ? a->cur.text : 0; }

/* ---- radio --------------------------------------------------------------- */
#define MFR_QMAX          4
#define MFR_GAP_MS        15000u
#define MFR_GAP_DEATH_MS  5000u
#define MFR_STALE_MS      30000u
#define MFR_P_INFO   0      /* marshals / engineer notice before critical */
#define MFR_P_CRIT   1      /* the critical pit / retire call */
#define MFR_P_DEATH  2      /* the engine is gone */

typedef struct { int eid; int prio; uint32_t t; } MfRadioReq;
typedef struct {
    int        terminal;
    int        n;
    MfRadioReq q[MFR_QMAX];
    int        said_any;
    uint32_t   last;
} MfRadioQ;

MFM_FN void mfr_clear(MfRadioQ *r) { memset(r, 0, sizeof *r); }

MFM_FN void mfr_del(MfRadioQ *r, int i)
{
    for (; i + 1 < r->n; i++) r->q[i] = r->q[i + 1];
    r->n--;
}

MFM_FN void mfr_terminal(MfRadioQ *r)
{
    int i;
    r->terminal = 1;
    for (i = 0; i < r->n; ) {
        if (r->q[i].prio < MFR_P_DEATH) mfr_del(r, i); else ++i;
    }
}

MFM_FN void mfr_req(MfRadioQ *r, int eid, int prio, uint32_t now)
{
    int i;
    if (r->terminal && prio < MFR_P_DEATH) return;
    if (prio >= MFR_P_DEATH)                     /* death: old news goes */
        for (i = 0; i < r->n; ) { if (r->q[i].prio < prio) mfr_del(r, i); else i++; }
    for (i = 0; i < r->n; i++) if (r->q[i].eid == eid) return;
    if (r->n >= MFR_QMAX) {
        int low = 0;
        for (i = 1; i < r->n; i++) if (r->q[i].prio < r->q[low].prio) low = i;
        if (r->q[low].prio > prio) return;
        mfr_del(r, low);
    }
    r->q[r->n].eid = eid;
    r->q[r->n].prio = prio;
    r->q[r->n].t = now;
    r->n++;
}

/* every pass: the eid to say now, or -1. `quiet` = the car is wrecked or
 * stopped: only death calls may still go out. */
MFM_FN int mfr_tick(MfRadioQ *r, uint32_t now, int quiet)
{
    int i, best = -1;
    for (i = 0; i < r->n; ) {
        if (mfm_due(now, r->q[i].t + MFR_STALE_MS)
            || ((quiet || r->terminal) && r->q[i].prio < MFR_P_DEATH)) mfr_del(r, i);
        else i++;
    }
    for (i = 0; i < r->n; i++)
        if (best < 0 || r->q[i].prio > r->q[best].prio) best = i;
    if (best < 0) return -1;
    if (r->said_any && !mfm_due(now, r->last + (r->q[best].prio >= MFR_P_DEATH
                                                ? MFR_GAP_DEATH_MS : MFR_GAP_MS)))
        return -1;
    {
        int eid = r->q[best].eid;
        mfr_del(r, best);
        r->said_any = 1;
        r->last = now;
        return eid;
    }
}

/* ---- V3 instruction sub-slot --------------------------------------------
 * The 2nd line of a two-line call is scheduled after the 1st went out (a
 * delay rolled in [lo, hi]) and does NOT consume another pre-critical slot.
 * The caller owns one slot per line id; mfi_tick returns the eid once. */
typedef struct { int armed; int done; uint32_t due; int eid; } MfInstrSlot;

MFM_FN void mfi_clear(MfInstrSlot *s) { memset(s, 0, sizeof *s); }

MFM_FN void mfi_arm(MfInstrSlot *s, int eid, uint32_t rnd, uint32_t now,
                    uint32_t lo, uint32_t hi)
{
    if (lo < 1u) lo = 1u;
    if (hi < lo) hi = lo;
    s->armed = 1;
    s->done = 0;
    s->eid = eid;
    s->due = now + lo + (rnd % (hi - lo + 1u));
}

MFM_FN int mfi_tick(MfInstrSlot *s, uint32_t now)
{
    if (!s->armed || s->done) return -1;
    if (!mfm_due(now, s->due)) return -1;
    s->done = 1;
    return s->eid;
}

/* N3 2026-10-06 (R7): eligibility is evaluated at FIRE time. False advice is
 * HELD until eligible (max MFI_HOLD_MAX_MS after due) while the caller keeps
 * the same fault+onset, then dropped with MFI_DROPPED so the caller can log
 * "VOICE: <id> dropped (<reason>)". Was: silent clear on the first
 * ineligible pass. */
#define MFI_HOLD_MAX_MS 120000u
#define MFI_DROPPED    (-2)

MFM_FN int mfi_due(const MfInstrSlot *s, uint32_t now)
{
    return s->armed && !s->done && mfm_due(now, s->due);
}

MFM_FN int mfi_tick_if(MfInstrSlot *s, uint32_t now, int eligible)
{
    if (!s->armed || s->done) return -1;
    if (!mfm_due(now, s->due)) return -1;
    if (eligible) return mfi_tick(s, now);
    if (mfm_due(now, s->due + MFI_HOLD_MAX_MS)) {
        mfi_clear(s);
        return MFI_DROPPED;
    }
    return -1;
}

/* ---- V3 rate limit (OVERREV01: max once per 120 s) ---------------------- */
MFM_FN int mfm_rate_ok(uint32_t *last, uint32_t now, uint32_t gap)
{
    if (*last && !mfm_due(now, *last + gap)) return 0;
    *last = now;
    return 1;
}

/* N3 2026-10-06 (item 5): rate limit per line that treats a different fault
 * TYPE as a new line ("except a genuinely new fault TYPE"). 1 = allowed, and
 * `last`/`last_fault` are updated. */
MFM_FN int mfm_rate_ok_fault(uint32_t *last, int *last_fault, int fault,
                             uint32_t now, uint32_t gap)
{
    if (*last && *last_fault == fault && !mfm_due(now, *last + gap)) return 0;
    *last = now;
    *last_fault = fault;
    return 1;
}

#endif
