# Pit Stop Failures policy v2 — API, configuration and integration gaps

Version 0.2.0-policy.3..
Last updated: 2026-09-21 (post re-review repairs R7–R9).

This document describes the offline C policy implemented in
`src/pit_failure_policy.[ch]` and `src/pit_failure_session.c`, the v2
configuration schema, the simulator contract, and the exact gaps that block
runtime integration. It is the authoritative description of the public API and
configuration semantics. The legacy runtime (`src/pit_commentary.c`) is **not**
connected to this code and is unchanged by policy v2.

Public API layout: the policy and the session lifecycle are declared in the
single header `src/pit_failure_policy.h`; there is no separate session header.

## 1. Units contract

Three unit systems appear in this codebase and are never mixed:

| Unit | Meaning | Where |
|---|---|---|
| basis points (`bp`) | 10000 bp = 100%, 1 bp = 0.01% | every `*_bp` config key, all rate reports |
| seconds | job/stop time | every duration key, all timing math |
| relative weights | unitless ordering only, not probabilities | corner weights, fuel subtype weights |

The legacy `config/pit_failures.ini` expresses per-mille probabilities of one
draw. Those values must not be copied into v2 keys verbatim; the simulator's
`--compat-report` mode produces a dry-run report of the differences and never
converts or writes a preset.

## 2. Default outcome table (one fully eligible neutral prepared stop)

| Bucket order | Outcome | bp | Probability |
|---|---|---:|---:|
| 1 | `slow_corner` | 280 | 2.80% |
| 2 | `wheelgun` | 100 | 1.00% |
| 3 | `fuel_rig` | 90 | 0.90% |
| 4 | `stall_delay` | 20 | 0.20% |
| 5 | `cross_thread` | 10 | 0.10% |
| 6 | `terminal_wheel` | 1 | 0.01% |
| — | `clean` | 9499 | 94.99% |

Recoverable sum: 500 bp. Total trouble: 501 bp. Terminal is the **entire**
terminal budget and is never scaled by context pressure.

Selection is one unbiased integer draw in `[0, 10000)`; the first cumulative
bucket strictly greater than the draw wins. `pf_bucket_for_draw(cfg, ctx,
draw)` is the single selection mapping and is exercised exhaustively for all
10000 values by the compiled tests. Draws at or past the terminal edge are
clean; values outside the universe are clean. Excluded (ineligible) buckets are
set to exactly zero mass and are never redistributed or re-rolled.

## 3. Contextual risk

```text
multiplier_percent = clamp(crew_risk_percent
    + late_request_bonus + late_change_bonus + wet_bonus + positioning_bonus,
    60, 240)
```

Bonuses are percentage points of the **multiplier**, not of absolute risk.
A flag contributes only when explicitly `true` (`PF_TRISTATE_TRUE`); `false`
and `unknown` both add zero. Defaults: crew 100; late request +15; late change
+15; wet +10; poor positioning +25.

Worked presets (all fully eligible):

| Preset | Multiplier | Recoverable total | Trouble total |
|---|---:|---:|---:|
| neutral (no flags) | 100% | 500 bp | 501 bp |
| all flags, crew 100 | 165% | 825 bp | 826 bp |
| all flags, crew 120 | 185% | 925 bp | 926 bp |

Scaling rule: each recoverable weight is scaled by `multiplier/100`; results are
floored and the remaining bp to the round-half-up total are distributed by
largest fractional remainder (ties by table order). If the scaled sum exceeds
`recoverable_cap_bp`, the cap is apportioned across the scaled categories with
the same rule. Terminal is added after the cap and is never clamped upward.

## 4. Eligibility

Eligibility is snapshotted when the service is committed and frozen for that
stop:

| Bucket | Requires |
|---|---|
| `slow_corner`, `wheelgun`, `terminal_wheel`, `cross_thread` | tyre work |
| `fuel_rig` | refuelling allowed and positive requested addition |
| `stall_delay` | real service possible and a release/restart opportunity |

Drive-through, stop-go and no-work visits roll no tyre/fuel problems. Unknown
optional context is neutral. Excluded category mass stays clean; it is never
redistributed to other categories.

## 5. Subselections

