# Pit-stop failure system — implementation plan

Status: **PLANNED, nothing built, nothing poke-proven.** Everything below is
static analysis plus correlation against the 2026-07-28 live pit trace
(`docs/pitstop_full_trace.txt`). Tier labels are honest — read them.

Produced 2026-07-28 by four parallel investigation passes (pit timer mechanism,
clip inventory, reusable DLL infrastructure, AI path + terminal outcomes).

---

## 1. Why this feature is cheap

The pit-stop duration is a **single float deadline, written exactly once per
stop, and never recomputed**. Adding delay is one add.

```
VehiclePhysicsModeDispatcher_004E4930   (per vehicle, per frame, arg = simTime)
  └ PitStateUpdateMachine_004A6E50       switch on pitState
      ├ case 2 → PitStopReadyCheck_004A6200     car at rest → compute duration ONCE
      │            └ PitStopOperationTimer_004A6340
      └ case 3 → PitStopProgress_004A68B0       per frame: refuel ramp, wheel anim,
                                                 completion test `now < stopEndTime`
```

`PitStopReadyCheck` writes `stopEndTime = duration + stopStartTime` at the
moment the car comes to rest. `PitStopProgress` thereafter only **compares**
against it. No other function in the 7,138-function corpus writes it.
`persistence_map.jsonl` classes it **LATCHED**.

⇒ **A wheel-gun failure is `*(float*)(veh+0x3470) += 4.0f`, once, on an edge.**
No code hook, no exe patch, no call-site redirect. Same code path for player
and AI.

## 2. The pit-object field map

**Base = `veh+0x32CC`.** Two independent derivations agree:
- `VehiclePhysicsModeDispatcher_004e4930` calls `PitStateUpdateMachine(this + 12992, a2)`,
  and its own `this` is `veh+0xC` (proved: it dispatches on `*(this+12988)` =
  `veh+0x32C8`, the CONFIRMED player/AI discriminator). `0xC + 12992 = 0x32CC`.
- Every offset in `docs/pitstop_full_trace.txt` lands exactly on a field this
  base predicts; base `0x32C0` would put `pittingStatus` at `0x3444`, which
  never moved across 4 traced cycles while `0x3450` moved 0→1→2→3→4→0.

| veh offset | pitobj | field | tier |
|---|---|---|---|
| `0x342C` | +352 | **pit-schedule entry COUNT** (see §7 correction) | DERIVED |
| `0x3430+8i` | +356… | scheduled pit LAP numbers, stride 8, consumed → `0x80000000` | DERIVED |
| `0x344C` | +384 | in-pit-lane byte | CONFIRMED (trace) |
| `0x3450` | +388 | **pitState** 0..4 | CONFIRMED |
| `0x3454` | +392 | state-1 entry time (150 s timeout vs `flt_5ACB08`) | DERIVED |
| `0x345C` | +400 | **serviceReady latch** — flips 0→1 when duration is written | CONFIRMED (trace) |
| `0x346C` | +416 | stopStartTime | DERIVED |
| **`0x3470`** | **+420** | **stopEndTime — THE KNOB** | **DERIVED (high conf), LATCHED** |
| `0x3474` | +424 | measured duration, written at completion | DERIVED |
| `0x3478` | +428 | lollipop / "2 s to go" one-shot | CONFIRMED, **RECALCULATED** |
| `0x347C` | +432 | fuel at stop start | DERIVED |
| `0x3480` | +436 | refuel ramp start time | DERIVED |
| `0x3484` | +440 | **target fuel** | DERIVED |
| `0x348C` | +448 | **refuelFlag** — gates the whole refuel path | CONFIRMED (trace cluster) |
| `0x3490` | +452 | tyreChangeFlag | CONFIRMED (trace cluster) |
| `0x3494` / `0x3498` | +456/460 | wingChangeFlag / repairFlag | DERIVED |
| `0x349C` / `0x34A0` | +464/468 | tyre-job start / end time | DERIVED |

pitState enum (CONFIRMED, 4+ live cycles): `0` on track, `1` driving/approach,
`2` entering pit lane, `3` **stopped in box**, `4` exiting box, →`0` at pit exit
line.

## 3. Where the duration comes from — computed, never configured

`config_schema.json` has **no** pit-service or refuel-rate token. Nothing in
`.hdv`/`.tbc`/`.plr` feeds it. `PitStopOperationTimer_004A6340` takes the **max**
of the enabled sub-jobs, each with a draw from the 256-entry noise table
`flt_8AAE74[dword_8AAE70++]`:

