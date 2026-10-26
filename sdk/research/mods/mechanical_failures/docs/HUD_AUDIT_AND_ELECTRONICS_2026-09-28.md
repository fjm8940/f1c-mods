# HUD/radio audit, oil leak in the engine mix, ELECTRONICS category (2026-09-28)

Owner requests:
1. Audit all in-game HUD messages and race-engineer voicelines - no debug
   messages in the player-facing HUD, only simple/realistic text.
2. Add oil leak as part of the engine failure (auto/timer mix).
3. New player-specific category **ELECTRONICS** with radio
   `PWS_MECHPROB14` "your electronics are periodically malfunctioning",
   game-logic issues: (3.1) fuel < 1 L -> throttle cutting, (3.2) misfire
   cutout, (3.3) losing race-engineer communications (HUD "RADIO COMMS
   LOST").

**Status:** development trial, ``. Built
(`-Wall -Wextra -Werror` clean), binary string-verified, off-game tests pass
(smoke + 17/17), electronics ABI round-trip byte-exact. **No live validation
yet** (game closed).

## 1. HUD + radio audit

### 1.1 In-game HUD messages (game message strip 0x7CD204)

| DLL | Message | Before | Verdict / action |
|---|---|---|---|
| gear | `NO DRIVE`, `GEARBOX FAILURE/FAILING/CRITICAL` | short CAPS, plausible | **kept** |
| susp | none (HUD removed 2026-08-14; status via panel `msg`) | - | **unchanged** |
| engine | `ENGINE STAGE 1 (SMOKE)` | dev wording | -> `ENGINE SMOKE` |
| engine | `ENGINE TERMINAL (RAMPING)` | dev wording | -> `ENGINE FAILURE` |
| engine | `ENGINE BLOWN (STRESS 0.6)` | dev value in text | -> `ENGINE BLOWN` |
| engine | `ENGINE SEIZED` | realistic | kept |
| engine | `OIL PRESSURE ZERO - SEIZED` | double wording | -> `ENGINE SEIZED` |
| engine | `OIL LEAK STARTED` | clunky | -> `OIL LEAK` |
| engine | `OIL LEAK STOPPED P%.0f` | dev telemetry | **removed from HUD** (log only) |
| engine | `REPAIRED` | terse | -> `ENGINE REPAIRED` |
| engine | `MISFIRE CUTOUT ON/OFF` | panel state, not driver reality | **removed from HUD** (log only) |
| engine | `WHITE/BLACK/FIRE ... ON/OFF`, `EMITTERS OFF` | test-tool state | **removed from HUD** (log only) |
| engine | `TELEMETRY ON/OFF` | debug | **removed from HUD** (log only) |
| engine | `L%.0f S%.2f T%.0f RC%d` / `S..T..RUN..RC` | raw debug dump | **removed from HUD** (log only) |
| engine | `NO LIVE SESSION` | not a driver reality | **removed from HUD** (log only) |
| electronics | `ELECTRONICS FAULT`, `RADIO COMMS LOST`, `FUEL STARVATION`, `ELECTRONICS OK` | new, driver-plausible | added |

Panel `msg` fields and the activity log keep the discarded detail for
testing; the player-facing strip now carries only realistic messages.

### 1.2 Race-engineer voicelines

| Family | eid | Clip | Verdict |
|---|---|---|---|
| gearbox | 80-93 | PWS_MECHPROB20-33 lost/stuck N | realistic, kept |
| suspension | 75 | PWS_MECHPROB16 damaged suspension | kept |
| engine | 130 | PWS_MECHANWARN0 (meatball / mechanical problem) | real line, kept for oil leak warn |
| engine | 119 | PWS_RETIRE04 (retirement on engine death) | kept |
| electronics | **76** | PWS_MECHPROB14 "electronics malfunction" | added |

**eid correction (measured, not guessed):** `full_event_dump.json` puts
PWS_MECHPROB14 at **eid 76** (score 74, cooldown 15.0, unblocked, PWS
channel). eid 77 is PWS_MECHPROB15 "Engine overheating" - using 77 would
play the wrong clip. No debug or placeholder lines exist in the radio path.

## 2. Oil leak joins the engine failure mix

- Owner weights (2026-09-28): **smoke 30 / wear 30 / instant 10 / seizure 10
  / oil leak 20**.
- Both sources use the table: the fixed-chance roll and the vanilla timer
  expiry.
- New `arm_oil_leak()` (deterministic arm, shared with the manual toggle)
  fires through the proven oil pipeline: pressure 100->0 over 60 s, blue
  smoke, eid 130 marshal warning at 25% severity, stress ramp past 50%,
  real seizure at 0% with eid 119.
- Last-lap guard unchanged (instant/seizure -> smoke). Oil leak is a slow
  burn, so it is allowed inside the final laps - the player can still make
  the finish.

## 3. ELECTRONICS category (new DLL, `electronics_failure.dll`)

### 3.1 Design

- One seeded roll per race/qualifying session, default **10%** (owner
  2026-09-28), same pattern as the other families. Panel chance combo
  5-100%.
- On a hit the car's electronics are **faulty for the session**:
  - **Radio eid 76** once + HUD `ELECTRONICS FAULT` (proven queue-injection
    + EVENT_TS confirmation pump, 500 ms retry / 20 s window).
  - **Misfire cutout** - periodic ignition-drop bursts on the proven
    `ENG_RUNNING` (0x7C9081) lever; the byte is restored to its pre-burst
    state, so it can never revive an already-dead engine.
  - **RADIO COMMS LOST** windows every 60-150 s for 8-15 s: the game's own
    commentary gate byte (`CM_CONTROLLER+0xAF0`, the early-out in
    `TextOverlayDisplayTimer_004186f0`) is cleared while the window runs,
    pending PWS entries (eid 1..290, channel split proven in `sub_4119D0`)
    are invalidated, and the HUD shows `RADIO COMMS LOST`. The TV channel
    (292..535) is untouched. A line already playing finishes.
- **Fuel < 1 L starvation**: while faulty, fuel below 1.0 L
  (`veh+0x3984`, float litres - "veh+0x3984 is FUEL" finding 2026-07-29)
  shows `FUEL STARVATION` once and intensifies the cutout schedule
  (120-400 ms drops every 0.3-1.2 s on top of the game's own starvation
  flicker). Without the electronics fault, the native flicker is unchanged.
- Manual controls: ARM / CLEAR+RESTORE COMMS / MISFIRE BURST / RADIO LOSS
  10 s / Dump / Inject / Unload. No game hook; poll + proven-field writes
  only, self-eject via `F1C_ELECFAIL_UNLOAD`.
- ABI: `F1C_ELECFAIL_CTL`, magic `0x43454C45`, 21 int fields, 84 bytes,
  `_Static_assert` locked; panel tab "Electronics"; LOAD ALL now injects
  four DLLs.

### 3.2 Known limitations (documented, not hidden)

- Radio-loss suppression is static-analysis-based: the gate byte and head
  eid are read from `TextOverlayDisplayTimer_004186f0`. One 20 Hz poll may
  miss an entry inserted and consumed inside the same 50 ms window; the
  gate byte covers this, but both need live confirmation.
- The fuel field type (float at `veh+0x3984`) is evidence-based, guarded by
  a plausibility window; confirm live.
- Both the engine tab's manual cutout and the electronics periodic cutout
  write `ENG_RUNNING` - they must not be enabled simultaneously in a test
  (documented; they do not fight in normal use).

## 4. Verification so far

| Check | Result |
|---|---|
| engine: `gcc -O2 -Wall -Wextra -Werror` | exit 0, no warnings |
| electronics: same | exit 0, no warnings |
| `_Static_assert` 84 bytes (electronics); EngineCtl unchanged 184 | pass |
| Electronics binary contains `RADIO COMMS LOST` / `ELECTRONICS FAULT` / `FUEL STARVATION` | yes |
| engine binary rebuilt with oil type + new HUD strings | yes |
| Panel smoke (4 tabs) + contract tests | PASS, 17/17 |
| Electronics ABI round-trip (ctypes harness) | PASS - 84 bytes, offsets byte-exact, get/set OK |

## 5. Pending live validation

1. Engine roll/timer -> oil leak type: blue smoke, eid 130 warn, stress
   ramp, seizure, eid 119. Check the 30/30/10/10/20 distribution over a few
   sessions (force with 100% chance).
2. Electronics ARM: eid 76 plays; HUD `ELECTRONICS FAULT`; periodic
   `RADIO COMMS LOST` windows actually silence new engineer lines; misfires
   felt; CLEAR restores comms.
3. Fuel starvation: drive below 1 L with electronics armed - `FUEL
   STARVATION` + stronger cutting.
4. HUD audit recheck in-car: no leftover debug strings from any tab.

## 6. Preservation

Before-state (copy-only):
`workspace_organization_audit/ELECTRONICS_HUD_BEFORE_2026-09-28/` - engine
source + build, panel, tests, product README, the subsystem design doc;
`RESTORE.md` has per-file SHA256. `electronics_failure.c/.dll` are new files.

## 7. Wasted-effort check

- Repeated reading of large material: none; the engine commentary/HUD code
  and the queue consumer were read once, in targeted windows.
- Unnecessarily broad builds/tests: none; two one-shot rebuilds (engine
  twice - once after HUD audit, once with oil leak) as designed.
- Failed/retried commands: one multi-substitution script reported a single
  non-matching placeholder (intentional no-op); one cutout state-machine
  bug (manual burst could turn periodic / never end) was caught in review
  before any build and fixed in one pass.
- Polling/waiting: none; live validation deferred because the game is
  closed.
