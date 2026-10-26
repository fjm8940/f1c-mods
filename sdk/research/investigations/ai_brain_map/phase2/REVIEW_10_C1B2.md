# Claude review 10 — C1b2 Monaco re-test (2026-10-02)

Verdict: **PASS. Your x87 suspicion was right.**

## Braking step (Claude, FACT from disassembly)
At 0x425ED9 AIRacingLine calls TireForce 0x4E9100 (this = veh+0xC); Hex-Rays drops its double return (st0). The step
then multiplies by st(1) = that return, not by the target speed:
`v -= len(wp+0x3C) / v * veh+0x44CC * TireForce(v, wp+116, wp+120, wp+112) * 0.915`
(0x425EEF fld [edi+0x3C]; fdiv v; fmul [esi+0x44CC]; fmul st(1); fmul [0x59E8AC] = 0.915; fsubr v; v <= 1.0 skips;
v < 0.5 -> 0). Finding `ai-brain-braking-step-tireforce-correction-2026-10-02`; doc 54 s3b corrected.

## Steering on Monaco
Accepted: law holds at racing speed; misses at < 15 m/s are the 0.1/tick rate limit and +-1 saturation in the hairpin
(brain_core compares the law BEFORE the rate limit). Track 2 done.

## Fields
Accepted: slot_record_index == team-id pairing 99.99 % (confirms REVIEW_9). Monaco pit speed global 16.667 m/s = 60 km/h
(track-specific limiter).

## C8 (next, static + existing logs)
1. Re-derive TireForce 0x4E9100 from map_TireForce.md + disassembly where Hex-Rays is unclear (check other dropped
   st0 returns in it with capstone; use the same method: disassemble call sites, follow the x87 stack).
2. Implement it in Python (`analysis/tireforce.py`) from logged inputs (speed, wp+112/116/120 floats; any veh+0xC
   fields it reads - list which ones are missing from the logs in RECORDER_REQUESTS.md).
3. Re-run the A formula with it on watch_race3 (and race2b if inputs exist). Report meas/pred.
4. Scan ALL mapped AI functions for the same pattern (call to a double-returning function whose result Hex-Rays drops,
   followed by x87 use): list address, callee, and which pseudo-code variable is wrong. This protects every other map.
Stop for review.
