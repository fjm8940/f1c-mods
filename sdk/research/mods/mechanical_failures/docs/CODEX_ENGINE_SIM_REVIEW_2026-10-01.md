# Independent engine simulation review - 2026-10-01

## Summary

**FIX FIRST. Do not promote the reviewed DLL.**
Hook removal and session teardown have unresolved crash paths.
Compatibility refusal is incomplete; the model has a reproduced fairness defect.
The 26 ABUSE runs without onset are reproduced; several die in seconds, not minutes.
Offline passes do not establish live safety or release acceptance.

## Scope, authority and artifact identity

Read-only review requested by `CODEX_ENGINE_SIM_REVIEW_PROMPT.txt`.
Product: mechanical_failures, canonical development candidate.
Pipeline gate: verification, followed by pending human gameplay validation.
The portfolio's broader investigate status is not a release approval.
No product stage was changed.

Root and development instructions, component README/current handoff,
governance orientation documents and SDK operating contract were consulted.
The initial workspace validator passed with 0 errors and 0 warnings.
SDK `start --json` reported `freshness.stale = false`.
The existing handoff and review prompt name this report, making it discoverable
without editing the component index.

Primary evidence is the frozen source used for the requested `db1932ec` build.
All source line references below refer to that snapshot, not later edits.
Paths in the findings table are relative to this component unless marked SDK.

| Reviewed item | SHA-256 |
|---|---|
| DLL, both scratch builds | `db1932ec9ef556962d7e5879951619448a968c81ffca9eed6a7f4d452eefa0d3` |
| engine_failure.c | `aca136866cd2c621bf99940b091706ad40c7b0b0f4babb4c2ce61e8df8ff5a3c` |
| mf_engine_model.h | `c7f4ee7bcf4155570ca723bbb2b236456a8b215eca98972e3320b98f3ab89242` |

The development and installed DLL matched the scratch builds when captured.
Another actor changed development source and installed another build during
this review. At the later hash check, development and installed DLL both read
`75609896f143a9b69eeace7afd6c1c7d7e928bef135dbb05765606491a502bc6`.
The later engine source hash was
`bad1207e4ced7fddf003673f0fd2d989f24460c47401ac38a7413ecd1064f1bc`.
A source diff showed F8 disabled and state publication added after a key toggle.
Those changes are acknowledged under E; the newer DLL was not rebuilt or
independently accepted here. This report must not be treated as its full review.

Evidence tiers: FACT = direct source observation or executed result;
DERIVED = consequence of that evidence; INFERENCE = unverified runtime outcome.
CRASH labels identify plausible crash paths, not crashes reproduced in this task.

## Findings

