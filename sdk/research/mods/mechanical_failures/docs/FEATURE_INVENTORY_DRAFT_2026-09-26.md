# Mechanical Failures — feature inventory of work built so far (draft, 2026-09-26)

**Purpose:** answer "what have we already built?" with one evidence-tagged list.
This is a draft inventory, not a release authorization and not a promise that any
item below ships. It was assembled from the sources and records named in
[LLM_ORIENTATION_BRIEF_2026-09-26.md](LLM_ORIENTATION_BRIEF_2026-09-26.md),
not from a fresh live session.

**Evidence tiers used here:**

- **DRIVER-CONFIRMED** — heard/felt in-car by Matias and recorded in the README,
  findings, or progress notes.
- **LIVE-PROVEN** — instrumented in a live session (logs/reads), not necessarily
  judged by feel.
- **BUILT** — code exists and compiles into a named artifact; live behaviour not
  proven.
- **INFRA** — supporting tool or mechanism, not a gameplay feature.
- **DISPROVEN/SUPERSEDED** — kept as record only; do not rebuild on it.

Sources inspected for this inventory (2026-09-26):

- `dll\gear_failure.c`, `dll\engine_failure.c`, `dll\suspension_failure_v5.c`,
  `dll\bluesmoke.c`, `dll\suspension_failure.c` (dev tree)
- `f1c_ida_decompiled\mods\sdk_sandbox\suspension_failure\suspension_failure.c`
- `f1c_ida_decompiled\mods\extended_vanilla_features\tools\mechanical_failure_gui.py`
- `f1c_ida_decompiled\mods\extended_vanilla_features\data\progress.json`
- `f1c_ida_decompiled\findings.jsonl` (mechanical/gearbox/engine/suspension/
  bluesmoke/retire rows)
