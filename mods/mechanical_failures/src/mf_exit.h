/* mf_exit.h - shared self-eject protocol for the mechanical-failure modules.
 *
 * Standalone builds: MF_EXIT(h) is the plain FreeLibraryAndExitThread, so the
 * family DLLs keep their proven hot-swap behaviour unchanged.
 *
 * Merged build (mechanical_failures.dll, -DMF_MERGED):
 *   one module holds four family workers; each ends with MF_EXIT. The last
 *   thread out sleeps a short grace (so the other threads have certainly
 *   reached their ExitThread inside the kernel) and only then frees the
 *   module. mf_exit_target_set() is called by the host once it knows how many
 *   family modules actually started. Any one named unload event is turned
 *   into "all four" by the host monitor thread.
 */
#pragma once

#include <windows.h>

#ifdef MF_MERGED

void mf_thread_exit(HMODULE h);
void mf_exit_target_set(int count);
/* E9-H1b: a family that cannot prove its game hook is fully removed/quiet
 * calls this; the host pins the module instead of freeing it. */
void mf_exit_pin_required(void);

#define MF_EXIT(h) mf_thread_exit(h)

#else

/* Standalone family builds keep the plain self-eject. The pin request is a
 * no-op here (the merged build, which is the gated artifact, implements it);
 * a standalone build must be rebuilt before it could leak a live detour. */
#define mf_exit_pin_required() ((void)0)
#define MF_EXIT(h) FreeLibraryAndExitThread((h), 0)

#endif
