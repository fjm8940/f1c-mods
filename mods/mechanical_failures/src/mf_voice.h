/* mf_voice.h - V3 engineer voice-line mapping (pure; Claude spec 2026-10-05).
 *
 * One entry per line of the V3 table: line id (V3 table order), the WAV/SDT
 * stem, the DEAD vanilla eid it borrows (V0 donor list: no native emitter,
 * none used by any first-party mod, all scores < 300) and the owner's text.
 *
 * The runtime side (engine_failure.c) writes word_5BEBB0[score(eid)] = 316+i
 * at load when the installed SDT carries the 35 appended clips, restores the
 * original lookup values on unload, and fires the donor eid through the
 * normal commentary queue. This header owns only the pure mapping, the
 * instruction pairing (2nd line 30-60 s after the 1st, same pre-critical
 * slot) and the collision check used by tools/msg_test.c.
 *
 * Donor eids (V0 section V0.3, first 35 of the selected 40):
 *   OILP01 3, OILP02 8, OILP03 9, OILLEAK01 10, OILLEAK02 16, OILT01 23,
 *   BEAR01 24, WATER01 25, WATER02 26, COOLANT01 27, COOLANT02 28, RAD01 29,
 *   RAD02 30, RAD03 31, GASKET01 32, VALVE01 33, VALVE02 36, OVERREV01 37,
 *   OVERREV02 38, EXH01 39, EXH02 40, BLOWBY01 46, ENG01 47, FUELP01 48,
 *   FUELP02 78, FUELLEAK01 79, FUELLEAK02 58, CRIT01 59, SAVE01 60,
 *   DEAD01 102, DEAD02 103, REPAIR01 112, CLEAR01 113, HIT01 71, HIT02 72.
 */
#ifndef MF_VOICE_H
#define MF_VOICE_H

#include <stdint.h>
#include <string.h>

#define MFV_COUNT 35
#define MFV_SDT_BASE 316              /* first appended clip index           */
#define MFV_SDT_NAME0 "PWS_MF01"      /* installed-SDT detection             */

/* V3 table order */
enum {
    MFV_OILP01 = 0, MFV_OILP02, MFV_OILP03,
    MFV_OILLEAK01, MFV_OILLEAK02, MFV_OILT01, MFV_BEAR01,
    MFV_WATER01, MFV_WATER02, MFV_COOLANT01, MFV_COOLANT02,
    MFV_RAD01, MFV_RAD02, MFV_RAD03, MFV_GASKET01,
    MFV_VALVE01, MFV_VALVE02, MFV_OVERREV01, MFV_OVERREV02,
    MFV_EXH01, MFV_EXH02, MFV_BLOWBY01, MFV_ENG01,
    MFV_FUELP01, MFV_FUELP02, MFV_FUELLEAK01, MFV_FUELLEAK02,
    MFV_CRIT01, MFV_SAVE01, MFV_DEAD01, MFV_DEAD02,
    MFV_REPAIR01, MFV_CLEAR01, MFV_HIT01, MFV_HIT02
};

typedef struct {
    int         line;
    const char *id;      /* MANIFEST / mf_voice id                      */
    int         eid;     /* donor eid (V0)                              */
    const char *text;    /* owner's script text (MANIFEST.csv)          */
} MfVoiceLine;

#define MFV_FN static __attribute__((unused))

