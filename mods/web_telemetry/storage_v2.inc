/* ========================================================================
 * 1.1.0 STORAGE V2 CORE (storage_v2.inc, batch 1)
 * Spec: STORAGE_MANAGER_SPEC_2026-10-13.md sections 3.3-3.5, 4, 11, 12.
 * Pure C: no game memory, no threads, no file IO. Included by web_telemetry.c
 * next to session_archive.inc. Nothing here is called by capture, the HTTP
 * API or the 1.0.1 archive yet, so runtime recording behaviour is unchanged.
 * ======================================================================== */

#define SV2_MAGIC "F1CTEL02"
#define SV2_HEADER_BYTES 192u
#define SV2_TABLE_MAX_CHANNELS 128u
#define SV2_MAX_CARS 104u
#define SV2_MAX_BLOCK_TICKS 255u
#define SV2_MAX_PAYLOAD (1u << 20)
#define SV2_META_MAX (16u * 1024u)
#define SV2_BLOCK_MAGIC "BLK2"
#define SV2_BLOCK_HEADER_BYTES 40u
#define SV2_BLOCK_DATA 1u
#define SV2_BLOCK_META 2u
#define SV2_BLOCK_END 3u
#define SV2_BLOCK_GAP 4u
#define SV2_BLOCK_EVENT 5u /* 2.1 (B1): JSON event, t_first = session sim time of the event */
#define SV2_EVENT_MAX 512u /* 2.1 (B1): size cap per event payload (spec 5, B1) */
#define SV2_BLOCK_FLAG_COMPRESSED 1u
/* 2.1 (B1, spec 4.1 and owner Q3/Q4): minor 1 is the only format a reader accepts. Minor 0 (1.1.0 batches 1-5)
 * is refused with SV2_R_VERSION; the owner removes those files himself. */
#define SV2_MINOR 1u
#define SV2_FLAG_HAS_SIM (1u << 9) /* 2.1: every DATA block carries per-tick sim time (sim_ms deltas) */
#define SV2_FLAG_ARCHIVED (1u << 0)
#define SV2_FLAG_CLOSED (1u << 2)
#define SV2_FLAG_COMPRESSED_BLOCKS (1u << 3)
#define SV2_FLAG_UNCOMPRESSED_FALLBACK (1u << 4)
#define SV2_FLAG_PLAYER_PCH (1u << 5)
#define SV2_FLAG_RECOVERED (1u << 6)
#define SV2_FLAG_LEGACY_IMPORT (1u << 7)
#define SV2_FLAG_TICK_GAPS (1u << 8)
#define SV2_COMP_NONE 0u
#define SV2_COMP_XPRESS 3u /* compressapi.h COMPRESS_ALGORITHM_XPRESS (FACT src) */
#define SV2_COMP_LZMS 5u   /* compressapi.h COMPRESS_ALGORITHM_LZMS (FACT src) */
#define SV2_TYPE_Q16 0u
#define SV2_TYPE_RAW32 1u
#define SV2_TYPE_TICK_MS 2u
#define SV2_SCOPE_PLAYER 1u
#define SV2_SCOPE_AI 2u
#define SV2_SCOPE_ALL 3u

/* Reader reason codes (spec 4.10). OK = no stop. */
#define SV2_OK 0
#define SV2_R_HEADER 1
#define SV2_R_TABLE 2
#define SV2_R_BLOCK_HEADER 3
#define SV2_R_BLOCK_CRC 4
#define SV2_R_BLOCK_DECODE 5
#define SV2_R_DECODER_UNAVAILABLE 6
#define SV2_R_VERSION 7 /* 2.1 (B1): minor version other than 1 (pre-B1 files are refused, owner Q3) */

/* ---- CRC-32 (reflected 0xEDB88320, zlib compatible), nibble table ---- */
static uint32_t sv2_crc32(uint32_t crc, const void *data, size_t n) {
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ t[crc & 0xFu];
        crc = (crc >> 4) ^ t[crc & 0xFu];
    }
    return ~crc;
}

/* ---- little-endian and float bit helpers ---- */
static uint64_t sv2_get_le(const uint8_t *p, unsigned bytes) {
    uint64_t v = 0;
    for (unsigned i = 0; i < bytes; i++) v |= (uint64_t)p[i] << (8u * i);
    return v;
}
static void sv2_put_le(uint8_t *p, uint64_t v, unsigned bytes) {
    for (unsigned i = 0; i < bytes; i++) p[i] = (uint8_t)(v >> (8u * i));
}
static uint32_t sv2_f32_bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float sv2_f32_from(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint64_t sv2_f64_bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double sv2_f64_from(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }

/* ---- zigzag (spec 4.4) ---- */
static uint32_t sv2_zz_enc(int32_t n) { return ((uint32_t)n << 1) ^ (n < 0 ? 0xFFFFFFFFu : 0u); }
static int32_t sv2_zz_dec(uint32_t z) { return (int32_t)((z >> 1) ^ (0u - (z & 1u))); }

/* ---- bounded writer ---- */
typedef struct { uint8_t *p; size_t cap, n; int err; } sv2_w_t;
static void sv2_w_bytes(sv2_w_t *w, const void *src, size_t k) {
    if (w->err || k > w->cap - w->n) { w->err = 1; return; }
    if (k) memcpy(w->p + w->n, src, k);
    w->n += k;
}
static void sv2_w_int(sv2_w_t *w, uint64_t v, unsigned bytes) {
    uint8_t b[8];
    sv2_put_le(b, v, bytes);
    sv2_w_bytes(w, b, bytes);
}
static void sv2_w_varint(sv2_w_t *w, uint32_t v) { /* LEB128, at most 5 bytes */
    uint8_t b[5];
    unsigned k = 0;
    do {
        uint8_t c = (uint8_t)(v & 0x7Fu);
        v >>= 7;
        if (v) c |= 0x80u;
        b[k++] = c;
    } while (v);
    sv2_w_bytes(w, b, k);
}
static void sv2_w_zz(sv2_w_t *w, int32_t v) { sv2_w_varint(w, sv2_zz_enc(v)); }

/* ---- bounded reader: every read is checked, a failure is sticky ---- */
typedef struct { const uint8_t *p; size_t n, pos; int err; } sv2_r_t;
static int sv2_r_take(sv2_r_t *r, size_t k) {
    if (r->err || k > r->n - r->pos) { r->err = 1; return 0; }
    return 1;
}
static uint64_t sv2_r_int(sv2_r_t *r, unsigned bytes) {
    uint64_t v;
    if (!sv2_r_take(r, bytes)) return 0;
    v = sv2_get_le(r->p + r->pos, bytes);
    r->pos += bytes;
    return v;
}
static int sv2_r_varint(sv2_r_t *r, uint32_t *out) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 5; i++) {
        uint8_t b;
        if (!sv2_r_take(r, 1)) return 0;
        b = r->p[r->pos++];
        if (i == 4 && (b & 0xF0u)) { r->err = 1; return 0; } /* over 32 bits or over 5 bytes */
        v |= (uint32_t)(b & 0x7Fu) << (7u * i);
        if (!(b & 0x80u)) { *out = v; return 1; }
    }
    r->err = 1; /* continuation bit still set after 5 bytes */
    return 0;
}
static int sv2_r_zz(sv2_r_t *r, int32_t *out) {
    uint32_t z;
    if (!sv2_r_varint(r, &z)) return 0;
    *out = sv2_zz_dec(z);
    return 1;
}

/* ---- Q16 quantizer (spec 4.5). Returns q; *clamped set for NaN, inf or out of range. ---- */
static uint16_t sv2_q16_encode(float v, float mn, float mx, int *clamped) {
    double lo = mn, hi = mx, x = v, q;
    if (clamped) *clamped = 0;
    if (!(hi > lo)) { if (clamped) *clamped = 1; return 0; }
    if (x != x) { x = lo; if (clamped) *clamped = 1; }
    else if (x < lo) { x = lo; if (clamped) *clamped = 1; }
    else if (x > hi) { x = hi; if (clamped) *clamped = 1; }
    q = floor((x - lo) * 65535.0 / (hi - lo) + 0.5);
    if (q < 0.0) q = 0.0;
    if (q > 65535.0) q = 65535.0;
    return (uint16_t)q;
}
static float sv2_q16_decode(uint16_t q, float mn, float mx) {
    return (float)((double)mn + (double)q * ((double)mx - (double)mn) / 65535.0);
}

