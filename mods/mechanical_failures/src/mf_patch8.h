/* AUDITFIX X1: pure x86 patch plan. No memory writes or Win32 dependency. */
#ifndef MF_PATCH8_H
#define MF_PATCH8_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct { uint64_t original, jump; } MfPatch8;
static inline uint64_t mf_patch8_pack(const unsigned char b[8])
{
    uint64_t v = 0; unsigned i;
    for (i = 0; i < 8; ++i) v |= (uint64_t)b[i] << (8u * i);
    return v;
}
static inline int mf_patch8_plan(MfPatch8 *p, uint32_t site, uint32_t hook,
                                size_t stolen, const unsigned char *expect,
                                const unsigned char original[8])
{
    unsigned char b[8]; uint32_t rel; size_t i;
    if (!p || !expect || !original || stolen < 5 || stolen > 8
        || (site & 7u) || memcmp(original, expect, stolen)) return 0;
    memcpy(b, original, 8);
    rel = hook - (site + 5u);
    b[0] = 0xE9;
    for (i = 0; i < 4; ++i) b[i + 1] = (unsigned char)(rel >> (8u * i));
    for (i = 5; i < stolen; ++i) b[i] = 0x90;
    p->original = mf_patch8_pack(original); p->jump = mf_patch8_pack(b);
    return 1;
}
static inline int mf_patch8_can_install(const MfPatch8 *p, uint64_t observed)
{ return p && observed == p->original; }
static inline int mf_patch8_can_remove(const MfPatch8 *p, uint64_t observed)
{ return p && observed == p->jump; }
#endif
