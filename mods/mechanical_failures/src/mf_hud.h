/* mf_hud.h - coloured, leak-free lines on the game's message strip
 * (owner 2026-09-29, docs/HUD_COLOUR_PLAN_2026-09-29.md).
 *
 * Strip: inner = *(u32*)0x7CD204, 8 slots x 92 bytes. Per slot (from
 * sub_45B9C0 / sub_45B430): text +0x3C, colour +0x8C (ARGB, default -1),
 * timer +0x90 (float), level +0x94 (default 3). We used to write text +
 * timer only, so our line showed in whatever colour the slot's last user
 * left. Now:
 *   - colour is set from a fixed table on every refresh (nobody can
 *     recolour our line);
 *   - the level is NEVER written (vanilla priority unchanged);
 *   - when one of our lines has faded (timer <= 0) and the slot still holds
 *     our text, the colour goes back to the game default (white), so a mod
 *     that writes text only can never inherit our colour. A slot whose text
 *     is no longer ours is never touched.
 * Vanilla adds reset colour/level themselves - they cannot inherit ours.
 * Header-only; each family keeps its own mfh_own_t. */
#ifndef MF_HUD_H
#define MF_HUD_H

#include <windows.h>
#include <stdint.h>
#include <string.h>

#define MFH_FN static __attribute__((unused))
#define MFH_OUTER     0x007CD204u
#define MFH_SLOTS     8
#define MFH_STRIDE    0x5C
#define MFH_TEXT      0x3C
#define MFH_COLOUR    0x8C
#define MFH_TIMER     0x90
#define MFH_TEXT_MAX  31
#define MFH_WHITE     0xFFFFFFFFu   /* game default, IDToColorMaskMapper else */
#define MFH_YELLOW    0xFFFFFF00u   /* level 2 */
#define MFH_RED       0xFFFF0000u   /* level 1 */

typedef struct {
    int  used[MFH_SLOTS];                    /* slot holds our coloured line */
    char text[MFH_SLOTS][MFH_TEXT_MAX + 1];
} mfh_own_t;

/* Owner 2026-09-29 (third pass): colours must be USEFUL - grab attention
 * only where the driver's reaction changes the outcome. Colour the
 * INSTRUCTION, never the diagnosis or a dead car (STOP THE CAR after the
 * engine is gone helps nobody). Each instruction below was checked: the
 * failure's progress scales with load_factor() or the pit clears/refills it.
 *   red    = act this second or the engine goes (valve float)
 *   yellow = act soon, it buys time / saves the race */
MFH_FN DWORD mfh_colour(const char *t)
{
    /* 2026-10-01 messaging audit (docs/MESSAGING_AUDIT_2026-10-01.md):
     * red = critical (the engine dies soon unless...), yellow = instruction.
     * Keep in step with mfm_tier (mf_msg.h). Legacy strings stay yellow. */
    static const char *const red[] = { "SHIFT NOW", "ENGINE CRITICAL",
        "PIT AND RETIRE", "FUEL STARVATION", NULL };
    static const char *const yellow[] = {
        "SHIFT EARLIER", "SHORT SHIFT", "COOL THE OIL", "COOL THE ENGINE",
        "BOX - DEBRIS", "EASE OFF", "SAVE FUEL", "BOX FOR FUEL",
        "WATCH THE REAR",
        "REVS DOWN", "PIT FOR AIR", "SHIFT EARLY", "PIT - DEBRIS",
        "PIT FOR FUEL", NULL };
    int i;
    for (i = 0; red[i]; i++) if (!strcmp(t, red[i])) return MFH_RED;
    for (i = 0; yellow[i]; i++) if (!strcmp(t, yellow[i])) return MFH_YELLOW;
    return MFH_WHITE;
}

#define MFH_AT(inner, n, off) ((inner) + (uintptr_t)(n) * MFH_STRIDE + (off))

/* Keep msg on screen for `hold` s: refresh our slot or claim a free one. */
MFH_FN void mfh_keep(mfh_own_t *o, const char *msg, float hold)
{
    uintptr_t inner = *(volatile uint32_t*)MFH_OUTER;
    DWORD col = mfh_colour(msg);
    int n, free_slot = -1;
    if (!inner || !msg[0]) return;
    for (n = 0; n < MFH_SLOTS; n++) {
        char *t = (char*)MFH_AT(inner, n, MFH_TEXT);
        float *tm = (float*)MFH_AT(inner, n, MFH_TIMER);
        if (strncmp(t, msg, MFH_TEXT_MAX) == 0) {
            *(volatile DWORD*)MFH_AT(inner, n, MFH_COLOUR) = col;
            *tm = hold;
            o->used[n] = 1;
            lstrcpynA(o->text[n], msg, MFH_TEXT_MAX + 1);
            return;
        }
        if (free_slot < 0 && *tm <= 0.0f) free_slot = n;
    }
    if (free_slot < 0) return;
    n = free_slot;
    lstrcpynA((char*)MFH_AT(inner, n, MFH_TEXT), msg, MFH_TEXT_MAX + 1);
    /* another writer (other family / mod) may have taken it meanwhile */
    if (strncmp((char*)MFH_AT(inner, n, MFH_TEXT), msg, MFH_TEXT_MAX) != 0) return;
    *(volatile DWORD*)MFH_AT(inner, n, MFH_COLOUR) = col;
    *(float*)MFH_AT(inner, n, MFH_TIMER) = hold;
    o->used[n] = 1;
    lstrcpynA(o->text[n], msg, MFH_TEXT_MAX + 1);
}

/* Take msg off the strip NOW (no fade): timer 0, text blank, colour back to
 * the game default. Only a slot that still holds our text is touched. */
MFH_FN void mfh_drop(mfh_own_t *o, const char *msg)
{
    uintptr_t inner = *(volatile uint32_t*)MFH_OUTER;
    int n;
    if (!inner || !msg || !msg[0]) return;
    for (n = 0; n < MFH_SLOTS; n++) {
        char *t;
        if (!o->used[n] || strncmp(o->text[n], msg, MFH_TEXT_MAX) != 0) continue;
        t = (char*)MFH_AT(inner, n, MFH_TEXT);
        if (strncmp(t, msg, MFH_TEXT_MAX) == 0) {
            *(float*)MFH_AT(inner, n, MFH_TIMER) = 0.0f;
            *(volatile DWORD*)MFH_AT(inner, n, MFH_COLOUR) = MFH_WHITE;
            t[0] = '\0';
        }
        o->used[n] = 0;
    }
}

/* Every tick: hand faded slots back in the game's default colour. */
MFH_FN void mfh_release(mfh_own_t *o)
{
    uintptr_t inner = *(volatile uint32_t*)MFH_OUTER;
    int n;
    if (!inner) return;
    for (n = 0; n < MFH_SLOTS; n++) {
        char *t;
        if (!o->used[n]) continue;
        t = (char*)MFH_AT(inner, n, MFH_TEXT);
        if (strncmp(t, o->text[n], MFH_TEXT_MAX) != 0) { o->used[n] = 0; continue; }
        if (*(float*)MFH_AT(inner, n, MFH_TIMER) <= 0.0f) {
            *(volatile DWORD*)MFH_AT(inner, n, MFH_COLOUR) = MFH_WHITE;
            o->used[n] = 0;
        }
    }
}

#endif
