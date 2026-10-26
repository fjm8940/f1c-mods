# Part-detach → commentary chain (PWS_AIHIT / PWS_HIT lines)

Session 2026-07-24. Traced from the race-engineer/PWS voice lines back to the
functions that emit them, then verified live with a Frida argument hook.

## The chain (FACT — from `functions/`, argument values hook-proven)

```
WheelControllerInputDecoder_00410bc0   (detach by part index)
CollisionSparkParticleEffects_004ec1a0 (collision-driven detach)
    -> RepairServiceStateDispatcher_004ed250(veh, partIdx)   __thiscall, *this = veh
        -> sub_41AFD0_0041afd0(veh, partIdx)                 __stdcall  <- commentary emitter
            -> DriverDataTableLookupAndTimeConversion_004144f0(buf, eventId, driverId, ...)
            -> InsertDriverTableEntry_00418560(0x5EA618, buf)
                -> controller 0x005EA618
                     +0x864  queue head (ONE entry deep)
                     +0xAE4  isPlaying
                     +0xAF8  per-event timestamp array (moves iff the line PLAYS)
                -> event table 0x005BF9A0, stride 0x14: pri / ... / +0xC cooldown / +0x10 score
                -> PWS lookup 0x005BEBB0 -> SDT clip
```

## partIdx → event id → clip (FACT)

`sub_41AFD0` branches player-vs-AI on `*(dword_7C54E8+18580) == *(veh+18580)`,
AI side additionally gated `dword_7C56A8 != 8`.

| partIdx | part | player event | AI event | clip |
|---|---|---|---|---|
| 0 | wheel | 106 `0x6a` | 221 `0xdd` | PWS_AIHIT07 |
| 1 | wheel | 107 `0x6b` | 221 `0xdd` | PWS_AIHIT07 |
| 2 | wheel | 109 `0x6d` | 221 `0xdd` | PWS_AIHIT07 |
| 3 | wheel | 110 `0x6e` | 221 `0xdd` | PWS_AIHIT07 |
| 4 | **front wing** | 108 `0x6c` | 222 `0xde` | PWS_AIHIT01 |
| 5 | **rear wing** | 111 `0x6f` | 223 `0xdf` | PWS_AIHIT02 |
| 6–13 | bodywork | none | none | — (silent by design) |

Front-vs-rear was DERIVED from `(v33 != 0) + 4` in `WheelControllerInputDecoder`
(even part index -> 4, odd -> 5). **Confirmed 2026-07-24**: hook logged
`partIdx 5` on a rear-end hit to slot 20 (Webber), which emitted `223`; owner
identifies AIHIT07 = wheel, AIHIT01 = front wing, AIHIT02 = rear wing.

## Findings

1. **`0xdc` / 220 / PWS_AIHIT08 is emitted by nobody.** Zero references to event
   id 220 across all 7,138 functions. Reachable only by queue injection.
   Its table entry is normal (`pri=40 cooldown=15.0 score=223`), so it is not
   FLT_MAX-blocked — it simply has no caller.

2. **All four wheels collapse to one AI line** (221). The player gets per-corner
   variants, and their numbering is NOT sequential — 108 is a wing, so the
   wheel ids are 106/107/109/110.

3. **Bodywork detaches (6–13) are silent.** They reach `sub_41AFD0`, fall
   through `default: break` without calling `DriverDataTableLookupAndTimeConversion`,
   yet still call `InsertDriverTableEntry` with a sentinel-only struct.

4. **AI part-detach lines QUEUE BEHIND the player-contact line, and may or may
   not survive the wait.** The player-contact lines `114` PWS_HIT10 / `115`
   PWS_HIT08 have `pri=20`; the AIHIT block has `pri=40`. The AIHIT event
   reaches the queue head and waits on `isPlaying`.
   - trials 1 and 2: waited ~7 s, then dropped, timestamp still `-1.0`
     (never played).
   - trial 3: waited 2.5 s and **played normally** (`223` ts `-1.0 -> 120.744`).

   So this is NOT deterministic starvation — earlier wording in this session
   claiming the AI line is always suppressed was WRONG. It is a priority
   ordering plus a wait that sometimes times out. What varies is how long the
   higher-priority clip holds `isPlaying`. Mechanism for a mod is still the
   priority field, not a cooldown, so no FLT_MAX workaround needed.

