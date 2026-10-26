/*
 * pit_failure_policy_v3.c — versioned two-stage policy extension.
 *
 * Version 0.4.0-two-stage.3 (Development build).
 * See src/pit_failure_policy_v3.h for the contract and units. This file is an
 * additive extension: it calls the accepted v2 primitives (splitmix64 RNG,
 * strict numeric parsing, triangular sampling, plan/timing structs) and never
 * changes a v2 decision.
 *
 * Decision order inside one stop (streams are independent, order is fixed):
 *   1. eligibility pre-check (no eligible parent -> skipped, no draws);
 *   2. GATE draw (bypassed when a parent is forced);
 *   3. PARENT draw over eligible weights (bypassed when forced);
 *   4. RESOLUTION draw within the parent (bypassed when forced);
 *   5. DURATION sample (triangular, one per stop);
 *   6. CORNER draw (wheel parents only);
 *   7. FUEL_SUBTYPE draw (fuel parent only);
 *   8. FIRE draw, then FIRE_DURATION samples (fuel parent only).
 */
#include "pit_failure_policy_v3.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------- helpers -- */

static void set_band(pf_duration_band_t *band, double min, double mode,
                     double max)
{
    band->min = min;
    band->mode = mode;
    band->max = max;
}

static int band_valid(const pf_duration_band_t *band)
{
    return isfinite(band->min) && isfinite(band->mode) && isfinite(band->max) &&
           band->min >= PF_SECONDS_MIN && band->max <= PF_SECONDS_MAX &&
           band->min <= band->mode && band->mode <= band->max;
}

static int range_int(const char *name, long long value, long long min,
                     long long max, char *err, size_t err_len)
{
    if (value < min || value > max) {
        if (err && err_len) {
            snprintf(err, err_len, "%s out of range %lld..%lld", name, min, max);
        }
        return 0;
    }
    return 1;
}

static int sum_positive(const int *weights, int count)
{
    int64_t total = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (weights[i] > 0) {
            total += weights[i];
        }
    }
    if (total <= 0) {
        return 0;
    }
    return total > INT_MAX ? INT_MAX : (int)total;
}

static int parent_outcome(int parent)
{
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL: return PF_OUTCOME_SLOW_CORNER;
    case PF3_PARENT_WHEELGUN: return PF_OUTCOME_WHEELGUN;
    case PF3_PARENT_FUEL_RIG: return PF_OUTCOME_FUEL_RIG;
    case PF3_PARENT_STALL: return PF_OUTCOME_STALL_DELAY;
    case PF3_PARENT_CROSS_THREAD: return PF_OUTCOME_CROSS_THREAD;
    default: return PF_OUTCOME_CLEAN;
    }
}

static int parent_job_kind(int parent)
{
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL:
    case PF3_PARENT_WHEELGUN:
    case PF3_PARENT_CROSS_THREAD:
        return PF_JOB_TYRE;
    case PF3_PARENT_FUEL_RIG:
        return PF_JOB_FUEL;
    case PF3_PARENT_STALL:
        return PF_JOB_STALL;
    default:
        return PF_JOB_UNKNOWN;
    }
}

static int parent_uses_wheel(int parent)
{
    return parent == PF3_PARENT_SLOW_WHEEL || parent == PF3_PARENT_WHEELGUN ||
           parent == PF3_PARENT_CROSS_THREAD;
}

static pf_rng_t stream_for(uint64_t seed, const pf_identity_t *id,
                           uint32_t domain)
{
    pf_rng_t rng;
    rng.state = pf_stream_seed(seed, id->epoch, id->vehicle_id,
                               id->service_token, domain);
    return rng;
}

/* ---------------------------------------------------------- defaults ---- */

void pf3_config_defaults(pf3_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));

    cfg->incident_chance_bp = 750;

    cfg->parent_weight[PF3_PARENT_SLOW_WHEEL] = 60;
    cfg->parent_weight[PF3_PARENT_WHEELGUN] = 17;
    cfg->parent_weight[PF3_PARENT_FUEL_RIG] = 17;
    cfg->parent_weight[PF3_PARENT_STALL] = 4;
    cfg->parent_weight[PF3_PARENT_CROSS_THREAD] = 2;

    cfg->resolution_weight[PF3_PARENT_SLOW_WHEEL][PF3_RES_SLOW_WHEEL_MINOR] = 85;
    cfg->resolution_weight[PF3_PARENT_SLOW_WHEEL][PF3_RES_SLOW_WHEEL_AWKWARD] = 15;
    set_band(&cfg->resolution_band[PF3_PARENT_SLOW_WHEEL][PF3_RES_SLOW_WHEEL_MINOR],
             0.8, 1.5, 3.0);
    set_band(&cfg->resolution_band[PF3_PARENT_SLOW_WHEEL][PF3_RES_SLOW_WHEEL_AWKWARD],
             3.0, 4.0, 6.0);

    cfg->resolution_weight[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_QUICK] = 80;
    cfg->resolution_weight[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_BACKUP] = 19;
    cfg->resolution_weight[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_IRREPARABLE] = 1;
    set_band(&cfg->resolution_band[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_QUICK],
             3.0, 5.0, 8.0);
    set_band(&cfg->resolution_band[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_BACKUP],
             8.0, 12.0, 20.0);
    set_band(&cfg->resolution_band[PF3_PARENT_WHEELGUN][PF3_RES_WHEELGUN_IRREPARABLE],
             12.0, 18.0, 30.0);

    cfg->resolution_weight[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_SLOW_FLOW] = 65;
    cfg->resolution_weight[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_TEMPORARY_NO_FLOW] = 20;
    cfg->resolution_weight[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_INTERRUPTED_FLOW] = 10;
    cfg->resolution_weight[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_SPILL] = 5;
    set_band(&cfg->resolution_band[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_SLOW_FLOW],
             3.0, 5.0, 9.0);
    set_band(&cfg->resolution_band[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_TEMPORARY_NO_FLOW],
             6.0, 9.0, 16.0);
    set_band(&cfg->resolution_band[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_INTERRUPTED_FLOW],
             4.0, 7.0, 12.0);
    set_band(&cfg->resolution_band[PF3_PARENT_FUEL_RIG][PF3_RES_FUEL_SPILL],
             3.0, 5.0, 10.0);

    cfg->resolution_weight[PF3_PARENT_STALL][PF3_RES_STALL_STRAIGHTFORWARD] = 85;
    cfg->resolution_weight[PF3_PARENT_STALL][PF3_RES_STALL_DIFFICULT] = 15;
    set_band(&cfg->resolution_band[PF3_PARENT_STALL][PF3_RES_STALL_STRAIGHTFORWARD],
             2.0, 3.0, 5.0);
    set_band(&cfg->resolution_band[PF3_PARENT_STALL][PF3_RES_STALL_DIFFICULT],
             5.0, 8.0, 15.0);

    cfg->resolution_weight[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_CORRECTED] = 70;
    cfg->resolution_weight[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_LENGTHY] = 26;
    cfg->resolution_weight[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_IRREPARABLE] = 4;
    set_band(&cfg->resolution_band[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_CORRECTED],
             6.0, 10.0, 15.0);
    set_band(&cfg->resolution_band[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_LENGTHY],
             15.0, 22.0, 35.0);
    set_band(&cfg->resolution_band[PF3_PARENT_CROSS_THREAD][PF3_RES_CT_IRREPARABLE],
             12.0, 18.0, 30.0);

    cfg->fire_chance_bp = 500;
    set_band(&cfg->fire_burn, 4.0, 7.0, 12.0);
    set_band(&cfg->fire_inspection, 2.0, 4.0, 7.0);

    cfg->corner_weight[PF_WHEEL_FL] = 25;
    cfg->corner_weight[PF_WHEEL_FR] = 25;
    cfg->corner_weight[PF_WHEEL_RL] = 25;
    cfg->corner_weight[PF_WHEEL_RR] = 25;
}

/* -------------------------------------------------------- validation ---- */

