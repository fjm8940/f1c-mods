# Ignition Investigation — Session 2026-07-29

> **SOLVED 2026-07-30 — the answer is byte `0x7C9081`.**
>
> This document records a hunt that did not find it. The goal below ("find the
> memory flag that controls whether the engine is running, and toggle it")
> is achieved by writing `0x7C9081` (the static player physics object
> `0x7C7428`, field `this+7257`): `0` stops the engine, `1` restarts it, with
> behaviour the driver confirmed is identical to the bound ignition key.
>
> It gates torque through the game's own code — `sub_447B10` opens with
> `if (!this[7257]) return 0.0;` and `Gearbox_00443b70` ends with
> `this[8] = sub_447B10(this) * this[8]`.
>
> Proof: Frida polled the byte at 10 Hz for 8 s while the driver pressed the
> real ignition key — four presses, four clean flips (1→0, 0→1, 1→0, 0→1),
> no drift.
>
> **Everything below remains valid as disproof** of the offsets it tested
> (`veh+0x39A0` recalculated, `veh+0x3984` is fuel, the key queue and
> DirectInput buffer both overwritten before the game reads them). None of
> those was this byte — the concept was right, the offsets were wrong.
>
> One thing still open: the ignition keybind does **not** reach the byte via
> `sub_4472A8` / `sub_4471F0`. Both were hooked during the same window and
> neither was called, so the real writer is inline somewhere else and is
> still unlocated. Use an x32dbg `bphws` on `0x7C9081` to find it — a Frida
> hardware watchpoint crashes this process.
>
> See `findings.jsonl` (2 rows, 2026-07-30) and
> `mods/extended_vanilla_features/patches/mechanical_failures/dll/engine_failure.c`
> (`Num6` drives it).

## What We Set Out To Do
Find the memory flag that controls whether the engine is running, and produce a
script that can toggle it directly (kill/revive engine via memory poke).

## What We Discovered

### Ignition Mechanics (from static analysis)

**Binding:** `Control - Ignition="(0, 51)"` in PLR file where 51 = DirectInput
keycode for `,` (comma). Companion `Control - Starter="(0, 52)"` for `.` (period).
Both hidden unless `Options Num Controls="44"`.

**Engine restart gate:** `OnboardStarter` in `engine.ini`. ALL cars in this mod
have `OnboardStarter=0` — engine CANNOT be restarted after stall.

**Auto-retirement timer:** After engine stops, `sub_4F1FE0` (retirement processor,
called per-frame from `VehicleYawRotationRecoveryController`) monitors the car.
After ~10 seconds stationary with dead engine, the game sets:
- `FinishState (0x4064) = 2` (retired)
- `RetiredFlag (0x4410) = 1`
- `driverStatus (0x4510) = 5` (garaged)
- `carConditions (0x3D6C) = 2`
- Car teleported to garage

**RPM writer:** `sub_4E51A0` at 0x004e51a0 is called ~80 Hz from
`PhysicsIntegrationStateMachine`. EngineRPM (0x39A0) and EngineAngVel (0xA0) are
VOLATILE — physics recalculates them every frame. Poking them to 0 does NOT stick.

### Flag Location

**Vehicle struct scan:** `ignition_trace.py` sampled the entire 0x48C0 vehicle
struct at 10 Hz and found NO discrete flip on ignition toggle. The ignition state
flag lives OUTSIDE the vehicle struct — in a heap-allocated child object.

**Full-process scan:** `ignition_scan.py` took snapshots of ALL writable memory
(engine ON vs OFF). The diff found 714 isolated boolean flips consistent across
two independent ignition-off runs. The 0xD030-0xD03B cluster has 17 byte-level
flips that are the prime candidates. The flag was NOT narrowed to a single address.

### What We Tried

| Approach | Result |
|----------|--------|
| `inject_key.py 0x33` (write comma to key queue 0x7CB21C) | **FAILED.** Key queue only handles camera/UI keys (via `KeyboardInputActionMapper`). Driving controls (ignition, starter, throttle) go through DirectInput polling, not the raw key queue. |
| `vkey.py tap comma` (DLL patches DirectInput DIK buffer) | **FAILED.** DLL injected OK, shared-memory byte 0x33 flipped correctly, but game did not respond. Likely blocked by session state (checkered flag / RacePhase=5). |
| `gamekey.py comma` (Interception driver, hardware-level) | **FAILED.** Interception driver doesn't recognize key name "comma". |
| Full-process snapshot + diff + poke (`find_ignition_flag.py auto`) | **FALSE HIT.** RPM never dropped (key injection didn't work), so diff was all physics noise. Candidate 0x000AE41B falsely appeared to restore RPM (RPM was already 419.9). |
| Direct RPM poke (write 0 to 0x39A0) | **DISPROVEN.** "Single write of 0 to veh+0x39A0 restored to 419.851 within 30 ms (audible RPM dip reported, then recovery)." |
| Pin fuel level (write 0 to 0x3984) | **DISPROVEN.** Value kept drifting — rewritten every frame. |
| ActiveFlag poke + vkey injection | **FAILED.** Even with ActiveFlag=1 in checkered phase, comma injection didn't kill engine. |
| Frida hook on `sub_4E51A0` (RPM writer) | **CONFIRMED** works at 80 Hz. `this.context.ecx` returned vehicle base + 0xC. Function identified but NOT hooked for engine kill. |
| Self-eject DLL pattern | **CONFIRMED.** Named event + `FreeLibraryAndExitThread` is the working pattern for hot-swapping injected DLLs without game restart. |
| `suspension_bend.py` — poke-proven suspension damage offsets | **FACT.** Player car: RL -1.927°/+1.503°, RR +3.323°/+1.072°, accumulators > 0.015 trip, ReasonCode=4. Base 0x7C7428, stride 1124, bend at +1116/+1124, flag +1105, accumulator +1132. |
| Pit crew animation freeze via Frida | **FACT.** 34 actors at veh+0x32D4 → crew → actor array (stride 120). Per-actor freeze proven via `RangeProportionInitializer` hook. |
| `tools/find_ignition_flag.py` — automated scan+poke script | Built and tested. Successfully snapshots full process, diffs, and iterates candidates. Blocked by failure to actually toggle ignition in current session state. |

### The Actual Blocker

The ignition toggle (comma key) works via DirectInput's analog/digital control
polling system, NOT the raw key queue. The raw key queue at 0x7CB218/0x7CB21C
only dispatches camera/UI keys. For driving controls, the game polls DirectInput
through the control profile system.

Two approaches can bypass this:
1. **Hook the RPM writer** (`sub_4E51A0` at 0x004e51a0) — force output to 0
   every frame. Requires DLL injection + Frida or code cave.
2. **Find the ignition state flag** in the heap child object and poke it.
   The 0xD030-0xD03B cluster needs narrowing — run `ignition_scan.py` with
   the game in a GREEN-FLAG session (RacePhase=4, engine actually responding
   to inputs), then use the poke-verify loop.

### Tools Created
- `tools/find_ignition_flag.py` — auto scan/diff/poke loop to find the flag
- `tools/watch_rpm.py` — real-time EngineRPM monitor
- `tools/check_session.py` — session state inspector
- `tools/force_ignition_off.py` — combined ActiveFlag+vkey+inject attempt
- `tools/probe_control_profile.py` — reads control profile struct
- `tools/verify_vkey.py` — checks vkeys.dll shared-memory state

### Key Offsets (for reference)
| Offset | Name | Type | Notes |
|--------|------|------|-------|
| 0x39A0 | EngineRPM | float | VOLATILE, recalculated every frame |
| 0x00A0 | EngineAngVel | float | VOLATILE |
| 0x4510 | driverStatus | int32 | 0=follow, 5=garaged |
| 0x4410 | RetiredFlag | int32 | |
| 0x45F4 | ActiveFlag | int32 | |
| 0x4064 | FinishState | int32 | 0=racing, 2=retired |
| 0x4068 | ReasonCode | int32 | |
| 0x3D6C | carConditions | int32 | |
| 0x32D4 | crewPtr | ptr | → pit crew actor array |
| 0x7CB218 | keyQueueCount | int32 | raw key queue count |
| 0x7CB21C | keyQueueList | int16[] | raw key queue (camera/UI only) |
| 0x5EA518 | ActiveControlProfilePtr | ptr | → control bindings struct |
