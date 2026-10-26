# Claude review 11 — C8 TireForce + dropped-ST0 scan (2026-10-02)

Verdict: **PASS.** Claude spot-check: AIRacingLine calls AerodynamicForceCalc 0x4E8B80 at 0x425A19 and 0x425C8C and each
call is followed directly by `fcomp [0x59E440]` = the branch tests the RETURN value, not v8 / v23*0.3 as Hex-Rays shows.
DROPPED_ST0_SCAN.md accepted as the warning list for every map (14 confirmed sites, 2 false positives).

A5 implemented by Claude (not yet live-tested, game in menus): brain_watch writes `cars_static.jsonl` = raw floats/ints
of veh 0x3A7C..0x3D50 per car every 60 s (TireForce/TireForceSolver inputs + tables); snapshots now carry wp+0x7C/0x80
(`k7c`, `k80`) and extra.v3520 / v3524.

C9 (next): add a "dropped st0" note to each affected map (Claude review section, do not re-tier), and prepare
c9_a_retest.py so that the next recorded race (with cars_static.jsonl) re-runs the braking step with your TireForce model.
Stop.
