# AI failure survey, 2026-10-08 (owner live test, dev MF `CCF9CE00`)

Method: the owner forces one failure per AI driver with the MF panel (dev). Claude records the panel data, the live f1c-memory speed/rank and
engine_failure_log lines. Expected outcome per row: drive on (mild) / pit / retire. Crawling = FAIL.
Feeds Part M in `development/mods/DS_PRESHIP_BATCH_2026-10-08.md`.

| # | Fault | Driver (slot) | Forced at t | Log reaction | Live result | Expected | Verdict |
|---|---|---|---|---|---|---|---|
| 1 | blowby (8) sev 1 | Ralf Schumacher (1) | 32 s | R1, margin 0.036, cap 92% | 67.8 km/h constant, 7th gear, 3782 rpm, thr 75%, rev limit 1026 (car about 1990), rank 21 | drive on slower, or pit/retire | FAIL (crawler) |
| 2 | exhaust (9) sev 0, NATURAL (impact g=12.7) | Luca Badoer (15) | 441 s | R2, margin 0.050, cap 94%; then `AI PULLOFF DIAG state 8 brk act 0.00 cmd 0.00 under-cap=40` every 1 s for 80+ s | 68.9 km/h at sim 548, rank 20, PassingNorm 0.29, aggression 0.35 | pit or drive on | FAIL (crawler; pull-off never brakes/stops) |
| 3 | waterleak (4) sev 0, NATURAL (impact g=11.2) | Juan Pablo Montoya (14) | 441 s | R1, margin 0.036, cap 93% | 69.2 km/h at sim 548, rank 19 | pit (water leak) | FAIL (crawler) |
| 4 | valve (1) sev 1, FORCED | Michael Schumacher (2) | 556 s | R1, margin 0.038, cap 91%; then `rev band restored` (log line 3674) | 68 km/h at sim 607-609, rank 5 -> 6 (dropping) | drive on slower / pit | FAIL (crawler, even with the rev band restored) |
| 5 | oilp (2) sev 1, FORCED | Rubens Barrichello (3) | 610 s | R1, margin 0.037, cap 90% (noticed after 1 s) | owner: same crawl | drive on / pit | FAIL (crawler) |
| 6 | (owner) | Tester A (4) | - | - | owner: same crawl | - | FAIL |
| 7 | waterleak, FORCED | Matteo Bobbi (8) | - | - | owner: same crawl | pit | FAIL |
| 8 | (owner) | Nick Heidfeld (17) | - | - | owner: shifts to top gear, crawls | - | FAIL |
| 9 | (owner) | Kimi Raikkonen (18) | - | - | owner: same | - | FAIL |