5. **`ReasonCode` (+0x4068) = 6 is written by the wheel path only**, guarded
   first-write-wins:
   ```c
   case 0: case 1: case 2: case 3:
     if ( !*(_DWORD *)(*this + 16488) ) *(_DWORD *)(*this + 16488) = 6;
   ```
   Answers the `mods/ai_wing_retire` open item about why ReasonCode=6 tracks
   wheel damage and not wing loss.

6. **`carConditions` (+0x3D6C) severity: 1 = wing/bodywork, 2 = wheel.**
   Promoted CONFIRMED -> FACT in `name_index.json`. Caveat: 2 is reachable by
   non-detach paths (slot 8 Trulli observed at `carConditions=2` with
   `ReasonCode=8` electronics).

7. **Losing a wing does not retire the car directly.** The car keeps
   `FinishState=0 RetiredFlag=0`, calls itself to the pits, drives back, and
   only retires on arrival in the box.

8. **Bodywork indices are not stable per-panel.** Same rear-end contact on the
   same car gave 6/9/10 in one trial and 7/8 in another. Do not treat 6-13 as
   fixed panel identities.

9. **Event 397 is a per-driver position update**, firing every ~2-3 s for each
   driver (`PositionChangeDetector_00420490` / `SeasonDisplayIDResolver_0041daf0`).
   WARNING when reading `part_detach_watch.js` output: the `driver` column
   carries driver ids that collide numerically with event ids — values 222 and
   223 appear there as DRIVERS, not events. Filter on the `eventId` column.

## Full retirement arc (FACT — `tools/trace_retire.py`, slot 20, one run)

```
 42.0s  rear wing off (partIdx 5)  carConditions 0->1, ReasonCode stays 0
 67.5s  callToBox   0 -> 1 | pitting 0 -> 1 | ActiveFlag 0 -> 1   (same tick)
188.5s  pitting     1 -> 2
195.1s  callToBox 1->0 | pitting 2->0 | FinishState 0->2 | RetiredFlag 0->1 | ActiveFlag 1->0
```

**The retire-vs-normal-stop discriminator is the `pitting` transition out of 2**
(`+0x3450`), and it is readable ~7 s of race time before the retire flags set:

| car | transition | outcome |
|---|---|---|
| Webber slot 20 | `pitting 2 -> 0` + FinishState/RetiredFlag same tick | RETIRE |
| Alonso slot 21 | `pitting 2 -> 3`, ActiveFlag 1->0, no FinishState change | normal stop |
| Trulli slot 8 | `pitting 3 -> 4`, ActiveFlag 0->1, then `4 -> 0` | stop completed, rejoined |

Healthy cycle is `pitting 0->1->2->3->4->0`. A retirement short-circuits `2 -> 0`
and never reaches 3. Reproduced on slot 6 (TesterA) at 626.94s with an identical
one-tick signature.

**`PitState` (+0x4040) is NOT pit progress — do not use it.** Earlier in this
session it was misread as pit-lane/pit-box progress. At 413-425s TWELVE cars ran
`PitState 2->0->1` within seconds of each other (Schumacher, Hakkinen,
Coulthard, Barrichello, R.Schumacher, Raikkonen, Frentzen, Hill, Irvine,
Montoya, De La Rosa) while not pitting. It behaves like a track-segment/sector
index. `callToBox` (+0x454C) and `pitting` (+0x3450) are the pit-truthful
fields — they moved only on cars actually pitting.

**`ReasonCode` is NEVER written on a wing-loss retirement** — Webber retired at
`ReasonCode=0`. Consistent with finding 5 (only partIdx 0-3 writes it).

## SOLVED: what decides rear-wing-loss = retire vs front-wing-loss = repair

This was the long-standing `mods/ai_wing_retire` open item. Mechanism found and
verified live 2026-07-24.

### The chain

