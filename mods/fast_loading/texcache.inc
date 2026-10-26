/* texcache.inc - included by f1c_fastload.c (DEV)
 *
 * Exact result cache for D3DXLoadSurfaceFromSurface (statically linked D3DX8
 * in the exe at 0x50099C, stdcall 8 args, ret 0x20). The game calls it once
 * per texture (from 0x53FD40) to convert/compress the decoded image into the
 * texture's top level: (dest, NULL, NULL, src, NULL, NULL, filter 4, colorkey).
 * D3DX format conversion and DXT compression is the slow part (0x504640).
 *
 * Key  = 128-bit hash of the source pixels (row bytes only, no pitch padding)
 *        + source desc + dest desc + filter + colorkey.
 * Miss = original D3DX call, then the dest pixels are read back and appended
 *        to the cache (only lockable non-DEFAULT-pool dests, full surfaces).
 * Hit  = the stored dest pixels are copied into the dest surface: the same
 *        bytes D3DX produced for the same input.
 * Verify=1: on a hit the original also runs and its output is compared.
 * Files: %TEMP%\f1c_fastload\texcache\data.bin (records), index.bin (24 B per
 * entry: h1, h2, offset, length). Entries written this session are usable
 * immediately (index kept in memory). */
#include <d3d8.h>

typedef HRESULT (__stdcall *lsfs_fn)(IDirect3DSurface8 *, const void *, const RECT *, IDirect3DSurface8 *,
                                     const void *, const RECT *, DWORD, DWORD);
static lsfs_fn g_lsfs_orig;
static const site_t g_lsfs_site = { 0x0050099C, 5, {0x55,0x8D,0x6C,0x24,0xA8}, "LoadSurfaceFromSurface_50099C" };

typedef struct { uint64_t h1, h2; uint32_t off, len; } tc_ent_t;
#define TC_SLOTS (1 << 16)                 /* open addressing, power of two */
static tc_ent_t *g_tc;                     /* TC_SLOTS entries, h1 == 0 = empty */
static int g_tc_n;
static int tc_ready;                       /* texture folder opened (TexCache, MipCache or FileCache on) */
static HANDLE g_tc_data = INVALID_HANDLE_VALUE, g_tc_idx = INVALID_HANDLE_VALUE;
static uint32_t g_tc_end;                  /* data.bin size */
static CRITICAL_SECTION g_tc_cs;
static long g_tc_hit, g_tc_miss, g_tc_store, g_tc_skip, g_tc_capskip;
static int g_tc_capped;
#ifdef FASTLOAD_DEV_TOOLS
static long g_tc_vpass, g_tc_vfail;
#endif
static uint64_t g_tc_hit_ticks, g_tc_miss_ticks;

/* CacheMaxMB cap (owner FL-3): no new entries once data.bin would pass the cap.
 * Logged once per session. */
static int tc_cap_ok(uint32_t len)
{
    if ((uint64_t)g_tc_end + len <= g_cache_cap) return 1;
    if (!g_tc_capped) {
        g_tc_capped = 1;
        logf_("texcache: size cap reached (%u MB): no new entries this session", (unsigned)(g_cache_cap >> 20));
    }
    g_tc_capskip++;
    return 0;
}

static int tc_rowinfo(D3DFORMAT f, UINT w, UINT h, UINT *rowbytes, UINT *rows)
{
    UINT bpp = 0;
    switch (f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: bpp = 4; break;
    case D3DFMT_R8G8B8: bpp = 3; break;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: case D3DFMT_A8R3G3B2: case D3DFMT_A8L8: bpp = 2; break;
    case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_R3G3B2: bpp = 1; break;
    case D3DFMT_DXT1: *rowbytes = ((w + 3) / 4) * 8; *rows = (h + 3) / 4; return 1;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
        *rowbytes = ((w + 3) / 4) * 16; *rows = (h + 3) / 4; return 1;
    default: return 0;
    }
    *rowbytes = w * bpp; *rows = h;
    return 1;
}

static void tc_mix(uint64_t *h1, uint64_t *h2, const unsigned char *p, size_t n)
{
    uint64_t a = *h1, b = *h2, w;
    while (n >= 8) {
        memcpy(&w, p, 8);
        a ^= w; a *= 0x9E3779B185EBCA87ULL; a = (a << 31) | (a >> 33);
        b += w; b *= 0xC2B2AE3D27D4EB4FULL; b ^= b >> 29;
        p += 8; n -= 8;
    }
    while (n--) { a ^= *p; a *= 0x100000001B3ULL; b += *p++; b *= 0x9E3779B185EBCA87ULL; }
    *h1 = a; *h2 = b;
}

