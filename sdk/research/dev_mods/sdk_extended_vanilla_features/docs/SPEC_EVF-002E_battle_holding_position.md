# SPEC EVF-002E — battle / holding position (Group E, T3)

```
CLIP / SDT index / EIDs:
  PWS_OD_HOLDING / 244 / 230  ("AI driver holds up another AI")

verdict:      NEEDS_TRACKER — T3 (multi-frame gap tracking + latch)
evidence:     gap-detection fields need identification
offsets:      gap/interval field NOT YET IDENTIFIED — see uncertainty
trigger:      |gap| < 0.5s sustained for 5+ seconds AND latch false
uncertainty:  live gap field location
```

---

## 1. Problem

The human notes: "driver1 (higher position) is fighting driver2 (attacking driver), maybe when their difference has been less than 0.5 seconds for 5 seconds or longer."

This requires:
1. A real-time gap measurement between two adjacent cars
2. Checking the gap stays below 0.5 seconds for 5 consecutive seconds
3. A latch to prevent re-firing

This is T3: needs a persistent timer counter across frames.

## 2. Gap field

The game computes per-car gaps. Known candidates:
- `totalTimeOnFinishLine` at veh+0x4098 (listed in name_index as "elapsedTime (alt)" note about 0x4570)
- LapTimeGapCalculation_0049b6d0.c reads veh+0x403C (lapNumber) and other time fields

**Needs live identification:** the gap between two adjacent cars in the standings. Look for a field that reads as seconds (e.g., 1.234 = 1.234 second gap).

```
LIVE READ REQUEST — gap/interval field
  precondition: race session, two cars visible on track with <1s gap
  commands:
    python tools/poke.py snapshot before_gap.json
    (wait 2 seconds, note which cars are close)
    python tools/poke.py diff before_gap.json --slot 0 --slot 1
  distinguishes:
    The gap field will be a float that changes smoothly with time
    (decreasing when catching up, increasing when pulling away).
    For two cars ~1s apart, expect ~1.0 ± noise. For the player
    looking at the car ahead, the field should read the time delta
    to the next car up in the standings.
```

## 3. Predicate

```
if SessionType == 7 AND dword_7D0308 != 0 AND flt_7CF3C8 != 0.0:
    for each pair (car_i, car_i+1) in standings order:
        gap = gap_between(car_i, car_i+1)  # seconds
        if gap < 0.5:
            timer[car_i] += delta_time
            if timer[car_i] >= 5.0 AND latch[car_i] == false:
                emit eid 230 for car_i
                latch[car_i] = true
        else:
            timer[car_i] = 0
            latch[car_i] = false
```

**Cooldown:** 1.0 — needs per-pair latch to prevent spam (gap < 0.5s would re-trigger every frame for the rest of the battle).

## 4. Collapse opportunity

Same observer as Group A, D — shares MainGameTickUpdate site. The gap comparison reuses existing AIOvertake code (gap/speed comparison in `OpponentProximityScanTeamRecognition_0042a7d0` or `AIOvertakeDefendStateChange_00419690`).

## 5. Uncertainty

1. **Gap field not identified.** The exact memory location for car-to-car gap (seconds) is unknown. `totalTimeOnFinishLine` at veh+0x4098 may work but is unverified. The `elapsedTime` at veh+0x4570 is SUSPECT (reads 0.0 on all cars).

2. **"Driver1 is higher position" clarification**: the clip is about the HIGHER-positioned AI being held up by the one BEHIND attacking. Or it's about the front car holding the attacker. Needs the actual audio to clarify which car is the subject.

3. **Live calibration of 0.5s/5s thresholds**: the human numbers (0.5s, 5s) are estimates. The actual threshold in the original game intent may differ. Needs testing.