Corners: equal default weights `25/25/25/25` for FL/FR/RL/RR; relative weights,
zero means never selected; no front-left bias, no rear time penalty.

Fuel subtypes (relative conditional weights within the `fuel_rig` bucket):
`slow_flow` 70, `temporary_no_flow` 20, `interrupted_flow` 10. All three
recover. There is no spill or fire subtype in v2.

Duration bands are triangular `(min, mode, max)` in seconds of **extra job
time**:

| Outcome | min | mode | max |
|---|---:|---:|---:|
| `slow_corner` | 1 | 2.5 | 5 |
| `wheelgun` | 3 | 5 | 10 |
| fuel `slow_flow` | 3 | 6 | 12 |
| fuel `temporary_no_flow` | 4 | 8 | 16 |
| fuel `interrupted_flow` | 3 | 6 | 12 |
| `stall_delay` | 2 | 4 | 8 |
| `cross_thread` | 10 | 18 | 35 |
| terminal diagnosis presentation | 8 | 12 | 20 |

Validation requires finite `0 <= min <= mode <= max <= 60`; equal endpoints are
legal.

## 6. Recovery and timing

A sampled total extra job time is drawn **once** per stop. Recovery attempts
are bounded by outcome (wheelgun/fuel/cross-thread: 2; slow corner/stall: 1;
maximum 3) and are placed evenly across the interval with the final attempt at
the end. Recoverable outcomes always resolve; there is no hidden escalation,
no additional duration roll and no extra retirement roll.

Job-baseline overlap:

```text
normal_end      = max(valid supplied job ends)
fault_end       = max(other job ends, affected_job_end + extra_job_time)
added_stop_time = max(0, fault_end - normal_end)
```

`stall_delay` occurs after normal work and adds directly. Examples with tyre
end 6 s and fuel end 10 s: `+2 s` tyre trouble adds 0 s; `+6 s` adds 2 s.
Unknown baseline timing yields "unknown added stop time", never an invented
value. Clean stops preserve the caller's timing exactly (no computation).
Selected faults are reported separately from measurably delayed stops.

## 7. Randomness, seed and stream derivation

- Generator: documented splitmix64 (`pf_rng_next_u64`), fixed-width integer
  arithmetic, no libc `rand`, no time, no padding-dependent hashing.
- `pf_rng_below()` uses rejection sampling; plain modulo is never used.
- **The seed is an explicit, immutable input.** `pf_session_create(cfg, seed)`
  fixes it for the session, and the pure APIs take it as a parameter:
  `pf_plan_build(cfg, id, ctx, seed, out)`,
  `pf_plan_category(cfg, id, ctx, seed)`,
  `pf_category_draw(id, seed)`.
- Streams derive from `mix(seed, epoch, vehicle_id, service_token, domain)`
  with domains `category=1`, `subtype=2`, `duration=3`, `cosmetic=4`.
- The seed participates in every decision; identity is independent of the
  seed. Adding audio/cosmetic draws, changing car processing order, or
  inserting another car's stop cannot change an existing stop's outcome.
  Player/AI labels are not part of the API.
- All `uint64_t` seeds are legal, including zero; `pf_parse_u64_strict()`
  rejects any value above `UINT64_MAX`. The simulator reports the effective
  seed and marks `--seed auto` runs `seed_auto: true`.

## 8. Configuration schema (v2)

`config/pit_failure_policy_v2.ini`, `[general] schema_version = 2`. All keys are
required; unknown keys, duplicate keys/sections, malformed numbers, NaN/Inf,
trailing junk, overflow, negative weights and unordered durations are fatal.
Validation happens on a temporary copy and the caller's config is published
only after complete success. Errors name line and key and produce a nonzero
simulator exit; there is no partial configuration and no silent fallback.