```
contact -> WheelControllerInputDecoder_00410bc0 / CollisionSparkParticleEffects_004ec1a0
        -> RepairServiceStateDispatcher_004ed250(veh, partIdx)     __thiscall, *this = veh
             case 4 (FRONT wing): v4 = this + 818  (0xCC8) -> ConditionalResourceRelease(v4)
             case 5 (REAR  wing): v4 = this + 835  (0xD0C) -> ConditionalResourceRelease(v4)
             cases 0-3 (wheels) never touch 818/835
        -> callToBox = 1        (BOTH wing paths call the car in)
        -> at the pit box: PitStopDecisionTireCompound_0042a3d0
               if ( PitStopBlockingCondition(this) || dword_7C56A4 != 7 ) {
                 if ( dword_7C56A4 == 7 ) {
                   *((_BYTE *)this + 17424) = 1;   // veh+0x4410 RetiredFlag
                   *((_BYTE *)this + 17908) = 0;   // veh+0x45F4 ActiveFlag
                   sub_4BC060(1);                  // retirement log entry
                 }
               }
```

### The deciding test — `PitStopBlockingCondition_0042aee0`

```c
return this[4111] >= dword_8ACF8C
    || dword_7C56A8 == 8
    || *((_BYTE *)this + 14842)
    || *(_DWORD *)(*this + 3352) && !*(_BYTE *)(*this + 3416);   // <-- damage term
```

`3352 = 0xD18` (ptr), `3416 = 0xD58` (byte flag), delta `0x40` — one record.

Two records exist, stride `0x44`:

| record | ptr | flag | armed by | tested by |
|---|---|---|---|---|
| **A = front wing** | `veh+0xCD4` | `veh+0xD14` | partIdx 4 | `StateTransition` only |
| **B = rear wing**  | `veh+0xD18` | `veh+0xD58` | partIdx 5 | `StateTransition` **and** `PitStopBlockingCondition` |

`PitStopDecisionTireCompound` tests BOTH for `StateTransition(1)`, but
`PitStopBlockingCondition` tests ONLY B. **That single asymmetry is the entire
front-repairs / rear-retires behaviour.** Healthy flag = 1; "armed" = flag 0
(the test is `!*(_BYTE*)`).

### Live verification (`tools/wing_record_probe.py`)

Controlled pair, both sampled on the same tick, `partIdx` hook-proven:

| slot | driver | partIdx | Aflg | Bflg | retire term | outcome |
|---|---|---|---|---|---|---|
| 1 | M. Schumacher | **4 front** | **0** | 1 | no | `pitting 2->3->4->0`, Aflg 0->1, carCond 1->0, REJOINED |
| 7 | K. Raikkonen | **5 rear** | 1 | **0** | **yes** | `pitting 2->0` + retire flags, RETIRED |

Raikkonen: detach at Frida t=1024.632, `callToBox 0->1` at t=1025.05 — 0.4 s.
Retired at 1063.06s with the one-tick signature. Third rear-wing retirement with
an identical signature (Webber, TesterA, Raikkonen).

**Flags reset once the retirement completes** — retired cars read `Bflg=1`
again. This is why post-hoc polling of Webber/TesterA showed nothing; the
observable window is only between detach and pit box.

### Fisichella ambiguity — resolved statically

Slot 13 retired with `reason=6` (wheel) AND `Bflg=0`, which looked like B might
also arm on wheel loss. It does not: `RepairServiceStateDispatcher` cases 0-3
never touch `this+818`/`this+835`. He lost a rear wing in the same incident.
(A hit hard enough to detach a wheel will normally take a wing too.)

## FORCING IT: retire a healthy car via the game's own path (WORKS, in-game confirmed)

`tools/force_wing_retire.py`. Two writes, no end-state forgery:

```
veh+0xD58 = 0   (byte)   arm RearWingIntactFlag   -- ptr at 0xD18 is always non-null
veh+0x454C = 1  (int32)  callToBox
```

Then do nothing. The car drives itself in, `PitStopBlockingCondition_0042aee0`
returns true at the box, and `PitStopDecisionTireCompound_0042a3d0` performs the
retirement.