Pattern (fact): 3 of 3 AI faults end at about 68-69 km/h, whatever the fault type or cap (92-94%). Natural (impact) faults crawl too, so this is
not specific to the force tool. Rows 2-3 come from one impact at t=441 (the owner's "one failure").

## Findings list (draft, updated live)
- F1 **Crawler at about 68 km/h**: every AI fault seen (blowby, exhaust, waterleak, valve; forced and natural) ends with the car stuck at
  67.8-69.2 km/h in a top gear at low rpm (Ralf: 7th, 3782 rpm, thr 75%). Owner: "AI does not know how to use the RPM". The cap is only 91-94%.
- F2 **Rev band lever**: the AI rev limit is cut to about 52% (Ralf 1026 vs about 1990) with shift points scaled; Michael still crawls after
  `rev band restored`, so the crawl outlives the lever (inference: the gear/rpm state is left wrong, or another lever still holds).
- F3 **Pull-off never completes**: Badoer `AI PULLOFF DIAG state 8 brk act 0.00 cmd 0.00 under-cap=40` repeats 80+ s; he never brakes,
  stops or retires.
- F4 **Little escalation**: at sim about 610 Montoya (waterleak) finally shows pit_status=1 (still 69 km/h); no other fault leads to a pit stop or a retirement; crawlers stay on the racing line for laps (rank losses, a hazard).
- F5 Panel: "AI rev limit" is shown as rpm but the value is an internal unit (1026 vs car 1990); the label is misleading.

## ROOT CAUSE (Claude, 2026-10-08, from code + live data)
`engine_failure.c` AI car derive: `revlim = ai_off_f(slot, AI_OFF_REVLIM)` re-derives `a->car` whenever the live field changes by more than 0.5%.
`ai_rev_apply` writes the fault cap INTO that field, so the car's "own" rev limit becomes the cap; the next cap = 90-94% of that. It ratchets every tick
down to the `lim > 100` guard (Ralf's panel car rev limit 1026 rpm = about 107 rad/s, fact). `ai_rev_apply` also writes upshift = 0.975 x cap
and scales the downshift points, so the AI upshifts at once to top gear and crawls at about 68 km/h. That explains F1/F2 and the owner's
"they all shift to maximum gear". The bug is in all builds with AI rev levers (live 1.0.0 likely, release 1.0.2 sure).
Fix (applied in dev source, before-copy `engine_failure.c.BEFORE_20261008_ratchet`): when `a->lsave.rev_valid`, derive from the saved original
`a->lsave.rev_limit`. Dev DLL rebuilt: `70AFF355` (prior `CCF9CE00` kept as `.BEFORE_20261008_ratchet`). Live verification pending.

## Live retest of ratchet fix, dev MF `70AFF355` (2026-10-08, Claude)
Owner forced valve (slot 1, t=77), oilp (slot 3, t=142), oilleak (slot 8, t=245); natural fuelleak slot 11. Sources: `engine_failure_log.txt`
(session from line 4376) + 2 f1c-memory driver samples (sim 302.6 s, 339.5 s, advancing).
- FACT: one `car rev_limit=` derive line per slot (1: 1990, 8: 1895, 11: 1895); no repeated or shrinking derive = ratchet gone.
- FACT: no 68 km/h crawl. Slot 1 (valve, R2 cap 75%) 228 then 109 km/h, dropped rank 13 to 16. Slot 8 (oilleak, R1 cap 92%) 205 / 165 km/h, matching cars around it.
- FACT: slot 3 oilp R1 to R3 to LIMP 60 s ("no pit path") then retire, pull-off, park, released FinishState 2 at t=281. Slot 11 same path, released t=324.
- OPEN: `AI PIT ... callToBox not wired` (repairable faults retire instead of pitting) = Part M. Gear not read directly (memory tool has no gear field); shifting is inferred from speed.
- FACT (follow-up): retire timing follows the limp timer in `engine_failure.c` ~8961: R3 retires after 60 s, R2 after 180 s, R1 has no limp
  timer (it ends on the vanilla failure clock, budget 300 s). Slot 1 (valve, R2) retired at t=432, then pulled off and parked at t=444.
  Slot 8 (oilleak, R1) had one 2.3 s mistake at t=359 and was still running when checked.
- FACT (code read, 2026-10-08, owner asked why forced oilleak on Sato slot 9 shows no blue yet): AI and player share the oil-loss model
  (`mf_engine_model.h` ~2736: hole grows with rev^2, loss with sqrt(p_oil)), so the leak starts slowly and nursing (lower revs) slows it.
  The smoke differs: player blue starts at 2% oil lost and follows throttle/rpm with lift and slosh puffs, up to 30 particles/s
  (`engine_failure.c` ~2920, `smoke_emit_drive`). AI blue (`mf_ai_react.h` 313 + `ai_blue_pump`) starts at 5% lost, flat density,
  12 particles/s at most, max 2 per push, no puffs. OPEN for owner: match the AI threshold to 2% and add puffs? No log of AI oil volume
  (add one in Part M so delays can be measured).
- FACT (live, sim 783 s, 14:18 UTC): Sato (slot 9, oilleak onset t=548, R1 92%) at 292 km/h, P4, no blue reported by owner, no new log lines.
  Button (slot 8, oilleak onset t=245, budget 300 s, R1) still racing at 251 km/h P10, about 240 s past the expected end; last log line t=359.
  INFERENCE: AI oilleak at R1 may not be progressing (oil loss stays under the 5% smoke line and the vanilla-clock end did not fire).
  Unproven: there is no AI oil-volume log. Also slot 6: force fuelp (R3) then force oilleak 9 s later; END logged "fault oilleak", so the
  second force overwrote the first fault (check whether forcing a car that already has a fault should be refused).
- CORRECTION (code read `mf_engine_model.h` ~2612, ~3350-3385): for an R1 AI car the forced "budget 300 s" only sets the PACE
  (`clock_drive`); it is not a deadline. The hard end is the Q7a terminal at the car's vanilla `ai_life` = 0, and a dev-forced car was not due
  to fail, so it has no near end. The oil leak hole grows from zero (`MF_OIL_A_GROW` 0.0012) and grows more slowly while the car is nursed.
  So Button still running at R1 is by design, not proven stalled (the earlier "bug" inference is withdrawn). Owner saw Sato start blue
  around sim 800+ s = past 5% oil lost, about 4-5 min after onset. The engine dies only when oil starvation fails the bearings (physics), or
  at R2+ through the limp timer (R2 180 s, R3 60 s). OPEN for owner: should forced/test faults get a hard deadline?
- FACT (code, owner asked "will Sato blow, with flames?"): an AI oilleak can end two ways. (1) Bearings reach h_b 0 = "bearing seizure",
  `blown` = END_BLOWN = the only ending with a fireball (`mf_engine_model.h` ~2778, `mf_ai_react.h` ~354). (2) The driver escalates to R2/R3
  and the limp timer retires him = END_STOP, engine stops with no fire. The time cannot be predicted: no oil or bearing log for AI (Part M item).
- FACT (code `mf_ai_react.h` 463-466, 507+): oil leak escalation gates. Blue starts at 5% oil lost. R2 is allowed only below 60% sump
  (`MF_CASCADE_OIL_FRAC`). R4 (retire) is allowed only below 30% sump (`MF_AIR_SUMP_RETIRE`). Each clear worsening re-rolls the dice
  (oil leak mix R1 45 / R2 25 / R4 15, R0 is zeroed once noticed), so a car can puff and race at R1 from 5% to 40% lost. Sato
  puffing at full pace = by design. Owner may find it too long; tuning candidates: lower the R2 gate or make bearing wear show sooner.
- Live panel (owner screenshot, 17:15, Sato slot 9): oil 3.7 L / 6.0 (38% lost), bearings 56%, "oil leak (medium), developing", vanilla
  fails-in 54:48. INFERENCE: the R2 gate opens at 3.6 L. Re-rolls fire on every clear worsening, so R2 is likely within seconds, then the
  180 s limp timer gives a quiet retire (no fire) unless the bearings go from 56% to 0 first, which is unlikely at the lower R2 cap. Fire needs
  bearing seizure. To be confirmed in the log (`AI REACT: slot 9 R2`, `AI END ... Option A, no fire` vs blown).
- FACT (log, Sato slot 9): R2 at t=1157 (cap 88%), LIMP 181 s, END R4 retire t=1351 "Option A, no fire", pulled off and parked t=1361.
  Prediction confirmed (quiet stop, no fireball). Whole oil leak took ~800 s from onset (t=548). Note: R2 came ~600 s after
  smoke started, so the owner watched a long stretch of puffing at full pace (tuning question open).
- Owner decisions 2026-10-08 evening: keep the oil leak progression; AI blue starts later (25% lost); add AI BLOW-UP fault. Spec = Part N in `../DS_PRESHIP_BATCH_2026-10-08.md` (includes the fireball audit table).

## MF 1.0.3 dev ECF1CF48 live check (2026-10-08, owner + Claude log read)
- FACT: forced blow-ups slot 1 (t=33), 20 (t=52), 13 (t=65): "engine blow-up, Option A blow-up"; 20 and 13 overlapped and the
  FLAMES descriptor was restored only after the LAST fire faded (owner list works). Owner: "blow ups work nice".
- FACT: Raikkonen slot 12 forced oilleak t=89: R1 92% at t=102, AI OIL 10% lost t=259 (h_b 0.97), 20% t=435 (h_b 0.88). No
  blue before 25% = N1 working. Natural rad fault Badoer slot 17 -> R3 -> limp 60 s -> retire, pull-off, released.
- FACT: Raikkonen 30% lost at t=611 (h_b 0.76); owner saw blue after the 20% mark = onset between 20-30% (25% design). N1 PASS live.

## 2026-10-12 live check: Ralf Schumacher (slot 10) stop

- FACT (engine_failure_log.txt): impact onset g=8.4 at t=71, fault exhaust sev 0; noticed -> R2, rev cap 94% (1870/1990);
  one M1 mistake t=165; `AI LIMP: slot 10 R2 for 180 s, no pit path - retires` -> R4 Option A (no fire) t=264;
  pull-off left, parked t=276. Live memory read (paused frame, 14:16 UTC): slot 10 upright, 0 km/h, lap 2, retired flag still 0.
- VERDICT: code did what the 2026-10-07 limp rule (`dll/engine_failure.c` ~9241) says. INFERENCE: outcome is off-intent - the
  rule was added for crawling cars, but Ralf ran at 94% cap (not crawling); a mild sev-0 exhaust crack went straight to R2 and
  retired with no worsening. Open for owner: exempt mild R2 (cap >= 90%) from the 180 s retire, or make R2 retire depend on damage.
- No code changed.

### 2026-10-12 fix (dev, installed 2343C0AF...): AI revival + mild R2

- FACT (code): a retired AI slot stayed `done` for the session (never re-modelled), idle stayed 0, and the game timer kept our
  owned value (remaining life + 600 s). INFERENCE: outside the race a car the game sends out again would adopt that timer; a
  worn-out engine would be re-armed with ~600 s and fail again within a few laps. Not observed live (game closed before Ralf rejoined).
- Change (`dll/engine_failure.c`, before-state `engine_failure.c.BEFORE_20261012_revive`, old dev DLL `mechanical_failures.dll.BEFORE_20261012_revive`):
  1. `ai_revive_check`: non-race session, retired slot back on road (finish 0, reason 0, >10 m/s, rpm) -> restore idle/table/levers,
     reset slot, log `AI: slot N ... back on track after retirement`.
  2. `ai_fresh_life`: first arming after any retirement draws a NEW engine life Normal(3*LifetimeAvg, 3*LifetimeVar) from the car's
     engine.ini (floor 25 % of mean) and writes the game timer; log `AI: slot N new engine - fresh life`. Race: game seed kept.
  3. Limp retire: R2 only retires after 180 s when its rev cap < 90 % (`MF_AI_LIMP_MILD_FRAC`); R3 unchanged; crawl guard unchanged.
- Failures stay ON in practice/qualifying (owner rule 2026-10-12).
- Verify: -Werror build clean; audit_regression_test 47/0 fail; ai_react_test 96 pass. PENDING owner live test: retired AI in
  practice rejoins -> `back on track` + `fresh life` lines; mild R2 car no longer retires at 180 s.
