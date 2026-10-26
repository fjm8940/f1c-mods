# Pit Stop Failures: implementation plan and reviewer handoff

Date: 2026-09-21. Recipient: DeepSeek V4.1 Flash or another implementation agent.
Workspace: `%USERPROFILE%\Desktop\F1`.
Product: `development\mods\pitstop_modding` (`pitstop-modding`).
Current state: 0.1.0-port.1 development prototype; runtime adaptation pending.

## 1. Assignment and authority

When the owner instructs you to execute this plan, implement Phases A-F fully.
Deliver working code, reproducible tests and a reviewer package, not another plan.
Phases G-I explain the subsequent runtime work. Do not implement them before the
reviewer accepts the offline work and issues a bounded runtime assignment.

The present request created this plan; it did not authorize installation, game
writes, release or automatically starting another agent. This plan supplies no
additional authority to do those things.

The owner wants tension and occasional botched stops, not strict historical
simulation. Recoverable delays dominate, prolonged failures are less common and
retirements are exceptional. A few understandable circumstances may raise risk.
Player and AI use the same rules. No forced race incident, pity timer, luck-history
modifier, catch-up handicap or hidden quota.

The earlier `docs/PROBABILITY_REVIEW_2026-09-21.md` proposed 1.23% total risk.
The owner found that too conservative. Do NOT use it as the new default. The
subsequent direction was 4-6% recoverable trouble per normal stop with bounded
contextual variation. Exact defaults below are provisional implementation choices,
not measured historical statistics or final owner-approved gameplay balance.

Success here means a compiled pure C policy, validated configuration, simulated
service adapter and replay harness using the same C code, passing verification,
and clear documentation. It does not mean the live DLL is modernized or safe.

## 2. Cold start and preservation

Before edits read root AGENTS.md, README.md, governance/README.md, PIPELINE.md,
PRODUCT_STATUS.json, development/AGENTS.md, nearest scoped instructions, component
README.md and STATUS.md. Then read verification/port-2026-09-21/REPORT.md and its
JSON manifests. Use focused symbol searches rather than broad corpus rereads.

