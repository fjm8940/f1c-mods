# SPEC EVF-002D — AI spin / off-track / incident (Group D, T2+T1)

```
CLIP / SDT index / EIDs:
  PWS_AIOFFTRACK0 / 237 / 224  ("AI recovers from spin")
  PWS_AIINCID01   / 248 / 233  ("AI stalls on the grid")
  PWS_AIINCID08   / 243 / 229  ("cars all over in sector X")

verdict:      NEEDS_TRACKER (224, 229 T2) + WIRED (233 T1)
evidence:     sub_41B250_0041b250.c, StartStalls at veh+0x44BC, red_flag script (external)
offsets:      veh+0x44BC=StartStalls (CONFIRMED, .rcd parsed by sub_424080)
trigger:      per-clip, see below
uncertainty:  AIOFFTRACK spin-detection threshold (speed/on-track state)
              INCID08 car-count threshold per sector
```

---

## 1. PWS_AIOFFTRACK0 (eid 224) — T2: no emitter

**No function references eid 224 as commentary event.** Grep confirmed.

**Human note:** "AI spins/causes a yellow flag, when yellow flag clears this is played. the game has a problem that when AI is pitting, slowing down causes a yellow flag. maybe look into this and make it so that yellow flag actually comes in when a car has been 50km/h or under for more than one second."

**Two separate problems in one note:**
1. The bug: AI pitting triggers yellow flags because slow speed (~80 km/h pit lane) is mistaken for a crash/spin. The fix is to check car state (pitting flag, pit-lane position) before raising yellow.
2. The feature: when an AI car recovers from a spin (or crash) and the yellow flag clears, play eid 224.

**Detection:**
- Track AI car speed (veh+0xA0 = EngineAngVel, or veh+0x3C/0x40 = velocity vector). When speed drops below ~50 km/h AND not pitting AND not in pit lane → yellow flag condition.
- Use per-sector yellow-flag edge detect (from Group A above) — when the flag clears AND the previous state was caused by a spin (not a pit), fire eid 224.

**Collapse opportunity:** shares the yellow-flag observer from Group A. The flag-clear event fires eid 224 (for sector-specific clear) in addition to eids 133/135/137.

**Cooldown:** 1.0 — need latch to prevent spam.

**Verdict:** NEEDS_TRACKER (T2). Observer detects spin-triggered yellow flag clear, emits eid 224.

**Uncertainty:** exact speed threshold for "spin" vs "normal braking" is undefined. The human proposes 50 km/h for >1 second. Needs calibration. Also, distinguishing "spin-recovery" vs "pit-lane slow" requires an on-track/pit-lane state flag.

---

## 2. PWS_AIINCID01 (eid 233) — T1: mechanism exists

**Emitter:** NOT found in decompiled functions. But the mechanism IS identified.

**Mechanism:** `StartStalls` at veh+0x44BC (float, CONFIRMED, from .rcd parser sub_424080). This is a driver ability parameter: probability of stalling on the grid. If the .rcd value is, say, 0.5, the driver has a 50% chance to stall.

**Detection:**
- At race start (grid formation), check if AI car stalls
- Stall state likely triggers `AIDriverStateTransition` or similar
- When an AI is detected as stalled on the grid, emit eid 233

**Check the red_flag script** for stall detection — the assignment says it may already cover this.

**Cooldown:** 1.0 — needs latch (stall should fire once).

**Verdict:** WIRED (T1). The mechanism (StartStalls in .rcd) is proven; the stall-trigger code IS in the game's AI state machine. Needs the exact stall-detection hook point.

**Uncertainty:** which specific function detects the stall outcome at race start. Likely `AIDriverStateTransition_00419120` or `AIFormationLap_0041a3a0`. The eid 233 has no known emitter — either the stall detection never fires commentary, or the eid is unreachable and needs a new injection.

---

## 3. PWS_AIINCID08 (eid 229) — T2: reuse red_flag detection

**No function references eid 229 as commentary event.**

**Human note:** "cars all over the place in sector 1/2/3. look into the red_flag script for the detection mechanism."

**Detection:** count the number of cars that are crashed/stopped/off-track per sector. When the count exceeds a threshold (probably 3+ cars in a sector), emit eid 229.

**Sector detection:** same per-sector state as Group A. The red_flag script likely uses vehicle position (veh+0x1C/0x20 PhysX/Y) matched against track sector boundaries from the .aiw file.

**Cooldown:** 1.0 — needs latch. This is a multi-condition check (N cars in sector X AND any of them has crashed/spun).

**Verdict:** NEEDS_TRACKER (T2). Observer counts stopped/spun cars per sector, emits when threshold exceeded.

**Uncertainty:** threshold count (how many cars?) and the "crashed/spun" vs "just slow" distinction. Sector boundaries are track-specific (.aiw file data). Needs the red_flag script review. Priority=70 — this is a high-priority commentary event that should override lower-priority messages.

---

## Summary

| Clip | SDT | EID | Emitter | Blocker | Verdict |
|------|-----|-----|---------|---------|---------|
| PWS_AIOFFTRACK0 | 237 | 224 | none | no emitter | T2: needs spin-detection + yellow-clear observer |
| PWS_AIINCID01 | 248 | 233 | none found | StartStalls mech exists | T1: stall detection exists, emitter needed |
| PWS_AIINCID08 | 243 | 229 | none | no emitter | T2: count crashed/spun cars per sector |
