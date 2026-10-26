# F1 Challenge input and force-feedback static/runtime map — 2026-08-30

## Scope and boundary

This investigation maps the F1 Challenge executable's input and force-feedback
(FFB) paths from the canonical raw Hex-Rays corpus. It performed no game-memory,
executable, configuration, or installation writes. The running-game check was
limited to the SDK's read-only module snapshot command; Windows denied the
process open because this terminal was not running at the game's integrity
level.

Evidence tiers in this record:

- **FACT** — directly supported by raw decompilation or a pre-existing
  canonical finding explicitly cited below.
- **INFERENCE** — a likely role derived from control flow, names, or layout;
  it must not be used as a write target or stable SDK ABI.
- **OPEN** — not established by current evidence.

## Executive map

```text
HWInput_Setup (0x455C60)
  -> DirectInput8Create
  -> DirectXDeviceInitialization x 2 (0x44FA60)
  -> per-frame sub_4537B0 (0x4537B0)
       -> Force_Feedback_Joystick_Input_Processing (0x44F150)
       -> per-device axes/buttons and input-action queue
  -> VehicleMainPhysicsUpdate (0x4441F0)
       -> Vehicle_Input_Smoothing (0x454DA0)
       -> VehiclePhysicsTimeStep (0x4C73F0)

MainGameTickUpdate (0x42C690)
  -> Driver_Input_Controller (0x451720): FFB device/effect updates

HWInput_Restart (0x455DA0)
  -> sub_44FF50 (0x44FF50)
       -> IDirectInputDevice8::GetObjectInfo / CreateEffect
       -> IDirectInputEffect::Download / Start
  -> Driver_Input_Controller actuator on/off command path

TireTemperatureSimStep -> sub_4EF2E0 (0x4EF2E0)
  -> legacy-named compact state/event-record builder (0x4E2760)
  -> 432-byte event-ring entry update only when changed / time threshold met
```

The two last arrows establish an important boundary: the recovered
legacy-named `ForceFeedback` function builds an 18-byte state/event record,
which `sub_4EF2E0` places in a 432-byte event-ring entry. The present evidence
does **not** locate a bridge from that record to `Driver_Input_Controller`, an
`IDirectInputEffect`, or any physical wheel-torque call. It must therefore not
be described as proven physical FFB output.

## FACT — DirectInput lifecycle and device polling

### Setup, registration and teardown

- `HWInput_Setup` at **0x00455C60**, called by `Game_Setup`, calls
  `DirectInput8Create(hinst, 0x800, IID_IDirectInput8A, ppvOut, 0)`. On success
  it invokes `DirectXDeviceInitialization` twice at `ppvOut + 866` and
  `ppvOut + 1121`; it destroys and reinitializes those subobjects once before
  its global-state initialization. This is a real DirectInput8A creation path,
  not a guessed library association.
- `DirectXDeviceInitialization` at **0x0044FA60** enumerates callback class
  `20` through the DirectInput interface, creates/configures one device from
  the enumerated data, calls its data-format/cooperative-level style methods,
  sets a 44-byte property descriptor, and finishes through `D3DTextureStage`.
  The exact method names and meaning of callback class `20` remain OPEN because
  the recovered interfaces are untyped.
- `HWInput_Enter` (**0x00455BF0**) registers the named actions and optionally
  calls `DualSourceAcquireWithFallback` when `byte_7D13A0 == 1`.
- `HWInput_Post` (**0x00455E70**) and `HWInput_Exit` (**0x00455F00**) destroy
  the two input subobjects; both release the root DirectInput interface at
  vtable offset `+0x8` and clear its pointer. `HWInput_Exit` additionally
  invokes `ModuleUnload` under the same `byte_7D13A0` condition.

### Digital state collection

- `Force_Feedback_Joystick_Input_Processing` (**0x0044F150**; legacy SDK name,
  not a claim that it only handles FFB) is called by the active input update
  `sub_4537B0` (**0x004537B0**) and by a loading-screen path.