- Component `README.md`; `development\mods\player_retire_commentary\README.md`;
  `development\mods\eighth_gear\` (status docs)

---

## 1. Gearbox — `dll\gear_failure.c` / `.dll`

All items below are **DRIVER-CONFIRMED 2026-07-30** unless noted. The README's
full mechanism write-up is in the dev tree README, § Gearbox.

### 1.1 Failure mechanics

| Feature | Detail | Evidence |
|---|---|---|
| Player SetGear detour | Hook at `0x447E00` (`PositionLapStateUpdate`), 5-byte `jmp`, prologue guard `8B 44 24 04 56` refuses to patch on mismatch; hand-assembled stub saves state, calls filter, runs stolen bytes, resumes at `0x447E05` | DRIVER-CONFIRMED |
| STUCK gear | No gear change at all; locked at whatever gear was engaged at arm time (or an explicit target gear via `stuck_gear`, waits until the driver is actually in it). Only ever refuses a change, never causes one | DRIVER-CONFIRMED (3867 setter calls, 3862 blocked, "locked perfectly at 1") |
| LOST gear(s) | Any set of gears 1–7 via `broken_mask` bits; selecting a broken gear walks to the next working gear in the direction of travel, falls back the other way, drops to neutral if every gear is gone | DRIVER-CONFIRMED (broken 2 → sequence 1,3,1,3) |
| LOST behaviour selector | Per lost gear: **skip past** (stripped dogs) or **drop to neutral** (recoverable by selecting another gear) | DRIVER-CONFIRMED |
| GEARBOX NEUTRAL fault | Terminal: every requested gear becomes neutral (game's own `arg==0` branch); optional auto-recovery timer (`neutral_recover_ms`, 0 = terminal) | DRIVER-CONFIRMED (live-proven mode) |
| BOTH mode | stuck + lost together | BUILT |
| Time cost on lost gear | First grab spits into neutral; only a second attempt within 1.5 s (`GRIND_MS`) forces the next working gear, so 1→3 is not as quick as 1→2 | DRIVER-CONFIRMED |
| Shift-refusal sound | Fires the game's down/neutral sound `0x4F89A0` on every refused shift, so a refusal does not read as a dropped input | DRIVER-CONFIRMED |
| Progression placeholders (unused) | Modes `MODE_OFF`/`MODE_STUCK`/`MODE_SKIP`/`MODE_BOTH`/`MODE_NEUTRAL`; single-gear `skip_gear`/`skip_by` fallback when no mask is ticked | BUILT |

### 1.2 Realism / escalation

| Feature | Detail | Evidence |
|---|---|---|
| Phase escalation | `PHASE_CHANCE_STUCK = {0, 20, 100, 100}` and `PHASE_CHANCE_LOST = {0, 25, 60, 100}` percent; `progressive` toggle; `escalate_ms` per phase (DLL default 10 s, GUI default 20 s) | DRIVER-CONFIRMED (bug fixed after "didn't seem to stick" feedback) |
| Stuck substitutes THE CURRENT gear | Stale arm-time gear caused the setter to actively engage an old gear and yank the car down; fixed to pass `cur` so the game's early-return fires | DRIVER-CONFIRMED |
| Gate on `engaged`, not `enabled` | CLEAR previously left lost-gear filtering running (could not upshift out of 1st, no release); every filter branch now gates on `engaged` | DRIVER-CONFIRMED (bug fixed) |
| Session safety | Lock released when the session ends; arming from neutral is deferred until the next gear is selected | BUILT (log-verified) |
| Terminal → DNF option | `dnf_enable` + `dnf_after_ms`: writes `ReasonCode=2` (Gearbox), `FinishState=2`, restart gate — the game runs its own teardown | BUILT (not driver-tested per README) |

### 1.3 Race-engineer commentary (14 clips restored)

| Feature | Detail | Evidence |
|---|---|---|
| Clip unblock | `PWS_MECHPROB20–33`, eids 80–93 (lost = `79+g`, stuck = `86+g`); all ship cooldown `FLT_MAX`; DLL clears it to 15 s, idempotent, only where `FLT_MAX` | DRIVER-CONFIRMED (3 heard: eid 84/88/89) |
| Announcement policy | Speaks only after the fault is **total**, then after `announce_delay_ms` (default 2.5 s), naming the gear at that moment; symptom → HUD → failure → radio | DRIVER-CONFIRMED |
| Injection robustness | Retry every 500 ms up to 20 s; confirm via `EVENT_TS(eid) > 0`; does not bail on `CONTROLLER+0xAE4 isPlaying` | DRIVER-CONFIRMED (fixes the three silence bugs of the old script) |
| GUI isolation test button | "Say it" fires a chosen stuck/lost line alone (`cmd_say`), with clip state feedback (idle / speaking / played / GAVE UP) | INFRA |
| Registered byte patch | `gearbox-commentary-unblock`, 14 sites (legacy `patches/manifest.json`); **not applied to the exe** | INFRA |
| GUI clip progress | 3 of 14 ear-checked; 11 still `WIP` in `progress.json` with notes and the isolation procedure | Open item |

### 1.4 HUD / status

| Feature | Detail | Evidence |
|---|---|---|
| HUD banner | Uses the game's own message strip (`0x7CD204`, 8 slots, stride `0x5C`), re-armed every tick: `GEARBOX FAILING`, `GEARBOX FAILURE`, `GEARBOX CRITICAL`, `NO DRIVE` | DRIVER-CONFIRMED |
| GUI status | Live gear/max gear, engaged mode + phase + "driver felt it", blocked/redirected/neutral counters, last request→result, clip state, unblocked count, hook health | INFRA |

### 1.5 Shared-memory ABI + lifecycle

- `F1C_GEARFAIL_CTL` block, magic `'GFAI'`, edge-triggered commands, append-only
  layout, `abi_version` + `struct_size` guard (launcher binder contract).
- Self-eject via event `F1C_GEARFAIL_UNLOAD`; removes hook and restores original
  bytes; never `FreeLibrary` from outside (documented crash class).
- Logs to `dll\gear_failure_log.txt`.

---

## 2. Engine — `dll\engine_failure.c` / `.dll`

**DRIVER-CONFIRMED 2026-07-30** for the base failure set and **2026-08-07** for
cutout/oil leak/retire commentary, unless noted. The `.dll` on disk (2026-08-07
18:52) was verified in this investigation to contain the Aug-7 feature strings
(`MISFIRE CUTOUT ON`, `OIL LEAK STARTED`, `OIL PRESSURE ZERO`, `RETIRE04
pending`), so the cutout/oil-leak/retire work is in the binary. The `.c` is
newer (2026-08-08) — the exact Aug-8 source delta is not proven present
(see §6).

### 2.1 Failure levers (arm the trigger, never forge the end state)

| Feature | Detail | Evidence |
|---|---|---|
| Real player site used | `EngineStressSimulation_0043ef20` fields: life `0x7C91DC`, stress `0x7C91D4` (cap 0.6), banks `0x7C920C/14/1C`, ignition `0x7C9081`; all four `.PLR`/`veh+0x3D70` timers are AI-only | DRIVER-CONFIRMED |
| Stage 1 (smoke) | `life = 1.5` → game fires ReasonCode 1, rolls a bank, radio line comes free, re-arms; white smoke, still drivable | DRIVER-CONFIRMED |
| Terminal (ramp) | `life = −16` → the game's own stress ramp to the 0.6 cap (~100 s), including bank-halving at −10.1 | DRIVER-CONFIRMED |
| Instant (blown) | `life = −16`, `stress = 0.6` (the one deliberate skip-ahead) | DRIVER-CONFIRMED |
| Seizure (binary stop) | Ignition byte `0x7C9081 = 0`, `ReasonCode` forced to 1 (explicit "this is an engine failure now"); no smoke, no progression | DRIVER-CONFIRMED |
| Power vs smoke split | Stress is the misfire/power channel (torque `*= (1 − v36²·stress)` in `Gearbox_00443b70`); banks are cosmetic particle emitters; healthy banks are `0.0`, not `1.0` | DRIVER-CONFIRMED (A/B: stress 0 restored power with smoke still running) |

### 2.2 Emitter controls

| Feature | Detail | Evidence |
|---|---|---|
| White / black / fire toggles | `EMIT_ON = 0.85` written to bank 1/2/3, independent of any failure; top-row `1`/`2`/`3` or GUI checkboxes | DRIVER-CONFIRMED (isolation-proven per bank 2026-07-30) |
| All emitters off | Bank1/2/3 = 0.0 (`4` key or GUI button) | DRIVER-CONFIRMED |

### 2.3 Glitches (mod-authored; no vanilla equivalent)

| Feature | Detail | Evidence |
|---|---|---|
| Misfire cutout (`C`) | Flicks the ignition byte off for random bursts 80–280 ms on random intervals 0.8–3 s; stutters, does not kill; exits with engine running; `Num0` repair clears | DRIVER-CONFIRMED |
| Oil leak (`O`) | Modelled oil-pressure gauge 100→0 over 60 s; blue smoke density rises the whole way via the DLL's own `ParticleTrailEmitter` calls using the live SpecialFX id=2 descriptor; `ENG_STRESS` climbs past 50 % pressure; real seizure (`ENG_RUNNING=0`, `ReasonCode=1`) at 0 % | DRIVER-CONFIRMED |
| Marshal warning | Fires `PWS_MECHANWARN0` (eid **130**, not 131) once when smoke becomes visible (`OIL_WARN_SEVERITY = 0.25`, i.e. pressure ≈ 75 %). README/GUI prose says "50 % mark" — behaviour/docs mismatch worth reconciling | DRIVER-CONFIRMED for the eid; severity-point text is inconsistent |
| Blue-smoke path reuse | Standalone `bluesmoke.c` derivation duplicated into this DLL so it stays self-contained; descriptor re-found each call (table moves per session) | INFRA / LIVE-PROVEN |

### 2.4 Engine-death commentary

| Feature | Detail | Evidence |
|---|---|---|
| `PWS_RETIRE04` (eid 119, not 120) | One line per engine death, single latch, two trigger paths: (a) known-certain death (oil leak, `Num6` seizure) → fixed 3–5 s pending timer; (b) uncertain death (vanilla life ramp) → ported `retire_commentary.c` stop detection: crossed 10 m/s this session, then <2 m/s held 2.5 s, re-check ReasonCode==1 before speaking | DRIVER-CONFIRMED |
| ReasonCode gate | Engine-specific gate `ReasonCode == 1` (a stale nonzero code previously mis-classified the story; `arm_seizure` now forces 1) | DRIVER-CONFIRMED |

### 2.5 Repair / diagnostics / controls

| Feature | Detail | Evidence |
|---|---|---|
| Repair all (`Num0`) | Restores saved life, stress 0, banks 0, corner accum/bends/flags, ReasonCode; restarts the engine the game's own way (running byte + rpm lifted to idle if below); clears cutout/oil state and retire latches | DRIVER-CONFIRMED |
| State dump (`Num8`) | One log line + short HUD summary (life/stress/torque/rpm/running/banks/ReasonCode/phase/session + corner accumulators) | INFRA |
| Telemetry HUD (`Num9`) | 250 ms live readout `S/T/RUN/RC` on the message strip | DRIVER-CONFIRMED |
| Session gate | Arms need `RACE_PHASE > 4`, otherwise HUD `NO LIVE SESSION`; repair/dump/telemetry always allowed | BUILT (log-verified) |
| Hotkey map | `Num3/4/5/6` stages; `Num7` deliberately disabled (suspension moved out); `Num8` dump; `Num9` telemetry; `Num0` repair; top-row `1/2/3/4` emitters; `C` cutout; `O` oil. `Num1`/`Num2` left free (exe red-flag scripts; top-row digits still change camera) | DRIVER-CONFIRMED |
| Shared-memory ABI | `F1C_ENGFAIL_CTL`, magic `'ENGF'`, append-only incl. 2026-08-07 glitch fields; status republished every 50 ms poll; `abi_version`/`struct_size` | INFRA |
| Self-eject | Event `F1C_ENGFAIL_UNLOAD`; worker thread exits via `FreeLibraryAndExitThread` | DRIVER-CONFIRMED (crash class documented) |
| GUI Engine tab | Stage buttons, emitter checkboxes, glitch toggles, repair/dump/telemetry, live life/stress/torque/rpm/reason/phase/session, cutout/oil status, inject/unload | INFRA |

---

## 3. Suspension — `dll\suspension_failure*.c` / SDK sandbox port

Base mechanism is **LIVE-PROVEN / DRIVER-CONFIRMED 2026-08-08** (two sessions:
instrumented proof and handling-feel test day). The forward-looking source is the
SDK sandbox port (2026-08-14).

### 3.1 Mechanism

| Feature | Detail | Evidence |
|---|---|---|
| Game's own bend input | Calls `SteeringGeometryUpdate_00442060(0x7C7428, wheel, severity)` — the exact call the game makes on an impact — instead of writing the bend fields (writes DISPROVEN + crash 2026-07-30) | DRIVER-CONFIRMED |
| Detour site | `VehicleMainPhysicsUpdate_004441f0`, 6-byte `push imm32/ret` because the prologue has no 5-byte boundary; 12 stolen bytes run in the stub; call made on the **game thread** at ~20 Hz | LIVE-PROVEN |
| Prologue guard | Refuses to patch unless the 12 stolen bytes match | BUILT |
| ReasonCode | The game's own accumulator trip raises `ReasonCode = 4` (Suspension); treated as trigger, not state | LIVE-PROVEN |

### 3.2 Stages, wheels, repair

| Feature | Detail | Evidence |
|---|---|---|
| Stage ramp | `APPLY_COUNT/SEV` = FAILING 2×0.30 (~1–3°), FAILURE 3×0.60 (~3–6°, retuned from 8×0.60 which slammed the clamp), COLLAPSED 30×1.00 (terminal, at the game's ±10° clamp) | DRIVER-CONFIRMED (clamp behaviour and retune both driven) |
| Wheel selection | Bitmask FL/FR/RL/RR, default RL+RR; GUI checkboxes; `Num8` cycles REAR → FRONT → ALL with publish-back so GUI cannot fight (a real bug fixed); REAR/FRONT/ALL | DRIVER-CONFIRMED for rear; front-only **not driven yet** |
| Repair | Zero bend angles/accumulators/flags + clear ReasonCode; live-proven clean restore | LIVE-PROVEN |
| Disarm vs repair | CLEAR releases the arming without restoring geometry; REPAIR restores | BUILT |

### 3.3 Radio, HUD, auto-reset (version differences)

| Feature | Detail | Evidence |
|---|---|---|
| Radio eid 75 | `PWS_MECHPROB16` "Damaged suspension – avoid kerbs" once per arm; confirmed by `EVENT_TS > 0`, 500 ms retry / 20 s | DRIVER-CONFIRMED |
| HUD (dev v5 only) | `SUSP FAILING/FAILURE/COLLAPSED/REPAIRED/OFF` on the game strip | BUILT (v5 source; not part of sandbox port) |
| Auto-reset | Clears damage + arm state when the player leaves the session, retires in a race, or returns to the pits in a non-race session (driver report 2026-08-14: HUD text persisted and bend damage survived session exit) | BUILT; trigger conditions documented; no driver-confirmed record of the reset itself found in this pass |
| `Num7` stage cycle | FAILING → FAILURE → COLLAPSED → off | DRIVER-CONFIRMED |

### 3.4 SDK sandbox port (canonical suspension source going forward)

| Feature | Detail | Evidence |
|---|---|---|
| SDK runtime validation | `f1c_target_validate_headers()` + strict code probe; refuses to start (`FreeLibraryAndExitThread`) with a target result string; requires SDK 1.x | BUILT (fail-closed path in code) |
| INI gate | `suspension_failure.ini [Mod] Enabled=` (default 1); disabled → self-exits | BUILT |
| Status via shared memory instead of HUD | `SusCtl.msg[64]`; no in-game text (deliberate 2026-08-14 change) | BUILT |
| Wheel-mask ordering fix | Wheels adopted **before** commands so an arm in the same tick bends the just-ticked wheels | BUILT (code comment; bug fix) |
| Exports | `F1CModId`, `F1CModSdkVersion` | BUILT |
| Build path | `python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure` | INFRA |

---

## 4. Blue smoke standalone — `dll\bluesmoke.c` / `.dll`

| Feature | Detail | Evidence |
|---|---|---|
| Direct emitter path | Walks the live SpecialFX reaction table (`0x8ACEA0+16/+20`, stride 108, id at +4) and calls `ParticleTrailEmitter` (`0x4CBC50`) directly; never caches the array base | LIVE-PROVEN |
| Effect browser | `Num*` / `Num/` step through every reaction in the live table; `Num0` toggles; 3 jittered particles per 50 ms tick; HUD shows `index/total NAME`, and `Max=0` when the game allocated no particles | BUILT (browser); LIVE-PROVEN (descriptor/calling convention) |
| Safety skips | RAIN (id 13) crashes the emitter (reproduced live, root cause not diagnosed) and is skipped in the cycle | LIVE-PROVEN (crash), BUILT (skip) |
| Purpose for the product | Source of the oil-leak blue-smoke emitter path (duplicated into `engine_failure.c` for self-containment) | INFRA |

---

## 5. Tooling / infrastructure built alongside

| Item | What it does | Evidence |
|---|---|---|
| `tools\mechanical_failure_gui.py` (legacy tree) | 3-tab trial GUI (Gearbox / Engine / Suspension): inject/unload via `inject.exe`, ARM/CLEAR/REPAIR, parameter editing, live status, clip test button, shared log pane; liveness via named unload event (not stale section) | INFRA |
| `mechanical_failure_gui.bat` | Elevating launcher | INFRA |
| `tools\dll_unload_signal.py` (pattern) | Signals the named self-eject event (never external `FreeLibrary`) | INFRA |
| Shared-memory ABI blocks | `F1C_GEARFAIL_CTL`, `F1C_ENGFAIL_CTL`, `F1C_SUSFAIL_CTL`, magic + append-only fields + ABI guard; used by GUI and launcher binder contract | INFRA |
| `mechanical_failures.py` | **Superseded/DISPROVEN** poll-based poke script, kept only as record (header says DEPRECATED, do not use) | DISPROVEN |
| `mt_mech.dll`, `mintest_here.dll` | Test/experimental binaries, not product | INFRA |
| `retire_commentary.c` (dev ancestor) | Working player-retire commentary DLL whose stop-detection state machine was ported into `engine_failure.c`; the mod itself lives at `development\mods\player_retire_commentary\` | DRIVER-CONFIRMED (ancestor) |
| Run logs | `gear_failure_log.txt`, `inject_out.txt`, per-DLL `*_log.txt` next to binaries | INFRA |

Verification used in this pass: `engine_failure.dll` was binary-searched for the
Aug-7 feature strings (all found); `suspension_failure_v5.c` vs the sandbox port
diffs at source level (157 insertions / 127 deletions; the differences are the
sandbox items in §3.4 plus HUD removal). No live session was run and no game
memory was read for this inventory.

---

## 6. Cross-cutting facts established while building (research results, not features)

- Vanilla `.PLR` "Mechanical failure rate" and the `veh+0x3D70/74/78/7C` timers
  are AI-only; the player never enters `FuelConsumptionAndWear_004ed960`.
- `damage.ini [PHYSICAL] Engine=` is collision-impulse seizure, unrelated to the
  wear model (F1C has no `EngineSeizure` token).
- Engine life drain measured 0.14/s idle → 0.28/s at stress 0.1415 → 0.635/s at
  stress 0.6; stress feeds back into life drain ~1:1.
- Misfire model: `flt_8AAE74` is a 256-entry uniform-random table, `dbl_8AFEA0`
  is SessionTime; flutter scales with revs (~11.5 Hz near the limiter).
- Suspension bend fields are READ-proven telemetry, not writable inputs; the
  only writer is `SteeringGeometryUpdate_00442060`; the bent flag is at +1105
  (not +1104).
- The game's message strip is the only HUD surface that works in-race
  (`TimerDisplayRender` and `DrawDebugTextOverlay` paths were dead ends).
- Commentary channel is chosen by eid range (PWS 1–290, TV 292–535); eids
  201–217 are other-driver lines, 340–383 are TV twins.
- `sub_41AC50` is **not** entered during a player engine failure, so the engine
  radio line needed a DLL emitter (the earlier "comes free" claim was
  disproven); PWS_MECHANWARN0/ RETIRE04 work-around is the DLL queue injection.

---

## 7. Open mismatches and gaps found during this inventory

1. `engine_failure.c` (2026-08-08) is ~19 h newer than
   `engine_failure.dll` (2026-08-07). The DLL **does** contain the Aug-7
   cutout/oil-leak/retire strings; the Aug-8 source delta is unverified and the
   artifact should be rebuilt before any release use.
2. Canonical `dll\suspension_failure.dll` in the dev tree is still the
   pre-retune 13:21 build; `suspension_failure_v5.c/.dll` (2026-08-14) is newer,
   and the sandbox port is newer still. The dev-tree DLL is not the current
   suspension behaviour.
3. Suspension "auto-reset" is implemented in v5 and the sandbox port but no
   driver-confirmed record of the reset firing was found in this pass.
4. Oil-leak marshal warning: code fires at `OIL_WARN_SEVERITY = 0.25`
   (≈ 75 % pressure); README and GUI prose say "50 % mark". Reconcile before
   documentation ships.
5. `Num8` collides between `engine_failure.dll` (state dump) and
   `suspension_failure.dll` (wheel cycle) when both are injected.
6. Ignition byte `0x7C9081` is also toggled by Pit Stop Failures 1.0.3
   (`live_client.cpp` ~line 399) — ownership/conflict still unresolved.
7. `eighth_gear` checked in this pass: configuration-only overlay (HDV/SVM
   files), **no DLL hook or gear-address reference**, so no hook-overlap risk
   with `gear_failure.dll` at the code level.
8. Not built at all (for scoping, not a defect list): automatic triggering,
   Lightloader host validation, code/layout compatibility gate, INI for the
   gear/engine DLLs, tests, installer, packaging, release candidate,
   `governance\PRODUCT_STATUS.json` entry.

---

## 8. Suggested use of this list

This is a **candidate feature menu**, not a ship list. For the step-by-step
process Matias asked for, each feature should be tagged with one of: keep as-is
for 1.0.0, retune, defer, or drop. The open items in §7 are the checks that
should happen before any of it is packaged.