| ID | Severity | File:line | Problem and evidence tier | Smallest justified fix |
|---|---|---|---|---|
| R01 | CRASH | dll/engine_failure.c:2577, 2602; dll/mechanical_failures.c:88 | Worker installs/restores a six-byte game instruction without quiescing execution. Exit counts family workers, then waits 50 ms; it does not count in-flight game hooks. Restore failure still clears installed state and allows DLL unload. FACT code; DERIVED execution into partial/unmapped code. | Patch at a proven game-thread safe point; verify hook ownership and successful restore, drain in-flight calls. Keep DLL resident if safe removal cannot be proved. |
| R02 | CRASH | dll/engine_failure.c:1362, 2188, 2444, 5816 | Unload calls raw lever/FX restores without SESSION_OVER protection. FX identity is only table pointer/count-derived, not allocation generation. Queued particle calls have no session-generation check in the hook. FACT; stale renderer/descriptor access is INFERENCE. | Tag saved state and bursts with session/object generation; discard after invalidation. Perform bounded writable-range validation and restoration at the game safe point. |
| R03 | CRASH | dll/engine_failure.c:2559, 5659; dll/mechanical_failures.c:186 | A failed detour code check logs and returns void; the worker still activates raw memory pumps. There is no aggregate executable code/layout refusal before fixed-address use. FACT; wrong executable crash/corruption is DERIVED risk. | Make compatibility validation a prerequisite to activation. Return failure before starting any family or touching game memory; test refusal. |
| R04 | WRONG | dll/mf_engine_model.h:1886, 2025 | Terminal checks precede the fairness clamp. A scratch probe stopped at t=1.01 with fair_end=60.00, then health was raised back to 0.02. FACT executed counterexample. | Gate every model terminal before latching it, or apply the floor before terminal decisions; retain explicit driver-error exceptions. |
| R05 | WRONG | dll/mf_engine_model.h:1190, 1309 | Drain above 20 s/s is discarded, while normal onset also needs 60 s accepted warm-up and 10 s confirmation. Reproduced 26 ABUSE CLOCK deaths with no onset. FACT sweep; DERIVED cause. | Add a narrowly validated short-horizon onset path, described under D. Raising the cap alone is insufficient. |
| R06 | WRONG | dll/engine_failure.c:4598, 4646 | Save/valve rev-band writes look like a car change. The model re-derives from the mod's lowered limiter, treating it as stock. FACT data flow; DERIVED altered normalization and thresholds. | Derive from captured stock limiter while its lever is owned; detect car changes using identity and untouched parameters. |
| R07 | WRONG | dll/engine_failure.c:5476, 5508, 4110 | SESSION_OVER returns before clearing g_model_live. Clock rollback only skips a tick. stint_clear clears family mirrors but leaves model onset/health and power/dead/save state. FACT; carry-over on an unsampled transition is DERIVED. | Centralize session/stint invalidation before the sentinel return. Use session generation/clock rollback, not only sampled race phase. Define which real failures persist. |
| R08 | WRONG | dll/engine_failure.c:877, 888 | In db1932ec, the control block is published before the F8 change. Next tick reads the old value as a panel request and reverses the key toggle when the block exists. FACT deterministic data flow. | Publish after key handling. Concurrent source already adds that publication and disables F8; retest the exact new DLL. |
| R09 | WRONG | dll/engine_failure.c:893, 4164 | Saved repair lifetime is captured once per DLL load and never invalidated for a new car/session. Repair can restore another engine's lifetime. FACT code; DERIVED behavior. | Bind the saved value to engine/session generation; invalidate it on change and recapture from the current valid engine. |
| R10 | WRONG | dll/mf_engine_model.h:1675, 1877, 1925 | Save's x0.6 and quality/grade growth scaling are applied through clock_drive. Gasket thermal/cascade and exhaust crack growth bypass it. FACT; blanket 'fault growth x0.6' is not implemented. | Apply one common intended growth scale to these progression paths, or explicitly specify which physical wear remains exempt before changing calibration. |
| R11 | RISK | dll/engine_failure.c:5073, 5125, 5186, 5360 | Shared AI tables avoid double scaling but still weaken healthy sharers. Restores lack per-value ownership/generation. Retirement returns before release; session reset discards saved table identity; off/on retains done/shared model state. FACT; persistence/overwrite depends on game reuse (INFERENCE). | Skip power writes for shared tables, or use a per-vehicle lever. Restore only owned current-generation values; reset enable state coherently. |
| R12 | RISK | dll/engine_failure.c:1349, 373; dll/mf_hud.h:69 | Worker writes race physics/HUD/radio fields while the game and other families use them. Volatile, pointer equality and IsBadReadPtr do not synchronize lifetimes or multi-field/64-bit updates. FACT; inconsistent/torn state is INFERENCE. | Submit coherent commands/snapshots to a reviewed game-thread adapter; keep the pure model on the worker. |
| R13 | RISK | dll/mf_engine_model.h:1099, 1124; dll/engine_failure.c:4595 | Nonfinite values are not rejected consistently. Scratch grade input infinity produced G0=NaN; mf_clamp(NaN) remains NaN. Raw-valid tests accept infinity. FACT probe/source. | Reject nonfinite raw inputs/scores before derive/step/write; preserve documented finite fallbacks and refuse unsafe outputs. |
| R14 | RISK | tools/engine_model_test.c:1945; dll/engine_failure.c:2503, 2519 | Niggle 5% check covers only a noise-square formula. The live hook also runs random discrete cuts at depth 0.3; their combined loss is not covered by the asserted bound. FACT omission; exceeding bound is DERIVED under the test's own noise assumptions. | Exempt harmless niggles from discrete cuts, or test and cap actual combined hook loss over the required window. |
| R15 | RISK | dll/engine_failure.c:5176, 5445 | Round-robin visits 8 AI slots per tick, but accumulated dt >10 s is cleared and discarded. A large accepted player tick can lose AI time. The 2 ms trigger is not a hard execution budget. FACT; gameplay slowdown is INFERENCE. | Consume accumulated time in bounded chunks and retain remainder; measure worst-case native scheduling before claiming a time bound. |
| R16 | RISK | tools/mech_failure_test_gui.py:479 | Current 111 fields match, but attach checks magic rather than advertised ABI/struct_size. ABI 13 remained unchanged as fields grew. FACT; old-client compatibility is unverified. | Require enough advertised bytes for each optional field, validate supported ABI, and add a real mapping compatibility test. |
| R17 | CLEANUP | dll/mf_engine_model.h:1563, 1573 | Exhaust-valve and head temperature states feed diagnostics but no fault choice, wear, or driver lever; this does not meet the owner's state rule by itself. FACT use search. | Remove the two states/updates or explicitly connect each to an approved visible/felt effect. Preserve ABI offsets if diagnostics are retired. |

## A. Game memory safety

**DERIVED: current guards do not establish safe write ownership.**
No game was launched, sampled or written by this review.
No crash or save corruption was reproduced. The inspected engine paths do
not write a save/profile file directly; consequences of damaged live state
being saved remain untested.

