/* AUDITFIX X1B: bounded process-thread pause, no game addresses.
 * Prepare/allocate/pin/log BEFORE begin. While paused use only the atomic
 * write and kernel protection/context/cache operations, never heap/loader/log.
 * Native enumeration repeats until every non-caller thread is suspended. */
#ifndef MF_THREAD_GATE_H
#define MF_THREAD_GATE_H
#include <windows.h>
#include <stdint.h>
#define MFT_FN static __attribute__((unused))
#define MFT_MAX 256
typedef LONG (NTAPI *MfNextThread)(HANDLE,HANDLE,ACCESS_MASK,ULONG,ULONG,PHANDLE);
/* 2026-10-09 (report review): a refusal now carries a typed reason, logged
 * by the caller AFTER mft_end (never while paused). why 0 = not refused. */
enum { MFT_OK = 0, MFT_NO_API, MFT_ENUM_FAIL, MFT_NO_ID, MFT_LIMIT, MFT_DUP_FAIL,
       MFT_SUSPEND_FAIL, MFT_CONTEXT_FAIL, MFT_IP_IN_RANGE, MFT_UNSTABLE };
typedef struct { HANDLE h[MFT_MAX]; DWORD id[MFT_MAX]; unsigned n;
                 int why; unsigned long detail; } MfThreadGate;
MFT_FN const char *mft_reason(const MfThreadGate *g)
{
    switch (g->why) {
    case MFT_OK:           return "ok";
    case MFT_NO_API:       return "no NtGetNextThread";
    case MFT_ENUM_FAIL:    return "thread enumeration failed";
    case MFT_NO_ID:        return "thread id unavailable";
    case MFT_LIMIT:        return "thread limit reached";
    case MFT_DUP_FAIL:     return "handle duplicate failed";
    case MFT_SUSPEND_FAIL: return "suspend failed";
    case MFT_CONTEXT_FAIL: return "thread context failed";
    case MFT_IP_IN_RANGE:  return "a thread was inside the patch range";
    case MFT_UNSTABLE:     return "thread set kept changing";
    }
    return "unknown";
}
#define MFT_REFUSE(g, w, d) do { (g)->why = (w); (g)->detail = (unsigned long)(d);                                  mft_end(g); return 0; } while (0)
MFT_FN void mft_end(MfThreadGate *g)
{
    while (g->n) { HANDLE h = g->h[--g->n]; ResumeThread(h); CloseHandle(h); }
}
/* range [lo,hi) must not contain ANY paused EIP. Includes first8 for patches. */
MFT_FN int mft_begin(MfThreadGate *g, uintptr_t lo, uintptr_t hi)
{
    union { FARPROC raw; MfNextThread fn; } next;
    DWORD self = GetCurrentThreadId(); unsigned pass, i;
    next.raw = GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtGetNextThread");
    g->n = 0; g->why = MFT_OK; g->detail = 0;
    if (!next.fn) { g->why = MFT_NO_API; return 0; }
    for (pass = 0; pass < 4; ++pass) {
        HANDLE previous = NULL; unsigned added = 0;
        for (;;) {
            HANDLE h = NULL; DWORD id; int seen = 0; LONG status;
            status = next.fn(GetCurrentProcess(), previous,
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                0, 0, &h);
            if (previous) CloseHandle(previous);
            previous = NULL;
            if ((ULONG)status == 0x8000001Au) break; /* STATUS_NO_MORE_ENTRIES */
            if (status < 0 || !h) MFT_REFUSE(g, MFT_ENUM_FAIL, (ULONG)status);
            previous = h; id = GetThreadId(h);
            if (!id) { CloseHandle(h); MFT_REFUSE(g, MFT_NO_ID, GetLastError()); }
            if (id == self) continue;
            for (i = 0; i < g->n; ++i) if (g->id[i] == id) seen = 1;
            if (seen) continue;
            if (g->n == MFT_MAX) { CloseHandle(h); MFT_REFUSE(g, MFT_LIMIT, MFT_MAX); }
            if (!DuplicateHandle(GetCurrentProcess(), h,
                GetCurrentProcess(), &g->h[g->n], 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                DWORD e = GetLastError();
                CloseHandle(h); MFT_REFUSE(g, MFT_DUP_FAIL, e);
            }
            if (SuspendThread(g->h[g->n]) == (DWORD)-1) {
                DWORD e = GetLastError();
                CloseHandle(g->h[g->n]); CloseHandle(h); MFT_REFUSE(g, MFT_SUSPEND_FAIL, e);
            }
            g->id[g->n++] = id; ++added;
        }
        if (!added) {
            for (i = 0; i < g->n; ++i) {
                CONTEXT ctx; ZeroMemory(&ctx, sizeof(ctx)); ctx.ContextFlags = CONTEXT_CONTROL;
                if (!GetThreadContext(g->h[i], &ctx))
                    MFT_REFUSE(g, MFT_CONTEXT_FAIL, GetLastError());
                if ((uintptr_t)ctx.Eip >= lo && (uintptr_t)ctx.Eip < hi)
                    MFT_REFUSE(g, MFT_IP_IN_RANGE, ctx.Eip);
            }
            MemoryBarrier(); return 1;
        }
    }
    MFT_REFUSE(g, MFT_UNSTABLE, g->n);
}
#endif
