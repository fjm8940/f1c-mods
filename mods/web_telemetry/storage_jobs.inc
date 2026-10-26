/* storage_jobs.inc: Telemetry 1.1.0 storage jobs thread (STORAGE_MANAGER_SPEC 6.1, 7.5, 7.6, 7.7, 7.8, OQ-14).
 * Included after storage_writer.inc. Batch 3 scope: startup repair (7.5), archive to LZMS (7.7),
 * delete to the Windows Recycle Bin (7.8), load selection (7.6), retired MaxStorageMB log line (OQ-14).
 * NOT here: import of 1.0.1 sessions (7.9; needs the META block of 4.3, deferred), HTTP entry points (batch 4).
 *
 * Rules enforced in this file:
 *  - the open session (sw_wr.part_path while sw_wr.have) is never touched;
 *  - a file is changed only by create -> verify -> rename (repair, archive); nothing is overwritten;
 *  - delete is SHFileOperationW with FOF_ALLOWUNDO; this file has no hard-delete call (AC-15, abort A-5);
 *  - the index is rebuilt by the writer only (sw_rebuild_req). */

#include <shellapi.h>

#define SJ_WORK        ((size_t)SV2_MAX_PAYLOAD)
#define SJ_CBUF        ((size_t)SV2_MAX_PAYLOAD + 1024u)
#define SJ_POOL_WORDS  ((size_t)SV2_MAX_CARS * SW_NCH * SV2_MAX_BLOCK_TICKS)
#define SJ_MSG         256
#define SJ_QUEUE_N     8u
#define SJ_MAX_PATHS   64u
#define SJ_JOB_ARCHIVE 1
#define SJ_JOB_DELETE  2

/* Result codes. SJ_OK covers "done" and "already archived". */
enum { SJ_OK = 0, SJ_E_DAMAGED, SJ_E_OPEN, SJ_E_EXISTS, SJ_E_IO, SJ_E_STOP, SJ_E_LZMS, SJ_E_NOFILE,
       SJ_E_RECYCLE_OFF, SJ_E_CHANGED, SJ_E_ABORT, SJ_E_NOT_MOVED };

typedef BOOL (WINAPI *sj_dummy_fn)(void);   /* placeholder type, keeps the typedef block uniform */

typedef LONG (WINAPI *sj_rkey_fn)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD);
typedef HRESULT (WINAPI *sj_qrb_fn)(LPCSTR, SHQUERYRBINFO *);
typedef int (WINAPI *sj_shfo_fn)(SHFILEOPSTRUCTW *);

#ifdef TELEMETRY_TEST
static sj_shfo_fn sj_test_shfo;        /* test double; NULL = real SHFileOperationW */
static int sj_test_recycle = -1;       /* -1 real check, 0 force "off", 1 force "on" */
static int sj_test_calls;              /* SHFileOperationW calls seen by the double */
static int sj_test_mode;               /* 0 move every item, 1 return error, 2 move one then abort */
static DWORD sj_test_flags;            /* fFlags of the last call */
#endif

static volatile LONG sj_stop_req;
static volatile LONG sj_busy;      /* 1 while the jobs thread runs (stop applies to it only) */
static volatile LONG sj_start_count;
static HANDLE sj_wake, sj_thread;
static CRITICAL_SECTION sj_cs;
static int sj_cs_ready;
static char sj_loaded[160];

/* ---------- jobs (batch 4): one job = an archive run over several sessions, or one delete over several paths ---------- */
#define SJ_NAME 180
typedef struct {
    int used, kind, state, result, recycle_working;   /* state: 0 queued, 1 running, 2 done */
    unsigned seq, n, nmoved, nleft;
    double expect, bytes_working, bytes_archived, bytes_moved;
    char folder[SJ_MAX_PATHS][120];    /* recording folder of each item */
    char file[SJ_MAX_PATHS][SJ_NAME];  /* archive: working .f1t name; delete: file name, "" = the recording folder */
    char moved[SJ_MAX_PATHS][SJ_NAME], left[SJ_MAX_PATHS][SJ_NAME];
    char msg[SJ_MSG];
} sj_job_t;
static sj_job_t sj_jobs[SJ_QUEUE_N];

/* ---------- buffers for one job ---------- */
typedef struct {
    sv2_data_t *scratch;
    uint32_t *pool;
    uint8_t *work, *work2;
} sj_ctx_t;

static void sj_ctx_free(sj_ctx_t *c) {
    free(c->scratch);
    free(c->pool);
    free(c->work);
    free(c->work2);
    memset(c, 0, sizeof *c);
}

static int sj_ctx_init(sj_ctx_t *c) {
    memset(c, 0, sizeof *c);
    c->scratch = (sv2_data_t *)calloc(1, sizeof(sv2_data_t));
    c->pool = (uint32_t *)malloc(SJ_POOL_WORDS * sizeof(uint32_t));
    c->work = (uint8_t *)malloc(SJ_WORK);
    c->work2 = (uint8_t *)malloc(SJ_WORK);
    if (!c->scratch || !c->pool || !c->work || !c->work2) { sj_ctx_free(c); return 0; }
    c->scratch->pool = c->pool;
    c->scratch->pool_cap = SJ_POOL_WORDS;
    return 1;
}

