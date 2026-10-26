/* mf_lamps.h - pure mapping: engine-family warning sources -> the two native
 * dash lamp bits (L run, 2026-10-05). No game access, no windows.h; unit
 * tested by tools/lamps_test.c.
 *
 * The DLL feeds in the held() state of each dash warning (the state the driver
 * actually sees on the message strip after its detection delay - NOT the
 * reminder blink phase) plus the engine-dead latch. The owner's rule
 * (changed 2026-10-06 after live valve test: thermometer lit at 102 C oil):
 *   engine lamp: the native lamp is an overheat thermometer, so ONLY real
 *                temperature warnings light it: W_OILT, W_WATER, W_RADBLOCK
 *                (debris, shown only once water temp is high).
 *                Valve / oil pressure / blowby / exhaust / coolant low /
 *                ENGINE CRITICAL / mod-dead no longer light it (HUD + radio
 *                still report them; native oil > threshold still lights it).
 *   fuel lamp:   W_FUEL (fuel pressure), W_FUELLEAK.
 * The W_* / C_* ids live in engine_failure.c; this enum is the DLL-side view
 * of the same warnings in the spec's order. Fuel warnings NEVER light the
 * engine lamp.
 */
#ifndef MF_LAMPS_H
#define MF_LAMPS_H

#define MF_LAMP_ENGINE 1
#define MF_LAMP_FUEL   2

enum {
    MF_LS_VALVE = 0,    /* W_VALVE    - LOSING REVS / SHIFT EARLIER */
    MF_LS_OILP,         /* W_OILP     - OIL PRESSURE (film) */
    MF_LS_OILLOW,       /* W_OILLOW   - OIL PRESSURE (gauge low) */
    MF_LS_OILT,         /* W_OILT     - OIL TEMP */
    MF_LS_WATER,        /* W_WATER    - WATER TEMP */
    MF_LS_FUEL,         /* W_FUEL     - FUEL PRESSURE */
    MF_LS_BLOWBY,       /* W_BLOWBY   - ENGINE PROBLEM */
    MF_LS_RADBLOCK,     /* W_RADBLOCK - BOX - DEBRIS */
    MF_LS_EXHTEMP,      /* W_EXHTEMP  - EXHAUST TEMP */
    MF_LS_FUELLEAK,     /* W_FUELLEAK - FUEL LEAK */
    MF_LS_CRITICAL,     /* any critical() "ENGINE CRITICAL" line */
    MF_LS_DEAD,         /* mod-killed engine latch (until repair/stint end) */
    MF_LS_COOLANTLOW,   /* W_COOLANTLOW - COOLANT LOW (Claude review 2026-10-05) */
    MF_LS_COUNT
};

static __attribute__((unused)) int mf_lamp_bit_for_source(int src)
{
    switch (src) {
    case MF_LS_FUEL:
    case MF_LS_FUELLEAK:
        return MF_LAMP_FUEL;
    case MF_LS_OILT:
    case MF_LS_WATER:
    case MF_LS_RADBLOCK:
        return MF_LAMP_ENGINE;
    default:
        return 0;
    }
}

/* OR of the lamp bits of every source set in `mask` (unknown bits ignored) */
static __attribute__((unused)) int mf_lamp_bits(unsigned mask)
{
    int bits = 0, i;
    for (i = 0; i < MF_LS_COUNT; i++)
        if (mask & (1u << i))
            bits |= mf_lamp_bit_for_source(i);
    return bits;
}

static __attribute__((unused)) const char *mf_lamp_source_name(int src)
{
    static const char *const names[MF_LS_COUNT] = {
        "W_VALVE", "W_OILP", "W_OILLOW", "W_OILT", "W_WATER", "W_FUEL",
        "W_BLOWBY", "W_RADBLOCK", "W_EXHTEMP", "W_FUELLEAK", "CRITICAL", "DEAD",
        "W_COOLANTLOW"
    };
    return (src >= 0 && src < MF_LS_COUNT) ? names[src] : "?";
}

/* first set source (enum order) that drives `lamp`, or "?" */
static __attribute__((unused)) const char *mf_lamp_first_name(unsigned mask, int lamp)
{
    int i;
    for (i = 0; i < MF_LS_COUNT; i++)
        if ((mask & (1u << i)) && mf_lamp_bit_for_source(i) == lamp)
            return mf_lamp_source_name(i);
    return "?";
}

/* Shift lights (2026-10-06, owner "go ahead" on option 1): the native mask
 * sub_49B7A0 (FACT, raw lines 33-60) lights LED165/170/175/182 from
 * rpm / rev limit: <0.90 -> 0, <0.93 -> 1, <0.96 -> 3, <0.977 -> 7, else 15.
 * The mod reuses the SAME steps against its lowered limit, so a valve or
 * save-mode rev cut shows on the wheel as an earlier full bar. */
static __attribute__((unused)) int mf_shift_mask(float ratio)
{
    if (!(ratio >= 0.90f)) return 0;   /* also NaN */
    if (ratio < 0.93f) return 1;
    if (ratio < 0.96f) return 3;
    if (ratio < 0.977f) return 7;
    return 15;
}

#endif
