# TRACTION_CONTROL — D9: AISteeringCorrection slip cut (BATCH B item 7)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d9_tc.py`
situations, 90 559 samples at 20 Hz. Static: `AISteeringCorrection_00426cf0.c` L1-70.

## 1. Static branch

```
slipA  = this[3633] (0x38C4)          # raw wheel-slip input 1
slipB  = this[3571] (0x37CC)          # raw wheel-slip input 2
blend  = slipA <= slipB ? slipA*0.3 + slipB*0.7 : slipA*0.7 + slipB*0.3
smoothed (v20) = (blend + slip_blend_last 0x4464) * 0.5
slip_blend_last 0x4464 = smoothed                       # stored every tick
if smoothed < 0.9 and (speed 0x4440 > 4.0 or watchdog_counter 0x446C != 0):
    depth = clamp(2 - 2.2222*smoothed, 0, 1)            # 1.0 at smoothed<=0.45, 0 at 0.9
    throttle 0x4434 = (SuspensionRatioClamp(this+12) - throttle)*depth + throttle
    if (x87 condition codes v12|v13) and byte 0x39F9:
        brake_on_slip 0x4028 = clamp(4*(throttle_before - after), 0, 1)
```
`brake_on_slip` 0x4028 and `slip_blend_last` 0x4464 are ABI fields; the raw inputs
0x38C4/0x37CC and the `SuspensionRatioClamp` return are **not** logged (recorder request R2).

## 2. Live frequency (Monza)

| metric | value |
|---|---|
| samples | 90 559 |
| trigger (smoothed slip < 0.9 & speed > 4 m/s) | **4 925 (5.44 %)** |
| `brake_on_slip` != 0 | **0 (0.000 %)** — never fired in either run |
| cmd_throttle <= 0.01 while trigger | 515 (10.5 % of trigger; braking/coasting samples) |
| steer-lift condition (|steer| > 0.1 & speed > 20) | 11 799 (13.0 %) |
| cmd_throttle under trigger | med 0.430, p10 0.000, p90 0.941 |
| cmd_throttle without trigger | med 0.428, p10 0.000, p90 1.000 |

Trigger share by speed and gear (strong low-gear signature):

| speed bin | share | gear | share |
|---|---|---|---|
| <10 m/s | 0.2 % | 1 | 20.5 % |
| 10-20 | 16.0 % | 2 | 15.8 % |
| 20-30 | 26.2 % | 3 | 3.6 % |
| 30-40 | 21.0 % | 4 | 0.3 % |
| 40-50 | 13.3 % | 5-7 | <=0.2 % |
| >=50 m/s | <=2.2 % | | |

## 3. Verdict

- The traction branch is real but **marginal in the logged state**: it is active 5.4 % of the
  samples, concentrated in 1st-2nd gear at 10-40 m/s (wheelspin region), and the aggregate throttle
  distribution barely differs from non-trigger samples (med 0.430 vs 0.428) — expected, because the
  cut depth is 0 at smoothed slip just below 0.9 and rises only as slip drops.
- `brake_on_slip` 0x4028 never fired: either its second condition (byte 0x39F9 + x87 cc) never held
  or the AI slip values stay above the brake-cut path. Negative result, OBSERVED.
- Whether `cmd_throttle` differs from the planner value when it fires **cannot be measured from
  these logs** (the pre-correction throttle is overwritten in 0x4434 and the cut target
  `SuspensionRatioClamp` is not logged). The steer-lift effect (13 % of samples) shares the same
  field, so any one-tick comparison would mix both corrections.
- Recorder request **R2** added to `phase3/RECORDER_REQUESTS.md` for the raw slip inputs and the
  clamp target; with those the cut formula can be replayed exactly like D1/D11.

## Limitations

- Trigger uses `speed > 4` only; the `watchdog_counter != 0` branch is not separable in the logs
  (0.2 % of <10 m/s samples may be watchdog cases).
- Situations only (battle/brake_zone/pit/trouble/retire/race_start); the overview does not log
  `slip_blend_last`, so low-intensity cruising outside cut situations is under-sampled.
- SC/RF mods active; no SC marker in logs.
