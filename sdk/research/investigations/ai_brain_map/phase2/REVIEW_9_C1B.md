# Claude review 9 — C1b unattended race analysis (2026-10-02)

Verdict: **PASS.** Careful work; mod-influenced periods flagged; OPEN kept OPEN.

Accepted results (OBSERVED, one track, 19 AI cars, SC/RF mods active):
- A: brake decision logic confirmed 459/459 (target < speed, brake > 0, throttle 0 on 454/459); profile slot 0 == speed;
  tail padding -1.8 exact. Speed-loss formula OPEN (wp+0x3C segment length not logged). Measured decel 31-33 m/s2 vs
  formula-implied 41-47: plausible reason for the brake/throttle pulsing (planner assumes more decel than achieved) -
  GUESS, test after logging wp+0x3C.
- B: steering law confirmed again (99.8-99.9 %, corr 0.99, lock 0.4014 constant). Second track still missing.
- C: opp_mode/opp_state follow the 5 team records (0x42CC stride 28), not slot rank. Claude fixed brain_core: pair each
  opponent by its id veh+0x4894 == opp_team_id[k]; live check 4 cars: every opponent matched a record.
- D: pit = mode 0 -> 4 -> 0 with 0x4484 held at 22.2 m/s (80 km/h) = pit limiter; short 0x4484 dips = traffic clamp.
  Post-race pit-in seen (Raikkonen). Modes 3/5 never seen.
- E: watchdog 45-50 -> mode 1 at 283-301 km/h (spin/wrong-way at speed); start incident used the reverse branch
  (0 -> 6 -> 7 -> 0 -> 6) with watchdog 0 (4-second progress check). Retire = carConditions 0 -> 2.

Validator blocker: `F1\.claude\scheduled_tasks.lock` was created by Claude's own session timer (sessionId matches).
Not DeepSeek's issue; owner decides (see chat). Do not touch it.

Claude already added waypoint seg_len (wp+0x3C) and speed_k (wp+0x84) to brain_core's waypoint records (first 6 waypoints
in every log snapshot). Next for DeepSeek (C7): list in phase2/RECORDER_REQUESTS.md any other fields the A/C/D analyses
missed (do not edit Claude's scripts). Then stop; the speed-loss formula is re-tested on the next recorded race.
