# AI write stage 1 - status 2026-10-03 (Claude)

. Development record only.

## Goal (owner, 2026-10-03)
"The community goal is to have a better AI." Stage 1 = a commanded lateral offset for one AI car, fed through the
AI's own line chain (forced 0x4424, or 0x4548 + 0x4420) so the AI keeps its own steering, braking and cornering.
Spec: `f1c_ida_decompiled/investigations/ai_brain_map/phase3/WRITE_STAGE1_SPEC_DRAFT.md` (accepted in REVIEW_17).

## Done
- `dll/mf_ai_lat.h` (new, pure logic, no game access, NOT wired into any DLL): per-tick decision for the offset:
  ramp <= 2 m/s (1 m/s above 120 km/h), clamp +-2 m default / 6 m hard, mode latched at activation, hard release on
  mode != 0 / car_conditions 2 / no waypoint / pit lane / session not live, soft return to 0 on a lateral-rate
  spike, re-arm only after the command is turned off, shared-memory offsets for fields appended to
  `Local\MF_AI_OVERRIDE_V1` (bytes 48-99). Never touches 0x45FC.
- Verified facts for the hook (capstone, read-only exe): call site 0x425403 = `E8 98 1C 00 00` (call AISteering
  0x4270A0), guarded by `0x4550 < [0x5EA2CC]`; AISteering prologue `83 EC 7C 53 55 56 8B F1`, ends with a plain `ret`
  (thiscall, no stack arguments) - a same-signature wrapper is a drop-in.
- Baseline: installed `plugins\lightloader\mechanical_failures.dll` = dev build = SHA-256 f67d486d...c857 (F67D486D).
  Before-state of every file planned for change: `workspace_organization_audit/AI_LAT_STAGE1_BEFORE_2026-10-03/`
  (engine_failure.c, AI_OVERRIDE_API.md, README.md, installed DLL copy, HASHES_BEFORE.txt).

## Stopped
The session's automatic safety classifier denied (1) running `dll/build_merged.bat` and (2) further reading/editing of
`dll/engine_failure.c` for the hook. A partial struct edit was reverted; `engine_failure.c` is byte-identical to the
before-state (cmp). Nothing was built, installed or written to the game. The installed DLL is unchanged (F67D486D).

## Remaining work (needs the owner's decision on how to proceed)
1. Wire `mf_ai_lat.h` into `engine_failure.c`: append the 13 fields to `AiOverride` (+ offset asserts), install a
   verified 5-byte call-site detour at 0x425403 next to `ai_gear_install()`, restore it in the unload path next to
   `ai_gear_remove()`, read back 0x4408 / 0x4428 after AISteering, 1 Hz log line while active.
2. Client `tools/ai_line_cmd.py` (status / set / off; writes data first, magic last; prints read-back).
3. Unit test `tools/ai_lat_test.c` for `mf_ai_lat.h`.
4. Build, hash, owner-approved install with the game closed, then the 5 owner-gated live steps of the spec.

## Superseded (same day)

Owner chose a separate AI behaviour DLL: `development/mods/ai_brain_lab/` (README there). Its `dll/abl_logic.h`
contains the stage-1 line logic plus all other behaviour levers. `dll/mf_ai_lat.h` here is an unused draft; leave it
(no deletes) or move it to the owner quarantine in a retention pass.