| Write family | Existing protection | Remaining gap |
|---|---|---|
| Player rev limit, rev-band thresholds/shift settings | Captured values and ownership comparisons | Generation, finite ordering, atomic multi-field commit, self-rederive (R06) |
| Radiator/heat-transfer and related double levers | MfLever saved/last/held | Raw dereference at restore; equal value is not proof of same engine; teardown and physics races |
| Oil temperature, fuel, life, stress, banks, ignition and retirement state | Many pumps guard SESSION_OVER/live drive; some clamps | Guard can change after check; manual terminal/repair paths remain raw; doubles can be unaligned and are not a synchronized snapshot |
| Player torque | Native output hook, player self check and model clamps | Installation/removal and in-flight lifetime (R01); hook reads mutable model mirrors |
| AI torque columns and timer | Slot loop 1..23, table count bounded, readable-range check, current pointer/count restore match | No roster/generation/writeability proof, no per-value restore ownership, shared effects (R11) |
| Suspension fields on repair/rear-break | Bounded four-wheel loop and fixed known offsets | Native wheel lifetime and repair scope need live verification; this review does not infer a field type merely from its name |
| Particle descriptor/renderer, queued effects | Descriptor count bound; queue critical section; FX table key | Queue synchronization protects the queue, not game allocations; renderer lifetime/ownership and post-session delivery (R02) |
| HUD and commentary queue | Short bounded text and fixed HUD slots; live-drive checks | Pointer lifetime, family contention and multi-field queue transactions (R12) |

Restores are not all double-write bugs: MfLever drops held after a restore,
FX clears saved, and AI drops table ownership. Their problem is restoring the
wrong generation or racing a replacement. AI's session-end guard avoids a
write but can strand modified values if the game retains the same table.
Skipping a stale restore is safer than writing old values into a new engine.

The default dead path returns zero from the native output hook after calling
the trampoline, preserving its side effects and keeping ignition on.
Legacy manual cutout/repair paths still write ignition; the ignition rule
must be tested separately for model deaths and manual test commands.

## B. Compatibility and fixed-address audit

**FACT: host proof exists; comprehensive target-layout refusal does not.**
The protected export checks host size, magic, ABI, product, capability,
generation, PID, seal and challenge/proof before activation.
DllMain is otherwise inert unless `mechanical_failures_dev.ini` enables
AllowInjector. That explicit development escape must be excluded from release
packaging; the reviewed development path is not 'host only'.

The engine's six-byte prologue test is a local hook check, not the required
whole dependency/layout gate. Failure continues activation (R03).
No whole-file EXE hash gate is proposed.

The SDK front door was used for prior findings, inspect, and global queries.
The literal-address inventory covers the engine, host, linked gear/electronics,
SDK suspension source, and the audio/HUD/exit/variance helpers.
It includes historical comments and disabled branches to avoid hiding leads.
79 unique address literals were queried; 18 code/interior addresses were
also inspected. The complete inventory appears in the appendix.
Named globals and function nodes are navigation evidence, not runtime layout
validation. Missing names and conflicting broad labels were not promoted to
proof. Object-relative fields were cross-checked against the prior findings
and decompiled engine/vehicle/HUD functions.

Native 0x447B10 reads the object's ignition/fuel state and returns the output
multiplier. The no-stack-argument thiscall body is consistent with the shim's
ECX self and unused EDX. This is DERIVED static ABI compatibility, not a
reviewed runtime callable contract: the SDK node remains LEGACY.
ParticleTrailEmitter's seven stack arguments are consistent with the adapter;
its decompiled integer return is ignored by the DLL's void typedef.
That observation does not prove the particle object's thread/lifetime contract.

Missing or mismatched code/layout must refuse *all* families before fixed
memory accesses. Family DllMain also reports TRUE regardless of CreateThread
success; started-worker counting is therefore not verified resource ownership.

## C. Model math against the paper and addenda

**FACT:** the supplied model suite passes 67,306 checks. On finite tested
inputs, the exponential EMA `1-exp(-dt/60)` implements the stated time constant.
Life increases and oversized drops are excluded from the ordinary estimate.
Usage-weighted fault choice, early hazard gating, triangular quality,
per-engine RNG and the grade score/median/clamps are present.
State is deterministic for identical explicit seed and input sequence;
live seeding and hook-local misfire RNG do not provide cross-run replay.

**FACT:** the finite real-file grade tests pass 306 checks. Grade score is
`0.7 ln(E) + 0.3 ln(max(E-Var,0.25E))`, normalized to the field median and
clamped to 0.80..1.20. Runtime grade is deliberately 1.0.
The sweep prints computed grades but setup_car does not assign them into
eng.grade; its behavior run also uses G=1.0. It does not prove graded gameplay.

Model time is seconds, RPM inputs rad/s, thermal state kelvin,
fuel/volume flows litres per second, and native life drain seconds per second.
The sweep's end column is seconds (D). The caller converts acceleration into
g before the model; that convention must be kept when replacing an adapter.

Most ordinary denominators have floors. Those floors do not sanitize NaN/inf
(R13), nor prove that the game supplied a coherent tuple.
The fixed-step model and stock thermal stand-in are not a native physics test.

