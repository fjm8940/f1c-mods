#ifndef F1C_LIVE_REPLAY_H
#define F1C_LIVE_REPLAY_H
/* Native instant-replay mode, live-roster fields and the reviewed replay-exit
 * helper used by the replay roster guard. Reviewed static layout 2026-09-22;
 * native/gameplay acceptance pending.
 *
 * Evidence (canonical decompiled functions):
 *  - MainGameTickUpdate_0042c690.c skips the whole main update while
 *    dword_8AB6E4 == 2 (replay playback).
 *  - sub_4B9890_004b9890.c enters instant replay, sets mode 2 and runs the
 *    playback loop, then LoadingPhase2 and returns with mode 1.
 *  - LoadingPhase1_004b87c0.c saves, for exactly VehicleCount entries, the
 *    control word (+13000), the +4189 byte, and bit 2 of +18620, then puts
 *    those entries into replay state.
 *  - ProcessDriverEntries_004b8da0.c destroys an existing vehicle with no
 *    selected replay identity and decrements VehicleCount unconditionally
 *    (a parked Safety Car at +13000 == -1).
 *  - LoadingPhase2_004b8940.c restores with the (possibly reduced) count and
 *    calls SetDriverVisibility_004c7aa0.c for the saved +18620 bit, which also
 *    performs the native audio side effect.
 *
 * Identity contract for the restore: pointer equality is not identity. Native
 * vehicles occupy reusable fixed slots, so callers capture and re-verify the
 * self-pointer at +0x4034, the roster id at +0x4894, the scene-root pointer
 * at +0x6F4 and the bounded driver name at +0x4620 before any write.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum {
    F1C_REPLAY_MODE_ADDRESS          = 0x008AB6E4u, /* int32 0/1/2 */
    F1C_REPLAY_MODE_IDLE             = 0,
    F1C_REPLAY_MODE_LIVE             = 1,
    F1C_REPLAY_MODE_PLAYBACK         = 2,           /* main update skipped */
    F1C_REPLAY_VEHICLE_STATE_OFF     = 13000u,      /* +0x32C8 control word */
    F1C_REPLAY_VISIBLE_A_OFF         = 4189u,       /* +0x105D driver byte   */
    F1C_REPLAY_VISIBLE_B_OFF         = 18620u,      /* +0x48BC bit 1=hidden  */
    F1C_REPLAY_SELF_OFF              = 0x4034u,     /* self-pointer          */
    F1C_REPLAY_DRIVER_NAME_OFF       = 0x4620u,     /* bounded ASCIIZ        */
    F1C_REPLAY_ROSTER_OFF            = 0x4894u,     /* roster id (18580)     */
    F1C_REPLAY_SCENE_ROOT_OFF        = 0x6F4u,      /* scene root pointer    */
    F1C_REPLAY_EXCLUDED_STATE        = -1,          /* no replay identity    */
    F1C_REPLAY_NAME_BYTES            = 24u
};

enum {
    F1C_REPLAY_SET_DRIVER_VISIBILITY_ADDRESS = 0x004C7AA0u
};
typedef void (__attribute__((thiscall)) *F1CReplayVisibilitySet)(void*, char);

/* First 20 reviewed bytes of SetDriverVisibility (0x004C7AA0): sets/clears
 * bits 1-2 of +18620 and performs the native audio update. */
static int f1c_replay_visibility_code_valid(const void *code, size_t bytes) {
    static const uint8_t expected[] = {
        0x8a,0x44,0x24,0x04,0x84,0xc0,0x56,0x8b,0xf1,0x8b,
        0x8e,0xbc,0x48,0x00,0x00,0x74,0x4e,0x83,0xc9,0x06
    };
    return code && bytes >= sizeof expected && !memcmp(code, expected, sizeof expected);
}

#endif
