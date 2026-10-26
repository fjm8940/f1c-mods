# Holes review (2026-10-16) — lap comparison: practice runs, failures, damage

Trigger: owner, during the live P1+P2 drive: AI may not push in early practice; what happens on a mechanical failure;
"investigate and think about this for holes". Evidence: live trace `%TEMP%\aad_trace_20261016T105728Z_pid28060.txt`
(practice A-1 Ring 2001, slider 93, sim time 0..1015 s, read-only parse), `tools/aad_rating_logger.py` (oracle; the C port
is identical), SDK findings, `mechanical_failures/dll/engine_failure.c`. Nothing changed in code; game running.

## Current rules (what the code does)
- Player lap clean = not first lap / lap >= 2, no pit, no off-track, no cut, no yellow, not in traffic > 25 % of samples,
  no sim jump, moving, 50 s < lap < 200 s.
- AI lap clean = fully observed, no pit, no yellow, 50 s < lap < 200 s. **Nothing else** (no off-track, traffic, damage,
  failure or slow-lap test for AI).
- Comparison = only AI laps that END inside the same window as the player's lap (between two player S/F crossings).
  Teammate = AI car with the same car name.

## Live data (FACT, this practice)
- AI drive in runs: out-lap (pit, excluded) -> 2-6 flying laps -> in-lap (pit, excluded) -> garage (multi-minute "laps"
  like 298, 454, 543 s; excluded only because they contain pit).
- Flying laps get faster through a run and through the session: Ferrari slot 1 71.50 -> 70.61; Jordan slot 13
  72.27 -> 71.13; Prost teammate 73.02 -> 72.65. So the owner is right in effect: early laps are slower — not "cruising",
  but warm-up + track evolution, about 0.5-1.5 s.
- Teammate (Prost slot 15) first went out at sim ~716 s, after the owner's 5 clean laps -> no lap in any player window
  -> `teammate_clean` null on every lap, although the teammate did 3 clean laps in the session.
- Mid-run slow AI laps are counted clean: BAR slot 17 75.09 s between 72.49 and 72.05 (+2.6 s; probably mistake or
  traffic — AI off-track is not read).

## Holes (ranked)
| # | Hole | Effect | Proposed fix |
|---|---|---|---|
| H1 | Same-window pairing in practice. Teammate laps outside the player's laps are lost. | Practice almost never has a teammate reference (seen live). | Practice: session-level comparison — player's best-N clean laps vs the teammate's best-N clean laps (N = 3), from the whole session; DD-26 comparable-team fallback when the teammate has < N. Race keeps the same-window pairing (fuel/track matched). |
| H2 | Early-run / early-session laps slower (warm-up, track evolution). | Medians mix slow early laps with fast late ones; bias depends on when each car ran. | Practice: best-N, not median of all. Race: unchanged (everyone runs together). |
| H3 | AI mechanical failure / MF symptoms. MF mod cuts AI torque / max speed (limp) BEFORE ReasonCode is set; game damage and failures set ReasonCode veh+0x4068 (FACT, findings). Neither is checked for AI. | A limping teammate looks slow -> player looks faster -> AI made harder (wrong way). | (a) Read veh+0x4068 for every car: lap with ReasonCode != 0 not clean, and the car is dropped for the rest of the session. (b) Per-car outlier rule: AI lap > 3 % slower than that car's own session median = not clean (catches MF limp before ReasonCode, mistakes, traffic). (c) Later: MF publishes per-slot symptom state over the broker (cross-mod ABI, owner decision). |
| H4 | Player mechanical failure / damage (wing loss, MF symptom, engine smoke). Not a player reason today. | Player limps -> rated slower -> AI made easier. | Player ReasonCode != 0 -> lap and rest of session not clean (new reason "car damaged or failing"). Plus player outlier rule (lap > 3 % over own session median, not clean) — catches unrecorded damage. |
| H5 | AI off-track / traffic / blue flags not detected. | Single slow laps pollute small samples (median of 1-3 laps). | Covered by H3(b) outlier rule. |
| H6 | Small samples: teammate median of 1-3 laps. | Noisy decision. | Practice min 3 already; enforce best-N with N=3 for both sides; CI rule stays. |
| H7 | Race fuel / strategy: teammate on a different pit stop phase (light vs heavy fuel). | ASSUMPTION (not measured in F1C): fuel weight differences of a few tenths up to ~1 s/lap. | Open: skip windows where the teammate pitted within the last K laps vs the player; measure in owner races before building. |
| H8 | Menu track browsing starts "weekends" (session 1 = menu too). | Harmless now (log-only); with Adaptive=1 would apply/log spurious decisions. | Start a weekend only when the player car has moved on track. |
| H9 | Quali: AI out-lap/push/in-lap; player push laps. Quali summaries are not used. | None now. | Keep quali out of rating. |
| H10 | Qualifying lock uses practice only if practice summary was closed before session 5 starts. | If practice summary fails (e.g. game exit), lock falls back. | Fine; logged. |

## Facts vs inferences
- FACT: lap lists above (trace parse; pit flag excludes out/in laps). FACT: ReasonCode veh+0x4068 set by engine failure
  (1), gearbox (2), suspension, electronics (8), and collision damage (findings: "damage sets ReasonCode"); stays 0 on a
  pit retirement. FACT (source): MF AI symptoms change torque / max speed (engine_failure.c B1/B3, `ai_maxspeed_apply`).
- INFERENCE: BAR 75.09 s lap = mistake or traffic (no AI off-track read). 3 % outlier threshold = proposal, to be
  checked against recorded races (A-1 Ring 3 % = 2.1 s).
- Not checked: whether AI damage (wing loss) always sets ReasonCode for AI slots.

## Next
After the owner drive: spec P2.1 = DD-26 + H1-H4 + H8 (oracle first, C port, replay tests incl. this trace). H7 needs data.

OWNER 2026-10-16 (during the drive): approved P2.1 = DD-26 + H1-H4 + H8, spec + build AFTER the drive; Haiku agents do most of the work, Claude reviews.
OWNER 2026-10-16: P2.1 scope + DD-27 (quali sets race level, AI consistency).
