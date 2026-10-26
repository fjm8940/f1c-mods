/* legacy_formats.c — STOP 4 preparation: standalone C port of the proven
 * legacy decoders. Parity target: legacy_dossier/tools/legacy_formats.py and
 * its test suite (tools/test_legacy_formats.py). Compiled only by the offline
 * test harness; not part of the product build yet (STOP 4 integration, after
 * parallel port task A lands).
 *
 * Fail-closed: every malformed/truncated/oversized input is refused with a
 * bounded error message. A torn .spt decodes up to its last complete record
 * and reports the partial tail; it is never guessed.
 */
#include "legacy_formats.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const unsigned char LF_SPT_XOR_KEYS[3] = { 0x5C, 0xB4, 0x12 };

static int lf_decode_lap_owned(unsigned char *d, size_t size, lf_lap_t *out,
                               char *err, size_t err_cap);
static int lf_decode_spt_owned(unsigned char *d, size_t size, lf_spt_t *out,
                               char *err, size_t err_cap);

static int lf_fail(char *err, size_t err_cap, const char *fmt, ...) {
    if (err && err_cap) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, err_cap, fmt, ap);
        va_end(ap);
        err[err_cap - 1] = '\0';
    }
    return -1;
}

static uint32_t rd_u32(const unsigned char *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static float rd_f32(const unsigned char *p) {
    float v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static unsigned char *lf_read_file(const char *path, size_t *size_out,
                                   char *err, size_t err_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        lf_fail(err, err_cap, "cannot open file: %s", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        lf_fail(err, err_cap, "seek failed: %s", path);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        lf_fail(err, err_cap, "tell failed: %s", path);
        return NULL;
    }
    if ((unsigned long)sz > LF_MAX_FILE_BYTES) {
        fclose(f);
        lf_fail(err, err_cap, "file exceeds %u byte ceiling: %ld",
                (unsigned)LF_MAX_FILE_BYTES, sz);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        lf_fail(err, err_cap, "rewind failed: %s", path);
        return NULL;
    }
    size_t size = (size_t)sz;
    unsigned char *d = (unsigned char *)malloc(size ? size : 1);
    if (!d) {
        fclose(f);
        lf_fail(err, err_cap, "out of memory for %lu bytes", (unsigned long)size);
        return NULL;
    }
    if (size && fread(d, 1, size, f) != size) {
        free(d);
        fclose(f);
        lf_fail(err, err_cap, "short read: %s", path);
        return NULL;
    }
    fclose(f);
    *size_out = size;
    return d;
}

/* byte-shift + reversal, exactly the Python _lap_deobfuscate: shift every
 * byte then reverse the block. */
static void lf_shift_rev(const unsigned char *src, uint32_t len, int shift,
                         unsigned char *dst) {
    uint32_t i;
    for (i = 0; i < len; i++)
        dst[i] = (unsigned char)((src[len - 1 - i] - shift) & 0xFF);
}

static int lf_key_char(unsigned char c, int first) {
    if (first)
        return c >= 'A' && c <= 'Z';
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* Bounded equivalent of the Python pair regex
 * ([A-Z][A-Z0-9_]*)=([^\s]*(?:\s+(?![A-Z][A-Z0-9_]*=)[^\s]+)*).
 * Keys must start at a token boundary; a value ends before whitespace that is
 * followed by another KEY= token. Test parity is asserted by the shared
 * expectations in legacy_formats_test.c. */
static void lf_parse_pairs(const char *text, size_t len, lf_pairs_t *out) {
    size_t i = 0;
    while (i < len) {
        int boundary = (i == 0) || text[i - 1] == ' ' || text[i - 1] == '\n' ||
                       text[i - 1] == '\r' || text[i - 1] == '\t';
        if (!boundary || !lf_key_char((unsigned char)text[i], 1)) {
            i++;
            continue;
        }
        size_t k = i + 1;
        while (k < len && lf_key_char((unsigned char)text[k], 0)) k++;
        if (k >= len || text[k] != '=') {
            i++;
            continue;
        }
        size_t key_len = k - i;
        size_t v = k + 1;
        size_t vend = v;
        size_t scan = v;
        while (scan < len) {
            unsigned char c = (unsigned char)text[scan];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                size_t la = scan;
                while (la < len && (text[la] == ' ' || text[la] == '\t' ||
                                    text[la] == '\r' || text[la] == '\n'))
                    la++;
                if (la < len && text[la] >= 'A' && text[la] <= 'Z') {
                    size_t lk = la + 1;
                    while (lk < len && lf_key_char((unsigned char)text[lk], 0)) lk++;
                    if (lk < len && text[lk] == '=') {
                        vend = scan;
                        break;
                    }
                }
                scan = la;
                continue;
            }
            scan++;
            vend = scan;
        }
        while (vend > v && (text[vend - 1] == ' ' || text[vend - 1] == '\t' ||
                            text[vend - 1] == '\r' || text[vend - 1] == '\n'))
            vend--;
        if (out->count < LF_MAX_PAIRS) {
            lf_pair_t *slot = &out->pairs[out->count++];
            size_t kl = key_len < LF_MAX_PAIR_KEY - 1 ? key_len : LF_MAX_PAIR_KEY - 1;
            size_t vl = (vend - v) < LF_MAX_PAIR_VALUE - 1 ? (vend - v)
                                                           : LF_MAX_PAIR_VALUE - 1;
            memcpy(slot->key, text + i, kl);
            slot->key[kl] = '\0';
            memcpy(slot->value, text + v, vl);
            slot->value[vl] = '\0';
        } else {
            out->overflow = 1;
        }
        i = (vend > k) ? vend : k + 1;
    }
}

const char *lf_pairs_get(const lf_pairs_t *pairs, const char *key) {
    const char *found = NULL;
    int i;
    for (i = 0; i < pairs->count; i++)
        if (strcmp(pairs->pairs[i].key, key) == 0)
            found = pairs->pairs[i].value; /* last wins, like the Python dict */
    return found;
}

int lf_lap_channel_index(const lf_lap_t *lap, const char *name) {
    int i;
    for (i = 0; i < lap->channel_count; i++)
        if (strcmp(lap->channel_names[i], name) == 0)
            return i;
    return -1;
}

static void lf_text_block_reset(lf_text_block_t *b) {
    memset(b, 0, sizeof(*b));
}

/* Decode one length-prefixed shifted text block at *off; advances *off. */
static int lf_lap_read_text_block(const unsigned char *d, size_t size, long *off,
                                  int shift, const char *label, lf_text_block_t *out,
                                  char *err, size_t err_cap) {
    if (*off + 4 > (long)size)
        return lf_fail(err, err_cap, "lap %s: length prefix truncated", label);
    uint32_t length = rd_u32(d + *off);
    if (length < 1 || length > 65536 || *off + 4 + (long)length > (long)size)
        return lf_fail(err, err_cap, "lap %s: block length out of bounds: %u",
                       label, length);
    unsigned char *tmp = (unsigned char *)malloc(length);
    if (!tmp)
        return lf_fail(err, err_cap, "lap %s: out of memory", label);
    lf_shift_rev(d + *off + 4, length, shift, tmp);
    lf_text_block_reset(out);
    out->offset = *off;
    out->length = length;
    out->shift = shift;
    size_t copy = length < LF_MAX_TEXT - 1 ? length : LF_MAX_TEXT - 1;
    memcpy(out->text, tmp, copy);
    out->text[copy] = '\0';
    out->text_truncated = (length >= LF_MAX_TEXT) ? 1 : 0;
    lf_parse_pairs(out->text, strlen(out->text), &out->pairs);
    free(tmp);
    *off += 4 + (long)length;
    return 0;
}

static int lf_lap_laptime_seconds(const char *value, double *seconds_out) {
    long a = 0, b = 0, c = 0;
    char tail = '\0';
    if (sscanf(value, "%ld.%ld.%ld%c", &a, &b, &c, &tail) != 3)
        return 0;
    if (a < 0 || b < 0 || c < 0 || b > 999 || c > 999)
        return 0;
    *seconds_out = (double)a * 60.0 + (double)b + (double)c / 1000.0;
    return 1;
}

static int lf_lap_printable_ratio(const unsigned char *text, uint32_t len) {
    uint32_t i, printable = 0;
    if (!len)
        return 0;
    for (i = 0; i < len; i++)
        if (text[i] >= 32 && text[i] < 127)
            printable++;
    return printable * 100 >= len * 98; /* >= 0.98 */
}

int lf_decode_lap(const char *path, lf_lap_t *out, char *err, size_t err_cap) {
    size_t size = 0;
    unsigned char *d = lf_read_file(path, &size, err, err_cap);
    if (!d)
        return -1;
    return lf_decode_lap_owned(d, size, out, err, err_cap);
}

/* Import route entry: the caller keeps ownership of `data`; the decoder works
 * on a private copy (the owned variant consumes and frees it). */
int lf_decode_lap_mem(const unsigned char *data, size_t size, lf_lap_t *out,
                      char *err, size_t err_cap) {
    if (!data || size == 0)
        return lf_fail(err, err_cap, "lap: empty body");
    if (size > LF_MAX_FILE_BYTES)
        return lf_fail(err, err_cap, "file exceeds %u byte ceiling: %lu",
                       (unsigned)LF_MAX_FILE_BYTES, (unsigned long)size);
    unsigned char *d = (unsigned char *)malloc(size);
    if (!d)
        return lf_fail(err, err_cap, "lap: out of memory for %lu bytes",
                       (unsigned long)size);
    memcpy(d, data, size);
    return lf_decode_lap_owned(d, size, out, err, err_cap);
}

static int lf_decode_lap_owned(unsigned char *d, size_t size, lf_lap_t *out,
                               char *err, size_t err_cap) {
    memset(out, 0, sizeof(*out));
    out->lap_time_agree = -1;
    if (size < 16) {
        free(d);
        return lf_fail(err, err_cap, "lap file too short: %lu", (unsigned long)size);
    }
    if (memcmp(d, "CHECKSUM=", 9) != 0) {
        free(d);
        return lf_fail(err, err_cap, "lap file does not start with a CHECKSUM= line");
    }
    long end = -1;
    for (long i = 9; i + 1 < (long)size && i < 64; i++)
        if (d[i] == '\r' && d[i + 1] == '\n') { end = i; break; }
    if (end < 0) {
        free(d);
        return lf_fail(err, err_cap, "lap checksum line not CRLF-terminated");
    }
    size_t line_len = (size_t)end < sizeof(out->checksum_line) - 1
                          ? (size_t)end : sizeof(out->checksum_line) - 1;
    memcpy(out->checksum_line, d, line_len);
    out->checksum_line[line_len] = '\0';
    {
        const char *eq = strchr(out->checksum_line, '=');
        int digits_ok = eq && eq[1] != '\0';
        for (const char *q = eq ? eq + 1 : ""; digits_ok && *q; q++)
            if (*q < '0' || *q > '9') digits_ok = 0;
        if (!digits_ok) {
            free(d);
            return lf_fail(err, err_cap, "lap checksum line malformed: %s",
                           out->checksum_line);
        }
        out->checksum_value = strtoull(eq + 1, NULL, 10);
    }
    long off = end + 2;

    if (lf_lap_read_text_block(d, size, &off, 5, "metadata block",
                               &out->metadata, err, err_cap) != 0) {
        free(d);
        return -1;
    }

    /* fixed field block (73 B when name "lap desc$") */
    if (off + 40 > (long)size) {
        free(d);
        return lf_fail(err, err_cap, "lap field block truncated");
    }
    out->field_u32_00 = rd_u32(d + off); off += 4;
    out->field_u32_04 = rd_u32(d + off); off += 4;
    for (int i = 0; i < 5; i++) { out->field_float_08[i] = rd_f32(d + off); off += 4; }
    out->field_u32_1c = rd_u32(d + off); off += 4;
    out->field_float_20 = rd_f32(d + off); off += 4;
    out->field_float_24 = rd_f32(d + off); off += 4;
    if (off + 20 + 4 > (long)size) {
        free(d);
        return lf_fail(err, err_cap, "lap field block truncated");
    }
    for (int i = 0; i < 5; i++) { out->field_zero_u32[i] = rd_u32(d + off); off += 4; }
    uint32_t name_len = rd_u32(d + off);
    if (name_len < 1 || name_len > LF_MAX_CHANNEL_NAME ||
        off + 4 + (long)name_len > (long)size) {
        free(d);
        return lf_fail(err, err_cap, "lap field name length out of bounds: %u", name_len);
    }
    memcpy(out->field_name, d + off + 4, name_len);
    out->field_name[name_len] = '\0';
    off += 4 + (long)name_len;

    if (lf_lap_read_text_block(d, size, &off, 5, "weather block",
                               &out->weather, err, err_cap) != 0) {
        free(d);
        return -1;
    }

    if (off + 8 > (long)size) {
        free(d);
        return lf_fail(err, err_cap, "lap channel table header truncated");
    }
    out->channel_table_marker = rd_u32(d + off); off += 4;
    uint32_t count = rd_u32(d + off); off += 4;
    if (count < 1 || count > LF_MAX_CHANNELS) {
        free(d);
        return lf_fail(err, err_cap, "lap channel count out of range: %u", count);
    }
    out->channel_count = (int)count;
    for (uint32_t i = 0; i < count; i++) {
        if (off + 4 > (long)size) {
            free(d);
            return lf_fail(err, err_cap, "lap name table truncated at channel %u", i);
        }
        uint32_t length = rd_u32(d + off);
        if (length < 1 || length > LF_MAX_CHANNEL_NAME ||
            off + 4 + (long)length > (long)size) {
            free(d);
            return lf_fail(err, err_cap, "lap name %u length out of bounds: %u", i, length);
        }
        memcpy(out->channel_names[i], d + off + 4, length);
        out->channel_names[i][length] = '\0';
        off += 4 + (long)length;
    }

    long data_start = off;
    size_t stride = (size_t)count * 4u;
    if ((size_t)((long)size - data_start) < stride) {
        free(d);
        return lf_fail(err, err_cap, "lap sample region truncated (no complete sample)");
    }
    out->data_start = data_start;
    out->stride = (int)stride;
    out->first_time_s = rd_f32(d + data_start);
    int samples = 1;
    while (data_start + (long)((size_t)(samples + 1) * stride) <= (long)size) {
        float prev = rd_f32(d + data_start + (long)((size_t)(samples - 1) * stride));
        float next = rd_f32(d + data_start + (long)((size_t)samples * stride));
        if (!(next > prev))
            break; /* strict increase while scanning the time channel */
        samples++;
        if ((unsigned)samples > LF_MAX_SAMPLES) {
            free(d);
            return lf_fail(err, err_cap, "lap sample count exceeds %u ceiling",
                           (unsigned)LF_MAX_SAMPLES);
        }
    }
    if ((size_t)samples * (size_t)count > LF_MAX_SAMPLE_FLOATS) {
        free(d);
        return lf_fail(err, err_cap, "lap sample floats exceed %u ceiling",
                       (unsigned)LF_MAX_SAMPLE_FLOATS);
    }
    out->sample_count = samples;
    out->last_time_s = rd_f32(d + data_start + (long)((size_t)(samples - 1) * stride));
    out->cadence_s = samples > 1
        ? (float)(((double)out->last_time_s - (double)out->first_time_s) / (samples - 1))
        : 0.0f;
    out->sample_floats = (size_t)samples * (size_t)count;
    out->samples = (float *)malloc(out->sample_floats * sizeof(float));
    if (!out->samples) {
        free(d);
        return lf_fail(err, err_cap, "lap: out of memory for samples");
    }
    memcpy(out->samples, d + data_start, out->sample_floats * sizeof(float));

    long trailer_start = data_start + (long)((size_t)samples * stride);
    out->trailer_offset = trailer_start;
    out->trailer_size = (long)size - trailer_start;
    if (out->trailer_size > 0) {
        const unsigned char *trailer = d + trailer_start;
        long cursor = 0;
        while (cursor + 8 <= out->trailer_size &&
               out->trailer_block_count < LF_MAX_TRAILER_BLOCKS) {
            uint32_t length = rd_u32(trailer + cursor);
            if (length >= 4 && length <= 8192 &&
                cursor + 4 + (long)length <= out->trailer_size) {
                for (int si = 0; si < 2; si++) {
                    int shift = si == 0 ? 7 : 5; /* trailer byte-7 first, then B-meta 5 */
                    unsigned char *tmp = (unsigned char *)malloc(length);
                    if (!tmp) {
                        lf_lap_free(out);
                        free(d);
                        return lf_fail(err, err_cap, "lap trailer: out of memory");
                    }
                    lf_shift_rev(trailer + cursor + 4, length, shift, tmp);
                    int ok = lf_lap_printable_ratio(tmp, length) &&
                             memchr(tmp, '=', length) != NULL;
                    if (ok) {
                        lf_text_block_t *blk =
                            &out->trailer_blocks[out->trailer_block_count++];
                        lf_text_block_reset(blk);
                        blk->offset = trailer_start + cursor;
                        blk->length = length;
                        blk->shift = shift;
                        size_t copy = length < LF_MAX_TEXT - 1 ? length : LF_MAX_TEXT - 1;
                        memcpy(blk->text, tmp, copy);
                        blk->text[copy] = '\0';
                        blk->text_truncated = (length >= LF_MAX_TEXT) ? 1 : 0;
                        lf_parse_pairs(blk->text, strlen(blk->text), &blk->pairs);
                        free(tmp);
                        break;
                    }
                    free(tmp);
                }
            }
            cursor++;
        }
    }

    const char *lap_time = lf_pairs_get(&out->weather.pairs, "LAPTIME");
    if (lap_time) {
        double seconds = 0.0;
        if (lf_lap_laptime_seconds(lap_time, &seconds)) {
            out->lap_time_from_weather_valid = 1;
            out->lap_time_from_weather_s = seconds;
            out->lap_time_agree =
                fabs(seconds - (double)out->field_float_08[0]) < 0.01 ? 1 : 0;
        }
    }
    free(d);
    return 0;
}

void lf_lap_free(lf_lap_t *lap) {
    if (!lap)
        return;
    free(lap->samples);
    lap->samples = NULL;
    lap->sample_floats = 0;
}

/* ---- .spt ------------------------------------------------------------- */

static int lf_spt_read_cstr(const unsigned char *d, size_t size, size_t *off,
                            char *dst, size_t cap, int *truncated,
                            char *err, size_t err_cap, const char *label) {
    size_t start = *off;
    const unsigned char *nul =
        memchr(d + start, 0, size - start);
    if (!nul)
        return lf_fail(err, err_cap, "spt identity string %s unterminated", label);
    size_t len = (size_t)(nul - (d + start));
    size_t copy = len < cap - 1 ? len : cap - 1;
    memcpy(dst, d + start, copy);
    dst[copy] = '\0';
    if (len >= cap && truncated)
        *truncated = 1;
    *off = start + len + 1;
    return 0;
}

/* Reserve capacity for `need` player records; returns 0 ok, -1 oom/overflow. */
static int lf_spt_reserve(lf_spt_t *spt, unsigned need, unsigned *cap_inout,
                          char *err, size_t err_cap) {
    if (need > LF_MAX_SPT_PLAYER_RECORDS)
        return lf_fail(err, err_cap,
                       "spt player records exceed %u ceiling",
                       (unsigned)LF_MAX_SPT_PLAYER_RECORDS);
    if (need <= *cap_inout)
        return 0;
    unsigned cap = *cap_inout ? *cap_inout : 4096u;
    while (cap < need)
        cap *= 2u;
    if (cap > LF_MAX_SPT_PLAYER_RECORDS)
        cap = LF_MAX_SPT_PLAYER_RECORDS;
    size_t stride = (size_t)(spt->row_stride > 0 ? spt->row_stride : 1);
    float *elapsed = (float *)realloc(spt->elapsed, (size_t)cap * sizeof(float));
    if (!elapsed)
        return lf_fail(err, err_cap, "spt: out of memory for elapsed records");
    spt->elapsed = elapsed;
    float *rows = (float *)realloc(spt->rows, (size_t)cap * stride * sizeof(float));
    if (!rows)
        return lf_fail(err, err_cap, "spt: out of memory for player payload rows");
    spt->rows = rows;
    *cap_inout = cap;
    return 0;
}

int lf_decode_spt(const char *path, lf_spt_t *out, char *err, size_t err_cap) {
    size_t size = 0;
    unsigned char *d = lf_read_file(path, &size, err, err_cap);
    if (!d)
        return -1;
    return lf_decode_spt_owned(d, size, out, err, err_cap);
}

/* Import route entry: the caller keeps ownership of `data`. */
int lf_decode_spt_mem(const unsigned char *data, size_t size, lf_spt_t *out,
                      char *err, size_t err_cap) {
    if (!data || size == 0)
        return lf_fail(err, err_cap, "spt: empty body");
    if (size > LF_MAX_FILE_BYTES)
        return lf_fail(err, err_cap, "file exceeds %u byte ceiling: %lu",
                       (unsigned)LF_MAX_FILE_BYTES, (unsigned long)size);
    unsigned char *d = (unsigned char *)malloc(size);
    if (!d)
        return lf_fail(err, err_cap, "spt: out of memory for %lu bytes",
                       (unsigned long)size);
    memcpy(d, data, size);
    return lf_decode_spt_owned(d, size, out, err, err_cap);
}

static int lf_decode_spt_owned(unsigned char *d, size_t size, lf_spt_t *out,
                               char *err, size_t err_cap) {
    memset(out, 0, sizeof(*out));
    if (size < 0x80) {
        free(d);
        return lf_fail(err, err_cap, "spt too short for header and descriptor table");
    }
    out->header_u32_00 = rd_u32(d);
    memcpy(out->header_seed_bytes, d + 4, 4);
    out->header_sample_interval_s = rd_f32(d + 8);
    size_t off = 0x0C;
    if (lf_spt_read_cstr(d, size, &off, out->driver, sizeof(out->driver),
                         &out->descriptor_truncated, err, err_cap, "driver") != 0) { free(d); return -1; }
    if (lf_spt_read_cstr(d, size, &off, out->team, sizeof(out->team),
                         &out->descriptor_truncated, err, err_cap, "team") != 0) { free(d); return -1; }
    if (lf_spt_read_cstr(d, size, &off, out->vehicle_path, sizeof(out->vehicle_path),
                         &out->descriptor_truncated, err, err_cap, "vehicle_path") != 0) { free(d); return -1; }
    if (lf_spt_read_cstr(d, size, &off, out->track, sizeof(out->track),
                         &out->descriptor_truncated, err, err_cap, "track") != 0) { free(d); return -1; }
    if (off + 4 > size) {
        free(d);
        return lf_fail(err, err_cap, "spt truncated before channel count");
    }
    uint32_t count = rd_u32(d + off);
    off += 4;
    if (count < 1 || count > LF_MAX_CHANNELS) {
        free(d);
        return lf_fail(err, err_cap, "spt channel count out of range: %u", count);
    }
    out->channel_count = (int)count;
    for (uint32_t i = 0; i < count; i++) {
        if (off >= size) {
            free(d);
            return lf_fail(err, err_cap, "spt descriptor table truncated at channel %u", i);
        }
        out->channels[i].code = d[off];
        const unsigned char *nul = memchr(d + off + 1, 0, size - (off + 1));
        if (!nul) {
            free(d);
            return lf_fail(err, err_cap, "spt channel name unterminated at channel %u", i);
        }
        size_t len = (size_t)(nul - (d + off + 1));
        if (len > LF_MAX_CHANNEL_NAME) {
            free(d);
            return lf_fail(err, err_cap,
                           "spt channel name too long at channel %u: %lu", i,
                           (unsigned long)len);
        }
        memcpy(out->channels[i].name, d + off + 1, len);
        out->channels[i].name[len] = '\0';
        off = (size_t)(nul - d) + 1;
    }
    out->descriptor_end = (long)off;

    if (off + 17 * 4 + 4 > size) {
        free(d);
        return lf_fail(err, err_cap, "spt truncated inside the session hash block");
    }
    for (int i = 0; i < 17; i++)
        out->session_hash_words[i] = rd_u32(d + off + (size_t)i * 4);
    off += 17 * 4;
    uint32_t embedded_size = rd_u32(d + off);
    off += 4;
    if (embedded_size > 1000000 || off + embedded_size > size) {
        free(d);
        return lf_fail(err, err_cap, "spt embedded block out of bounds: %u", embedded_size);
    }
    out->embedded_size = embedded_size;
    {
        unsigned char *blob = (unsigned char *)malloc(embedded_size ? embedded_size : 1);
        if (!blob) {
            free(d);
            return lf_fail(err, err_cap, "spt: out of memory for embedded block");
        }
        int ascii = 1;
        for (uint32_t i = 0; i < embedded_size; i++) {
            unsigned char b = (unsigned char)(d[off + i] ^ LF_SPT_XOR_KEYS[i % 3u]);
            blob[i] = b;
            if (b > 0x7F)
                ascii = 0;
        }
        out->embedded_ascii_valid = ascii;
        if (!ascii) {
            free(blob);
            free(d);
            return lf_fail(err, err_cap,
                           "spt embedded block does not decode as ASCII with the recorded XOR keys");
        }
        size_t first_end = 0;
        while (first_end < embedded_size && blob[first_end] != '\n') first_end++;
        size_t fs = 0, fe = first_end;
        while (fs < fe && (blob[fs] == ' ' || blob[fs] == '\t' || blob[fs] == '\r')) fs++;
        while (fe > fs && (blob[fe - 1] == ' ' || blob[fe - 1] == '\t' || blob[fe - 1] == '\r')) fe--;
        size_t flen = fe - fs;
        if (flen >= sizeof(out->embedded_first_line))
            flen = sizeof(out->embedded_first_line) - 1;
        memcpy(out->embedded_first_line, blob + fs, flen);
        out->embedded_first_line[flen] = '\0';
        int sections = 0;
        size_t pos = 0;
        while (pos < embedded_size) {
            size_t line_end = pos;
            while (line_end < embedded_size && blob[line_end] != '\n') line_end++;
            if (line_end > pos && blob[pos] == '[' && blob[line_end - 1] == ']')
                sections++;
            pos = line_end + 1;
        }
        out->embedded_section_count = sections;
        free(blob);
    }
    off += embedded_size;
    out->stream_start = (long)off;

    out->type_codes[0] = 0x00;
    out->type_codes[1] = 0x01;
    out->type_codes[2] = 0x02;
    out->type_codes[3] = 0x03;
    out->type_codes[4] = 0x04;
    out->type_codes[5] = 0xFF;
    out->row_stride = out->channel_count < LF_SPT_FIELDS ? out->channel_count
                                                         : LF_SPT_FIELDS;

    unsigned cap = 0;
    unsigned records = 0;
    struct { int delta_ms; unsigned count; } deltas[64];
    int delta_n = 0;
    uint32_t last_elapsed_bits = 0;
    int have_last_bits = 0;
    size_t p = off;
    long stream_start = out->stream_start;

    while (p < size) {
        unsigned char rtype = d[p];
        size_t rsize;
        switch (rtype) {
        case 0x00: rsize = LF_SPT_SIZE_PLAYER; break;
        case 0x01: case 0x02: case 0x03: case 0xFF: rsize = LF_SPT_SIZE_EVENT; break;
        case 0x04: rsize = LF_SPT_SIZE_ENV; break;
        default:
            free(d);
            return lf_fail(err, err_cap, "spt: unknown record type 0x%02X at 0x%lX",
                           rtype, (unsigned long)p);
        }
        if (p + rsize > size) {
            out->partial_final_record_bytes = (long)(size - p);
            break;
        }
        int rank = rtype == 0x00 ? 0 : rtype == 0x01 ? 1 : rtype == 0x02 ? 2
                 : rtype == 0x03 ? 3 : rtype == 0x04 ? 4 : 5;
        out->type_counts[rank]++;
        if (rtype == 0x00) {
            float value = rd_f32(d + p + 1);
            if (records > 0) {
                float prev = out->elapsed[records - 1];
                int delta_ms = (int)lroundf((value - prev) * 1000.0f);
                int found = 0;
                for (int i = 0; i < delta_n; i++)
                    if (deltas[i].delta_ms == delta_ms) { deltas[i].count++; found = 1; break; }
                if (!found && delta_n < 64) {
                    deltas[delta_n].delta_ms = delta_ms;
                    deltas[delta_n].count = 1;
                    delta_n++;
                }
            }
            if (lf_spt_reserve(out, records + 1, &cap, err, err_cap) != 0) {
                lf_spt_free(out);
                free(d);
                return -1;
            }
            out->elapsed[records] = value;
            memcpy(out->rows + (size_t)records * (size_t)out->row_stride,
                   d + p + 1, (size_t)out->row_stride * sizeof(float));
            records++;
            last_elapsed_bits = rd_u32(d + p + 1);
            have_last_bits = 1;
        } else if (rtype == 0x04) {
            if (out->env_count < LF_MAX_SPT_ENV) {
                out->env_flags[out->env_count] = rd_u32(d + p + 1);
                for (int i = 0; i < 5; i++)
                    out->env_floats[out->env_count][i] =
                        rd_f32(d + p + 5 + (size_t)i * 4);
                out->env_count++;
            }
        } else if (out->event_count < LF_MAX_SPT_EVENTS) {
            out->event_rtype[out->event_count] = rtype;
            uint32_t raw = rd_u32(d + p + 1);
            out->event_raw[out->event_count] = raw;
            out->event_has_value[out->event_count] = have_last_bits;
            out->event_value[out->event_count] =
                have_last_bits ? (raw ^ last_elapsed_bits) : 0u;
            out->event_count++;
        }
        p += rsize;
    }
    if (records == 0) {
        free(d);
        return lf_fail(err, err_cap, "spt: no player records (type 0x00) found in the stream");
    }
    out->player_records = (long)records;
    out->first_elapsed_s = out->elapsed[0];
    out->last_elapsed_s = out->elapsed[records - 1];
    out->duration_s = out->last_elapsed_s - out->first_elapsed_s;
    out->stream_bytes = (long)p - stream_start;
    {
        int best = 0;
        for (int i = 1; i < delta_n; i++)
            if (deltas[i].count > deltas[best].count)
                best = i;
        out->dominant_delta_s = delta_n > 0 ? (float)deltas[best].delta_ms / 1000.0f : 0.0f;
    }
    free(d);
    return 0;
}

void lf_spt_free(lf_spt_t *spt) {
    if (!spt)
        return;
    free(spt->elapsed);
    free(spt->rows);
    spt->elapsed = NULL;
    spt->rows = NULL;
}
