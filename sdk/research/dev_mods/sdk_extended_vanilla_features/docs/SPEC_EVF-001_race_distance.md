# EVF-001: race-distance milestone commentary (eids 274, 271, 270)

```
CLIP / SDT index / EIDs:
  PWS_2001_163 / 292 / 274  ("one quarter distance")
  PWS_2001_164 / 288 / 271  ("that's half distance")
  PWS_2001_165 / 287 / 270  ("three quarter distance gone")

verdict:      NEEDS_TRACKER — reclassified T2→T3
evidence:     this spec (every number cited below)
offsets:      0x403C→lapNumber→CONFIRMED→LATCHED
              0x8ACF8C→TotalRaceLaps→CONFIRMED→global (session-level)
trigger:      lapNumber >= ceil(TotalRaceLaps * pct) AND per-milestone latch false
uncertainty:  see section 5 below — lapNumber during formation lap, TotalRaceLaps in timed sessions,
              rounding on short races, mid-race join edge cases
```

---

## 1. Laps completed vs. total laps

**Player laps completed:** `veh[player]+0x403C`, int32, name `lapNumber`.

- tier: **CONFIRMED** (f1cui source, 16 readers, `name_index.json`)
- persistence: **LATCHED** (`persistence_map.jsonl` slot 1)
- evidence: `SessionCheckEndCondition_0046d0d0.c:23` reads it as `player+16444` (= 0x403C) for qualifying end-condition. `tools/ask.py offset 0x403C`.
- scope: **per-car**. Read via player vehicle pointer at `0x7C54E8`.
- semantics: increments each time the car crosses the finish line. Starts at 0 (formation/out-lap not counted). After completing lap 1, equals 1.

**Total race laps:** global `0x8ACF8C`, name `TotalRaceLaps`.

- tier: **CONFIRMED** (f1cui + docs12, `name_index.json`)
- evidence: `SessionCheckEndCondition_0046d0d0.c:23` compares `lapNumber >= TotalRaceLaps - 1`. `tools/ask.py global 0x8ACF8C` — 19 readers, 0 static writers (likely set by runtime init code not traced by the static pipeline).
- scope: **session-level**. Same value for all cars.

---

## 2. Percentage-distance setting

The game parses three lap-related configs (from `config_schema.json`):

| Source | Token | Parser | Parser-offset |
|--------|-------|--------|--------------|
| `.gdb` | `RaceLaps` (e.g. 58) | `sub_4B1190` | +0x2114 |
| Player options | `Race Laps` | `PlayerOptionsInitializer` | +0x21F0 |
| Player options | `Race Length` | `PlayerOptionsInitializer` | +0x23E8 |

The percentage reduction is applied at session init: `TotalRaceLaps` at `0x8ACF8C` is the **post-reduction** value. Evidence: `SessionCheckEndCondition` compares `lapNumber` directly against it, no scaling — `lapNumber >= TotalRaceLaps - 1`. If reduction were unapplied, the game would need to scale here, which it does not.

**Verified by:** `tools/ask.py token RaceLaps`, `config_schema.json` sub_4B1190 / PlayerOptionsInitializer entries.

**Caveat:** for timed races (session-type=7 with a `RaceTimeThreshold`), `TotalRaceLaps` may be unset or zero. The predicate below handles this by checking `TotalRaceLaps > 0` before computing percentages.

---

## 3. Predicate + latch design

All three clips share one tick-site and one per-car state block.

**Predicates:**

| Milestone | eid | Condition |
|-----------|-----|-----------|
| 25% | 274 | `lapNumber >= ceil(TotalRaceLaps * 0.25)` AND `latch_25 == false` |
| 50% | 271 | `lapNumber >= ceil(TotalRaceLaps * 0.5)` AND `latch_50 == false` |
| 75% | 270 | `lapNumber >= ceil(TotalRaceLaps * 0.75)` AND `latch_75 == false` |

Where `lapNumber` = `*(int32*)(player_veh + 0x403C)`, `TotalRaceLaps` = `*(int32*)(0x8ACF8C)`.

