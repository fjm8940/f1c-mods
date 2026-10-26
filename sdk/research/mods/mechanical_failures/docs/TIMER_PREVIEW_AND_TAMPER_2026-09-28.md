# Timer preview + live tamper (2026-09-28)

Owner request: see the unfrozen lifetime timers in real time in the GUI, know
which fault will fire when a timer runs out, and be able to rewrite a timer
live (e.g. set it to 10 seconds).

**Status:** development trial, ``. Built warning-clean;
panel smoke + 19/19 tests; **live-verified 2026-09-28** in a practice session
(PID 9796): live values read, next-fault preview shown, gearbox tamper to
10 s counted down and FIRED the previewed type.

## 1. Live timer readout

Each family Auto section (Gearbox / Engine / Suspension) now has a
`Live timer:` line, refreshed every panel tick (400 ms):

- `11256 s   |   next: stuck` - remaining seconds and the fault that expiry
  will fire;
- `FIRED <type>` once it has fired;
- `not seeded (mode 0/3?)` when the PLR failure mode leaves the counter
  at FLT_MAX;
- `off (enable Auto + vanilla timer)` when the machinery is disabled.

Values seen live: gear 11256 s, engine 8670 s, suspension 13819 s (practice,
undivided seeds).

## 2. Next-fault preview (behaviour change)

The timer source used to roll the fault type **at expiry**. It now rolls ONCE
**when the timer arms** (session start / enabling), publishes it in
`auto_type`, and the panel shows it; expiry fires exactly that type. The
last-lap downgrade rules still apply at fire time (a previewed
no-drive/COLLAPSED/instant inside the final guard laps is still downgraded).
The defensive roll inside the expiry path remains only as a fallback.

## 3. Live tamper

`Set to: [10] s  [SET]` in each Auto section:

- writes `timer_set_ms`, raises `cmd_set_timer`;
- the DLL (on its next tick, any live session) writes the counter to that
  value, resets the fired latch (so it can fire again) and logs
  `TIMER: forced to 10.0 s by the panel`;
- pressing SET also ticks Auto + vanilla timer for that family, so one press
  is enough.

New ABI fields (ABI 4 -> 5): `cmd_set_timer`, `timer_set_ms` on all three
families; struct sizes gear 216, engine 192, suspension 264
(`_Static_assert`-locked).

## 4. Session scope (behaviour change)

The vanilla timer source (and the live readout/tamper) now runs in **any
on-track session** (practice/test day included) - vanilla AI counters tick in
all of them (phase > 4 only). The fixed-chance ROLL stays race + qualifying
only. Reason: practice sessions are the practical place to test timers, as
hit live today (session type 4).

## 5. Live evidence (2026-09-28, practice, PID 9796)

| Step | Result |
|---|---|
| ABIs after injection | 5 / 5 / 5 |
| Readouts | gear 11256 s next=stuck; engine 8670 s next=seizure; suspension 13819 s next=FAILURE |
| Tamper gearbox -> 10 s | remaining counted 9, 8, ... 1, 0; at t+11 `auto_fired=1`, type `stuck` exactly as previewed |
| Neutral safety | the car was in neutral at that moment; the gear DLL latched `ARM pending - car in neutral, will latch the next gear` and will engage on the next gear selection (existing proven behaviour, not a defect) |

## 6. Known notes

- If the game is mid-session when the DLL is (re)injected, enabling
  Auto+timer starts draining from the current (usually still seeded) value;
  use SET to decide the moment yourself.
- Electronics has no vanilla timer (the game seeds four counters; brakes and
  electronics are not among them). Brakes remains the pending future family.

## 7. Preservation

Before-state (copy-only): `workspace_organization_audit/TIMER_TAMPER_BEFORE_2026-09-28/`
- gear/engine/suspension sources, merged host, panel, tests, README;
`RESTORE.md` has per-file SHA256.
