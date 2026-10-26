# START_REACTION — D7: launch behaviour and start tables (BATCH B item 5)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d7_start.py`
Live: race_start situations of `watch_race2` (Imola, 19 drivers) and `watch_race4fullc`
(Monza, 19 drivers). Static: `AIMainUpdate_00424e70.c` L96-L131, exe data read with pefile.

## 1. Static start machinery

- **Throttle ramp table 0x5C30E0, 26 floats, monotone 0.0..0.949**:
  `[0.0, 0.05, 0.0998, 0.1494, 0.1987, 0.2474, 0.2955, 0.3429, 0.3894, 0.435, 0.4794, 0.5227, 0.5646, 0.6052, 0.6442, 0.6816, 0.7174, 0.7513, 0.7833, 0.8134, 0.8415, 0.8674, 0.8912, 0.9128, 0.932, 0.949]`.
- The two reads are the **same table read in opposite directions**: `v18 = v17 * -25.0` (`v17 = min(session_time - last_toggle, 1.0)`),
  `this+17768 != 0` reads `0x5C30E0 + 100 + 4*trunc(v18)` (0.949 -> 0.0), else `dword_5C3144[trunc(v18)]`
  (0.0 -> 0.949); `0x5C3144 = 0x5C30E0 + 100` (overlap of the same 26 values). Output goes to
  **0x4434 cmd_throttle** during the start phase (`this+17768` = `unk_5C30E0`, `this+17764` =
  `dword_5C3144`).
- **Random toggle schedule**: when the current ramp finishes (`db >= this+17764`), the same flag flips
  and the next toggle time is `db + 1.0 + rand%100*0.02` (1.0..3.0 s) or `db + rand%100*0.01`
  (0..1 s), alternating with the flag. So before the launch the AI alternates ramp (0..0.949) and
  0-throttle phases with random 0-3 s periods.
- `throttle_cap_race_start 0x44B8` (`.rcd` STARTSDRY/STARTSWET): parse-time
  `cap = 1.0 + 0.1*(blend + rand%200*0.01)` clamped (`>4.0 -> 4.0`, flag -> -1.0), where
  `blend = (1-wetness)*STARTSDRY + wetness*STARTSWET` (D5 section 2).
- `StartStalls 0x44BC` at parse: `rand%10000 < StartStalls*100` -> start-state pair
  (0x4144=0, 0x4148=4/5), consumed in AIMainUpdate's start block (L80/L89 random state). No stall
  occurred in the two captured dry starts.

## 2. Live launch reaction

First car >1 m/s: Imola `t0 = 11.48 s`, Monza `t0 = 48.09 s` (capture started during the countdown).

| metric | Imola race2 (n=19) | Monza race4fullc (n=19) |
|---|---|---|
| time to own >1 m/s after t0 | min 0.00, med 0.10, p90 0.15, max 3.55 | min 0.00, med 0.10, p90 0.15, max 0.16 (18 measured) |
| cmd_throttle onset (before t0) | med -9.75 s range -9.75..+3.04 | med -8.50 s range -8.60..-8.45 (18) |
| cap 0x44B8 | 1.000..1.170 (med 1.080) | 1.009..1.192 (med 1.115) |

- The field launches **together**: every car crosses 1 m/s within 0.16 s of the first mover except
  **Christian Klien (+3.55 s, Imola)** — the already-documented race2 start incident (mode 6 reverse
  at sim 15-27, `car_conditions` 2); the start light gate, not a per-driver AI delay, releases the field.
- Throttle is applied **8.4-9.8 s before movement** (countdown): live 0x4434 follows the exact table
  values (probe: 0.9490, 0.9320, 0.8912, 0.8674, 0.8415, 0.7833, ... descending, then 0.0 for ~1.8 s,
  then another phase) — confirmation of the read-backwards ramp + random toggle.
- `throttle_cap_race_start` is **constant per driver for the whole capture** (parse-time value), no
  time profile: dry runs, so `cap = 1 + 0.1*(STARTSDRY + rand200*0.01)`; observed caps imply
  `STARTSDRY + rand200*0.01` 0.0..1.92 across the field.
- Fernando Alonso's Monza capture has no >1 m/s sample (act_throttle onset just 0.10 s before t0);
  his race_start situation window ends before his first movement. Not a stall.

## 3. Correlations / verdict

- Per-driver launch delay to movement is **not** produced by the AIMainUpdate random values: the
  random toggles shape the pre-launch throttle ramp (visible in 0x4434), and the movement release is
  the light/countdown. Motion reaction is effectively constant (0.00-0.16 s).
- `0x44B8` cap correlates with driver (STARTSDRY + parse random), not with measured reaction
  (Spearman not computed; the observed spread sits inside the 0.16 s field spread).
- **VERDICT: start randomness = random throttle-ramp toggle schedule (1-3 s / 0-1 s phases) plus
  per-driver STARTSDRY/WET + rand%200*0.01 cap jitter; the launch itself is simultaneous (start-light
  gated). StartStalls is the scripted stall path (not seen in these two runs).**
- Klien's +3.55 s remains the recovery/stuck outlier, not a normal launch delay.

## Limitations

- No wet race_start capture (wet Imola folder starts after the launch) -> STARTSWET/blend effect only
  static.
- 0x4144/0x4148 start-state pair is not in the ABI; the stall path is static-only here.
- Alonso Monza window truncated; his reaction not counted.
