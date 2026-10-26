/* mf_compat.h - aggregate executable compatibility gate (E9 hardening H1a).
 *
 * Pure C, no windows.h: the same byte-match logic the merged DLL runs against
 * the live game image is unit-tested in tools/engine_model_test.c with fake
 * pattern bytes (the H1a refusal test). The DLL supplies a fetch callback
 * that reads the live process image; the test supplies one that reads a fake
 * buffer, so the refusal path is exercised without a game.
 *
 * Callers build one manifest per family (address, expected bytes, length,
 * human-readable name) and pass them to mf_compat_verify(): the host runs
 * every family manifest BEFORE any family DllMain starts (R03), and each
 * family's own hook install re-checks its own site as a second line.
 */
#ifndef MF_COMPAT_H
#define MF_COMPAT_H

#define MF_COMPAT_MAX_LEN 16

/* returns 1 and fills dst when len bytes at addr are readable, else 0 */
typedef int (*mf_compat_fetch_fn)(unsigned int addr, unsigned char *dst, int len);

typedef struct {
    unsigned int addr;
    unsigned char expect[MF_COMPAT_MAX_LEN];
    int len;
    const char *what;
} MfCompatItem;

/* NULL when every item matches; otherwise the `what` of the first mismatch
 * (unreadable target included). Never writes anything. */
static inline const char *mf_compat_verify(const MfCompatItem *items, int count,
                                           mf_compat_fetch_fn fetch)
{
    int i, k;
    for (i = 0; i < count; i++) {
        unsigned char buf[MF_COMPAT_MAX_LEN];
        if (items[i].len < 1 || items[i].len > MF_COMPAT_MAX_LEN)
            return items[i].what;
        if (!fetch(items[i].addr, buf, items[i].len))
            return items[i].what;
        for (k = 0; k < items[i].len; k++)
            if (buf[k] != items[i].expect[k])
                return items[i].what;
    }
    return NULL;
}

#endif /* MF_COMPAT_H */