static int validate_parent(const pf3_config_t *cfg, int parent, char *err,
                           size_t err_len)
{
    int count = pf3_resolution_count(parent);
    int i;
    int64_t total = 0;

    if (!range_int("parent weight", cfg->parent_weight[parent], 0,
                   PF3_WEIGHT_MAX, err, err_len)) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (!range_int("resolution weight",
                       cfg->resolution_weight[parent][i], 0, PF3_WEIGHT_MAX,
                       err, err_len)) {
            return 0;
        }
        if (!band_valid(&cfg->resolution_band[parent][i])) {
            if (err && err_len) {
                snprintf(err, err_len,
                         "resolution duration band %d/%d is unordered or out "
                         "of 0..60 seconds", parent, i);
            }
            return 0;
        }
        total += cfg->resolution_weight[parent][i];
    }
    if (total <= 0) {
        if (err && err_len) {
            snprintf(err, err_len,
                     "parent %d resolution weights sum to zero", parent);
        }
        return 0;
    }
    return 1;
}

pf_config_status_t pf3_config_validate(pf3_config_t *cfg, char *err,
                                       size_t err_len)
{
    int parent;
    int64_t parent_sum = 0;

    if (!cfg) {
        if (err && err_len) {
            snprintf(err, err_len, "configuration pointer is null");
        }
        return PF_CONFIG_SCHEMA;
    }
    if (!range_int("incident_chance_bp", cfg->incident_chance_bp,
                   PF3_GATE_MIN_BP, PF3_GATE_MAX_BP, err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    if (!range_int("fire_chance_bp", cfg->fire_chance_bp, 0, PF3_FIRE_UNIVERSE,
                   err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    if (!band_valid(&cfg->fire_burn) || !band_valid(&cfg->fire_inspection)) {
        if (err && err_len) {
            snprintf(err, err_len,
                     "fire duration band is unordered or out of 0..60 seconds");
        }
        return PF_CONFIG_RANGE;
    }
    for (parent = 0; parent < PF3_PARENT_COUNT; parent++) {
        int i;
        if (!validate_parent(cfg, parent, err, err_len)) {
            return PF_CONFIG_RANGE;
        }
        for (i = 0; i < PF3_RESOLUTION_MAX; i++) {
            if (i >= pf3_resolution_count(parent)) {
                if (cfg->resolution_weight[parent][i] != 0) {
                    if (err && err_len) {
                        snprintf(err, err_len,
                                 "parent %d has weight for unused resolution %d",
                                 parent, i);
                    }
                    return PF_CONFIG_RANGE;
                }
            }
        }
        parent_sum += cfg->parent_weight[parent];
    }
    if (parent_sum <= 0) {
        if (err && err_len) {
            snprintf(err, err_len, "parent weights sum to zero");
        }
        return PF_CONFIG_RANGE;
    }
    for (parent = 0; parent < PF_WHEEL_COUNT; parent++) {
        if (!range_int("corner weight", cfg->corner_weight[parent], 0,
                       PF3_WEIGHT_MAX, err, err_len)) {
            return PF_CONFIG_RANGE;
        }
    }
    if (sum_positive(cfg->corner_weight, PF_WHEEL_COUNT) <= 0) {
        if (err && err_len) {
            snprintf(err, err_len, "corner weights sum to zero");
        }
        return PF_CONFIG_RANGE;
    }
    return PF_CONFIG_OK;
}

/* ------------------------------------------------------------- parsing -- */

enum {
    SEC_GENERAL = 0,
    SEC_PARENTS,
    SEC_SLOW_WHEEL,
    SEC_WHEELGUN,
    SEC_FUEL,
    SEC_STALL,
    SEC_CROSS_THREAD,
    SEC_CORNER,
    SEC_COUNT
};

typedef struct {
    int section;
    const char *name;
} pf3_key_t;

enum {
    K_SCHEMA = 0,
    K_GATE,
    K_PARENT_SLOW_WHEEL,
    K_PARENT_WHEELGUN,
    K_PARENT_FUEL,
    K_PARENT_STALL,
    K_PARENT_CROSS_THREAD,
    K_SW_MINOR_W,
    K_SW_AWKWARD_W,
    K_SW_MINOR_MIN,
    K_SW_MINOR_MODE,
    K_SW_MINOR_MAX,
    K_SW_AWKWARD_MIN,
    K_SW_AWKWARD_MODE,
    K_SW_AWKWARD_MAX,
    K_WG_QUICK_W,
    K_WG_BACKUP_W,
    K_WG_IRREPARABLE_W,
    K_WG_QUICK_MIN,
    K_WG_QUICK_MODE,
    K_WG_QUICK_MAX,
    K_WG_BACKUP_MIN,
    K_WG_BACKUP_MODE,
    K_WG_BACKUP_MAX,
    K_WG_IRREPARABLE_MIN,
    K_WG_IRREPARABLE_MODE,
    K_WG_IRREPARABLE_MAX,
    K_FUEL_SLOW_W,
    K_FUEL_TEMP_W,
    K_FUEL_INTERRUPTED_W,
    K_FUEL_SPILL_W,
    K_FUEL_SLOW_MIN,
    K_FUEL_SLOW_MODE,
    K_FUEL_SLOW_MAX,
    K_FUEL_TEMP_MIN,
    K_FUEL_TEMP_MODE,
    K_FUEL_TEMP_MAX,
    K_FUEL_INTERRUPTED_MIN,
    K_FUEL_INTERRUPTED_MODE,
    K_FUEL_INTERRUPTED_MAX,
    K_FUEL_SPILL_MIN,
    K_FUEL_SPILL_MODE,
    K_FUEL_SPILL_MAX,
    K_FUEL_FIRE_BP,
    K_FUEL_FIRE_BURN_MIN,
    K_FUEL_FIRE_BURN_MODE,
    K_FUEL_FIRE_BURN_MAX,
    K_FUEL_FIRE_INSP_MIN,
    K_FUEL_FIRE_INSP_MODE,
    K_FUEL_FIRE_INSP_MAX,
    K_STALL_SIMPLE_W,
    K_STALL_DIFFICULT_W,
    K_STALL_SIMPLE_MIN,
    K_STALL_SIMPLE_MODE,
    K_STALL_SIMPLE_MAX,
    K_STALL_DIFFICULT_MIN,
    K_STALL_DIFFICULT_MODE,
    K_STALL_DIFFICULT_MAX,
    K_CT_CORRECTED_W,
    K_CT_LENGTHY_W,
    K_CT_IRREPARABLE_W,
    K_CT_CORRECTED_MIN,
    K_CT_CORRECTED_MODE,
    K_CT_CORRECTED_MAX,
    K_CT_LENGTHY_MIN,
    K_CT_LENGTHY_MODE,
    K_CT_LENGTHY_MAX,
    K_CT_IRREPARABLE_MIN,
    K_CT_IRREPARABLE_MODE,
    K_CT_IRREPARABLE_MAX,
    K_CORNER_FL,
    K_CORNER_FR,
    K_CORNER_RL,
    K_CORNER_RR,
    K_COUNT
};

static const pf3_key_t PF3_KEYS[K_COUNT] = {
    { SEC_GENERAL, "schema_version" },
    { SEC_GENERAL, "incident_chance_bp" },
    { SEC_PARENTS, "slow_wheel_weight" },
    { SEC_PARENTS, "wheelgun_weight" },
    { SEC_PARENTS, "fuel_rig_weight" },
    { SEC_PARENTS, "stall_weight" },
    { SEC_PARENTS, "cross_thread_weight" },
    { SEC_SLOW_WHEEL, "minor_weight" },
    { SEC_SLOW_WHEEL, "awkward_weight" },
    { SEC_SLOW_WHEEL, "minor_min" },
    { SEC_SLOW_WHEEL, "minor_mode" },
    { SEC_SLOW_WHEEL, "minor_max" },
    { SEC_SLOW_WHEEL, "awkward_min" },
    { SEC_SLOW_WHEEL, "awkward_mode" },
    { SEC_SLOW_WHEEL, "awkward_max" },
    { SEC_WHEELGUN, "quick_weight" },
    { SEC_WHEELGUN, "backup_weight" },
    { SEC_WHEELGUN, "irreparable_weight" },
    { SEC_WHEELGUN, "quick_min" },
    { SEC_WHEELGUN, "quick_mode" },
    { SEC_WHEELGUN, "quick_max" },
    { SEC_WHEELGUN, "backup_min" },
    { SEC_WHEELGUN, "backup_mode" },
    { SEC_WHEELGUN, "backup_max" },
    { SEC_WHEELGUN, "irreparable_min" },
    { SEC_WHEELGUN, "irreparable_mode" },
    { SEC_WHEELGUN, "irreparable_max" },
    { SEC_FUEL, "slow_flow_weight" },
    { SEC_FUEL, "temporary_no_flow_weight" },
    { SEC_FUEL, "interrupted_flow_weight" },
    { SEC_FUEL, "spill_weight" },
    { SEC_FUEL, "slow_flow_min" },
    { SEC_FUEL, "slow_flow_mode" },
    { SEC_FUEL, "slow_flow_max" },
    { SEC_FUEL, "temporary_no_flow_min" },
    { SEC_FUEL, "temporary_no_flow_mode" },
    { SEC_FUEL, "temporary_no_flow_max" },
    { SEC_FUEL, "interrupted_flow_min" },
    { SEC_FUEL, "interrupted_flow_mode" },
    { SEC_FUEL, "interrupted_flow_max" },
    { SEC_FUEL, "spill_min" },
    { SEC_FUEL, "spill_mode" },
    { SEC_FUEL, "spill_max" },
    { SEC_FUEL, "fire_chance_bp" },
    { SEC_FUEL, "fire_burn_min" },
    { SEC_FUEL, "fire_burn_mode" },
    { SEC_FUEL, "fire_burn_max" },
    { SEC_FUEL, "fire_inspection_min" },
    { SEC_FUEL, "fire_inspection_mode" },
    { SEC_FUEL, "fire_inspection_max" },
    { SEC_STALL, "straightforward_weight" },
    { SEC_STALL, "difficult_weight" },
    { SEC_STALL, "straightforward_min" },
    { SEC_STALL, "straightforward_mode" },
    { SEC_STALL, "straightforward_max" },
    { SEC_STALL, "difficult_min" },
    { SEC_STALL, "difficult_mode" },
    { SEC_STALL, "difficult_max" },
    { SEC_CROSS_THREAD, "corrected_weight" },
    { SEC_CROSS_THREAD, "lengthy_weight" },
    { SEC_CROSS_THREAD, "irreparable_weight" },
    { SEC_CROSS_THREAD, "corrected_min" },
    { SEC_CROSS_THREAD, "corrected_mode" },
    { SEC_CROSS_THREAD, "corrected_max" },
    { SEC_CROSS_THREAD, "lengthy_min" },
    { SEC_CROSS_THREAD, "lengthy_mode" },
    { SEC_CROSS_THREAD, "lengthy_max" },
    { SEC_CROSS_THREAD, "irreparable_min" },
    { SEC_CROSS_THREAD, "irreparable_mode" },
    { SEC_CROSS_THREAD, "irreparable_max" },
    { SEC_CORNER, "fl_weight" },
    { SEC_CORNER, "fr_weight" },
    { SEC_CORNER, "rl_weight" },
    { SEC_CORNER, "rr_weight" }
};

static const char *SEC_NAMES[SEC_COUNT] = {
    "general", "parents", "slow_wheel", "wheelgun", "fuel_rig", "stall",
    "cross_thread", "corner"
};

static pf_config_status_t fail(char *err, size_t err_len, int line,
                               const char *fmt, const char *detail)
{
    if (err && err_len) {
        size_t used;
        snprintf(err, err_len, "line %d: ", line);
        used = strlen(err);
        if (used < err_len) {
            snprintf(err + used, err_len - used, fmt, detail);
        }
    }
    return PF_CONFIG_SYNTAX;
}

static int find_section(const char *name)
{
    int i;
    for (i = 0; i < SEC_COUNT; i++) {
        if (strcmp(name, SEC_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_key(int section, const char *name)
{
    int i;
    for (i = 0; i < K_COUNT; i++) {
        if (PF3_KEYS[i].section == section &&
            strcmp(PF3_KEYS[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static char *read_file_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    size_t cap = 8192;
    size_t len = 0;
    char *buffer;

    if (!file) {
        return NULL;
    }
    buffer = (char *)malloc(cap);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    for (;;) {
        size_t got;
        if (len + 2048 + 1 > cap) {
            char *grown;
            cap *= 2;
            grown = (char *)realloc(buffer, cap);
            if (!grown) {
                free(buffer);
                fclose(file);
                return NULL;
            }
            buffer = grown;
        }
        got = fread(buffer + len, 1, 2048, file);
        len += got;
        if (got < 2048) {
            if (ferror(file)) {
                free(buffer);
                fclose(file);
                return NULL;
            }
            break;
        }
    }
    fclose(file);
    buffer[len] = '\0';
    return buffer;
}

static void trim_in_place(char *text)
{
    size_t len;
    char *start = text;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
    len = strlen(text);
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }
}

pf_config_status_t pf3_config_parse(const char *text, pf3_config_t *out,
                                    char *err, size_t err_len)
{
    pf3_config_t pending;
    unsigned char seen[K_COUNT];
    char *copy;
    char *cursor;
    int section = -1;
    int line = 0;
    int i;
    pf_config_status_t status = PF_CONFIG_OK;

    if (!text || !out) {
        if (err && err_len) {
            snprintf(err, err_len, "null configuration input");
        }
        return PF_CONFIG_SCHEMA;
    }
    pf3_config_defaults(&pending);
    memset(seen, 0, sizeof(seen));

    copy = (char *)malloc(strlen(text) + 1);
    if (!copy) {
        if (err && err_len) {
            snprintf(err, err_len, "allocation failure");
        }
        return PF_CONFIG_IO;
    }
    strcpy(copy, text);

    cursor = copy;
    while (cursor && *cursor) {
        char *newline = strchr(cursor, '\n');
        char *raw = cursor;
        char *equals;
        char *value;
        int key;

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = NULL;
        }
        line++;
        if (raw[0] && raw[strlen(raw) - 1] == '\r') {
            raw[strlen(raw) - 1] = '\0';
        }
        trim_in_place(raw);
        if (raw[0] == '\0' || raw[0] == ';' || raw[0] == '#') {
            continue;
        }
        if (raw[0] == '[') {
            char *close = strchr(raw, ']');
            if (!close || close[1] != '\0') {
                status = fail(err, err_len, line, "malformed section header %s",
                              raw);
                break;
            }
            *close = '\0';
            trim_in_place(raw + 1);
            section = find_section(raw + 1);
            if (section < 0) {
                status = fail(err, err_len, line, "unknown section %s", raw + 1);
                break;
            }
            continue;
        }
        equals = strchr(raw, '=');
        if (!equals) {
            status = fail(err, err_len, line, "missing '=' in %s", raw);
            break;
        }
        *equals = '\0';
        value = equals + 1;
        trim_in_place(raw);
        trim_in_place(value);
        if (section < 0) {
            status = fail(err, err_len, line, "key %s before any section", raw);
            break;
        }
        key = find_key(section, raw);
        if (key < 0) {
            status = fail(err, err_len, line, "unknown key %s", raw);
            break;
        }
        if (seen[key]) {
            status = fail(err, err_len, line, "duplicate key %s", raw);
            break;
        }
        seen[key] = 1;

        if (key == K_SCHEMA) {
            uint64_t schema = 0;
            if (!pf_parse_u64_strict(value, &schema)) {
                status = fail(err, err_len, line,
                              "malformed schema_version %s", value);
                break;
            }
            if (schema != PF3_SCHEMA_VERSION) {
                if (err && err_len) {
                    snprintf(err, err_len,
                             "line %d: schema_version %llu is not the v3 "
                             "extension (expected 3)",
                             line, (unsigned long long)schema);
                }
                status = PF_CONFIG_SCHEMA;
                break;
            }
        } else if (strstr(PF3_KEYS[key].name, "_min") ||
                   strstr(PF3_KEYS[key].name, "_mode") ||
                   strstr(PF3_KEYS[key].name, "_max")) {
            double number = 0.0;
            if (!pf_parse_double_strict(value, &number) ||
                number < PF_SECONDS_MIN || number > PF_SECONDS_MAX) {
                status = fail(err, err_len, line,
                              "duration %s out of 0..60 seconds", value);
                break;
            }
            switch (key) {
            case K_SW_MINOR_MIN: pending.resolution_band[0][0].min = number; break;
            case K_SW_MINOR_MODE: pending.resolution_band[0][0].mode = number; break;
            case K_SW_MINOR_MAX: pending.resolution_band[0][0].max = number; break;
            case K_SW_AWKWARD_MIN: pending.resolution_band[0][1].min = number; break;
            case K_SW_AWKWARD_MODE: pending.resolution_band[0][1].mode = number; break;
            case K_SW_AWKWARD_MAX: pending.resolution_band[0][1].max = number; break;
            case K_WG_QUICK_MIN: pending.resolution_band[1][0].min = number; break;
            case K_WG_QUICK_MODE: pending.resolution_band[1][0].mode = number; break;
            case K_WG_QUICK_MAX: pending.resolution_band[1][0].max = number; break;
            case K_WG_BACKUP_MIN: pending.resolution_band[1][1].min = number; break;
            case K_WG_BACKUP_MODE: pending.resolution_band[1][1].mode = number; break;
            case K_WG_BACKUP_MAX: pending.resolution_band[1][1].max = number; break;
            case K_WG_IRREPARABLE_MIN: pending.resolution_band[1][2].min = number; break;
            case K_WG_IRREPARABLE_MODE: pending.resolution_band[1][2].mode = number; break;
            case K_WG_IRREPARABLE_MAX: pending.resolution_band[1][2].max = number; break;
            case K_FUEL_SLOW_MIN: pending.resolution_band[2][0].min = number; break;
            case K_FUEL_SLOW_MODE: pending.resolution_band[2][0].mode = number; break;
            case K_FUEL_SLOW_MAX: pending.resolution_band[2][0].max = number; break;
            case K_FUEL_TEMP_MIN: pending.resolution_band[2][1].min = number; break;
            case K_FUEL_TEMP_MODE: pending.resolution_band[2][1].mode = number; break;
            case K_FUEL_TEMP_MAX: pending.resolution_band[2][1].max = number; break;
            case K_FUEL_INTERRUPTED_MIN: pending.resolution_band[2][2].min = number; break;
            case K_FUEL_INTERRUPTED_MODE: pending.resolution_band[2][2].mode = number; break;
            case K_FUEL_INTERRUPTED_MAX: pending.resolution_band[2][2].max = number; break;
            case K_FUEL_SPILL_MIN: pending.resolution_band[2][3].min = number; break;
            case K_FUEL_SPILL_MODE: pending.resolution_band[2][3].mode = number; break;
            case K_FUEL_SPILL_MAX: pending.resolution_band[2][3].max = number; break;
            case K_FUEL_FIRE_BURN_MIN: pending.fire_burn.min = number; break;
            case K_FUEL_FIRE_BURN_MODE: pending.fire_burn.mode = number; break;
            case K_FUEL_FIRE_BURN_MAX: pending.fire_burn.max = number; break;
            case K_FUEL_FIRE_INSP_MIN: pending.fire_inspection.min = number; break;
            case K_FUEL_FIRE_INSP_MODE: pending.fire_inspection.mode = number; break;
            case K_FUEL_FIRE_INSP_MAX: pending.fire_inspection.max = number; break;
            case K_STALL_SIMPLE_MIN: pending.resolution_band[3][0].min = number; break;
            case K_STALL_SIMPLE_MODE: pending.resolution_band[3][0].mode = number; break;
            case K_STALL_SIMPLE_MAX: pending.resolution_band[3][0].max = number; break;
            case K_STALL_DIFFICULT_MIN: pending.resolution_band[3][1].min = number; break;
            case K_STALL_DIFFICULT_MODE: pending.resolution_band[3][1].mode = number; break;
            case K_STALL_DIFFICULT_MAX: pending.resolution_band[3][1].max = number; break;
            case K_CT_CORRECTED_MIN: pending.resolution_band[4][0].min = number; break;
            case K_CT_CORRECTED_MODE: pending.resolution_band[4][0].mode = number; break;
            case K_CT_CORRECTED_MAX: pending.resolution_band[4][0].max = number; break;
            case K_CT_LENGTHY_MIN: pending.resolution_band[4][1].min = number; break;
            case K_CT_LENGTHY_MODE: pending.resolution_band[4][1].mode = number; break;
            case K_CT_LENGTHY_MAX: pending.resolution_band[4][1].max = number; break;
            case K_CT_IRREPARABLE_MIN: pending.resolution_band[4][2].min = number; break;
            case K_CT_IRREPARABLE_MODE: pending.resolution_band[4][2].mode = number; break;
            case K_CT_IRREPARABLE_MAX: pending.resolution_band[4][2].max = number; break;
            default:
                status = fail(err, err_len, line, "internal key mapping %s",
                              raw);
                break;
            }
            if (status != PF_CONFIG_OK) {
                break;
            }
        } else {
            uint64_t number = 0;
            long long value_ll;
            if (!pf_parse_u64_strict(value, &number) ||
                number > (uint64_t)INT_MAX) {
                status = fail(err, err_len, line, "malformed integer %s",
                              value);
                break;
            }
            value_ll = (long long)number;
            if (!range_int(PF3_KEYS[key].name, value_ll, 0, PF3_WEIGHT_MAX,
                           err, err_len)) {
                status = PF_CONFIG_RANGE;
                break;
            }
            switch (key) {
            case K_GATE:
                if (value_ll < PF3_GATE_MIN_BP || value_ll > PF3_GATE_MAX_BP) {
                    if (err && err_len) {
                        snprintf(err, err_len,
                                 "line %d: incident_chance_bp out of range "
                                 "500..1000", line);
                    }
                    status = PF_CONFIG_RANGE;
                    break;
                }
                pending.incident_chance_bp = (int)value_ll;
                break;
            case K_PARENT_SLOW_WHEEL: pending.parent_weight[0] = (int)value_ll; break;
            case K_PARENT_WHEELGUN: pending.parent_weight[1] = (int)value_ll; break;
            case K_PARENT_FUEL: pending.parent_weight[2] = (int)value_ll; break;
            case K_PARENT_STALL: pending.parent_weight[3] = (int)value_ll; break;
            case K_PARENT_CROSS_THREAD: pending.parent_weight[4] = (int)value_ll; break;
            case K_SW_MINOR_W: pending.resolution_weight[0][0] = (int)value_ll; break;
            case K_SW_AWKWARD_W: pending.resolution_weight[0][1] = (int)value_ll; break;
            case K_WG_QUICK_W: pending.resolution_weight[1][0] = (int)value_ll; break;
            case K_WG_BACKUP_W: pending.resolution_weight[1][1] = (int)value_ll; break;
            case K_WG_IRREPARABLE_W: pending.resolution_weight[1][2] = (int)value_ll; break;
            case K_FUEL_SLOW_W: pending.resolution_weight[2][0] = (int)value_ll; break;
            case K_FUEL_TEMP_W: pending.resolution_weight[2][1] = (int)value_ll; break;
            case K_FUEL_INTERRUPTED_W: pending.resolution_weight[2][2] = (int)value_ll; break;
            case K_FUEL_SPILL_W: pending.resolution_weight[2][3] = (int)value_ll; break;
            case K_FUEL_FIRE_BP:
                if (value_ll > PF3_FIRE_UNIVERSE) {
                    if (err && err_len) {
                        snprintf(err, err_len,
                                 "line %d: fire_chance_bp out of range "
                                 "0..10000", line);
                    }
                    status = PF_CONFIG_RANGE;
                    break;
                }
                pending.fire_chance_bp = (int)value_ll;
                break;
            case K_STALL_SIMPLE_W: pending.resolution_weight[3][0] = (int)value_ll; break;
            case K_STALL_DIFFICULT_W: pending.resolution_weight[3][1] = (int)value_ll; break;
            case K_CT_CORRECTED_W: pending.resolution_weight[4][0] = (int)value_ll; break;
            case K_CT_LENGTHY_W: pending.resolution_weight[4][1] = (int)value_ll; break;
            case K_CT_IRREPARABLE_W: pending.resolution_weight[4][2] = (int)value_ll; break;
            case K_CORNER_FL: pending.corner_weight[PF_WHEEL_FL] = (int)value_ll; break;
            case K_CORNER_FR: pending.corner_weight[PF_WHEEL_FR] = (int)value_ll; break;
            case K_CORNER_RL: pending.corner_weight[PF_WHEEL_RL] = (int)value_ll; break;
            case K_CORNER_RR: pending.corner_weight[PF_WHEEL_RR] = (int)value_ll; break;
            default:
                status = fail(err, err_len, line, "internal key mapping %s",
                              raw);
                break;
            }
            if (status != PF_CONFIG_OK) {
                break;
            }
        }
    }
    free(copy);
    if (status != PF_CONFIG_OK) {
        return status;
    }
    for (i = 0; i < K_COUNT; i++) {
        if (!seen[i]) {
            if (err && err_len) {
                snprintf(err, err_len, "missing key %s in section %s",
                         PF3_KEYS[i].name, SEC_NAMES[PF3_KEYS[i].section]);
            }
            return PF_CONFIG_SCHEMA;
        }
    }
    status = pf3_config_validate(&pending, err, err_len);
    if (status != PF_CONFIG_OK) {
        return status;
    }
    *out = pending;
    return PF_CONFIG_OK;
}

/* -------------------------------------------------------------- render -- */

static size_t append_text(char *buf, size_t buf_len, size_t offset,
                          const char *text)
{
    size_t need = strlen(text);
    if (buf && buf_len > 0 && offset < buf_len) {
        size_t room = buf_len - 1 - offset;
        size_t copy = need < room ? need : room;
        if (copy > 0) {
            memcpy(buf + offset, text, copy);
        }
        buf[offset + copy] = '\0';
    }
    return offset + need;
}

static size_t append_int(char *buf, size_t buf_len, size_t offset, int value)
{
    char number[32];
    snprintf(number, sizeof(number), "%d", value);
    return append_text(buf, buf_len, offset, number);
}

static size_t append_double(char *buf, size_t buf_len, size_t offset,
                            double value)
{
    char number[48];
    snprintf(number, sizeof(number), "%.10g", value);
    return append_text(buf, buf_len, offset, number);
}

static size_t render_band(char *buf, size_t buf_len, size_t offset,
                          const char *name, const pf_duration_band_t *band)
{
    offset = append_text(buf, buf_len, offset, name);
    offset = append_text(buf, buf_len, offset, "_min = ");
    offset = append_double(buf, buf_len, offset, band->min);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = append_text(buf, buf_len, offset, name);
    offset = append_text(buf, buf_len, offset, "_mode = ");
    offset = append_double(buf, buf_len, offset, band->mode);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = append_text(buf, buf_len, offset, name);
    offset = append_text(buf, buf_len, offset, "_max = ");
    offset = append_double(buf, buf_len, offset, band->max);
    offset = append_text(buf, buf_len, offset, "\n");
    return offset;
}

size_t pf3_config_render(const pf3_config_t *cfg, char *buf, size_t buf_len)
{
    size_t offset = 0;

    if (!cfg) {
        if (buf && buf_len > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    offset = append_text(buf, buf_len, offset,
                         "; pit_failure_policy_v3.ini — two-stage preset.\n"
                         "; Development build. Offline extension; the\n"
                         "; accepted v2 module and schema 2 are unchanged.\n\n"
                         "[general]\n"
                         "schema_version = 3\n"
                         "incident_chance_bp = ");
    offset = append_int(buf, buf_len, offset, cfg->incident_chance_bp);
    offset = append_text(buf, buf_len, offset, "\n\n[parents]\n");
    offset = append_text(buf, buf_len, offset, "slow_wheel_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->parent_weight[PF3_PARENT_SLOW_WHEEL]);
    offset = append_text(buf, buf_len, offset, "\nwheelgun_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->parent_weight[PF3_PARENT_WHEELGUN]);
    offset = append_text(buf, buf_len, offset, "\nfuel_rig_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->parent_weight[PF3_PARENT_FUEL_RIG]);
    offset = append_text(buf, buf_len, offset, "\nstall_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->parent_weight[PF3_PARENT_STALL]);
    offset = append_text(buf, buf_len, offset, "\ncross_thread_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->parent_weight[PF3_PARENT_CROSS_THREAD]);
    offset = append_text(buf, buf_len, offset, "\n\n[slow_wheel]\nminor_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[0][0]);
    offset = append_text(buf, buf_len, offset, "\nawkward_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[0][1]);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "minor", &cfg->resolution_band[0][0]);
    offset = render_band(buf, buf_len, offset, "awkward", &cfg->resolution_band[0][1]);
    offset = append_text(buf, buf_len, offset, "\n[wheelgun]\nquick_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[1][0]);
    offset = append_text(buf, buf_len, offset, "\nbackup_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[1][1]);
    offset = append_text(buf, buf_len, offset, "\nirreparable_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[1][2]);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "quick", &cfg->resolution_band[1][0]);
    offset = render_band(buf, buf_len, offset, "backup", &cfg->resolution_band[1][1]);
    offset = render_band(buf, buf_len, offset, "irreparable", &cfg->resolution_band[1][2]);
    offset = append_text(buf, buf_len, offset, "\n[fuel_rig]\nslow_flow_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[2][0]);
    offset = append_text(buf, buf_len, offset, "\ntemporary_no_flow_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[2][1]);
    offset = append_text(buf, buf_len, offset, "\ninterrupted_flow_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[2][2]);
    offset = append_text(buf, buf_len, offset, "\nspill_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[2][3]);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "slow_flow", &cfg->resolution_band[2][0]);
    offset = render_band(buf, buf_len, offset, "temporary_no_flow", &cfg->resolution_band[2][1]);
    offset = render_band(buf, buf_len, offset, "interrupted_flow", &cfg->resolution_band[2][2]);
    offset = render_band(buf, buf_len, offset, "spill", &cfg->resolution_band[2][3]);
    offset = append_text(buf, buf_len, offset, "fire_chance_bp = ");
    offset = append_int(buf, buf_len, offset, cfg->fire_chance_bp);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "fire_burn", &cfg->fire_burn);
    offset = render_band(buf, buf_len, offset, "fire_inspection", &cfg->fire_inspection);
    offset = append_text(buf, buf_len, offset, "\n[stall]\nstraightforward_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[3][0]);
    offset = append_text(buf, buf_len, offset, "\ndifficult_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[3][1]);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "straightforward", &cfg->resolution_band[3][0]);
    offset = render_band(buf, buf_len, offset, "difficult", &cfg->resolution_band[3][1]);
    offset = append_text(buf, buf_len, offset, "\n[cross_thread]\ncorrected_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[4][0]);
    offset = append_text(buf, buf_len, offset, "\nlengthy_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[4][1]);
    offset = append_text(buf, buf_len, offset, "\nirreparable_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->resolution_weight[4][2]);
    offset = append_text(buf, buf_len, offset, "\n");
    offset = render_band(buf, buf_len, offset, "corrected", &cfg->resolution_band[4][0]);
    offset = render_band(buf, buf_len, offset, "lengthy", &cfg->resolution_band[4][1]);
    offset = render_band(buf, buf_len, offset, "irreparable", &cfg->resolution_band[4][2]);
    offset = append_text(buf, buf_len, offset, "\n[corner]\nfl_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->corner_weight[PF_WHEEL_FL]);
    offset = append_text(buf, buf_len, offset, "\nfr_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->corner_weight[PF_WHEEL_FR]);
    offset = append_text(buf, buf_len, offset, "\nrl_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->corner_weight[PF_WHEEL_RL]);
    offset = append_text(buf, buf_len, offset, "\nrr_weight = ");
    offset = append_int(buf, buf_len, offset, cfg->corner_weight[PF_WHEEL_RR]);
    offset = append_text(buf, buf_len, offset, "\n");
    return offset;
}

pf_config_status_t pf3_config_parse_file(const char *path, pf3_config_t *out,
                                         char *err, size_t err_len)
{
    char *text = read_file_text(path);
    pf_config_status_t status;

    if (!text) {
        if (err && err_len) {
            snprintf(err, err_len, "cannot read %s", path);
        }
        return PF_CONFIG_IO;
    }
    status = pf3_config_parse(text, out, err, err_len);
    free(text);
    return status;
}

/* ---------------------------------------------------------------- draws -- */

uint64_t pf3_gate_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return PF3_GATE_UNIVERSE;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_GATE);
    return pf_rng_below(&rng, PF3_GATE_UNIVERSE);
}

uint64_t pf3_parent_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return UINT64_MAX;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_PARENT);
    return pf_rng_next_u64(&rng);
}

uint64_t pf3_resolution_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return UINT64_MAX;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_RESOLUTION);
    return pf_rng_next_u64(&rng);
}

uint64_t pf3_corner_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return UINT64_MAX;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_CORNER);
    return pf_rng_next_u64(&rng);
}

uint64_t pf3_fuel_subtype_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return UINT64_MAX;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_FUEL_SUBTYPE);
    return pf_rng_next_u64(&rng);
}

uint64_t pf3_fire_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return UINT64_MAX;
    }
    rng = stream_for(seed, id, PF3_DOMAIN_FIRE);
    return pf_rng_next_u64(&rng);
}

/* ------------------------------------------------------------- mappings -- */

int pf3_parent_mask(const pf_context_t *ctx)
{
    int mask = 0;
    const pf_eligibility_t *e;
    if (!ctx) {
        return 0;
    }
    e = &ctx->eligibility;
    if (e->tyre_work) {
        mask |= 1 << PF3_PARENT_SLOW_WHEEL;
        mask |= 1 << PF3_PARENT_WHEELGUN;
        mask |= 1 << PF3_PARENT_CROSS_THREAD;
    }
    if (e->fuel_allowed && e->fuel_add_liters > 0) {
        mask |= 1 << PF3_PARENT_FUEL_RIG;
    }
    if (e->real_service_possible && e->release_possible) {
        mask |= 1 << PF3_PARENT_STALL;
    }
    return mask;
}

int pf3_parent_total_weight(const pf3_config_t *cfg, const pf_context_t *ctx)
{
    int mask;
    int total = 0;
    int parent;
    if (!cfg) {
        return 0;
    }
    mask = pf3_parent_mask(ctx);
    for (parent = 0; parent < PF3_PARENT_COUNT; parent++) {
        if ((mask & (1 << parent)) && cfg->parent_weight[parent] > 0) {
            total += cfg->parent_weight[parent];
        }
    }
    return total;
}

int pf3_gate_passes_for_draw(const pf3_config_t *cfg, uint32_t draw)
{
    if (!cfg || draw >= PF3_GATE_UNIVERSE) {
        return 0;
    }
    return (int)draw < cfg->incident_chance_bp;
}

int pf3_parent_for_draw(const pf3_config_t *cfg, const pf_context_t *ctx,
                        uint64_t draw)
{
    int mask;
    int64_t cumulative = 0;
    int parent;
    if (!cfg) {
        return -1;
    }
    mask = pf3_parent_mask(ctx);
    for (parent = 0; parent < PF3_PARENT_COUNT; parent++) {
        if (!(mask & (1 << parent)) || cfg->parent_weight[parent] <= 0) {
            continue;
        }
        cumulative += cfg->parent_weight[parent];
        if ((int64_t)draw < cumulative) {
            return parent;
        }
    }
    return -1;
}

int pf3_resolution_count(int parent)
{
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL: return 2;
    case PF3_PARENT_WHEELGUN: return 3;
    case PF3_PARENT_FUEL_RIG: return 4;
    case PF3_PARENT_STALL: return 2;
    case PF3_PARENT_CROSS_THREAD: return 3;
    default: return 0;
    }
}

int pf3_resolution_weight_sum(const pf3_config_t *cfg, int parent)
{
    int count;
    int total = 0;
    int i;
    if (!cfg || parent < 0 || parent >= PF3_PARENT_COUNT) {
        return 0;
    }
    count = pf3_resolution_count(parent);
    for (i = 0; i < count; i++) {
        if (cfg->resolution_weight[parent][i] > 0) {
            total += cfg->resolution_weight[parent][i];
        }
    }
    return total;
}

int pf3_resolution_for_draw(const pf3_config_t *cfg, int parent, uint64_t draw)
{
    int count;
    int64_t cumulative = 0;
    int i;
    if (!cfg || parent < 0 || parent >= PF3_PARENT_COUNT) {
        return -1;
    }
    count = pf3_resolution_count(parent);
    for (i = 0; i < count; i++) {
        if (cfg->resolution_weight[parent][i] <= 0) {
            continue;
        }
        cumulative += cfg->resolution_weight[parent][i];
        if ((int64_t)draw < cumulative) {
            return i;
        }
    }
    return -1;
}

int pf3_resolution_is_terminal(int parent, int resolution)
{
    return (parent == PF3_PARENT_WHEELGUN &&
            resolution == PF3_RES_WHEELGUN_IRREPARABLE) ||
           (parent == PF3_PARENT_CROSS_THREAD &&
            resolution == PF3_RES_CT_IRREPARABLE);
}

int pf3_resolution_attempts(int parent, int resolution)
{
    if (pf3_resolution_is_terminal(parent, resolution)) {
        return 0;
    }
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL:
        return 1;
    case PF3_PARENT_WHEELGUN:
        return resolution == PF3_RES_WHEELGUN_BACKUP ? 2 : 1;
    case PF3_PARENT_FUEL_RIG:
        return resolution == PF3_RES_FUEL_SPILL ? 1 : 2;
    case PF3_PARENT_STALL:
        return resolution == PF3_RES_STALL_DIFFICULT ? 2 : 1;
    case PF3_PARENT_CROSS_THREAD:
        return resolution == PF3_RES_CT_LENGTHY ? 2 : 1;
    default:
        return 0;
    }
}

int pf3_corner_for_draw(const pf3_config_t *cfg, uint64_t draw)
{
    int64_t cumulative = 0;
    int i;
    if (!cfg) {
        return -1;
    }
    for (i = 0; i < PF_WHEEL_COUNT; i++) {
        if (cfg->corner_weight[i] <= 0) {
            continue;
        }
        cumulative += cfg->corner_weight[i];
        if ((int64_t)draw < cumulative) {
            return i;
        }
    }
    return -1;
}

int pf3_fuel_subtype_for_draw(const pf3_config_t *cfg, uint64_t draw)
{
    int64_t cumulative = 0;
    int i;
    if (!cfg) {
        return -1;
    }
    for (i = 0; i < pf3_resolution_count(PF3_PARENT_FUEL_RIG); i++) {
        if (cfg->resolution_weight[PF3_PARENT_FUEL_RIG][i] <= 0) {
            continue;
        }
        cumulative += cfg->resolution_weight[PF3_PARENT_FUEL_RIG][i];
        if ((int64_t)draw < cumulative) {
            return PF_FUEL_SUBTYPE_SLOW_FLOW + i;
        }
    }
    return -1;
}

int pf3_fire_selected_for_draw(const pf3_config_t *cfg, uint32_t draw)
{
    if (!cfg || draw >= PF3_FIRE_UNIVERSE) {
        return 0;
    }
    return (int)draw < cfg->fire_chance_bp;
}

/* -------------------------------------------------------------- forcing -- */

pf3_forcing_t pf3_forcing_natural(void)
{
    pf3_forcing_t forcing;
    forcing.parent = -1;
    forcing.resolution = -1;
    forcing.fire = -1;
    forcing.fire_supported = 0;
    return forcing;
}

/* Returns 0 when usable, otherwise the PF_PLAN_V3_FORCED_* code. */
static int forcing_error_code(const pf3_config_t *cfg, const pf_context_t *ctx,
                              const pf3_forcing_t *forcing)
{
    int mask;
    if (!forcing) {
        return 0;
    }
    if (forcing->parent < -1 || forcing->parent >= PF3_PARENT_COUNT) {
        return PF_PLAN_V3_FORCED_INELIGIBLE_PARENT;
    }
    if (forcing->fire < -1 || forcing->fire > 1) {
        return PF_PLAN_V3_FORCED_INVALID_FIRE;
    }
    if (forcing->fire_supported != 0 && forcing->fire_supported != 1) {
        return PF_PLAN_V3_FORCED_INVALID_FIRE;
    }
    if (forcing->resolution < -1) {
        /* Strict lower bound: only -1 (natural) or a valid index is legal. */
        return PF_PLAN_V3_FORCED_INVALID_RESOLUTION;
    }
    if (forcing->parent >= 0) {
        mask = pf3_parent_mask(ctx);
        if (!(mask & (1 << forcing->parent)) ||
            cfg->parent_weight[forcing->parent] <= 0) {
            return PF_PLAN_V3_FORCED_INELIGIBLE_PARENT;
        }
        if (forcing->resolution >= 0) {
            int count = pf3_resolution_count(forcing->parent);
            if (forcing->resolution >= count ||
                cfg->resolution_weight[forcing->parent][forcing->resolution] <= 0) {
                return PF_PLAN_V3_FORCED_INVALID_RESOLUTION;
            }
        }
    } else if (forcing->resolution >= 0) {
        return PF_PLAN_V3_FORCED_INVALID_RESOLUTION;
    }
    if (forcing->fire == 1 && forcing->parent != PF3_PARENT_FUEL_RIG) {
        return PF_PLAN_V3_FORCED_INVALID_FIRE;
    }
    return 0;
}

int pf3_forcing_valid(const pf3_config_t *cfg, const pf_context_t *ctx,
                      const pf3_forcing_t *forcing, char *reason,
                      size_t reason_len)
{
    int code = forcing_error_code(cfg, ctx, forcing);
    if (code != 0) {
        if (reason && reason_len) {
            snprintf(reason, reason_len, "%s", pf3_plan_status_name(code));
        }
        return 0;
    }
    return 1;
}

/* -------------------------------------------------------------- planning -- */

pf_outcome_t pf3_plan_build_full(const pf3_config_t *cfg,
                                 const pf_identity_t *id,
                                 const pf_context_t *ctx, uint64_t seed,
                                 const pf3_forcing_t *forcing_caller,
                                 pf3_plan_t *out)
{
    pf3_forcing_t forcing =
        forcing_caller ? *forcing_caller : pf3_forcing_natural();
    int mask;
    int parent = -1;
    int resolution = -1;
    int forcing_error;

    if (!out) {
        return PF_OUTCOME_CLEAN;
    }
    memset(out, 0, sizeof(*out));
    out->outcome = PF_OUTCOME_CLEAN;
    out->severity = PF_CLEAN;
    out->category_index = PF_CATEGORY_COUNT + 1;
    out->affected_job_kind = PF_JOB_UNKNOWN;
    out->wheel = -1;
    out->fuel_subtype = PF_FUEL_SUBTYPE_NONE;
    out->status = PF_PLAN_V3_CLEAN;
    out->gate_draw = -1;
    out->parent = -1;
    out->resolution = -1;
    out->fire_draw = -1;
    out->fire_supported = forcing.fire_supported ? 1 : 0;
    if (!cfg || !id) {
        return out->outcome;
    }
    out->gate_bp = cfg->incident_chance_bp;

    forcing_error = forcing_error_code(cfg, ctx, &forcing);
    if (forcing_error != 0) {
        out->status = forcing_error;
        return out->outcome;
    }
    mask = pf3_parent_mask(ctx);
    if (mask == 0 || pf3_parent_total_weight(cfg, ctx) <= 0) {
        out->status = PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY;
        return out->outcome;
    }

    if (forcing.parent >= 0) {
        /* Forced parent bypasses ONLY the gate and the parent draw. */
        parent = forcing.parent;
        out->gate_passed = 1;
        out->gate_draw = -1;
        out->status = PF_PLAN_V3_INCIDENT;
    } else {
        pf_rng_t gate_rng = stream_for(seed, id, PF3_DOMAIN_GATE);
        uint64_t draw = pf_rng_below(&gate_rng, PF3_GATE_UNIVERSE);
        out->gate_draw = (int)draw;
        out->gate_passed = pf3_gate_passes_for_draw(cfg, (uint32_t)draw);
        if (!out->gate_passed) {
            out->status = PF_PLAN_V3_CLEAN;
            return out->outcome;
        }
        out->status = PF_PLAN_V3_INCIDENT;
        {
            pf_rng_t parent_rng = stream_for(seed, id, PF3_DOMAIN_PARENT);
            int total = pf3_parent_total_weight(cfg, ctx);
            uint64_t pdraw = pf_rng_below(&parent_rng, (uint64_t)total);
            parent = pf3_parent_for_draw(cfg, ctx, pdraw);
        }
        if (parent < 0) {
            out->status = PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY;
            return out->outcome;
        }
    }

    if (forcing.resolution >= 0) {
        resolution = forcing.resolution;
    } else if (parent == PF3_PARENT_FUEL_RIG) {
        /* One authoritative fuel-subtype draw feeds resolution, duration band,
         * retry schedule and narration. FIRE stays on its own stream. */
        int sum = pf3_resolution_weight_sum(cfg, parent);
        pf_rng_t subtype_rng = stream_for(seed, id, PF3_DOMAIN_FUEL_SUBTYPE);
        uint64_t sdraw = pf_rng_below(&subtype_rng, (uint64_t)sum);
        int subtype = pf3_fuel_subtype_for_draw(cfg, sdraw);
        resolution = subtype >= 0 ? subtype - PF_FUEL_SUBTYPE_SLOW_FLOW : -1;
    } else {
        int sum = pf3_resolution_weight_sum(cfg, parent);
        pf_rng_t resolution_rng =
            stream_for(seed, id, PF3_DOMAIN_RESOLUTION);
        uint64_t rdraw = pf_rng_below(&resolution_rng, (uint64_t)sum);
        resolution = pf3_resolution_for_draw(cfg, parent, rdraw);
    }
    if (resolution < 0) {
        out->status = PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY;
        return out->outcome;
    }

    out->parent = parent;
    out->resolution = resolution;
    out->outcome = (pf_outcome_t)parent_outcome(parent);
    out->category_index = parent + 1;
    out->affected_job_kind = parent_job_kind(parent);

    {
        pf_rng_t duration_rng = stream_for(seed, id, PF3_DOMAIN_DURATION);
        if (pf3_resolution_is_terminal(parent, resolution)) {
            out->severity = PF_TERMINAL;
            out->terminal = 1;
            out->attempts = 0;
            out->terminal_presentation_seconds = pf_sample_triangular(
                &duration_rng, cfg->resolution_band[parent][resolution]);
        } else {
            out->severity = PF_RECOVERABLE;
            out->attempts = pf3_resolution_attempts(parent, resolution);
            if (out->attempts <= 0) {
                out->attempts = 1;
            }
            out->extra_job_seconds = pf_sample_triangular(
                &duration_rng, cfg->resolution_band[parent][resolution]);
        }
    }

    if (parent_uses_wheel(parent)) {
        int corner_total = sum_positive(cfg->corner_weight, PF_WHEEL_COUNT);
        if (corner_total > 0) {
            pf_rng_t corner_rng = stream_for(seed, id, PF3_DOMAIN_CORNER);
            uint64_t cdraw =
                pf_rng_below(&corner_rng, (uint64_t)corner_total);
            out->wheel = pf3_corner_for_draw(cfg, cdraw);
        }
    }

    if (parent == PF3_PARENT_FUEL_RIG) {
        int fire_selected;

        /* fuel_subtype is the same authoritative selection as resolution. */
        out->fuel_subtype =
            (pf_fuel_subtype_t)(PF_FUEL_SUBTYPE_SLOW_FLOW + resolution);

        if (forcing.fire >= 0) {
            fire_selected = forcing.fire;
        } else {
            pf_rng_t fire_rng = stream_for(seed, id, PF3_DOMAIN_FIRE);
            uint64_t fdraw = pf_rng_below(&fire_rng, PF3_FIRE_UNIVERSE);
            out->fire_draw = (int)fdraw;
            fire_selected = pf3_fire_selected_for_draw(cfg, (uint32_t)fdraw);
        }
        if (fire_selected) {
            /* Fire never rerolls the ordinary subtype. Burn then inspection
             * are drawn in this fixed order and reported as planned bands;
             * the ordinary job delay stays frozen and never absorbs them
             * offline. Realisation requires provider execution evidence. */
            pf_rng_t fire_duration_rng =
                stream_for(seed, id, PF3_DOMAIN_FIRE_DURATION);
            double burn =
                pf_sample_triangular(&fire_duration_rng, cfg->fire_burn);
            double inspection =
                pf_sample_triangular(&fire_duration_rng, cfg->fire_inspection);
            out->fire_selected = 1;
            out->fire_burn_seconds = burn;
            out->fire_inspection_seconds = inspection;
            out->fire_extra_seconds = burn + inspection;
            out->fire_suppressed = out->fire_supported ? 0 : 1;
        }
    }
    return out->outcome;
}

pf_outcome_t pf3_plan_build(const pf3_config_t *cfg, const pf_identity_t *id,
                            const pf_context_t *ctx, uint64_t seed,
                            pf3_plan_t *out)
{
    return pf3_plan_build_full(cfg, id, ctx, seed, NULL, out);
}

/* --------------------------------------------------------------- timing -- */

static int pf3_collect_end(const pf_job_t *jobs, size_t n, pf_job_kind_t kind,
                           double *out)
{
    int found = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind != kind) {
            continue;
        }
        if (!isfinite(value) || value < 0.0) {
            return 0;
        }
        if (!found || value > *out) {
            *out = value;
        }
        found = 1;
    }
    return found;
}

int pf3_plan_apply_timing(const pf3_plan_t *plan, const pf_job_t *jobs,
                          size_t n, pf_timing_result_t *out)
{
    double normal_end = 0.0;
    double affected_end = 0.0;
    double other_max = 0.0;
    int have_any = 0;
    int have_timing = 0;
    size_t i;

    if (!out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return 0;
    }
    if (plan->severity == PF_CLEAN) {
        out->known = 0;
        out->added_stop_time = 0.0;
        return 0;
    }
    if (plan->severity == PF_TERMINAL) {
        out->known = 0;
        out->added_stop_time = 0.0;
        return 0;
    }
    for (i = 0; i < n && i < PF_JOB_MAX; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind == PF_JOB_UNKNOWN) {
            continue;
        }
        if (!isfinite(value) || value < 0.0) {
            out->known = 0;
            return 0;
        }
        if (!have_any || value > normal_end) {
            normal_end = value;
        }
        have_any = 1;
    }
    if (!have_any) {
        out->known = 0;
        return 0;
    }
    if (plan->affected_job_kind == PF_JOB_STALL) {
        affected_end = normal_end;
        have_timing = 1;
    } else {
        pf_job_kind_t kind = (pf_job_kind_t)plan->affected_job_kind;
        if (!pf3_collect_end(jobs, n, kind, &affected_end)) {
            out->known = 0;
            return 0;
        }
        have_timing = 1;
    }
    if (!have_timing) {
        out->known = 0;
        return 0;
    }
    for (i = 0; i < n && i < PF_JOB_MAX; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind == PF_JOB_UNKNOWN || !isfinite(value) || value < 0.0) {
            continue;
        }
        if (plan->affected_job_kind == PF_JOB_STALL ||
            jobs[i].kind != (pf_job_kind_t)plan->affected_job_kind) {
            if (value > other_max) {
                other_max = value;
            }
        }
    }
    out->known = 1;
    out->normal_end = normal_end;
    out->fault_end = other_max > affected_end + plan->extra_job_seconds
                         ? other_max
                         : affected_end + plan->extra_job_seconds;
    out->added_stop_time = out->fault_end - normal_end;
    if (out->added_stop_time < 0.0) {
        out->added_stop_time = 0.0;
    }
    return 1;
}

/* ------------------------------------------------------------ rate math -- */

void pf3_rate_report(const pf3_config_t *cfg, const pf_context_t *ctx,
                     pf3_rate_report_t *out)
{
    int mask;
    int64_t eligible_weight = 0;
    int parent;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!cfg) {
        return;
    }
    out->gate_bp = cfg->incident_chance_bp;
    mask = pf3_parent_mask(ctx);
    out->eligible_mask = mask;
    for (parent = 0; parent < PF3_PARENT_COUNT; parent++) {
        out->parent_weight[parent] = cfg->parent_weight[parent];
        if ((mask & (1 << parent)) && cfg->parent_weight[parent] > 0) {
            eligible_weight += cfg->parent_weight[parent];
        }
    }
    if (eligible_weight <= 0) {
        return;
    }
    for (parent = 0; parent < PF3_PARENT_COUNT; parent++) {
        if (!(mask & (1 << parent)) || cfg->parent_weight[parent] <= 0) {
            continue;
        }
        out->parent_conditional_bp[parent] =
            (int)((int64_t)cfg->parent_weight[parent] * 10000 / eligible_weight);
        out->parent_conditional[parent] =
            (double)cfg->parent_weight[parent] / (double)eligible_weight;
    }
    {
        double terminal = 0.0;
        if (mask & (1 << PF3_PARENT_WHEELGUN)) {
            int sum = pf3_resolution_weight_sum(cfg, PF3_PARENT_WHEELGUN);
            if (sum > 0) {
                terminal += out->parent_conditional[PF3_PARENT_WHEELGUN] *
                            ((double)cfg->resolution_weight[PF3_PARENT_WHEELGUN]
                                                          [PF3_RES_WHEELGUN_IRREPARABLE] /
                             (double)sum);
            }
        }
        if (mask & (1 << PF3_PARENT_CROSS_THREAD)) {
            int sum = pf3_resolution_weight_sum(cfg, PF3_PARENT_CROSS_THREAD);
            if (sum > 0) {
                terminal += out->parent_conditional[PF3_PARENT_CROSS_THREAD] *
                            ((double)cfg->resolution_weight[PF3_PARENT_CROSS_THREAD]
                                                          [PF3_RES_CT_IRREPARABLE] /
                             (double)sum);
            }
        }
        out->terminal_given_incident = terminal;
        out->terminal_per_stop =
            (double)cfg->incident_chance_bp / 10000.0 * terminal;
    }
    {
        double fire = 0.0;
        if (mask & (1 << PF3_PARENT_FUEL_RIG)) {
            fire = out->parent_conditional[PF3_PARENT_FUEL_RIG] *
                   (double)cfg->fire_chance_bp / 10000.0;
        }
        out->fire_given_incident = fire;
        out->fire_per_stop =
            (double)cfg->incident_chance_bp / 10000.0 * fire;
    }
}

/* --------------------------------------------------------------- names -- */

const char *pf3_parent_name(int parent)
{
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL: return "slow_wheel";
    case PF3_PARENT_WHEELGUN: return "wheelgun";
    case PF3_PARENT_FUEL_RIG: return "fuel_rig";
    case PF3_PARENT_STALL: return "stall";
    case PF3_PARENT_CROSS_THREAD: return "cross_thread";
    default: return "none";
    }
}

const char *pf3_resolution_name(int parent, int resolution)
{
    switch (parent) {
    case PF3_PARENT_SLOW_WHEEL:
        return resolution == PF3_RES_SLOW_WHEEL_MINOR ? "minor" :
               resolution == PF3_RES_SLOW_WHEEL_AWKWARD ? "awkward" : "none";
    case PF3_PARENT_WHEELGUN:
        return resolution == PF3_RES_WHEELGUN_QUICK ? "quick_fix" :
               resolution == PF3_RES_WHEELGUN_BACKUP ? "backup_gun" :
               resolution == PF3_RES_WHEELGUN_IRREPARABLE ? "irreparable_nut"
                                                          : "none";
    case PF3_PARENT_FUEL_RIG:
        return resolution == PF3_RES_FUEL_SLOW_FLOW ? "slow_flow" :
               resolution == PF3_RES_FUEL_TEMPORARY_NO_FLOW
                   ? "temporary_no_flow" :
               resolution == PF3_RES_FUEL_INTERRUPTED_FLOW ? "interrupted_flow"
               : resolution == PF3_RES_FUEL_SPILL ? "spill" : "none";
    case PF3_PARENT_STALL:
        return resolution == PF3_RES_STALL_STRAIGHTFORWARD ? "straightforward" :
               resolution == PF3_RES_STALL_DIFFICULT ? "difficult_restart"
                                                     : "none";
    case PF3_PARENT_CROSS_THREAD:
        return resolution == PF3_RES_CT_CORRECTED ? "corrected" :
               resolution == PF3_RES_CT_LENGTHY ? "lengthy_rework" :
               resolution == PF3_RES_CT_IRREPARABLE ? "irreparable" : "none";
    default:
        return "none";
    }
}

const char *pf3_plan_status_name(int status)
{
    switch (status) {
    case PF_PLAN_V3_CLEAN: return "clean";
    case PF_PLAN_V3_INCIDENT: return "incident";
    case PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY:
        return "skipped_no_eligible_category";
    case PF_PLAN_V3_FORCED_INELIGIBLE_PARENT:
        return "forced_ineligible_parent";
    case PF_PLAN_V3_FORCED_INVALID_RESOLUTION:
        return "forced_invalid_resolution";
    case PF_PLAN_V3_FORCED_INVALID_FIRE:
        return "forced_invalid_fire";
    default: return "unknown";
    }
}

const char *pf3_policy_version(void)
{
    return "0.4.0-two-stage.3";
}

const char *pf3_config_hash(const pf3_config_t *cfg)
{
    static char hex[17];
    char document[8192];
    size_t written = 0;
    size_t i;
    uint64_t hash = UINT64_C(1469598103934665603);

    if (cfg) {
        written = pf3_config_render(cfg, document, sizeof(document));
        if (written >= sizeof(document)) {
            written = sizeof(document) - 1;
        }
    }
    for (i = 0; i < written; i++) {
        hash ^= (unsigned char)document[i];
        hash *= UINT64_C(1099511628211);
    }
    snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)hash);
    return hex;
}
