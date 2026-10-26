# Hook validation plan (work-left item 1) — adaptive_ai_difficulty v0.1.0-dev — 2026-10-15

Status: PLAN ONLY. Nothing run. Artifact under test: `dll/adaptive_ai_difficulty.dll`
sha256 `495d7eb058e2029a018ca6ba5376daa08c5f16effc62a4ee4735a4a96e5e155a` ().
Spec: `HOOK_SPEC_2026-10-15.md` sections 8-10 (tests A1-A9, R-1..R-8). Runner: `tools/aad_race_runner.py`
with the H26 columns (`max_speed_kmh`, `offtrack_events`, `offtrack_samples`, `surface_samples`).
Probe: `tools/strength_probe.py` (stored strength B = veh+0x3C00 per car, all pids).

## What is already known (do not redo)
- MEASURED (Run 9, Austria 2001): x1.05 = -1.51 s/lap; HOOK installed, APPLY x21, slot 0 pass-through, no REFUSED.
- Observed in Run 9 `hook_log.txt`: slots 13, 14, 17 reached `calls 3` at  with factor 1.050, i.e.
  the stub saw a third native re-init (likely pit) and applied the factor. DERIVED: re-init keeps the factor.
  Missing: B probe after the pit.
- Side effects open: earlier AI pits (bit-4 fuel use), 18 vs 21 cars running late (cause unknown).
- FACT (V0, installed Lightloader 2.3.25, `dinput8.dll` sha256 226356C7..., source
  `development/lightloader/v2.0.0-hub-dev.1/replay_naming_dev_2026-10-13/lightloader_core.c`): plugin handles are
  "intentionally retained for the lifetime of the game process" (line 189); `FreeLibrary(plugin)` only on failed
  init (line 492). So "unload" = process exit only. (An earlier draft cited the old v1.2.0 tree by mistake.)

## Session V0 — agent only, no game (static)
1. Spec 9.1: capstone decode 0x424790..0x424AAB of `F1 (1).exe`; prove x87 stack empty at 0x424AA6. Tool in %TEMP%.
2. A1: compare the 37 + 10 + 16 pattern bytes in all four exes (read-only file reads).
3. Check the current Lightloader for runtime unload / reload (FreeLibrary paths). Decides A8b.
4. Write `tools/aad_refusal_launch.py` (admin): start the game CREATE_SUSPENDED, write ONE equivalent byte in
   process memory only (0x424795 `33 db` -> `31 db`, both = xor ebx,ebx), verify, resume. Exe file untouched.
   py_compile only.
Pass: 9.1 closed (FACT), A1 table, unload answer, launcher compiles. Record in this file + findings.

## Session V1 — one instance, functional (owner ~40 min)
Setup: Austria 2001, 50 % race (pits happen), dry, slider 100, player retired at start, ini `Enabled=1`, `AllAIFactor=1.000`.
| Step | Action | Pass |
|---|---|---|
| A3 | probe at grid | every AI B == native 0.988 (1e-6); log `HOOK installed` |
| A5 | lap 5: ini `Slot5=1.100` | log APPLY slot 5 within ~4 s; probe B5 = 0.988 x 1.10; other cars unchanged |
| A7 | same probe | player B unchanged (0.988) |
| A6 | after slot 5 pits (runner `pit` column) | probe B5 still = 0.988 x 1.10; log `STUB slot 5: calls` +1 |
| A8a | lap 20: remove `Slot5` | APPLY back; probe B5 == 0.988 (1e-5) |
| R-4 | add `Slot7=abc`, `AllAIFactor=2.0` | log "not a number" fallback, clamp to 1.20 logged; then set back to 1.000 |
| A8b | quit game | log UNLOAD lines (expected per V0 step 3); no crash dialog |
Runner: `run --plan none` (log only). Agent analyses laps.csv: slot 5 lap delta while x1.10, `surface_samples > 0`
for AI (H26 live check), off-track counts sane.

## Session V2 — refusal path (owner ~5 min, no race)
R-1: start via `aad_refusal_launch.py`, load to any grid. Pass: log `REFUSED AAD-P1`, no `HOOK installed`, probe B ==
native for all AI. R-2 (foreign bytes at the call site) shares the `bytes_match` path; not tested separately (DERIVED).
R-3 (MP/CO-OP/replay) waits for the CO-OP item (H25).

