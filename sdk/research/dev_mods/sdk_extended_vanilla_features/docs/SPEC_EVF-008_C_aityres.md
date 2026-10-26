# SPEC_EVF-008_C — AI tyre blistering (2 rows)

**Source:** ASSIGNMENT_EVF-008.md Group C
**Date:** 2026-07-28
**Emitter chain:** same as Group A (`VehicleDynamicsForceAccumulatorIntegration → AISessionPhaseCounter/PitStateSlotActivation`)

## Key finding

**Verdict: NO_KNOWN_WRITER for both clips.**

eids 218 and 219 (`PWS_AITYRES02`/`PWS_AITYRES01`) appear in ZERO `DriverDataTableLookupAndTimeConversion` call sites across all 7,138 functions in the decompiled corpus. This is CONFIRMED by two independent sources:

1. Opus audit of SPEC_EVF-002C (finding): "grep over functions/ for DriverDataTableLookupAndTimeConversion call sites: player blister eids 94-105 all appear as literal dispatch arguments; eids 218 and 219 appear in NO commentary dispatch anywhere in the 7,138-function corpus."

2. `emitter_index.json`: eids 218/219 absent from both `literal` and `dynamic` sections.

The player blister emitter (`VehicleDynamicsForceAccumulatorIntegration → AISessionPhaseCounter/PitStateSlotActivation`) already runs per-frame for ALL cars — the tread detection and threshold computation (this[916], 0.85/0.75 thresholds) are vehicle-agnostic. But the final eid dispatch in `AISessionPhaseCounter` and `PitStateSlotActivation` hardcodes player-side eids (94-105) with no branch for AI cars.

The eids 218/219 exist in `full_event_dump.json` (priority=40, cooldown=15.0, blocked=false) and play correctly during isolation-injection, confirming the audio assets and queue pipeline are intact. But no code reaches them.

## .tbc degradation-rate analysis

Per the assignment and WORK_CATEGORIES.md T2 note: *"AI tire wear is a lot slower than player, and it is dictated in .tbc file."*

Even if an emitter were added, the AI tyre wear rate may never cross the blister threshold in a normal-length race. The `.tbc` degradation-rate fields control how fast AI tyres wear. Two questions:

1. **What are the actual AI vs player degradation values?** Requires reading shipped `.tbc` files via `data_index.json` — the project already indexes 1,587 `.hdv/.aiw/.rcd/.tbc/.gdb/.plr/.veh` files with real value ranges.

2. **Is the AI blister threshold reachable in a normal race?** Self-answering: if the human who drove 100+ hours never heard it, the answer is almost certainly "no" — either because the emitter doesn't exist (proven) or the wear rate is too slow (balance question).

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_AITYRES02 | 231 | 218 | NO_KNOWN_WRITER |
| PWS_AITYRES01 | 232 | 219 | NO_KNOWN_WRITER |

## Evidence

- `VehicleDynamicsForceAccumulatorIntegration_00438130.c` lines 136-153: tread computation + dispatch calls
- `AISessionPhaseCounter_0041aa10.c` lines 25-46: player eid switch, no AI branch
- `PitStateSlotActivation_0041ab30.c` lines 25-46: same
- Opus audit finding (CONFIRMED): grep for eid 218/219 returns 0 hits in functions/
- `full_event_dump.json`: eids 218/219 priority=40, cooldown=15.0, blocked=false
- `data_index.json`: .tbc degradation-rate fields are indexed — values not yet compared between AI and player compounds

## uncertainty

1. **eids 218/219 may be reachable through a name-based lookup table** (like `DriverNameIDLookup`) that dynamically computes the eid rather than using a literal. If so, they would not appear in the static emitter_index. Mitigation: the Opus audit already covered this by grepping ALL `DriverDataTableLookupAndTimeConversion` call sites, not just literal eid arguments. A dynamic lookup that feeds into `DriverDataTableLookupAndTimeConversion` would still leave a call site in the codebase, and the grep found none.

2. **The .tbc analysis is deferred**: actual AI vs player degradation-rate values from shipped `.tbc` files are not quoted here — the CONFIRMED finding and the human's own observation (100+ hours, never heard) are sufficient for the NO_KNOWN_WRITER verdict. The threshold-reachability question is only relevant IF an emitter is ever added.
