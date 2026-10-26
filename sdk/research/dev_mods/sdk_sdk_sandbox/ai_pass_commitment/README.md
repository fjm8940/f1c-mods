# AI Pass Commitment

This is the development-only home for making AI drivers commit to safe passing
moves in traffic without flattening their `RaceAbility` differences. It is
non-production and defaults to **observation-only**. The first bounded
intervention now exists behind `ObserveOnly=0`: it changes only follow-mode
pass-pressure retention from vanilla's 85 percent to the configured value
(default 95 percent). It does not touch lane modes, speed ceilings, throttle,
brake, steering or the executable on disk.

## Evidence and current decision

The 2026-09-02 Hockenheim capture showed that Barrichello's apparently
decisive pursuit was not caused by a more aggressive profile. He had one
nearby candidate, while Burti had all five candidate entries populated.
Burti's effective TrackAggression was higher (`0.853660` versus `0.768294`),
and neither driver emitted a nonzero transient target or lateral directive in
the 30-second comparison. Both drivers lifted through the normal racing-line
throttle command.

The intended fix therefore belongs in dense-traffic lane selection and pass
commitment, not in a mass RCD rewrite or a post-planner full-throttle override.
`ThrottleFactor` is rewritten every frame and must not become the control
surface.

## Staged implementation contract

### Phase 1 — identify the rejected decision (completed)

- **Completed:** the two machine-code call sites prove that `Overtaking` is a
  `__thiscall` on the candidate car with the evaluating AI car and lane mode as
  its two stack arguments. See `ABI_EVIDENCE.md`.
- Capture candidate pointer, candidate-to-mode index, mode, relative geometry,
  planner throttle/brake, transient target, lateral directive, pit state,
  damage state, race phase and yellow/replay state on one clock.
- Name a rejection gate only after repeated live events and static control flow
  agree.

### Phase 2 — shadow decisions (implemented; live run pending)

- Run the proposed relaxed decision beside vanilla without writing game state.
- Log `would_commit` with the exact reason, candidate, side and safety gates.
- Require zero shadow decisions during braking, pit service/entry, replay,
  yellow/safety-car control, invalid vehicle state or an occupied side.

`shadow_policy.hpp` contains the fail-closed decision skeleton and compile-time
checks. A reviewed `AIMainUpdate(F1CVehicle*)` hook now snapshots all five lane
modes before vanilla runs and checks the result afterward. In observe-only mode
it counts every transition where 95-percent retention would differ from the
vanilla result without writing state.

The causal speed-ceiling-to-pedal field is now identified at vehicle `+0x4484`
and exposed read-only through the historically named `trafficSpeedCap`
telemetry recipe. `AIMainUpdate` resets it to a high sentinel before planning;
traffic branches in `AISteering` can lower it; `AISteeringCorrection` also
folds in the independent max-speed restriction at `+0x4538`, then suppresses
throttle or adds braking according to the resulting deficit. A `+0x4484`
sample alone therefore proves the pedal cause but not whether traffic or the
separate restriction supplied the minimum. Both fields must be captured on
one clock. They remain safety-critical and are not declared writable.

The focused 30-second live trace captured 152 cap transitions for Barrichello
and 134 for Burti. Of the finite-cap transition frames, Barrichello had 51 and
Burti had 40 with a speed deficit over 10, which is the vanilla full-brake
band. Burti's maximum observed deficit was `31.729`, versus `21.579` for
Barrichello. These are transition-event counts, not time-weighted durations.
`shadow_policy.hpp` now classifies the exact vanilla ceiling-response bands at
compile time; it still performs no runtime reads or writes.

A corrected 15-second same-clock capture included `+0x4538`. That restriction
remained at its `99999` sentinel while all 675 active `+0x4484` transitions
were lower, isolating `AISteering` rather than the external restriction as the
source in that green-race window. Burti produced 16 active traffic ceilings,
all below the restriction, with a maximum speed deficit of `21.05`; his
closest recorded candidate was roughly 23–30 distance units ahead while its
mapped lane state remained mode 16 (follow).

Static review also identified the per-lane pressure/history member at
`+0x42DC + 28*lane`. Eligible nearby closing traffic adds
`max(1,-candidateMotion) * TrackAggressionEffective`, distant or ineligible
traffic subtracts 5, the value is capped at 450, and a transition into mode 16
retains 85 percent. The new read-only `steeringLanePressure0..4` recipes and
pure compile-time reconstruction expose this mechanism for validation. This
is a useful diagnostic surface, but the first live distribution shows it is
not sufficient by itself: 413 of 911 mode-16 pressure-transition events were
already at or above 300, while all 141 mode-1 events were above 300. High
pressure contributes to a pass decision but can still be vetoed by the
left/right clearance and collision-time scores. Changing the 0.85 retention or
its decision thresholds is not authorized until those scores can be observed.

Eight consecutive Burti ceiling transitions matched one reviewed mode-16
formula exactly: `ceiling = ownSpeed + candidateMotion - 10`. The candidate
was 21.34–23.70 distance units ahead, and the resulting deficits remained in
the full-brake band. Across the capture, 121 active transitions matched this
formula for one of the five candidates within 0.05. A counterfactual reduction
of the `10` margin to `5` would cut full-brake classifications in those matched
events from 113 to 49, which is far too large a behavioral change to apply
without contact-rate evidence. That constant is therefore a shadow-analysis
target, not a current patch.

### Phase 3 — guarded intervention (built; not live-validated)

- The first implementation adjusts only the affected lane's pressure after a
  new transition into mode 16, scaling the vanilla result by
  `configuredRetention / 85` and capping it at 450.
- It runs inside the reviewed AI game-thread boundary and only when
  `RaceActiveFlag != 0`, `RacePhase == 5`, `AIControlFlag == 1`, the car is not
  retired, `PitStatus == 0`, and `CallToBox == 0`.
- Preserve vanilla braking and collision-time protection.
- Add short commitment hysteresis so a safe selected side is not reconsidered
  every frame.
- Never force final throttle and never modify the original executable.

The hook target is protected by the verified six-byte entry signature
`83 EC 1C 53 55 56`; mismatch refuses installation. Unload restores the entry
bytes before releasing the trampoline and waits for in-flight calls. The five
pressure fields were promoted into the canonical SDK only after static and live
evidence agreed, so the mod contains no new raw offsets.

The SDK load dry-run succeeds, including PID validation and the dedicated
32-bit injector. The actual observe-only load is currently blocked because the
guarded loader stages only into the governed workspace game path
`%USERPROFILE%\Desktop\F1\F1 Challenge V10`, which is absent, while the live
installation is under Documents. No fake workspace game directory was created
and the guarded loader was not bypassed.

## Acceptance criteria

- A faster follower in dense traffic attempts a legal side more often than the
  unmodified control run.
- Completed pit-free passes increase without a material increase in contacts,
  spins, missed braking points or yellow-flag violations.
- Driver pace ordering remains controlled by the existing car and
  `RaceAbility` model.
- Clean unload restores entirely vanilla behavior.

## Abort conditions

Abort intervention on any unresolved ABI, stale object/session handle,
unsupported executable, missing broker capability, unsafe session state,
occupied passing side, pit/damage/replay/yellow state, or measurable collision
regression.

Primary evidence is indexed in
`research/game_mechanics/09-complete-game/LIVE_2002_AGGRESSION_RELIABILITY_AUDIT_2026-09-02.md`.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/ai_pass_commitment
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_AI_PASS_COMMITMENT_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/ai_pass_commitment C:\path\to\staging`, which
refuses unowned overwrites.