static tc_ent_t *tc_find(uint64_t h1, uint64_t h2, int insert)
{
    uint32_t i = (uint32_t)h1 & (TC_SLOTS - 1), k;
    for (k = 0; k < TC_SLOTS; k++, i = (i + 1) & (TC_SLOTS - 1)) {
        if (!g_tc[i].h1) return insert ? &g_tc[i] : NULL;
        if (g_tc[i].h1 == h1 && g_tc[i].h2 == h2) return &g_tc[i];
    }
    return NULL;
}

static int tc_open_pair(const char *dir)
{
    char p[FL_PATH_MAX + 32];
    fl_path_cat(p, sizeof p, dir, "\\data.bin");
    g_tc_data = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    fl_path_cat(p, sizeof p, dir, "\\index.bin");
    g_tc_idx = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return g_tc_data != INVALID_HANDLE_VALUE && g_tc_idx != INVALID_HANDLE_VALUE;
}

/* Cache folder: %LOCALAPPDATA%\Lightloader\fast_loading\texcache */
static int tc_open(void)
{
    char dir[FL_PATH_MAX], p[FL_PATH_MAX], old[FL_PATH_MAX];
    DWORD got, hi = 0;
    tc_ent_t e;
    InitializeCriticalSection(&g_tc_cs);
    g_tc = (tc_ent_t *)VirtualAlloc(NULL, sizeof(tc_ent_t) * TC_SLOTS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_tc) return 0;
    if (!fl_base_dir(dir, sizeof dir)) return 0;
    strncat(dir, "\\texcache", sizeof dir - strlen(dir) - 1);
    CreateDirectoryA(dir, NULL);
    if (!tc_open_pair(dir)) return 0;
    g_tc_end = GetFileSize(g_tc_data, &hi);
    if (hi) return 0;                     /* > 4 GB: refuse */
    if ((uint64_t)g_tc_end >= g_cache_cap) {
        /* Startup, over the cap (FL-3): move this cache's own pair aside as
         * data.old.bin / index.old.bin (the previous .old pair is deleted first,
         * same folder, same names only) and start empty. */
        CloseHandle(g_tc_data); CloseHandle(g_tc_idx);
        g_tc_data = g_tc_idx = INVALID_HANDLE_VALUE;
        fl_path_cat(p, sizeof p, dir, "\\data.old.bin");
        fl_path_cat(old, sizeof old, dir, "\\index.old.bin");
        DeleteFileA(p); DeleteFileA(old);
        fl_path_cat(p, sizeof p, dir, "\\data.bin");
        fl_path_cat(old, sizeof old, dir, "\\data.old.bin");
        MoveFileExA(p, old, 0);
        fl_path_cat(p, sizeof p, dir, "\\index.bin");
        fl_path_cat(old, sizeof old, dir, "\\index.old.bin");
        MoveFileExA(p, old, 0);
        if (!tc_open_pair(dir)) return 0;
        g_tc_end = GetFileSize(g_tc_data, &hi);
        logf_("texcache: at startup the cache was over the cap (%u MB); moved aside, starting empty",
              (unsigned)(g_cache_cap >> 20));
    }
    /* load the index; drop entries that point past the data end (torn write) */
    SetFilePointer(g_tc_idx, 0, NULL, FILE_BEGIN);
    while (ReadFile(g_tc_idx, &e, sizeof e, &got, NULL) && got == sizeof e) {
        tc_ent_t *s;
        if (!e.h1 || (uint64_t)e.off + e.len > g_tc_end || g_tc_n >= TC_SLOTS / 2) continue;
        s = tc_find(e.h1, e.h2, 1);
        if (s && !s->h1) { *s = e; g_tc_n++; }
    }
    SetFilePointer(g_tc_idx, 0, NULL, FILE_END);
    return 1;
}

