# Claude review 4 — groups 3-4 + B5 (2026-10-02)

Verdict: **PASS, but not finished: B8 was not done.** Re-run: B1 test OK, verify_maps 0 mismatches (19 maps).
Spot checks OK: glossary 0x4420/0x4424/0x4548/0x453C match reviews and prior art.

Notes:
- AIRacingLinePathFollowing writes no pedals/steering (placement/visual path walk): mark "not per-tick driving" in
  Identity, keep in glossary with that note.
- AISteeringRacingLineCorrection = recovery/re-placement service (session-type gated), not normal racing. Same note.
- Glossary readers miss float-index readers inside `this = float*` functions (example 0x453C, 0x4500, 0x4504, 0x4580
  are read by AIComputePassScore via a1[4431] etc.). Fix in B8 by taking readers from all CSV forms.

Next: **do B8** (handoff section 3, "AI Brain ABI v1 field registry"; re-read the handoff, it was added after you
started). Then stop for review. No GUI, no dll code.
