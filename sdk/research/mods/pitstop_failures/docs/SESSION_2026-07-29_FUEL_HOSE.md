# Fuel hose failure — session record, 2026-07-29 (second half)

Shipping build: **`dll/pit_commentary_v10.dll`** — untested in-game.

v9 (18:56) already contained the `VirtualProtect` fix for the `0x59E96C` write;
the "crashes" note above it was written before that build and is stale. v10 adds
one thing: `set_refuel_rate(REFUEL_RATE)` on the self-eject path, so unloading
the DLL mid-failure can no longer leave the game's global flow constant at
4 L/s permanently.

---

## 1. Fuel hose mechanism — FOUND

**`veh+0x3480` = `refuelRampStart`** (CONFIRMED, from previous analysis).

Game formula in `PitStopProgress`:
```c
fuelNow = fuelAtStart + (simTime - refuelRampStart) * 12.0;
if (fuelNow > targetFuel) fuelNow = targetFuel;
```

Three failure variants via rate manipulation:

| Variant | Effect | Rate |
|---|---|---|
| **SLOW** (9002) | Partial blockage — fuel flows slowly | 4 L/s |
| **NOFUEL** (9003) | Hose disconnected — zero flow | 0 L/s |
| **CUT** (9004) | Intermittent — flows 3L, then stops | 12→0 L/s |

## 2. Ramp manipulation approach — PARTIALLY WORKS

Two approaches tried:

### Approach A: Ramp push via `veh+0x3480` (ramp write)

Formula: `fake_ramp = now - (now - origRamp) * RATE/12` then `wr_ramp(0, fake_ramp)`.

**Result**: Works intermittently. Log shows `ramp_now == orig` after 2 seconds — game overwrites `veh+0x3480` every frame. Effective rate is blend of our write (active ~33ms) and game's overwrite (66ms) per 100ms poll interval. Slows fuel but not reliably at target rate.

**Log evidence**: `ramp_now=118.242 orig=118.242` after 2s — identical, no drift.

### Approach B: Global rate constant patch at `0x59E96C`

Game reads `float` at VA `0x59E96C` every frame during `PitStopProgress`. Never writes to it. Patching this gives reliable rate control.

**Implementation**:
```c
#define REFUEL_RATE_VA    0x0059E96C
*(volatile float*)REFUEL_RATE_VA = SLOW_RATE;  // 4.0f
```

**Crash**: Writing to `0x59E96C` causes access violation. This address is in `.rdata` (read-only). Fix: use `VirtualProtect` to unprotect the page before writing.

**Must restore 12.0 on recovery/box-exit** to avoid affecting other cars.

**`PitStopOperationTimer` timing**: This function computes pit duration ONCE per stop (during `PitStopReadyCheck`), BEFORE the fuel ramp appears. Our patch fires on ramp capture (AFTER duration is computed), so duration ≠ actual fuel time. **Box lock** (continuous `stopEndTime` push, §4) bridges this gap.

## 3. Audio

All fuel variants use **EID 56 = PWS_2001_79** (CONFIRMED from `full_event_dump.json`):
- EID 56 = PWS_2001_79 = "fuel hose issue" ← correct
- EID 57 = PWS_2001_78 = different clip (says "FUEL CLEAR" — wrong)

Fire timing: commentary fires **on ramp capture** (when `refuelRampStart` first appears in `veh+0x3480`), not at box entry. This ensures:
1. "BRAKE" commentary fires first (on 2→3 edge)
2. Fuel hose commentary fires when fuel ACTUALLY starts flowing

Audio eid is same for all variants currently. Could split per variant via `fuel_eid()` if different clips are identified.

## 4. Box lock — keeps car in pit while fuel fills

`PitStopOperationTimer` computed `stopEndTime` at 12 L/s. At 4 L/s, fuel needs 3x longer. Without intervention, pit ends early → game caps fuel to target → magic full tank.

**Fix**: continuous `stopEndTime` push while fuel variant active:

```c
if (g_fuel_variant != FUEL_VAR_NONE &&
    rd_fuel(0) < *(volatile float*)(VEH_BASE + 0x3484)) {
    float want = sim_t() + 3.0f;
    if (want > rd_endt(0)) wr_endt(0, want);
}
```

On recovery clear: extend timer by `(targetFuel - fuel) / 12.0 + 1.0` so remaining fuel fills at normal rate.

## 5. GUI integration

### Pseudo-eids for variant selection

| GUI Button | Pseudo-EID | DLL handles |
|---|---|---|
| Fuel SLOW | 9002 | Arms FUEL_VAR_SLOW |
| Fuel NO FUEL | 9003 | Arms FUEL_VAR_NOFUEL |
| Fuel CUT | 9004 | Arms FUEL_VAR_CUT |
| Refuel (random) | 56 | Picks random variant (backward compat) |

### Queue architecture

