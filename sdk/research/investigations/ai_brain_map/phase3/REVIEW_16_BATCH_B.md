# Claude review 16 — phase 3 batch B (D4-D15) + registry v1.3 (2026-10-02)

Verdict: **PASS. Registry v1.3 built** (`ai_brain_abi_v1_3.json`, script `phase3/merge_abi_v1_3.py`):
27 OBSERVED / 77 DERIVED / 36 GUESS (v1.2: 23 / 67 / 50; v1.1: 15 / 68 / 57).

## Checks

- verify_maps, verify_maps_phase2, verify_maps_phase3, test_abi (now also v1.3), test_extract_offsets: exit 0.
- D10 spot check (capstone, Claude): 0x4293F0..0x42942B = `mov [esi+0x4408], [esi+0x440C]`; if byte 0x4548
  `mov [esi+0x4408], [esi+0x4420]`; then 0x4424 compared with 0.0 and stored if non-zero. Matches the report.
  0x424838 `fld [esi+0x396C]` precedes the 0x44B4 store. Accepted.
- Viewer v3 replay (Claude, independent code path) on wet brake zones: model/game median 0.9987 (n 32) agrees with
  D11a (n 733, 0.9993).

## Accepted results (headline)

| task | result | evidence tier |
|---|---|---|
| D11a | corner-target formula holds in the wet (n 733, med 0.9993, 100 % within 2 %); braking step med 1.000; wet targets 75 % of dry at the same corners, decel 66 %, lap time +20 %; 59 tyre-model entries change wet vs dry | OBSERVED (one wet race) |
| D4 | pace multiplier model explains 5465/5467 (Monza) and 1399/1399 (wet) changes; 4 s cadence; traffic-driven, no rubber-banding vs the player | OBSERVED |
| D5 | 59 RandLCG sites, 25 AI; no random driving mistakes; randomness = start ramp/stalls, pace gate, profile variant, incident gates. `sub_5881F0` = CRT pow, return consumed (REVIEW_15 suspicion resolved: not dropped) | DERIVED static |
| D6 | opponent modes 16/5 = approach / hold, 1/2/7/8 = attempt / alongside; 0x4578/79 fire in attack modes | OBSERVED context, names DERIVED |
| D7 | field launches within 0.10 s median; randomness is the pre-launch ramp + cap jitter | OBSERVED |
| D8 | pit decision chain sub_429960 -> AIPushLap / tyre < 0.9 -> call_to_box -> mode 4 -> release; doc 53 Q2 solved (0x458C int) | DERIVED + OBSERVED |
| D9 | traction trigger 5.4 % of Monza samples (gears 1-2); brake_on_slip never fired | OBSERVED, magnitude open (R2) |
| D10 | 0x4408 writer = AISteering epilogue; 0x44B4 init-only; no pointer writer. Doc 54 section 7 closed | FACT-level code read, Claude-checked |
| D12 | write readiness for all 140 fields; ranked top-10 (driver_status, 0x4424, 0x4420+0x4548, speed_limit, max_speed, 0x44CC, 0x44F8, 0x4534, 0x4588) | DERIVED |
| D13 | vanilla damage: wing records force pit/retire; car_conditions 2 = dead coast, no AI retire decision | DERIVED |
| D14 | waypoint record 316 B, every AI-read offset with readers and live ranges | DERIVED/OBSERVED |
| D15 | 14 promote-only fields merged into v1.3; 36 GUESS left, each with one settling action | - |

## Open (Claude / owner)

1. D14 branch_flag float-bit records on Monza: check before using branch paths (write stage 1 does not need them).
2. D4 two stopped-car outliers (SC/RF suspect) - ignore unless repeated.
3. R2 recorder request (raw slip inputs + clamp target) for the traction-cut magnitude.
4. Doc 53 Q1/Q3/Q4/Q5 not answerable from brain logs (need pit-side capture).
5. D6 rank-vs-team-record pairing caveat stands for any per-opponent claim.
6. Wet race SC/RF state unconfirmed by owner.
7. 36 GUESS fields: list in `phase3/GUESS_V12_NEXT.md`.

## Changed files

New: `ai_brain_abi_v1_3.json`, `phase3/merge_abi_v1_3.py`, this review. Edited: `test_abi.py` (v1.3 check),
`mods/sdk_sandbox/ai_mind_monitor/brain_core.py` (prefers v1.3, opponent-mode names from D6),
`brain_explain.py` (pace / speed tiers), `docs/54-ai-driving-brain-map.md` (section 7 + summary), findings.