**Latch mechanism:** three per-driver boolean flags in a custom DLL-side state block (e.g. `uint8_t fired_25, fired_50, fired_75` per slot). On trigger, set the flag to TRUE. The event fires exactly once per flag per race.

**Latch reset conditions:**
- `SessionType` (global `0x7C56A4`, FACT) changes to or from 7
- `RaceActiveFlag` (global `0x7D0308`, FACT) transitions to 1 (race start)
- Any race restart

**Cooldown note:** all three eids have `cooldown: 1.0` (`full_event_dump.json`). This 1-second cooldown CANNOT gate re-firing — a bare `lapNumber >= ceil(N)` is true for the rest of the race once crossed. The latch is the only correct mechanism.

**Guard gates (must pass for the event to emit):**
- `dword_7D0308` (RaceActiveFlag) != 0 — player is on track
- `flt_7CF3C8 != 0.0` — commentary audio system active
- `SessionType == 7` — race session only (not qualifying/practice)
- `TotalRaceLaps > 0` — protects against timed races where the lap count may be irrelevant

**Why T3, not T2:** the check itself is simple (one multiply, one compare), but it needs 3 persistent latch flags that survive across frames and reset on session transitions. The cooldown field is insufficient as a gate. This is a per-frame tick-hook with state, which is the definition of T3 in `WORK_CATEGORIES.md`.

**Collapse opportunity:** these three clips share one tick-hook and one state block. Together with `PWS_2001_159` (eid 275, "session is over"), which checks a different condition (`lapNumber >= TotalRaceLaps` or session-time zero), all four are one observer. Notes on 159 — different mechanism, not in scope for this spec, but same tick-site.

---

## 4. eid→clip mapping verified

All confirmed against `data/event_clip_map.json`:
- `"274": "PWS_2001_163.mp"` ✓
- `"271": "PWS_2001_164.mp"` ✓
- `"270": "PWS_2001_165.mp"` ✓

No mismatch with the user's table.

---

## 5. Uncertainty

1. **`lapNumber` semantics at race start**: the value during formation lap is unobserved. If `lapNumber` increments on the formation-lap finish line (before the race proper starts), the 25% trigger could fire early (e.g., lap 1 of 4 = 25% on a 3-lap race). Needs one-lap live observation: read `veh+0x403C` during formation lap vs. after race start.

2. **`TotalRaceLaps` for timed races**: the value at `0x8ACF8C` during a "Race Time" limited session is unverified. If it holds the track's full lap count (from .gdb) while the session is time-limited, the percentage calculation would be wrong (meaningless denominator for a timed race). Needs live read during a timed-race session.

3. **Mid-race join / restart**: the latch flags live in DLL-side memory. If the DLL unloads and reloads (game restart), flags reset naturally. If the player joins an existing race mid-session (online), `lapNumber` may already be past a threshold — the latch prevents replay of old milestones but the first tick would fire all whose thresholds are already met. This is probably correct behavior (joining at lap 30 of 44 should hear "half distance"), but the exact interaction with online/multiplayer join is untested.

4. **Ceil rounding on small TotalRaceLaps**: for a 3-lap race, `ceil(3 * 0.25) = 1`, `ceil(3 * 0.5) = 2`, `ceil(3 * 0.75) = 3`. This fires 25% after lap 1, 50% after lap 2, 75% after lap 3 (simultaneous with race end). Acceptable, but the 75%/race-end overlap may cause both clips to queue — priority ordering between eid 270 (pri=40) and eid 275 (pri=40) is a tie. Needs testing on short races.

5. **Writer of `TotalRaceLaps` unknown**: static analysis found 0 writers for `0x8ACF8C`. If the value is set by a function not captured by the static pipeline (e.g., via a different pointer base), we cannot regression-test that a poke to this address holds. This matters only if we ever need to modify it — reading it is safe. The CONFIRMED tier comes from f1cui observation and the 19 reader functions, not from a poke.

---

## 6. Related clip (not in scope)

`PWS_2001_159` (eid 275, sdt 293, "session is over") — different mechanism (clock-hits-zero or SessionType transition). Shares the same tick-site. Reference only; not part of this spec.
