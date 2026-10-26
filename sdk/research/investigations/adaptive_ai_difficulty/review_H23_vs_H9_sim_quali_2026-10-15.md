# Review: H23 vs H9, simulated (skipped) qualifying draw count — 2026-10-15

Reviewer: Claude (Opus 5.5), static, read-only. Resolves finding `aad-simulated-quali-grid-2026-10-15` (OPEN).
Exe read: `F1 Challenge V10\F1 (1).exe` (same code as the clean copy at the hook site per HOOK_SPEC), capstone
disassembly of 0x4BEB10..0x4BEF40. The decompile `functions/AIOvertake_004beb10.c` stops at
`JUMPOUT(0x4BEC10)`; the per-lap loop is in that undecompiled tail.

## Verdict
- H9 is right: the sim best lap is the MINIMUM over many draws (one draw per simulated lap). H23 C1 is wrong.
- H23 rest stands: base B and the strength fallback factor (FACT), and the shift write on brain+844 (DERIVED).

## Evidence (FACT, asm addresses)
Brain = 868-byte stack struct `v7` in sub_46BF10 (ebp in sub_4BEB10); sub_46BF10 loops
`while (sub_4BEB10(...))`. Step dt = brain+0x350 = 10.0 s (init 1092616192 in sub_4BE700).
- 0x4BEBAD-0x4BEBC6: init (sub_4BF0A0) only while brain+0x342 == 0. Init does the first draw per car.
- 0x4BEC10-0x4BEC1C: every tick, per car slot k: elapsed[k] (brain+0x1A0+4k) += 10 s.
- 0x4BEC25-0x4BEC8C: skip car if veh+0x32C8 in {-1, 0}, laps veh+0x403C >= brain+0x35C (lap cap),
  non-race and laps > sub_42AE90(veh), or veh+0x4064 in {1, 2, 3}.
- 0x4BEC92-0x4BECB5: if lap[k] (brain+4k) < 0.0 then lap[k] = RandomVariationScaler 0x4BEA10(veh, brain+0x34C).
  brain+0x34C = 844 = base B. NEW DRAW EACH LAP.
- 0x4BECB7-0x4BECC4: lap completes when elapsed[k] > lap[k].
- 0x4BECCA-0x4BEE09: sub_4BEF60(veh, 1.0) returns nonzero = event path (sub_4B33B0 / sub_4BBFC0, random
  type 1-9 from table 0x8AAE74); lap not counted. Meaning OPEN (likely sim incident).
- 0x4BEE0E-0x4BEE6C: laps++, best lap veh+0x409C (16540) = min(best, lap[k]).
- 0x4BEEC7-0x4BEED5: elapsed[k] -= lap[k]; lap[k] = -1.0 (forces the next draw).
- Loop ends per car at the lap cap or the stop flag; session ends by time (sub_46BF10: v6 = flt_8ACF94 -
  flt_8ACF90 for sessions 1-6).

## Consequences
- Simulated quali grid = min of N draws, N = laps the car fits in the session time (capped by brain+0x35C and,
  outside the race, sub_42AE90). More draws = faster best lap, smaller spread than a single draw.
- Mod shift write (H23 section 4) still valid: brain+844 is set once by sub_4BE760 inside init and then read by
  every draw (0x4BECA6). The brain is a stack local of sub_46BF10, so the write must be a code hook after
  0x4BF0A0's call to sub_4BE760 (or a multiply on the RVS return at 0x4BECB5 and in init L89), not a memory poke.
- H23 C2/C3 corrections (strength term uses s/120, not ability; physics strength does not reach the sim) agree
  with H9 and the code.

## Open
- sub_4BEF60 event meaning and rate; 0x42AE90 per-car lap cap in quali.
- pow operand order in RVS (H23 O2); GDB 'Qualify Laptime' units (O4).
- Live check: skip a qualifying, compare grid times against B and draw count.