**The flag HOLDS when poked** — it is not recomputed per frame (unlike `0x1065`,
see `mods/ai_pit_retire`). Verified on 14 cars.

### Why this succeeds where `mods/ai_pit_retire` failed

That mod wrote the retirement END-STATE (`FinishState=2, RetiredFlag=1,
ActiveFlag=0`) directly and left the car **stuck at the pit box** (PhysX ~10.25)
instead of its garage (PhysX ~17.0). Arming the trigger instead lets the game
run its own retirement, which includes the garage teleport for free.

In-game confirmed 2026-07-24, slot 12 Montoya, 30 s end to end:

```
 0.00  RWflag=0 callBox=1 pitting=0                      armed, on track
24.60  pitting=2                                         at the box
30.12  callBox 1->0 | pitting 2->0 | finish 0->2 | retired 0->1 | active 1->0
       game resets RWflag to 1,  pos=(17.02, 0.56, 493.26)   <-- GARAGE
```

Then `arm-all` on the whole field: **21/22 retired, every one at PhysX
16.91-17.15** with per-team Z spacing (302.74 .. 493.26), player untouched and
still racing. `reason` stays 0 and `carConditions` stays 0 — a clean retirement
with no damage cosmetics, since nothing was actually detached.

Retirements cascade rather than firing at once: each car retires when IT reaches
its own box, so the field thins out over a lap or two.

```
python tools/force_wing_retire.py list
python tools/force_wing_retire.py arm 12 --hold 90
python tools/force_wing_retire.py arm-all          # every car except player
python tools/force_wing_retire.py status           # GARAGE/BOX position tagging
python tools/force_wing_retire.py revert 12        # flag=1, callToBox=0
python tools/force_wing_retire.py revert-all
```

Reversible while the car is still en route; once the game has retired it the
write is spent (the game resets the flag itself).

### Unresolved detail

`RepairServiceStateDispatcher` reaches the wing objects as `this+818` (`0xCC8`)
and `this+835` (`0xD0C`), both `this`-relative; the retire test reads
`*this+0xCD4` / `*this+0xD18`, vehicle-relative. Consistent `0xC` gap, base
difference not resolved. Does not affect the conclusion — the live probe
confirms the vehicle-relative offsets directly.

| car | partIdx (hook) | pit sequence | outcome |
|---|---|---|---|
| slot 18 Sarrazin | **4 = front wing** (+ body13) | `pitting 1->2->3->4->0`, ActiveFlag 1->0 then 0->1, `carConditions 1->0` | REPAIRED, rejoined, kept racing |
| slot 20 Webber | **5 = rear wing** (+ body7/8) | `pitting 2->0` short-circuit | RETIRED |
| slot  6 TesterA | **5 = rear wing** (+ body6/7/8/9) | `pitting 2->0` short-circuit | RETIRED |

Both rear-wing retirements set `callToBox 1->0 | pitting 2->0 | FinishState 0->2
| RetiredFlag 0->1 | ActiveFlag 1->0` in ONE tick, and left `ReasonCode = 0`.

TesterA is the sharper case: he had just COMPLETED a normal stop, lost the rear
wing on pit exit, and was called straight back in (`callToBox 0->1` two seconds
after the previous stop closed out) — then retired on arrival. So the decision
is driven by the damage state at the box, not by the stop that was already
scheduled.

## `carConditions` is live state, NOT a cumulative record

19 transitions observed in one session. Every `1 -> 0` lands on a car that
pitted — the stop REPAIRS it:

```
125.93 TesterA   0->1     405.11 TesterA   1->0
162.54 Sarrazin 0->1     492.39 Sarrazin 1->0
 92.78 Heidfeld 0->1     175.98 Heidfeld 1->0
 95.51 Manning  0->1     350.05 Manning  1->0
 67.09 Alesi    0->1     355.88 Alesi    1->0
```

Do not read it as damage history. A mod that samples it will see it reset at
every pit stop.

## RETRACTED: detached-part slot array (hypothesis FAILED to reproduce)