Run from workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1
```

Known baseline: eight errors, comprising unexpected root .pytest_cache and trials,
missing F1 Challenge V10_clean reported twice, and four source-like files under
production/bug_report_site versions 1.0.0/1.0.1. Capture a fresh baseline. Do not
repair unrelated issues or say the validator passes. Investigate new differences.

Nothing is deleted/reset/cleaned/discarded. No new top-level folders. Never read or
copy api1.txt, private keys or credentials. Do not edit installations, production,
archive, third_party, research originals or compatibility-hub files. Never execute
legacy GUI/injection/memory-write tools. Do not alter unrelated working changes.

Before changing an existing file, preserve its complete before-state in a unique
`workspace_organization_audit/PITSTOP_POLICY_BEFORE_<date-time>/` tree mirroring
its workspace-relative path. Include source, build scripts and maintained docs.
Record hashes and restoration mapping. Check collisions before writes. All
build/test outputs get fresh product-local trial directories; retain failures.
Restoration must preserve the then-current state before copying back. No deletion
of newly created work as a rollback. Git is optional; explicit diffs/hashes suffice.

SDK work, if later needed, first reads `f1c_ida_decompiled/AGENTS.md` and uses its
front door from that repository root: `python tools/f1c_mod.py start --json`.
Use route/search/inspect/help there; do not invoke internal SDK scripts directly.
Phases A-F should require no SDK changes or broad sdk-test run.

## 3. Existing code and scope

Active legacy source: `src/pit_commentary.c`. Preserve it unchanged during A-F.
`dll/` contains historical source/binaries. `build.ps1` builds the legacy x86 DLL
with WinMM into unique trials. `config/pit_failures.ini` is a preserved game-tuning
snapshot. `tools/legacy/pit_failure_gui.py` is protocol evidence only.

Existing defects: player risk is 11.4%/stop; AI uses separate sequential rolls
totalling about 28.752%; uniform fuel subtype selection includes spills; recovery
can add terminal outcomes beyond the nominal terminal probability; config lacks
validation; audio and failure selection share RNG. Do not replicate these.

Runtime defects are separate: fixed-address writes, worker-thread game calls,
global fuel-rate changes, player-slot assumptions, direct retirement-state writes,
unowned vehicle scratch queue, session cleanup and unverified AI pit observations.
Do not hook the new policy to those mechanisms and declare the runtime repaired.

## 4. Deliverables

Use C11, fixed-width types and no game/OS APIs in policy code. Suggested layout:

| File | Responsibility |
|---|---|
| src/pit_failure_policy.h/.c | Validated config, risk weights, deterministic selection, immutable outcome plans, delay math |
| src/pit_failure_session.h/.c | Service identity, once-only lifecycle and simulated semantic commands |
| tools/pit_failure_sim.c | CLI simulation/replay calling the same policy modules |
| config/pit_failure_policy_v2.ini | New versioned preset; legacy INI untouched |
| tests/ | Boundary, malformed-config, deterministic replay and distribution checks |
| build-policy.ps1, test-policy.ps1 | Commands callable from any cwd; paths from PSScriptRoot; fresh output directories |
| docs/POLICY_V2.md | Actual public API/config semantics and integration gaps |
| verification/policy-v2-<timestamp>/ | Results, hashes, commands, diff and IMPLEMENTATION_REPORT.md |

A simpler equivalent layout is allowed if documented. Avoid frameworks, packages,
services and unnecessary dependencies. Python standard-library orchestration is
fine, but tests must exercise compiled C; a separate Python reimplementation is
not evidence that C works. Preserve the existing legacy build path. Do not add a
dormant live adapter behind a flag. Use version 0.2.0-policy.1 unless occupied.
Artifacts:; live integration explicitly not implemented.

## 5. Exact probability semantics

Use basis points: 10000 bp = 100%; 1 bp = 0.01%. Never silently mix per-mille,
percentage and relative-weight units. Use checked wide arithmetic.

Default normal prepared stop with all work eligible:

| Outcome, in bucket order | bp | Probability |
|---|---:|---:|
| slow_corner | 280 | 2.80% |
| wheelgun, recoverable | 100 | 1.00% |
| fuel_rig, recoverable | 90 | 0.90% |
| stall_delay, recoverable | 20 | 0.20% |
| cross_thread, recoverable | 10 | 0.10% |
| terminal_wheel | 1 | 0.01% |
| clean | 9499 | 94.99% |

Recoverable sum is 500 bp; total is 501 bp. Two independent fully eligible stops
give `1-(1-.0501)^2 = 9.768999%` chance of any fault. With 44 stops across the
field, some minor trouble occurs in most races. Explain this distinction: the
revised tension target gives frequent field incidents but mostly clean races for
one driver. Do not quietly restore the previous rare-field preset.

Terminal probability is the ENTIRE terminal budget. Recoverable outcomes never
receive an additional retirement roll. Preselect cause and recoverability once;
retry events describe the selected outcome rather than rolling extra disasters.

### Eligibility and selection

Snapshot committed service work once. Tyre categories require tyre work; fuel
requires refuelling allowed plus positive requested addition. Stall requires real
service and a release/restart opportunity. Terminal_wheel requires tyre work.
Drive-through, stop-go and no-work visits must not roll tyre/fuel problems.

Unknown eligibility excludes the affected category. Zero its absolute mass and
leave that probability clean. Never redistribute excluded mass or reroll until a
valid fault appears. Optional unknown context is neutral, not a reason to exclude
otherwise verified service. A decision requires a known stable service identity.

Compute weights, apply eligibility, then draw ONE unbiased integer in [0,9999].
Choose the first cumulative bucket strictly greater than the draw. Unclaimed
values are clean. Subtype/corner/duration draws afterward are allowed. Test exact
edges, zero-weight categories and the clean interval.

### Contextual risk

Inputs include explicit validity flags. No telemetry acquisition in A-F.

```text
multiplier_percent = clamp(crew_risk_percent
    + late_request_bonus + late_change_bonus + wet_bonus + positioning_bonus,
    60, 240)