- Its input-device object has a type discriminator at `+0x0` and a DirectInput
  device pointer at `+0x4`. When type is `2`, it requests a 256-byte device
  state. On two expected HRESULT failures it calls the device method at vtable
  `+0x1C` and retries once. Other device types read up to 20-byte buffered
  events and merge their high-bit pressed state into the same 256-byte local
  state table.
- The function processes action slots `1..255`, with per-slot repeat/timing
  state at `+0x480`, state bytes around `+0x37C`, and a queued 16-bit action
  list beginning at `+0xC88`; count is at `+0xC84`. This layout is direct code
  evidence, but semantic field names are INFERENCE.
- Held actions are emitted only after the per-action timing gate against
  `dbl_8AFEA8`; the slot's flags govern immediate versus repeat behaviour.
  Some action IDs are additionally ORed with modifier bits from known pressed
  key indices. Holding key index 58 changes a documented set of emitted action
  IDs by setting/clearing bit `0x8000`.

### Named action surface and bindings

`InputActionRegistration` (**0x00455290**) registers this action vocabulary:

- steering right/left; accelerate; brake; shift up/down; neutral; reverse;
  gears 1–7; clutch;
- traction-control override; launch control; LCD mode/up/down/increase/decrease;
  request pit; RPM limiter; brake-bias forward/rear;
- look left/right/behind/up/down/roll-left/roll-right; instant replay; pause;
  restart race; zero head tracker; vehicle labels; ignition; starter; horn;
  headlights; handbrake.

`PlayerOptionsInitializer` (**0x004AA270**) creates persistent binding records
for at least the following primary action IDs:

| Action | Persistent record offset within its options object | Default binding ID |
|---|---:|---:|
| Accelerate | `+0x365C` | 30 |
| Brake | `+0x3678` | 44 |
| Steer left | `+0x3694` | 51 |
| Steer right | `+0x36B0` | 52 |
| Clutch | `+0x3720` | 16 |
| Shift up | `+0x36CC` | 56 |
| Shift down | `+0x36E8` | 184 |

The same initializer creates the later gear, display, pit-request, camera, and
other action records. The exact persistent-file key spelling and the exact
meaning of every numeric default ID are OPEN; the table must not be mistaken
for DIK scan codes.

### Existing canonical live evidence relevant to bindings

- **FACT (pre-existing, not newly retested):** `dword_7CB218` is the pending
  action count and `word_7CB21C[]` holds 16-bit action/scancode values.
  `KeyboardInputActionMapper` (**0x0040BE70**) consumes values from that array
  every frame for camera actions (including 74, 78, 156, 199, 201, 209, 210).
  The corpus finding `keyboard-action-queue-background-input` records prior
  live confirmation. This investigation did not write to that queue.
- **FACT (pre-existing negative result):** the canonical findings record that
  external `SendInput`, `PostMessage`, self-created-IDirectInput-device hooks,
  and `GetKeyboardState` did not reach in-race action handling in the tested
  setup. This does not negate the executable's DirectInput creation/polling
  code above; it rules out those attempted external observation/control paths.

## FACT — analog conversion, dead zones, sensitivity, and rates

### Configuration schema observed in raw code

`PlayerOptionsInitializer` builds a `Controls` section and declares:

| Key family | Default/range established by raw initializer |
|---|---|
| `Keyboard Steering Rate` | default `0.2`, range `0..1` |
| `Keyboard Throttle Rate` | default `0.5`, range `0..1` |
| `Keyboard Brake Rate` | default `0.5`, range `0..1` |
| `Keyboard Clutch Rate` | default `0.2`, range `0..1` |
| `Force Feedback` | integer/boolean default `0`, range `0..1` |
| `Axis [%2.2d, %2.2d] Dead Zone` | default `0.0`, range `0..1` |
| `Axis [%2.2d, %2.2d] Sensitivity` | default `0.5`, range `0..1` |
| `Axis [%2.2d, %2.2d] Center` | default `0.5`, range `0..1`; UI text says `0.0 Min 0.5 Center 1.0` |

The axis key is generated per device/axis. The number of usable configured
device and axis slots is OPEN; do not infer it from the fixed 256-entry digital
table or from FFB's separate channel counts.