static int tc_hash_src(IDirect3DSurface8 *src, const D3DSURFACE_DESC *sd, const D3DSURFACE_DESC *dd,
                       DWORD filter, DWORD ck, uint64_t *h1, uint64_t *h2)
{
    D3DLOCKED_RECT lr;
    UINT rb, rows, y;
    uint32_t params[12];
    uint32_t mx = 0;
    __asm__ volatile("stmxcsr %0" : "=m"(mx));
    if (!tc_rowinfo(sd->Format, sd->Width, sd->Height, &rb, &rows)) return 0;
    if (FAILED(src->lpVtbl->LockRect(src, &lr, NULL, D3DLOCK_READONLY))) return 0;
    *h1 = 0x243F6A8885A308D3ULL; *h2 = 0x13198A2E03707344ULL;
    params[0] = sd->Format; params[1] = sd->Width; params[2] = sd->Height;
    params[3] = dd->Format; params[4] = dd->Width; params[5] = dd->Height;
    params[6] = filter; params[7] = ck; params[8] = rb; params[9] = rows;
    params[10] = get_cw(); params[11] = mx;   /* D3DX output varied across sessions for the same input but not within one (2026-10-20); FPU mode is the suspected cause */
    tc_mix(h1, h2, (const unsigned char *)params, sizeof params);
    for (y = 0; y < rows; y++) tc_mix(h1, h2, (const unsigned char *)lr.pBits + (size_t)y * lr.Pitch, rb);
    src->lpVtbl->UnlockRect(src);
    if (!*h1) *h1 = 1;
    return 1;
}

/* copy dest surface rows to/from buf; dir 0 = read dest into buf, 1 = write buf into dest */
static int tc_dest_io(IDirect3DSurface8 *dst, const D3DSURFACE_DESC *dd, unsigned char *buf, uint32_t len, int dir)
{
    D3DLOCKED_RECT lr;
    UINT rb, rows, y;
    if (!tc_rowinfo(dd->Format, dd->Width, dd->Height, &rb, &rows) || (uint64_t)rb * rows != len) return 0;
    if (FAILED(dst->lpVtbl->LockRect(dst, &lr, NULL, dir ? 0 : D3DLOCK_READONLY))) return 0;
    for (y = 0; y < rows; y++) {
        unsigned char *row = (unsigned char *)lr.pBits + (size_t)y * lr.Pitch;
        if (dir) memcpy(row, buf + (size_t)y * rb, rb); else memcpy(buf + (size_t)y * rb, row, rb);
    }
    dst->lpVtbl->UnlockRect(dst);
    return 1;
}

static int tc_read(const tc_ent_t *e, unsigned char *buf)
{
    OVERLAPPED o;
    DWORD got = 0;
    memset(&o, 0, sizeof o);
    o.Offset = e->off;
    return ReadFile(g_tc_data, buf, e->len, &got, &o) && got == e->len;
}

static HRESULT __stdcall fl_lsfs(IDirect3DSurface8 *dst, const void *dpal, const RECT *drect, IDirect3DSurface8 *src,
                                 const void *spal, const RECT *srect, DWORD filter, DWORD ck)
{
    D3DSURFACE_DESC sd, dd;
    uint64_t h1, h2, t0 = now();
    UINT rb, rows;
    uint32_t len;
    tc_ent_t *e;
    unsigned char *buf;
    HRESULT r;
    if (dpal || drect || spal || srect || !dst || !src ||
        FAILED(src->lpVtbl->GetDesc(src, &sd)) || FAILED(dst->lpVtbl->GetDesc(dst, &dd)) ||
        dd.Pool == D3DPOOL_DEFAULT || !tc_rowinfo(dd.Format, dd.Width, dd.Height, &rb, &rows) ||
        !tc_hash_src(src, &sd, &dd, filter, ck, &h1, &h2)) {
        g_tc_skip++;
        return g_lsfs_orig(dst, dpal, drect, src, spal, srect, filter, ck);
    }
    len = rb * rows;
    buf = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len ? len : 1);
    if (!buf) return g_lsfs_orig(dst, dpal, drect, src, spal, srect, filter, ck);
    EnterCriticalSection(&g_tc_cs);
    e = tc_find(h1, h2, 0);
    if (e && e->len == len && tc_read(e, buf)) {
        LeaveCriticalSection(&g_tc_cs);
#ifdef FASTLOAD_DEV_TOOLS
        if (g_verify) {
            unsigned char *v = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len);
            r = g_lsfs_orig(dst, dpal, drect, src, spal, srect, filter, ck);
            if (v && r == D3D_OK && tc_dest_io(dst, &dd, v, len, 0) && !memcmp(v, buf, len)) g_tc_vpass++;
            else {
                /* diagnose: run D3DX a second time on the same input; if the two
                 * D3DX outputs differ, D3DX itself is not deterministic here */
                unsigned char *v2 = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len);
                uint32_t dc = 0, d2 = 0, i;
                uint16_t cw = get_cw();
                HRESULT r2 = v2 ? g_lsfs_orig(dst, dpal, drect, src, spal, srect, filter, ck) : E_FAIL;
                if (v2 && r2 == D3D_OK && tc_dest_io(dst, &dd, v2, len, 0)) {
                    for (i = 0; i < len; i++) { dc += v[i] != buf[i]; d2 += v[i] != v2[i]; }
                }
                g_tc_vfail++;
                logf_("  TEXCACHE VERIFY FAIL fmt %u->%u %ux%u: cache vs D3DX %u of %u bytes differ; D3DX run1 vs run2 %u differ; x87 cw %04X",
                      sd.Format, dd.Format, dd.Width, dd.Height, dc, len, d2, cw);
                if (v2) HeapFree(GetProcessHeap(), 0, v2);
            }
            if (v) HeapFree(GetProcessHeap(), 0, v);
            HeapFree(GetProcessHeap(), 0, buf);
            g_tc_hit++;
            return r;
        }
