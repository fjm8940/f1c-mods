# D1 — corner-speed target vs logged 0x43FC (Monza)

Status: **DONE (PASS)**. Task from `tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_PHASE3_2026-10-02.md` (D1, STOP 1).
Read-only. Scripts: `phase3/analysis/d1_static.py`, `phase3/analysis/tireforcesolver.py`,

## D1.1 static — call structure (capstone on the exe, read-only)

`AIComputePassScore` = `sub_42A780`, 0x42A780..0x42A7C1 (`ret 4`; `__userpurge`, callee pops the one
stack arg). The real inputs are only `ecx` (vehicle) and the stack arg (waypoint); the Hex-Rays
`double a2@<st0>` is the return of the `TireForceSolver` call (DROPPED_ST0_SCAN; finding
`hexrays-dropped-st0-ai-brain-2026-10-02`).

| fact | asm evidence |
|---|---|
| `esi` = `a1` = **vehicle base** (not veh+0xC) | 0x42A791 `mov esi, ecx`; after the call 0x42A7A1 `fmul [esi+0x4504]`, 0x42A7A7 `fadd [esi+0x4500]`, 0x42A7AD `fmul [esi+0x4580]`, 0x42A7B3 `fmul [esi+0x453C]`, 0x42A7B9 `fmul [edi+0x84]` |
| `edi` = waypoint pointer (stack arg) | 0x42A782 `mov edi, [esp+0xc]` |
| TireForceSolver `ecx` = **veh+0xC** | 0x42A799 `lea ecx, [esi+0xC]` just before 0x42A79C `call 0x4E8CE0` |
| operand order `TFS(a2=wp+0x74, a3=wp+0x78 ptr, a4=wp+0x70, a5=16.0)` | 0x42A786/89 `mov eax,[edi+0x70]`, `mov edx,[edi+0x74]`; 0x42A78C `push 0x41800000`; 0x42A793 `push eax`; 0x42A794 `lea ecx,[edi+0x78]`; 0x42A797 `push ecx`; 0x42A798 `push edx` (thiscall pushes a5,a4,a3,a2 right-to-left) |
| return = `(TFS * veh[0x4504] + veh[0x4500]) * veh[0x4580] * veh[0x453C] * wp[0x84]` | 0x42A7A1..0x42A7B9 (confirmed operand order) |

Callers: the exe contains **exactly 4 direct call sites**, all inside `AIRacingLine` (0x425920), and
**zero absolute dword pointers** to 0x42A780 anywhere in the image (no indirect/table calls).

| site | AIRacingLine role | path taken |
|---|---|---|
| 0x425D77 | L205 reachability test #1: `AIComputePassScore(wp) < speed*1.05` | compare only, value not stored |
| 0x425D90 | L205 test #2: same for `*(wp+0x110)` (next wp) | compare only |
| 0x425DBC | L206-209 **limiting-waypoint target**: `v50 = AIComputePassScore(wp) * veh[0x4554]` (0x4554 = 1.0 in every sampled tick) | bypass 1: `wp+0x134 == 1` -> `(double)(int)wp+0x138` (`fild`, branch_val) |
| 0x425EBC | L243-260 projection loop, target of each next wp | bypass 2: `wp+0x134 == 1` -> `flt_8ACFB4` global pit speed (`fld [0x8acfb4]`); bypass 3: linked-car block (`[wp+0x114]` -> `+0x12C == chain id` and id != 1 and id != 0) -> fixed `12.0` (`0x41400000`); else AIComputePassScore |

The stored pair 0x43F8 (`limiting waypoint`) / 0x43FC (`target`) is set at L298-299 from the loop's
current `v5`/`v50`, so 0x43FC is exactly the D1.1 formula value (times 0x4554, here 1.0).

## D1.2 model — `phase3/analysis/tireforcesolver.py`