### Keyboard/digital ramp path

- `Vehicle_Input_Smoothing` (**0x00454DA0**) is called by
  `VehicleMainPhysicsUpdate` (**0x004441F0**) and uses the elapsed time argument
  plus rate globals `flt_7D12A4`, `flt_7D12C8`, `flt_7D12EC`, and
  `flt_7D1310`.
- It preserves state at offsets `+0x2410`, `+0x2414`, `+0x2418`, `+0x241C`,
  and `+0x2420`, ramps three values toward `0..1`, ramps another bidirectionally
  toward `-1..1`, and clamps each endpoint. A mode value of `2` bypasses ramping
  and copies an alternate raw value.
- **INFERENCE:** these are the digital-to-analog steer/throttle/brake/clutch
  smoothing outputs. Call-site decompilation has a signature disagreement, so
  exact output-to-control ordering is not established. Treat the relationship
  as a processing model, not a field layout for a mod.
- `Steering_input_smoothing_with_saturation` (**0x004569B0**) further adds
  `±0.5 * rate` (or `±2.0 * rate` when mode `1`) to two accumulators. Its calls
  are not recovered, so it is a related helper rather than a proven point on
  the active player path.

### Physics consumption

- `VehiclePhysicsTimeStep` (**0x004C73F0**) decodes a packed control record.
  It converts a steering component, explicitly clamps it to `[-1, +1]`, stores
  it at vehicle `+0x1040`, and multiplies it by `+0x1044` for the front-wheel
  suspension updates. The packet also carries button/flag bits, including
  `0x08000000` and `0x04000000` which drive a sound toggle and a braking-state
  mirror respectively.
- **FACT (SDK-confirmed but not a control lever):** vehicle `+0x3584`
  (`BrakeFactor`) and `+0x357C` (`ThrottleFactor`) exist in the canonical SDK.
  `ParkingBrakeIndicator` at `+0x1065` is explicitly an output recomputed each
  frame from the packed control bit; it must not be used as an input write
  target. No canonical vehicle field currently exposes direct human clutch or
  steering input.
- **OPEN:** exact correspondence among hardware-axis IDs, axis center/dead-zone
  transforms, vehicle raw packet bytes, `ThrottleFactor`, `BrakeFactor`, and
  clutch remains unvalidated. A future passive run needs a known good vehicle
  recipe plus deliberate human pedal/wheel movement in a guarded weekend.

## FACT — force-feedback configuration and legacy-named event-record candidate

### Persistent FFB key families

`PlayerOptionsInitializer` declares:

- `FFB Device Type` (`0..3`), `FFB Effects Level` (`0..5`), `FFB Gain`
  (default `1.0`, `0..1`), `FFB Throttle FX On` and `FFB Brake FX On Steer`;
- steering vibration: frequency, zero/magnitude, slope, waveform;
- steering force: amount, exponent, input and output limits, gain;
- steering friction and damper: coefficient and saturation;
- throttle vibration: engine scaling, magnitude, slope, waveform, update
  amount;
- brake vibration: corresponding frequency/magnitude/slope/waveform/update
  controls;
- rumble-strip: strength, frequency, waveform, wheel/contact scaling, update
  amount; and
- `FFB Jolt Magnitude`.

For each generated joystick/axis pair it also creates:

- `FFB Joy[%2.2d] Axis[%2.2d] Spring Saturation Pos`;
- `FFB Joy[%2.2d] Axis[%2.2d] Spring Coefficient Pos`;
- `FFB Joy[%2.2d] Axis[%2.2d] Spring Saturation Neg`; and
- `FFB Joy[%2.2d] Axis[%2.2d] Spring Coefficient Neg`.

The raw initializer proves configuration support and its bounds/defaults; it
does not prove all keys are consumed on the present executable path.

### Legacy-named state/event record and transmission cadence

- `Driver_Input_Controller` (**0x00451720**) runs from `MainGameTickUpdate`
  when `dword_7CBF24` is nonzero. It drives up to ten controller/effect slots,
  checks player-versus-AI/session state, collision effects, and calls through
  device/effect-object vtables. Its exact effect-index-to-key-family mapping is
  OPEN because the 732-line function's object types are not recovered.
