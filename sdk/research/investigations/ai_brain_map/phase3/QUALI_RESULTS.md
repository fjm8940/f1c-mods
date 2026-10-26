# Qualifying live checks — wet Imola, 2026-10-02 (Claude)

Data (temp, disposable): `%TEMP%\ai_brain_live\watch_20261002T203309Z` (first minutes, old watcher) +
`watch_20261002T203532Z` (rest, with the new `yield` situation kind and interaction_target / call_to_box events).
Session global 0x7C56A4 = 5 (qualifying), wetness 0.80, 19 AI cars, owner: 20-minute session. Script:
`phase3/analysis/quali_checks.py` (read-only). Owner SC/RF mod state not confirmed (no SC in qualifying expected).

## A. Who reacts to whom in qualifying — static prediction CONFIRMED 14/14

Static (investigations/ai_interaction_role/README.md 2026-08-18, Overtaking session != 7 branch): in qualifying the
interaction target 0x45FC is written only when THIS car fails ServerSessionGateCheck (lapOfRun 0x4094 == 2 and not
called to box = on a flying lap) and the candidate passes it (lapOfRun != 2 or boxed). ProximityScan's auto-yield is
race-only.

Live: 14 interaction-target sets, **14/14 match**: writer always on lapOfRun 2 (flying), target always lapOfRun 1
(out lap), neither called to box. Pairs: Barrichello -> Massa (3), Alonso -> Heidfeld (5), Klien -> Raikkonen (6).
Lateral request -3.0 m (9) or 0.0 (5), throttle cap 0.75 while set (one 1.0 sample at the first tick).

Meaning (inference from code + data): in qualifying the car on the flying lap steers around the slower out-lap car
(and lifts to 75 % throttle); the out-lap car does NOT make room. No blue-flag style yield by the slow car exists in
qualifying.

## B. Pace multiplier outside the race — CONFIRMED

- 1 Hz steps: 61 up, 14 down, 21 680 flat. Up = AIPushLap ramp (+0.003 x laps, non-race).
- 13 of the 14 downs are 1.0000 -> 0.9975 = the initial 1.0 (AIDriverStructInit) clamped to the ceiling
  baseline + push (0.9975) by the AIPushLap min(): not a traffic step. No traffic-driven decreases (race logic off).
- 1 down: Trulli 0.9975 -> **0.8435** with car_conditions 1. Static: AIAbility `carConditions == 1 -> pace =
  0x44F4 x 0.85`; 0.85 x 0.9923 = 0.8435 exact.

## C. Trulli incident (car_conditions 1, first live sighting)

sim 1270-1278: 223 km/h -> 130 km/h, car_conditions 0 -> 1 at 1273.4, stopped (2 km/h) by 1277.5; pace 0.8435 at the
next AIAbility tick (1277.5); wrong-way counter 13; 1285 mode 6 (reverse) -> 1289 mode 7 -> 1290 mode 0, crawling.
car_conditions 1 = incident/damaged state that also forces the slow pace (meaning DERIVED; trigger not identified).

## D. Session structure

- (lapOfRun, mode) samples: (1,5) parked 11 480, (2,0) flying 6 381, (1,0) out lap 3 759, (1,3) garage exit 126,
  (2,6)/(2,7) recovery 27. lapOfRun = 1 on the out lap, 2 from the first timed lap on (also on the in-lap).
- Mode transitions: 5 -> 3 -> 0 (33, garage exit), 0 -> 5 (30, garage entry), 0 -> 6 -> 7 -> 0 (7, recovery).
  call_to_box 0 -> 1 (38) / 1 -> 0 (30): runs end with a call to the box.

## Follow-ups

- DeepSeek batch C E1 (static test list) can be checked against this file.
- brain_watch `pit` situations still fill their cap in qualifying (call_to_box lasts the whole in-lap); fine for now.