#endif /* FASTLOAD_DEV_TOOLS */
        if (tc_dest_io(dst, &dd, buf, len, 1)) {
            HeapFree(GetProcessHeap(), 0, buf);
            g_tc_hit++; g_tc_hit_ticks += now() - t0;
            return D3D_OK;
        }
        EnterCriticalSection(&g_tc_cs);
    }
    LeaveCriticalSection(&g_tc_cs);
    r = g_lsfs_orig(dst, dpal, drect, src, spal, srect, filter, ck);
    g_tc_miss++; g_tc_miss_ticks += now() - t0;
    if (r == D3D_OK && tc_dest_io(dst, &dd, buf, len, 0)) {
        EnterCriticalSection(&g_tc_cs);
        if (g_tc_n < TC_SLOTS / 2 && (uint64_t)g_tc_end + len < 0xF0000000ULL && tc_cap_ok(len) && !tc_find(h1, h2, 0)) {
            OVERLAPPED o;
            DWORD w = 0, wi = 0;
            tc_ent_t ne;
            memset(&o, 0, sizeof o);
            o.Offset = g_tc_end;
            if (WriteFile(g_tc_data, buf, len, &w, &o) && w == len) {
                ne.h1 = h1; ne.h2 = h2; ne.off = g_tc_end; ne.len = len;
                g_tc_end += len;
                if (WriteFile(g_tc_idx, &ne, sizeof ne, &wi, NULL) && wi == sizeof ne) {
                    tc_ent_t *s = tc_find(h1, h2, 1);
                    if (s) { *s = ne; g_tc_n++; g_tc_store++; }
                }
            }
        }
        LeaveCriticalSection(&g_tc_cs);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return r;
}

static int install_texcache(void)
{
    if (!tc_ready || !site_ok(&g_lsfs_site)) return 0;
    g_lsfs_orig = (lsfs_fn)(void *)make_tramp(g_exec + 1152, &g_lsfs_site);
    write_jmp(g_lsfs_site.va, (void *)fl_lsfs, g_lsfs_site.steal);
    return 1;
}

static void texcache_report(void)
{
    logf_("texcache: %d entries, hit %ld (%lu us), miss %ld (%lu us), stored %ld, skipped %ld, cap skips %ld",
          g_tc_n, g_tc_hit, us(g_tc_hit_ticks), g_tc_miss, us(g_tc_miss_ticks), g_tc_store, g_tc_skip, g_tc_capskip);
#ifdef FASTLOAD_DEV_TOOLS
    logf_("texcache verify: pass %ld fail %ld", g_tc_vpass, g_tc_vfail);
#endif
}

/* ---------------------------------------------------- mip level cache
 * 0x500BC8 = statically linked D3DXFilterTexture(tex, palette, srcLevel, filter)
 * (stdcall 4 args, ret 0x10); 0x53FD40 calls it with (tex, NULL, D3DX_DEFAULT, 5)
 * to build all lower mip levels from level 0. Key = hash of level-0 bytes +
 * level-0 desc + level count + filter + x87 cw + MXCSR + tag. Value = levels
 * 1..n back to back. Same miss/hit/verify rules as above. */
