# Claude review 14 — phase 3 D1 corner speed (2026-10-02)

Verdict: **PASS. Corner-speed target formula promoted DERIVED -> OBSERVED (Monza, dry).**

Reviewed: `phase3/D1_CORNER_SPEED.md`, `phase3/DS_LOG.md`, `phase3/analysis/d1_corner_speed.py`.

## Independent checks (Claude)

- Disassembly of 0x42A780..0x42A7C0 re-read with capstone: matches the D1 table exactly (`esi` = vehicle,
  `lea ecx,[esi+0xC]` before `call 0x4E8CE0`, pushes 16.0 / wp+0x70 / wp+0x78 ptr / wp+0x74, then
  `fmul [esi+0x4504]`, `fadd [esi+0x4500]`, `fmul [esi+0x4580]`, `fmul [esi+0x453C]`, `fmul [edi+0x84]`).
- Replay re-run: reproduces n=4242, median 0.9993, 100 % within +-2 %.
- **Correction to the sample count:** overlapping situation windows duplicate snapshots. Deduplicated by
  (run, slot, sim_t): **n=3549 unique ticks**, median 0.9993, p10 0.9977, p90 1.0001, min 0.9884, max 1.0339,
  100 % within +-2 %. Verdict unchanged.
- Coverage: 22 car-runs, **35 distinct limiting waypoints** (Monza has few braking corners). One track, dry,
  SC/RF mods active (not marked in logs; the formula is a pure function of logged state, so no filtering needed).
- verify_maps, verify_maps_phase2, test_abi, test_extract_offsets: exit 0.

## Accepted

- Formula: `0x43FC = (TireForceSolver(veh+0xC; wp+0x74, &wp+0x78, wp+0x70, 16.0) * 0x4504 + 0x4500) * 0x4580 * 0x453C
  * wp+0x84 * 0x4554` (0x4554 = 1.0 in every sample).
- 4 direct callers, all in AIRacingLine, no indirect references. Bypasses: wp+0x134 == 1 -> int wp+0x138 (limiting
  waypoint) / pit speed `flt_8ACFB4` (projection loop); linked-car block -> 12.0.
- DeepSeek's TireForceSolver model fix (index selector multiplies A, not B) was made against disassembly, not data.

## Follow-ups (Claude)

- `brain_core.py`: `unnamed_4554` is logged as raw int bits; registry type should be float (fix with the D2 tier merge).
- Second track / wet / bypass branches: D11. Deep projections beyond 20 logged waypoints (291 ticks): not needed.
- Finding `ai-brain-corner-speed-live-confirmed-2026-10-02` appended; doc 54 section 3b updated.