/* Whole file into memory. Returns 1 on success; *out is NULL on failure. */
static int sj_read_file(const char *path, uint8_t **out, size_t *len) {
    FILE *f;
    long sz;
    uint8_t *p;
    size_t got;
    *out = NULL;
    *len = 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    sz = ftell(f);
    if (sz <= 0 || sz > LONG_MAX / 2 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    p = (uint8_t *)malloc((size_t)sz);
    if (!p) { fclose(f); return 0; }
    got = fread(p, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(p); return 0; }
    *out = p;
    *len = (size_t)sz;
    return 1;
}

/* Size of a file; directories count as 0. *exists = 1 when the path exists. */
static double sj_file_bytes(const char *path, int *exists) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    *exists = 0;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a)) return 0.0;
    *exists = 1;
    if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0.0;
    return (double)a.nFileSizeHigh * 4294967296.0 + (double)a.nFileSizeLow;
}

/* 1 when the path is the .part of the session the writer is writing right now. */
static int sj_is_open_part(const char *path) {
    return sw_wr.have && _stricmp(path, sw_wr.part_path) == 0;
}

/* ---------- block walk (one block of a file image) ---------- */
typedef struct {
    sv2_block_t b;
    size_t pos, next;
    const uint8_t *raw;
    size_t raw_len;
} sj_blk_t;

/* Reads the block at pos. Returns SV2_OK or a reader reason. raw points into buf or into work. */
static int sj_block_at(const uint8_t *buf, size_t len, size_t pos, uint32_t seq, uint32_t alg, uint8_t *work, sj_blk_t *o) {
    const uint8_t *stored;
    size_t avail, got = 0;
    if (pos > len || len - pos < SV2_BLOCK_HEADER_BYTES) return SV2_R_BLOCK_HEADER;
    if (sv2_block_hdr_decode(buf + pos, SV2_BLOCK_HEADER_BYTES, &o->b) != SV2_OK || o->b.seq != seq) return SV2_R_BLOCK_HEADER;
    avail = len - pos - SV2_BLOCK_HEADER_BYTES;
    if (o->b.stored_len > avail) return SV2_R_BLOCK_CRC;
    stored = buf + pos + SV2_BLOCK_HEADER_BYTES;
    if (sv2_crc32(0, stored, o->b.stored_len) != o->b.payload_crc) return SV2_R_BLOCK_CRC;
    if (o->b.flags & SV2_BLOCK_FLAG_COMPRESSED) {
        int r;
        if (o->b.raw_len > SJ_WORK) return SV2_R_BLOCK_DECODE;
        r = sv2_decompress_block(alg, stored, o->b.stored_len, work, SJ_WORK, &got);
        if (r < 0) return SV2_R_DECODER_UNAVAILABLE;
        if (r == 0 || got != o->b.raw_len) return SV2_R_BLOCK_DECODE;
        o->raw = work;
    } else {
        o->raw = stored;
        got = o->b.stored_len;
    }
    o->raw_len = got;
    o->pos = pos;
    o->next = pos + SV2_BLOCK_HEADER_BYTES + (size_t)o->b.stored_len;
    return SV2_OK;
}

/* 1 when both valid prefixes hold the same blocks: type, ticks, t_first and the raw payload bytes. */
static int sj_same_payloads(const uint8_t *a, size_t an, const sv2_scan_t *ra,
                            const uint8_t *b, size_t bn, const sv2_scan_t *rb, uint8_t *wa, uint8_t *wb) {
    size_t pa = (size_t)ra->header.channel_table_offset + ra->header.channel_table_bytes;
    size_t pb = (size_t)rb->header.channel_table_offset + rb->header.channel_table_bytes;
    uint32_t seq = 0;
    while (pa < ra->valid_bytes && pb < rb->valid_bytes) {
        sj_blk_t x, y;
        if (sj_block_at(a, an, pa, seq, ra->header.compression_id, wa, &x) != SV2_OK) return 0;
        if (sj_block_at(b, bn, pb, seq, rb->header.compression_id, wb, &y) != SV2_OK) return 0;
        if (x.b.type != y.b.type || x.b.n_ticks != y.b.n_ticks || x.b.t_first != y.b.t_first) return 0;
        if (x.raw_len != y.raw_len || memcmp(x.raw, y.raw, x.raw_len) != 0) return 0;
        pa = x.next;
        pb = y.next;
        seq++;
    }
    return pa == ra->valid_bytes && pb == rb->valid_bytes;
}

/* ---------- 7.5 repair: a .part becomes a recovered .f1t; the original is renamed, never deleted ---------- */
static int sj_delete(const char *const *paths, unsigned n, double expect, char *why, size_t cap,
                     unsigned *moved, unsigned *left);