The E9g-g near-zero classification uses prior game drain to distinguish a
real clock end from a garage reset. It fixes the false-survival classification,
but does not repair earlier drain warm-up/onset (D).

The fairness clamp is too late (R04). Executed probe: manually applied
coolant fault, h_g=0.02, water=465 K, a 100-second onset TTF, then a 10 ms step.
Output: `stop=1 blown=0 health_g=0.02000 t=1.01 fair_end=60.00`.
This deliberately isolates ordering; it is not a measured live water trajectory.

Save mode: the rev-band pump uses the intended x0.97 factor, choosing the
lower valve/save ceiling. Early random hazard uses x0.5. Clock-driven
progression uses x0.6, but other growth bypasses it (R10). Existing leakage
and physical consumption may reasonably persist after stabilization; a claim
that every damage process halts or scales needs a more precise specification.

Niggles never directly change life/health. Disabling them stops new hazard
rolls but does not immediately clear an active kind/duration; misfire/sensor
output can continue until expiry. Specify whether that is intended.
The 5% assertion assumes noise mean-square 1/3 and omits discrete cuts.
Under that same assumption, depth 0.3 gives ~10% noise loss while active.
One cut/s with mean 0.08 s and strength 0.45 adds loss during cuts even when
combined by max rather than addition. A five-second maximum event can
therefore exceed the asserted ten-second average. This is a DERIVED estimate;
actual native noise/timing integration was not measured here.

## D. The 26 ABUSE runs without onset

**FACT: reproduced unchanged, with a units correction.**
All 145 files were parsed again and the original scratch sweep rerun.
There are 26 ABUSE CLOCK rows with onset=-1:

- 20 2004RH/2005RH files: clock end 7..14 **seconds**.
- Four 1994 Ford HB files: 161, 161, 161 and 202 **seconds**.
- Root 94renaultv10: 112 seconds.
- Pacific Ilmor: 322 seconds.

The supplied prompt and the latest CAR_SWEEP prose say minutes. That is
incorrect: run_to_end returns s->t, sim_tick advances it in seconds, and the
three-hour cap is 10,800. Existing documentation was left unchanged as requested.

**DERIVED root cause:** onset already *is* tied to projected time_to_fail.
The guess that it is not tied to that projection is disproved by the code.
The projection is unavailable or late because of its eligibility gates:

1. Rate samples above 20 s/s never enter the ordinary EMA/warm-up.
   Positive native exponential wear can exceed 20 without being a reset.
2. Onset needs 60 s of accepted on-track/running samples, then 10 s continuously
   inside its lead window. A 7..14 s lifetime cannot satisfy those gates.
3. The EMA starts at zero and can underestimate rapidly increasing drain.
   Skipped high samples and confirmation delay also matter in longer runs.
4. E9g-g now recognizes those final near-zero events as clock deaths; it does
   not create an earlier warning or an onset. Correct classification exposes
   the gap that earlier false resets hid.

ABUSE holds 1.05 times the limiter on straights, not exactly the limiter.
The printed drain_lim is only an at-limiter reference, not its full trace.
For a 2004 car whose limiter is 750 RPM above its base with a 150 RPM range,
the RPM term alone is `0.5*2^5 = 16` at the limiter. At 1.05 limiter it is
far larger. Fast exhaustion and the rejected samples are consistent with
the observed seconds. Raising the 20 cap alone still cannot satisfy 60+10 s.

**Smallest fix sketch; not implemented or tested here:**

- Keep the existing EMA/lead/confirmation path for ordinary finite traces.
- After validating engine/session identity and positive native life, retain a
  separate recent valid drain estimate, corroborated by adjacent samples and
  the known native law. Do not call every large decrease a reseed.
- If that evidence predicts exhaustion before the ordinary path can warm or
  confirm, allow one emergency onset through the existing fault-roll/anchor
  mechanism. Bypass only the warm/confirm delay for that imminent positive-life
  case. Anchor TTF/fairness to the short remaining budget; never modify life
  or re-arm an engine that is already at zero.
- Reject session reloads, repair/reseed increases, collision deaths and
  nonfinite/unreadable snapshots. No new fixed address is needed for the
  mathematical branch; runtime identity still needs the compatibility work.
- A warning-only version is even smaller if owner intent is solely audible
  notice. It will not make sweep onset=-1 disappear, and must not be sold as
  satisfying that acceptance criterion.

**Expected target, INFERENCE:** 26 -> 0 eligible ABUSE CLOCK rows without
onset, with warning timing limited by the available seconds. A scheduled
radio delay of 5..15 s cannot guarantee audible warning before a 7 s death;
log onset alone is not evidence of a useful warning. Use an immediate
appropriate symptom for the short-budget case, subject to human validation.

