/* coop_mf_bridge.h - CO-OP <-> Mechanical Failures in-process bridge (ABI v1).
 *
 * 2026-10-10 (Claude, owner request "MF failures in sync for everyone in
 * CO-OP"). Spec: mechanical_failures/docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md
 *
 * The CO-OP DLL (coop_mode) CREATES this named block and carries the events
 * over its existing UDP link (ManifestPort, default 34500). Mechanical
 * Failures only OPENS it. No block, or active == 0, means "not a CO-OP
 * session": MF then runs its single-player code paths unchanged.
 *
 * IDENTICAL COPIES: mechanical_failures/dll/coop_mf_bridge.h and
 * coop_mode/dll/coop_mf_bridge.h (tests/test_coop_bridge_abi.py checks).
 *
 * Rings are single-consumer. "out" (MF -> network) has several producers in
 * MF (worker + game thread), serialised by out_lock (spin, never held across
 * a call). "in" (network -> MF) has one producer (CO-OP net thread) and one
 * consumer (MF engine worker). */
#ifndef COOP_MF_BRIDGE_H
#define COOP_MF_BRIDGE_H

#include <windows.h>
#include <stdint.h>

#define COOP_MF_BRIDGE_NAME   "Local\\COOP_MF_BRIDGE_V1"
#define COOP_MF_MAGIC         0x31464D43u   /* "CMF1" */
#define COOP_MF_VERSION       1
#define COOP_MF_RING          256           /* power of two */
#define COOP_MF_WIRE_TAG      "MFEV1"       /* datagram prefix (5 bytes) */

/* event types; | COOP_MF_RELIABLE = CO-OP sends the datagram 3 times and the
 * receiver drops duplicates by id */
#define COOP_MF_EV_BURST      1   /* i0 sfx id (2 = blue smoke), i1 n; f0 rate f1 spread f2 up */
#define COOP_MF_EV_BANKS      2   /* f0..f2 smoke banks veh+0x3D54/58/5C */
#define COOP_MF_EV_BLOW       3   /* i0 1 = fire starts, 0 = fire ends */
#define COOP_MF_EV_SOUND      4   /* i0 0 = MF dir file, 1 = game Audio\Vehicle file; f0 gain; text = file name */
#define COOP_MF_EV_RETIRE     5   /* i0 ReasonCode, i1 FinishState; text = fault name */
#define COOP_MF_RELIABLE      0x8000u

#pragma pack(push, 4)
typedef struct CoopMfEvent {
    uint16_t type;        /* COOP_MF_EV_* (| COOP_MF_RELIABLE) */
    int16_t  vslot;       /* vehicle slot on the SENDING PC */
    uint32_t id;          /* sender-unique (origin << 20 | seq) */
    char     name[24];    /* driver name veh+0x4620 (car identity across PCs) */
    int32_t  i[2];
    float    f[3];
    char     text[24];
} CoopMfEvent;            /* 76 bytes */

typedef struct CoopMfBridge {
    uint32_t magic;
    uint32_t version;
    volatile LONG active;     /* CO-OP: 1 while a CO-OP session (host or client) runs */
    volatile LONG role;       /* CO-OP: 1 host, 2 client */
    volatile LONG out_lock;   /* MF producers */
    volatile LONG out_w;      /* MF writes   */
    volatile LONG out_r;      /* CO-OP reads */
    volatile LONG in_w;       /* CO-OP writes */
    volatile LONG in_r;       /* MF reads    */
    volatile LONG sent, recv, dropped;   /* CO-OP diagnostics */
    volatile LONG mf_attached;           /* MF: 1 once MF opened the block */
    CoopMfEvent out[COOP_MF_RING];
    CoopMfEvent in[COOP_MF_RING];
} CoopMfBridge;
#pragma pack(pop)

/* producer side, MF -> out (spin lock; returns 0 when full) */
static __inline int coop_mf_push_out(CoopMfBridge *b, const CoopMfEvent *e)
{
    LONG w;
    int ok = 0;
    while (InterlockedCompareExchange(&b->out_lock, 1, 0) != 0) Sleep(0);
    w = b->out_w;
    if ((LONG)(w - b->out_r) < COOP_MF_RING) {
        b->out[w & (COOP_MF_RING - 1)] = *e;
        MemoryBarrier();
        b->out_w = w + 1;
        ok = 1;
    }
    InterlockedExchange(&b->out_lock, 0);
    return ok;
}

/* single producer, CO-OP -> in (returns 0 when full) */
static __inline int coop_mf_push_in(CoopMfBridge *b, const CoopMfEvent *e)
{
    LONG w = b->in_w;
    if ((LONG)(w - b->in_r) >= COOP_MF_RING) return 0;
    b->in[w & (COOP_MF_RING - 1)] = *e;
    MemoryBarrier();
    b->in_w = w + 1;
    return 1;
}

/* single consumer for either ring; returns 1 with *e filled */
static __inline int coop_mf_pop(CoopMfEvent *ring, volatile LONG *r,
                                volatile LONG *w, CoopMfEvent *e)
{
    LONG rr = *r;
    if (rr == *w) return 0;
    MemoryBarrier();
    *e = ring[rr & (COOP_MF_RING - 1)];
    MemoryBarrier();
    *r = rr + 1;
    return 1;
}

#endif /* COOP_MF_BRIDGE_H */
