# Pit failure tuning: outcome chart and implementation handoff

Status: PROPOSED, 2026-09-21. This task produces a plan only. No probability,
source, DLL, running game or release setting was changed. The implementing LLM
must work in the canonical development product and preserve the working build.

## 1. Goal and evidence

Make a pit stop uncertain without making faults routine for the same driver.
Most faults cost a few seconds; some cost positions; terminal wheel damage is
exceptional. Use the refuelling-era setting of F1 Challenge rather than assuming
modern tyre-only stops. The values below are gameplay design proposals, NOT
measured real-world failure frequencies.

Real-world mechanism references:
- [F1: Bottas Monaco wheel-nut retirement](https://www.formula1.com/en/latest/article/bottas-warns-mercedes-need-to-learn-from-the-big-mistake-in-pit-stop-that.5hJfbpyMZ7fBEKDkzzLc4V): a damaged wheel nut prevented removal. This supports irreparable wheel-interface damage as a rare terminal branch, not a blanket rule that a failed pneumatic gun retires a car.
- [F1: Singapore fuel-hose incident](https://www.formula1.com/en/latest/article/do-you-remember-f1s-first-ever-night-race.40aylFbXcPZSJRGiQv5Dgh): premature release with hose attached. Fuel equipment and release failures are distinct from slow pumping; do not create unsafe-release mechanics in this tuning task.
- [F1: refuelling history](https://www.formula1.com/en/latest/article/refuelling-set-to-return-in-2017.2l3aCUxdRhtycev934MHzd): refuelling was banned for 2010 after sixteen years. This reference establishes era, not current rules or event rates.

Read first: root AGENTS/governance, product README/STATUS,
CURRENT_PROBABILITIES_2026-09-21.md, retirement-voice and ai-mechanics reports.
For native work, obey the SDK AGENTS and canonical f1c_mod.py front door.
Read the fire addendum and latest integration cleanup contract before fire work.

## 2. Proposed neutral preset: 96.50% clean, 3.50% trouble

These percentages apply to a stop eligible for tyres, fuel and release. Exactly
one parent incident per service. Terminal outcomes are INCLUDED in parent rates.

| Parent | Chance per stop | Outcomes conditional on that parent |
|---|---:|---|
| Slow wheel change | 2.00% | 85% minor hesitation; 15% awkward change |
| Wheel-gun / nut engagement problem | 0.60% | 80% quick recovery; 19% replacement gun/repeated work; 1% irreparable wheel-interface damage, retire |
| Fuel-rig problem | 0.65% | Ordinary subtype 70% slow, 20% temporarily no flow, 10% interrupted; independently within this parent, 5% selects a recoverable leak/spill fire complication |
| Engine stall | 0.15% | 85% first restart succeeds; 15% difficult restart, eventually succeeds |
| Cross-thread / binding wheel nut | 0.10% | 70% corrected; 26% lengthy rework; 4% irreparable, retire |
| Clean | 96.50% | Native service only |

Do not retain the old independent 0.01% terminal bucket ON TOP of these rates.
Terminal mass is wheelgun0.60%*1%=0.006%, plus cross-thread0.10%*4%=0.004%:
total0.010%=one per10,000 eligible stops on average. These are planned terminal
outcomes, not extra risks rolled at each recovery attempt.

Fuel fire selects at0.65%*5%=0.0325% per fully eligible stop, about1 per3,077.
It is part of a fuel fault, not an additional parent incident. Preserve the
addendum's recoverable-only fire target: no fire DNF or general engine failure.
Low flow itself does not ignite: describe a leak/spill at the coupling as the
complication. Unsupported fire suppresses to its already-drawn ordinary fuel
subtype, with a reason. Never reroll for a different failure.

Keep FL/FR/RL/RR equal25% within wheel incidents. No unsupported front-left bias.
The parent grouping is presentation: a gun swap by itself cannot retire a car;
the terminal branch must explicitly represent damaged nut/hub engagement.

## 3. Duration and event chart

All triples are triangular(min, mode, max), in seconds, with continuous draws.
Recovery durations are TOTAL extra affected-job time, not fresh additions for
every retry. The mode is the density peak, not a guaranteed duration.

| Parent / outcome | Conditional chance | Duration triple | Visible sequence |
|---|---:|---|---|
| Slow: hesitation | 85% | (0.8,1.5,3) | Fault, complete |
| Slow: awkward change | 15% | (3,4,6) | Fault, complete |
| Wheelgun: quick fix | 80% | (3,5,8) | Fault, complete |
| Wheelgun: backup/repeated work | 19% | (8,12,20) | Fault, one intermediate attempt, complete |
| Wheelgun: terminal damage | 1% | (12,18,30) diagnosis | Fault, diagnosis, actual retirement, confirmed garage, retirement radio |
| Fuel: slow | 70% | (3,5,9) | Fuel fault, recovery |
| Fuel: temporarily no flow | 20% | (6,9,16) | Fuel fault, one attempt, recovery |
| Fuel: interrupted | 10% | (4,7,12) | Fuel fault, one attempt, recovery |
| Stall: first restart | 85% | (2,3,5) | Engine stops, restart succeeds |
| Stall: difficult restart | 15% | (5,8,15) | Engine stops, unsuccessful attempt, restart succeeds |
| Cross-thread: corrected | 70% | (6,10,15) | Fault, recovery |
| Cross-thread: lengthy rework | 26% | (15,22,35) | Fault, one attempt, recovery |
| Cross-thread: terminal | 4% | (12,18,30) diagnosis | Fault, diagnosis, actual retirement, confirmed garage, retirement radio |
| Fire complication | 5% of fuel faults | burn(4,7,12), inspection(2,4,7) | Fuel callout/HUD, owned suspension, permitted ignition, extinguish/clear, inspect, resume remaining work, release |

Fuel rows are subtype probabilities, and fire is a separate conditional draw;
do not add70+20+10+5 as a subtype distribution. With fire suppressed or absent,
the fuel subtype distribution remains70/20/10. Once fire is realised, use the
provider's remaining-work/clearance deadline, not blind addition of all bands.
Respect the existing bounded acquisition, visibility, fuel and cleanup contract.

For two-stage recovery, schedule the intermediate attempt at half the sampled
recovery duration and final recovery at its end. This is finite choreography,
not repeated coin flips. A long delay must not accidentally double at each cue.
Final branch is fixed at commit; the user learns it through the sequence.

Parallel timing remains mandatory: tyre/fuel faults extend their own job; stop
release is the maximum of all job deadlines. Never force a minimum additional
overall stop time just to make a minor fault visible. Stall adds after normal
work. Current AI shutdown begins at READY: explicitly reconcile the engine
stop/restart timeline with the release-delay timeline before claiming a true
post-service stall. Use known normal-completion timing and the existing owned
hold/identity checks, with mocked and native evidence; do not invent timing if
the stop no longer supports safe engine operations. Preserve the current tested
mechanics until a coherent replacement is verified.

## 4. Context without runaway catastrophe

Keep neutral crew risk100%. Initial proposal: late request+10, late service
change+10, wet+5, poor positioning+15 percentage points of the multiplier.
For this preset clamp combined multiplier to100..140%. A future user-selected
crew preset is separate; do not invent per-team ability ratings now.

Scale RECOVERABLE leaf weights only. Wheelgun terminal0.006% and cross-thread
terminal0.004% remain fixed absolute masses when eligible. Thus at140%, total
trouble=3.49%*1.4+0.01%=4.896%, not3.50%*1.4. Fuel risk scales once; conditional
fire remains5%, not another pressure multiplication. At maximum pressure fire
selection is0.0455%. Terminal conditional percentages therefore fall under
pressure; the base chart's1% and4% apply only at neutral context.

Eligibility masking follows scaling: no tyres removes all wheel leaves including
terminal; no refuelling removes fuel/fire; no valid release removes stall.
Removed mass becomes clean. No redistribution to another failure family.
Neutral tyre-only chance2.85%; fuel-only plus stall0.80%.

Live context remains neutral unless a flag has a verified source and threshold:
late request means provider-confirmed insufficient prep time; late change needs
timestamped job change; wet needs verified wet-surface data; poor positioning
needs actual service-box error, NOT the pit-entry marker. Do not infer these
from race position, driver reputation, elapsed race time or screen text. Report
each as connected, unavailable or disabled. Keep missing inputs unknown/neutral.
Do not build new native providers just to obtain these tuning flags in this pass.

No pity timer, per-race quota, mandatory failure, escalating streak correction or
anti-player bias. Same probabilities for AI/player; report unavailable player
mechanical outcomes separately rather than claiming equal realised behaviour.

## 5. Expected frequency, not a guarantee

At neutral full eligibility and independent draws: one driver's two-stop race
has6.8775% chance of at least one incident. A field with44 eligible stops has
1.54 incidents on average,79.15% chance of at least one, and20.85% of races with
none. This means an individual's faults are occasional even when another car's
pit problem is heard in many races. Tyre-only stops and hidden overlap lower the
visible disruption rate. Terminal probability across44 stops is0.439%; fire
selection1.420%, before capability suppression. A64-stop race would differ;
report varied field sizes rather than claiming a universal per-race rate.

## 6. Implementation instructions for the receiving LLM

1. Preserve the current source/build/active-document before-states and record
   exact hashes. The running retirement-voice build ends in4652F017 and has owner
   audible acceptance. Establish current state afresh: another task may have
   changed files. Work only in development and canonical SDK where necessary.
2. Reuse live/policy_dev.c, live/session_dev.c, the accepted RNG/identity/queue
   contracts, live timing helper, GUI and native SDK mechanics. Do not rewrite
   the GUI, broker, audio player, native retirement, or original alpha mod.
   Keep src/pit_failure_policy.[ch] and src/pit_failure_session.c acceptance
   baselines intact. Put new policy types in a versioned development extension.
3. Add an explicit new schema/preset version; preserve v2 replay interpretation.
   Model parent_cause, resolution, severity, corner, fuel_subtype, selected_fire,
   duration and schedule separately. A parent does not determine terminality.
   Retain the terminal_wheel GUI shortcut as an explicit forced terminal test;
   remove its independent normal-play mass in the NEW preset only.
4. Use exact integer probability arithmetic with sufficiently fine resolution
   (e.g.100,000,000 tickets and64-bit intermediates), and the existing unbiased
   RNG-below helper. Compile parent*resolution leaf weights before context:
   neutral wheelgun leaves0.48%,0.114%,0.006%; cross-thread0.07%,0.026%,0.004%.
   Select exactly one leaf or clean. Do not round terminal branches to whole bp
   or pick a parent and reapply unchanged terminal odds after risk scaling.
   Document deterministic rounding for configurable percentages, total sum and
   clean remainder; reject invalid/overflowing weights or duration order.
5. Keep RNG domains explicit and independent for incident selection, corner,
   fuel subtype, duration, fire and fire timing. Seed plus immutable epoch/car/
   service identity freezes the plan once; polling and narration never reroll.
   Preserve original RNG mapping for v2. For new normal play replace the current
   hard-coded seed with a session seed obtained once from an OS source and
   recorded; explicit supplied replay seed wins. Injection reload must not
   silently restart a known game-session sequence. If stable resume state is
   unavailable, retain explicit fixed-seed testing and mark normal seeding pending.
6. Derive finite event schedules from the frozen resolution. Preserve copy-out
   plans, cancel-purge, tombstone retention, bounded queues, clock invalidation
   and x87 precision restoration. Native retirement only at the terminal stage;
   announcement237 once after authoritative DNF/garage, preserving the owner-
   tested subject and20-second queue lifetime. Never enqueue Go on terminal.
7. Use existing voice catalogue and mappings. No extra generic call over every
   retry. Minor recovery uses existing applicable audio; named AI fault then
   eventual retirement if needed. Fuel problems do not get an engine-blown
   retirement line. Do not invent fire-specific speech. Preserve alpha limiter,
   Brake, Go and exit cues with their existing validity gates.
8. Extend the existing test panel minimally: normal parent selection retains
   randomized severity; an optional advanced resolution selector can force each
   branch for deterministic tests. Existing driver/failure/OK flow stays simple.
   Force changes ONLY selection, not timing/eligibility/native effects. Display
   unavailable fire/player-terminal branches honestly; keep reserve-before-pit-
   call ordering, request dedup, expiry and identity checks.
9. Keep normal configuration separate from overrides. Generate the embedded
   configuration from the versioned preset with a hash/version exposed in status;
   test that editing an INI cannot misleadingly appear to update an already
   loaded DLL. Avoid maintaining silently divergent copies.

## 7. Scope and validation gates

Deliver working severity tuning first. Fire ownership/fuel control remains a
separate extension: reuse research and existing fire lab only through the current
reviewed SDK contract. A legacy shared emitter is not authorisation to transplant
its thread, player engine banks, global descriptor restore or native RNG effects.
If prerequisites are absent, ship the development candidate with explicit fire
suppression and list that blocker; do not block ordinary tuning on new research.
No new unsafe release, wheel detachment, injuries, penalties or mechanical fire
damage. No unsolicited changes to other mods.

Required focused checks, before any gameplay claim:
- Exact compiled totals and all leaf interval boundaries; clean remainder;
  eligibility masking; neutral/full-pressure terminal mass100 per million.
- Each forced branch through the actual adapter, correct named subject/corner,
  delayed retirement, no early/duplicate retirement announcement, and release.
- Job-overlap fixtures including completely hidden slow tyre delay; stall
  timeline coherence; finite retries; no compounding duration per event.
- Seed determinism, v2 unchanged fixtures, forced versus normal schedule parity,
  arbitrary polling schedules, interleaved cars, cancellation, replacement,
  clock faults, queue saturation and preserved x87 environment.
- At least1M simulated stops across predeclared fixed seeds for parent/leaf/fire
  rates; separate analytic exact-weight checks for rare terminal correctness.
  Predeclare a conservative statistical tolerance (e.g.6 binomial standard
  deviations plus rounding allowance), no favourable seed searches/retry-until-
  pass. Report aggregate and per-seed counts, selected versus realised/suppressed,
  duration quantiles and added-stop quantiles. Empirical rates are not exact math.
- Simulated season summaries for24/44/66 eligible stops per race: clean races,
  incidents per driver, long stops, DNF and fires. Distinguish job-time severity
  from actual added-stop time. Do not tune solely against the all-cars average.
- Current build/test commands and canonical SDK gate for SDK/native changes.
  Preserve generated outputs before rebuild. Run workspace validator; known
  eight-error baseline is not a new failure or permission to fix unrelated files.

Then present one review package: changed paths/hashes, source/config diff,
probability and branch tables, tests, known limits, restoration procedure and
short human test checklist. Do not auto-load or retire additional live drivers
under this draft alone. Once authorised, test isolated AI/player cases, then
normal stochastic races; audible/visual judgement belongs to Matias. Preserve
prior accepted mechanics, scope failures clearly, and stop for review at the
candidate rather than publishing a release or claiming100% complete gameplay.

## Plan-task documentation gate

This draft is indexed in the product README. Current implementation is unchanged;
the proposed reduction, branching, modifiers and seed lifecycle are not enabled.
No new test suite/build was run for a prose-only plan. Workspace validator retains
the known eight errors/zero warnings. Before-state of the edited README is in the
dated PIT_TUNING_PLAN_BEFORE audit batch. Wasted-effort check: none material;
source audit and fire addendum were reused, searches bounded, no polling.