| job | formula | constant (VA, readers) |
|---|---|---|
| crew-readiness penalty `v7` | `clamp((lastUsed + 25.0 − now) * 0.5, 0, 4.5)` | `0x5ACB5C` = 4.5 (2) |
| refuel | `(target − current) * 0.0833333 + 2·rand + v7 + 1.5` | `0x5ACB58` = 1/12 (2) |
| tyre change | `max(2·r³ over 4 wheels) + v7 + 6.5` | `0x59E960` = 6.5 (2) |
| wing change | `v7 + 8.0` | `0x59E4DC` = 8.0 (**8 readers — shared, do not patch**) |
| damage repair | `v7 + 8.5`, `+39.0` if rear-wing record set | `0x5ACB54` = 39.0 (**1 reader**) |

Refuel **flow** is separate: `PitStopProgress` ramps at **12.0 L/s** from
`flt_59E96C` (2 readers). Patch flow and duration together or the fuel gauge
and the clock disagree.

Fuel *target* comes from the pit-request menu slider (`veh+0x3338`), i.e. the
player's or AI's own strategy — not a file.

Reserve the byte-patch route (`0x5ACB54`, `0x5ACB5C`, `0x59E960`) for a
**global** pit-time rebalance. For a per-stop random failure, poke `0x3470`.

## 4. Clip inventory

All eids read from the **`"eid"` field** of `data/full_event_dump.json` — not
`score`, not `sdt_index`. That column confusion shipped two broken DLLs on
2026-07-28. Every clip also has a TV twin in 292..535; **only 1..290 routes to
PWS audio.** Do not use the high twin.

**Player side** (all currently WIP, no emitter, not blocked, cooldown 1.0):

| SDT | clip | **PWS eid** | TV twin | role |
|---|---|---|---|---|
| 55 | PWS_2001_79 | **56** | 365 | fuel hose / slow refuel |
| 56 | PWS_2001_78 | **57** | 362 | refuel successful |
| 57 | PWS_TEAMPIT01 | **58** | 366 | teammate pit order active |
| 58 | PWS_TEAMPIT02 | **59** | 368 | teammate has pitted |
| 59 | PWS_2001_96 | **60** | 370 | rejoined in Nth |
| 60 | PWS_2001_76 | **61** | 372 | **wheel gun failure** |
| 61 | PWS_2001_68 | **62** | 369 | FL ok |
| 62 | PWS_2001_72 | **63** | 358 | problem FL |
| 63 | PWS_2001_69 | **64** | 357 | FR ok |
| 64 | PWS_2001_73 | **65** | 359 | slow FR |
| 65 | PWS_2001_77 | **66** | 371 | **wheel jammed / DNF** |
| 66 | PWS_2001_70 | **67** | 338 | RL ok |
| 67 | PWS_2001_74 | **68** | 346 | slow RL |
| 68 | PWS_2001_71 | **69** | 349 | RR ok |
| 69 | PWS_2001_75 | **70** | 339 | slow RR |

**AI side:**

| SDT | clip | **PWS eid** | role |
|---|---|---|---|
| 195 | PWS_MISC30 | **183** | problem in the pits |
| 196 | PWS_2001_131 | **184** | stalled in pits |
| 197 | PWS_2001_132 | **185** | fuel hose jammed |
| 198/199/200 | PWS_2001_133/134/135 | **186/187/188** | wheel stuck |

Two hard facts about this set:
- **No emitter exists for any of the 21.** Zero literal sites in
  `data/emitter_index.json`; zero hits grepping all `functions/*.c` for
  `DriverDataTableLookupAndTimeConversion(…, <eid>,`. There is no guard to
  relax — every clip needs a new DLL-side emitter.
- **Zero blocked, zero FLT_MAX, cooldown 1.0 across the set.** The cheap
  4-byte cooldown fix used for PWS_HIT01-06 is **not available here.**

