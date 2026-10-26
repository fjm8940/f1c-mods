# Claude review 3 — REVIEW_2 additions + group 2 (2026-10-02)

Verdict: **PASS.** Re-run: B1 test OK, verify_maps 0 mismatches (15 maps). Group 2 meaning is coherent with prior art
(corridor 0x4414/0x4418, request 0x4420, working target 0x440C, published offset 0x4408).

Decisions:
1. Extractor upgrade (a1 tracking, `*((TYPE *)base + N)`) accepted. **Regenerate offsets_ALL.csv** from all per-function
   CSVs now (it is your own generated output; overwriting it is allowed). Log it in DS_LOG.md. Rule 1 deviation accepted
   for generated CSVs only.
2. AIOvertake 0x4BEB10 flag noted: keep the map, mark "not AI driving" in Identity; exclude from the brain glossary.

Correction for map_AISteering_part5.md epilogue (Claude, source L1293-L1313; `this` is char*, float index *4):
- 0x440C = max/min update from v145/v146 (corridor result).
- 0x4408 = 0x440C; **if byte 0x4548 != 0 then 0x4408 = 0x4420 (request used directly)**; if float 0x4424 != 0 then
  0x4408 = 0x4424 (last word). So 0x4548 is an override-enable, not an "unless" block, and 0x4424 wins over both.
- If 0x4408 >= 0x4414 or <= 0x4418, byte 0x4140 = 1 (outside corridor flag). Then byte 0x4488 = 0 (the waypoint re-scan flag used by sub_426430). 0x4548 is NOT cleared here.
- Question to carry: 0x4424 is also written in AIRacingLine L133-L142 (half/full of wp+24). Is it the same meaning
  (a forced lateral)? List both writers in the glossary.

Continue with group 3, then group 4, B5, then final stop.