Model of `TireForceSolver 0x4E8CE0` from `functions/TireForceSolver_004e8ce0.c` + full function
disassembly (`d1_static.py`). `this = veh+0xC`; five `_frnd` (`sub_5895A2`) returns are stored, so
the table index uses the rounded value and the interpolation fraction uses the raw value
(DROPPED_ST0_SCAN, review 11). Seed `= |wp+0x70| * veh+0x3D08`; table `veh+0x3D0C`, clamped to 128;
interval `[v8-4, v8+4]`; per-speed force from `veh+0x3A7C/0x3520` (this[3740]/[3397]), index from
`A = w^2*this[3740] - (w^2*wp+0x74 + wp+0x7C)*this[3397]` times `this[3871]`/`this[3855]`, value from
`veh+0x3C8C`/`0x3C4C` tables, `± B` with `B = (w^2*wp+0x70 - wp+0x78)*this[3397]`; interval stepping
and the root interpolation resolved from asm 0x4E8FD3..0x4E909C.

Every table it reads exists in `cars_static.jsonl` (veh 0x3A7C..0x3D50) plus `extra.v3520`; MISSING is
empty — no `RECORDER_REQUESTS` entry needed. Recorded-log quirk worked around in the replay:
GUESS field `unnamed_4554` is logged as raw int bits (1065353216 = 1.0).

## D1.3 replay — `d1_corner_speed.py`

Per snapshot with non-null `limiting_waypoint_ptr`, target != 9999, waypoint present in the 20 logged
chain entries, `branch_flag == branch_val == 0`, latest `cars_static` row (same slot, sim_t <= snapshot):

| set | n | median meas/pred | p10 | p90 | min | max | within +-2% |
|---|---|---|---|---|---|---|---|
| all (with *0x4554, = doc-54 form) | 4242 | **0.9993** | 0.9976 | 1.0001 | 0.9884 | 1.0339 | **100.0%** |
| watch_race4fullc (start) | 150 | 0.9992 | 0.9989 | 0.9994 | 0.9989 | 0.9997 | 100.0% |
| watch_race4fulld (to lap 37) | 4092 | 0.9993 | 0.9976 | 1.0001 | 0.9884 | 1.0339 | 100.0% |

- `0x4554 = 1.0` in all 4242 samples (raw int 1065353216), so doc 54's formula and the code form are identical.
- By target bin, median ratio: 0-20 m/s 1.0001, 20-40 0.9993, 40-60 0.9990 — no speed dependence.
- By driver all medians 0.9987..0.9996 (18 drivers) — no car dependence.
- 10 worst cases are 0.9884..1.0339; largest is one Albers tick (pred 43.6 vs meas 45.1, k120 = -0.40);
  the rest are within 1.2%. A small systematic -0.07% offset is consistent with the 60 s
  `cars_static` sampling of slowly drifting tyre tables.
- Exclusions: 85343 ticks with null limiting waypoint (target 9999), 683 branch samples,
  291 snapshots whose limiting waypoint is deeper than the logged 20 chain entries, 0 missing static rows.

## Verdict

**PASS** — median within +-2% and p10..p90 within +-5% (both far inside). The DERIVED doc 54 formula
is live-confirmed on Monza for every valid sample; no constant tuning was applied (the one model bug
found during the run — table index selector multiplies `A`, not `B` — was fixed against the
disassembly, not against the data). Claude still owns the findings.jsonl entry.

## Limitations / open

- SC/RF mods were active in both runs (NOTES.md); no SC/RF marker exists in the brain logs. The
  formula is a deterministic function of logged per-car state and matched 100% anyway; nothing was
  filtered on SC/RF. Wet/second-track validation still owed (D11).
- 291/4626 candidate ticks had the limiting waypoint beyond the 20 logged chain entries (deep
  projections); a `--waypoints N` / target-per-waypoint recorder request would cover them (already
  tracked as RECORDER_REQUESTS A1/A2, accepted only for 6 of 20 at the time of these runs; the 20
  present in watch_race4* were enough here).
- The solver's `return 128` / `return 0` guard paths were exercised only through the model, not
  independently verified live; the 100% replay agreement makes a systematic error there unlikely.