**RACE/CASUAL invariance is not proved.** No patched sweep was run because
the request prohibits source edits. The baseline itself has one RACE CLOCK
without onset: 94renaultv10 at 638 s. A general valid high-drain rescue can
change that case too. Runtime has no RACE/ABUSE profile label. Identical input
histories must receive identical handling; an unconditional promise to repair
all such traces while preserving every RACE trace is not justified.
CASUAL has no CLOCK row without onset in this run. Require a per-file,
per-seed comparison of onset/fault/severity/terminal/time and RNG consumption
for unchanged ordinary traces. Record any changed RACE case explicitly.

## E. Lifecycle, dead state, save key and control ABI

**FACT:** normal sampled live-entry resets model, save and dead latch.
Repair resets the model and game channels. Normal pumps often check the
session sentinel before touching fields. That is not a complete state machine.

Session time zero is used as SESSION_OVER. A return there cannot itself
invalidate model state; if a complete leave/re-enter transition falls between
worker samples, g_model_live remains true. A backward clock becomes an unsigned
large dt and is skipped, without a model reset (R07).
Car change detection compares only limiter/RPM base; equal-valued different
cars, torque-table replacement and other changed physics inputs are missed.

Non-race pit/garage stint clearing releases mirrors and owned levers but does
not clear the corresponding model fault. The next pump can republish it.
Conversely, a real race failure should not be healed just because the car
enters pits. Resolve that distinction with explicit engine/stint identity.
The pure E9g-g clock-reset tests do not test this DLL transition sequence.

Dead latch is effective in the hooked player output path. Rev-band restoration
on stint exit and native re-arm interactions still need a live trace proving
that a retired model engine never recovers torque except by approved repair
or a new engine. Repair's lifetime snapshot is load-global (R09).

In db1932ec, F8 uses high-bit key state, a rising edge, and foreground PID
matching. This avoids ordinary hold-repeat and unfocused toggles, but consuming
the edge while unfocused means a held key does not toggle on refocus.
The publish ordering makes the original key state unstable (R08).
Concurrent source explicitly records that F8 is the game's traction-control
key, sets MF_SAVE_KEY=0 and adds post-key publication. Current behavior is
panel-only. No new key is selected by this review.

Control layout: a static size assertion compiled, and a separate parser
compared all C fields with ENG_F: **111 C fields, 111 Python fields,
444 bytes, zero offset mismatches**. Save mode is slot 110; the appended
niggle/AI switches occupy slots 106..109. This verifies present field order,
not concurrent command delivery or backward-compatible attachment (R16).

## F. AI shadow engines

**FACT:** off by default, test-mode activation, 23 non-player slots.
Current table bounds prevent a simple local saved-array overrun for accepted
counts. One owner per shared table prevents compounded scaling/restoration.
It does not isolate the failure to that owner: every vehicle reading the
shared table sees the changed power column.

Pointer/count matching at restore is stronger than writing a cached pointer
blindly. It still accepts reused allocations and overwrites another writer's
values. IsBadReadPtr is a read check, not a writable/lifetime lock.
Retired/done slots stop updates before releasing prior table changes.
ai_session_reset drops table state without a guarded release; off/on does
not reset done/shared fault state. Whether the native game reloads each
affected table must be verified instead of assumed.

Native-law mode uses relative lifetime and thermal fallbacks, not independently
proven per-car AI engine data. No team address is guessed. Grade remains 1.0.
Timer mode supplies its known drain explicitly; it does not exercise the
player's high-rate slope filter in the same way.

Cost: full pass measures elapsed time and switches to 8-car round-robin after
exceeding 2 ms. Model dt up to 10 s can require about 1,000 fixed substeps
per processed car. Round-robin is a workload reduction, not a 2 ms guarantee.
Accumulated dt above the model limit is lost (R15). No native tick/frame
timing or full-grid CTRL+T cost was captured here.

## G. Simplicity under the owner's state rule

Most model state contributes to symptoms, wear, cascades or usage-based fault
selection. Temperature/health/fuel diagnostic fields should not be counted as
driver effects merely because the developer panel can display them.
t_ev_k and t_head_k have updates and panel telemetry but no such downstream
effect found in the reviewed model (R17).

The duplicated oil-pressure and valve-pressure diagnostics can diverge from
the model if expressions change. Prefer returning those diagnostics from the
same evaluation when next touching that interface; this is not a reason to
rewrite the working model during a safety fix.
Disabled MF_LEGACY_* paths and disabled pit-top-up support carry historical
complexity. No crash claim follows from their presence. Remove them only in
a separate preserved cleanup with behavior comparison, not during this review.

## H. Verification executed and gaps

All builds and sweep outputs were in TEMP. No canonical DLL was overwritten
by this reviewer. No GUI or game was launched; no game memory was written.