| Section | Keys | Units / range |
|---|---|---|
| `general` | `schema_version`, `crew_risk_percent` | 2; 80..120 |
| `probability` | `slow_corner_bp`, `wheelgun_bp`, `fuel_rig_bp`, `stall_delay_bp`, `cross_thread_bp`, `terminal_wheel_bp`, `recoverable_cap_bp` | bp; base sum <= 1200; terminal <= 10; cap 1..1200 |
| `duration` | `*_min`, `*_mode`, `*_max` for the eight bands | seconds; 0..60, ordered |
| `corner` | `fl_weight`, `fr_weight`, `rl_weight`, `rr_weight` | relative, >= 0, sum > 0 |
| `fuel_subtypes` | `slow_flow_weight`, `temporary_no_flow_weight`, `interrupted_flow_weight` | relative, >= 0, sum > 0 |
| `context` | `late_request_bonus`, `late_change_bonus`, `wet_bonus`, `positioning_bonus` | 0..100 |

`pf_config_render()` writes a canonical round-trippable document; the default
preset is emitted by `pf_config_defaults()` and mirrored in the shipped INI.

Numeric parsing is shared by the config parser, the CLI and replay files:
`pf_parse_u64_strict()` (digits only, no sign/whitespace/junk, full range) and
`pf_parse_double_strict()` (finite, no sign/whitespace/NaN/Inf, full
consumption).

## 9. Session lifecycle

### Storage model (bounded, explicit)

- `active` holds services currently being serviced; `history` retains
  completed/cancelled identities of the **current epoch** so duplicate
  notifications return the stored plan and cancelled tokens stay rejected.
- Completed stops do not consume active capacity; a full weekend (1200+
  stops in one epoch) is covered by the compiled capacity test. `new_epoch`
  is only for a real session discontinuity.
- Both stores are bounded (`PF_ACTIVE_MAX`, `PF_HISTORY_MAX`, `PF_QUEUE_MAX`;
  a build may override the macros for bounded-limit testing/tools). **History
  space is reserved at commit**, so an accepted service can always be retained
  at completion; once reservations fill the bound, further commits are refused
  explicitly with `PF_SESSION_ERR_CAPACITY`. Nothing is evicted and no token is
  silently reused.

### Plan ownership (copy-out)

`pf_session_commit(..., pf_plan_t *out_plan, int *out_committed)` copies the
decision into caller-owned storage: no borrowed pointer is returned, and a
retained copy stays valid across active compaction, array growth, completion
into history and epoch reset. A duplicate notification (new `out_committed ==
0`) copies the stored plan; refusal returns -1 with `last_error`.
`pf_session_plan()` remains a copy-out lookup.

### Clock contract (one shared clock)

- One shared, monotonic simulated time per session. `commit`, `advance`,
  `pause` and `resume` require finite `now` >= the last accepted time.
- NaN, Inf or backwards time is a discontinuity: every live service in the
  current epoch is cancelled, undelivered commands of the epoch are discarded,
  and every time-bearing call fails (`PF_SESSION_ERR_INVALID_TIME` on the
  detecting call, then `PF_SESSION_ERR_INVALID_STATE`) until
  `pf_session_new_epoch()`.
- `pause()` freezes progression for all services; `resume()` excludes the
  wall-clock pause duration.
- Stale-epoch callbacks are ignored; identities from other epochs are refused.

### Delivery contract (exactly once, never lost, always drainable)

- Due events are appended once to an internal queue when their `due_seconds`
  is reached; `pf_session_advance()` drains them.
- `cmds == NULL` or `cmd_capacity <= 0` is a **query**: validates/advances the
  clock, queues newly due events, copies nothing and returns the number of
  undelivered commands (`pf_session_pending()` provides the same count).
- Otherwise advance writes at most `cmd_capacity` commands from the front of
  the queue and returns the number written (never more than capacity).
- Lifecycle transitions happen when the final event is queued, not when it is
  drained; delivery is transport and never changes state.
- **Backpressure (R9):** when the delivery queue is full, production stops but
  already-queued events are still drained; unqueued events are retried on
  later calls. Advance returns -1 only when the session requires a reset;
  capacity conditions are reported through `last_error`
  (`PF_SESSION_ERR_CAPACITY`). A saturated queue never blocks draining and
  never discards events.
- `sequence` is the per-service event index, incremented only when an event is
  accepted into the queue, so event IDs are identical under dense, sparse,
  paused or late polling.

### Cancellation (R7)

