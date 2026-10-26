# Pit Stop Failures — Phase G integration specification (revision 4)

Status: **specification for review, revision 4 (G1/G2 cleanup, cursor edges,
owner player/AI acceptance target).** No implementation performed. Date:
2026-09-21. Product: `pitstop-modding` (development prototype). Offline policy
base: 0.2.0-policy.3 (accepted). Fire extension proposed as `0.3.0-policy.1`.
. No runtime integration, no game installation change and
no live write was made in producing this document.

Revision 4 supersedes revision 3
(SHA-256 40E29D680FCBFAEA260D762B7153274DD8677AD2E00929E9F2B34C57B786390A;
before-state preserved under
`workspace_organization_audit\PITSTOP_PHASE_G_R4_BEFORE_20260921-161302\`).
Disposition of the revision-3 review items is in Section 14. Nothing in any
revision was executed; all proposed APIs remain proposals for SDK-owner review.

Authority: `docs/IMPLEMENTATION_PLAN_2026-09-21.md` Section 11 (Phase G),
`docs/PIT_FIRE_ADDENDUM_2026-09-21.md`, `docs/GAMEPLAY_ACCEPTANCE_2026-09-21.md`
(owner target), `docs/TEST_GUI_CONTRACT.md`, the three integration reviews, and
the owner instructions of 2026-09-21.

## 1. Result summary

| Capability class | Status for this adapter |
|---|---|
| Session/vehicle identity | **Supported** |
| Pit state: `pit_status`, `call_to_box` | **Supported** (ABI-11 snapshot) |
| Pit state: service latch, work flags, job fields, fuel bound | **Native layout only**, not broker-exposed |
| Session environment, game-thread posting | **Supported** |
| Sim session clock | **Native CONFIRMED global only** |
| Per-stop service identity + service events | **Missing** — new API (6.1) |
| Work eligibility + normal job timing + certified fuel bound | **Missing** — new snapshot fields (6.1) |
| Service transaction: baseline, deadline, lease, cleanup | **Missing** — new capability (6.2) |
| Presentation (commentary + acknowledged HUD) | **Missing** — accepted design (6.3) |
| Fuel-service suspend/resume | **Missing** — native mechanism gated (6.4) |
| Owned fire effect | **Missing / blocked** — ownership evidence required (6.5) |
| AI/player retirement/garage + confirmed narration | **Missing** — new provider required by the owner target (6.6); blocked until a reviewed path exists |
| Native terminal via raw writes / teleport | **Forbidden** |
| Broker ABI negotiation | **Divergent** (11/13/15, revision-1 capture); matrix to be rebuilt at Phase I |

**Six** narrowly scoped broker capabilities (bits 14–19) plus native
investigations are required: pit service (14), service transaction (15),
presentation (16), fuel control (17), owned effects (18), retirement/garage
(19). Separately authorized offline policy/mock work does not require the live
capabilities (Section 12).

## 2. Investigation method

Read-only through the SDK front door from its repository root: `start`,
`sdk-info`, `capabilities`, `sdk-query function ParticleTrailEmitter`
(`LEGACY`, null prototype), `sdk-query function PitStopOperationTimer`
(`CONFIRMED`), `sdk-query field PitStopEndTime` (`CONFIRMED`
`latched_control`), `evidence-query findings fuel` (34 records), plus canonical
headers and the fire lab. Hashes:
`verification/pit-integration-2026-09-21/sdk-source-hashes.json`;
`verification/pit-integration-r2-2026-09-21/sdk-hash-recheck.json` shows 20/21
files unchanged and `tools/sdk/AUTOMATION_CHECKPOINT.json` drifted (cause not
inferred; no fact here depends on it).

Evidence tiers: **broker ABI**, **FACT/CONFIRMED**, **native-reviewed**,
**LEGACY**, **PROPOSED**. Runtime consequences are inference; nothing is
live-validated.

## 3. Capability inventory

### 3.1 Broker core

`f1c_broker_abi.h` (ABI 15 header; bits 0–13 in use), `f1c_broker.h/.c`:
client registry and game-thread posting (`F1CBrokerPost`), 256-entry client
queue, update budget **32 tasks / 2000 µs**, slow-callback quarantine
**5000 µs (5 ms)**, ABI-14 native callback budget
`F1C_BROKER_NATIVE_CALLBACK_US` = **250000 µs (250 ms)** — three distinct
numbers. Existing capabilities include `LIFECYCLE`, `SEMANTIC_PIT`,
`VEHICLE_SNAPSHOTS`, `POLL_EVENTS`, `SESSION_SNAPSHOT`, `PAUSE_MENU`,
`RESTART_GUARD`, `CALLBACK_RECOVERY`, `SESSION_END`, `RESULTS_FINALIZE`,
`PRELOAD`, `PENALTY_HOLD`, `PACE_CONTROL`, `HIGH_GEAR_RATIO`. No pit-service,
transaction, presentation, fuel-control, effect or retirement capability
exists.

### 3.2 Identity and lifecycle (broker-supported)

`F1CBrokerVehicleHandle { session_generation, object_generation, slot }`;
ABI-11 vehicle snapshots (`pit_status`, `call_to_box`, `ai_control`,
`controller_role`, `retired`, `finish_state`, `reason_code`,
`car_conditions`, `driver_id`, `team_id`, names, damage/orientation);
ABI-7 session snapshots (session generation/type/race phase, wetness,
temperatures); poll events for session/vehicle/pause-menu. The service-ready
latch is **not exposed**.

### 3.3 Pit/service internals (native-reviewed, not exposed)

`veh+0x3450` `PitStatus`; `veh+0x345C` service-ready latch;
`veh+0x346C/0x3470` stop start/end; `veh+0x454C` call-to-box.
`F1CPitMachine` at `veh+0x32CC`: `refuel_active` (+0x1C0),
`tire_change_active` (+0x1C4), `wing_adjustment_active` (+0x1C8),
`repair_active` (+0x1CC), `fuel_start/time/target` (+0x1B0/1B4/1B8),
`tire_service_start/end` (+0x1D0/1D4), `stop_start/end` (+0x1A0/1A4).
`PitStopOperationTimer_004a6340` (`CONFIRMED`): max of enabled jobs + noise;
nominal refuel flow 12.0 L/s from `flt_59E96C` (**context only — not a bound**;
see 6.1/7.2). Reviewed helpers include `f1c_pit_hold_after_service` and
`f1c_pit_request_game_owned_garage`. **Pit-entry marker is entry evidence
only**; correct-box identification remains missing.

### 3.4 Commentary, presentation and cues

Reviewed ABIs: `CommentaryEventQueue` (0x004144F0, `CONFIRMED`),
`CommentaryDriverEntryInit`, `CommentaryNameListRebuild`, `DriverNameIDLookup`.
Injection is historically proven; the tire-poke trigger is **DISPROVEN**. No
audio-start observation exists, so the fire milestone is a HUD acknowledgement
(6.3). Legacy HUD strip `G_HUDMessageStripPtr` (0x7CD204, FACT) is shared and
must be broker-owned.

Alpha cue declarations in `src/pit_commentary.c` (inherited source claims, not
live playback today): Limiter On eid 51 (status 0→2), Brake eid 53
(PWS_PIT08; **trigger mapping unverified**), Go eid 45 (status 3→4), Limiter
Off eid 43 (status 4→0). Retirement eid 117 (PWS_RETIRE02) is declared; the
driver-specific clip and subject mapping are **not established**.

### 3.5 Effects and fire (legacy lab only)

`fuel_fire_lab` uses `ParticleTrailEmitter` (0x004CBC50, **LEGACY**, null
prototype), rewrites the live per-track FLAMES descriptor, uses
`CreateThread`, engine flames bank 0x7C921C and engine kill 0x7C9081.
Excluded as-is; ownership evidence required (6.5).

### 3.6 Fuel-service control

No owned mechanism identified; candidates are static-derived and unproven as
controls; the global rate constant is forbidden; `f1c_restart_fuel.h` is
offline fuel-level setting, not per-stop suspension. Gated (6.4).

### 3.7 Pit hold / delay

`PitStopEndTime` (0x3470, `CONFIRMED` `latched_control`) is the reviewed lever.
The service transaction (6.2) owns baseline, deadline, lease, lifetime and
cleanup.

### 3.8 Time, pause, session boundaries

`G_SessionClockSeconds` (0x008AFEA0, `CONFIRMED`). Pause freezes the
simulation. Session generation increments at boundaries.

### 3.9 Effects/terminal facts

Player pause-menu `RETIRE_PITS` is the only player retirement command; the
game-owned garage helper drives the game's blocking/garage path. **No AI
retirement/garage path is identified.** Raw `FinishState`/`ReasonCode`/
`PLAYER_OUT` writes and teleports are forbidden.

### 3.10 Development GUI frontend

`tools/pit_test_gui.py` + `docs/TEST_GUI_CONTRACT.md`: the panel exists with
its action disabled; the runtime connection is not implemented. Contract
summary in Section 8.2.

## 4. Policy mapping matrix

| Policy item | Proposed interface | Tier | Gap | Fallback |
|---|---|---|---|---|
| `epoch`, `vehicle_id` | session generation + handle | broker ABI | none | — |
| `service_token` | pit-event/snapshot token (6.1) | PROPOSED | missing | refuse to decide |
| service state / commit point | 6.0 state machine at READY | PROPOSED | missing | no stop rolls |
| tyre/fuel eligibility, liters | `work_flags`, `fuel_add_liters` | native-reviewed | not exposed | category masked |
| real service possible | any flag + ready + status 3 | native-reviewed | not exposed | refuse |
| release possible | READY/ACTIVE + valid baselines | native-reviewed | 6.0 | skip stop |
| normal/per-job ends, certified fuel bound | snapshot fields | `CONFIRMED` sources | not exposed | timing unknown; fire suppressed without bound |
| sim time | coherent batch `session_time` | `CONFIRMED` source | not exposed | adapter cannot run |
| late request/change/poor positioning | no measurement | — | unmeasurable | neutral |
| wet | session snapshot | broker ABI | none | neutral |
| commentary / HUD acknowledgement | presentation capability (6.3) | reviewed ABI / PROPOSED | missing | suppress presentation |
| added delay / stall | service transaction (6.2) | `CONFIRMED` lever | missing | select, no measurable delay |
| fuel suspend/resume | 6.4 bound to transaction | none | missing | suppress fire |
| fire effect | 6.5 bound to transaction | LEGACY | blocked | suppress fire |
| terminal wheel | retirement/garage provider (6.6) + narration gate | none | missing | **AI terminal acceptance blocked** (never a semantic-event pass) |
| cancellation | transaction cleanup state machine (6.2) | broker ABI | partly missing | enforced broker-side |

## 5. Configuration contract additions

- New `schema_version = 3` file `config/pit_failure_policy_v3.ini`; existing v2
  files remain valid with **fire disabled**.
- `[fire] fire_given_fuel_fault_bp` (0..1000, default 0; fire preset 500);
  strict parser rules unchanged. One conditional draw in `PF_DOMAIN_FIRE` for
  an already-selected `FUEL_RIG` plan.
- Absolute selection rate = `p_fuel` × `fire_given_fuel_fault_bp/10000`;
  denominator is parent fuel faults. Configured probability is validated
  against **selected/parent faults**; realised (**ignited/parent faults**) is
  ≤ selected (equality with zero suppression) and never used to tune or
  reroll.
- Duration budget: ignition 1–2 s after the confirmed milestone, burn
  tri(4, 7, 12) s, inspection tri(2, 4, 7) s; dissipation bounded by the
  provider.
- The development 100% arming input is **not** configuration: it is the
  adapter contract in Section 8.2 (one-shot, identity-scoped, production-off).
  Global config is never modified for tests.

## 6. Proposed SDK additions

All additions follow the append-only pattern: new capability bits, new
exports, typed `F1CBrokerResult`, `structure_size`-guarded requests, no new
hooks, no raw addresses in mod code.

The compilable contract declarations exercised by the isolated mock live in
`mock/pit_mock_contract.h` (artifact `mock-contract-1`, prefix `F1CMock`). The
declarations in this section are documentation for SDK review and are not
claimed to be a buildable SDK ABI; the mock header is the executable reference
for the current revision.

### 6.0 One observation time, explicit discontinuities, timing inputs (accepted)

State key `(session_generation, object_generation, slot, service_token)`.
States `IDLE → DETECTED (token assigned) → READY (adapter commit milestone) →
ACTIVE → ENDED | CANCELLED`. Events are advisory; snapshots authoritative;
duplicate READY idempotent. **ACTIVE late attach is skipped** (`MISSED_SERVICE`).
All samples in one tick must carry the **same** `snapshot_sequence` and
`session_time`; any mismatch (older or newer) retries the whole batch with no
mutation. An **authoritative** invalid clock (NaN/Inf/backwards/reset/unload)
invalidates the policy session and drives transaction cleanup (6.2) before a
new epoch. Policy job ends are relative: `end_seconds = native_job_end −
stop_start_time`, finite and ≥ 0; native absolute times are used only for the
transaction/fire timeline. Baselines are captured at READY before any
mutation; the adapter never re-invokes `PitStopOperationTimer` for random
timing.

### 6.1 Pit-service snapshot and versioned pit-event polling (G1 cleanup)

The existing `F1CBrokerEvent`/`F1CBrokerPollEvents` record and export remain
untouched. Capability bit 14.

Snapshot fields (as revision 3) plus a **provider-certified fuel-work bound**:

```c
#define F1C_BROKER_PIT_VALID_FUEL_BOUND  (1ull << 6)
/* fuel_remaining_upper_seconds: certified upper bound for completing the
   remaining fuel service after suspension and resume, including resume/setup
   overhead and the active fuel-rig fault. Nominal flow (12.0 L/s) is context,
   never a substitute for this bound. */

typedef struct F1CBrokerPitServiceSnapshot {
    uint32_t structure_size;
    uint32_t snapshot_version;      /* 1 */
    uint64_t valid_fields;
    F1CBrokerVehicleHandle handle;
    uint64_t service_token;
    uint64_t snapshot_sequence;
    float    session_time;
    int32_t  pit_status;
    int32_t  service_ready;
    uint32_t work_flags;
    float    fuel_add_liters;
    float    fuel_remaining_upper_seconds;  /* VALID_FUEL_BOUND */
    float    stop_start_time;
    float    stop_end_time_baseline;
    float    tyre_job_end_time;
    float    fuel_job_end_time;
    float    wing_job_end_time;
    float    repair_job_end_time;
    float    normal_job_end_time;
} F1CBrokerPitServiceSnapshot;
```

Pit events (own format, explicit request arguments) as revision 3, with the
cursor rules completed:

```c
typedef struct F1CBrokerPitEventRequest {
    uint32_t structure_size;
    uint32_t event_version;        /* must equal 1 */
    uint64_t after_sequence;       /* next sequence to include; 0 = sentinel */
    F1CBrokerPitEvent *records;    /* NULL only when record_capacity == 0 */
    uint32_t record_capacity;
    uint32_t record_size;          /* must equal sizeof(F1CBrokerPitEvent) */
    uint32_t record_stride_bytes;  /* >= record_size */
    uint32_t reserved;
} F1CBrokerPitEventRequest;

typedef struct F1CBrokerPitEventResult {
    uint32_t structure_size;
    uint32_t reserved;
    uint64_t next_sequence;
    uint64_t dropped;
    uint64_t skipped_stale;
    uint64_t oldest_retained;
    uint32_t written;
    uint32_t reserved2;
} F1CBrokerPitEventResult;
```

Cursor semantics (complete):

1. **Zero sentinel normalization.** `start = (after_sequence == 0) ?
   oldest_retained : after_sequence`. Then
   `if (start < oldest_retained) { dropped = oldest_retained − start; start =
   oldest_retained; } else dropped = 0;`. The zero sentinel therefore never
   counts `oldest_retained − 0` as loss.
2. **All-stale scans progress.** `next_sequence = highest examined ring
   sequence + 1` whenever at least one record was examined, even when every
   examined record was stale and nothing was returned. `skipped_stale` counts
   examined-stale records; `dropped` counts only overflow-skipped sequences.
   A reader can never loop on the same stale records.
3. **Nothing examined.** With `record_capacity == 0`, or an empty ring,
   `written = 0` and `next_sequence` is unchanged (`after_sequence`, or
   `oldest_retained` for the zero sentinel on an empty ring).

**Cursor examples.** The two one-record polls start from a ring holding
sequences 5–8. The zero-capacity and all-stale rows assume the ring has since
advanced (oldest retained 9, records 9–12 examined for the all-stale row); the
overflow row assumes oldest retained 9 with the consumer cursor at 6.

| Case | after | capacity | Returns | written | next | dropped | skipped_stale |
|---|---:|---:|---|---:|---:|---:|---:|
| two one-record polls, first | 0 | 1 | seq 5 | 1 | 6 | 0 | 0 |
| two one-record polls, second | 6 | 1 | seq 6 | 1 | 7 | 0 | 0 |
| zero-capacity query | 6 | 0 | — | 0 | 6 | 0 | 0 |
| empty ring, sentinel | 0 | 4 | — | 0 | oldest | 0 | 0 |
| all examined stale | 6 | 4 | — | 0 | 13 (examined through 12) | 0 | 4 |
| overflow | 6 | 4 | from oldest 9 | ≤4 | ≥10 | 3 | per stale examined |

Version/size/stride, capability negotiation and failure results are unchanged
from revision 3 (explicit request arguments; `BAD_ARGUMENT` on unknown version,
wrong `record_size`, undersized stride, overflow in stride × capacity, or NULL
records with capacity > 0; absent capability disables pit-service features).

### 6.2 Service transaction cleanup state machine (G2 blocking corrections)

Capability bit 15 owns baseline, deadline, lease, lifetime and **cleanup**.

```c
typedef struct F1CBrokerServiceTxnRequest {
    uint32_t structure_size;
    uint32_t request_version;      /* 1 */
    F1CBrokerVehicleHandle handle;
    uint64_t service_token;
    float    max_lifetime_seconds; /* 1..120, absolute cap from Begin */
    float    initial_lease_seconds;/* 0.25..10 */
    uint32_t flags;                /* reserved, 0 */
} F1CBrokerServiceTxnRequest;

/* Begin returns baseline_stop_end_time (B) and txn_id. */
F1CBrokerResult F1CBrokerServiceTxnBegin(token, req, uint32_t *txn_id, float *B);
F1CBrokerResult F1CBrokerServiceTxnRenew(token, txn_id, float lease_seconds);
F1CBrokerResult F1CBrokerServiceTxnDeadline(token, txn_id, float T);
F1CBrokerResult F1CBrokerServiceTxnEnd(token, txn_id, float final_deadline);
/* FuelSuspend/FuelResume and EffectBegin/Stop/Status take txn_id (6.4/6.5). */
```

**Deadline invariants** (unchanged): `T >= B`; non-decreasing; `T <= begin +
max_lifetime`; writes only while leased and only when the game value equals the
last provider write or `B`; a later game write ends writes (`SUPERSEDED`).
`Renew` only extends the lease.

**End preflight — validate before any side effect.** `End(final)` performs,
in order, all of the following before touching anything:

1. identity still current for `(session_generation, object_generation, slot,
   service_token)`; otherwise `INVALIDATED` with **no writes**;
2. effect state `CLEARED`; if `EMITTING`/`DISSIPATING` → `EFFECT_ACTIVE` and
   **no deadline write, no fuel resume, no release, no state change** (effect
   stop/clearance is a distinct transition);
3. `max(B, final) <= begin + max_lifetime` and `final >= 0`; otherwise
   `BAD_ARGUMENT`, no writes — End cannot bypass the lifetime cap or shorten
   below `B`;
4. reconciliation: write `max(B, final)` only if the game value still equals
   the last provider write; otherwise leave the game's newer value untouched
   (`SUPERSEDED`).

Only after preflight: write (if reconcilable) → resume fuel if suspended →
release → `ENDED`.

**Lease expiry drives cleanup without client cooperation.** The provider
watches its own session-time lease in its update loop. On expiry it enters
cleanup autonomously: stop emission (if active) → wait for provider-declared
clearance (bounded) → resume fuel → leave the last written `T` (never reduce)
→ release. No later adapter call and no unrelated lifecycle event is required.
The deadline `T` already covers inspection and remaining fuel, so a crashed
client still yields a safe, bounded release.

**Authoritative invalid clock.** Stale transport samples are retried without
mutation (6.0). For an authoritative invalid clock, the provider cannot wait
for a session-time clearance: it stops emission immediately (provider-internal,
no game write), and then either (a) completes cleanup on the next valid update
of the same live session, or (b) if the clock stays invalid for a wall-clock
cleanup budget (proposed 5 s), or the session was reset/destroyed, marks the
transaction `INVALIDATED`: no deadline writes, no fuel resume into a replaced
car, resources considered gone with the session, and a new epoch is required.
**If the fire provider cannot guarantee emission stop and bounded cleanup in
this condition, fire remains disabled.**

**Destroyed/reset separation.** Identity mismatch (car replaced, slot reuse,
session change) never writes to the new occupant: cleanup is
`INVALIDATED`, not a same-session sequence. Valid same-session cleanup (End,
lease expiry, service end, unregister) is separate and always writes only to
the still-current identity.

**Transition table** (guards, actions, outputs):

| Trigger | Guards | Deadline | Fuel | Effect | Exit |
|---|---|---|---|---|---|
| `End(final)` while effect active | identity current; effect not CLEARED | **none** | **none** | **none** | `EFFECT_ACTIVE`, transaction unchanged |
| `End(final)` after CLEARED | identity current; `max(B,final) ≤ cap`, `final ≥ 0` | write `max(B,final)` iff reconcilable; else leave game value | resume if suspended | already CLEARED | `ENDED` (or `SUPERSEDED` note) |
| `End(final)` with invalid final | identity current | **none** | **none** | **none** | `BAD_ARGUMENT` |
| Lease expired | identity current | leave last `T` (no reduction) | resume | stop emission → wait CLEARED (bounded) | `ENDED_LEASE_EXPIRED` |
| Authoritative clock invalid | session still current | no writes; cleanup on next valid update | resume on next valid update | stop emission immediately (no game write) | `CLEANUP_PENDING` → `ENDED` on valid update |
| Clock invalid beyond wall budget or session destroyed | identity lost/reset | **none** | no writes to replaced car | emission stopped provider-internally | `INVALIDATED`; new epoch required |
| Service destroyed / slot reused | identity mismatch | **none** | **none** | emission stop only | `INVALIDATED` |
| Suppression after suspension | fire branch not committed | keep `T_ord` | resume | none acquired | `ENDED_SUPPRESSED`; ordinary pump fault proceeds |
| `SUPERSEDED` (game wrote later) | identity current | stop writing; game value stands | unchanged until End | unchanged | `OPEN_SUPERSEDED` → cleanup later |
| Client unregistered / unloaded (still suspended) | identity current | leave `T` | resume | stop/clear as above | `ENDED` |
| Late tick past `T` and lease | identity current | no writes | resume on cleanup | stop/clear | `ENDED` idempotent |

**Model assertions** (to be executed by a separately authorized mock; not
claimed here): no fuel resume or deadline reduction on `EFFECT_ACTIVE`; no
session-time wait while the clock is invalid; no writes to stale identities;
eventual owned-resource cleanup without client cooperation; `End` cannot
exceed the lifetime cap.

### 6.3 Presentation capability (accepted; cue use added)

Bit 16; identity-scoped requests with `handle`, `service_token`,
`request_id`, `channel` (HUD | COMMENTARY), `commentary_event_id`, bounded
text, `hud_seconds`; states `NONE/ACCEPTED/VISIBLE/EXPIRED/CANCELLED/REFUSED`;
status query and cancel. HUD `VISIBLE` = owned strip slot written and re-read
intact on the next update (proxy, not proof of human viewing); commentary is
enqueue-only and cannot satisfy the fire milestone; arbitration per (client,
vehicle); purge on service end/cancel/session end/unload. The adapter's cue
contract (Section 8.1) uses this capability for every pit-lane and fault cue.

### 6.4 Fuel-service suspend/resume (gated)

Bit 17, taking `txn_id`. Suspend only the matching service's refuelling job;
never the global rate constant; never another car. The transaction lease is
the fuel lease (one lease, Section 6.2), so fuel cannot outlive the
transaction and lease expiry resumes it. The native mechanism remains
unproven and gated; without it fire is suppressed.

### 6.5 Owned fire effect (ownership evidence required)

Bit 18, taking `txn_id`; preset as revision 3; states `EMITTING`,
`DISSIPATING`, `CLEARED`; `EffectStop` ends emission but not clearance; the car
is never released before the safety clearance milestone (Section 7.2/9.2);
`End` while active returns `EFFECT_ACTIVE`. No whole-descriptor save/restore;
ownership evidence required (allocation/lifetime, writer inventory, RNG
policy, two-car live verification). Blocked until reviewed.

### 6.6 Retirement/garage provider and narration gate (owner acceptance)

Bit 19:

```c
typedef enum F1CBrokerRetirementMode {
    F1C_BROKER_RETIRE_GARAGE_OWNED = 1  /* game-owned pit/garage transition */
} F1CBrokerRetirementMode;

typedef struct F1CBrokerRetirementRequest {
    uint32_t structure_size;
    uint32_t request_version;      /* 1 */
    F1CBrokerVehicleHandle handle;
    uint64_t service_token;
    uint32_t mode;                 /* F1CBrokerRetirementMode */
    uint32_t flags;                /* reserved, 0 */
} F1CBrokerRetirementRequest;

typedef enum F1CBrokerRetirementState {
    F1C_BROKER_RETIRE_REQUESTED = 1,
    F1C_BROKER_RETIRE_TRANSITIONING,
    F1C_BROKER_RETIRE_GARAGED,     /* authoritative confirmation */
    F1C_BROKER_RETIRE_FAILED
} F1CBrokerRetirementState;

typedef struct F1CBrokerRetirementStatus {
    uint32_t structure_size;
    uint32_t state;
    F1CBrokerVehicleHandle handle;
    uint64_t service_token;
    uint32_t session_generation;
    float    changed_at_session_time;
} F1CBrokerRetirementStatus;

F1CBrokerResult F1CBrokerRetireVehicle(token, req, uint32_t *retirement_id);
F1CBrokerResult F1CBrokerRetirementGetStatus(token, retirement_id,
                                             F1CBrokerRetirementStatus *);
F1CBrokerResult F1CBrokerRetirementCancel(token, retirement_id);
```

Semantics: a game-thread command driving the **game's own** retirement/garage
transition for the exact identity; never raw `FinishState`/`ReasonCode`/
`PLAYER_OUT` writes, never a teleport; status is sourced from authoritative
observation (vehicle lifecycle/garage state) so `GARAGED` means the transition
actually completed. **Narration (retirement announcement) may play only after
`GARAGED`**, never after a request. Failure leaves the car a participant and
reports `FAILED`; session change/reset cancels and invalidates. The native
path for AI is **not identified**; until a reviewed path exists, the AI
terminal acceptance case stays **blocked** and must not be passed by a
semantic event, a raw write or a teleport. Player terminal uses the same
provider where the game-owned path exists; the pause-menu retirement remains
an observation of the game's own path, not a substitute for provider status.

**Narration mapping gate.** The driver-specific retirement clip and subject
mapping are not established. Investigate the prior alpha assets and the
historical event catalogue before assigning identifiers; until verified,
retirement narration reports `RETIREMENT_NARRATION_UNMAPPED` and the terminal
case remains blocked. Never substitute a player first-person clip for AI.

### 6.7 Unified ownership, cleanup, concurrency (updated)

- Every call validates the full identity key; stale tokens never write.
- Cleanup is broker-driven (6.2) and never requires client cooperation.
- One transaction per (client, vehicle, service); one deadline per
  transaction; presentation arbitration per (client, vehicle); effects bounded
  per client and vehicle; typed refusals.
- New capabilities obey the existing update budget (32 / 2 ms), slow-callback
  threshold (5 ms) and native callback budget (250 ms, unchanged).
- Order: effect stop → wait CLEARED → inspection (gameplay, already inside
  `T`) → fuel resume → End.

## 7. Fire extension specification

### 7.1 Decision and probability

As Section 5. Fire is conditional on an already-selected `FUEL_RIG` plan; one
draw in `PF_DOMAIN_FIRE`; ordinary subtypes and streams unchanged.

### 7.2 Sequence (one consistent order, everywhere)

When a fire complication is selected at READY (the plan is known there):

1. READY: commit once, capture baselines, `Begin` transaction, **suspend fuel**
   (only for a fire-selected plan; an ordinary fuel fault never suspends),
   set the ordinary deadline `T_ord = B + ordinary_added_delay`.
2. Announce the pump problem: HUD + commentary eid 56 (player) / 185 (AI).
   `VISIBLE` is the milestone.
3. On `VISIBLE`: sample the 1–2 s ignition delay. Compute
   `C_safe = dissipates_at`, `T_fire = max(T_ord, C_safe + inspection +
   fuel_remaining_upper_seconds)` using the **provider-certified bound**
   (`F1C_BROKER_PIT_VALID_FUEL_BOUND`). If the bound is invalid → suppress
   (`FIRE_SUPPRESSED_NO_FUEL_BOUND`); the nominal 12 L/s is never used.
4. Budget gate before ignition: `T_fire <= begin + max_lifetime`, effect
   handle obtainable, identity/state recheck. Any failure → suppress:
   resume fuel (if suspended), `End(T_ord)`, diagnostics
   `FIRE_SUPPRESSED_*`, best-effort ordinary pump message. Never rerolled,
   never ignites at a later stop.
5. `Deadline(T_fire)` → `EffectBegin` → `IGNITE` once + `FIRE IN THE PITS`.
6. Burn ends → `EffectStop` → wait `CLEARED` → **inspection** → `FuelResume`
   → remaining fuel service → `End(T_fire)` at release. The parent pump fault
   never emits `RECOVERED`/GO while the fire is active, and the car is never
   released before `C_safe`.

### 7.3 Timing

Ignition 1–2 s after `VISIBLE`; burn tri(4, 7, 12) s; dissipation provider-
bounded; inspection tri(2, 4, 7) s. All absolute times are session time; a
late tick never shortens a deadline; the never-shorten/supersede rules
protect longer native work.

### 7.4 Cancellation and cleanup

Per the 6.2 state machine: effect stop → wait CLEARED → inspection (if the
stop is a natural fire end) → fuel resume → `End(max(B, current_T))`.
Cancellation before ignition: resume → `End(T_ord)`. Crashed clients are
covered by lease-expiry cleanup; invalid clocks by the rules in 6.2.

### 7.5 Counters and reporting

`selected_parent_fuel_faults`, `fire_selected`, `fire_suppressed` (reason),
`fire_ignited`, `fire_extinguished`, `fire_resolved`. Selection is validated
with selected/parent faults; realised is ≤ selected and separate.

### 7.6 Callout

Fuel-problem audio + `FIRE IN THE PITS` / `FIRE OUT` HUD. No fire-specific
spoken clip is verified; none is claimed.

## 8. Player/AI acceptance contract (owner target)

Source: `docs/GAMEPLAY_ACCEPTANCE_2026-09-21.md`, indexed here as the
end-to-end acceptance target. Nothing in this section is evidence of
implementation.

### 8.1 Player cues (alpha restoration)

All cues use the presentation capability (6.3) and are driven by observed
state, never a fixed timer. Terminal stops never issue a false Go.

| Milestone (observed) | Cue | Declared eid | Gate | Unknown/blocked |
|---|---|---:|---|---|
| pit lane entry, `pit_status` 0/1→2 | Limiter On | 51 | status edge observed | — |
| box entry, `pit_status`→3 | Brake | 53 | status edge observed | clip/trigger mapping **unverified** → investigate prior alpha assets before assigning |
| safe service release, `pit_status` 3→4 | Go | 45 | **provider-confirmed release** (transaction release / game state), not a timer; suppressed on terminal outcome | — |
| pit exit, `pit_status` 4→0 | Limiter Off | 43 | only after a Go within the session window | — |

Fault presentation at the box (step 3 of the owner target): matching text,
correct engineer callout (eid 56 for the player), visible/audible behavior and
real consequences through the transaction/provider path; a log entry or a
delayed sound alone is not a working failure.

### 8.2 Development test-GUI and arming contract

`docs/TEST_GUI_CONTRACT.md` defines the frontend (implemented, disabled) and
the adapter contract: local HTTP 127.0.0.1:18743, `GET /v1/status` and
`POST /v1/commands` with `operation = force_failure_and_pit`,
`failure_probability_bp = 10000`, `scope = next_eligible_service`,
`ttl_seconds = 300`, deduplication by `(instance_id, request_id)`, receipt
states `accepted/armed/consumed/rejected/expired/cancelled`, bounded receipt
storage, and the backend transaction: **reserve the one-shot override first;
only then submit the broker call-to-box request; if either fails, remove the
unconsumed override and reject** ("a failure to reserve must never request
the pit stop"). The override arms exactly one future eligible service for the
same full identity, changes only the top-level selection gate to 100%, keeps
ordinary category weights, eligibility, durations, recovery and the
conditional fire chance, and expires after 300 wall-clock seconds or on
session/object change, invalid identity, unload or authoritative time
discontinuity. It never attaches to an already-committed service. Production
config is untouched; no seed search or identity change is permitted. The
adapter owns this endpoint; it is development-only and off in production.

### 8.3 AI acceptance

- Select an active AI (role `ai`; remote/retired excluded), arm its next
  eligible stop and request call-to-box for the same full identity; other
  drivers' probabilities and behavior are unchanged.
- `call_to_box` acknowledgement means the semantic request was accepted, not
  that the driver reached the pits.
- The spectated car's problem must visibly affect its service
  duration/behavior; narration must identify the correct subject
  (`DriverNameIDLookup` mapping; player first-person clips never used for AI;
  fuel fault uses eid 185).
- Recoverable outcome: correct computed delay, then normal rejoining.
- **Terminal outcome:** genuine retirement through the 6.6 provider, a
  driver-specific retirement announcement **after** `GARAGED`, and transfer to
  the pit/garage via the reviewed game-owned path. Until the provider and the
  narration mapping exist, this case is **blocked**, not passed by a semantic
  event or teleport. A recoverable acceptance test never has to retire; a
  100% generic fault chance does not mean 100% fire or terminal probability.

### 8.4 Evidence per acceptance test

Exact runtime/config hashes; session/vehicle/service identity; normal
baseline; chosen plan; override consumption; actual release/retirement state;
and a capture covering sound/text/vehicle behavior. Test normal and forced
modes, player and AI, next stop, cancellation/reset, simultaneous cars and no
unintended effect on the other car. Owner listening/viewing is required for
audio subject, cue timing and presentation acceptance.

## 9. Worked timelines

Session seconds. "Deadline" is the provider's current `stop_end_time` target;
`B` is the native baseline.

### 9.1 Calm stop (no fire; no suspension)

| Time | Event | Deadline | Fuel | Effect |
|---:|---|---|---|---|
| 100.0 | READY; Begin txn; B = 105 | 105 | normal | none |
| 100.1 | ordinary extra 2 → `Deadline(107)` | 107 | normal | none |
| 100.6 | Renew lease | 107 | normal | none |
| 107.0 | service end; `End(107)` | 107 | normal | none |

### 9.2 Fire timeline (consistent sequence; corrected assertions)

Fire selected; fuel suspended at READY (100.0). B = 105, `T_ord` = 107,
announce 100.5, VISIBLE 101.0, ignition delay 1.5 → ignition 102.5, burn 12 →
114.5, dissipation 2 → 116.5 (`C_safe`), inspection 4 → 120.5, certified fuel
bound 2.5 s → `T_fire = max(107, 116.5 + 4 + 2.5) = 123.0`. Lifetime 60 → cap
160; budget passes.

| Time | Event | Deadline | Fuel | Effect |
|---:|---|---|---|---|
| 100.0 | READY; Begin txn; suspend fuel; B = 105 | 105 | **suspended** | none |
| 100.1 | `Deadline(107)` (ordinary) | 107 | suspended | none |
| 100.5 | HUD announce + eid 56 | 107 | suspended | none |
| 101.0 | HUD VISIBLE (milestone) | 107 | suspended | none |
| 101.1 | compute `C_safe` 116.5, `T_fire` 123.0; budget OK | 107 | suspended | none |
| 101.2 | `Deadline(123)` (non-decreasing) | **123** | suspended | none |
| 102.5 | `EffectBegin`; IGNITE; HUD FIRE | 123 | suspended | EMITTING |
| 114.5 | `EffectStop` | 123 | suspended | DISSIPATING |
| 116.5 | `EffectStatus` → CLEARED (`C_safe`) | 123 | suspended | CLEARED |
| 116.6 | inspection begins | 123 | suspended | CLEARED |
| 120.5 | inspection ends; `FuelResume`; remaining fuel service | 123 | normal | CLEARED |
| 123.0 | `End(123)`; release | 123 | normal | CLEARED |

Assertions: deadline 107 **until 101.1**, then 123 from 101.2 (the previous
"stays 107 before ignition" claim was wrong); release never before `C_safe`
116.5 nor before 123; native baseline never shortened; fuel suspended only
100.0–120.5.

### 9.3 Suppression with unwind (post-suspension)

Fire selected → suspended at 100.0; HUD never reaches VISIBLE; timeout 3 s →
suppress at 104.0: `FuelResume`, `End(107)`, best-effort ordinary pump
message, diagnostics `FIRE_SUPPRESSED_PRESENTATION`; the ordinary pump fault
proceeds. No effect is ever created.

### 9.4 Lifetime-budget refusal (before ignition)

Announce 100.5, last-moment VISIBLE 103.5; max burn/dissipation/inspection and
bound give `T_fire = 131.0`; `max_lifetime = 30` → cap 130.0 → the budget gate
refuses **before ignition** (`BUDGET_EXCEEDED`), `End(107)`, no effect,
diagnostics `FIRE_SUPPRESSED_BUDGET`.

### 9.5 Lease expiry drives cleanup (still-registered crashed client)

Client vanishes after ignition (102.5) without renewing; lease expires 105.0.
The provider autonomously stops emission at 105.0, waits `CLEARED` (declared
dissipation; 107.0), resumes fuel, leaves the deadline at 123 and releases at
123 with no client call. No indefinite hold; no stranded suspension.

### 9.6 Authoritative clock invalid during emission

NaN at 109.0: emission stops immediately (provider-internal, no game write);
policy session invalidated. The clock stays invalid beyond the 5 s wall
budget → `INVALIDATED`: no deadline writes, no fuel resume into a replaced
car, resources considered gone with the session; a new epoch is required.
Never waits on a dead session clock.

### 9.7 Normal deadline changes during a fire

At 108.0 the game writes 130 (> target 123): the provider reports
`SUPERSEDED` and stops writing; the car leaves at 130, after `C_safe` 116.5.

### 9.8 End while emitting (preflight refusal)

At 110.0 the adapter calls `End(107)` while the effect is EMITTING: preflight
returns `EFFECT_ACTIVE` with **no** deadline reduction, **no** fuel resume and
no state change. The adapter must `EffectStop`, wait CLEARED, then `End`.

### 9.9 Late tick; suppression-free cancellation

A tick at 130.0 (past 123 and the lease) performs no writes; `End(123)` is
idempotent; diagnostics `LATE_TICK`.

## 10. Acceptance tests

### 10.1 Offline policy and fake provider

As revision 3 (exhaustive conditional draws, masking, stream isolation,
selection rate with parent-fuel denominator, suppression counters, exactly-once
fire lifecycle, fake-provider replay of every timeline in Section 9 with
deadline/fuel/effect assertions at each step).

### 10.2 SDK provider mocks

- **Pit-event cursor (G1 cleanups):** zero sentinel normalization (no
  `oldest − 0` loss); two one-record polls without duplicate/skip; zero
  capacity with valid and invalid version; empty ring; **all-stale scan
  advances `next_sequence` past examined records and cannot loop**; overflow
  counters; stale-generation skipping; undersized size/stride; canaries; old/
  new provider compatibility matrix.
- **Observation batch (G4):** future and stale car samples (whole-batch
  retry); interleaved cars with exact sequence equality; authoritative
  NaN/Inf/backwards; delayed READY; ACTIVE late attach; duplicate READY; slot
  reuse; reset while fuel is suspended.
- **Transaction cleanup (G2):** every transition-table row, including
  `End`/active-effect leaving resources unchanged, `End` cap/native
  validations, lease-expiry autonomous cleanup, invalid-clock cleanup with and
  without a live session, destroyed/reset invalidation without writes to a
  replaced car, suppression unwinds, `SUPERSEDED`, crashed-client and
  unregister paths, and late ticks.
- **Presentation:** HUD VISIBLE/overwritten, commentary enqueue/dedupe,
  two-car arbitration, purge, stale acknowledgement.
- **Effects (gated):** stop vs clearance, no shared-descriptor corruption, no
  engine/fuel modification, RNG policy.
- **Retirement (gated):** request → authoritative `GARAGED` status; narration
  only after `GARAGED`; failure leaves the car a participant; cancel on
  session change; no raw writes or teleport.

### 10.3 Adapter/arming tests (GUI contract)

The `TEST_GUI_CONTRACT.md` list: real roster identity matching and slot-reuse
refusal; arm+pit acceptance and every partial failure with no orphan arm or
unrequested pit-in; 100% top-level gate with ordinary conditional weights and
untouched duration/subtype/fire streams; one-shot consumption, duplicate
request, response loss, reconnect, expiry, reset/unload, already-committed
service; truthful reporting of missing native providers; end-to-end guarded
gameplay (blocked until the runtime adapter exists).

### 10.4 Cue tests (player and AI)

Limiter On/Brake/Go/Limiter Off at their observed milestones; Go only on
provider-confirmed release; no Go on terminal; Limiter Off gating; AI fault
narration identifies the correct subject (never a player clip); retirement
narration only after authoritative `GARAGED` and only with a verified
driver-specific clip.

### 10.5 Guarded game validation (Phase I, owner present; not authorized)

As revision 3, plus the Section 8.4 evidence collection (hashes, identity,
baseline, plan, override consumption, release/retirement state, capture) and
owner listening/viewing.

## 11. Blockers

1. **Six missing broker capabilities** (6.1–6.6, with 6.4/6.5/6.6 gated); the
   runtime adapter cannot start without them and has no raw fallback.
2. **Fire-effect ownership evidence absent**; whole-descriptor restore
   forbidden.
3. **No proven per-car fuel-suspension mechanism**; fire suppressed if
   unproven.
4. **No AI retirement/garage path**; the owner's AI terminal acceptance case
   stays blocked until 6.6 exists and is reviewed. No semantic-event pass.
5. **Retirement narration mapping unverified** (driver-specific clip/subject)
   and **Brake clip/trigger unverified**; investigate prior alpha
   assets/catalogue before assigning identifiers.
6. **Presentation limits**: no audio-start observation; fire depends on HUD
   VISIBLE; native HUD acknowledgement/arbitration must be proven.
7. **Broker ABI divergence** (11/13/15, revision-1 capture) with checkpoint
   drift; rebuild the negotiation matrix at Phase I.
8. **Clean-test install missing / guarded weekend not authorized.**

## 12. Implementation sequence

1. Specification review (this resubmission) — no code.
2. Separately authorized offline work (no live effects required): policy v3
   fire extension + fake provider + acceptance tests; scoped SDK provider
   investigations for 6.1–6.6 with focused tests; alpha asset/catalogue
   investigation for the Brake and retirement narration mappings.
3. Reviewed **runtime adapter** (Phase H) against the accepted capabilities.
4. Guarded game validation (Phase I) with the owner present.

No review has authorized steps 2–4; this specification does not either.

## 13. Evidence index

Revision-1/2/3 evidence remains in `verification/pit-integration-2026-09-21/`,
`verification/pit-integration-r2-2026-09-21/`,
`verification/pit-integration-r3-2026-09-21/`, and
`verification/pit-integration-r4-2026-09-21/` (unchanged). Revision 4 adds
`verification/pit-integration-r4-2026-09-21/`:

- `REVISION_R4.md` — G1/G2 amendments, acceptance incorporation, hashes.
- `workspace-validator.txt` — final validator result.

The isolated mock implementation (review-r4 handoff) adds:

- `mock/pit_mock_contract.h`, `mock/pit_mock_provider.c`,
  `mock/pit_mock_tests.c` — compilable contract declarations, deterministic
  provider and executable tests (184 checks).
- `build-mock.ps1`, `test-mock.ps1`, `tools/mock_trace_check.py` — build/run
  scripts and the independent trace-invariant checker.
- `verification/mock-contract-2026-09-21/` — test logs, JSONL traces for both
  architectures, trace-check result, source/artifact hashes and the report.

No SDK, game, policy, config or binary file was modified. The fire addendum
(1EC7918DADEE6F5789952F460B9630825AB5F5CD184D6FF6D6849AD832883A90) and all
review directories are untouched.

## 14. Revision log (revision-3 review items)

| Item | Disposition |
|---|---|
| G1: normalize zero sentinel before dropped | 6.1 cursor rule 1: sentinel maps to `oldest_retained` before the dropped computation; zero-sentinel example shows zero loss |
| G1: all-stale scans must progress | 6.1 cursor rule 2: `next_sequence` advances past every examined record; `skipped_stale` counts them; all-stale example added |
| G1: add all-stale, zero-capacity/empty-ring cases | 6.1 example table and 10.2 mock list |
| G2.1: End must preflight before side effects | 6.2 End preflight steps 1–4; `EFFECT_ACTIVE` leaves everything unchanged; End validates lifetime/native invariants (cannot bypass the cap) |
| G2.2: lease expiry drives cleanup; dead clock; destroyed/reset separation | 6.2 lease-expiry autonomous cleanup; invalid-clock rules with wall-clock budget and `INVALIDATED`; identity-mismatch separation; fire disabled if the provider cannot guarantee bounded cleanup |
| G2.3: consistent timeline | 7.2 sequence fixed (suspend at READY only for fire-selected plans; resume after inspection); 9.2 timeline rewritten; assertion corrected (deadline 107 until 101.1, 123 from 101.2) |
| G2.4: no unproven fuel estimate | 6.1 `fuel_remaining_upper_seconds` with `VALID_FUEL_BOUND`; 7.2 uses only the certified bound, suppresses otherwise; 12 L/s documented as context only |
| G2 evidence request | 6.2 transition table with guards/actions/exits, crashed-client row, and model assertions (to be executed by a later authorized mock) |
| Owner acceptance | 8.1 player cues with gates and unknowns; 8.2 GUI/arming contract; 8.3 AI narration and terminal acceptance (blocked until 6.6 + narration mapping); 8.4 evidence per test; 10.4 cue tests; blockers 4–5 |
| Retirement provider | 6.6 new capability bit 19, authoritative `GARAGED` before narration, no raw writes/teleport; summary and mapping updated |
| Count cleanup | Six capability bits (14–19) because the owner added the required retirement provider |
| Mock implementation (review-r4 handoff) | `mock/` now implements the compilable contract (`mock/pit_mock_contract.h`) with the two corrections (C1 invalid-clock cleanup obligation; C2 suppression preserves remaining work), executable transition/timeline/cursor/GUI/cue/retirement checks, deterministic JSONL traces, an independent trace checker, and dual-architecture runs; retirement function renamed `F1CBrokerRetirementGetStatus` and the all-stale cursor fixture made consistent |

Reviewed revision 3:
40E29D680FCBFAEA260D762B7153274DD8677AD2E00929E9F2B34C57B786390A.