| Command/check | Result |
|---|---|
| powershell -NoProfile -ExecutionPolicy Bypass -File tools/workspace/check-workspace-structure.ps1 | Initial PASS, 0 errors, 0 warnings; final result recorded below |
| SDK python tools/f1c_mod.py start --json | Freshness not stale |
| SDK evidence-query findings, inspect, evidence-query global | Prior art consulted; 79 literal-address audit, 18 code/interior inspections |
| tools/run_engine_model_test.bat, from tools | ALL PASS, 67,306 checks |
| python -B -m unittest test_mech_failure_test_gui | 20 tests, OK |
| python -B mech_test_panel.py --smoke-test | PASS |
| Two scratch gcc builds using merged batch flags | Exact matching db1932ec hash above; -O2 -Wall -Wextra -Werror, deterministic link timestamp option |
| Scratch python parse.py, gcc sweep.c, sweep.exe, python stats.py | 145 engine files; reproduced aggregate and 26 missing ABUSE onsets |
| Scratch gcc grade_test.c, grade_test.exe | ALL PASS, 306 checks |
| Scratch gcc review_probe.c, review_probe.exe | Fairness counterexample; nonfinite grade/clamp counterexamples |
| C field parser vs Python ENG_F | 111/111 fields, 444 bytes, no mismatches |

Build used C:/msys64/mingw32/bin/gcc.exe, family DllMain renames and MF_MERGED=1.
Copied batch-relative SDK paths do not resolve from TEMP; the scratch build
used the canonical SDK include and suspension source as read-only absolute
inputs. Link libraries: kernel32 and winmm; --no-insert-timestamp.

Scratch directories:

- %USERPROFILE%/AppData/Local/Temp/codex_engine_review_build_m3v03kw6
- %USERPROFILE%/AppData/Local/Temp/codex_engine_review_sweep_hrna4z23

The 92 mod-file rows: GENTLE all survived; RACE 6 survived/85 model terminal/
1 CLOCK; CASUAL 26 survived/2 model terminal/64 CLOCK; ABUSE 0 survived/
56 model terminal/36 CLOCK, of which 26 without onset.
The 50 'vanilla-era' folder rows: GENTLE all survived; RACE 5 survived/45
model terminal; CASUAL 5 survived/6 model terminal/39 CLOCK; ABUSE 5 survived/
45 model terminal. Three separately grouped safety-car rows all survived.
Folder groups include non-race car files; they are not the live session roster.
Manual oil-pressure test terminates all 145 rows.

The tests omit native allocation teardown, refusal paths, hook patch atomicity,
unload while inside a hook, real race/garage transitions, HUD/radio contention,
cross-process ABI compatibility, and keyboard/game binding interactions.
The sweep uses a synthetic thermal/driver loop, average initial life, fixed
seeds, and early/niggle defaults different from ordinary live activation.
It does not prove native warning audio, full-grid performance or quality/grade
distribution over real sampled lifetimes. Model check count is not coverage
of these integration properties.

### Extra live checks to add to BIG_LIVE_TEST (list only)

1. After safe-point/refusal fixes, reject wrong code patterns/layout, absent
   host proof and release-packaged injector escape; assert zero game writes.
2. Exercise unload during active torque/particle calls, including forced
   restore failure and another hook owner. Confirm pin/refusal, not timed unload.
3. Repeat restart, ESC/menu, garage and session changes with live model/dead/save
   state; include clock rollback and a transition shorter than one worker tick.
4. Compare stock car constants before/during/after save and valve ceiling.
   No CARDATA rederive may adopt the mod's limited rev band as stock.
5. Trigger short-budget RH clock deaths after the onset fix; record onset,
   actual dash/audio symptom time, native stage-one and final death separately.
6. Test repair after changing car/session; record current and restored life,
   suspension scope, ignition and dead latch. No old-engine lifetime may return.
7. Use a shared-table AI pair: damage one, finish/retire it, disable/re-enable,
   change session and unload. Check the healthy sharer's table/power and every
   owned restored value. Test noPlayer/full-grid configurations.
8. Measure AI/game tick cost at normal speed and CTRL+T; include long accepted
   dt and retained backlog. Record frame timing, not just model runtime.
9. Verify panel save toggling on the exact newer DLL; if a key is later chosen,
   test hold/refocus and existing game bindings. F8 is currently disabled.
10. Measure a worst-duration niggle's actual combined torque over ten seconds;
    test disabling an already active niggle and agree immediate-vs-expiry behavior.

### Open questions for Matias

1. Should a non-race garage return replace the model engine, while real race
   pit stops retain damage? Define that boundary for R07.
2. Must emergency onset preserve every RACE result, including the existing
   94renault no-onset death, or may it correct that known case too?
3. Is save x0.6 intended for every damage progression, including thermal
   gasket and crack growth, or only clock-driven wear?
4. Should shared AI torque-table power effects be skipped until a per-car
   adapter exists, preserving reliability-only TEST behavior?
5. Should disabling niggles clear an active symptom immediately, or let its
   bounded duration finish?

### Limitations, preservation and follow-up

No source fixes were authorized by this review prompt. Fix sketches are
recommendations, not implemented results. Live validation remains pending.
Workspace writes by this reviewer are this new report and appended SDK
findings. No existing source/doc/INI was edited and no files were deleted.
Restoration: no runtime state or canonical artifact was changed by this
reviewer, so no rollback is required. Scratch evidence is disposable under
retention policy; nothing was purged. Concurrent edits belong to their actor.