```

Defaults: crew risk 100; true late request +15; true late service change +15;
true wet condition +10; true poor positioning +25. False/unknown flags add zero.
Crew risk may be configured 80..120 per team; absent team settings =100. Do not
invent actual team ratings or randomize a crew rating each race.

Bonuses are additive to the multiplier, NOT percentage points added to risk.
All flags/default crew: multiplier 165%, recoverable total 825 bp. Crew120 plus
all flags: 185%, total 925 bp. Terminal remains 1 bp regardless of pressure.

Scale recoverable categories BEFORE eligibility. Target scaled sum is
round-half-up(base_sum*multiplier/100). Floor individual results; distribute
remaining bp by descending fractional remainder, ties resolved by table order.
If total exceeds recoverable cap (default 1200 bp), apportion the cap across the
scaled categories with the same largest-remainder rule. Terminal is separate.
Never clamp upward: zero risk stays zero and masked categories stay clean.

Validate custom recoverable base sum <=1200 bp, terminal <=10 bp, cap in 1..1200,
crew 80..120, context bonuses 0..100, nonnegative individual weights, and combined
effective probability <=10000. Use overflow-safe intermediates. These configuration
bounds are intentional v2 limits; terminal default remains 1, not 10 bp.

Late request might eventually mean <5 seconds preparation; late change could use
that window. These are provisional adapter definitions, not facts to infer from
legacy fields. Poor-position and wet measurements require fair player/AI evidence.
Until reviewed, unavailable factors remain neutral with an explicit reason.

## 6. Corners, subtypes, severity and timing

Choose a corner after selecting a wheel fault. Equal default weights 25/25/25/25;
nonnegative weights with positive checked total. Zero weight means never selected.
No unsupported front-left bias or automatic rear-corner time penalty.

Fuel conditional weights: slow_flow 70, temporary_no_flow 20, interrupted_flow 10.
These are relative subtype weights, not per-stop percentages. No spill/fire in v2.
All three recover; actual fuel manipulation belongs to later reviewed runtime work.

Provisional triangular extra JOB time in seconds:

| Outcome | min / mode / max |
|---|---|
| slow_corner | 1 / 2.5 / 5 |
| wheelgun | 3 / 5 / 10 |
| fuel slow_flow | 3 / 6 / 12 |
| fuel temporary_no_flow | 4 / 8 / 16 |
| fuel interrupted_flow | 3 / 6 / 12 |
| stall_delay | 2 / 4 / 8 |
| cross_thread | 10 / 18 / 35 |
| terminal diagnosis presentation | 8 / 12 / 20 |

Validate finite `0 <= min <= mode <= max <= 60`; equal endpoints legal. Document
floating-point tolerance. No unbounded distribution or infinite terminal timer.

Sample total extra job time once. At most three recovery attempts; default two
for wheelgun/fuel/cross_thread, one for slow_corner/stall. Place attempts evenly
within the sampled interval, final attempt at its end. Recoverable outcomes always
resolve successfully by that bound; no hidden escalation or additional random
duration. Stable event IDs and once-only emission handle repeated/late callbacks.

For supplied valid normal job completion times:

```text
normal_end = max(normal job ends)
fault_end = max(other job ends, affected_job_end + extra_job_time)
added_stop_time = max(0, fault_end - normal_end)
```

Stall occurs after normal work and adds directly. Tyre end6/fuel end10/+2 tyre
trouble =>0 extra stop seconds; +6 =>2 extra. Never add to a job AND again to total
deadline. Clean preserves timing exactly. Unknown baseline timing => unknown added
stop time, not invented values. This blocks real effect application while allowing
synthetic tests. Report selected faults separately from measurably delayed stops.
Do not impose an artificial minimum visible delay to conceal overlap.

## 7. RNG and service lifecycle

Choose a small documented deterministic PRNG with known-answer vectors, fixed-width
integer arithmetic and stable seed mixing. No cryptography needed. Use rejection
sampling or equivalent unbiased bounded selection, not plain modulo. Support all
uint64 seeds including zero; reject malformed/overflowed seeds. Simulator saves
the explicit seed before sampling; if auto-generated, report it. No time/libc rand
inside policy. Never hash raw padded C structs; document encoding/domain tags.

Derive streams from seed + session epoch + stable vehicle identity + service token
and domain. Separate category, subtype, duration and cosmetic domains. Adding audio
draws, changing car processing order, or inserting another car's stop must not
change the original stop. Player/AI label must not enter the randomization key.

Identity is `(epoch, vehicle ID, service token)`. At committed service, freeze
configuration/context and decide once. Duplicate notification returns the same
plan. A cancelled service cannot reuse its token for a new stop. Unknown identity
returns explicit no-decision. Do not rely solely on legacy box-state edges.

States: idle, service-planned, resolving, resolved, cancelled, terminal-requested.
Terminal-requested emits a semantic request only, no process writes. New epoch
clears state; old callbacks ignored. Pause freezes progression; resume excludes
wall-clock pause duration. Invalid/backwards simulation time cancels and requires
an explicit reset/new epoch, never negative deadlines or rerolls. Premature service
exit cancels outstanding effects. Each fake command is tagged and emitted once.

## 8. Configuration contract

New filename and schema_version=2; explicit *_bp keys and duration units. Do not
overwrite legacy INI or silently parse its per-mille values as basis points.
Reject duplicate keys/sections, unknown keys, numeric trailing junk, NaN/Inf,
overflow, negative weights, missing schema marker and invalid duration ordering.
Document required keys and optional defaults. Validate into temporary state;
publish atomically only after complete success. Errors name key/line and produce
nonzero simulator exit. No partial configuration or silent fallback on malformed
input. A future live adapter would disable the feature on invalid configuration.

Provide a dry-run legacy compatibility report: identify old units, unsupported
keys and obsolete recovery/escalation semantics. Never automatically write a
converted preset or claim lossless conversion. Recommend explicit v2 selection.

## 9. Execute Phases A-F

**A — Orient/preserve:** capture instructions, baseline hashes/validator, inspect
picker/recovery/seed symbols and write a short execution checkpoint naming files.
Do not scan the whole corpus. Unexpected provenance changes require investigation.

**B — Pure policy/config:** implement the API, strict parser, validation, exact
weight calculation and selection. Test boundaries before adding lifecycle state.
Compile with strict warnings. Keep error handling explicit and portable.

**C — Outcome plans/session:** implement domains, corners, fuel subtypes, bounded
recovery and overlap math; fake timestamps/services produce semantic commands.
No Windows headers, process handles, hooks, game functions or DLL loading.

**D — Usable simulator:** accept config, seed, stop count, named scenario and replay
file. JSON reports effective rates, counts, seed, timing statistics, masked/unknown
inputs and selected/delayed/terminal totals. Replay supports interleaved identities,
epochs, eligibility/context, times, pause/cancel and optional normal job times.
Keep million-stop output aggregated; detailed event logs are opt-in.

**E — Verify:** run matrix below on the compiled implementation. Correct bugs,
preserve failures and rerun affected tests. Do not loosen expectations to get green.
Build host harness and x86 target with recorded toolchain identity. The installed
MinGW x86 compiler may serve both on Windows. If an additional toolchain is absent,
state the coverage limit rather than installing one unnecessarily. No game launch.

**F — Package for review:** update maintained docs after preservation. State new
offline policy verified, existing DLL still legacy/unintegrated. Run final workspace
validator, documentation links and protected-original hashes. Follow governance
evidence-change rule; a plan alone never advances maturity. Deliver the reviewer
package and STOP at review. Do not stop earlier with stubs or unrun tests.

## 10. Required acceptance tests

Use independent expected fixtures and public contract behavior, not assertions
that merely mirror implementation. Python tests must invoke compiled policy code.

| Group | Cases |
|---|---|
| Buckets | Exhaust all10000 draw values: exact counts, first/last edges, zero bins, terminal and clean |
| Risk | 500+1 neutral, 825+1 all flags, 925+1 crew120/all flags; cap/apportionment/ties; zero config and overflow |
| Eligibility | Both/tyre-only/fuel-only/no-work/unknown/disabled refuelling; excluded mass remains clean |
| Fairness | Identical identity/context, differing AI/player label => identical result |
| Config | Negative/huge values, NaN/Inf, trailing junk, duplicates/unknowns, missing schema, unordered durations; no partial publication |
| Subselection | Corner/subtype weighted boundaries, zero weights, three fuel subtypes only |
| RNG | Known vectors, seed zero/max, bounded/rejection branch, reproducibility, separate streams |
| Recovery | All recoverable plans terminate within bound; <=3 attempts; no hidden DNF; no duplicate events |
| Lifecycle | Duplicate/cancel/new stop, new epoch/same ID, stale events, pause, backward/NaN time, interleaved cars |
| Timing | Overlap examples, stall, clean identity, unknown baseline, no double-counting/negative/NaN |
| Preservation | Original source/config/DLL/legacy tree unchanged; commands create only intended fresh outputs |

Statistical test: 1,000,000 fully eligible neutral stops across four fixed recorded
seeds (250000 each), independent service identities. For each category, E=N*p;
require `abs(observed-E) <= 6*sqrt(N*p*(1-p))+2`. If p=0 require exact zero.
Terminal expectation is100, not an exact required count. Report expected/observed
and tolerances. Add pressure and eligibility scenarios >=200000 stops each with
independently calculated probability fixtures. Exhaustive tests remain mandatory.
Never switch seeds until failing tests pass. Investigate failures and record fixes.

Scripted replay covers clean, every category/corner/fuel subtype, long recoverable,
terminal, cancellation and simultaneous stops. Forced plans are TEST_ONLY and
excluded from natural-frequency reports. Include commands for a fresh terminal.

## 11. Runtime roadmap: Phases G-I, after review

**G — Reviewer and capability specification:** reviewer inspects diff, reruns tests
and independently checks probabilities. Then investigate SDK/broker interfaces for
session/service/vehicle identity, work eligibility, time, optional context, normal
job timing, per-car delay ownership, commentary and native retirement. Map each
input/output to exact interface, evidence tier, missing capability and fallback.
Do not copy legacy offsets. Missing capability requires a narrowly reviewed SDK
addition; do not silently relocate the product or weaken SDK sandbox authority.

**H — Scoped runtime adapter:** only after separate assignment, negotiate broker
capabilities. Missing essential effect interface disables that effect with reason;
optional missing risk data stays neutral. No fallback to dangerous legacy writes.
Game-thread commands require ownership/cancellation tokens. No worker calls,
duplicate hooks or scratch vehicle queue. Replace global refuel mutation with
reviewed per-car control or leave fuel effects unsupported. Terminal behavior needs
separate review; policy terminal output does not authorize forged retirement state.

Map semantic outcomes to verified HUD/audio. Never identify AI as the player.
Busy/dropped audio does not change policy or block recovery. Handle cancellation,
session end, pause and unload by releasing only owned state. Test simultaneous
cars/conflicting mods. No sleeps or synchronous file I/O in callbacks. Follow SDK
tests/checkpoint rules if changing SDK; preserve all candidate hashes/evidence.

**I — Guarded game verification:** requires identified clean target, preserved
before-state and scoped runtime authority. Canonical clean installation is missing;
do not construct a replacement or mutate the live install without owner direction.
Use guarded full weekend, never QuickRace. Test zero-risk control, forced each
fault, recovery, cancellation, pause, restart, session exit, unload, simultaneous
player/AI and compatibility. Verify fuel conservation, native timing, AI release,
audio and terminal behavior. Then natural-frequency weekends and owner judgment
of tension/fairness/presentation. A changed binary needs new exact-hash validation.
Release requires human acceptance and explicit authority; promote exact tested
hash, never compile in production. None of this is authorized by Phases A-F.

## 12. Stop conditions and reviewer report

Stop affected work if it would touch protected state, require an unavailable
runtime interface, conflict with scoped authority, or require changing this policy
contract to fix a contradiction. Preserve evidence and continue independent work.
Never invent telemetry, a working integration or human validation. Routine local
implementation choices do not need repeated permission.

IMPLEMENTATION_REPORT.md must contain:

1. Version/outcome, separate offline and runtime completion status.
2. Changed-file list, before/after hashes and restoration map.
3. Exact commands/toolchain versions and evidence output locations.
4. Per-test results, distribution counts/expectations/tolerances and seeds.
5. Deviations with reasons, including any unimplemented contract requirements.
6. Focused source diff, API/config documentation and build artifacts.
7. Remaining defects/unknown inputs/effects and next review gate.
8. Documentation gate and final validator delta, not a false clean pass.
9. Wasted-effort check: repeated reads, broad tests, avoidable retries, polling/idle.

Reviewer checklist: units correct; no pressure boost to terminal; no hidden
escalation; no AI-specific odds; no redistribution of unavailable categories;
compiled shared logic really tested; stable streams/identity; bounded recovery;
correct overlap; no game/protected edits; originals preserved; no runtime claims.

## 13. Suggested skills

No skill needed for pure C work. Read AGENTS.md. Use handoff only for a small
temporary continuation pointer, leaving canonical evidence here. SDK guidance is
required if later SDK work is assigned. Do not invoke unrelated cloud/browser/
image skills or spawn agents without explicit owner/applicable authorization.

## 14. Record of this planning task

This is a plan, not implemented policy or a lifecycle promotion. Revised tension
direction is captured; numeric tuning is provisional. Source/config/binaries/game/
governance status unchanged. README before-state is preserved at
`workspace_organization_audit/PITSTOP_IMPLEMENTATION_PLAN_BEFORE_2026-09-21/README.md`.
Restore to component README only after preserving its then-current state. Retain
the plan/new evidence. Plan is indexed from README; runtime/human gates are explicit.
Initial validator had eight known errors; final result is recorded separately.
Wasted effort: combined orientation output truncated; one oversized shell command
was rejected before execution and replaced by apply_patch. No builds or polling.