typedef HRESULT (__stdcall *filt_fn)(IDirect3DBaseTexture8 *, const void *, DWORD, DWORD);
static filt_fn g_filt_orig;
static const site_t g_filt_site = { 0x00500BC8, 6, {0x55,0x8B,0xEC,0x83,0xEC,0x64}, "FilterTexture_500BC8" };
static long g_mp_hit, g_mp_miss, g_mp_skip;
#ifdef FASTLOAD_DEV_TOOLS
static long g_mp_vpass, g_mp_vfail;
#endif
static uint64_t g_mp_hit_ticks, g_mp_miss_ticks;

/* read (dir 0) or write (dir 1) levels 1..n of tex to/from buf; returns bytes or 0 */
static uint32_t mp_levels_io(IDirect3DTexture8 *tex, DWORD n, unsigned char *buf, uint32_t cap, int dir)
{
    DWORD l;
    uint32_t off = 0;
    for (l = 1; l < n; l++) {
        IDirect3DSurface8 *s = NULL;
        D3DSURFACE_DESC d;
        UINT rb, rows;
        int ok;
        if (FAILED(tex->lpVtbl->GetSurfaceLevel(tex, l, &s)) || !s) return 0;
        ok = SUCCEEDED(s->lpVtbl->GetDesc(s, &d)) && tc_rowinfo(d.Format, d.Width, d.Height, &rb, &rows) &&
             (uint64_t)off + (uint64_t)rb * rows <= cap;
        if (ok && buf) ok = tc_dest_io(s, &d, buf + off, rb * rows, dir);
        s->lpVtbl->Release(s);
        if (!ok) return 0;
        off += rb * rows;
    }
    return off;
}

static void tc_store(uint64_t h1, uint64_t h2, const unsigned char *buf, uint32_t len)
{
    EnterCriticalSection(&g_tc_cs);
    if (g_tc_n < TC_SLOTS / 2 && (uint64_t)g_tc_end + len < 0xF0000000ULL && tc_cap_ok(len) && !tc_find(h1, h2, 0)) {
        OVERLAPPED o;
        DWORD w = 0, wi = 0;
        tc_ent_t ne;
        memset(&o, 0, sizeof o);
        o.Offset = g_tc_end;
        if (WriteFile(g_tc_data, buf, len, &w, &o) && w == len) {
            ne.h1 = h1; ne.h2 = h2; ne.off = g_tc_end; ne.len = len;
            g_tc_end += len;
            if (WriteFile(g_tc_idx, &ne, sizeof ne, &wi, NULL) && wi == sizeof ne) {
                tc_ent_t *s = tc_find(h1, h2, 1);
                if (s) { *s = ne; g_tc_n++; g_tc_store++; }
            }
        }
    }
    LeaveCriticalSection(&g_tc_cs);
}