## Session V3 — second track, parallel (owner ~1 h, AI-only, like Run 9)
Track: Monza 2001 (high speed: shows gearing/top-speed side effect via `max_speed_kmh`). 100 %, dry, slider 100,
all RA 1.0. A = `Enabled=0`, B = `Enabled=1 AllAIFactor=1.050`. Load one at a time.
Analysis (agent): median clean-lap delta + bootstrap CI (same filter as Run 9), pit count/laps A vs B,
`max_speed_kmh` A vs B on clean laps, `offtrack_events` per 100 laps, cars running at the end + cause (off-track
before retirement?). Pass: delta measured with CI; side effects quantified. No pass/fail threshold on pace.

## Afterwards
- Optional V4 (overnight): x1.10 vs native, same filter, for crash rate (complements list).
- Update `dll/README.md` open items, baseline README (Run 10+), findings (`aad-hook-*`), PRODUCT_STATUS only if the
  stage really changes.
- MUST RESTORE list in `HANDOFF_NEXT_2026-10-15.md` still applies after the last session.

## Abort conditions
Crash on load or at grid; any `HOOK FAILED` / `UNHOOK FAILED`; probe B of the player changes; game frame
stops advancing (check sim time, not identical samples). Then: ini `Enabled=0`, record, stop.

## V0 results — 2026-10-15 (agent, static, no game)
1. Spec 9.1 CLOSED (FACT, capstone on `F1 (1).exe`): x87 depth at 0x424AA6 = 0. Path: `call 0x424AC0` returns one
   value on both branches (fall-through ends `fadd` depth 1; jg branch 0x424AFE ends `faddp` depth 1), popped by
   `fstp [esp+0x30]` at 0x424A78; optional `fld/fmul/fstp` at 0x424A90-0x424A9A is net 0. Earlier x87 ops at
   0x424838-0x424878 balanced. The stub's empty-stack assumption holds.
2. A1 PASS (FACT): 37-byte block 0x424A90, prologue 0x424790, callee 0x4F1510 match in all four exes:
   `F1 (1).exe` d8fb5722..., `modified f1 challenge 99-02.exe` 3d9592ee..., live `F1 Challenge Online.exe`
   1a70e5ed..., clean `F1 Challenge Online.exe` de151845... (sha256 prefixes).
3. Unload (FACT): installed Lightloader 2.3.25 keeps plugins for the process lifetime (see "already known").
   A8b = observe UNLOAD log at game exit only; mid-race restore (A8a) is the real restore test.
4. `tools/aad_refusal_launch.py` written (compile OK, not run). Flip 0x424795 `33 db` -> `31 db` in process
   memory; capstone confirms both = `xor ebx, ebx`. Default exe = live `F1 Challenge Online.exe` (poke PROC_NAME);
   `--dry-run` reads only and terminates the suspended child. Manager source has no game-launch code, so the owner
   starts the exe directly; the launcher does the same. Not proven: that the DLL's check runs after the resume
   (expected: Lightloader loads plugins in-process after start).
Next: V1 (owner, one instance).

## V1 owner steps (prepared 2026-10-15, not run)
Tools added for V1 (compile-checked; aad_ini.py tested on a %TEMP% copy only): `tools/aad_ini.py` (show/set/del keys
in the installed ini), `tools/strength_probe.py --all --watch N --out F` (all live slots + laps + pit state, timed).
State found before V1: installed ini `Enabled=1 AllAIFactor=1.050` (left from Run 9); DLL 495d7eb0... installed;
PLR AI usages 1.00 (test state, MUST RESTORE later).
1. Game closed. Normal pwsh: `python "...\tools\aad_ini.py" set Enabled=1 AllAIFactor=1.000`
2. Start the game. Austria 2001, race 50 %, dry, normal fuel, slider 100. Grid, retire the player as in Run 9.
3. Admin pwsh #1: `python "...\tools\aad_race_runner.py" run --plan none`
4. Admin pwsh #2: `python "...\tools\strength_probe.py" --all --watch 20 --out "$env:TEMP\aad_v1_probe.txt"`
5. Leader lap 5: `aad_ini.py set Slot5=1.100` (A5/A7). 6. Leader lap 22: `aad_ini.py del Slot5` (A8a).
7. Leader lap 30: `aad_ini.py set Slot7=abc AllAIFactor=2.0`, ~10 s later `aad_ini.py set AllAIFactor=1.000`, then
   `aad_ini.py del Slot7` (R-4: non-number fallback + clamp to 1.20 logged; all AI at x1.20 for ~10 s).