Prioritize R01-R04 and the compatibility/identity foundation before attempting
production validation. Then implement D as a separately defined change,
compare exact per-file seeded traces, and run owner live gates against one
immutable candidate hash. Do not infer promotion authority from this report.

Wasted-effort review:

- Some initial combined document/prior-art reads were truncated. Targeted
  reads followed; repeating the full large engine result was unnecessary.
- Required model/GUI/sweep checks and two requested deterministic builds were
  run once per intended purpose; no game builds/copies were created in workspace.
- The global query audit initially used unsupported --json, then hit Windows
  subprocess decoding. It was corrected with the supported command and explicit
  UTF-8. A scratch-probe command had a quoting failure; apply_patch corrected it.
  One TEMP source read used the wrong nested path. These failures produced no
  workspace changes and are not counted as successful checks.
- Waits were bounded completion polls for running builds/audits; there was no
  idle gameplay polling or repeated unchanged live sampling.

## Fixed-address audit appendix

Generated from the reviewed linked inputs. All entries were queried through
the SDK global front door; code/interior addresses below 0x500000 were also
inspected. Comments/disabled code can introduce unused literals here.
The table is an evidence inventory, not an acceptance whitelist.

<!-- Address inventory and documentation gate appended by the reviewer. -->

| Address | SDK result (not runtime proof) | First source |
|---|---|---|
| 0x0042C690 | inspect: MainGameTickUpdate; static lead | SDK suspension_failure.c:67 |
| 0x0043EF20 | inspect: EngineStressSimulation; static lead | engine_failure.c:283 |
| 0x00442060 | inspect: SteeringGeometryUpdate; static lead | SDK suspension_failure.c:196 |
| 0x00443B70 | inspect: Gearbox; static lead | engine_failure.c:2498 |
| 0x004441F0 | inspect: VehicleMainPhysicsUpdate; static lead | gear_failure.c:25 |
| 0x00447AB0 | inspect: TrigTableLookup; static lead | engine_failure.c:2387 |
| 0x00447B10 | inspect: RandomPhysicsParameterGenerator; static lead | engine_failure.c:1647 |
| 0x00447B16 | Interior address: inspect has no standalone function node; needs containing-code validation | engine_failure.c:2384 |
| 0x00447E00 | inspect: PositionLapStateUpdate; static lead | gear_failure.c:22 |
| 0x00447E05 | Interior address: inspect has no standalone function node; needs containing-code validation | gear_failure.c:59 |
| 0x00447E5F | Interior address: inspect has no standalone function node; needs containing-code validation | gear_failure.c:21 |
| 0x004A0FD0 | inspect: RestartGate; static lead | SDK suspension_failure.c:67 |
| 0x004CBC50 | inspect: ParticleTrailEmitter; static lead | engine_failure.c:338 |
| 0x004E4C4F | Interior address: inspect has no standalone function node; needs containing-code validation | gear_failure.c:11 |
| 0x004EC1A0 | inspect: CollisionSparkParticleEffects; static lead | engine_failure.c:1955 |
| 0x004ED960 | inspect: FuelConsumptionAndWear; static lead | engine_failure.c:11 |
| 0x004F88A0 | inspect: SoundEffectTrigger; static lead | gear_failure.c:126 |
| 0x004F89A0 | inspect: SoundEffectTrigger_89a0; static lead | gear_failure.c:126 |
| 0x0055EF14 | (not in name_index) | engine_failure.c:1644 |
| 0x005BF9A0 | CommentaryEventTable   [DERIVED] | engine_failure.c:348 |
| 0x005EA618 | CommentaryQueueController   [CONFIRMED] | engine_failure.c:347 |
| 0x005EC6D8 | VehicleArrayBase   [FACT] | engine_failure.c:301 |
| 0x005EC6E4 | SeasonScoringDBBase   [DERIVED] | engine_failure.c:15 |
| 0x007C56A4 | SessionType   [FACT] | engine_failure.c:521 |
| 0x007C56A8 | RacePhase   [CONFIRMED] | engine_failure.c:522 |
| 0x007C7428 | PlayerPhysicsObject   [FACT] | engine_failure.c:21 |
| 0x007C7430 | PoseInterpDuration   [DERIVED] | engine_failure.c:55 |
| 0x007C75C4 | GlobalSpeedTime (double)   [DERIVED] | engine_failure.c:3913 |
| 0x007C75D4 | (not in name_index) | engine_failure.c:264 |
| 0x007C75DC | (not in name_index) | engine_failure.c:265 |
| 0x007C8C50 | (not in name_index) | engine_failure.c:3438 |
| 0x007C8C58 | (not in name_index) | engine_failure.c:3439 |
| 0x007C9078 | (not in name_index) | engine_failure.c:277 |
| 0x007C9081 | EngineRunningIgnitionFlag   [FACT] | engine_failure.c:601 |
| 0x007C9084 | CameraTransformDouble   [DERIVED] | engine_failure.c:579 |
| 0x007C9094 | (not in name_index) | engine_failure.c:261 |
| 0x007C90A4 | (not in name_index) | engine_failure.c:274 |
| 0x007C90AC | (not in name_index) | engine_failure.c:262 |
| 0x007C90B4 | (not in name_index) | engine_failure.c:263 |
| 0x007C90BC | (not in name_index) | engine_failure.c:2659 |
| 0x007C90C4 | (not in name_index) | engine_failure.c:582 |
| 0x007C9104 | (not in name_index) | engine_failure.c:1618 |
| 0x007C910C | (not in name_index) | engine_failure.c:2657 |
| 0x007C9114 | (not in name_index) | engine_failure.c:2658 |
| 0x007C9124 | (not in name_index) | engine_failure.c:254 |
| 0x007C9130 | (not in name_index) | engine_failure.c:255 |
| 0x007C9174 | ParamBaseValueA   [DERIVED] | engine_failure.c:1616 |
| 0x007C917C | (not in name_index) | engine_failure.c:1617 |
| 0x007C9184 | (not in name_index) | engine_failure.c:272 |
| 0x007C918C | (not in name_index) | engine_failure.c:1389 |
| 0x007C9194 | (not in name_index) | engine_failure.c:273 |
| 0x007C919C | ParamBaseValue   [DERIVED] | engine_failure.c:278 |
| 0x007C91A4 | (not in name_index) | engine_failure.c:266 |
| 0x007C91AC | (not in name_index) | engine_failure.c:267 |
| 0x007C91B4 | ParameterBaseCase1   [DERIVED] | engine_failure.c:3416 |
| 0x007C91BC | (not in name_index) | engine_failure.c:3417 |
| 0x007C91C4 | (not in name_index) | engine_failure.c:3417 |
| 0x007C91D4 | EngineStressRate   [FACT] | engine_failure.c:25 |
| 0x007C91DC | EngineLifeSeconds   [FACT] | engine_failure.c:24 |
| 0x007C91E4 | (not in name_index) | engine_failure.c:245 |
| 0x007C91EC | (not in name_index) | engine_failure.c:246 |
| 0x007C91F4 | (not in name_index) | engine_failure.c:247 |
| 0x007C91FC | (not in name_index) | engine_failure.c:248 |
| 0x007C920C | EngineDamageBank1_WhiteSmoke   [FACT] | engine_failure.c:26 |
| 0x007C9214 | EngineDamageBank2_BlackSmoke   [FACT] | engine_failure.c:27 |
| 0x007C921C | EngineDamageBank3_Flames   [FACT] | engine_failure.c:28 |
| 0x007C9318 | PlayerCurrentGear   [FACT] | gear_failure.c:11 |
| 0x007C931C | PlayerTargetGear   [FACT] | gear_failure.c:27 |
| 0x007C9320 | PlayerShiftDirection   [FACT] | gear_failure.c:28 |
| 0x007CD204 | HUDMessageStripPtr   [FACT] | engine_failure.c:608 |
| 0x007CE258 | PlayerOutFlag   [DERIVED] | gear_failure.c:123 |
| 0x007CE6F4 | BrakeDiffSettings   [DERIVED] | electronics_failure.c:177 |
| 0x007CEA5C | EngineWearMode (0/2/3)   [DERIVED] | engine_failure.c:526 |
| 0x007D0308 | RaceActiveFlag   [FACT] | gear_failure.c:69 |
| 0x008AAE74 | TrigLookup256   [DERIVED] | engine_failure.c:281 |
| 0x008ACEA0 | ScoringGridBase   [DERIVED] | engine_failure.c:326 |
| 0x008ACF8C | TotalRaceLaps   [CONFIRMED] | engine_failure.c:530 |
| 0x008AFEA0 | SessionTime   [DERIVED] | engine_failure.c:332 |
| 0x008AFEF0 | AmbientTempC   [CONFIRMED] | engine_failure.c:249 |

Conflicting global labels such as 0x007C9084 and broadly named parameter
slots do not supersede reviewed object offsets. Unnamed data is unresolved
in the global name index; prior engine initialization/physics evidence is
needed for its meaning. The runtime layout and refusal tests remain pending.

## Documentation gate

PASS: this new report exists at the requested canonical docs path and is
discoverable through the existing review prompt/current handoff. It separates
FACT, DERIVED and INFERENCE, names the reviewed hashes and concurrent changes,
and lists pending native/human validation. Five uniquely identified findings
were appended to the SDK journal with tier, evidence and source.
The workspace validator after the report/appendix and SDK append passed:
0 errors, 0 warnings. UTF-8 without BOM and edited text were checked.

## Related physics assessment

Owner follow-up: [simulation fidelity, accuracy and missing mechanisms](CODEX_ENGINE_PHYSICS_ASSESSMENT_2026-10-01.md). This is a separate physical-quality assessment, not acceptance of a later DLL build.
