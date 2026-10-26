# SPEC_EVF-008_D — AI collision pair (2 rows)

**Source:** ASSIGNMENT_EVF-008.md Group D
**Date:** 2026-07-28
**Emitter:** `sub_41AD80_0041ad80`

**AMENDMENT 2026-07-28 (post-review):** the LIVE_TRUSTED verdict below rested
on reading the human's "same for this, stock game" as real-play recognition.
Asked directly afterward, the human confirmed they have never actually heard
either clip fire. **Both `PWS_AIINCID10` and `PWS_AIINCID11` are reverted to
WIP** in `data/progress.json`. The code trace in this document (emitter is
live, `on_frame`, reached via `CollisionDetectionSystem`, predicate
requires an actual AI-vs-AI collision above ~1250 impact-speed units) still
stands as a reachability analysis — what's retracted is only the claim that
this has been witnessed firing naturally. See `findings.jsonl`, "Second
correction: 12 more clips reverted, human real-play recognition claim was a
misread."

## Emitter identity

| Property | Value |
|---|---|
| Function | `sub_41AD80_0041ad80` |
| Priority | score 4, `on_frame` (FACT-tier deterministic per-tick marker) |
| Called from | `CollisionDetectionSystem_004ea970` (CONFIRMED: live collision physics) |
| `this` identity | per-vehicle tracking struct (accesses `this+*(a2+18580)+319` = per-driver collision cooldown array) |

## Guard conditions

```
dword_7D0308 != 0          // RaceActiveFlag (FACT)
flt_7CF3C8 != 0.0          // commentary enabled (CONFIRMED)
dword_7C56A8 != 8          // RacePhase != post-race (CONFIRMED)
```

**No SessionType==7 gate.** Collision commentary fires in any session type.

## Trigger predicate

```c
// Two vehicles a2 and a3 collide. Check per-driver cooldown:
this[driverA_index + 319]  // veh-struct-relative collision timestamp for driver A
this[driverB_index + 319]  // same for driver B

// Both drivers must not have had a collision in the last 2 seconds:
dbl_8AFEA0 - 2.0 > collision_ts_A && dbl_8AFEA0 - 2.0 > collision_ts_B

// Collision must be high-speed:
a4 > 1250.0                // impact speed in internal distance units
```

When triggered, updates both drivers' collision timestamps to current session time (`dbl_8AFEA0`) and records the other car's pointer for future reference.

## eid dispatch logic

```
if driverA == player:
    if dist_B < dist_A:  emit eid 114  // player overtakes → "well done you passed"
    else:                emit eid 115  // player falls back → "player overtaken by AI"
elif driverB == player:
    if dist_B < dist_A:  emit eid 115
    else:                emit eid 114
else:  // AI vs AI collision
    if dist_B < dist_A:
        if random() > 0.0:  emit eid 225  // AIINCID10 (with severity context)
        else:               emit eid 226  // AIINCID11 (alternate)
    else:
        if random() > 0.0:  emit eid 225
        else:               emit eid 226
```

## Per-clip analysis

### PWS_AIINCID10 (eid 225, sdt 238) and PWS_AIINCID11 (eid 226, sdt 239)

Both eids are the default AI-vs-AI collision commentary path. The distinction is a random gate:
- eid 225: fires when `flt_8AB274[rand_idx] > 0.0` (random check passes, ~50% probability given the lookup table values)
- eid 226: fires when random check fails (fallback)

The eid context includes both drivers' IDs: `DriverDataTableLookupAndTimeConversion(..., eid, driverA_name_id, driverB_name_id, -1, -1, ...)`. This explains the "(1)" vs "(2)" question: these are NOT distinct severity bands — they are **arbitrary random alternate lines** for AI-vs-AI collisions, providing variety rather than contextual distinction. Same collision, same conditions, random line selection.

**Verdict: LIVE_UNPROVEN_PREDICATE**

The emitter IS live (on_frame, called from `CollisionDetectionSystem` every tick). The guard conditions are satisfiable (RaceActiveFlag=3, commentary enabled, RacePhase != 8). The specific trigger requires:
1. An actual AI-vs-AI collision event (not just proximity — `a4 > 1250.0` impact speed threshold)
2. Both AI drivers must not have had a collision within the last 2 seconds (per-driver cooldown)
3. Neither driver can be the player (player-vs-AI takes the eid 114/115 path)

The human recognized these clips in real play (the assignment says "the human directly recognized in real play during testing" for PWS_OVERTAKE01/AIBLUE03, which are siblings in the same function). The CONFIRMED finding notes these were injection-tested and confirmed. But "recognized in real play" is the strongest evidence available.

**Not LIVE_TRUSTED because:** LIVE_TRUSTED requires either (a) independent writer cite or (b) human real-play recognition on record in `progress.json`. The human did recognize these in play, but `progress.json`'s note field needs checking. If the note records this recognition explicitly, these would qualify for LIVE_TRUSTED.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_AIINCID10 | 238 | 225 | LIVE_UNPROVEN_PREDICATE |
| PWS_AIINCID11 | 239 | 226 | LIVE_UNPROVEN_PREDICATE |

## Evidence

- `sub_41AD80_0041ad80.c` lines 14-93: full collision dispatch logic
- `CollisionDetectionSystem_004ea970.c`: caller (on_frame per-tick)
- CONFIRMED finding: isolation-injection confirmed audible content matches description
- Cooldown: eid 225 cd=30.0, eid 226 cd=30.0, both blocked=false

## uncertainty

1. **Random line selection vs severity**: the assignment asks whether eids 225/226 are "two distinct severity/context bands, or arbitrary duplicate lines." The code shows they are arbitrary random alternates — the random check at line 50/69 gates which eid fires, but the collision conditions (speed, cooldown, drivers) are identical. The driver-name context passed to the audio system distinguishes WHICH drivers collided, not how severe the collision is.

2. **The 1250.0 impact speed threshold** is uncalibrated — what game speed or collision force does this correspond to? A gentle tap may not reach it. This is a DERIVED value from the decompiled code; its real-world significance (do most AI collisions clear 1250?) is unmeasured.

3. **The 2-second per-driver cooldown** prevents chatter from a single multi-car incident. This is reasonable but means minor repeated contacts by the same driver within 2s are suppressed.
