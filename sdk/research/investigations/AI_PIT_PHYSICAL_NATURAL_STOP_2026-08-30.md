# Passive natural-AI pit physical-state capture — 2026-08-30

## Scope and guardrails

This bounded, passive observation used only `python tools/f1c_mod.py runtime-correlate` against `F1 Challenge Online.exe`. Nine independent named-recipe monitors ran for 600 seconds at 5 Hz in owner-authorized hidden elevated PowerShell because normal process opening is denied. They performed no memory write or input and did not change assets, restart the game, force a pit stop, or force damage. Every raw artifact identifies game PID 4836.

The tool-created raw artifacts are append-only under `runtime_memory/observations/`. All nine saw 22 active slots with zero unreadable reads. The compact, machine-readable evidence and SHA-256 integrity map is `runtime_memory/observations/ai_pit_physical_20260830_summary.json`.

## Field selection and evidence boundaries

| Field | Offset | Treatment in this report |
| --- | ---: | --- |
| `tirePuncture_RR`, `RL`, `FR`, `FL` | `0x363C`, `0x3734`, `0x382C`, `0x3924` | Historical live evidence identifies these floats as remaining tread (`1.0` fresh); raw canonical names are retained. |
| `tireChoice` | `0x397C` | Confirmed `int32`, but only a track-local compound-list index. |
| `nextChosenTires__rw_` | `0x3378` | Confirmed raw field; semantics unresolved. |
| `FrontWingIntactFlag`, `RearWingIntactFlag` | `0x0D14`, `0x0D58` | Confirmed byte fields. Unchanged values imply no repair claim. |
| `carConditions` | `0x3D6C` | Confirmed field; no part-level meaning is assigned. |
| `PitStatus` | `0x3450` | This capture independently observed one full `0->1->2->3->4->0` path. |
| `PitTyreChangeFlag`, `PitWingChangeFlag`, `PitRepairFlag` | `0x3490`, `0x3494`, `0x3498` | Source-derived candidate labels; only sampled values are reported. |
| `PitTargetFuel`, `PitVisitCounterCandidate` | `0x3484`, `0x406C` | Source-derived candidate labels; only sampled values are reported. |

## One observed natural stop (slot 16)

| Boundary | Time from monitor start (s) | Observed result |
| --- | ---: | --- |
| `0 -> 1` | 355.6 | All tread values near 0.9966–0.9974; `tireChoice=1`. |
| `1 -> 2` | 446.1–447.1 | Tread 0.9963–0.9971; `PitTargetFuel=49.0`. |
| `2 -> 3` | 457.8–458.7 | RR/RL/FR/FL were all `1.0`; `PitTargetFuel=135.0`. |
| `3 -> 4` | 467.0–468.0 | All four remained `1.0`; job/timing trace duration was 9.25 seconds. |
| `4 -> 0` | 471.2–472.2 | Tread was 0.9999928–0.9999959. |

The small timestamp spread is from separately launched monitors, not a second stop.

### Tyre observation

| Corner | At `1 -> 2` | At `2 -> 3` | Observed rise |
| --- | ---: | ---: | ---: |
| RR | 0.9962950349 | 1.0 | +0.0037049651 |
| RL | 0.9971069694 | 1.0 | +0.0028930306 |
| FR | 0.9964759350 | 1.0 | +0.0035240650 |
| FL | 0.9968553185 | 1.0 | +0.0031446815 |

This is strong direct evidence of a tyre-refresh outcome in this natural stop. Each tread value was contextual rather than an independent trigger, so its exact update point is bounded by 5 Hz samples; no job ordering or general rule follows.

`tireChoice` stayed `1` at every captured boundary and never triggered independently. It therefore neither identifies a physical compound nor rules out a same-index replacement. `nextChosenTires__rw_` stayed the raw float `2.802596928649634e-45`; no compound-intent name is justified.

### Damage/repair co-occurrence

`FrontWingIntactFlag` was `0` at states 1 and 2, then `1` at states 3, 4, and 0. `PitRepairFlag` was `1` at every boundary, but did not independently trigger. This is a single observed co-occurrence in the sampled state-2-to-3 interval, not proof that the candidate flag caused the front-wing change or a universal repair recipe.

`RearWingIntactFlag` stayed `1`; `PitWingChangeFlag` stayed `0`; neither independently triggered. This trace contains no observed rear-wing change. `carConditions` was `1` through state 2 and `0` from state 3. `PitVisitCounterCandidate` changed `0->1` at the sampled exit (`4->0`). These are raw value/timing observations only.

The tyre and repair candidate flags were `1` at each boundary; the wing candidate flag was `0`. `PitTargetFuel` changed from sampled `49.0` before `2->3` to `135.0` at and after it. None of these source-derived candidate labels becomes causal evidence from this one trace.

## Artifact record and limitations

The summary sidecar lists the nine raw artifact paths and SHA-256 digests. A first, non-mutating attempt to pass multiple comma-separated values to `--context`/`--also-trigger` was rejected by front-door validation because each option accepts one named recipe. It emitted no artifact; the completed capture used isolated one-field traces.

- One completed AI stop does not establish frequency, all compounds, repair ordering, or field causality.
- No natural rear-wing change or independent `tireChoice` transition occurred in this window.
- A future owner-approved passive trace can make one tread or wing field the single secondary trigger to narrow the update time. It must retain this read-only front door.
- No game, production, canonical SDK, or governance document was edited. New state is limited to this additive investigation, its additive recipe, and tool-created observations/summary.
