# Full-race AI wear trace — 65 minutes, 22 cars, 5764 samples

Unattended trace of a 53-lap race (reached lap 46), sampling every car every 15 s
via `mods/f1cui_web_remote`. Raw data:
`mods/f1cui_web_remote/web_telemetry_race_20260725_194623.jsonl` (3 MB, 7151
lines). Report tool: `tools/wear_trace_report.py`.

This answers several questions that were open, and corrects one field name.

## 1. The component countdown is a plain wall clock — SETTLED

**Every one of the 21 simulated cars drained at exactly 1.000/s for the entire
race.** min 1.000, max 1.000, mean 1.000.

Over that span oil temperature climbed from ~101 °C to ~113 °C and revs varied
continuously. Neither moved the drain by a measurable amount. So:

- `LifetimeOilTemp` and `LifetimeEngineRPM` do **not** modulate this counter.
- The earlier "5.7x burn rate" inference from `engine.ini` is dead. It was
  arithmetic, never observation, and the observation now contradicts it.

The counter also **keeps draining on a car that has already retired**: Fisichella
was out from the start (`fs=2, ret=1, dead=1`) and his engine counter still fell
37358 → 33337 over the race. It is a clock, not a wear integrator.

### AI engines cannot fail from wear in a normal race

Lowest counter reached by anyone: **10400 s** (Manning). A 53-lap race consumes
roughly 4000 s. With seeds of 20000–40000 s and a 1.0/s drain, no AI engine can
ever reach zero within a race distance.

Therefore **AI mechanical DNFs must come from the scheduled-failure table**
(`veh+0x4144`), which overwrites a counter with a small random value at a
scheduled lap. Throughout this entire race every car's table stayed unarmed
(all five slots `[-1, 0]`), and indeed **not one AI car suffered a mechanical
failure**. The only retirement was Fisichella, already out at the start.

## 2. AI tyre wear is real but negligible

First stint (start → first pit stop), 20 cars:

| Metric | Value |
|---|---|
| tyre loss per lap | **0.315 – 0.351** per-mille of tread (mean **0.334**) |
| spread across the entire field | ±5 % |
| front/rear ratio | **1.02** (fronts marginally faster) |
| loss over a full 53-lap race | **1.77 %** of tread |

Two things follow. The wear **is** simulated — it moves consistently, fronts wear
slightly faster than rears, and it resets to 1.0 at a pit stop. But it is far too
small to matter: under 2 % of tread across a full race distance, and effectively
**identical for every team and driver** (±5 % is not strategy, it is noise).
This corroborates the user's prior report that AI tyre wear "is not simulated
correctly" — it is better described as simulated but inconsequential.

## 3. AI pit strategy, as actually observed

A one-stop race. Every car stopped once, between **laps 36 and 45**, taking
**+32 to +59 L** of fuel and a fresh set. The exception is De La Rosa, who
stopped on **lap 9** after damage (below).

## 4. CORRECTION — `pittingStatus` (`veh+0x3450`) is NOT the AI pit state

**It read 0 in all 6653 wear samples**, across 20 cars that demonstrably pitted.
Not once did it show the documented 1/2/3/4 enum. Consequences:

- The mod's pit-stop tracker recorded **zero** stops for a race with 20 of them.
- Any logic keyed on `pitting >= 2` silently never fires for AI cars.

Pit stops are reliably detectable from physics instead: **tyre wear jumping back
toward 1.0**, and **fuel level increasing**. Both are unambiguous and were used
to reconstruct the strategy above.

Whether `0x3450` means something else entirely, or only applies to the human car,
is not established.

## 5. Yellow flags fire for cars in the PIT LANE

All 15 yellow/green pairs occurred in **zone 27** and cluster on laps 40–46 —
exactly the pit window — plus one pair at lap 10, exactly when De La Rosa pitted.

That is consistent with the writer's logic (`sub_4D1730`): it marks the zone of
any **non-DNF car below a speed threshold**, and a car in the pit lane qualifies.
So a "yellow flag" in this game frequently means nothing more than *someone is in
the pits*. Worth knowing before treating zone yellows as incidents.

## 6. A complete damage lifecycle, captured end to end

De La Rosa, lap 9: `DAMAGE` → `FRONTWING` (detached) → pitted (+34 L, fresh
tyres) → `FIXEDWING` (repaired) → rejoined and ran to lap 44. This validates the
front-wing repair detection added earlier the same session — the front wing is
repairable, as the decompiled blocking condition predicts, and the dashboard
tracked the whole arc without a false terminal call.

His oil also fell 100.2 → 84.8 °C, the only car to cool rather than warm, which
fits an early stop and a long slow lap.

## Still open

- What the human's engine-failure path is. `veh+0x32C8 == 0` for the player means
  none of the above applies to them: their countdown never ticks and their
  OilTemp readout is frozen. A player engine died anyway, by an overheat
  mechanism that is still unlocated.
- Whether the scheduled-failure table is ever armed, and by what. It stayed
  `[-1, 0]` for 22 cars over a full race. Championship mode, or a reliability
  setting, may populate it.
