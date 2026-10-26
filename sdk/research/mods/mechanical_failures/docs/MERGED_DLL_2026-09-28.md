# Merged Mechanical Failures DLL (2026-09-28)

**Owner directive:** "all of these should be merged into one large mechanical
failures DLL." Executed: `dll/mechanical_failures.dll` now contains all four
families, while every family keeps its own source file and its own shared
control block, so the existing test panel works unchanged.

**Status:** development trial, ``. Built warning-clean,
symbol-verified, panel smoke + 19/19 tests pass. **First live injection done
2026-09-28** - all four families up from one module (see section 5a); one
live bug found and fixed the same hour. **Lightloader startup implemented
and installed the same day (section 9).**

## 5a. Live validation - first injection (2026-09-28, race session)

| Step | Result |
|---|---|
| Inject `mechanical_failures.dll` (game PID 15468, race, phase 5) | injector OK, no error |
| Control blocks after injection | all four attach: gear hb+33/s, engine/susp/elec hb+20/s; ABI 4 / 4 / 4 / 1; struct 208/184/256/84 - exactly as designed |
| Hooks | gear `hook_ok=1`, log `hook installed at 0x447E00`; suspension log `hook installed at 0x4441F0` |
| Session detection | all four logs `session live (type=7)` |
| Logs | gear/engine/suspension/electronics all healthy; engine log also lands in the game folder (relative path, unchanged behaviour) |
| No crash | Windows Application log shows no new APPCRASH for the session; the game was later closed normally (no self-eject log line, no WER event) |

**Live bug found and fixed (electronics):** the manual RADIO LOSS 10 s test
started a window (gate cleared, radio suppressed, `radio_loss_left_ms`
counted down) but the loss **pump only ran while `armed`**, so an unarmed
manual window never ended - the commentary gate stayed off and the engineer
stayed silent; the status field also ran negative. Fixed the same hour:
`radio_loss_pump()` now runs every tick (manual and armed paths), and the
published remaining time is clamped to >= 0. Comms were restored live via
CLEAR before the fix; merged + standalone DLLs rebuilt with the fix.
Post-fix re-test pending the next game launch.

## 5b. Pending live validation

1. Re-inject merged DLL; manual RADIO LOSS 10 s must end by itself
   (`radio_loss_left_ms` back to 0, comms audibly restored).
2. Arm each family (manual buttons) - behaviours unchanged from the separate
   DLLs.
3. Press Unload on any tab - all four workers stop and the module ejects;
   game stable. Re-inject after a rebuild (hot-swap loop).

## 1. What "merged" means here

| Piece | Where |
|---|---|
| Merged artifact | `dll/mechanical_failures.dll` (inject target) |
| Family sources | `dll/gear_failure.c`, `dll/engine_failure.c`, `dll/electronics_failure.c`, `f1c_ida_decompiled\mods\sdk_sandbox\suspension_failure\suspension_failure.c` (each still builds standalone) |
| Family control blocks | unchanged: `F1C_GEARFAIL_CTL` / `F1C_ENGFAIL_CTL` / `F1C_SUSFAIL_CTL` / `F1C_ELECFAIL_CTL` |
| Family unload events | unchanged: `F1C_GEARFAIL_UNLOAD` / `F1C_ENGFAIL_UNLOAD` / `F1C_SUSFAIL_UNLOAD` / `F1C_ELECFAIL_UNLOAD` |
| Build | `dll/build_merged.bat` (objects + host, one link) |

No family logic changed. Each source gained exactly two mechanical edits:
`#include "mf_exit.h"` and `FreeLibraryAndExitThread(...) -> MF_EXIT(...)`.

## 2. Mechanics (why it is safe)

- **Object linking, not copy-paste.** The four family sources compile to
  separate objects with their entry points renamed
  (`-DDllMain=DllMain_gear|engine|susp|elec`). Statics stay per-translation-
  unit exactly as before; there is no cross-family state and no symbol
  collision (verified with `nm`: one real `DllMain` + four renamed entry
  points + host helpers).
- **Four workers, one module.** The host `DllMain` starts a monitor thread,
  then calls the four family entry points; each spawns its proven worker
  thread with its own 20 Hz loop, hooks (gearbox SetGear detour, suspension
  physics detour) and shared-memory block - identical to the four-DLL
  layout.
- **Any one unload event stops everything.** The monitor
  (`WaitForMultipleObjects` on the four named events) turns one signal into
  all four, so any panel "Unload DLL" button cleanly stops the merged DLL.
- **Self-eject race handled by `mf_exit.h`.** Each worker ends with
  `MF_EXIT(h)`. Merged build: `mf_thread_exit()` increments an exit counter;
  the **last** thread (host-recorded target count) sleeps 50 ms - by then
  the others have certainly reached `ExitThread` inside the kernel - and only
  then calls `FreeLibraryAndExitThread`. Standalone builds: `MF_EXIT` is the
  plain `FreeLibraryAndExitThread`, so the family DLLs keep their proven
  behaviour; all four were rebuilt to confirm.
