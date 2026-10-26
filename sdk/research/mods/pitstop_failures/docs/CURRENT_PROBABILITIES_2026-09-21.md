# Current probability and outcome audit — 2026-09-21

Source-reviewed facts, no configuration or runtime changes. Applies to the live
development prototype, not a release or a claim of real-world F1 statistics.
Sources: live/policy_config.h (embedded preset), live/policy_dev.c (selection,
duration and timing), live/session_dev.c (events), live/live_client.cpp (context
and effects), live/pit_live_timing.h (eligibility). External INI values currently
match the embedded preset; editing the INI alone does not update the loaded DLL.

Normal eligible stop, neutral context:

| Outcome | Per-stop probability | Duration min / mode / max, seconds | Result |
|---|---:|---|---|
| Slow corner | 2.80% | 1 / 2.5 / 5 | Recover |
| Wheel gun | 1.00% | 3 / 5 / 10 | Recover |
| Fuel slow flow | 0.63% | 3 / 6 / 12 | Recover |
| Fuel temporary no flow | 0.18% | 4 / 8 / 16 | Recover |
| Fuel interrupted flow | 0.09% | 3 / 6 / 12 | Recover |
| Stall | 0.20% | 2 / 4 / 8 | Recover |
| Cross-thread | 0.10% | 10 / 18 / 35 | Recover |
| Terminal wheel | 0.01% | 8 / 12 / 20 diagnosis | AI DNF/garage |
| Clean | 94.99% | none | Normal work |

Fuel's parent probability is 0.90%; conditional subtype weights are 70/20/10.
Total trouble 5.01%, recoverable 5.00%. One category draw per committed service;
no escalation roll from recoverable to terminal, no per-frame draw. All wheel
outcomes select FL/FR/RL/RR equally (25% each). Duration is continuous triangular:
mode is the density peak, not a guaranteed or discrete-duration bucket.

Slow-corner/stall plans have one recovery stage; wheelgun/fuel/cross-thread two
(intermediate attempt halfway through sampled duration, recovery at the end).
These are fixed scheduled events, not probabilistic retry successes. Terminal
has no recovery. Tyre/fuel time extends the affected parallel job, so added stop
time can be smaller or zero if other work takes longer. Stall adds its sampled
duration to the normal overall deadline. AI actual engine shutdown starts at
READY, with restart driven by the scheduled recovery event; it does not mirror
the policy's post-work timing description exactly.

Tyre faults require tyre work, fuel faults require positive refuelling, stall
requires a real service and possible release. Ineligible probability becomes
clean, not redistributed. Tyre-only eligible service: trouble4.11%, clean95.89%.
Fuel-only with release: trouble1.10%, clean98.90%. Invalid/unknown native job
timing refuses injection rather than making up timing.

Configured recoverable multipliers: crew100%, late request+15 points, late
change+15, wet+10, poor positioning+25. All four flags yield165% of base risk
(recoverable8.25% plus terminal0.01%). Recoverable cap12%; terminal never scales.
The live client creates zeroed context and only fills eligibility: none of the
four pressure/weather flags are supplied, and there are no current per-team,
fatigue, repeat-failure or championship-position adjustments.

GUI exact-category tests override selection to100% on eligible service while
preserving corner/subtype/duration draws. Normal unarmed eligible stops use the
base policy. Seed20260921 plus epoch/vehicle/service identity gives reproducible
separate category/duration streams; it is not an escalating chance after clean
stops. No automatic per-race minimum or maximum failure quota exists.

Runtime limits: terminal mechanical retirement supports AI only; player terminal
is unavailable without reroll. AI stall has native stop/restart; player stall
is delay/presentation. Fuel subtypes have delay/presentation but no owned native
fuel-flow suspension. Fire is unimplemented (effective0%); the proposed fire
addendum is not part of this loaded policy.

Verification: inspected the named source paths and matched INI/embedded values.
No new tests required for this read-only audit; no gameplay or settings changed.
Pending: full gameplay matrix, pressure input integration, fire/fuel ownership,
player mechanical outcomes. Product remains implement/prototype.
Wasted-effort check: one oversized search output and a mistyped numeric
Select-Object argument corrected; no builds or polling performed.
