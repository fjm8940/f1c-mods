/* mf_coop.h - CO-OP compatibility layer for Mechanical Failures (2026-10-10).
 *
 * Spec: docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md
 *
 * SINGLE-PLAYER RULE: every function here returns the single-player answer
 * (player = vehicle slot 0, MF index = vehicle slot) unless the CO-OP DLL's
 * bridge block exists AND reports an active CO-OP session. Without the
 * CO-OP mod (or outside a CO-OP session) MF behaves exactly as before.
 *
 * Included by every MF family translation unit (engine, gear, electronics,
 * suspension via mf_audio.h). All state is per translation unit (static). */
#ifndef MF_COOP_H
#define MF_COOP_H

#include <windows.h>
#include <stdint.h>
#include "coop_mf_bridge.h"

#define MFC_VEH_ARRAY      0x005EC6D8u   /* vehicle slot 0 */
#define MFC_VEH_STRIDE     0x48C0u
#define MFC_VEH_SLOTS      104
#define MFC_OFF_MARKER     0x32C8u       /* controller: 0 local human, 1 AI, 2 remote, 3 replay, -1 empty */

static CoopMfBridge *mfc_b = NULL;
static DWORD mfc_open_ms = 0;
static volatile LONG mfc_plr_slot = 0;   /* cached local human slot */
static DWORD mfc_plr_ms = 0;

static __inline uintptr_t mfc_veh(int vslot)
{
    return (uintptr_t)MFC_VEH_ARRAY + (uintptr_t)MFC_VEH_STRIDE * (uintptr_t)vslot;
}

static __inline int mfc_marker(int vslot)
{
    if (vslot < 0 || vslot >= MFC_VEH_SLOTS) return -1;
    return *(volatile int*)(mfc_veh(vslot) + MFC_OFF_MARKER);
}

/* the bridge block, or NULL (retry the open at most once a second) */
static CoopMfBridge *mfc_bridge(void)
{
    DWORD now;
    HANDLE h;
    void *p;
    if (mfc_b) return mfc_b;
    now = GetTickCount();
    if (mfc_open_ms && now - mfc_open_ms < 1000u) return NULL;
    mfc_open_ms = now ? now : 1;
    h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, COOP_MF_BRIDGE_NAME);
    if (!h) return NULL;
    p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CoopMfBridge));
    CloseHandle(h);                       /* the view keeps the mapping */
    if (!p) return NULL;
    if (((CoopMfBridge*)p)->magic != COOP_MF_MAGIC
        || ((CoopMfBridge*)p)->version != COOP_MF_VERSION) {
        UnmapViewOfFile(p);
        return NULL;
    }
    mfc_b = (CoopMfBridge*)p;
    InterlockedExchange(&mfc_b->mf_attached, 1);
    return mfc_b;
}

/* 1 only inside a live CO-OP session (host or client) */
static __inline int mfc_active(void)
{
    CoopMfBridge *b = mfc_bridge();
    return b && b->active;
}

static __inline int mfc_is_host(void)
{
    CoopMfBridge *b = mfc_bridge();
    return b && b->active && b->role == 1;
}

/* Vehicle slot of the car this PC drives. Single-player / no CO-OP: 0.
 * CO-OP: the slot whose controller marker is 0 (host = 0, client = 1+),
 * re-resolved at most every 250 ms; none found -> 0. */
static int mfc_player_vslot(void)
{
    DWORD now;
    int s;
    if (!mfc_active()) return 0;
    now = GetTickCount();
    if (mfc_plr_ms && now - mfc_plr_ms < 250u) return (int)mfc_plr_slot;
    mfc_plr_ms = now ? now : 1;
    for (s = 0; s < MFC_VEH_SLOTS; s++) {
        if (mfc_marker(s) == 0) {
            InterlockedExchange(&mfc_plr_slot, s);
            return s;
        }
    }
    InterlockedExchange(&mfc_plr_slot, 0);
    return 0;
}

/* base address of the car this PC drives (single-player: 0x5EC6D8) */
static __inline uintptr_t mfc_player_base(void)
{
    return mfc_veh(mfc_player_vslot());
}

#endif /* MF_COOP_H */
