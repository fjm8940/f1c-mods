# Pit failure system + wheel gun audio — session record, 2026-07-29

What changed, what was proven, and the five traps that cost time. Read this
before touching `dll/pit_commentary.c`.

Shipping build: **`dll/pit_commentary_v8.dll`**.

---

## 1. Player retirement — SOLVED, two writes

The long-standing blocker ("wheel jammed DNF plays audio but can't trigger
actual retirement from polling — needs `CreateRemoteThread` calling
`sub_4F1FE0`") was wrong. It needs neither.

```c
*(int *)(VEH_BASE + 0x4064) = 2;    /* FinishState = DNF      */
*(int *)0x007CE258          = 1;    /* player out -> teardown */
```

`RestartGate_004a0fd0` reads `dword_7CE258` each tick; when it is 1 it calls
`ConditionalInitGuard()` -> `sub_4A0250()` = session teardown to the menu.
`sub_4F1FE0`'s own player-retire branch ends with exactly this write
(`if (!this[3247]) dword_7CE258 = 1;`), so this is the game's path, not forged
state.

Live: flag read back 0 within 0.5 s (consumed), retirement + menu return
confirmed on screen. `dword_7C569C` stayed 5 — the `7C569C = 6` arm needs
`dword_7CE264 == 1` and is a different exit.

Implemented as `retire_player()`.

## 2. Ignition — DISPROVEN, do not retry

The keybind is real (`Matias.PLR`: `Control - Ignition="(0, 51)"` = keyboard,
scancode 51 = `,`) and pressing it in-game does retire the car. It still cannot
be replicated from a mod. Everything below failed live:

- **Key queue** (`word_7CB21C` + `dword_7CB218`): the write is accepted, the
  engine is unaffected. That queue feeds hardcoded scancode handlers (assist
  toggles etc.), not bindable controls.
- **DirectInput keyboard buffer** (found at `0x724812F8` via a 3-state memory
  diff): DirectInput refreshes it from the device before the game reads it, so
  the byte never survives to be seen.
- **Engine speed** `veh+0x39A0 = 0`: RECALCULATED — restores within one 30 ms
  frame. The write *does* land (audible RPM dip, then recovery).
- **`veh+0x3984`**: this is FUEL, not an ignition gate — `SuspensionAndWheelPhysics_004e5f60:558`
  computes it from setup fields, and it visibly drains on its own.

Ignition-off is also **irreversible** — it retires you — so the reversible
A/B/C memory-diff that would isolate the flag is not possible.

## 3. The engine object is `veh+0xC`, not `veh`

Cost the most time this session. `sub_4E51A0` / `sub_4E5AE0` / `sub_4EFA40`
operate on `veh+0xC`, so their decompiled `this+N` maps to `veh+0xC+N`. Proven
by a Frida hook reading `ecx`: `0x5f0fa4` and `0x5f5864` = vehicle bases
`0x5f0f98` / `0x5f5858` plus `0xC`, 160 calls each per 2 s (80 Hz).

With the correct base the fields read coherently:

| `this+N` | `veh+` | meaning |
|---|---|---|
| 14740 | 0x39A0 | engine speed |
| 14744 | 0x39A4 | rev limit (1874.5) |
| 14748 | 0x39A8 | **idle** (398.7) |
| 14712 | 0x3984 | fuel |
| 14776 | 0x39C4 | starter timer |
| 14832 | 0x39FC | CurrentGear (independently matches `name_index`) |

Read with the vehicle base instead, the same fields are nonsense — the 0/1 byte
at `this+14828` reads as float 7.39. **A single offset appearing to match is not
proof of base identity**; `veh+0x39A0` lining up with RPM was a coincidence that
sent the first three attempts down the wrong path.

## 4. Failure system rewrite

### Probabilities: ~55% -> 11.4% per stop

`pick_player()` used independent sequential rolls that stacked. The per-corner
loop alone was `1 - 0.88^4` = 40%. Replaced with ONE per-mille draw and
cumulative buckets, so the configured numbers are the actual odds:

| Failure | ‰ | notes |
|---|---|---|
| slow corner | 40 | weighted corner pick |
| wheel gun (recovers) | 25 | |
| fuel rig | 20 | 3 variants |
| stall | 15 | |
| cross-thread | 10 | new; long, can escalate |
| **wheel jam -> DNF** | 4 | terminal |

Clean 88.6%, failure 11.4%, terminal 0.4%.

### Durations: fixed constants -> triangular(min, mode, max)

Every failure had ONE constant, so every wheel gun failure cost exactly 15.0 s.
Now sampled per stop via `tri()`. Rear corners take `rear_penalty` longer.

### Depth without new clips

- **Recovery re-roll**: 85% clear per attempt, else extend and re-fire eid 66.
- **Escalation**: a cross-thread that has resisted twice has a 20% chance of
  turning terminal — the DNF becomes an outcome of the failure tree rather than
  an unrelated coin flip.

### Wheel jam now pins the car in the box

Previously it released the car and fired the DNF on pit exit. Now `g_jam_hold`
keeps it in the box until the retirement fires. `stopEndTime` is re-armed to an
**absolute** `sim_t() + HOLD_LEAD` every tick, never `+=` — if the DLL dies
mid-hold the car frees itself in 3 s instead of being stranded (the known
`taskkill /F` hazard). If the car somehow leaves anyway, it retires immediately.

### Two bugs fixed

1. **Wheel gun discarded its sampled delay.** `pick_player()` set
   `DELAY_WHEELGUN`, then the fire block did `float extra = 25.0f;` and used
   that instead. The constant that looked like it controlled duration did not.
2. **Slow-corner fired only the last corner.** The loop rolled all four,
   accumulated `total += DELAY_SLOW_TIRE` per hit, but overwrote `picked` — two
   failed corners gave double delay with one corner's audio.

### Everything is INI-driven

`pit_failures.ini` next to the exe (probabilities, corner weights, every
duration triple, recovery/escalation, audio repeat cap). Loaded once at DLL
start; reload = unload-signal + re-inject, no rebuild.

## 5. Wheel gun audio

### The old behaviour

One WAV, once per stop, at a fixed `tyre_start + 2.0` — unrelated to any gun
animation. Hence "one gun noise at the start and silence after".

### What it does now

- **Timings come from the live crew**, not hardcoded: `veh+0x32D4` -> crew,
  `+0x0C`/`+0x10` count/array (stride 120), `actor+112`/`+116` channel count and
  array (stride 28), `channel+12`/`+16` = baseStart/baseEnd. Falls back to
  fronts 2.3->9.8 / rears 1.0->9.8 and logs which it used.
- Measured windows: **FL 2.3/9.8, FR 2.5/9.8, RL 1.0/10.4, RR 1.0/10.4** — the
  rears start earlier AND run ~0.6 s longer than the fronts.
- **One clip per corner per stop.** Each `wheelguns_N.wav` is already a complete
  BRRT-BRRT cycle, so one clip is a corner's whole job. A clean stop is 4 clips,
  staggered by the real windows.
- **Only a failing corner repeats**, spaced `tri(2.5, 3.5, 5.0)` and hard-capped
  at `gun_max_repeats` (3).

### Polyphony

`PlaySoundA` plays ONE sound at a time per process — a second call cuts the
first off. It never showed up while the DLL played a single WAV per stop.
Replaced with **8 `waveOut` voices as a shared pool**, round-robin with
"prefer a finished voice" selection, so clips layer freely including a corner
over itself. WAVs are decoded once into memory (48 kHz stereo).

---

## Traps — all five cost real time

1. **`actor+100` is a `char*` TO the name, not inline characters.** Reading it
   as inline text made `strncmp(nm, "WHEELGUN", 8)` compare pointer bytes, so
   the crew walk silently found nothing and fell back. `dump_crew_struct.py`
   flags it as a "string-pointer slot" — believe it.
2. **Never call `waveOutOpen` from `DllMain`.** It pulls in audio driver DLLs
   under the loader lock and deadlocks. The injector still prints "Injected OK"
   (`CreateRemoteThread` returns before `DllMain` finishes), so the only symptom
   is a missing log line — and afterwards **no DLL can load into that process at
   all**, requiring a game restart. `audio_init()` runs at the top of
   `thread_main` for this reason.
3. **A single matching offset does not prove base identity** — see §3.
4. **State reset placed inside a conditional branch that cannot run.** The burst
   scheduler's reset lived inside the `ps == 3` (in-box) branch, so it never ran
   between stops and bursts were scheduled once per session. Resets belong on
   the exit path.
5. **The sim only ticks while the game has focus.** Every live test run from a
   shell is invalid unless `mods/no_tab_pause` is injected — several early
   measurements this session read bit-identical values and looked like "the
   write did nothing". Always confirm the sim is running (count changing vehicle
   dwords per second) before concluding a poke failed.

## Files touched

| File | Change |
|---|---|
| `dll/pit_commentary.c` | retire_player, jam box-pin, single-roll picker, `tri()`, INI loader, waveOut mixer, crew gun windows |
| `dll/pit_commentary_v8.dll` | shipping build |
| `<game>/pit_failures.ini` | new — all tunables |
| `docs/FAILURE_PLAN.md` | new — catalogue, probabilities, build order |
| `docs/MOD_REFERENCE.md` | corrected the stale "cannot retire from polling" + ignition sections |
| `tools/pit_failure_gui.py` | added Cross-Thread button |
| `patches/pitstop_modding/pit_failure_gui.bat` | new — self-elevating launcher (GUI needs admin) |
| corpus `tools/ignition_trace.py`, `ignition_scan.py`, `inject_key.py` | new — investigation tools, kept for the record |