- The legacy-named `ForceFeedback` function (**0x004E2760**) receives an object
  at its base, packs four fixed descriptors into bytes/words at
  `+0x938..+0x946`, and returns the resulting **18-byte state/event record**.
  With enable byte `+0x98C` clear, it zeros the record's mask/value region. With
  it set, it iterates **17** seven-float records, sets a bit for each enabled
  record, saturates three values to `[-511.98001, +511.98001]`, multiplies by
  `64`, and stores signed 16-bit values saturated to `[-32767, +32767]`.
- `sub_4EF2E0` (**0x004EF2E0**) calls that builder from the tire-temperature
  simulation path. It compares the new 18-byte record with the previous one;
  on a change after 0.125–4.0 seconds, or after a longer interval, it inserts
  the record into a **432-byte event-ring entry** through
  `ProgressBarEventEncode`. This proves delta/rate-limited event transport, not
  FFB-device output.
- **OPEN:** the four descriptors may represent wheels/contact state, but that
  is not proven. Likewise, the 17 records may be FFB channels, replicated
  vehicle/contact state, or another encoded event family. The function's legacy
  name is insufficient proof of either semantic claim.

### Hardware endpoint (superseded by the static addendum below)

The first static slice had not located concrete effect creation. The later
addendum supersedes that negative result: `0x0044FF50` proves effect creation,
download, and start, and `0x00451720` proves actuator on/off commands. Effect
update/stop/release and any bridge from the `0x4E2760` record/event ring remain
OPEN. The earlier negative result is retained here as investigation history,
not as the report's current conclusion.

## Runtime observation

### Result

The normal terminal's direct read-only attempt,
`python tools/f1c_mod.py memory-snapshot input_ffb_baseline_20260830 --scope module`,
returned:

```text
ERROR: OpenProcess access denied; run Codex/terminal at the game's integrity level
```

That normal-terminal attempt produced no snapshot and did not read or write
target memory. The SDK `doctor` check still passed, but reported the optional
target as not present from that terminal's access context.