Trial 1 (Webber) suggested an array of detached-part slots at `0x10B8`/`0x1144`
stride `0x8C` with float-triple positions at `+0x10`. **Trial 2 (TesterA, rear
wing, different slot, same session) falsified it.** Explicit re-test of all 12
predicted offsets:

- `0x10B8 0x1144 0x10C8 0x10CC 0x10D0 0x1154 0x1158 0x115C 0x1324 0x00C4`
  — did not move AT ALL on the car that shed the parts.
- `0x036C 0x0378` — moved, but also moved on all three control cars => noise.

Zero of twelve survived. These were artifacts of that one diff window, not a
part-detach structure. Recorded here so nobody re-derives them.

Lesson for the method: a single `runtime_delta` diff minus one control is not
sufficient evidence to name an offset. Require reproduction on a second slot.

## Historical: the trial-1 candidate list (superseded by the retraction above)

`runtime_delta` diff of slot 20 across wing-loss -> pit -> retire, MINUS a
control diff of slot 21 (Alonso, pitted normally in the same window). Raw diff
was 2253 paths; control subtraction leaves 215, of which 40 are flat.

Method is validated: the subtraction independently recovered `0x4064`
FinishState and `0x4410` RetiredFlag, both known FACT, without being told them.

| offset | before -> after |
|---|---|
| `0x10B8`, `0x1144` | `0` -> `0x00080000` |
| `0x10C8`-`0x10D0` | `0` -> float triple `(1.308, 2.053, 10.454)` |
| `0x1154`-`0x115C` | `0` -> float triple `(-0.960, 2.053, 10.907)` |
| `0x036C` | `0` -> `2` |
| `0x0378` | `0` -> `1` |
| `0x1324` | `0` -> `2` |
| `0x00C4` | `0` -> `1` |
| `0x33C0`-`0x33E0` | 9 consecutive dwords moved |
| `0x0370`-`0x0384` | floats, `0` -> nonzero |
| others | `0x103C 0x1610 0x18FC 0x39B0 0x4114 0x42DC 0x434C 0x44CC 0x44F8 0x4590 0x459C 0x45E0` |

All of the above are UNNAMED in `name_index.json` — new territory.

Hypothesis (GUESS): an array of detached-part slots, record stride
`0x1144 - 0x10B8 = 0x8C`, flag at `+0x00`, position at `+0x10`. The two triples
share Y (2.053) and near-Z, with mirrored X (+1.31 / -0.96). Does not reconcile
yet: two records populated but three parts detached (5, 7, 8).

Caveats: single trial; control subtraction assumes the control lap exercised the
same physics paths (Alonso actually moved MORE offsets, 2698 vs 2253, so the
subtraction is conservative but not airtight). Needs a second wing-loss on a
different slot to see whether these reproduce.

## Corrections to earlier claims in this session

- `this[3928]` in `RepairServiceStateDispatcher` is **not** vehicle+0x3D60.
  `this` is a wrapper/physics object; `*this` is the vehicle. Only fields
  reached via `*this` (e.g. `+16488` = `+0x4068` ReasonCode) are vehicle
  offsets. The writer of `carConditions` is still unattributed.
- The queue head at `+0x864` is one entry deep, so two detaches in the same
  frame overwrite each other. A 30 Hz poll cannot see the loser. This is why
  the argument hook, not polling, is the correct instrument.

## Tools

- `tools/part_detach_watch.js` — Frida agent, hooks all three functions and
  logs raw `partIdx` + `eventId`. Run:
  `python tools/mod_pipeline/watch_run.py tools/part_detach_watch.js --out detach.csv`
  Read-only. Calling conventions inferred from Hex-Rays signatures; sanity
  check is `partIdx` in 0–13 and `slot` in 0–21.
- `tools/watch_damage_commentary.py` — polls damage fields + the per-event
  timestamp array. Complements the hook: the hook sees **emission**, this sees
  **playback**. They differ (see finding 4).

## Open

- Wheel case (221) not yet captured with the hook — needs one clean
  wheel-only hit.
- Webber-style drive-to-pit-then-retire tail after a wing loss.
- What decides rear-wing-loss = retire vs front-wing-loss = repair
  (`mods/ai_wing_retire` open item) — still unanswered.