static const MfVoiceLine MFV_LINES[MFV_COUNT] = {
    { MFV_OILP01,      "MF_OILP01",      3,  "Oil pressure dropping." },
    { MFV_OILP02,      "MF_OILP02",      8,  "Oil pressure low, short shift." },
    { MFV_OILP03,      "MF_OILP03",      9,  "Low oil pressure, keep the revs down." },
    { MFV_OILLEAK01,   "MF_OILLEAK01",  10,  "You're leaking oil." },
    { MFV_OILLEAK02,   "MF_OILLEAK02",  16,  "Oil leak, take it easy." },
    { MFV_OILT01,      "MF_OILT01",     23,  "Oil temperature high, back off." },
    { MFV_BEAR01,      "MF_BEAR01",     24,  "Bearings are going, box now." },
    { MFV_WATER01,     "MF_WATER01",    25,  "Water temperature rising." },
    { MFV_WATER02,     "MF_WATER02",    26,  "Water temp high, lift and coast." },
    { MFV_COOLANT01,   "MF_COOLANT01",  27,  "You're losing coolant." },
    { MFV_COOLANT02,   "MF_COOLANT02",  28,  "Coolant low, box this lap." },
    { MFV_RAD01,       "MF_RAD01",      29,  "Debris in the radiator." },
    { MFV_RAD02,       "MF_RAD02",      30,  "Radiator blocked, find clean air." },
    { MFV_RAD03,       "MF_RAD03",      31,  "Radiator's clear now." },
    { MFV_GASKET01,    "MF_GASKET01",   32,  "Head gasket's gone." },
    { MFV_VALVE01,     "MF_VALVE01",    33,  "Losing valve pressure." },
    { MFV_VALVE02,     "MF_VALVE02",    36,  "Valves are floating, short shift." },
    { MFV_OVERREV01,   "MF_OVERREV01",  37,  "Over-rev, watch the downshifts." },
    { MFV_OVERREV02,   "MF_OVERREV02",  38,  "That over-rev hurt the engine." },
    { MFV_EXH01,       "MF_EXH01",      39,  "Exhaust is cracked." },
    { MFV_EXH02,       "MF_EXH02",      40,  "Exhaust problem, keep pushing." },
    { MFV_BLOWBY01,    "MF_BLOWBY01",   46,  "Engine's losing compression." },
    { MFV_ENG01,       "MF_ENG01",      47,  "We're seeing an engine problem." },
    { MFV_FUELP01,     "MF_FUELP01",    48,  "Fuel pressure dropping." },
    { MFV_FUELP02,     "MF_FUELP02",    78,  "Fuel pressure low, keep it smooth." },
    { MFV_FUELLEAK01,  "MF_FUELLEAK01", 79,  "You've got a fuel leak." },
    { MFV_FUELLEAK02,  "MF_FUELLEAK02", 58,  "Fuel leak, box now." },
    { MFV_CRIT01,      "MF_CRIT01",     59,  "Engine critical, box now." },
    { MFV_SAVE01,      "MF_SAVE01",     60,  "Save the engine." },
    { MFV_DEAD01,      "MF_DEAD01",     102,  "Engine's gone, stop the car." },
    { MFV_DEAD02,      "MF_DEAD02",     103,  "Engine's failed, park it safely." },
    { MFV_REPAIR01,    "MF_REPAIR01",   112,  "Engine fixed, go." },
    { MFV_CLEAR01,     "MF_CLEAR01",    113,  "Problem's cleared, push now." },
    { MFV_HIT01,       "MF_HIT01",      71,  "Big hit, checking the data." },
    { MFV_HIT02,       "MF_HIT02",      72,  "Damage after that hit, watch the temps." },
};

/* The 2nd line of a two-line row, fired 30-60 s after the 1st and counted as
 * the same pre-critical slot. OILLEAK01 goes 15-20 s after the marshals'
 * line (MECHWARN) and chains OILLEAK02 after itself.
 * N3 2026-10-06 (H15): EXH01 no longer pairs with EXH02 - the owner-approved
 * text "Exhaust problem, keep pushing." contradicts the model (higher revs
 * grow the crack faster), so the line is NOT said any more; SDT untouched,
 * a new recording is an owner decision. */
MFV_FN int mfv_instr_of(int line)
{
    switch (line) {
    case MFV_OILP01:    return MFV_OILP02;   /* 02/03 picked at random (alt) */
    case MFV_OILLEAK01: return MFV_OILLEAK02;
    case MFV_WATER01:   return MFV_WATER02;
    case MFV_RAD01:     return MFV_RAD02;
    case MFV_VALVE01:   return MFV_VALVE02;
    case MFV_FUELP01:   return MFV_FUELP02;
    default:            return -1;
    }
}

/* AUDITFIX: runtime diagnosis selection and narrated-condition guards. */
MFV_FN int mfv_water_diagnosis(int debris) { return debris ? MFV_RAD01 : MFV_WATER01; }
MFV_FN int mfv_instruction_eligible(int line, int active, int same_fault,
                                   int valve_float, int cool_engine, int oil_warning,
                                   int debris, int oil_hot)
{
    if (!active || !same_fault) return 0;
    switch (line) {
    case MFV_VALVE02: return valve_float;
    case MFV_WATER02: return cool_engine;
    case MFV_OILP02: case MFV_OILP03: return oil_warning;
    case MFV_RAD02: return debris;
    case MFV_OILT01: return oil_hot;
    default: return 1;
    }
}

/* alternate instruction line for rows that "pick one at random" */
MFV_FN int mfv_instr_alt_of(int line)
{
    return (line == MFV_OILP01) ? MFV_OILP03 : -1;
}

MFV_FN void mfv_instr_delay_ms(int line, uint32_t *lo, uint32_t *hi)
{
    if (line == MFV_OILLEAK01) { *lo = 15000u; *hi = 20000u; }
    else                       { *lo = 30000u; *hi = 60000u; }
}

/* vanilla eids this mod/its siblings use today - a donor must not collide */
MFV_FN int mfv_vanilla_in_use(int eid)
{
    if (eid == 50 || eid == 57 || eid == 62 || eid == 64 || eid == 67 || eid == 69) return 1;
    if (eid == 49 || eid == 75 || eid == 76 || eid == 77
        || eid == 119 || eid == 130)
        return 1;
    if (eid >= 80 && eid <= 93) return 1;   /* MECHPROB20-33 gearbox */
    return 0;
}

/* 1 = the mapping is sound: ids in order, eids unique, PWS range, no
 * collision with any vanilla eid in use. tools/msg_test.c gates on this. */