⚠️ **Trap:** `emitter_index.json`'s dynamic list flags `PitExit_0041c0f0.c:127`
with eid 516 (PWS_2001_131's TV twin). `sub_41C0F0` is a *finishing-position*
dispatcher — 516 means "position == 1". Same score-collision class as the
QUAL28-30 dead end. **Do not route PWS_2001_131 through it.**

## 5. Architecture decision — extend the existing DLL, do not add one

`dll/pit_exit_commentary.c` **is** the v7 source (the `_vN.dll` files are build
artifacts only; there is no separate `_v7.c`). It already owns the
`pittingStatus` edge machine and is live-confirmed.

There is **no tick-site collision** — every commentary DLL in this folder is
poll-based with no code hook. The "share one DLL" rule binds only the
grid_reorder/red_flag call redirects at `0x0042C409` / `0x0042D1E1`.

The real collision is the **single-slot commentary queue**. A pit-failure line
fires at exactly the edges v7 already owns (PIT06/PIT10/PIT12). Two DLLs polling
independently will race and can emit "Go" before "wheel jammed". One owner can
sequence `Limiter On → [jam: HUD + callout] → Go → Limiter Off` coherently.

⇒ **Build into `pit_exit_commentary.c`.** Bump to v8.

### Injector — use the function-call variant, not the queue write

Two exist and they are **not** interchangeable:

```c
/* PREFERRED — pit_exit_commentary.c:98, goes through the real pipeline */
typedef void (__thiscall *StructInitFn)(void* buf);
typedef void (__thiscall *DriverLookupFn)(void* buf, int eid, int a3, int a4,
                                          int a5, int a6, float a7, float a8);
typedef char (__thiscall *InsertEntryFn)(void* controller, void* buf);
#define FN_STRUCT_INIT   ((StructInitFn)0x004144D0)
#define FN_DRIVER_LOOKUP ((DriverLookupFn)0x004144F0)
#define FN_INSERT_ENTRY  ((InsertEntryFn)0x00418560)

static void inject_commentary(int eid) {
    float buf[8];
    FN_STRUCT_INIT(buf);
    FN_DRIVER_LOOKUP(buf, eid, -1, -1, -1, -1, -2.0f, -2.0f);
    FN_INSERT_ENTRY((void*)CONTROLLER, buf);
}
```

The older direct queue-write (`redflag.c:382`) **zeroes**
`CONTROLLER+0xAF8+eid*4` — the exact array used as the play-confirmation
signal. Mixing the two makes confirmation never fire.

Confirm playback with `EVENT_TS(eid)` = `*(float*)(CONTROLLER+0xAF8+eid*4)`,
compared before/after. Not `IS_PLAYING` (+0xAE4 — false-positives on any
audio), not `Q_HEAD` (+0x864 — drains within one 100 ms poll, gives false
"gave up" on clips that played).

### HUD textbox for "WHEEL JAMMED"

```c
#define HUD_OUTER_PTR   0x007cd204   /* ptr-to-ptr */
#define SLOT_COUNT      8
#define SLOT_STRIDE     0x5c
#define SLOT_TEXT_OFF   0x3c         /* char[32] ASCIIZ, 31 chars + NUL */
#define SLOT_TIMER_OFF  0x90         /* float; >0 visible, counts down */
```
Timer decays — a persistent message must be re-stamped every tick
(`hud_refresh_red_flag()` pattern, `redflag.c:1103`). Dismiss with
`*timer = -1.0f`.

## 6. Build order

### Phase 0 — live confirmation (do first, one session, no code)
1. Read `veh+0x3470` as a float while stopped in the box. Must be ≈ `now + 10`.
   Settles §2's base beyond the two static derivations.
2. Confirm `veh+0x345C` flips 0→1 at the moment service starts.
3. **Isolation-test every eid** in §4 with `tools/commentary_inject.py`
   `inject_event(eid)`, human confirming out loud. Standing project rule; the
   v1/v2 bug passed every mechanical check and still played the wrong clip.
4. Resolve the AI trigger question (§8).

### Phase 1 — synthesised player failures (the core feature)
Roll a failure on the `pitState 2→3` edge once `serviceReady` latches. On a hit:
`*(float*)(veh+0x3470) += delay`, optionally extend `0x34A0` so the wheel-gun
animation covers the delay, and fire the matching clip.

- wheel gun failure → eid **61**, +3-5 s
- slow corner FL/FR/RL/RR → eids **63/65/68/70**, +2-4 s
- clean stop → the "ok" quartet **62/64/67/69** (see caveat below)
- refuel complete → eid **57** (FuelLevel stops rising while state 3)
- rejoined in Nth → eid **60**, on the already-shipped 4→0 edge

Caveat on the "ok" quartet: all four service flags flip on the same tick, so
per-corner attribution is unproven. The human triage already doubts four lines
fit inside one stop — pick one, or drop them.

### Phase 2 — fuel hose
`*(float*)(veh+0x3484) = *(float*)(veh+0x347C)` (target = current; preferred,
keeps flags consistent and allows a partial fill) **or**
`*(int*)(veh+0x348C) = 0`. Fire eid **56** (player) / **185** (AI).

**Hard prerequisite:** gate on `dword_7D0B64` (fuel-consumption realism option).
With it off, `FuelMultiplierValue_0042AD20` returns a constant 100, fuel never
depletes, and the feature is inert.

An underfuelled AI does **not** silently run dry —
`RacePhasePitEntryTrigger_00429EB0` → `ForcePitExit_0042A110` forces a second
stop below one lap of fuel. If it does empty, `FuelConsumptionAndWear_004ED960`
sets `ReasonCode` (`veh+0x4068`) `= 9` (Fuel) without setting FinishState, and
`PitStopProgress` clears the 9 on the next successful refuel.

### Phase 3 — AI failures
Identical technique on an AI slot: eids **183/186/187/188**. Same poll loop,
same latch. Blocked on the §8 question.

### Phase 4 — wheel jammed / DNF (highest cost, do last)
Sequence matters:
1. `veh+0x4068 = ReasonCode` (4 Suspension or 2 Gearbox read best for a jam).
2. Inject eid **66** **while `FinishState` is still 0**.
3. Confirm the clip played.
4. Only then write the kill set: `0x4064 = 2`, `0x4410 = 1`, `0x45F4 = 0`,
   optional `0x453C = 0.35`.

Two constraints:
- For the **player**, `FinishState 0 → 2` **is session teardown** — it dumps to
  the menu (FACT, two independent observations). There is no "retire and watch"
  mode. This ends the race.
- The stock retire emitter is unreachable anyway, and is additionally gated off
  by `veh+0x3450 != 3` — precisely while the car is in the box. Use the proven
  `dll/retire_commentary.c` queue path.

For an **AI** retire-from-box, the game has a native path and the arming trick
is already FACT: write `veh+0xD58 = 0` and `veh+0x454C = 1` while in state 3.
`PitStopBlockingCondition_0042AEE0` then refuses service and the game retires
the car itself, garage teleport and standings entry included. Do **not** forge
the end state — DISPROVEN, strands the car at the box with physics stopped
(`mods/ai_pit_retire/RETIRE_SOLVED_2026-07-24.md`). The shipped
`ai_pit_retire.c` is the old forged-state approach; copy the README recipe, not
that file.

## 7. Corrections to record

**`veh+0x342C` is not `aiPitStrategyIndex`.** Currently named that
[CONFIRMED] with the note "no writer found, reads 3 on every car". It is the
**pit-schedule entry count** inside the pit object, with `veh+0x3430+8i` the
scheduled lap numbers (consumed → `0x80000000`). `AIFormationLap_0041A3A0`
reading it as a 1/2/3-stop selector is coincidentally correct — count *is* the
number of stops — so the "AIPit10 fires on 1-stoppers" bug is a real 3-entry
array, not a stuck value. Tier: DERIVED, static, not poke-proven.

**`redflag.c:585` has a stale pitState comment** — says `pitting == 2` is "in
the box, verified on AI cars". The CONFIRMED enum from 4+ traced cycles says
`2 = entering pit lane`, `3 = stopped in box`. Red flag survives it because it
only needs "player is in the pit area", but it will mislead the next reader.

## 8. Open questions — resolve before Phase 3

1. **The AI pitState contradiction.** An existing CONFIRMED finding says
   `veh+0x3450` read **0 for all 20 AI cars** across a full-race trace despite
   them demonstrably pitting. If that holds, the AI side cannot edge-detect
   pitState and must watch the service flags (`0x348C`/`0x3490`) or the
   fuel-rising signature instead. This directly gates Phase 3.
2. **`this[11]` (≈`veh+0x32F8`).** Nonzero takes a completely different early
   branch (`ProgressBarIncrement`, `*+444 = 1`) that bypasses the duration
   calculation entirely. Purpose unknown — possibly a scripted/replay/instant-pit
   path. Verify it is 0 for normal stops before trusting the timer.
3. **`qhead+0x1C` priority.** Untested lever for winning the single queue slot.
   Unknown whether priority is honoured from the queue entry or re-read from the
   table at play time. Relevant only if "WHEEL JAMMED" must pre-empt an
   in-progress line.
4. **PWS_PIT09 ("Gear")** fires natively with an unidentified trigger; two live
   probes failed to land it. If a failure sequence needs to suppress or reorder
   it, that is unsolved.

## 9. What is NOT proven

Nothing in this plan is poke-proven. The offsets are static derivation plus
correlation against one live trace of four *successful* stops — no failed stop
has ever been traced. Phase 0 exists precisely to convert the load-bearing
claims (`0x3470` as the knob, `0x345C` as the latch, all 21 eids) from DERIVED
to FACT before any of it ships.