static HRESULT __stdcall fl_filter(IDirect3DBaseTexture8 *bt, const void *pal, DWORD srclevel, DWORD filter)
{
    IDirect3DTexture8 *tex = (IDirect3DTexture8 *)bt;
    IDirect3DSurface8 *s0 = NULL;
    D3DSURFACE_DESC d0;
    DWORD n;
    uint64_t h1, h2, t0 = now();
    uint32_t len;
    unsigned char *buf;
    tc_ent_t *e;
    HRESULT r;
    if (pal || !bt || (srclevel != 0 && srclevel != 0xFFFFFFFF) || bt->lpVtbl->GetType(bt) != D3DRTYPE_TEXTURE ||
        (n = tex->lpVtbl->GetLevelCount(tex)) < 2 || FAILED(tex->lpVtbl->GetSurfaceLevel(tex, 0, &s0)) || !s0) {
        g_mp_skip++;
        return g_filt_orig(bt, pal, srclevel, filter);
    }
    if (FAILED(s0->lpVtbl->GetDesc(s0, &d0)) || d0.Pool == D3DPOOL_DEFAULT ||
        !tc_hash_src(s0, &d0, &d0, filter, 0x4D495053 /* 'MIPS' */ ^ n, &h1, &h2)) {
        s0->lpVtbl->Release(s0);
        g_mp_skip++;
        return g_filt_orig(bt, pal, srclevel, filter);
    }
    s0->lpVtbl->Release(s0);
    len = mp_levels_io(tex, n, NULL, 0xFFFFFFFF, 0);
    buf = len ? (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len) : NULL;
    if (!buf) { g_mp_skip++; return g_filt_orig(bt, pal, srclevel, filter); }
    EnterCriticalSection(&g_tc_cs);
    e = tc_find(h1, h2, 0);
    if (e && e->len == len && tc_read(e, buf)) {
        LeaveCriticalSection(&g_tc_cs);
#ifdef FASTLOAD_DEV_TOOLS
        if (g_verify) {
            unsigned char *v = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len);
            r = g_filt_orig(bt, pal, srclevel, filter);
            if (v && r == D3D_OK && mp_levels_io(tex, n, v, len, 0) == len && !memcmp(v, buf, len)) g_mp_vpass++;
            else { g_mp_vfail++; logf_("  MIPCACHE VERIFY FAIL fmt %u %ux%u levels %lu", d0.Format, d0.Width, d0.Height, n); }
            if (v) HeapFree(GetProcessHeap(), 0, v);
            HeapFree(GetProcessHeap(), 0, buf);
            g_mp_hit++;
            return r;
        }
#endif /* FASTLOAD_DEV_TOOLS */
        if (mp_levels_io(tex, n, buf, len, 1) == len) {
            HeapFree(GetProcessHeap(), 0, buf);
            g_mp_hit++; g_mp_hit_ticks += now() - t0;
            return D3D_OK;
        }
    } else LeaveCriticalSection(&g_tc_cs);
    r = g_filt_orig(bt, pal, srclevel, filter);
    g_mp_miss++; g_mp_miss_ticks += now() - t0;
    if (r == D3D_OK && mp_levels_io(tex, n, buf, len, 0) == len) tc_store(h1, h2, buf, len);
    HeapFree(GetProcessHeap(), 0, buf);
    return r;
}

static int install_mipcache(void)
{
    if (!tc_ready || !site_ok(&g_filt_site)) return 0;
    g_filt_orig = (filt_fn)(void *)make_tramp(g_exec + 1216, &g_filt_site);
    write_jmp(g_filt_site.va, (void *)fl_filter, g_filt_site.steal);
    return 1;
}

static void mipcache_report(void)
{
    logf_("mipcache: hit %ld (%lu us), miss %ld (%lu us), skipped %ld",
          g_mp_hit, us(g_mp_hit_ticks), g_mp_miss, us(g_mp_miss_ticks), g_mp_skip);
#ifdef FASTLOAD_DEV_TOOLS
    logf_("mipcache verify: pass %ld fail %ld", g_mp_vpass, g_mp_vfail);
#endif
}

/* ------------------------------------------------ decode + convert cache
 * 0x5008E4 = statically linked D3DXLoadSurfaceFromFileInMemory(dest, pal,
 * rect, srcData, srcSize, srcRect, filter, colorkey, pSrcInfo) (stdcall 9,
 * ret 0x24); 0x53FED0 calls it with (surf, 0, 0, data, size, 0, 1, ck, info)
 * after it read the image file into memory. It decodes the image (BMP/TGA/
 * DDS...) and converts it into the surface: tex_decode + 0x504640 time.
 * Key = hash of the file bytes + dest desc + filter + colorkey + FPU state.
 * Value = 28-byte D3DXIMAGE_INFO (the game's buffer is 7 dwords) + surface
 * bytes. Same miss/hit/verify rules. */
typedef HRESULT (__stdcall *lsffm_fn)(IDirect3DSurface8 *, const void *, const RECT *, const void *, UINT,
                                      const RECT *, DWORD, DWORD, void *);
static lsffm_fn g_lsffm_orig;
static const site_t g_lsffm_site = { 0x005008E4, 6, {0x55,0x8B,0xEC,0x83,0xEC,0x64}, "LoadSurfaceFromFileInMemory_5008E4" };
static long g_fm_hit, g_fm_miss, g_fm_skip;
#ifdef FASTLOAD_DEV_TOOLS
static long g_fm_vpass, g_fm_vfail;
#endif
static uint64_t g_fm_hit_ticks, g_fm_miss_ticks;
#define FM_INFO 28