MFV_FN int mfv_check(void)
{
    int i, j;
    for (i = 0; i < MFV_COUNT; i++) {
        if (MFV_LINES[i].line != i) return 0;
        if (!MFV_LINES[i].id || !MFV_LINES[i].text) return 0;
        if (MFV_LINES[i].eid < 1 || MFV_LINES[i].eid > 290) return 0;
        if (mfv_vanilla_in_use(MFV_LINES[i].eid)) return 0;
        for (j = 0; j < i; j++)
            if (MFV_LINES[j].eid == MFV_LINES[i].eid) return 0;
    }
    return 1;
}

MFV_FN int mfv_eid(int line)
{
    return (line >= 0 && line < MFV_COUNT) ? MFV_LINES[line].eid : -1;
}

MFV_FN const char *mfv_text(int line)
{
    return (line >= 0 && line < MFV_COUNT) ? MFV_LINES[line].text : "";
}

MFV_FN const char *mfv_id(int line)
{
    return (line >= 0 && line < MFV_COUNT) ? MFV_LINES[line].id : "";
}

/* 2026-10-07: donor eid -> line (the eids are now only internal tokens of
 * the radio queue; the line plays from its loose WAV). -1 = not a line. */
MFV_FN int mfv_line_of_eid(int eid)
{
    int i;
    for (i = 0; i < MFV_COUNT; i++)
        if (MFV_LINES[i].eid == eid) return i;
    return -1;
}

/* ---- V3b (ADDENDUM 3): install/restore plan (pure) -----------------------
 * The event-table row is 5 dwords: +0 priority, +4 delay (float bits),
 * +8 f3 (float bits), +0xC cooldown (float bits), +0x10 score. The V3b
 * parity writes set the first four to eid 77's CURRENT runtime values (read
 * at load, not constants); the score is never touched. All donors are
 * validated BEFORE anything is written - a failed install writes nothing. */
typedef struct {
    int pri;
    int delay_bits;
    int f3_bits;
    int cd_bits;
    int score;
} MfVoiceRow;

typedef struct {
    MfVoiceRow     saved[MFV_COUNT];
    MfVoiceRow     wrote[MFV_COUNT];
    unsigned short lk_saved[MFV_COUNT];
    unsigned short lk_wrote[MFV_COUNT];
    int            installed;
} MfVoicePlan;

/* all-or-nothing validation: every donor score in 0..299 and every stitch
 * type >= 0x40. 1 = safe to install. */
MFV_FN int mfv_plan_validate(const int *scores, const unsigned *stitch)
{
    int i, j;
    for (i = 0; i < MFV_COUNT; i++) {
        for (j = 0; j < i; j++) if (scores[i] == scores[j]) return 0;
        if (scores[i] < 0 || scores[i] >= 300) return 0;
        if (stitch[i] < 0x40u) return 0;
    }
    return 1;
}

/* fill the plan: parity fields = the reference (eid 77) row, score kept.
 * Call ONLY after mfv_plan_validate() returned 1. */
MFV_FN void mfv_plan_fill(const MfVoiceRow *rows, const MfVoiceRow *ref,
                          MfVoicePlan *p)
{
    int i;
    for (i = 0; i < MFV_COUNT; i++) {
        p->saved[i] = rows[i];
        p->wrote[i] = *ref;
        p->wrote[i].score = rows[i].score;
    }
    p->installed = 1;
}

/* restore plan: out[i] = saved[i] while the current row still holds every
 * value we wrote; otherwise out[i] = the current row (left alone). Returns
 * the number of donors fully restored. */
MFV_FN int mfv_plan_restore(const MfVoiceRow *cur, const MfVoicePlan *p,
                            MfVoiceRow *out)
{
    int i, n = 0, ok;
    for (i = 0; i < MFV_COUNT; i++) {
        ok = 1;
        if (cur[i].score      != p->wrote[i].score)      ok = 0;
        if (cur[i].pri        != p->wrote[i].pri)        ok = 0;
        if (cur[i].delay_bits != p->wrote[i].delay_bits) ok = 0;
        if (cur[i].f3_bits    != p->wrote[i].f3_bits)    ok = 0;
        if (cur[i].cd_bits    != p->wrote[i].cd_bits)    ok = 0;
        if (ok) { out[i] = p->saved[i]; n++; }
        else    { out[i] = cur[i]; }
    }
    return n;
}

/* Runtime restore must own the whole row AND its saved-score lookup. */
MFV_FN int mfv_restore_owned(const MfVoiceRow *cur, const MfVoicePlan *p,
                             int i, unsigned short lookup)
{
    if (!p->installed || i < 0 || i >= MFV_COUNT) return 0;
    if (p->saved[i].score < 0 || p->saved[i].score >= 300) return 0;
    return cur->score == p->saved[i].score && cur->pri == p->wrote[i].pri
        && cur->delay_bits == p->wrote[i].delay_bits
        && cur->f3_bits == p->wrote[i].f3_bits
        && cur->cd_bits == p->wrote[i].cd_bits && lookup == p->lk_wrote[i];
}

/* lookup entry restore: same ownership rule. 1 = restored. */
MFV_FN int mfv_lookup_restore(unsigned short cur, unsigned short ours,
                              unsigned short saved, unsigned short *out)
{
    if (cur != ours) { *out = cur; return 0; }
    *out = saved;
    return 1;
}

#endif /* MF_VOICE_H */
