/* legacy_formats.h — STOP 4 preparation: C port of the proven Python legacy
 * decoders (legacy_dossier/tools/legacy_formats.py). NOT WIRED into the product:
 * this file is standalone and is compiled only by the offline test harness.
 *
 * Scope (matches the STOP 3R design, section 6.5):
 *   - .lap  reference lap: checksum line, byte-5 metadata (B1) and weather (B2)
 *     blocks, fixed field block, channel name table, float32 sample region,
 *     byte-7 trailer text block(s).
 *   - .spt game recorder store: header identity, channel descriptors, session
 *     hash words, XOR-encoded .VEH copy (validated, not retained), typed record
 *     stream (player 229 B, environment 25 B, event 5 B), player payload rows.
 *
 * Evidence basis: FORMAT_SPECS_f1_telemetry_2026-10-09.md,
 * tools/legacy_formats.py (parity target), tools/test_legacy_formats.py,
 * findings engine-rpm-live-scale-20261009. All decoding is read-only and
 * fail-closed; the harness never writes to a decoded file.
 *
 * Units: the .lap sample values are raw file floats with the viewer's own
 * channel names; the .spt player payload floats are the writer's own values
 * (Elapsed Time (s) first, Engine RPM already converted by the game writer).
 * Display-side live RPM conversion is lf_rad_s_to_rpm() below — never label a
 * raw rad/s value as RPM.
 */
#ifndef F1C_LEGACY_FORMATS_H
#define F1C_LEGACY_FORMATS_H

#include <stddef.h>
#include <stdint.h>

/* ---- import bounds (STOP 3R section 5.1; stricter than "no bound") ---- */
#define LF_MAX_FILE_BYTES          (64u * 1024u * 1024u)
#define LF_MAX_CHANNELS            512
#define LF_MAX_CHANNEL_NAME        64
#define LF_MAX_SAMPLES             2000000u
#define LF_MAX_SAMPLE_FLOATS       (16u * 1024u * 1024u) /* 64 MB of samples */
#define LF_MAX_TEXT                512
#define LF_MAX_PAIRS               32
#define LF_MAX_PAIR_KEY            32
#define LF_MAX_PAIR_VALUE          96
#define LF_MAX_TRAILER_BLOCKS      8
#define LF_MAX_ID                  128
#define LF_MAX_VEHICLE_PATH        256
#define LF_MAX_EMBEDDED_SECTIONS   32
#define LF_MAX_SPT_PLAYER_RECORDS  131072u /* ~29 MB of 57-float rows */
#define LF_MAX_SPT_ENV             3
#define LF_MAX_SPT_EVENTS          8
#define LF_ERR_CAP                 256

/* writer-proven record sizes (SPT_RECORD_SIZES in the Python decoder) */
#define LF_SPT_SIZE_PLAYER         229u
#define LF_SPT_SIZE_ENV            25u
#define LF_SPT_SIZE_EVENT          5u
#define LF_SPT_FIELDS              57

/* rad/s -> RPM factor: finding engine-rpm-live-scale-20261009.
 * The live /telemetry field c.r is engine angular speed in rad/s for player
 * and AI; any "RPM" label must apply this factor first. The legacy .spt file
 * already stores the writer's converted RPM, so file decoding must not apply
 * the factor again. */
#define LF_RPM_FACTOR 9.549296585513721

static inline double lf_rad_s_to_rpm(double rad_per_s) {
    return rad_per_s * LF_RPM_FACTOR;
}

/* XOR keys proven by FileReadWithXORDecode 0x004D5AF0 (game corpus). */
extern const unsigned char LF_SPT_XOR_KEYS[3];

typedef struct {
    char key[LF_MAX_PAIR_KEY];
    char value[LF_MAX_PAIR_VALUE];
} lf_pair_t;

typedef struct {
    int count;
    int overflow; /* 1 = more pairs than LF_MAX_PAIRS; first ones kept */
    lf_pair_t pairs[LF_MAX_PAIRS];
} lf_pairs_t;

typedef struct {
    long offset;
    uint32_t length;    /* encoded byte length in the file */
    int shift;          /* byte shift that decoded it (5 metadata, 7 trailer) */
    int text_truncated; /* decoded text longer than LF_MAX_TEXT */
    char text[LF_MAX_TEXT];
    lf_pairs_t pairs;
} lf_text_block_t;