static HRESULT __stdcall fl_lsffm(IDirect3DSurface8 *dst, const void *pal, const RECT *drect, const void *data, UINT size,
                                  const RECT *srect, DWORD filter, DWORD ck, void *info)
{
    D3DSURFACE_DESC dd;
    UINT rb, rows;
    uint64_t h1, h2, t0 = now();
    uint32_t params[10], mx = 0, len;
    unsigned char *buf, tmpinfo[FM_INFO];
    tc_ent_t *e;
    HRESULT r;
    if (pal || drect || srect || !dst || !data || !size || FAILED(dst->lpVtbl->GetDesc(dst, &dd)) ||
        dd.Pool == D3DPOOL_DEFAULT || !tc_rowinfo(dd.Format, dd.Width, dd.Height, &rb, &rows)) {
        g_fm_skip++;
        return g_lsffm_orig(dst, pal, drect, data, size, srect, filter, ck, info);
    }
    __asm__ volatile("stmxcsr %0" : "=m"(mx));
    h1 = 0x452821E638D01377ULL; h2 = 0xBE5466CF34E90C6CULL;
    params[0] = 0x46494C45; /* 'FILE' */ params[1] = size; params[2] = dd.Format; params[3] = dd.Width;
    params[4] = dd.Height; params[5] = filter; params[6] = ck; params[7] = get_cw(); params[8] = mx; params[9] = rb * rows;
    tc_mix(&h1, &h2, (const unsigned char *)params, sizeof params);
    tc_mix(&h1, &h2, (const unsigned char *)data, size);
    if (!h1) h1 = 1;
    len = FM_INFO + rb * rows;
    buf = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len);
    if (!buf) return g_lsffm_orig(dst, pal, drect, data, size, srect, filter, ck, info);
    EnterCriticalSection(&g_tc_cs);
    e = tc_find(h1, h2, 0);
    if (e && e->len == len && tc_read(e, buf)) {
        LeaveCriticalSection(&g_tc_cs);
#ifdef FASTLOAD_DEV_TOOLS
        if (g_verify) {
            unsigned char *v = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, len);
            memset(tmpinfo, 0, sizeof tmpinfo);
            r = g_lsffm_orig(dst, pal, drect, data, size, srect, filter, ck, tmpinfo);
            if (v && r == D3D_OK && tc_dest_io(dst, &dd, v + FM_INFO, rb * rows, 0) && !memcmp(v + FM_INFO, buf + FM_INFO, rb * rows) &&
                !memcmp(tmpinfo, buf, FM_INFO)) g_fm_vpass++;
            else { g_fm_vfail++; logf_("  FILECACHE VERIFY FAIL size %u -> fmt %u %ux%u", size, dd.Format, dd.Width, dd.Height); }
            if (info) memcpy(info, tmpinfo, FM_INFO);
            if (v) HeapFree(GetProcessHeap(), 0, v);
            HeapFree(GetProcessHeap(), 0, buf);
            g_fm_hit++;
            return r;
        }
#endif /* FASTLOAD_DEV_TOOLS */
        if (tc_dest_io(dst, &dd, buf + FM_INFO, rb * rows, 1)) {
            if (info) memcpy(info, buf, FM_INFO);
            HeapFree(GetProcessHeap(), 0, buf);
            g_fm_hit++; g_fm_hit_ticks += now() - t0;
            return D3D_OK;
        }
    } else LeaveCriticalSection(&g_tc_cs);
    memset(tmpinfo, 0, sizeof tmpinfo);
    r = g_lsffm_orig(dst, pal, drect, data, size, srect, filter, ck, tmpinfo);
    if (info) memcpy(info, tmpinfo, FM_INFO);
    g_fm_miss++; g_fm_miss_ticks += now() - t0;
    if (r == D3D_OK && tc_dest_io(dst, &dd, buf + FM_INFO, rb * rows, 0)) {
        memcpy(buf, tmpinfo, FM_INFO);
        tc_store(h1, h2, buf, len);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return r;
}

static int install_filecache(void)
{
    if (!tc_ready || !site_ok(&g_lsffm_site)) return 0;
    g_lsffm_orig = (lsffm_fn)(void *)make_tramp(g_exec + 1280, &g_lsffm_site);
    write_jmp(g_lsffm_site.va, (void *)fl_lsffm, g_lsffm_site.steal);
    return 1;
}

static void filecache_report(void)
{
    logf_("filecache: hit %ld (%lu us), miss %ld (%lu us), skipped %ld",
          g_fm_hit, us(g_fm_hit_ticks), g_fm_miss, us(g_fm_miss_ticks), g_fm_skip);
#ifdef FASTLOAD_DEV_TOOLS
    logf_("filecache verify: pass %ld fail %ld", g_fm_vpass, g_fm_vfail);
#endif
}