static int sj_repair_part(const char *part, char *why, size_t cap) {
    sj_ctx_t cx;
    uint8_t *buf = NULL, *out = NULL, *chk = NULL;
    size_t len = 0, outn = 0, chkn = 0, L = strlen(part);
    sv2_scan_t rep, r2;
    sv2_header_t h;
    uint8_t hb[SV2_HEADER_BYTES];
    char repair[SW_PATHBUF], fin[SW_PATHBUF], dmg[SW_PATHBUF];
    FILE *f;
    int rc = SJ_E_DAMAGED;

    why[0] = 0;
    if (L < 9 || _stricmp(part + L - 9, ".f1t.part") != 0) { sw_fmt(why, cap, "not a .f1t.part file"); return SJ_E_DAMAGED; }
    if (sj_is_open_part(part)) { sw_fmt(why, cap, "session is open; not repaired"); return SJ_E_OPEN; }
    if (!sj_ctx_init(&cx)) { sw_fmt(why, cap, "out of memory"); return SJ_E_IO; }
    if (!sj_read_file(part, &buf, &len)) { sw_fmt(why, cap, "cannot read the .part; left unchanged"); sj_ctx_free(&cx); return SJ_E_IO; }
    sv2_scan(buf, len, cx.scratch, cx.work, SJ_WORK, &rep);
    if (rep.data_blocks == 0) { sw_fmt(why, cap, "no valid data block; left unchanged (damaged)"); goto done; }

    /* output = header (RECOVERED, CLOSED) + the original bytes up to the valid prefix (+ END when missing) */
    h = rep.header;
    h.flags |= SV2_FLAG_RECOVERED | SV2_FLAG_CLOSED;
    sv2_header_encode(&h, hb);
    out = (uint8_t *)malloc(rep.valid_bytes + SV2_BLOCK_HEADER_BYTES + 12u);
    if (!out) { sw_fmt(why, cap, "out of memory"); rc = SJ_E_IO; goto done; }
    memcpy(out, hb, SV2_HEADER_BYTES);
    memcpy(out + SV2_HEADER_BYTES, buf + SV2_HEADER_BYTES, rep.valid_bytes - SV2_HEADER_BYTES);
    outn = rep.valid_bytes;
    if (!rep.closed) {
        uint8_t pl[12], fr[SV2_BLOCK_HEADER_BYTES + 12];
        sv2_block_t eb;
        size_t fn;
        memset(&eb, 0, sizeof eb);
        sv2_put_le(pl, rep.data_blocks, 4);
        sv2_put_le(pl + 4, rep.ticks, 4);
        sv2_put_le(pl + 8, rep.dropped_ticks, 4);
        eb.type = SV2_BLOCK_END;
        eb.seq = rep.valid_blocks + rep.skipped_blocks;
        eb.raw_len = eb.stored_len = 12;
        fn = sv2_block_put(fr, sizeof fr, eb, pl);
        if (!fn) { sw_fmt(why, cap, "END block failed; left unchanged"); goto done; }
        memcpy(out + outn, fr, fn);
        outn += fn;
    }

    sw_fmt(repair, sizeof repair, "%.*s.repair", (int)(L - 5), part);
    sw_fmt(fin, sizeof fin, "%.*s", (int)(L - 5), part);
    sw_fmt(dmg, sizeof dmg, "%s.damaged", part);
    f = fopen(repair, "wb");
    if (!f) { sw_fmt(why, cap, "cannot create the repair file; .part left unchanged"); rc = SJ_E_IO; goto done; }
    if (fwrite(out, 1, outn, f) != outn) { fclose(f); sw_fmt(why, cap, "repair write failed; .part left unchanged"); rc = SJ_E_IO; goto done; }
    fflush(f);
    fclose(f);

    /* verify the written copy by reading it back (spec 7.5 step b) */
    if (!sj_read_file(repair, &chk, &chkn)) { sw_fmt(why, cap, "repaired copy unreadable; .part left unchanged"); rc = SJ_E_IO; goto done; }
    sv2_scan(chk, chkn, cx.scratch, cx.work, SJ_WORK, &r2);
    if (r2.reason != SV2_OK || !r2.closed || r2.ticks != rep.ticks || r2.data_blocks != rep.data_blocks ||
        !(r2.header.flags & SV2_FLAG_RECOVERED) || r2.valid_bytes != chkn) {
        sw_fmt(why, cap, "repaired copy failed verification; .part left unchanged");
        goto done;
    }

    /* rename the repaired copy to its final name (no overwrite), then keep the original as .damaged */
    if (!MoveFileExA(repair, fin, 0)) { sw_fmt(why, cap, "final name exists or rename failed; .part left unchanged"); rc = SJ_E_EXISTS; goto done; }
    if (!MoveFileExA(part, dmg, 0)) { sw_fmt(why, cap, "repaired file written, but the .part could not be renamed to .damaged"); rc = SJ_E_IO; goto done; }
    sw_fmt(why, cap, "recovered: readable up to %.3f s, %u data blocks", rep.last_good_t, (unsigned)rep.data_blocks);
    rc = SJ_OK;

    /* 1.1.1 (owner decision B): read the recovered file back once more; only then move the original .damaged copy to
     * the Windows Recycle Bin (existing sj_delete path, FOF_ALLOWUNDO). If the check or the bin move fails, the original stays. */
    {
        uint8_t *fb = NULL;
        size_t fbn = 0;
        sv2_scan_t fr;
        int readable = 0;
        if (sj_read_file(fin, &fb, &fbn)) {
            sv2_scan(fb, fbn, cx.scratch, cx.work, SJ_WORK, &fr);
            readable = fr.reason == SV2_OK && fr.closed && fr.ticks == rep.ticks && fr.data_blocks == rep.data_blocks &&
                       (fr.header.flags & SV2_FLAG_RECOVERED) && fr.valid_bytes == fbn;
        }
        free(fb);
        if (readable) {
            const char *one[1] = {dmg};
            char rb[200];
            unsigned moved = 0, left = 0;
            int mr = sj_delete(one, 1, (double)len, rb, sizeof rb, &moved, &left);
            if (mr == SJ_OK) log_f("storage: recovered %s; original %s moved to the Windows Recycle Bin", fin, dmg);
            else log_f("storage: recovered %s; original kept (Windows Recycle Bin not used: %s)", fin, rb);
        } else {
            log_f("storage: recovered copy %s did not read back; original kept as .damaged", fin);
        }
    }

done:
    free(buf);
    free(out);
    free(chk);
    sj_ctx_free(&cx);
    return rc;
}