/* ---- channel catalog (spec 4.5 proposal, ranges ESTIMATE, OQ-5). Ids 0..23 batch 1-2, 24..63 pch batch 3. ---- */
typedef struct {
    uint8_t id, type, scope;
    const char *key;
    float min, max;
} sv2_catalog_entry_t;
static const sv2_catalog_entry_t sv2_catalog[] = {
    {0, SV2_TYPE_TICK_MS, SV2_SCOPE_ALL, "tick_ms", 0.0f, 0.0f},
    {1, SV2_TYPE_Q16, SV2_SCOPE_ALL, "dist", 0.0f, 8000.0f},
    {2, SV2_TYPE_Q16, SV2_SCOPE_ALL, "speed", 0.0f, 120.0f},
    {3, SV2_TYPE_Q16, SV2_SCOPE_ALL, "throttle", 0.0f, 1.0f},
    {4, SV2_TYPE_Q16, SV2_SCOPE_ALL, "brake", 0.0f, 1.0f},
    {5, SV2_TYPE_Q16, SV2_SCOPE_AI, "steer", -1.0f, 1.0f},
    {6, SV2_TYPE_Q16, SV2_SCOPE_ALL, "glat", -80.0f, 80.0f},
    {7, SV2_TYPE_Q16, SV2_SCOPE_ALL, "glon", -80.0f, 80.0f},
    {8, SV2_TYPE_Q16, SV2_SCOPE_ALL, "gvert", -60.0f, 60.0f},
    {9, SV2_TYPE_Q16, SV2_SCOPE_ALL, "yaw", -3.1416f, 3.1416f},
    {10, SV2_TYPE_Q16, SV2_SCOPE_ALL, "yaw_rate", -10.0f, 10.0f},
    {11, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tread_fl", 0.0f, 1.0f},
    {12, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tread_fr", 0.0f, 1.0f},
    {13, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tread_rl", 0.0f, 1.0f},
    {14, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tread_rr", 0.0f, 1.0f},
    {15, SV2_TYPE_Q16, SV2_SCOPE_ALL, "rpm", 0.0f, 25000.0f},
    {16, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "gear", 0.0f, 0.0f},
    {17, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tx", -10000.0f, 10000.0f},
    {18, SV2_TYPE_Q16, SV2_SCOPE_ALL, "tz", -10000.0f, 10000.0f},
    {19, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "flags", 0.0f, 0.0f},
    {20, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "ttemp_fl", 0.0f, 200.0f},
    {21, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "ttemp_fr", 0.0f, 200.0f},
    {22, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "ttemp_rl", 0.0f, 200.0f},
    {23, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "ttemp_rr", 0.0f, 200.0f},
    /* 1.1.0 batch 3: player chassis ids 24..63 (spec 4.6, trimmed to 40 slots, OQ-5). Stored as
     * RAW32 exact bit patterns (f32 bits for floats, i32 for ints): no measured range exists
     * (OQ-5), so no Q16 bound is invented. Ids 60 and 61 are integers, the rest are f32 bits. */
    {24, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_ride_fl", 0.0f, 0.0f},
    {25, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_ride_fr", 0.0f, 0.0f},
    {26, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_ride_rl", 0.0f, 0.0f},
    {27, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_ride_rr", 0.0f, 0.0f},
    {28, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_susp_fl", 0.0f, 0.0f},
    {29, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_susp_fr", 0.0f, 0.0f},
    {30, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_susp_rl", 0.0f, 0.0f},
    {31, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_susp_rr", 0.0f, 0.0f},
    {32, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_tdef_fl", 0.0f, 0.0f},
    {33, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_tdef_fr", 0.0f, 0.0f},
    {34, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_tdef_rl", 0.0f, 0.0f},
    {35, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_tdef_rr", 0.0f, 0.0f},
    {36, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_press_fl", 0.0f, 0.0f},
    {37, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_press_fr", 0.0f, 0.0f},
    {38, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_press_rl", 0.0f, 0.0f},
    {39, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_press_rr", 0.0f, 0.0f},
    {40, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_grip_fl", 0.0f, 0.0f},
    {41, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_grip_fr", 0.0f, 0.0f},
    {42, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_grip_rl", 0.0f, 0.0f},
    {43, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_grip_rr", 0.0f, 0.0f},
    {44, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_camber_fl", 0.0f, 0.0f},
    {45, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_camber_fr", 0.0f, 0.0f},
    {46, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_camber_rl", 0.0f, 0.0f},
    {47, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_camber_rr", 0.0f, 0.0f},
    {48, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_spin_fl", 0.0f, 0.0f},
    {49, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_spin_fr", 0.0f, 0.0f},
    {50, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_spin_rl", 0.0f, 0.0f},
    {51, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_spin_rr", 0.0f, 0.0f},
    {52, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_disc_fl", 0.0f, 0.0f},
    {53, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_disc_fr", 0.0f, 0.0f},
    {54, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_disc_rl", 0.0f, 0.0f},
    {55, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_disc_rr", 0.0f, 0.0f},
    {56, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_bend_fl", 0.0f, 0.0f},
    {57, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_bend_fr", 0.0f, 0.0f},
    {58, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_bend_rl", 0.0f, 0.0f},
    {59, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_bend_rr", 0.0f, 0.0f},
    {60, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_bottom", 0.0f, 0.0f},
    {61, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_compound", 0.0f, 0.0f},
    {62, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_fuel", 0.0f, 0.0f},
    {63, SV2_TYPE_RAW32, SV2_SCOPE_PLAYER, "pch_rpm", 0.0f, 0.0f},
    /* 2.1 (B1, spec 5 B1 / inventory T13-T29, T32, T35): ids 64..124, all cars unless the scope says player.
     * Q16 ranges are ESTIMATE (OQ-5): a value outside the range is clamped and counted (sw_wr.clamped).
     * RAW32 integers are exact (sw_raw_is_int in storage_writer.inc); RAW32 floats are f32 bit patterns. */
    {64, SV2_TYPE_Q16, SV2_SCOPE_ALL, "wear_fl", 0.0f, 2.0f},      /* T13 raw tyre wear, 1.0 = fresh */
    {65, SV2_TYPE_Q16, SV2_SCOPE_ALL, "wear_fr", 0.0f, 2.0f},
    {66, SV2_TYPE_Q16, SV2_SCOPE_ALL, "wear_rl", 0.0f, 2.0f},
    {67, SV2_TYPE_Q16, SV2_SCOPE_ALL, "wear_rr", 0.0f, 2.0f},
    {68, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "position", 0.0f, 0.0f},   /* T24 */
    {69, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "lap", 0.0f, 0.0f},        /* T23 lap number */
    {70, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "pit_state", 0.0f, 0.0f},  /* T25 pittingStatus 0..4 */
    {71, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "call_to_box", 0.0f, 0.0f},/* T25 */
    {72, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "tire_choice", 0.0f, 0.0f},/* T26 compound choice */
    {73, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "track_limits", 0.0f, 0.0f},/* T29 */
    {74, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "fw_flag", 0.0f, 0.0f},    /* T28 1 intact, 0 armed/detached, -1 unreadable */
    {75, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rw_flag", 0.0f, 0.0f},
    {76, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "dmg_tier", 0.0f, 0.0f},
    {77, SV2_TYPE_Q16, SV2_SCOPE_ALL, "oil_temp", 0.0f, 200.0f},   /* T15 C */
    {78, SV2_TYPE_Q16, SV2_SCOPE_ALL, "water_temp", 0.0f, 200.0f},
    {79, SV2_TYPE_Q16, SV2_SCOPE_ALL, "fuel_level", 0.0f, 200.0f}, /* T32 units per game */
    {80, SV2_TYPE_Q16, SV2_SCOPE_ALL, "gap_ahead_m", -1.0f, 20000.0f}, /* T21, -1 = none */
    {81, SV2_TYPE_Q16, SV2_SCOPE_ALL, "gap_behind_m", -1.0f, 20000.0f},
    {82, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "gap_ahead_id", 0.0f, 0.0f},
    {83, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "gap_behind_id", 0.0f, 0.0f},
    {84, SV2_TYPE_Q16, SV2_SCOPE_ALL, "sec_delta", -200.0f, 200.0f},      /* T22 live sector delta, s */
    {85, SV2_TYPE_Q16, SV2_SCOPE_ALL, "last_sec_delta", -200.0f, 200.0f},
    {86, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "last_lap_s", 0.0f, 0.0f},        /* T23 game lap time, f32 bits */
    {87, SV2_TYPE_Q16, SV2_SCOPE_ALL, "abt_front", -1000.0f, 1200.0f},    /* T9/brake temp C, -999 = none */
    {88, SV2_TYPE_Q16, SV2_SCOPE_ALL, "abt_rear", -1000.0f, 1200.0f},
    {89, SV2_TYPE_Q16, SV2_SCOPE_ALL, "aws_fl", 0.0f, 1000.0f},           /* wheel spin rad/s */
    {90, SV2_TYPE_Q16, SV2_SCOPE_ALL, "aws_fr", 0.0f, 1000.0f},
    {91, SV2_TYPE_Q16, SV2_SCOPE_ALL, "aws_rl", 0.0f, 1000.0f},
    {92, SV2_TYPE_Q16, SV2_SCOPE_ALL, "aws_rr", 0.0f, 1000.0f},
    {93, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "tcarc_fl", 0.0f, 200.0f},       /* T10 carcass C (FACT player) */
    {94, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "tcarc_fr", 0.0f, 200.0f},
    {95, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "tcarc_rl", 0.0f, 200.0f},
    {96, SV2_TYPE_Q16, SV2_SCOPE_PLAYER, "tcarc_rr", 0.0f, 200.0f},
    {97, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "race_num", 0.0f, 0.0f},         /* T27 */
    {98, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "controller_role", 0.0f, 0.0f},  /* T27 CO-OP role */
    {99, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "car_valid", 0.0f, 0.0f},        /* T19 validity bits */
    {100, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "vf6", 0.0f, 0.0f},             /* research validity bits */
    {101, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "contact_with", 0.0f, 0.0f},    /* heuristic contact, -1 none */
    {102, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_su_fl", 0.0f, 0.0f},       /* T35 RESEARCH candidates, f32 bits, Q2 all cars */
    {103, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_su_fr", 0.0f, 0.0f},
    {104, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_su_rl", 0.0f, 0.0f},
    {105, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_su_rr", 0.0f, 0.0f},
    {106, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_ld_fl", 0.0f, 0.0f},
    {107, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_ld_fr", 0.0f, 0.0f},
    {108, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_ld_rl", 0.0f, 0.0f},
    {109, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_ld_rr", 0.0f, 0.0f},
    {110, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_sl_fl", 0.0f, 0.0f},
    {111, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_sl_fr", 0.0f, 0.0f},
    {112, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_sl_rl", 0.0f, 0.0f},
    {113, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_sl_rr", 0.0f, 0.0f},
    {114, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_fo_fl", 0.0f, 0.0f},
    {115, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_fo_fr", 0.0f, 0.0f},
    {116, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_fo_rl", 0.0f, 0.0f},
    {117, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "rx_fo_rr", 0.0f, 0.0f},
    {118, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "penalties", 0.0f, 0.0f},
    {119, SV2_TYPE_Q16, SV2_SCOPE_ALL, "fuel_avg_lap", 0.0f, 10.0f},    /* T32 rolling use per lap */
    {120, SV2_TYPE_Q16, SV2_SCOPE_ALL, "fuel_aut_laps", 0.0f, 500.0f},  /* T32 autonomy, laps */
    {121, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "box_soon", 0.0f, 0.0f},       /* heuristic, labelled */
    {122, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "finish_state", 0.0f, 0.0f},   /* T36 classification input */
    {123, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "driver_status", 0.0f, 0.0f},
    {124, SV2_TYPE_RAW32, SV2_SCOPE_ALL, "reason_code", 0.0f, 0.0f},
};
#define SV2_CATALOG_COUNT ((unsigned)(sizeof sv2_catalog / sizeof sv2_catalog[0]))

/* ---- channel table (spec 4.2) ---- */
typedef struct {
    uint8_t id, type, scope;
    float min, max;
    uint8_t name_len;
    char name[33];
} sv2_channel_t;

static void sv2_catalog_to_table(sv2_channel_t *out) {
    for (unsigned i = 0; i < SV2_CATALOG_COUNT; i++) {
        const sv2_catalog_entry_t *e = &sv2_catalog[i];
        size_t k = strlen(e->key);
        memset(&out[i], 0, sizeof out[i]);
        out[i].id = e->id;
        out[i].type = e->type;
        out[i].scope = e->scope;
        out[i].min = e->min;
        out[i].max = e->max;
        out[i].name_len = (uint8_t)k;
        memcpy(out[i].name, e->key, k);
    }
}

static size_t sv2_table_encode(const sv2_channel_t *ch, uint32_t count, uint8_t *out, size_t cap) {
    sv2_w_t w = {out, cap, 0, 0};
    if (count == 0 || count > SV2_TABLE_MAX_CHANNELS) return 0;
    sv2_w_int(&w, count, 2);
    sv2_w_int(&w, 0, 2);
    for (uint32_t i = 0; i < count; i++) {
        const sv2_channel_t *c = &ch[i];
        if (c->name_len < 1 || c->name_len > 32) return 0;
        sv2_w_int(&w, c->id, 1);
        sv2_w_int(&w, c->type, 1);
        sv2_w_int(&w, c->scope, 1);
        sv2_w_int(&w, 0, 1);
        sv2_w_int(&w, sv2_f32_bits(c->min), 4);
        sv2_w_int(&w, sv2_f32_bits(c->max), 4);
        sv2_w_int(&w, c->name_len, 1);
        sv2_w_bytes(&w, c->name, c->name_len);
    }
    if (w.err) return 0;
    sv2_w_int(&w, sv2_crc32(0, out, w.n), 4);
    return w.err ? 0 : w.n;
}

/* p/n = whole table span (entries + trailing CRC). Returns SV2_OK or SV2_R_TABLE. */
static int sv2_table_decode(const uint8_t *p, size_t n, uint32_t declared, sv2_channel_t *out) {
    sv2_r_t r;
    uint8_t seen[SV2_TABLE_MAX_CHANNELS];
    uint32_t count;
    if (n < 8) return SV2_R_TABLE;
    if (sv2_get_le(p + n - 4, 4) != sv2_crc32(0, p, n - 4)) return SV2_R_TABLE;
    r.p = p; r.n = n - 4; r.pos = 0; r.err = 0;
    count = (uint32_t)sv2_r_int(&r, 2);
    if (sv2_r_int(&r, 2) != 0) return SV2_R_TABLE;
    if (count != declared || count == 0 || count > SV2_TABLE_MAX_CHANNELS) return SV2_R_TABLE;
    memset(seen, 0, sizeof seen);
    for (uint32_t i = 0; i < count; i++) {
        sv2_channel_t *c = &out[i];
        uint8_t id, type, scope, rsv;
        float mn, mx;
        uint8_t name_len;
        if (!sv2_r_take(&r, 13)) return SV2_R_TABLE;
        id = (uint8_t)sv2_r_int(&r, 1);
        type = (uint8_t)sv2_r_int(&r, 1);
        scope = (uint8_t)sv2_r_int(&r, 1);
        rsv = (uint8_t)sv2_r_int(&r, 1);
        mn = sv2_f32_from((uint32_t)sv2_r_int(&r, 4));
        mx = sv2_f32_from((uint32_t)sv2_r_int(&r, 4));
        name_len = (uint8_t)sv2_r_int(&r, 1);
        if (r.err || id >= SV2_TABLE_MAX_CHANNELS || seen[id] || type > SV2_TYPE_TICK_MS) return SV2_R_TABLE;
        if (rsv != 0 || scope == 0 || scope > SV2_SCOPE_ALL || name_len < 1 || name_len > 32) return SV2_R_TABLE;
        if (!sv2_r_take(&r, name_len)) return SV2_R_TABLE;
        for (uint8_t k = 0; k < name_len; k++) {
            uint8_t ch = r.p[r.pos + k];
            if (ch < 0x20u || ch > 0x7Eu) return SV2_R_TABLE;
            c->name[k] = (char)ch;
        }
        c->name[name_len] = 0;
        r.pos += name_len;
        if (!isfinite((double)mn) || !isfinite((double)mx)) return SV2_R_TABLE;
        if (type == SV2_TYPE_Q16 && !(mx > mn)) return SV2_R_TABLE;
        c->id = id; c->type = type; c->scope = scope; c->min = mn; c->max = mx; c->name_len = name_len;
        seen[id] = 1;
    }
    if (r.err || r.pos != r.n) return SV2_R_TABLE;
    return SV2_OK;
}

/* ---- file header (spec 4.1, 192 bytes) ---- */
typedef struct {
    uint32_t flags, compression_id, block_ticks_max, block_ms_max, channel_table_offset, channel_table_bytes;
    double start_unix_utc;
    int32_t tz_offset_min, session_type;
    uint32_t broker_generation, channels_declared;
    char track[65];
    char session_name[61];
} sv2_header_t;

static void sv2_header_encode(const sv2_header_t *h, uint8_t out[SV2_HEADER_BYTES]) {
    memset(out, 0, SV2_HEADER_BYTES);
    memcpy(out, SV2_MAGIC, 8);
    sv2_put_le(out + 8, 2, 2);
    sv2_put_le(out + 10, SV2_MINOR, 2);
    sv2_put_le(out + 12, SV2_HEADER_BYTES, 4);
    sv2_put_le(out + 16, h->flags, 4);
    sv2_put_le(out + 20, h->compression_id, 4);
    sv2_put_le(out + 24, h->block_ticks_max, 4);
    sv2_put_le(out + 28, h->block_ms_max, 4);
    sv2_put_le(out + 32, h->channel_table_offset, 4);
    sv2_put_le(out + 36, h->channel_table_bytes, 4);
    sv2_put_le(out + 40, sv2_f64_bits(h->start_unix_utc), 8);
    sv2_put_le(out + 48, (uint32_t)h->tz_offset_min, 4);
    sv2_put_le(out + 52, (uint32_t)h->session_type, 4);
    sv2_put_le(out + 56, h->broker_generation, 4);
    sv2_put_le(out + 60, h->channels_declared, 4);
    {
        size_t k = strlen(h->track);
        if (k > 64) k = 64;
        memcpy(out + 64, h->track, k);
    }
    {
        size_t k = strlen(h->session_name);
        if (k > 60) k = 60;
        memcpy(out + 128, h->session_name, k);
    }
    sv2_put_le(out + 188, sv2_crc32(0, out, 188), 4);
}

static int sv2_header_decode(const uint8_t *p, size_t n, sv2_header_t *h) {
    if (n < SV2_HEADER_BYTES) return SV2_R_HEADER;
    if (memcmp(p, SV2_MAGIC, 8) != 0) return SV2_R_HEADER;
    if (sv2_get_le(p + 8, 2) != 2 || sv2_get_le(p + 12, 4) != SV2_HEADER_BYTES) return SV2_R_HEADER;
    if (sv2_get_le(p + 188, 4) != sv2_crc32(0, p, 188)) return SV2_R_HEADER;
    if (sv2_get_le(p + 10, 2) != SV2_MINOR) return SV2_R_VERSION;
    h->flags = (uint32_t)sv2_get_le(p + 16, 4);
    h->compression_id = (uint32_t)sv2_get_le(p + 20, 4);
    h->block_ticks_max = (uint32_t)sv2_get_le(p + 24, 4);
    h->block_ms_max = (uint32_t)sv2_get_le(p + 28, 4);
    h->channel_table_offset = (uint32_t)sv2_get_le(p + 32, 4);
    h->channel_table_bytes = (uint32_t)sv2_get_le(p + 36, 4);
    h->start_unix_utc = sv2_f64_from(sv2_get_le(p + 40, 8));
    h->tz_offset_min = (int32_t)(uint32_t)sv2_get_le(p + 48, 4);
    h->session_type = (int32_t)(uint32_t)sv2_get_le(p + 52, 4);
    h->broker_generation = (uint32_t)sv2_get_le(p + 56, 4);
    h->channels_declared = (uint32_t)sv2_get_le(p + 60, 4);
    memcpy(h->track, p + 64, 64);
    h->track[64] = 0;
    memcpy(h->session_name, p + 128, 60);
    h->session_name[60] = 0;
    return SV2_OK;
}

/* ---- block framing (spec 4.3, 40-byte header) ---- */
typedef struct {
    uint8_t type, flags;
    uint32_t seq, n_ticks, raw_len, stored_len, payload_crc;
    double t_first;
} sv2_block_t;

static void sv2_block_hdr_encode(const sv2_block_t *b, uint8_t out[SV2_BLOCK_HEADER_BYTES]) {
    memset(out, 0, SV2_BLOCK_HEADER_BYTES);
    memcpy(out, SV2_BLOCK_MAGIC, 4);
    out[4] = b->type;
    out[5] = b->flags;
    sv2_put_le(out + 8, b->seq, 4);
    sv2_put_le(out + 12, b->n_ticks, 4);
    sv2_put_le(out + 16, sv2_f64_bits(b->t_first), 8);
    sv2_put_le(out + 24, b->raw_len, 4);
    sv2_put_le(out + 28, b->stored_len, 4);
    sv2_put_le(out + 32, b->payload_crc, 4);
    sv2_put_le(out + 36, sv2_crc32(0, out, 36), 4);
}

/* Returns SV2_OK or SV2_R_BLOCK_HEADER. Payload size and CRC are checked by the caller. */
static int sv2_block_hdr_decode(const uint8_t *p, size_t n, sv2_block_t *b) {
    double t;
    if (n < SV2_BLOCK_HEADER_BYTES) return SV2_R_BLOCK_HEADER;
    if (memcmp(p, SV2_BLOCK_MAGIC, 4) != 0) return SV2_R_BLOCK_HEADER;
    if (sv2_get_le(p + 36, 4) != sv2_crc32(0, p, 36)) return SV2_R_BLOCK_HEADER;
    b->type = p[4];
    b->flags = p[5];
    if (b->type < SV2_BLOCK_DATA || b->type > SV2_BLOCK_EVENT) return SV2_R_BLOCK_HEADER;
    if (b->flags & ~SV2_BLOCK_FLAG_COMPRESSED) return SV2_R_BLOCK_HEADER;
    if (sv2_get_le(p + 6, 2) != 0) return SV2_R_BLOCK_HEADER;
    b->seq = (uint32_t)sv2_get_le(p + 8, 4);
    b->n_ticks = (uint32_t)sv2_get_le(p + 12, 4);
    t = sv2_f64_from(sv2_get_le(p + 16, 8));
    b->raw_len = (uint32_t)sv2_get_le(p + 24, 4);
    b->stored_len = (uint32_t)sv2_get_le(p + 28, 4);
    b->payload_crc = (uint32_t)sv2_get_le(p + 32, 4);
    b->t_first = t;
    if (!isfinite(t)) return SV2_R_BLOCK_HEADER;
    if (b->raw_len > SV2_MAX_PAYLOAD || b->stored_len > SV2_MAX_PAYLOAD) return SV2_R_BLOCK_HEADER;
    if (b->type == SV2_BLOCK_DATA) {
        if (b->n_ticks < 1 || b->n_ticks > SV2_MAX_BLOCK_TICKS) return SV2_R_BLOCK_HEADER;
    } else if (b->n_ticks != 0 || (b->type != SV2_BLOCK_EVENT && t != 0.0)) {
        return SV2_R_BLOCK_HEADER; /* EVENT carries its sim time in t_first (2.1) */
    }
    if (!(b->flags & SV2_BLOCK_FLAG_COMPRESSED) && b->raw_len != b->stored_len) return SV2_R_BLOCK_HEADER;
    return SV2_OK;
}

/* Writes one framed block (header + stored payload). Returns bytes written or 0. */
static size_t sv2_block_put(uint8_t *out, size_t cap, sv2_block_t b, const uint8_t *stored) {
    if (b.stored_len > cap || cap - b.stored_len < SV2_BLOCK_HEADER_BYTES) return 0;
    b.payload_crc = sv2_crc32(0, stored, b.stored_len);
    sv2_block_hdr_encode(&b, out);
    if (b.stored_len) memcpy(out + SV2_BLOCK_HEADER_BYTES, stored, b.stored_len);
    return SV2_BLOCK_HEADER_BYTES + (size_t)b.stored_len;
}

/* ---- DATA payload (spec 4.4) ---- */
typedef struct {
    uint8_t slot, scope, first_tick, count;
    uint64_t mask_lo, mask_hi;
    /* 2.1: channels whose count values are all equal are written once (first value only). The encoder derives
     * these bits from vals; the decoder fills the repeated values. Subset of mask (checked on decode). */
    uint64_t cmask_lo, cmask_hi;
    uint32_t *vals; /* popcount(mask) * count entries, channel-major: vals[rank * count + i] */
} sv2_car_t;

typedef struct {
    uint8_t n_ticks, n_cars;
    int32_t dt_ms[SV2_MAX_BLOCK_TICKS]; /* dt_ms[0] = 0 (wall clock, tick_ms) */
    int32_t sim_ms[SV2_MAX_BLOCK_TICKS]; /* 2.1: sim_ms[0] = 0; session sim time per tick, ms relative to tick 0 */
    sv2_car_t cars[SV2_MAX_CARS];
    uint32_t *pool; /* decode output pool; caller sized, pool_cap entries */
    size_t pool_cap;
} sv2_data_t;

static uint32_t sv2_popcount64(uint64_t v) {
    uint32_t c = 0;
    while (v) { v &= v - 1; c++; }
    return c;
}
static int sv2_mask_bit(uint64_t lo, uint64_t hi, uint32_t id) {
    return id < 64u ? (int)((lo >> id) & 1u) : (int)((hi >> (id - 64u)) & 1u);
}
static void sv2_mask_set(uint64_t *lo, uint64_t *hi, uint32_t id) {
    if (id < 64u) *lo |= (uint64_t)1u << id;
    else *hi |= (uint64_t)1u << (id - 64u);
}
static void sv2_index_map(const sv2_channel_t *ch, uint32_t nch, int16_t idx[SV2_TABLE_MAX_CHANNELS]) {
    for (uint32_t i = 0; i < SV2_TABLE_MAX_CHANNELS; i++) idx[i] = -1;
    for (uint32_t i = 0; i < nch && i < SV2_TABLE_MAX_CHANNELS; i++)
        if (ch[i].id < SV2_TABLE_MAX_CHANNELS) idx[ch[i].id] = (int16_t)i;
}
/* Car scope 1 = player (needs table bit PLAYER), 0 = AI (needs table bit AI). */
static int sv2_scope_ok(uint8_t car_scope, uint8_t ch_scope) {
    return (ch_scope & (car_scope ? SV2_SCOPE_PLAYER : SV2_SCOPE_AI)) != 0;
}

/* 2.1 DATA payload (B1). Returns bytes written, or 0 on any bounds, range or scope error.
 * Layout: u8 n_ticks, u8 n_cars, zz dt_ms[n_ticks] (dt_ms[0] = 0), zz sim_ms[n_ticks] (sim_ms[0] = 0),
 * then per car: u8 slot, u8 scope, u8 first_tick, u8 count, u64 mask_lo, u64 mask_hi, u64 cmask_lo, u64 cmask_hi.
 * Per present channel (ascending id): first value (Q16 2 B, RAW32 4 B), then count-1 zigzag deltas, unless the
 * channel is constant over the block (cmask bit set: first value only). The encoder derives cmask from vals. */
static int sv2_vals_const(const uint32_t *v, uint32_t count) {
    for (uint32_t i = 1; i < count; i++)
        if (v[i] != v[0]) return 0;
    return 1;
}
static size_t sv2_data_encode(const sv2_data_t *d, const sv2_channel_t *ch, uint32_t nch, uint8_t *out, size_t cap) {
    int16_t idx[SV2_TABLE_MAX_CHANNELS];
    sv2_w_t w = {out, cap, 0, 0};
    if (d->n_ticks < 1 || d->n_cars < 1 || d->n_cars > SV2_MAX_CARS || d->dt_ms[0] != 0 || d->sim_ms[0] != 0) return 0;
    sv2_index_map(ch, nch, idx);
    sv2_w_int(&w, d->n_ticks, 1);
    sv2_w_int(&w, d->n_cars, 1);
    for (uint32_t i = 0; i < d->n_ticks; i++) sv2_w_zz(&w, d->dt_ms[i]);
    for (uint32_t i = 0; i < d->n_ticks; i++) sv2_w_zz(&w, d->sim_ms[i]);
    for (uint32_t c = 0; c < d->n_cars; c++) {
        const sv2_car_t *car = &d->cars[c];
        uint32_t rank = 0;
        uint64_t clo = 0, chi = 0;
        if (car->slot > 103 || car->scope > 1 || car->count < 1 || (uint32_t)car->first_tick + car->count > d->n_ticks)
            return 0;
        for (uint32_t id = 0; id < SV2_TABLE_MAX_CHANNELS; id++) {
            if (!sv2_mask_bit(car->mask_lo, car->mask_hi, id)) continue;
            if (car->count > 1 && sv2_vals_const(car->vals + (size_t)rank * car->count, car->count)) {
                if (id < 64u) clo |= (uint64_t)1u << id;
                else chi |= (uint64_t)1u << (id - 64u);
            }
            rank++;
        }
        sv2_w_int(&w, car->slot, 1);
        sv2_w_int(&w, car->scope, 1);
        sv2_w_int(&w, car->first_tick, 1);
        sv2_w_int(&w, car->count, 1);
        sv2_w_int(&w, car->mask_lo, 8);
        sv2_w_int(&w, car->mask_hi, 8);
        sv2_w_int(&w, clo, 8);
        sv2_w_int(&w, chi, 8);
        rank = 0;
        for (uint32_t id = 0; id < SV2_TABLE_MAX_CHANNELS; id++) {
            const sv2_channel_t *cc;
            const uint32_t *v;
            int is_const;
            if (!sv2_mask_bit(car->mask_lo, car->mask_hi, id)) continue;
            if (id == 0 || idx[id] < 0) return 0; /* tick channel is not per car; unknown channel */
            cc = &ch[idx[id]];
            if (!sv2_scope_ok(car->scope, cc->scope)) return 0;
            v = car->vals + (size_t)rank * car->count;
            is_const = sv2_mask_bit(clo, chi, id);
            if (cc->type == SV2_TYPE_Q16) {
                if (v[0] > 65535u) return 0;
                sv2_w_int(&w, v[0], 2);
                for (uint32_t i = 1; !is_const && i < car->count; i++) {
                    int64_t diff;
                    if (v[i] > 65535u) return 0;
                    diff = (int64_t)v[i] - (int64_t)v[i - 1];
                    sv2_w_zz(&w, (int32_t)diff);
                }
            } else if (cc->type == SV2_TYPE_RAW32) {
                sv2_w_int(&w, v[0], 4);
                for (uint32_t i = 1; !is_const && i < car->count; i++)
                    sv2_w_zz(&w, (int32_t)(uint32_t)(v[i] - v[i - 1]));
            } else {
                return 0;
            }
            rank++;
        }
    }
    if (w.err) return 0;
    return w.n;
}

/* Returns SV2_OK or SV2_R_BLOCK_DECODE. Every read is bounded; trailing bytes are an error. */
static int sv2_data_decode(const uint8_t *p, size_t n, const sv2_channel_t *ch, uint32_t nch, sv2_data_t *d) {
    int16_t idx[SV2_TABLE_MAX_CHANNELS];
    sv2_r_t r = {p, n, 0, 0};
    size_t used = 0;
    sv2_index_map(ch, nch, idx);
    d->n_ticks = (uint8_t)sv2_r_int(&r, 1);
    d->n_cars = (uint8_t)sv2_r_int(&r, 1);
    if (r.err || d->n_ticks < 1 || d->n_cars < 1 || d->n_cars > SV2_MAX_CARS) return SV2_R_BLOCK_DECODE;
    for (uint32_t i = 0; i < d->n_ticks; i++)
        if (!sv2_r_zz(&r, &d->dt_ms[i])) return SV2_R_BLOCK_DECODE;
    for (uint32_t i = 0; i < d->n_ticks; i++)
        if (!sv2_r_zz(&r, &d->sim_ms[i])) return SV2_R_BLOCK_DECODE;
    if (d->dt_ms[0] != 0 || d->sim_ms[0] != 0) return SV2_R_BLOCK_DECODE;
    for (uint32_t c = 0; c < d->n_cars; c++) {
        sv2_car_t *car = &d->cars[c];
        uint32_t rank = 0, npresent;
        size_t need;
        car->slot = (uint8_t)sv2_r_int(&r, 1);
        car->scope = (uint8_t)sv2_r_int(&r, 1);
        car->first_tick = (uint8_t)sv2_r_int(&r, 1);
        car->count = (uint8_t)sv2_r_int(&r, 1);
        car->mask_lo = sv2_r_int(&r, 8);
        car->mask_hi = sv2_r_int(&r, 8);
        car->cmask_lo = sv2_r_int(&r, 8);
        car->cmask_hi = sv2_r_int(&r, 8);
        if (r.err || car->slot > 103 || car->scope > 1 || car->count < 1 ||
            (uint32_t)car->first_tick + car->count > d->n_ticks)
            return SV2_R_BLOCK_DECODE;
        if ((car->cmask_lo & ~car->mask_lo) != 0 || (car->cmask_hi & ~car->mask_hi) != 0) return SV2_R_BLOCK_DECODE;
        npresent = sv2_popcount64(car->mask_lo) + sv2_popcount64(car->mask_hi);
        need = (size_t)npresent * car->count;
        if (need > d->pool_cap - used || used > d->pool_cap) return SV2_R_BLOCK_DECODE;
        car->vals = d->pool + used;
        used += need;
        for (uint32_t id = 0; id < SV2_TABLE_MAX_CHANNELS; id++) {
            const sv2_channel_t *cc;
            uint32_t *v;
            int is_const;
            if (!sv2_mask_bit(car->mask_lo, car->mask_hi, id)) continue;
            if (id == 0 || idx[id] < 0) return SV2_R_BLOCK_DECODE;
            cc = &ch[idx[id]];
            if (!sv2_scope_ok(car->scope, cc->scope)) return SV2_R_BLOCK_DECODE;
            v = car->vals + (size_t)rank * car->count;
            is_const = sv2_mask_bit(car->cmask_lo, car->cmask_hi, id);
            if (cc->type == SV2_TYPE_Q16) {
                int64_t q = (int64_t)sv2_r_int(&r, 2);
                v[0] = (uint32_t)q;
                for (uint32_t i = 1; i < car->count; i++) {
                    int32_t diff = 0;
                    if (is_const) { v[i] = v[0]; continue; }
                    if (!sv2_r_zz(&r, &diff)) return SV2_R_BLOCK_DECODE;
                    q += diff;
                    if (q < 0 || q > 65535) return SV2_R_BLOCK_DECODE;
                    v[i] = (uint32_t)q;
                }
            } else if (cc->type == SV2_TYPE_RAW32) {
                uint32_t acc = (uint32_t)sv2_r_int(&r, 4);
                v[0] = acc;
                for (uint32_t i = 1; i < car->count; i++) {
                    int32_t diff = 0;
                    if (is_const) { v[i] = v[0]; continue; }
                    if (!sv2_r_zz(&r, &diff)) return SV2_R_BLOCK_DECODE;
                    acc = acc + (uint32_t)diff;
                    v[i] = acc;
                }
            } else {
                return SV2_R_BLOCK_DECODE;
            }
            if (r.err) return SV2_R_BLOCK_DECODE;
            rank++;
        }
    }
    if (r.err || r.pos != r.n) return SV2_R_BLOCK_DECODE;
    return SV2_OK;
}

/* ---- compression adapter (spec 4.8): cabinet.dll loaded at runtime ---- */
typedef BOOL (WINAPI *sv2_create_fn)(DWORD algorithm, void *allocation, void **handle);
typedef BOOL (WINAPI *sv2_run_fn)(void *handle, void *in, SIZE_T in_size, void *out, SIZE_T out_size, SIZE_T *used);
typedef BOOL (WINAPI *sv2_close_fn)(void *handle);

typedef struct {
    int state; /* 0 untried, 1 ready, -1 unavailable */
    HMODULE module;
    sv2_create_fn create_comp, create_dec;
    sv2_run_fn comp, dec;
    sv2_close_fn close_comp, close_dec;
} sv2_cab_t;
static sv2_cab_t sv2_cab;
#ifdef TELEMETRY_TEST
static int sv2_test_cabinet_forced_off; /* test hook: simulate cabinet.dll missing */
#endif
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

#define SV2_BIND(field, name)                                                  \
    do {                                                                       \
        FARPROC sv2_fp = GetProcAddress(m, name);                              \
        if (!sv2_fp) { FreeLibrary(m); sv2_cab.state = -1; return 0; }         \
        memcpy(&(field), &sv2_fp, sizeof(field));                              \
    } while (0)

static int sv2_cab_ready(void) {
#ifdef TELEMETRY_TEST
    if (sv2_test_cabinet_forced_off) return 0;
#endif
    if (sv2_cab.state) return sv2_cab.state > 0;
    {
        HMODULE m = LoadLibraryExA("cabinet.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!m) { sv2_cab.state = -1; return 0; }
        sv2_cab.module = m;
        SV2_BIND(sv2_cab.create_comp, "CreateCompressor");
        SV2_BIND(sv2_cab.comp, "Compress");
        SV2_BIND(sv2_cab.close_comp, "CloseCompressor");
        SV2_BIND(sv2_cab.create_dec, "CreateDecompressor");
        SV2_BIND(sv2_cab.dec, "Decompress");
        SV2_BIND(sv2_cab.close_dec, "CloseDecompressor");
    }
    sv2_cab.state = 1;
    return 1;
}

/* Returns 1 if the algorithm can be created on this PC (XPRESS for working blocks, LZMS for archive). */
static int sv2_alg_available(uint32_t alg) {
    void *h = NULL;
    if (!sv2_cab_ready()) return 0;
    if (!sv2_cab.create_comp(alg, NULL, &h) || !h) return 0;
    sv2_cab.close_comp(h);
    return 1;
}

/* Compressed size, or 0 when unavailable, failed or not smaller than cap. */
static size_t sv2_compress_block(uint32_t alg, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    void *h = NULL;
    SIZE_T used = 0;
    BOOL ok;
    if (n == 0 || cap == 0 || !sv2_cab_ready()) return 0;
    if (!sv2_cab.create_comp(alg, NULL, &h) || !h) return 0;
    ok = sv2_cab.comp(h, (void *)in, (SIZE_T)n, out, (SIZE_T)cap, &used);
    sv2_cab.close_comp(h);
    return ok ? (size_t)used : 0;
}

/* Returns 1 ok, 0 corrupt or decode failure, -1 decoder unavailable (not corruption). */
static int sv2_decompress_block(uint32_t alg, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *out_n) {
    void *h = NULL;
    SIZE_T used = 0;
    BOOL ok;
    if (!sv2_cab_ready()) return -1;
    if (!sv2_cab.create_dec(alg, NULL, &h) || !h) return -1;
    ok = sv2_cab.dec(h, (void *)in, (SIZE_T)n, out, (SIZE_T)cap, &used);
    sv2_cab.close_dec(h);
    if (!ok) return 0;
    *out_n = (size_t)used;
    return 1;
}

/* Writer helper: stores compressed when smaller (flag bit0), otherwise raw. Returns 1 ok, 0 no room. */
static int sv2_pack_payload(uint32_t alg, const uint8_t *raw, size_t n, uint8_t *out, size_t cap, size_t *stored, uint8_t *flags) {
    size_t c = 0;
    *flags = 0;
    if (alg != SV2_COMP_NONE && n >= 2 && cap >= n) c = sv2_compress_block(alg, raw, n, out, n - 1);
    if (c > 0 && c < n) { *stored = c; *flags = SV2_BLOCK_FLAG_COMPRESSED; return 1; }
    if (cap < n) return 0;
    if (n) memcpy(out, raw, n);
    *stored = n;
    return 1;
}

/* 2.1 EVENT payload (B1): one JSON object, printable ASCII, at most SV2_EVENT_MAX bytes, brackets and quotes balanced
 * (escapes inside strings honoured). Shape check only: the writer produces the text, the reader only refuses junk. */
static int sv2_event_ok(const uint8_t *p, size_t n) {
    int depth = 0, in_str = 0;
    if (n < 2 || n > SV2_EVENT_MAX || p[0] != '{' || p[n - 1] != '}') return 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = p[i];
        if (c < 0x20u || c > 0x7Eu) return 0;
        if (in_str) {
            if (c == '\\') { if (++i >= n) return 0; }
            else if (c == '"') in_str = 0;
            continue;
        }
        if (c == '"') in_str = 1;
        else if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') { if (--depth < 0) return 0; }
    }
    return depth == 0 && !in_str;
}

/* ---- bounded file reader (spec 4.10). Stops at the last good block. ---- */
typedef struct {
    uint32_t valid_blocks, data_blocks, ticks, dropped_ticks, skipped_blocks;
    double last_good_t;
    size_t valid_bytes;
    int closed, truncated, reason;
    uint32_t reason_seq;
    uint32_t end_data_blocks, end_total_ticks, end_dropped_ticks;
    uint32_t events; /* 2.1: EVENT blocks that passed the shape check */
    sv2_header_t header;
    sv2_channel_t channels[SV2_TABLE_MAX_CHANNELS];
    uint32_t channel_count;
} sv2_scan_t;

/* scratch: DATA decode target. work: decompression buffer of at least SV2_MAX_PAYLOAD bytes. */
static int sv2_scan(const uint8_t *buf, size_t len, sv2_data_t *scratch, uint8_t *work, size_t work_cap, sv2_scan_t *out) {
    sv2_header_t *h = &out->header;
    size_t pos, cto, ctb;
    uint32_t seq = 0;
    memset(out, 0, sizeof *out);
    {
        int hr = sv2_header_decode(buf, len, h); /* SV2_R_HEADER, or SV2_R_VERSION for a pre-2.1 file (B1) */
        if (hr != SV2_OK) { out->reason = hr; return out->reason; }
    }
    cto = h->channel_table_offset;
    ctb = h->channel_table_bytes;
    if (cto < SV2_HEADER_BYTES || ctb < 8 || cto > len || ctb > len - cto || h->channels_declared == 0 ||
        h->channels_declared > SV2_TABLE_MAX_CHANNELS || sv2_table_decode(buf + cto, ctb, h->channels_declared, out->channels) != SV2_OK) {
        out->reason = SV2_R_TABLE;
        return out->reason;
    }
    out->channel_count = h->channels_declared;
    if (!(h->flags & SV2_FLAG_HAS_SIM)) { out->reason = SV2_R_HEADER; return out->reason; } /* 2.1 requires per-tick sim */
    pos = cto + ctb;
    out->valid_bytes = pos;
    while (pos < len) {
        sv2_block_t b;
        const uint8_t *stored;
        size_t avail;
        const uint8_t *payload;
        size_t plen = 0;
        int skip = 0;
        if (len - pos < SV2_BLOCK_HEADER_BYTES) { out->truncated = 1; out->reason = SV2_R_BLOCK_HEADER; out->reason_seq = seq; break; }
        if (sv2_block_hdr_decode(buf + pos, SV2_BLOCK_HEADER_BYTES, &b) != SV2_OK || b.seq != seq) {
            out->reason = SV2_R_BLOCK_HEADER; out->reason_seq = seq; break;
        }
        avail = len - pos - SV2_BLOCK_HEADER_BYTES;
        if (b.stored_len > avail) { out->truncated = 1; out->reason = SV2_R_BLOCK_CRC; out->reason_seq = seq; break; }
        stored = buf + pos + SV2_BLOCK_HEADER_BYTES;
        if (sv2_crc32(0, stored, b.stored_len) != b.payload_crc) { out->reason = SV2_R_BLOCK_CRC; out->reason_seq = seq; break; }
        if (b.flags & SV2_BLOCK_FLAG_COMPRESSED) {
            size_t got = 0;
            int r = (b.raw_len <= work_cap) ? sv2_decompress_block(h->compression_id, stored, b.stored_len, work, work_cap, &got) : 0;
            if (r < 0) {
                skip = 1;
            } else if (r == 0 || got != b.raw_len) {
                out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break;
            }
            payload = work;
            plen = got;
        } else {
            payload = stored;
            plen = b.stored_len;
        }
        if (skip) {
            out->skipped_blocks++;
            if (out->reason == SV2_OK) { out->reason = SV2_R_DECODER_UNAVAILABLE; out->reason_seq = seq; }
        } else if (b.type == SV2_BLOCK_DATA) {
            if (sv2_data_decode(payload, plen, out->channels, out->channel_count, scratch) != SV2_OK || scratch->n_ticks != b.n_ticks) {
                out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break;
            }
            out->last_good_t = b.t_first + (double)scratch->sim_ms[b.n_ticks - 1] / 1000.0; /* 2.1: sim time of the last tick */
            out->data_blocks++;
            out->ticks += b.n_ticks;
        } else if (b.type == SV2_BLOCK_META) {
            if (plen > SV2_META_MAX) { out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break; }
        } else if (b.type == SV2_BLOCK_END) {
            sv2_r_t r = {payload, plen, 0, 0};
            out->end_data_blocks = (uint32_t)sv2_r_int(&r, 4);
            out->end_total_ticks = (uint32_t)sv2_r_int(&r, 4);
            out->end_dropped_ticks = (uint32_t)sv2_r_int(&r, 4);
            if (r.err || r.pos != r.n) { out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break; }
            out->closed = 1;
        } else if (b.type == SV2_BLOCK_EVENT) { /* 2.1: raw JSON, t_first = sim time of the event */
            if (skip || b.flags != 0 || !isfinite(b.t_first) || !sv2_event_ok(payload, plen)) {
                out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break;
            }
            out->events++;
        } else { /* GAP: u32 dropped_ticks, f64 t_from, f64 t_to */
            sv2_r_t r = {payload, plen, 0, 0};
            uint32_t dropped = (uint32_t)sv2_r_int(&r, 4);
            (void)sv2_r_int(&r, 8);
            (void)sv2_r_int(&r, 8);
            if (r.err || r.pos != r.n) { out->reason = SV2_R_BLOCK_DECODE; out->reason_seq = seq; break; }
            out->dropped_ticks += dropped;
        }
        if (!skip) out->valid_blocks++;
        pos += SV2_BLOCK_HEADER_BYTES + (size_t)b.stored_len;
        out->valid_bytes = pos;
        seq++;
        if (b.type == SV2_BLOCK_END) {
            if (pos < len) { out->reason = SV2_R_BLOCK_HEADER; out->reason_seq = seq; }
            break;
        }
    }
    return out->reason;
}

/* Bounded copy, always NUL-terminated. */
static void sv2_copy(char *dst, size_t cap, const char *src) {
    size_t k;
    if (cap == 0) return;
    k = strlen(src);
    if (k > cap - 1) k = cap - 1;
    memcpy(dst, src, k);
    dst[k] = 0;
}

/* ---- AIW track and season parsing (spec 3.4) ---- */
static int sv2_is_digit(char c) { return c >= '0' && c <= '9'; }
/* 1.1.1 (T1): "YYYY" + 0..6 ASCII letters + "_" + track, e.g. 2002_, 1995CK_, 2003RH_, 1996CTDP_.
 * Installed AIW names use the letter form; the year is always the 4 digits. */
static int sv2_year_prefix(const char *s, size_t n, int *year, size_t *track_at) {
    size_t k = 0;
    if (n < 6 || !sv2_is_digit(s[0]) || !sv2_is_digit(s[1]) || !sv2_is_digit(s[2]) || !sv2_is_digit(s[3])) return 0;
    while (k < 6 && 4 + k < n && ((s[4 + k] >= 'A' && s[4 + k] <= 'Z') || (s[4 + k] >= 'a' && s[4 + k] <= 'z'))) k++;
    if (5 + k >= n || s[4 + k] != '_') return 0;
    *year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    *track_at = 5 + k;
    return 1;
}
static int sv2_is_sep(char c) { return c == '\\' || c == '/'; }

/* Sanitize per 3.4 rule 6. Returns the length written (0 when nothing remains). */
static size_t sv2_track_sanitize(const char *in, size_t n, char *out, size_t cap) {
    size_t a = 0, b = n, o = 0;
    if (cap == 0) return 0;
    while (a < b && in[a] == ' ') a++;
    while (b > a && in[b - 1] == ' ') b--;
    for (size_t i = a; i < b && o < 32 && o + 1 < cap; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            if (o > 0 && out[o - 1] != '-') out[o++] = '-';
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            out[o++] = (char)c;
        }
    }
    while (o > 0 && (out[o - 1] == '-' || out[o - 1] == '_')) o--;
    while (o > 0 && out[0] == '-') { memmove(out, out + 1, --o); }
    out[o] = 0;
    return o;
}

/* Rules 1-8 of 3.4. track gets "Unknown" when nothing usable remains; year is 0 when invalid. */
static void sv2_aiw_parse(const char *p, size_t plen, char *track, size_t cap, int *year) {
    size_t n = plen, end, base_start = 0, base_len, parent_start = 0, parent_len = 0, tsrc_at = 0, tsrc_len = 0;
    int base_year = 0, parent_year = 0, ok = 0;
    int base_match, parent_match;
    size_t parent_at = 0, ptrack_at = 0;
    *year = 0;
    sv2_copy(track, cap, "Unknown");
    if (!p || n == 0 || n > 259 || n < 4) return;
    if (!(p[n - 4] == '.' && (p[n - 3] | 0x20) == 'a' && (p[n - 2] | 0x20) == 'i' && (p[n - 1] | 0x20) == 'w')) return;
    end = n - 4;
    for (size_t i = end; i > 0; i--) if (sv2_is_sep(p[i - 1])) { base_start = i; break; }
    base_len = end - base_start;
    if (base_start > 0) {
        size_t sep = base_start - 1;
        parent_at = sep;
        for (size_t i = sep; i > 0; i--) if (sv2_is_sep(p[i - 1])) { parent_start = i; break; }
        parent_len = parent_at - parent_start;
    }
    base_match = sv2_year_prefix(p + base_start, base_len, &base_year, &tsrc_at);
    parent_match = base_start > 0 && sv2_year_prefix(p + parent_start, parent_len, &parent_year, &ptrack_at);
    if (base_match) {
        *year = base_year;
        tsrc_len = base_len - tsrc_at;
        tsrc_at = base_start + tsrc_at;
        ok = 1;
    } else if (parent_match && parent_len == base_len && memcmp(p + parent_start, p + base_start, base_len) == 0) {
        /* rule 4 as written: parent folder equal to base. Unreachable when base matches rule 3. */
        *year = parent_year;
        tsrc_at = parent_start + ptrack_at;
        tsrc_len = parent_len - ptrack_at;
        ok = 1;
    }
    if (!ok) return;
    {
        char clean[40];
        size_t k = sv2_track_sanitize(p + tsrc_at, tsrc_len, clean, sizeof clean);
        if (k == 0) { *year = (*year >= 1950 && *year <= 2099) ? *year : 0; return; }
        if (*year < 1950 || *year > 2099) *year = 0;
        sv2_copy(track, cap, clean);
    }
}

/* ---- naming (spec 3.3, 3.5) ---- */
typedef struct { int year, mon, day, hour, min, sec; } sv2_local_t;

/* ---- replay-style names (1.1.1, owner decision A) ----
 * Logic copied from Lightloader replay_naming.c (rn_country_from_aiw_path, rn_mode_token,
 * rn_session_token). Not linked. Session stem and weekend folder:
 *   "<COUNTRY> <YEAR> - <Mode> - <Session> - YYYY-MM-DD HH-MM"   e.g. BRAZIL 1995 - Championship - Race - 2026-10-13 16-14
 *   weekend folder: "<COUNTRY> <YEAR> - <Mode> - YYYY-MM-DD HH-MM" (weekend start)
 * An empty mode, a year of 0 and a NULL session are omitted. */
static int sv2_lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int sv2_is_pathsep(char c) { return c == '\\' || c == '/'; }
static size_t sv2_app(char *b, size_t cap, size_t len, const char *fmt, ...) {
    va_list ap;
    int k;
    if (len >= cap) return 0;
    va_start(ap, fmt);
    k = vsnprintf(b + len, cap - len, fmt, ap);
    va_end(ap);
    if (k <= 0 || (size_t)k >= cap - len) return 0;
    return len + (size_t)k;
}
/* Removes characters Windows forbids in names, control and non-ASCII bytes (the API accepts printable ASCII only). */
static void sv2_clean_token(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if ((unsigned char)*p < 0x20 || (unsigned char)*p >= 0x80) continue;
        if (strchr("\\/:*?\"<>|", *p)) continue;
        *o++ = *p;
    }
    *o = 0;
}
static int sv2_folder_is(const char *s, size_t len, const char *name) {
    size_t i;
    if (strlen(name) != len) return 0;
    for (i = 0; i < len; i++) if (sv2_lc((unsigned char)s[i]) != sv2_lc((unsigned char)name[i])) return 0;
    return 1;
}
/* Country from the folder after "Circuits\" (Classics: its track folder), '_' -> ' ', upper case.
 * Aliases: Adelaide AUSTRALIA, Aida JAPAN, Estoril PORTUGAL, Jerez SPAIN. Returns 0 when none. */
static int sv2_country_from_aiw(const char *path, char *out, size_t cap) {
    static const char key[] = "circuits";
    static const struct { const char *folder, *country; } alias[] = {
        {"Adelaide", "AUSTRALIA"}, {"Aida", "JAPAN"}, {"Estoril", "PORTUGAL"}, {"Jerez", "SPAIN"}};
    const char *p, *s = NULL, *e, *s2, *e2;
    size_t i, n = 0, len;
    if (!path || !out || cap == 0) return 0;
    out[0] = 0;
    for (p = path; *p && !s; p++) {
        int m = (p == path || sv2_is_pathsep(p[-1]));
        for (i = 0; m && i < 8; i++) if (sv2_lc((unsigned char)p[i]) != key[i]) m = 0;
        if (m && sv2_is_pathsep(p[8])) s = p + 9;
    }
    if (!s) return 0;
    e = s;
    while (*e && !sv2_is_pathsep(*e)) e++;
    if (e == s || *e == 0) return 0;
    len = (size_t)(e - s);
    if (sv2_folder_is(s, len, "classics")) {
        s2 = e + 1;
        e2 = s2;
        while (*e2 && !sv2_is_pathsep(*e2)) e2++;
        if (e2 != s2 && *e2 != 0) { s = s2; e = e2; len = (size_t)(e - s); }
    }
    for (i = 0; i < sizeof alias / sizeof alias[0]; i++) {
        if (sv2_folder_is(s, len, alias[i].folder)) {
            if (strlen(alias[i].country) + 1 > cap) return 0;
            strcpy(out, alias[i].country);
            return 1;
        }
    }
    for (p = s; p < e; p++) {
        char c = *p == '_' ? ' ' : (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
        if (n + 1 >= cap) return 0;
        out[n++] = c;
    }
    out[n] = 0;
    sv2_clean_token(out);
    return n > 0;
}
/* Mode from the game mode global (0x7D0410) and the multiplayer session kind (2 = CO-OP). Empty when unknown. */
static int sv2_mode_token(int game_mode, int mp_kind, char *out, size_t cap) {
    const char *label;
    switch (game_mode) {
    case 1: label = "Test Day"; break;
    case 2:
    case 3: label = "Race"; break;
    case 4: label = "Championship"; break;
    case 5: label = mp_kind == 2 ? "CO-OP" : "Multiplayer"; break;
    default: label = ""; break;
    }
    if (strlen(label) + 1 > cap) return 0;
    strcpy(out, label);
    return 1;
}
/* Session label from the session type 0..7 (Test Day, Practice 1-4, Qualifying, Warmup, Race); else S<n>. */
static int sv2_session_token(int session_type, char *out, size_t cap) {
    static const char *const labels[] = {"Test Day", "Practice 1", "Practice 2", "Practice 3", "Practice 4",
                                         "Qualifying", "Warmup", "Race"};
    int k;
    if (cap == 0) return 0;
    if (session_type >= 0 && session_type <= 7) k = snprintf(out, cap, "%s", labels[session_type]);
    else k = snprintf(out, cap, "S%d", session_type);
    return k > 0 && (size_t)k < cap;
}
static int sv2_name(char *out, size_t cap, const char *country, int year, const char *mode, const char *session,
                    const sv2_local_t *t) {
    size_t n = sv2_app(out, cap, 0, "%s", country && country[0] ? country : "Unknown");
    if (n && year > 0) n = sv2_app(out, cap, n, " %04d", year);
    if (n && mode && mode[0]) n = sv2_app(out, cap, n, " - %s", mode);
    if (n && session && session[0]) n = sv2_app(out, cap, n, " - %s", session);
    if (n) n = sv2_app(out, cap, n, " - %04d-%02d-%02d %02d-%02d", t->year, t->mon, t->day, t->hour, t->min);
    return n > 0;
}
/* Session stem. session NULL = omitted (Test Day mode). */
static int sv2_session_stem(char *out, size_t cap, const char *country, int year, const char *mode, const char *session,
                            const sv2_local_t *t) {
    return sv2_name(out, cap, country, year, mode, session, t);
}
/* Weekend folder name: the same name without the session part. */
static int sv2_weekend_stem(char *out, size_t cap, const char *country, int year, const char *mode, const sv2_local_t *t) {
    return sv2_name(out, cap, country, year, mode, NULL, t);
}

/* "YYYYMMDD-HHMMSS_s<ty>_g<gen>" (3.3). ty -1 becomes 99. Returns 1 if valid (length 8..47, [0-9A-Za-z_-]). */
static int sv2_session_id(char *out, size_t cap, const sv2_local_t *t, int session_type, uint32_t gen) {
    int ty = session_type < 0 ? 99 : session_type;
    int k = snprintf(out, cap, "%04d%02d%02d-%02d%02d%02d_s%d_g%u", t->year, t->mon, t->day, t->hour, t->min, t->sec, ty, gen);
    if (k <= 0 || (size_t)k >= cap) return 0;
    if (k < 8 || k > 47) return 0;
    for (int i = 0; i < k; i++) {
        char c = out[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* Weekend grouping (3.5). Seq starts at 1 per process; new weekend on track or year change,
 * on a Test Day (type 0) after a race with the same key (OQ-4 PROPOSAL, open). */
/* 1.1.1 (T2): the key is country + year + mode, so a Test Day (mode 1) is always its own weekend. */
typedef struct { int seq, have, race_seen; char key[160]; } sv2_weekend_t;
static int sv2_weekend_next(sv2_weekend_t *w, const char *key, int session_type) {
    int fresh = !w->have || strcmp(w->key, key) != 0 || (session_type == 0 && w->race_seen);
    if (fresh) {
        w->seq++;
        w->have = 1;
        w->race_seen = 0;
        sv2_copy(w->key, sizeof w->key, key);
    }
    if (session_type == 7) w->race_seen = 1;
    return w->seq;
}
/* "COUNTRY|YEAR|MODE" (the separator is not allowed in names, so it cannot clash with a name part). */
static int sv2_weekend_key(char *out, size_t cap, const char *country, int year, const char *mode) {
    int k = snprintf(out, cap, "%s|%04d|%s", country ? country : "", year, mode ? mode : "");
    return k > 0 && (size_t)k < cap;
}

/* Collision suffix: stem, stem_2, stem_3 ... (U-9). exists returns 1 when the name is taken.
 * Returns the suffix used (1 for none), or 0 when exhausted. */
typedef int (*sv2_exists_fn)(void *ctx, const char *name);
static int sv2_unique_stem(char *stem, size_t cap, sv2_exists_fn exists, void *ctx) {
    char base[120];
    size_t blen = strlen(stem);
    if (blen + 8 >= sizeof base || blen + 8 > cap) return 0;
    memcpy(base, stem, blen + 1);
    if (!exists(ctx, stem)) return 1;
    for (int k = 2; k <= 9999; k++) {
        snprintf(stem, cap, "%s_%d", base, k);
        if (!exists(ctx, stem)) return k;
    }
    return 0;
}

/* ========================================================================
 * 2.1 B2 INDEX CORE (batch B2, 2026-10-14; spec TELEMETRY_LIVE_REVIEW_SPEC_2026-10-13.md 3.2 and 5 B2)
 * Pure C: no lock, no thread, no HTTP, no file IO. One walk over a file buffer builds
 *   (a) the DATA block index: file offset, seq, tick count, session sim span of each block;
 *   (b) the lap index: completed laps per car. Game lap time comes from the EVENT "lap" record (last_lap_s);
 *       sim-time start and end come from channel 69 (lap number) changes, so a pause does not count.
 * Nothing here is called by capture, the HTTP API or the recording writer yet. Not done in this part of B2:
 * the review worker (R-V1), the incremental index of an open .part (LIVE layer 2), and the recordings routes.
 * ======================================================================== */
#define SV2_IDX_R_CAPACITY 8    /* walk stopped: block index array full (the index is partial, nothing is invented) */
#define SV2_IDX_LAP_TOL 0.002   /* s: EVENT time (%.3f) versus DATA tick sim time (writer rounds to 1 ms) */
#define SV2_CH_LAP 69u          /* T23 lap number, RAW32 integer (two's complement bits in the value array) */

typedef struct {
    uint32_t file_offset, seq, ticks;
    double t_first, t_last; /* session sim seconds of the first and last tick of the block */
} sv2_idx_block_t;

typedef struct {
    uint32_t car, lap;      /* car slot; completed lap number (the lap that ended at end_sim) */
    double end_sim;         /* session sim time of the lap change */
    double start_sim;       /* valid when has_start: sim time at which this lap became current */
    double game_time;       /* game lap time (EVENT last_lap_s), valid when has_game */
    uint8_t has_start, has_game;
    uint32_t samples;       /* R1-B: DATA ticks of this lap (0 when only the EVENT was seen) */
} sv2_idx_lap_t;

typedef struct { uint8_t seen, start_known; uint32_t lap; double start; uint32_t n; /* R1-B: ticks of the current lap */
    uint8_t ctl; /* Claude follow-up (REPLAY 2.1 role): bit 0 channel 98 was 0 at a tick, bit 1 it was 2 */
    /* 1.2.4 (DVOA report 2026-10-17): the car's newest DATA tick in the file (any channel) and the index of its block. The
     * writer stops writing a car that leaves the track (qualifying runs done, retired); the replay holds that last state. */
    uint8_t any; uint32_t last_block; double t_last;
    /* 1.2.5 (DVOA report 2026-10-18): the game time of the car's previous accepted lap EVENT. An in-lap the game does
     * not time leaves channel 86 at the previous lap's value, so 1.2.4 and older files repeat that time. */
    uint8_t ev_have; double ev_game; } sv2_idx_car_t;

typedef struct {
    sv2_idx_block_t *blocks; uint32_t nblocks, cap_blocks;  /* caller arrays */
    sv2_idx_lap_t *laps; uint32_t nlaps, cap_laps;          /* caller arrays */
    sv2_idx_car_t car[SV2_MAX_CARS];
    sv2_channel_t ch[SV2_TABLE_MAX_CHANNELS];               /* channel table of the walked file */
    uint32_t nch, comp_id;
    uint32_t data_blocks, events_lap, lap_rows_dropped, blocks_read; /* blocks_read: instrumentation for B2-4 */
    int monotonic, complete, reason;
    uint32_t reason_seq;
} sv2_idx_t;

static void sv2_idx_init(sv2_idx_t *ix, sv2_idx_block_t *blocks, uint32_t cap_blocks, sv2_idx_lap_t *laps, uint32_t cap_laps) {
    memset(ix, 0, sizeof *ix);
    ix->blocks = blocks; ix->cap_blocks = cap_blocks;
    ix->laps = laps; ix->cap_laps = cap_laps;
}

/* Frames one block at pos: header, bounds, CRC and decompression, with the same rules as sv2_scan. */
static int sv2_frame_at(const uint8_t *buf, size_t len, size_t pos, uint32_t seq, uint32_t comp_id, sv2_block_t *b,
                        const uint8_t **payload, size_t *plen, uint8_t *work, size_t work_cap) {
    const uint8_t *stored;
    size_t avail;
    if (pos > len || len - pos < SV2_BLOCK_HEADER_BYTES) return SV2_R_BLOCK_HEADER;
    if (sv2_block_hdr_decode(buf + pos, SV2_BLOCK_HEADER_BYTES, b) != SV2_OK || b->seq != seq) return SV2_R_BLOCK_HEADER;
    avail = len - pos - SV2_BLOCK_HEADER_BYTES;
    if (b->stored_len > avail) return SV2_R_BLOCK_CRC; /* truncated tail, as sv2_scan reports it */
    stored = buf + pos + SV2_BLOCK_HEADER_BYTES;
    if (sv2_crc32(0, stored, b->stored_len) != b->payload_crc) return SV2_R_BLOCK_CRC;
    if (b->flags & SV2_BLOCK_FLAG_COMPRESSED) {
        size_t got = 0;
        int r = (b->raw_len <= work_cap) ? sv2_decompress_block(comp_id, stored, b->stored_len, work, work_cap, &got) : 0;
        if (r < 0) return SV2_R_DECODER_UNAVAILABLE;
        if (r == 0 || got != b->raw_len) return SV2_R_BLOCK_DECODE;
        *payload = work;
        *plen = got;
    } else {
        *payload = stored;
        *plen = b->stored_len;
    }
    return SV2_OK;
}

/* Lap rows are matched by car, lap and end time (within SV2_IDX_LAP_TOL). The writer flushes the open DATA block
 * before it writes an EVENT, so an EVENT can come before the DATA block that holds the same sample: both orders merge. */
static sv2_idx_lap_t *sv2_idx_lap_put(sv2_idx_t *ix, uint32_t car, uint32_t lap, double end, int from_event, double game,
                                      int has_start, double start) {
    sv2_idx_lap_t *r = NULL;
    for (uint32_t i = 0; i < ix->nlaps; i++) {
        if (ix->laps[i].car == car && ix->laps[i].lap == lap && fabs(ix->laps[i].end_sim - end) <= SV2_IDX_LAP_TOL) {
            r = &ix->laps[i];
            break;
        }
    }
    if (!r) {
        if (ix->nlaps >= ix->cap_laps) { ix->lap_rows_dropped++; return NULL; }
        r = &ix->laps[ix->nlaps++];
        memset(r, 0, sizeof *r);
        r->car = car;
        r->lap = lap;
        r->end_sim = end;
    }
    if (from_event && !r->has_game) { r->has_game = 1; r->game_time = game; }
    if (has_start && !r->has_start) { r->has_start = 1; r->start_sim = start; }
    return r;
}

/* Channel 69 changes per car, read tick by tick from the DATA block (sim time of each tick, not wall clock). */
static void sv2_idx_apply_data(sv2_idx_t *ix, const sv2_block_t *b, const sv2_data_t *d) {
    for (uint32_t c = 0; c < d->n_cars; c++) {
        const sv2_car_t *car = &d->cars[c];
        if (car->slot < SV2_MAX_CARS && car->count > 0 && ix->nblocks > 0) {   /* 1.2.4: newest tick of the car (the caller added the block) */
            double tl = b->t_first + (double)d->sim_ms[car->first_tick + car->count - 1u] / 1000.0;
            sv2_idx_car_t *z = &ix->car[car->slot];
            if (!z->any || tl >= z->t_last) { z->any = 1; z->last_block = ix->nblocks - 1u; z->t_last = tl; }
        }
        if (car->slot < SV2_MAX_CARS && sv2_mask_bit(car->mask_lo, car->mask_hi, 98u)) {   /* role flags of the session */
            uint32_t r98 = sv2_popcount64(car->mask_lo) + sv2_popcount64(car->mask_hi & (((uint64_t)1 << (98u - 64u)) - 1u));
            const uint32_t *v98 = car->vals + (size_t)r98 * car->count;
            for (uint32_t i = 0; i < car->count; i++) {
                if (v98[i] == 0u) ix->car[car->slot].ctl |= 1u;
                else if (v98[i] == 2u) ix->car[car->slot].ctl |= 2u;
            }
        }
        uint32_t rank, count = car->count;
        const uint32_t *v;
        sv2_idx_car_t *s;
        if (car->slot >= SV2_MAX_CARS || !sv2_mask_bit(car->mask_lo, car->mask_hi, SV2_CH_LAP)) continue;
        /* values are channel-major in ascending id: lo ids 0..63 are all below 69, hi ids 64..68 are bits 0..4 */
        rank = sv2_popcount64(car->mask_lo) + sv2_popcount64(car->mask_hi & (((uint64_t)1 << (SV2_CH_LAP - 64u)) - 1u));
        v = car->vals + (size_t)rank * count;
        s = &ix->car[car->slot];
        for (uint32_t i = 0; i < count; i++) {
            uint32_t tick = (uint32_t)car->first_tick + i;
            double t = b->t_first + (double)d->sim_ms[tick] / 1000.0;
            uint32_t lap = v[i];
            if ((int32_t)lap < 0) continue;
            if (!s->seen) { s->seen = 1; s->lap = lap; s->start_known = 0; s->start = t; s->n = 1; continue; }
            if (lap != s->lap) {
                sv2_idx_lap_t *row = sv2_idx_lap_put(ix, car->slot, s->lap, t, 0, 0.0, s->start_known, s->start);
                if (row && !row->samples) row->samples = s->n;   /* R1-B: ticks of the lap that just ended */
                s->lap = lap;
                s->start = t;
                s->start_known = 1;
                s->n = 1;
            } else {
                s->n++;
            }
        }
    }
}

/* Reads a numeric JSON member written by the writer ("key":number). Returns 0 when absent or not finite. */
static int sv2_json_num(const char *j, const char *key, double *out) {
    char pat[32];
    const char *p;
    char *end = NULL;
    double v;
    snprintf(pat, sizeof pat, "\"%s\":", key);
    p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    v = strtod(p, &end);
    if (end == p || !isfinite(v)) return 0;
    *out = v;
    return 1;
}

/* EVENT lap: {"type":"lap","t":T,"car":C,"lap":N,"prev_lap":P,"last_lap_s":G}. The completed lap is prev_lap. */
static void sv2_idx_apply_event(sv2_idx_t *ix, const uint8_t *payload, size_t plen) {
    char js[SV2_EVENT_MAX + 1];
    double car_d, prev_d, t, game;
    if (plen == 0 || plen > SV2_EVENT_MAX) return;
    memcpy(js, payload, plen);
    js[plen] = '\0';
    if (!strstr(js, "\"type\":\"lap\"")) return;
    if (!sv2_json_num(js, "car", &car_d) || !sv2_json_num(js, "prev_lap", &prev_d) ||
        !sv2_json_num(js, "t", &t) || !sv2_json_num(js, "last_lap_s", &game)) return;
    if (car_d < 0.0 || car_d >= (double)SV2_MAX_CARS || prev_d < 0.0 || prev_d > 4294967295.0 || game < 0.0) return;
    {   /* 1.2.5: a repeated game time is the previous lap's value, not this lap's: the lap stays untimed */
        sv2_idx_car_t *ec = &ix->car[(uint32_t)car_d];
        if (ec->ev_have && fabs(game - ec->ev_game) < 0.0005) return;   /* times are written with 3 decimals */
        ec->ev_have = 1;
        ec->ev_game = game;
    }
    sv2_idx_lap_put(ix, (uint32_t)car_d, (uint32_t)prev_d, t, 1, game, 0, 0.0);
    ix->events_lap++;
}

/* One walk over the file. Stops at END (complete = 1), at the first bad block (reason and seq set), or at the
 * committed end of an open file (complete = 0, reason SV2_OK). d->pool and d->pool_cap must be set by the caller.
 * Returns ix->reason. */
static int sv2_index_build(const uint8_t *buf, size_t len, sv2_data_t *d, uint8_t *work, size_t work_cap, sv2_idx_t *ix) {
    sv2_header_t h;
    size_t cto, ctb, pos;
    uint32_t seq = 0;
    double prev_t = -1e300;
    ix->nblocks = ix->nlaps = 0;
    ix->data_blocks = ix->events_lap = ix->lap_rows_dropped = ix->blocks_read = 0;
    ix->monotonic = 1;
    ix->complete = 0;
    ix->reason = SV2_OK;
    ix->reason_seq = 0;
    memset(ix->car, 0, sizeof ix->car);
    {
        int hr = sv2_header_decode(buf, len, &h);
        if (hr != SV2_OK) { ix->reason = hr; return hr; }
    }
    cto = h.channel_table_offset;
    ctb = h.channel_table_bytes;
    if (cto < SV2_HEADER_BYTES || ctb < 8 || cto > len || ctb > len - cto || h.channels_declared == 0 ||
        h.channels_declared > SV2_TABLE_MAX_CHANNELS || sv2_table_decode(buf + cto, ctb, h.channels_declared, ix->ch) != SV2_OK) {
        ix->reason = SV2_R_TABLE;
        return ix->reason;
    }
    if (!(h.flags & SV2_FLAG_HAS_SIM)) { ix->reason = SV2_R_HEADER; return ix->reason; }
    ix->nch = h.channels_declared;
    ix->comp_id = h.compression_id;
    pos = cto + ctb;
    while (pos < len) {
        sv2_block_t b;
        const uint8_t *payload = NULL;
        size_t plen = 0;
        int r = sv2_frame_at(buf, len, pos, seq, h.compression_id, &b, &payload, &plen, work, work_cap);
        if (r != SV2_OK) { ix->reason = r; ix->reason_seq = seq; return r; }
        if (b.type == SV2_BLOCK_DATA) {
            sv2_idx_block_t *ib;
            if (ix->nblocks >= ix->cap_blocks) { ix->reason = SV2_IDX_R_CAPACITY; ix->reason_seq = seq; return ix->reason; }
            if (sv2_data_decode(payload, plen, ix->ch, ix->nch, d) != SV2_OK || d->n_ticks != b.n_ticks) {
                ix->reason = SV2_R_BLOCK_DECODE; ix->reason_seq = seq; return ix->reason;
            }
            ib = &ix->blocks[ix->nblocks++];
            ib->file_offset = (uint32_t)pos;
            ib->seq = seq;
            ib->ticks = b.n_ticks;
            ib->t_first = b.t_first;
            ib->t_last = b.t_first + (double)d->sim_ms[b.n_ticks - 1] / 1000.0;
            if (b.t_first < prev_t) ix->monotonic = 0;
            prev_t = b.t_first;
            ix->data_blocks++;
            sv2_idx_apply_data(ix, &b, d);
        } else if (b.type == SV2_BLOCK_EVENT) {
            if (b.flags != 0 || !isfinite(b.t_first) || !sv2_event_ok(payload, plen)) {
                ix->reason = SV2_R_BLOCK_DECODE; ix->reason_seq = seq; return ix->reason;
            }
            sv2_idx_apply_event(ix, payload, plen);
        } else if (b.type == SV2_BLOCK_END) {
            ix->complete = 1;
            pos += SV2_BLOCK_HEADER_BYTES + (size_t)b.stored_len;
            if (pos < len) { ix->reason = SV2_R_BLOCK_HEADER; ix->reason_seq = seq + 1; }
            return ix->reason;
        }
        /* META and GAP carry nothing the index needs (GAP is validated by sv2_scan, not here). */
        pos += SV2_BLOCK_HEADER_BYTES + (size_t)b.stored_len;
        seq++;
    }
    return ix->reason;
}

/* ---- R1-A file-backed reader (2026-10-14). This file stays free of file IO: the caller supplies read_at (storage_api.inc
 * uses ReadFile). Same validation as sv2_frame_at and sv2_index_build, one block at a time, never the whole file. ---- */
#define SV2_TABLE_READ_MAX 16384u   /* channel table cap of the file reader (the table is about 2 KB) */

typedef struct {
    int (*read_at)(void *ctx, uint64_t off, uint8_t *dst, size_t n); /* returns 0 only when exactly n bytes were read */
    void *ctx;
    uint64_t len;                                                    /* file length in bytes */
} sv2_rd_t;

/* CRC and decompression of one block whose stored bytes are in stored (the rules of sv2_frame_at). */
static int sv2_frame_finish(const sv2_block_t *b, const uint8_t *stored, uint32_t comp_id, const uint8_t **payload,
                            size_t *plen, uint8_t *work, size_t work_cap) {
    if (sv2_crc32(0, stored, b->stored_len) != b->payload_crc) return SV2_R_BLOCK_CRC;
    if (b->flags & SV2_BLOCK_FLAG_COMPRESSED) {
        size_t got = 0;
        int r = (b->raw_len <= work_cap) ? sv2_decompress_block(comp_id, stored, b->stored_len, work, work_cap, &got) : 0;
        if (r < 0) return SV2_R_DECODER_UNAVAILABLE;
        if (r == 0 || got != b->raw_len) return SV2_R_BLOCK_DECODE;
        *payload = work;
        *plen = got;
    } else {
        *payload = stored;
        *plen = b->stored_len;
    }
    return SV2_OK;
}

/* Frames one block at file offset pos: 40 header bytes, then at most SV2_MAX_PAYLOAD stored bytes into stored. */
static int sv2_rd_frame(const sv2_rd_t *rd, uint64_t pos, uint32_t seq, uint32_t comp_id, sv2_block_t *b,
                        const uint8_t **payload, size_t *plen, uint8_t *stored, uint8_t *work, size_t work_cap) {
    uint8_t hdr[SV2_BLOCK_HEADER_BYTES];
    uint64_t avail;
    if (pos > rd->len || rd->len - pos < SV2_BLOCK_HEADER_BYTES) return SV2_R_BLOCK_HEADER;
    if (rd->read_at(rd->ctx, pos, hdr, SV2_BLOCK_HEADER_BYTES) != 0) return SV2_R_BLOCK_HEADER;
    if (sv2_block_hdr_decode(hdr, SV2_BLOCK_HEADER_BYTES, b) != SV2_OK || b->seq != seq) return SV2_R_BLOCK_HEADER;
    avail = rd->len - pos - SV2_BLOCK_HEADER_BYTES;
    if ((uint64_t)b->stored_len > avail) return SV2_R_BLOCK_CRC;   /* truncated tail, as sv2_frame_at reports it */
    if (b->stored_len > SV2_MAX_PAYLOAD) return SV2_R_BLOCK_HEADER;
    if (b->stored_len && rd->read_at(rd->ctx, pos + SV2_BLOCK_HEADER_BYTES, stored, b->stored_len) != 0) return SV2_R_BLOCK_CRC;
    return sv2_frame_finish(b, stored, comp_id, payload, plen, work, work_cap);
}

static int sv2_rd_header(const sv2_rd_t *rd, sv2_header_t *h) {
    uint8_t hb[SV2_HEADER_BYTES];
    if (rd->len < SV2_HEADER_BYTES || rd->read_at(rd->ctx, 0, hb, SV2_HEADER_BYTES) != 0) return SV2_R_HEADER;
    return sv2_header_decode(hb, SV2_HEADER_BYTES, h);
}

/* sv2_index_build on the file reader: same walk, same rules and same reason codes. The header and channel table are
 * read once; each DATA block is read on its own. stored must hold SV2_MAX_PAYLOAD bytes, work holds work_cap bytes,
 * and d->pool is caller sized. Returns ix->reason. */
static int sv2_index_build_rd(const sv2_rd_t *rd, sv2_data_t *d, uint8_t *stored, uint8_t *work, size_t work_cap, sv2_idx_t *ix) {
    sv2_header_t h;
    uint64_t cto, ctb, pos;
    uint32_t seq = 0;
    double prev_t = -1e300;
    ix->nblocks = ix->nlaps = 0;
    ix->data_blocks = ix->events_lap = ix->lap_rows_dropped = ix->blocks_read = 0;
    ix->monotonic = 1;
    ix->complete = 0;
    ix->reason = SV2_OK;
    ix->reason_seq = 0;
    memset(ix->car, 0, sizeof ix->car);
    {
        int hr = sv2_rd_header(rd, &h);
        if (hr != SV2_OK) { ix->reason = hr; return hr; }
    }
    cto = h.channel_table_offset;
    ctb = h.channel_table_bytes;
    if (cto < SV2_HEADER_BYTES || ctb < 8 || ctb > SV2_TABLE_READ_MAX || cto > rd->len || ctb > rd->len - cto ||
        h.channels_declared == 0 || h.channels_declared > SV2_TABLE_MAX_CHANNELS ||
        rd->read_at(rd->ctx, cto, stored, (size_t)ctb) != 0 ||
        sv2_table_decode(stored, (size_t)ctb, h.channels_declared, ix->ch) != SV2_OK) {
        ix->reason = SV2_R_TABLE;
        return ix->reason;
    }
    if (!(h.flags & SV2_FLAG_HAS_SIM)) { ix->reason = SV2_R_HEADER; return ix->reason; }
    ix->nch = h.channels_declared;
    ix->comp_id = h.compression_id;
    pos = cto + ctb;
    while (pos < rd->len) {
        sv2_block_t b;
        const uint8_t *payload = NULL;
        size_t plen = 0;
        int r = sv2_rd_frame(rd, pos, seq, h.compression_id, &b, &payload, &plen, stored, work, work_cap);
        if (r != SV2_OK) { ix->reason = r; ix->reason_seq = seq; return r; }
        if (b.type == SV2_BLOCK_DATA) {
            sv2_idx_block_t *ib;
            if (ix->nblocks >= ix->cap_blocks) { ix->reason = SV2_IDX_R_CAPACITY; ix->reason_seq = seq; return ix->reason; }
            if (sv2_data_decode(payload, plen, ix->ch, ix->nch, d) != SV2_OK || d->n_ticks != b.n_ticks) {
                ix->reason = SV2_R_BLOCK_DECODE; ix->reason_seq = seq; return ix->reason;
            }
            ib = &ix->blocks[ix->nblocks++];
            ib->file_offset = (uint32_t)pos;
            ib->seq = seq;
            ib->ticks = b.n_ticks;
            ib->t_first = b.t_first;
            ib->t_last = b.t_first + (double)d->sim_ms[b.n_ticks - 1] / 1000.0;
            if (b.t_first < prev_t) ix->monotonic = 0;
            prev_t = b.t_first;
            ix->data_blocks++;
            sv2_idx_apply_data(ix, &b, d);
        } else if (b.type == SV2_BLOCK_EVENT) {
            if (b.flags != 0 || !isfinite(b.t_first) || !sv2_event_ok(payload, plen)) {
                ix->reason = SV2_R_BLOCK_DECODE; ix->reason_seq = seq; return ix->reason;
            }
            sv2_idx_apply_event(ix, payload, plen);
        } else if (b.type == SV2_BLOCK_END) {
            ix->complete = 1;
            pos += SV2_BLOCK_HEADER_BYTES + (uint64_t)b.stored_len;
            if (pos < rd->len) { ix->reason = SV2_R_BLOCK_HEADER; ix->reason_seq = seq + 1; }
            return ix->reason;
        }
        pos += SV2_BLOCK_HEADER_BYTES + (uint64_t)b.stored_len;
        seq++;
    }
    return ix->reason;
}

/* sv2_idx_read_block on the file reader: one indexed DATA block, read on its own (blocks_read counts it). */
static int sv2_idx_read_block_rd(const sv2_rd_t *rd, sv2_idx_t *ix, uint32_t i, sv2_data_t *d, uint8_t *stored,
                                 uint8_t *work, size_t work_cap) {
    sv2_block_t b;
    const uint8_t *payload = NULL;
    size_t plen = 0;
    int r;
    if (i >= ix->nblocks) return SV2_R_BLOCK_HEADER;
    r = sv2_rd_frame(rd, ix->blocks[i].file_offset, ix->blocks[i].seq, ix->comp_id, &b, &payload, &plen, stored, work, work_cap);
    if (r != SV2_OK) return r;
    ix->blocks_read++;
    if (b.type != SV2_BLOCK_DATA || sv2_data_decode(payload, plen, ix->ch, ix->nch, d) != SV2_OK || d->n_ticks != b.n_ticks)
        return SV2_R_BLOCK_DECODE;
    return SV2_OK;
}

/* Decodes one indexed DATA block. Counts every block read (blocks_read), so a test can prove that a query touched
 * only the blocks it needed. */
static int sv2_idx_read_block(const uint8_t *buf, size_t len, sv2_idx_t *ix, uint32_t i, sv2_data_t *d,
                              uint8_t *work, size_t work_cap) {
    sv2_block_t b;
    const uint8_t *payload = NULL;
    size_t plen = 0;
    int r;
    if (i >= ix->nblocks) return SV2_R_BLOCK_HEADER;
    r = sv2_frame_at(buf, len, ix->blocks[i].file_offset, ix->blocks[i].seq, ix->comp_id, &b, &payload, &plen, work, work_cap);
    if (r != SV2_OK) return r;
    ix->blocks_read++;
    if (b.type != SV2_BLOCK_DATA || sv2_data_decode(payload, plen, ix->ch, ix->nch, d) != SV2_OK || d->n_ticks != b.n_ticks)
        return SV2_R_BLOCK_DECODE;
    return SV2_OK;
}

/* REPLAY 2.1 / TAPE 2.2: calls fn for every EVENT block in file order (sim time t = t_first). Only the 40-byte
 * headers of the other blocks are read; an EVENT gets the full CRC and decompression check (sv2_rd_frame). Returns
 * SV2_OK at END (or at the committed end of an open file), or the first reason. stored: SV2_MAX_PAYLOAD bytes. */
typedef int (*sv2_event_fn)(void *ctx, double t, const uint8_t *payload, size_t plen);
static int sv2_event_walk_rd(const sv2_rd_t *rd, sv2_event_fn fn, void *ctx, uint8_t *stored, uint8_t *work, size_t work_cap) {
    sv2_header_t h;
    uint64_t cto, ctb, pos;
    uint32_t seq = 0;
    int hr = sv2_rd_header(rd, &h);
    if (hr != SV2_OK) return hr;
    cto = h.channel_table_offset;
    ctb = h.channel_table_bytes;
    if (cto < SV2_HEADER_BYTES || ctb < 8 || ctb > SV2_TABLE_READ_MAX || cto > rd->len || ctb > rd->len - cto) return SV2_R_TABLE;
    pos = cto + ctb;
    while (pos < rd->len) {
        uint8_t hb[SV2_BLOCK_HEADER_BYTES];
        sv2_block_t b;
        const uint8_t *payload = NULL;
        size_t plen = 0;
        int r;
        if (rd->len - pos < SV2_BLOCK_HEADER_BYTES || rd->read_at(rd->ctx, pos, hb, SV2_BLOCK_HEADER_BYTES) != 0)
            return SV2_R_BLOCK_HEADER;
        if (sv2_block_hdr_decode(hb, SV2_BLOCK_HEADER_BYTES, &b) != SV2_OK || b.seq != seq) return SV2_R_BLOCK_HEADER;
        if (b.type == SV2_BLOCK_END) return SV2_OK;
        if (b.type == SV2_BLOCK_EVENT) {
            r = sv2_rd_frame(rd, pos, seq, h.compression_id, &b, &payload, &plen, stored, work, work_cap);
            if (r != SV2_OK) return r;
            r = fn(ctx, b.t_first, payload, plen);
            if (r != SV2_OK) return r;
        }
        if ((uint64_t)b.stored_len > rd->len - pos - SV2_BLOCK_HEADER_BYTES) return SV2_R_BLOCK_CRC;
        pos += SV2_BLOCK_HEADER_BYTES + (uint64_t)b.stored_len;
        seq++;
    }
    return SV2_OK;
}

/* Blocks whose sim span overlaps [t0, t1] (inclusive). Returns the number; *first gets the first index. */
static uint32_t sv2_idx_span(const sv2_idx_t *ix, double t0, double t1, uint32_t *first) {
    uint32_t n = 0;
    if (first) *first = 0;
    for (uint32_t i = 0; i < ix->nblocks; i++) {
        if (ix->blocks[i].t_last < t0 || ix->blocks[i].t_first > t1) continue;
        if (!n && first) *first = i;
        n++;
    }
    return n;
}

/* 1.2.5 (DVOA report 2026-10-18): a lap row the review lists. Timed: start seen and the game's lap time. Untimed: start
 * seen and at least SV2_UNTIMED_MIN_S of sim time, no game time (out lap, in lap, a lap the game did not time). 1.2.4
 * and older listed only timed laps, so those laps had no telemetry in PAST. A shorter row is a lap counter change in
 * the garage or a reset, not a lap. */
#define SV2_UNTIMED_MIN_S 20.0
static int sv2_idx_lap_listed(const sv2_idx_lap_t *r) {
    return r->has_start && (r->has_game || r->end_sim - r->start_sim >= SV2_UNTIMED_MIN_S);
}

static const sv2_idx_lap_t *sv2_idx_lap_find(const sv2_idx_t *ix, uint32_t car, uint32_t lap) {
    for (uint32_t i = 0; i < ix->nlaps; i++)
        if (ix->laps[i].car == car && ix->laps[i].lap == lap) return &ix->laps[i];
    return NULL;
}

/* Lap time for the list: the game time when an EVENT matched (preferred), else the sim-time difference (derived).
 * Returns 0 when the index has neither. *from_game is 1 for the game time. */
static int sv2_idx_lap_time(const sv2_idx_lap_t *r, double *out, int *from_game) {
    if (r->has_game) { *out = r->game_time; *from_game = 1; return 1; }
    if (r->has_start) { *out = r->end_sim - r->start_sim; *from_game = 0; return 1; }
    return 0;
}