- `mf_exit.h` is copied next to the sandbox suspension source so its
  canonical `f1c_mod.py build` keeps working (that build was re-run: clean).

## 3. Panel

- `LOAD MERGED DLL` injects `dll/mechanical_failures.dll` once; all four tabs
  attach immediately (same blocks). Refuses to inject when a partial set of
  blocks is present (legacy DLL still loaded) - unload/restart first.
- Each tab's Inject button also injects the merged DLL. Unload buttons signal
  their family event; the merge monitor stops all families. The activity log
  says so.
- The four individual DLLs remain on disk as **legacy development
  artifacts**; do not inject them together with the merged DLL.

## 4. Verification so far

| Check | Result |
|---|---|
| `build_merged.bat` (`-Wall -Wextra -Werror`) | exit 0, no warnings |
| `nm` on merged DLL | 1 `DllMain`, `DllMain_gear/engine/susp/elec`, `mf_thread_exit`, `mf_monitor` present |
| Binary strings | all four `*_CTL` + all four unload events + family HUD strings present |
| Standalone rebuilds after the include edit | gear / engine / electronics / sandbox suspension all build clean |
| Panel smoke + contract tests | PASS, 18/18 (new `test_merged_dll_exists`) |

## 5c. Pending live validation (original pre-live list, kept as written)

1. Inject `mechanical_failures.dll` once - all four blocks attach
   (panel tabs live).
2. Arm each family (manual buttons) - behaviours unchanged from the separate
   DLLs.
3. Press Unload on any tab - all four workers stop and the module ejects;
   game stable. Re-inject after a rebuild (hot-swap loop).

## 6. Known limitations

- If a family refuses to start (e.g. suspension target validation fails), the
  exit-count can never reach the host target, so a later unload will not free
  the module - the game restart is the recovery. Documented edge, dev-only.
- The 50 ms grace is a pragmatic window, not a formal guarantee; it is far
  larger than the instruction gap it covers (all threads are already inside
  `ExitThread` when it starts).
- The four legacy DLLs are not deleted (preservation rule) and can still be
  built; only the merged DLL is a supported injection target now.

## 7. Preservation

Before-state (copy-only): `workspace_organization_audit/MERGE_BEFORE_2026-09-28/`
- all four family sources, panel, tests, product README, sandbox README,
plus the subsystem design doc before its merge note; `RESTORE.md` has
per-file SHA256. New files (not preserved, they did not exist):
`mechanical_failures.c`, `mf_exit.h`, `build_merged.bat`,
`mechanical_failures.dll` and its `.o` files.

## 8. Wasted-effort check

- Repeated reading of large material: none; the family tail/exit sites were
  located with one grep.
- Unnecessarily broad builds/tests: one failed build attempt from a wrong
  relative path (fixed in one edit); one `rg` scan from the workspace root
  hit a huge unrelated tree and timed out - will scope searches to the
  product directory from now on.
- Polling/waiting: none; live injection correctly deferred (game closed).

## 9. Lightloader startup (2026-09-28)

Owner request: "the .DLL is loaded with lightloader so I can get to testing
right now."

- The merged DLL now exports the Lightloader protected plug-in entry
  `rfe_1d3b7a91` and **does not activate from DllMain**. Activation runs only
  after the full ABI v2 host proof verifies: magic, ABI >= 2.0, product id,
  `LLP_CAP_EXCLUSIVE_HOST`, generation, live process id, host seal, and the
  loader-side proof (`llv_6a20c4e7` challenge/response) - the exact
  reference flow from `lightloader v1.1.0 protected_plugin_smoke.c`. Any
  mismatch returns FALSE and the loader frees the module (fail-closed), so
  an ordinary injector leaves it inert.
- Installed: `plugins\lightloader\mechanical_failures.dll` in the active game
  (`%USERPROFILE%\Documents\F1 Challenge V10`, the Desktop path is a
  junction). The installed Lightloader `dinput8.dll` was hash-verified equal
  to production v1.1.0 and exports the proof; `redflag.dll` already uses the
  same protected entry.
- **Dev exception:** `mechanical_failures_dev.ini` (next to the DEV copy
  only, not in the game folder) has `[Dev] AllowInjector=1`, which lets the
  test panel's `LOAD MERGED DLL` activate the dev-tree copy for hot-swap
  during development. Release packaging must ship without that ini - then
  only the Lightloader path activates.
- Host status block `F1C_MECHFAIL_CTL` ('MFHM', 32 bytes) published at
  activation: `host_verified`, `dev_injector`, `families_started`,
  `game_pid`. The panel shows a top line:
  `merged DLL: Lightloader host verified - 4/4 families` (green) /
  `dev injector (no host proof)` (amber) / `not loaded`.
- Expected next launch: `LightLoader.log` gains
  `Loaded plug-in mechanical_failures.dll.` and the panel connects with no
  injection step.

Verification so far: merged DLL builds clean; `objdump` shows the
undecorated `rfe_1d3b7a91` export; installed copy hash-matches the build;
panel smoke + 19/19 tests (new host-block contract test). Live launch test
pending (game was closed at install time).