`pf_session_cancel()` purges **all undelivered** queued commands for the exact
`(epoch, vehicle, token)` while preserving other identities and their order.
It works both for active services and for services whose plan already
completed into history with its final event queued but undrained. The
cancelled-token tombstone is retained, so later commits of the token are
refused. Events already delivered to a caller cannot be recalled; that
boundary is explicit.

### Identity and decisions

Identity is `(epoch, vehicle_id, service_token)`. A commit freezes
configuration/context and decides exactly once with the session seed; a
duplicate notification of a completed service copies the same stored plan
(`*out_committed == 0`); a cancelled token is refused with
`PF_SESSION_ERR_TOKEN_CANCELLED`. Terminal outcomes emit a semantic
`terminal-request` command only; the policy never writes game state and
contains no OS or game calls.

## 10. Simulator CLI

```powershell
pit_failure_sim --config <v2-ini> --seed 42 --stops 1000000 --json report.json
pit_failure_sim --config <v2-ini> --seed 20260921 --replay tests\replay-scenarios.txt --jobs tyre=6,fuel=10
pit_failure_sim --compat-report config\pit_failures.ini --json compat.json
```

Options: `--scenario neutral|pressed|tyre_only|fuel_only|no_work|unknown`,
`--events <jsonl>` (opt-in detailed log), `--tokens-from <n>`.
The report is aggregate JSON: effective rates, counts, subselections, timing
statistics, seed (nullable only for compat mode) and histogram. Exit codes:
0 success, 2 configuration/number error (strict rejection of malformed seeds,
stop counts, token ranges, job specs and configs), 3 replay error, 4 platform
error.

Replay script commands (one per line, `#` comments):

```text
seed <uint64>                   set the session seed (only before first commit)
epoch <E>                       start a new epoch and clear state
commit <vehicle> <token> [flags]  commit a service
advance <seconds>               advance relative to the current replay time
advance_to <seconds>            advance to an absolute replay time
pause [seconds] | resume [seconds]   default: the current replay time
cancel <vehicle> <token>
end                             stop processing
```

Commit flags: `late_request=`, `late_change=`, `wet=`, `positioning=`
(`true|false|unknown`), `tyre=0|1`, `fuel=<liters>`, `nofuel`, `nowork`,
`norelease`. All numbers are parsed strictly; malformed input fails the line
and the run. Replay reports the effective seed. Forced/scenario plans in replay
files are test inputs and are not natural-frequency evidence.

## 11. Legacy compatibility findings (dry run)

`--compat-report` on `config/pit_failures.ini` reports 11 probability-like
per-mille keys, 34 duration keys, 1 unsupported behavior (`rear_penalty`) and
3 obsolete recovery-semantics keys (`recover_pct`, `fuel_recover_pct`,
`escalate_pct`). The legacy `fuel_spill` subtype has no v2 equivalent. The
report performs no conversion and recommends selecting the v2 preset
explicitly.

## 12. Integration gaps (runtime work, not authorized here)

The runtime adapter is explicitly not implemented. Interfaces identified below
were **not investigated in this offline work**; treat them as unavailable and
unverified for this adapter until Phase G maps them from SDK/broker evidence.
Known gaps before any live use:

1. **Effect interface ownership.** A broker interface for per-car delay
   ownership, safe release/restart and semantic output mapping is unavailable
   and unverified. The adapter must use the SDK front door and game-thread
   command tokens.
2. **Session/service identity source.** The game-side source for
   `(epoch, vehicle_id, service_token)` is undefined; a stable per-stop token
   is required, and `new_epoch` must map to a real session discontinuity.
3. **Normal job baselines.** The adapter must supply tyre/fuel job ends for
   `added_stop_time`; without them the policy reports unknown timing and the
   effect cannot be measured (the policy still decides).
4. **Optional risk inputs.** Late request/change, wet and poor-positioning
   measurements are provisional; they stay neutral until reviewed with fair
   player/AI evidence.
5. **Terminal handling.** Retirement or terminal presentation requires a
   separate reviewed path; policy output never authorizes state writes.
6. **Audio.** The module emits no audio; a runtime adapter must map outcome
   events to verified HUD/audio and must not let a busy/dropped cue change
   policy or recovery.

Until those are specified and reviewed, the correct integration state remains:
new offline policy verified, existing DLL still legacy/unintegrated.