8. Race end: quit the game normally (A8b UNLOAD log). Runner and probe stop by themselves.
Agent afterwards: copy runner folder, probe file, hook log into `baseline-2001-equal-ra-2026-10-14/run-10-v1-.../`,
check A3/A5/A6/A7/A8/R-4 and H26 columns. A6 needs slot 5 to pit while at x1.10 (not certain in a 50 % race).

## V1 + V3-A in parallel — started 2026-10-15 (owner asked: agent runs it, parallel instances)
- Shared ini = one hooked instance only. Game 1 (pid 22968, started  loaded `Enabled=1` (hook installed);
  agent set factors to 1.000 at 23:21 (live, before any race). Agent then set `Enabled=0` so game 2 (Monza native,
  V3-A) loads unpatched. DLL reads Enabled only at startup (`ada_load_ini`, `if (startup)`).
- `tools/aad_v1_orchestrator.py --instances 2` (new, compile OK) launched elevated: waits until both games race,
  picks the hooked pid by the 0x424AA6 bytes, starts one runner over all pids + probe watch, ini steps by the V1
  leader lap (5 / 22 / 30). Output `%TEMP%\aad_v1_20261015T202224Z\`.
- Later pair: V2 refusal + V3-B (Monza x1.05, loaded with Enabled=1 AllAIFactor=1.050).
- Owner decision 2026-10-15: Monza V3-A and V3-B both at 50 % race distance (not 100 %). Fewer pit stops, so the pit
  side effect is weaker; pace delta and max speed unaffected.
- Interim 20:35 UTC (FACT, hook log + probe): A3 pass at the grid (all AI B 0.988 at factor 1.000). A5 pass: ini
  Slot5=1.100 at 23: -> hook log `APPLY slot 5: native 0.988 x factor 1.100 -> B 1.0868 (was 0.988)` at
  23:32:02.753 (1.6 s); probe slot 5 = 1.08680, slot 4 = 0.98800. Monza instance (Enabled=0) unaffected: every AI
  0.98500. Retired player B = 1.00000 in both (GDS only, matches prior finding). H26 columns live: max speed
  306 km/h (Austria) / 329 km/h (Monza), AI surface_samples ~145 per lap (material read works on AI).
- V1 RESULT 2026-10-15: A3, A5, A7, A8a, R-4 PASS; A6 not covered (no pit at 50 %); A8b N/A by design.
  Details: baseline README Run 10. Next: V2 refusal launch, V3-B Monza x1.05 (night2), A6 via Austria 100 % (night2).
- V3 status 2026-10-16 (night1, Run 11): second-track requirement met on 4 tracks (Monaco, Hungaroring,
  Indianapolis, Melbourne): hook x1.05 = -1.15..-1.34 s/lap, relative S -0.28..-0.31. Still open: V3-B Monza x1.05
  (night2), A6 pit re-init (night2 Austria 100 %), wet (night2), x1.10 side effects (night3), V2 refusal launch.
- PLANNED 2026-10-16: slider ladder (`aad_night_launcher.py --plan slider`): Austria 2001, 50 %, dry, all native,
  sliders 70/80/85/90/93/95/97/100, one per window. Purpose: measure the slider below 100 (never measured; Run 8
  covered 110/120), check the code formula GDS = Worst/Mid/BestAdjust interpolation (Austria 0.85/1.00/1.10 ->
  slider 93 = 0.965), and translate a player's slider into the mod's factor scale.
- 2026-10-16 night2 + slider ladder (Runs 12-13): A6 PASS (factor survives AI pit stops). V3-B Monza x1.05 done
  (-1.26..-1.68 s, not parallel). Wet: lever ~40 % stronger. Slider ladder 70-100 measured. Still open: V2 refusal
  launch, x1.10 side effects (night3), hook below 1.00 (never tested), hook at slider != 100, CO-OP.
- 2026-10-16: DLL 0.1.1-dev (sha256 89e2f7ce..., AAD_INI per-process ini) built and installed; launcher plan
  `factor93` = Austria slider 93: native + factors 0.85/0.90/0.95/0.975/1.00/1.05/1.10 in one parallel batch (tests
  the hook below 1.00, on top of a non-100 slider, and the identity at 1.00). Not run yet.
- 2026-10-16 Run 14 (factor93): hook below 1.00 PASS (linear to 0.85, symmetric), identity x1.00 PASS, per-instance ini
  PASS, x1.10 not flattening. Still open: V2 refusal launch, CO-OP, hook > 1.10 (friend Nurburgring needs ~1.12).

## Live dry-run plan (rating logger + nudge controller), prepared 2026-10-16, not run
Tool: `tools/aad_live_dryrun.py --track austria_2001` (admin, one UAC; read-only: no game writes, controller DRY on
an ini copy, no store writes). Owner drives Austria 2001, race ~15 % (about 11 laps), dry, own slider; game started
normally (shared ini Enabled=0 = vanilla). Owner actions: start in the pack (traffic laps), run wide off the track
once around lap 4, cut a chicane/kerb once around lap 6, otherwise clean laps. Checks (agent): every car's name read
(cars.txt), player car + AI teammate found, slider value, laps classified (traffic/off-track/cut reasons fire on the
right laps: live check of 0x40E8 and 0x4104), teammate gap per lap and in the summary, controller DRY decisions every
3 clean laps and the lock at 1/3. Note: RCD files are still in the equal-RA test state (all AI RaceAbility 1.0).
Fixed before the run: `aad_rating_logger.py --help` crashed (unescaped %TEMP% in a help text).

## Live dry-run RESULT 2026-10-16-09:25 UTC (owner driving, keyboard; output %TEMP%\aad_dryrun_20261016T090455Z)
Game pid 1348, slider 93 (read live), race 22 laps, vanilla (shared ini Enabled=0). Player car "2001 Prost", AI
teammate found automatically: Juan Pablo Montoya (2001) (car name veh+0x4678 read for all 22 cars: FACT).
Track label passed as austria_2001, but lap times (~105-112 s, AI ~106-110 s) do not fit Austria (AI ~74 s at slider 93);
22 laps fits Spa 2001 at 50 % -> the label is probably wrong (owner to confirm; the tools still need the track from the game).

| lap | time | clean | reasons | teammate lap | gap to teammate |
|---|---|---|---|---|---|
| 1 | 115.172 | no | first lap, lap < 2, traffic | - | - |
| 2 | 109.288 | no | off_track, traffic | - | - |
| 3 | 108.132 | yes | | 109.749 | -1.617 |
| 4 | 107.617 | no | traffic | 109.292 | -1.675 |
| 5 | 107.162 | yes | | 109.814 | -2.652 |
| 6 | 105.151 | no | off_track, cut | 109.384 | -4.233 |
| 7 | 106.419 | yes | | 109.600 | -3.181 |
| 8 | 105.766 | yes | | 109.313 | -3.547 |
| 9 | 111.938 | no | off_track, traffic | 109.357 | +2.581 |

- FACT (owner-confirmed events): off-track byte 0x40E8 fired on the laps the owner ran wide (2) and spun (9); the cut
  counter 0x4104 fired on lap 6 (owner cut + ran wide there) but NOT on the lap-4 chicane cut -> mild cuts can be missed
  (counter increments only above a severity threshold per the decompile). Traffic and first-lap exclusions fire.
- Controller (DRY, Realistic): first decision after 3 clean laps (lap 5): gap -1.608 s vs target -> step 1.050 -> 1.053
  (step cap); LOCK after leader lap 8 (> 22/3). Nothing written.
- Bug found and fixed: the controller and aad_apply read AllAIFactor 1.050 from the shared ini although Enabled=0 (AI at
  native); both now return 1.00 when Enabled != 1 (tests 24/32 OK).
- Earlier attempt (pid 21456): the owner retired; no player laps; the controller then saw the leader jump to lap 12 in
  3 minutes (the game finishing the race in simulation) and locked: harmless.