**FACT (coordinator's later user-authorized elevated read-only observation):**
the elevated hidden-PowerShell path successfully produced
`runtime_memory/preferred_systems_elevated_20260830/manifest.json` from PID
`4836`. Its module base was `0x00400000`; it captured `6,266,880` bytes across
`1,530` pages with `0` unreadable pages. The executable path resolved under
`%USERPROFILE%\Documents\F1 Challenge V10`. Elevated read-standings also
reported `RaceActiveFlag=3`, `LapCount=53`, and `VehicleCount=22`.

This establishes that a same-integrity read-only runtime path is available. It
does **not** validate any input field, binding, FFB state/event record, or
physical FFB endpoint: no deliberate control movement or FFB correlation was
performed.

### Runtime-observable candidates (read-only only)

| Candidate | Static anchor | What a future guarded-weekend observation can establish |
|---|---|---|
| Device/object activation | `HWInput_Setup`, `DirectXDeviceInitialization`, `sub_44F150` | whether DirectInput setup succeeds, which path (state vs buffered event) is active, and reacquisition behaviour after focus changes |
| Digital action queue | `0x7CB218`, `0x7CB21C`, `KeyboardInputActionMapper` | action IDs generated by real key presses; observe only, never inject for this investigation |
| Vehicle output fields | player vehicle `+0x357C`, `+0x3584`, `+0x1040`, `+0x1065` | correlation of throttle/brake/steering to human movement; `+0x1065` remains indicator-only |
| Legacy-named event record | `ForceFeedback +0x938`, enable `+0x98C`, caller `sub_4EF2E0` | correlate record changes with wheel/contact and FFB-UI changes without presuming the record is FFB |
| Possible FFB endpoint | `Driver_Input_Controller` effect slots | determine device/effect object identity and start/update/stop call sequence, and prove or disprove a bridge from the event record |

Do not observe a QuickRace. The required live test is a human-driven full race
weekend guarded by `f1cm_guard.asi`, with a same-integrity read-only terminal.

## Open questions and next bounded work

1. Resolve `Driver_Input_Controller`'s effect-object API chain and prove or
   disprove a bridge from the `0x4E2760` event record. Only then classify the
   record as FFB output, another driver protocol, or unrelated contact/event
   state.
2. Recover exact config-file key spelling/default values and the device/axis
   loop bounds from the complete initializer/object metadata.
3. Build a read-only named input recipe only after static validation identifies
   the player input packet and its relocation-safe base. Do not hard-code the
   addresses recorded here into mod code.
4. In a guarded weekend, correlate one control at a time (full left/right,
   throttle, brake, clutch) with packet/vehicle output changes, then repeat
   with FFB toggled in the normal UI. Record raw observations separately from
   this static map.
5. Test whether `FFB Effects Level` affects the 17-record enable mask; do not
   call them FFB channels until a bridge to the physical effect path is proven.

## Evidence commands and source ledger

All repository interrogation used the canonical front door from the repo root:

```text
python tools/f1c_mod.py start --json
python tools/f1c_mod.py doctor
python tools/f1c_mod.py search "DirectInput" --corpus all --context 2 --limit 80
python tools/f1c_mod.py search "Force Feedback" --corpus raw --context 2 --limit 80
python tools/f1c_mod.py search "FFB Joy" --corpus raw --context 1 --limit 40
python tools/f1c_mod.py search "Axis [" --corpus raw --context 3 --limit 80
python tools/f1c_mod.py search "Control Steer" --corpus raw --context 2 --limit 40
python tools/f1c_mod.py inspect 0x00455c60 --corpus raw --full
python tools/f1c_mod.py inspect 0x0044fa60 --corpus raw --full
python tools/f1c_mod.py inspect 0x0044f150 --corpus raw --full
python tools/f1c_mod.py inspect 0x00455290 --corpus raw --full
python tools/f1c_mod.py inspect 0x00454da0 --corpus raw --full
python tools/f1c_mod.py inspect 0x004c73f0 --corpus raw --full
python tools/f1c_mod.py inspect 0x00451720 --corpus raw --full
python tools/f1c_mod.py inspect 0x004e2760 --corpus raw --full
python tools/f1c_mod.py inspect 0x004ef2e0 --corpus raw --full
python tools/f1c_mod.py sdk-query field throttle --limit 50 --json
python tools/f1c_mod.py sdk-query field brake --limit 50 --json
python tools/f1c_mod.py evidence-query findings "input"
python tools/f1c_mod.py memory-snapshot input_ffb_baseline_20260830 --scope module
```

Canonical raw evidence files used: `functions/HWInput_Setup_00455c60.c`,
`functions/DirectXDeviceInitialization_0044fa60.c`,
`functions/HWInput_Enter_00455bf0.c`, `functions/HWInput_Post_00455e70.c`,
`functions/HWInput_Exit_00455f00.c`, `functions/sub_44F150_0044f150.c`,
`functions/sub_4537B0_004537b0.c`, `functions/InputActionRegistration_00455290.c`,
`functions/PlayerOptionsInitializer_004aa270.c`,
`functions/sub_454DA0_00454da0.c`, `functions/VehiclePhysicsTimeStep_004c73f0.c`,
`functions/DriverInputController_00451720.c`,
`functions/ForceFeedback_004e2760.c`, and `functions/sub_4EF2E0_004ef2e0.c`.

## Failures, limitations, and wasted-effort check

- Coordinator review corrected an overstatement in the first draft: raw code
  proves an 18-byte legacy-named state/event record and its insertion into a
  432-byte event-ring entry, not physical FFB output. All four-descriptor and
  17-record semantics are now explicitly OPEN pending a bridge to effect calls.
- Function inspection initially used filename-style identifiers rather than
  `inspect`'s required exact address form; six lookups were corrected to the
  address form before evidence collection continued. This was avoidable by
  reading the command help first.
- A manual decimal-to-hex conversion attempt for several registration helper
  addresses was wrong; no source was changed and the later source search
  supplied the correct `0x0044C710`/`0x0044CE90` addresses. Avoid manual
  conversion; use `search` to resolve named functions.
- Several broad search/inspect outputs were truncated by the terminal. The
  resulting map relies only on the retained raw excerpts and targeted follow-up
  searches listed above; no claim depends on omitted output.
- The normal terminal could not open the game process because of a Windows
  integrity mismatch. A later coordinator-run, user-authorized elevated path
  captured the module successfully, but no input/FFB correlation experiment was
  performed. Neither attempt changed game state.
- No unnecessary build, injection, hook, game-memory write, or game-install
  edit occurred. No repeated polling was performed after the explicit access
  blocker. No other material wasted effort observed.
## Addendum — DirectInput effect lifecycle (static-only continuation)

### Correction to any earlier unresolved-effect wording

**FACT:** This addendum supersedes any earlier statement in this report that effect creation had not been located. `sub_44FF50` (`0x0044FF50`) demonstrably creates and starts DirectInput effects. This does **not** bridge the legacy-named 18-byte record at `0x004E2760`, or the 432-byte ring entry at `0x004EF2E0`, to physical force feedback. Those remain separate event-record/ring candidates.

### FACT — DirectInput interfaces, effects, and actuator lifecycle

`HWInput_Setup` (`0x00455C60`) calls `DirectInput8Create` with version `0x800` and `IID_IDirectInput8A`; the result is checked before setup continues. `DirectXDeviceInitialization` (`0x0044FA60`) enumerates through root vtable `+16`, creates through root `+12`, and configures/acquires the resulting device through `+44`, `+52`, `+12`, `+24`, and `+28`. Under the standard 32-bit DirectInput8 ABI these are enumeration/device creation then `SetDataFormat`, `SetCooperativeLevel`, `GetCapabilities`, `SetProperty`, and `Acquire`. The raw error branches are visible, but the referenced data-format/property structures have not been decoded.

`HWInput_Restart` (`0x00455DA0`) conditionally calls `sub_44FF50(this + 5504)` when `dword_7D1FFC` is nonzero. In that raw function:

- An exact three-iteration role-selection loop (`i = 0..2`) clears selected-role fields `this[314/316/317]`, selectors `this[322/324/325]`, and related signs/states. Its candidate group permutation is `2, 0, 1`; raw code does not name these roles as steering/throttle/brake.
- Device vtable `+56` receives a 316-byte object-instance buffer and object selectors `0, 4, 8, 12, 16, 20`. This is `GetObjectInfo` in the DirectInputDevice8 ABI; 316 matches the 32-bit `DIDEVICEOBJECTINSTANCE` size. Successful instances with returned flag bit 0 set are retained.
- Each prospective effect clears `v23[25]` and invokes device vtable `+72` with a GUID pointer, effect descriptor, output `v23 + 25`, and null outer object. This is the exact `IDirectInputDevice8::CreateEffect` call shape. Negative HRESULT or null output exits that setup branch.
- On success, returned effect `v23[25]` is called at vtable `+40`, then (except effect index 7) at `+28` with `(-1, 0)`. In the standard `IDirectInputEffect` ABI these are `Download` and `Start`; that proves create/download/start. The GUID and type-specific descriptor data remain undecoded, so no effect family (constant, periodic, condition, ramp, custom) is assigned.
- The same setup performs device `+32` then `+28`, resolving to `Unacquire` and `Acquire`; failures prevent normal continuation. `HWInput_Post`/`HWInput_Exit` release the root interface at `+8` after child cleanup, but a per-effect `Release`, `Stop`, or `Unload` path was not found in the inspected chain.

`DriverInputController` (`0x00451720`), reached from `MainGameTickUpdate` when `dword_7CBF24` is nonzero, independently toggles actuators. On `byte_7C56AC` false-to-true transition it visits four guarded slot pointers at `this + 1256`, `+1260`, `+1264`, `+1268`; for each non-null device interface at `slot + 128`, it calls vtable `+88` with `32`. On the true-to-false transition it repeats this with `16`. The ABI resolves `+88` as `SendForceFeedbackCommand`, `32` as `DISFFC_SETACTUATORSON`, and `16` as `DISFFC_SETACTUATORSOFF`. Thus actuator control for up to four associated interfaces is **FACT**.

### FACT — bounds, queues, and controls up to the static limit

| Area | Evidence |
|---|---|
| Digital state | `sub_44F150` processes indices `1..255` from a 256-byte state table. |
| Buffered events | It uses a 1,280-byte local buffer with 20-byte records: at most 64 records per read. |
| Modifiers | Ctrl indices 29/157 add `0x1000`; Alt 56 `0x2000`; Shift 42/54 `0x8000`; NumLock 69 `0x4000`. |
| Action queue | Encoded 16-bit actions append at byte offset `0xC88`; its count is reset at `0xC84`. No explicit pre-append cap is visible in this routine, so capacity is unresolved. |
| Binding defaults | Player option defaults are binding IDs, not proven scan codes: accelerate 30, brake 44, steer-left 51, steer-right 52, shift-up 56, shift-down 184, clutch 16. |
| Axis config | Per-axis dead zone, sensitivity, and centre are declared `[0,1]`; centre description is `0.0 Min 0.5 Center 1.0`. This proves option ranges, not their sampler transform. |
| Keyboard rates | Steering `.2`; throttle/brake `.5`; clutch `.2`, all declared `[0,1]`. |

`Vehicle_Input_Smoothing` (`0x00454DA0`) has five stateful outputs: three unidirectional values clamped `[0,1]`, one bidirectional value clamped `[-1,1]`, and a gear output ranging `-2..7` subject to state flags. In each non-raw branch, delta is `globalRate * deltaTime * 10`; the first unidirectional release decays at `1.5x` normal rate unless an alternate flag is set. This formula/clamp recovery is **FACT**, while the decompiler's disagreement with its `VehicleMainPhysicsUpdate` (`0x004441F0`) call signature makes steering/throttle/brake/clutch naming **INFERENCE** only.

### FACT — packet consumers, intentionally unnamed

`VehiclePhysicsTimeStep` (`0x004C73F0`) converts the scalar at `a2 + 8`, clamps it `[-1,1]`, writes vehicle `+0x1040`, multiplies it by `+0x1044`, and applies it in an exact four-iteration wheel loop only while `v8 < 2`. A front-wheel interpretation is plausible but unnamed in raw code. Verified flag consumers: `a2+52` bit `0x80` updates `+0x1069`; `a2+56` bits `0x08000000`, `0x04000000`, `0x02000000` update `+0x105D`, `+0x1065`/confirmed `ParkingBrakeIndicator +0x3584`, and `+0x1067`; `a2+59` bit 0 updates `+0x4024`. They must not be promoted to named input controls without a concrete producer path.

### INFERENCE / unresolved dataflow

`DriverInputController` also contains a ten-entry loop over 104-byte controller/effect-like records. Its internal `+100` interface is called at vtable `+24`, and separate paths call `+28`/`+32`. The standard `IDirectInputEffect` ABI makes this compatible with `SetParameters`/`Start`/`Stop`, but **INFERENCE only**: no inspected static assignment links that pointer to `sub_44FF50`'s `CreateEffect` output. Therefore update/stop, effect scaling, and effect family remain open. So do the concrete readers and transforms for the declared axis dead-zone/sensitivity/centre keys.

### Static stopping point, evidence, failures, and limitations

- Stopping point: decoding the GUID/DIEFFECT buffers; resolving `CreateEffect` output into the controller's ten records; reconciling smoothing call signature; locating axis-config consumers. These are required before claiming named effect families, scaling, per-control transforms, or a 0x4E2760 FFB link.
- Read-only evidence commands: `python tools/f1c_mod.py search "v23[25]" --corpus raw --context 10 --limit 20`; address-linked raw `inspect`/`search` for `0x0044FF50`, `0x00451720`, `0x00454DA0`, and `0x004C73F0`. No runtime/memory interaction in this continuation.
- Wasted-effort record: broad `v86 + 100` and `k >= 8` searches produced mostly unrelated/truncated output and support no positive claim. Two commands using `--context 12` failed because the supported maximum is 10. Future work should use address-specific inspection or unique call expressions.