/* Startup scan (7.5 step 1): every recording folder, every *.f1t.part. Runs on the jobs thread after registration. */
static unsigned sj_repair_all(void) {
    WIN32_FIND_DATAA fd, ff;
    char pat[SW_PATHBUF], sub[SW_PATHBUF], part[SW_PATHBUF], why[SJ_MSG];
    HANDLE h, hf;
    unsigned n = 0;
    if (!sw_enabled) return 0;
    sw_fmt(pat, sizeof pat, "%s*", sw_rec_dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        sw_fmt(sub, sizeof sub, "%s%s\\*", sw_rec_dir, fd.cFileName);
        hf = FindFirstFileA(sub, &ff);
        if (hf == INVALID_HANDLE_VALUE) continue;
        do {
            size_t L;
            if (ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            L = strlen(ff.cFileName);
            if (L < 9 || _stricmp(ff.cFileName + L - 9, ".f1t.part") != 0) continue;
            if (sj_stop_req) break;
            sw_fmt(part, sizeof part, "%s%s\\%s", sw_rec_dir, fd.cFileName, ff.cFileName);
            if (sj_repair_part(part, why, sizeof why) == SJ_OK) {
                n++;
                log_f("storage: %s %s", part, why);
            } else {
                log_f("storage: repair not done for %s: %s", part, why);
            }
        } while (FindNextFileA(hf, &ff));
        FindClose(hf);
        if (sj_stop_req) break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (n) InterlockedExchange(&sw_rebuild_req, 1);
    return n;
}

/* ---------- 7.7 archive: .f1t (XPRESS) -> .f1a (LZMS), verified bit-exact, never overwritten ---------- */
static int sj_archive_file(const char *f1t, char *why, size_t cap) {
    sj_ctx_t cx;
    size_t L = strlen(f1t), srcn = 0, dstn = 0, cto, ctb, pos;
    char f1a[SW_PATHBUF], partial[SW_PATHBUF];
    uint8_t *src = NULL, *dst = NULL, *frame = NULL, *cbuf = NULL;
    sv2_scan_t rep, r2;
    sv2_header_t h;
    uint8_t hb[SV2_HEADER_BYTES];
    FILE *f = NULL;
    uint32_t seq = 0;
    int rc = SJ_E_IO;

    why[0] = 0;
    if (L < 5 || _stricmp(f1t + L - 4, ".f1t") != 0) { sw_fmt(why, cap, "not a .f1t file"); return SJ_E_DAMAGED; }
    sw_fmt(f1a, sizeof f1a, "%.*sf1a", (int)(L - 3), f1t);
    sw_fmt(partial, sizeof partial, "%s.partial", f1a);
    if (!sv2_alg_available(SV2_COMP_LZMS)) {
        sw_fmt(why, cap, "archive refused: LZMS compression (cabinet.dll) is not available on this PC; the working copy is unchanged");
        return SJ_E_LZMS;
    }
    if (!sj_ctx_init(&cx)) { sw_fmt(why, cap, "out of memory"); return SJ_E_IO; }
    if (!sj_read_file(f1t, &src, &srcn)) { sw_fmt(why, cap, "cannot read the working copy"); goto done; }
    sv2_scan(src, srcn, cx.scratch, cx.work, SJ_WORK, &rep);
    if (rep.reason != SV2_OK || !rep.closed) { sw_fmt(why, cap, "session is not closed or is damaged: not archived"); rc = SJ_E_DAMAGED; goto done; }

    if (sj_read_file(f1a, &dst, &dstn)) {   /* 7.7 step 3a: an existing archive is only accepted when it verifies */
        sv2_scan(dst, dstn, cx.scratch, cx.work, SJ_WORK, &r2);
        if (r2.reason == SV2_OK && r2.closed && r2.valid_bytes == dstn && (r2.header.flags & SV2_FLAG_ARCHIVED) &&
            sj_same_payloads(src, srcn, &rep, dst, dstn, &r2, cx.work, cx.work2)) {
            sw_fmt(why, cap, "already archived");
            rc = SJ_OK;
        } else {
            sw_fmt(why, cap, "an archive exists and does not match: not overwritten");
            rc = SJ_E_EXISTS;
        }
        goto done;
    }

    f = fopen(partial, "wb");
    if (!f) { sw_fmt(why, cap, "cannot create the .partial file"); goto done; }
    h = rep.header;
    h.flags |= SV2_FLAG_ARCHIVED | SV2_FLAG_COMPRESSED_BLOCKS;
    h.compression_id = SV2_COMP_LZMS;
    sv2_header_encode(&h, hb);
    cto = rep.header.channel_table_offset;
    ctb = rep.header.channel_table_bytes;
    if (fwrite(hb, 1, sizeof hb, f) != sizeof hb ||
        fwrite(src + SV2_HEADER_BYTES, 1, cto + ctb - SV2_HEADER_BYTES, f) != cto + ctb - SV2_HEADER_BYTES) {
        sw_fmt(why, cap, "write failed: .partial kept, working copy unchanged");
        rc = SJ_E_IO;
        goto done;
    }
    cbuf = (uint8_t *)malloc(SJ_CBUF);
    frame = (uint8_t *)malloc(SJ_CBUF + SV2_BLOCK_HEADER_BYTES);
    if (!cbuf || !frame) { sw_fmt(why, cap, "out of memory"); goto done; }

    pos = cto + ctb;
    while (pos < rep.valid_bytes) {
        sj_blk_t x;
        sv2_block_t nb;
        const uint8_t *payload;
        size_t c, stored, fn;
        if (sj_stop_req && sj_busy) {
            sw_fmt(why, cap, "archive stopped: .partial left in place, working copy unchanged");
            rc = SJ_E_STOP;
            goto done;
        }
        if (sj_block_at(src, srcn, pos, seq, rep.header.compression_id, cx.work, &x) != SV2_OK) {
            sw_fmt(why, cap, "damaged block during archive: .partial left in place, working copy unchanged");
            rc = SJ_E_DAMAGED;
            goto done;
        }
        c = x.raw_len ? sv2_compress_block(SV2_COMP_LZMS, x.raw, x.raw_len, cbuf, SJ_CBUF) : 0;
        if (x.raw_len && c == 0) {   /* failure, not "not smaller": no silent fallback (4.8) */
            sw_fmt(why, cap, "LZMS compression failed: archive refused, .partial left in place, working copy unchanged");
            rc = SJ_E_LZMS;
            goto done;
        }
        if (c > 0 && c < x.raw_len) { payload = cbuf; stored = c; }
        else { payload = x.raw; stored = x.raw_len; }
        nb = x.b;
        nb.flags = (uint8_t)((c > 0 && c < x.raw_len) ? SV2_BLOCK_FLAG_COMPRESSED : 0);
        nb.raw_len = (uint32_t)x.raw_len;
        nb.stored_len = (uint32_t)stored;
        fn = sv2_block_put(frame, SJ_CBUF + SV2_BLOCK_HEADER_BYTES, nb, payload);
        if (!fn || fwrite(frame, 1, fn, f) != fn) {
            sw_fmt(why, cap, "write failed: .partial kept, working copy unchanged");
            rc = SJ_E_IO;
            goto done;
        }
        pos = x.next;
        seq++;
    }
    if (fclose(f) != 0) { f = NULL; sw_fmt(why, cap, "close failed: .partial kept, working copy unchanged"); rc = SJ_E_IO; goto done; }
    f = NULL;

    /* 7.7 step 3d: read back and compare every block with the working copy */
    if (!sj_read_file(partial, &dst, &dstn)) { sw_fmt(why, cap, "cannot read the .partial back"); goto done; }
    sv2_scan(dst, dstn, cx.scratch, cx.work, SJ_WORK, &r2);
    if (r2.reason != SV2_OK || !r2.closed || r2.valid_bytes != dstn || r2.ticks != rep.ticks ||
        r2.data_blocks != rep.data_blocks || !(r2.header.flags & SV2_FLAG_ARCHIVED) ||
        r2.header.compression_id != SV2_COMP_LZMS ||
        !sj_same_payloads(src, srcn, &rep, dst, dstn, &r2, cx.work, cx.work2)) {
        sw_fmt(why, cap, "archive verification failed: .partial left in place, working copy unchanged");
        rc = SJ_E_DAMAGED;
        goto done;
    }
    if (!MoveFileExA(partial, f1a, 0)) { sw_fmt(why, cap, "archive rename failed (name taken?): .partial kept"); rc = SJ_E_EXISTS; goto done; }
    sw_fmt(why, cap, "archived");
    rc = SJ_OK;

done:
    if (f) fclose(f);
    free(src);
    free(dst);
    free(frame);
    free(cbuf);
    sj_ctx_free(&cx);
    return rc;
}

/* ---------- 7.8 delete: Windows Recycle Bin only (FOF_ALLOWUNDO), pre-checks first, abort on any failure ---------- */
static sj_shfo_fn sj_real_shfo(void) {
    static sj_shfo_fn fn;
    static int tried;
    if (!tried) {
        HMODULE m = LoadLibraryA("shell32.dll");
        FARPROC fp = m ? GetProcAddress(m, "SHFileOperationW") : NULL;
        tried = 1;
        if (fp) memcpy(&fn, &fp, sizeof fn);
    }
    return fn;
}

/* Pre-check (3.7, OQ-15 INFERENCE): the drive's Windows Recycle Bin answers and NukeOnDelete is not set. */
static int sj_recycle_on(const char *path, char *why, size_t cap) {
    char root[4], vol[64], key[200];
    const char *lb, *rb;
    DWORD val = 0, vsz = sizeof val, typ = 0;
    HMODULE sh, adv;
    FARPROC fp;
    sj_qrb_fn qfn;
    sj_rkey_fn rfn;
    SHQUERYRBINFO info;
    HRESULT hr;
    LONG rc;
#ifdef TELEMETRY_TEST
    if (sj_test_recycle == 0) { sw_fmt(why, cap, "Windows Recycle Bin is off for this drive; nothing deleted"); return 0; }
    if (sj_test_recycle == 1) return 1;
#endif
    if (!path[0] || path[1] != ':') { sw_fmt(why, cap, "Windows Recycle Bin: drive letter not found; nothing deleted"); return 0; }
    root[0] = path[0]; root[1] = ':'; root[2] = '\\'; root[3] = 0;
    sh = LoadLibraryA("shell32.dll");
    fp = sh ? GetProcAddress(sh, "SHQueryRecycleBinA") : NULL;
    if (!fp) { sw_fmt(why, cap, "Windows Recycle Bin API unavailable; nothing deleted"); return 0; }
    memcpy(&qfn, &fp, sizeof qfn);
    memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    hr = qfn(root, &info);
    if (FAILED(hr)) { sw_fmt(why, cap, "Windows Recycle Bin not available for this drive; nothing deleted"); return 0; }
    if (!GetVolumeNameForVolumeMountPointA(root, vol, sizeof vol)) { sw_fmt(why, cap, "cannot read the drive id; nothing deleted"); return 0; }
    lb = strchr(vol, '{');
    rb = lb ? strchr(lb, '}') : NULL;
    if (!lb || !rb || rb - lb > 60) { sw_fmt(why, cap, "cannot read the drive id; nothing deleted"); return 0; }
    sw_fmt(key, sizeof key, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BitBucket\\Volume\\%.*s", (int)(rb - lb + 1), lb);
    adv = LoadLibraryA("advapi32.dll");
    fp = adv ? GetProcAddress(adv, "RegGetValueA") : NULL;
    if (!fp) { sw_fmt(why, cap, "registry API unavailable; nothing deleted"); return 0; }
    memcpy(&rfn, &fp, sizeof rfn);
    rc = rfn(HKEY_CURRENT_USER, key, "NukeOnDelete", 0x00000010 /* RRF_RT_REG_DWORD */, &typ, &val, &vsz);
    if (rc == ERROR_SUCCESS && val != 0) { sw_fmt(why, cap, "Windows Recycle Bin is set to delete at once for this drive; nothing deleted"); return 0; }
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) { sw_fmt(why, cap, "cannot confirm the Windows Recycle Bin setting; nothing deleted"); return 0; }
    return 1;
}

/* Deletes paths[] to the Windows Recycle Bin. expect = the size the user confirmed (bytes of existing files).
 * Refuses before any call when: an open .part is listed, the Windows Recycle Bin check fails, or expect differs. */
static int sj_delete(const char *const *paths, unsigned n, double expect, char *why, size_t cap,
                     unsigned *moved, unsigned *left) {
    static wchar_t list[SJ_MAX_PATHS * SW_PATHBUF + 4];
    unsigned char had[SJ_MAX_PATHS];
    double total = 0.0;
    unsigned i, present = 0, wp = 0;
    int ret, aborted = 0;
    SHFILEOPSTRUCTW op;
    sj_shfo_fn fn = NULL;
    *moved = 0;
    *left = 0;
    why[0] = 0;
    if (n == 0 || n > SJ_MAX_PATHS) { sw_fmt(why, cap, "nothing selected"); return SJ_E_NOFILE; }
    for (i = 0; i < n; i++) {
        int ex = 0;
        double b;
        if (sj_is_open_part(paths[i])) { sw_fmt(why, cap, "an open session cannot be deleted"); return SJ_E_OPEN; }
        b = sj_file_bytes(paths[i], &ex);
        had[i] = (unsigned char)ex;
        if (ex) { present++; total += b; }
    }
    if (!present) { sw_fmt(why, cap, "nothing to delete (already gone)"); return SJ_E_NOFILE; }
    if (!sj_recycle_on(paths[0], why, cap)) return SJ_E_RECYCLE_OFF;
    if (fabs(total - expect) > 0.5) { sw_fmt(why, cap, "changed, reload the list; nothing deleted"); return SJ_E_CHANGED; }
    for (i = 0; i < n; i++) {
        int k;
        if (!had[i]) continue;
        k = MultiByteToWideChar(CP_ACP, 0, paths[i], -1, list + wp, (int)(sizeof list / sizeof list[0] - wp));
        if (k <= 0) { sw_fmt(why, cap, "path too long; nothing deleted"); return SJ_E_NOFILE; }
        wp += (unsigned)k;
    }
    list[wp] = 0;   /* double NUL terminator */
    memset(&op, 0, sizeof op);
    op.wFunc = FO_DELETE;
    op.pFrom = list;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
#ifdef TELEMETRY_TEST
    fn = sj_test_shfo;
#endif
    if (!fn) fn = sj_real_shfo();
    if (!fn) { sw_fmt(why, cap, "Windows Recycle Bin API unavailable; nothing deleted"); return SJ_E_RECYCLE_OFF; }
#ifdef TELEMETRY_TEST
    sj_test_calls++;
    sj_test_flags = op.fFlags;
#endif
    ret = fn(&op);
    aborted = op.fAnyOperationsAborted ? 1 : 0;
    for (i = 0; i < n; i++) {
        int ex = 0;
        if (!had[i]) continue;
        sj_file_bytes(paths[i], &ex);
        if (ex) (*left)++;
    }
    *moved = present - *left;
    if (ret != 0 || aborted) {
        sw_fmt(why, cap, "delete aborted (code %d): %u still present, nothing else attempted", ret, *left);
        return SJ_E_ABORT;
    }
    if (*left) { sw_fmt(why, cap, "%u item(s) not moved (still present)", *left); return SJ_E_NOT_MOVED; }
    sw_fmt(why, cap, "moved %u item(s) to the Windows Recycle Bin", *moved);
    return SJ_OK;
}

#ifdef TELEMETRY_TEST
/* Test double for SHFileOperationW: moves into the bin folder given in sj_test_bin (rename only). */
static char sj_test_bin[SW_PATHBUF];
static int WINAPI sj_test_shfo_double(SHFILEOPSTRUCTW *op) {
    const wchar_t *p = op->pFrom;
    int k = 0;
    if (sj_test_mode == 1) return 1;   /* error: nothing moved */
    while (*p) {
        char src[SW_PATHBUF], dst[SW_PATHBUF];
        wchar_t dstw[SW_PATHBUF];
        WideCharToMultiByte(CP_ACP, 0, p, -1, src, sizeof src, NULL, NULL);
        sw_fmt(dst, sizeof dst, "%sitem%d.bin", sj_test_bin, k);
        MultiByteToWideChar(CP_ACP, 0, dst, -1, dstw, SW_PATHBUF);
        if (!MoveFileExW(p, dstw, 0)) return 2;
        k++;
        p += wcslen(p) + 1;
        if (sj_test_mode == 2) { op->fAnyOperationsAborted = TRUE; break; }
    }
    return 0;
}
#endif

/* ---------- 7.6 load: selection only, no file is changed ---------- */
static int sj_load(const char *folder) {
    char p[SW_PATHBUF];
    if (!sw_enabled || !folder || !folder[0] || strchr(folder, '\\') || strchr(folder, '/') || strstr(folder, "..")) return 0;
    sw_fmt(p, sizeof p, "%s%s", sw_rec_dir, folder);
    if (!sw_is_dir(p)) return 0;
    sv2_copy(sj_loaded, sizeof sj_loaded, folder);
    return 1;
}

/* ---------- OQ-14: MaxStorageMB is retired; log once if the key is still in web_telemetry.ini ---------- */
static void sj_log_retired_keys(void) {
    char ini[MAX_PATH + 32], val[32];
    DWORD n;
    if (!settings_module_ini_path(ini, sizeof ini)) return;
    n = GetPrivateProfileStringA("Mod", "MaxStorageMB", "", val, sizeof val, ini);
    if (n) log_f("storage: [Mod] MaxStorageMB=%s is retired in 1.1.0 (no auto-delete; 1 GB warning only, OQ-14)", val);
}

/* ---------- queue and thread ---------- */
static unsigned sj_seq;   /* job numbers, shown as archive-N and delete-N */

/* Submits a job of n items (folders[i], files[i]; files[i] "" = the recording folder, delete only).
 * Returns the slot, or -1 when every slot is queued or running. *seq_out receives the job number. */
static int sj_submit(int kind, unsigned n, const char *const *folders, const char *const *files, double expect,
                     int recycle, unsigned *seq_out) {
    int slot = -1;
    if (!sj_cs_ready || n == 0 || n > SJ_MAX_PATHS || !seq_out) return -1;
    EnterCriticalSection(&sj_cs);
    for (unsigned i = 0; i < SJ_QUEUE_N && slot < 0; i++) if (!sj_jobs[i].used) slot = (int)i;
    if (slot < 0) {   /* every slot is used: reuse the oldest finished job, never a running or queued one */
        for (unsigned i = 0; i < SJ_QUEUE_N; i++)
            if (sj_jobs[i].state == 2 && (slot < 0 || sj_jobs[i].seq < sj_jobs[slot].seq)) slot = (int)i;
    }
    if (slot >= 0) {
        sj_job_t *j = &sj_jobs[slot];
        memset(j, 0, sizeof *j);
        j->used = 1;
        j->kind = kind;
        j->n = n;
        j->expect = expect;
        j->recycle_working = recycle ? 1 : 0;
        j->seq = ++sj_seq;
        for (unsigned k = 0; k < n; k++) {
            sv2_copy(j->folder[k], sizeof j->folder[k], folders[k]);
            sv2_copy(j->file[k], sizeof j->file[k], files[k]);
        }
        *seq_out = j->seq;
    }
    LeaveCriticalSection(&sj_cs);
    if (slot >= 0 && sj_wake) SetEvent(sj_wake);
    return slot;
}

/* Archive job (7.7 per session, in order, stop at the first error). With recycle_working, the working copy goes to the
 * Windows Recycle Bin only after its archive verified (7.7 step 5). */
static void sj_run_archive(sj_job_t *j, char *msg, size_t msgcap, int *rc) {
    static char pa[SW_PATHBUF], pf[SW_PATHBUF];
    *rc = SJ_OK;
    for (unsigned k = 0; k < j->n && *rc == SJ_OK; k++) {
        size_t L = strlen(j->file[k]);
        char f1a[SJ_NAME];
        const char *paths[1];
        int ex = 0;
        double work;
        if (L < 5) { sw_fmt(msg, msgcap, "bad item name"); *rc = SJ_E_DAMAGED; break; }
        sw_fmt(pa, sizeof pa, "%s%s\\%s", sw_rec_dir, j->folder[k], j->file[k]);
        work = sj_file_bytes(pa, &ex);
        *rc = sj_archive_file(pa, msg, msgcap);
        if (*rc != SJ_OK) break;
        sw_fmt(f1a, sizeof f1a, "%.*sf1a", (int)(L - 3), j->file[k]);
        sw_fmt(pf, sizeof pf, "%s%s\\%s", sw_rec_dir, j->folder[k], f1a);
        j->bytes_archived += sj_file_bytes(pf, &ex);
        j->bytes_working += work;
        if (j->recycle_working) {
            unsigned mv = 0, lf = 0;
            paths[0] = pa;
            *rc = sj_delete(paths, 1, work, msg, msgcap, &mv, &lf);
            if (*rc == SJ_OK) {   /* batch 6: the working .f1t went to the bin; the .f1a stays in the folder */
                j->bytes_moved += work;
                if (j->nmoved < SJ_MAX_PATHS) sv2_copy(j->moved[j->nmoved++], sizeof j->moved[0], j->file[k]);
            } else if (j->nleft < SJ_MAX_PATHS) sv2_copy(j->left[j->nleft++], sizeof j->left[0], j->file[k]);
        }
    }
    if (*rc == SJ_OK) sw_fmt(msg, msgcap, "archived %u session(s); working copy %s", j->n,
                             j->recycle_working ? "moved to the Windows Recycle Bin" : "kept");
}

static void sj_run(int idx) {
    sj_job_t *j = &sj_jobs[idx];
    static char pbuf[SJ_MAX_PATHS][SW_PATHBUF];
    static double before[SJ_MAX_PATHS];
    static int had[SJ_MAX_PATHS];
    const char *paths[SJ_MAX_PATHS];
    char msg[SJ_MSG];
    int rc = SJ_E_IO;
    msg[0] = 0;
    if (j->kind == SJ_JOB_ARCHIVE) {
        sj_run_archive(j, msg, sizeof msg, &rc);
    } else if (j->kind == SJ_JOB_DELETE) {
        unsigned mv = 0, lf = 0;
        for (unsigned k = 0; k < j->n; k++) {
            int ex = 0;
            if (j->file[k][0]) sw_fmt(pbuf[k], sizeof pbuf[k], "%s%s\\%s", sw_rec_dir, j->folder[k], j->file[k]);
            else sw_fmt(pbuf[k], sizeof pbuf[k], "%s%s", sw_rec_dir, j->folder[k]);
            paths[k] = pbuf[k];
            before[k] = sj_file_bytes(pbuf[k], &ex);
            had[k] = ex;
        }
        rc = sj_delete(paths, j->n, j->expect, msg, sizeof msg, &mv, &lf);
        for (unsigned k = 0; k < j->n; k++) {
            int ex = 0;
            const char *name = j->file[k][0] ? j->file[k] : j->folder[k];
            sj_file_bytes(paths[k], &ex);
            if (ex) { if (j->nleft < SJ_MAX_PATHS) sv2_copy(j->left[j->nleft++], sizeof j->left[0], name); }
            else if (had[k]) { if (j->nmoved < SJ_MAX_PATHS) sv2_copy(j->moved[j->nmoved++], sizeof j->moved[0], name); j->bytes_moved += before[k]; }
        }
    }
    EnterCriticalSection(&sj_cs);
    j->result = rc;
    sv2_copy(j->msg, sizeof j->msg, msg);
    j->state = 2;
    LeaveCriticalSection(&sj_cs);
    InterlockedExchange(&sw_rebuild_req, 1);   /* 7.7 / 7.8: the writer rescans the folders (index update) */
}

static DWORD WINAPI sj_main(void *unused) {
    (void)unused;
    InterlockedExchange(&sj_busy, 1);
    sj_repair_all();   /* 7.5: startup repair, after registration (spec 6.6) */
    for (;;) {
        int idx = -1;
        if (sj_stop_req) break;
        EnterCriticalSection(&sj_cs);
        for (unsigned i = 0; i < SJ_QUEUE_N; i++) {
            if (sj_jobs[i].used && sj_jobs[i].state == 0) { idx = (int)i; sj_jobs[i].state = 1; break; }
        }
        LeaveCriticalSection(&sj_cs);
        if (idx >= 0) { sj_run(idx); continue; }
        WaitForSingleObject(sj_wake, 500);
    }
    InterlockedExchange(&sj_busy, 0);
    return 0;
}

static void sj_start(void) {
    if (!sj_cs_ready) { InitializeCriticalSection(&sj_cs); sj_cs_ready = 1; }
    InterlockedExchange(&sj_stop_req, 0);
    InterlockedIncrement(&sj_start_count);
    sj_wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    sj_thread = sj_wake ? CreateThread(NULL, 0, sj_main, NULL, 0, NULL) : NULL;
    if (!sj_thread) log_f("storage: jobs thread not started");
}

/* Stops the jobs thread. A running archive stops between blocks and leaves its .partial (7.3). */
static void sj_stop(DWORD timeout_ms) {
    if (!sj_thread) return;
    InterlockedExchange(&sj_stop_req, 1);
    SetEvent(sj_wake);
    if (WaitForSingleObject(sj_thread, timeout_ms) == WAIT_OBJECT_0) {
        CloseHandle(sj_thread);
        CloseHandle(sj_wake);
        sj_thread = NULL;
        sj_wake = NULL;
    } else {
        log_f("storage: jobs thread did not stop in %lu ms; an archive in progress keeps its .partial", (unsigned long)timeout_ms);
    }
}