typedef struct {
    /* checksum line */
    uint64_t checksum_value;
    char checksum_line[64];

    /* metadata (B1) and weather (B2) shifted text blocks */
    lf_text_block_t metadata;
    lf_text_block_t weather;

    /* fixed 73-byte field block */
    uint32_t field_u32_00;
    uint32_t field_u32_04;
    float field_float_08[5];   /* [0] = lap time candidate (s) */
    uint32_t field_u32_1c;
    float field_float_20;
    float field_float_24;
    uint32_t field_zero_u32[5];
    char field_name[LF_MAX_CHANNEL_NAME + 1];

    /* channel table */
    uint32_t channel_table_marker;
    int channel_count;
    char channel_names[LF_MAX_CHANNELS][LF_MAX_CHANNEL_NAME + 1];

    /* sample region (float32, little-endian host assumed: x86/win32 target) */
    long data_start;
    int stride;         /* channel_count * 4 */
    int sample_count;
    float first_time_s;
    float last_time_s;
    float cadence_s;    /* 0 when sample_count < 2 */
    float *samples;     /* owned: sample_count * channel_count, sample-major */

    /* trailer */
    long trailer_offset;
    long trailer_size;
    int trailer_block_count;
    lf_text_block_t trailer_blocks[LF_MAX_TRAILER_BLOCKS];

    /* lap time cross-check: weather LAPTIME text vs field float */
    int lap_time_from_weather_valid;
    double lap_time_from_weather_s;
    int lap_time_agree; /* 1 agree, 0 disagree, -1 unknown */

    size_t sample_floats;
} lf_lap_t;

typedef struct {
    int code;
    char name[LF_MAX_CHANNEL_NAME + 1];
} lf_spt_channel_t;

typedef struct {
    /* header */
    uint32_t header_u32_00;
    unsigned char header_seed_bytes[4];
    float header_sample_interval_s;
    char driver[LF_MAX_ID];
    char team[LF_MAX_ID];
    char vehicle_path[LF_MAX_VEHICLE_PATH];
    char track[LF_MAX_ID];
    int descriptor_truncated; /* identity string longer than its cap */

    /* descriptors */
    int channel_count;
    lf_spt_channel_t channels[LF_MAX_CHANNELS];
    long descriptor_end;

    /* session hash block and embedded XOR-encoded .VEH copy */
    uint32_t session_hash_words[17];
    uint32_t embedded_size;
    int embedded_ascii_valid;
    char embedded_first_line[LF_MAX_ID];
    int embedded_section_count;

    /* typed record stream */
    long stream_start;
    int type_codes[6];            /* 0x00, 0x01, 0x02, 0x03, 0x04, 0xFF */
    uint32_t type_counts[6];
    long player_records;
    int row_stride;               /* min(channel_count, LF_SPT_FIELDS) */
    float *elapsed;               /* owned: player_records */
    float *rows;                  /* owned: player_records * row_stride */
    float first_elapsed_s;
    float last_elapsed_s;
    float dominant_delta_s;
    float duration_s;
    long stream_bytes;
    long partial_final_record_bytes;

    int env_count;
    uint32_t env_flags[LF_MAX_SPT_ENV];
    float env_floats[LF_MAX_SPT_ENV][5];

    int event_count;
    int event_rtype[LF_MAX_SPT_EVENTS];
    uint32_t event_raw[LF_MAX_SPT_EVENTS];
    int event_has_value[LF_MAX_SPT_EVENTS];
    uint32_t event_value[LF_MAX_SPT_EVENTS];
} lf_spt_t;

/* Decode a .lap file. Returns 0 on success, -1 on refusal (err filled). */
int lf_decode_lap(const char *path, lf_lap_t *out, char *err, size_t err_cap);
/* Decode .lap bytes already in memory (the caller keeps ownership). */
int lf_decode_lap_mem(const unsigned char *data, size_t size, lf_lap_t *out,
                      char *err, size_t err_cap);
void lf_lap_free(lf_lap_t *lap);

/* Decode a .spt file. Returns 0 on success, -1 on refusal (err filled). */
int lf_decode_spt(const char *path, lf_spt_t *out, char *err, size_t err_cap);
/* Decode .spt bytes already in memory (the caller keeps ownership). */
int lf_decode_spt_mem(const unsigned char *data, size_t size, lf_spt_t *out,
                      char *err, size_t err_cap);
void lf_spt_free(lf_spt_t *spt);

/* Lookups used by conversion/tests. Return -1 when absent (exact match). */
int lf_lap_channel_index(const lf_lap_t *lap, const char *name);
const char *lf_pairs_get(const lf_pairs_t *pairs, const char *key); /* last wins */

#endif /* F1C_LEGACY_FORMATS_H */