```python
# GUI: always queues fuel variants via veh+0x34A4 (never direct-injects)
FUEL_EIDS = {56, 9002, 9003, 9004}
if eid in FUEL_EIDS:
    self._queue_failure(eid, delay)  # writes to veh+0x34A4
    return
```

```c
// DLL: polls queue every tick while in box (not just on 2→3 edge)
if (ps == 3 && rd_svcrdy(0) == 1) {
    if (g_fuel_variant == FUEL_VAR_NONE) {
        int qeid = rd_queue_eid();
        if (qeid == EID_P_FUEL_SLOW)   { fvar = FUEL_VAR_SLOW; ... }
        // arm variant, reset_fuel_state(), clear queue
    }
    apply_fuel_manipulation();
}
```

Previously the GUI direct-injected commentary (bypassing DLL) when in-box. Fixed: fuel eids always queued.

## 6. Recovery re-roll — NOFUEL and CUT ONLY (changed, v13)

**SLOW no longer recovers.** A partial blockage degrades the flow for the whole
stop; there is nothing to clear. Live trace proved 4 L/s held exactly
(`dfuel=2.01` per 0.5 s poll, ramp fixed) and that the perceived "catch up at
25-30 L" was purely this recovery roll restoring 12 L/s ~5-10 s in. The box lock
now holds the car until the tank reaches target at the degraded rate, and a new
completion check restores the flow constant once it does.

NOFUEL and CUT keep the re-roll — with no recovery those two deliver zero fuel
and would strand the car in the box forever.

Pattern below still applies to NOFUEL/CUT, same as wheel gun:

- `g_fuel_delay` seconds after ramp capture → recovery check
- `fuel_recover_pct` (80%) chance to clear on first attempt
- On fail: extend by `tri(fuel_reattempt)` + replay EID 56
- On clear: restore rate to 12.0, extend timer for remaining fuel

`g_fuel_delay` sampled from `tri(d_fuel_slow)` = ~10s (from INI).

**INI defaults** (updated):
```
fuel_slow_min    = 6.0   fuel_slow_mode   = 10.0   fuel_slow_max    = 18.0
fuel_none_min    = 5.0   fuel_none_mode   = 8.0    fuel_none_max    = 15.0
fuel_cut_min     = 4.0   fuel_cut_mode    = 7.0    fuel_cut_max     = 12.0
fuel_recover_pct = 80
fuel_reattempt_min = 4.0  fuel_reattempt_mode = 6.0  fuel_reattempt_max = 12.0
```

## 7. HUD

Delayed 2 seconds after ramp capture via `g_fuel_hud_at`.

| Variant | HUD text |
|---|---|
| SLOW | FUEL PUMP ISSUE |
| CUT | FUEL PUMP ISSUE |
| NOFUEL | FUEL PUMP MALFUNCTION |

## 8. Open issues / Next steps

1. **CRASH**: `VirtualProtect` needed before writing `0x59E96C`. The page is in `.rdata` (read-only).
2. **Rate restoration**: Must restore 12.0 on every exit path (recovery clear, box exit, DLL unload).
3. **CUT variant untested**: The cut-after-3L logic reads `rd_fuel(0)` and `rd_fuel_start(0)`. Verify `fuel_start` offset is correct at ramp capture time.
4. **Multi-car pit stops**: Global rate patch affects ALL cars. Safe in single-player (only player pits at a time). For AI pitting simultaneously: race condition.

## 9. Files touched

| File | Change |
|---|---|
| `dll/pit_commentary.c` | Fuel variants A/B/C, pseudo-eids 9002-9004, rate patch at 0x59E96C, mid-stop queue poll, box lock, recovery re-roll, HUD, fuel_eid() |
| `dll/pit_commentary_v9.dll` | shipping build (crashes) |
| `<game>/pit_failures.ini` | fuel_slow/none/cut durations, fuel_recover_pct, fuel_reattempt durations, fuel_ramp_delay |
| `tools/pit_failure_gui.py` | Fuel SLOW/NOFUEL/CUT buttons, FUEL_EIDS set, always-queue for fuel |

## 10. Traps

6. **INI overrides C defaults**. `load_cfg()` reads `pit_failures.ini` at DLL start. Changing defaults in C has no effect if old INI exists. Must update INI or delete it.
7. **`veh+0x3480` is RECALCULATED** — game overwrites every frame. Poking it directly is unreliable.
8. **Rate constant at `0x59E96C` is in read-only memory**. Must `VirtualProtect` before writing.
9. **Fire timing matters**. Firing at box entry plays before "BRAKE". Fire on ramp capture ensures correct narrative order.
10. **GUI direct injection bypasses DLL**. When `in_box` was true, GUI called `inject_eid()` directly instead of `_queue_failure()`. Only the DLL handles ramp manipulation.
11. **Queue only read on entry edge**. Added per-tick poll for mid-stop variant arming.
