/* Reviewed native local-yellow input filter. See LOCAL_YELLOW_REVIEW_2026-09-08.md.
   Never write the steward output array directly. Intercept the native setter
   only on the reviewed game thread, with exact-target hook validation. */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define F1C_LOCAL_YELLOW_SET_ADDRESS ((void *)0x004D0F00u)
static const unsigned char F1C_LOCAL_YELLOW_SET_EXPECTED[6] = {
    0x8B, 0x44, 0x24, 0x04, 0x89, 0x01
};
#define F1C_LOCAL_YELLOW_ARRAY_POINTER 0x008ACFF0u
#define F1C_LOCAL_YELLOW_ZONE_COUNT 0x008ACFECu

static inline int f1c_local_yellow_filter_state(uintptr_t zone, uintptr_t base,
                                               int count, int state, int suppress)
{
    uintptr_t offset;
    if (!suppress || state != 2 || !base || count <= 0 || count > 512 || zone < base)
        return state;
    offset = zone - base;
    if (offset % 8u || offset / 8u >= (uintptr_t)count) return state;
    return 0;
}
