# Web Telemetry native acceptance review - 2026-10-09

Outcome: **capture works; engineering comparison fails acceptance**. Owner requested live verification. Product remains a private verification candidate. No code/binary changes, hot replacement, promotion or publication.

## Target and observed success

Installed 4.2.0-rc.1 DLL SHA-256 22DEB680422DE43E2986647F4C3EA9C0158D28B7A004EDCBF8C8708A9BEB67C7. LightLoader.log confirms 12 plugins loaded, zero failed, including telemetry. Independent SDK read-only memory reader identifies PID 14732, canonical EXE, race session 7 / phase 5, 22 cars, 2002_Hockenheim.

Health series from 08:07:31.106 to 08:08:01.510 UTC (00-00-00-00-00-00 local) advanced sequence 4168 -> 4735, approximately 18.65 Hz. Maximum sampled age 94 ms; zero reported failed captures; HTTP p95 about 27.5 ms. This establishes freshness, not native callback CPU cost or FPS. No crash observed during this bounded check; general stability is unproven.

Both manually selected AI cars recorded changing speed, throttle, brake, steering, acceleration, yaw/yaw-rate and tread. JSON sample counts matched serialized arrays. Browser rendered Engineering after tab reopening and Load, with no observed console warnings/errors. Comparison arithmetic equals elapsed_B minus elapsed_A to rounding (~0.001 s), but its reference coordinates are invalid.

## Defects requiring correction

| Priority | Finding | Reproduction / evidence | Required repair |
|---|---|---|---|
| P1 | Wrong AI distance source | web_telemetry.c:423 defines OFF_TRACK_PART_EXACT=0x4048; :3052 reads it for all cars. Existing FACT `f1c-lapdist-4044-not-4048` (2026-07-31) corrects the per-car field to 0x4044. Live AI distance stalls/jumps; comparison has only 15 valid delta points out of 1000. | Independently validate current target +0x4044, correct all distance consumers and ledger, add regressions shaped like this live capture. Older player-only 0x4048 proof does not establish AI validity. |
| P1 | Car dropdowns do not select recorder | Browser A=Rubens/B=Schumacher leaves server A=-1/B=1. engPopulate onchange only changes ENG_SEL; engLoad fetches fixed trace sides. No UI call to /trace/select. | Connect selection to recorder with identity/generation guards; populate laps for the actual selected car. Otherwise the displayed dropdown can identify another car's trace. |
| P1 | Completed lap choices do not refresh | Browser remains at no completed laps after server records lap 1. Leaving/reopening Engineering reveals it. engRefresh is called only by Engineering initialization. | Refresh status while open and preserve valid selections; test a live crossing without switching tabs. |
| P2 | AI identity frozen before activation | Schumacher telemetry a=1, controller_role=1; independent reader AI control=1; trace contains steering, but export ai=0 and browser labels him player physics. trace_update assigns is_ai only at initialization. | Use reviewed current physics/controller identity, split traces on relevant changes, avoid equating an old control flag with current physics mode. |
| P1 | Partial first lap marked COMPLETE | Rubens manually selected at sim 162.454, final sample 231.551 (69.097 s), but export lap_time=105.223 and flags=5 (COMPLETE plus BREAKS). A +1 increment alone marks complete. | Require witnessed full-lap baseline/coverage; keep first partial lap incomplete and exclude it as an elapsed-time reference. Current breaks incidentally exclude it from clean statistics; distance repair alone must not remove this protection. |
| P2 / open cause | Team identity needs validation | Unrelated BAR, McLaren and Renault descriptions share hash 2166136261 (the FNV empty-string basis); selection matches any equal nonzero hash. | Trace roster-string producer and missing identity handling. Cross-team equality observed; empty-string origin is inference, not newly proven. |

## Durable live excerpts

- A: slot 2, Rubens Barrichello, Ferrari, ai=1; lap 1, 1315 samples, start_sim=162.454, lap_time=105.223, flags=5. First distance 6781.3 m; last 2.3 m. Speed range 25.25-86.35 m/s. 17 adjacent raw gaps meet reverse/teleport/time-break rules; grid reports 1300 unusable intervals.
- B: slot 1, Michael Schumacher, McLaren, ai=0 (incorrect metadata); lap 1, 1978 samples, start_sim=125.892, lap_time=105.143, flags=5. First and last distance 2.3 m; early jump 6779 m in 0.048 s. Speed range 23.14-86.43 m/s. 17 adjacent raw gaps; grid reports 1960 unusable intervals.
- Compare requested 5 m step, returned step 6.79 m / 1000 points (actual TRACE_GRID_MAX is 1000, despite ledger saying 1500). Only 15 valid deltas, range -0.661 to -0.457 s. No trustworthy corner/pace conclusion follows.
- The native player Matthias Steppe is stopped, finish=3 / DSQ, confirmed independently and in the dashboard incident. Automatic player selection removes him. This prevents player-vs-AI acceptance in this session; it is not attributed to telemetry.

Bulk captures are disposable under `%TEMP%/f1c-web-telemetry-live-20261009`: health_series.json, A_lap1.json, B_lap1.json, compare_lap1.json and status/telemetry samples. Material excerpts above remain durable if bulk is quarantined. Channel dynamics demonstrate changing observations, not new proof of every unit/frame/wheel label.

## Changes, limits and restoration

Used GET /trace/select?which=A&car=2 solely to select recorder A for AI-vs-AI diagnosis. This changes telemetry selection, not game memory. B remained automatic slot 1. Browser selected the matching names and loaded lap 1. Restore a desired slot explicitly; game restart reinitializes automatic defaults. No code or DLL changed. Existing installation restoration remains in LIVE_CHECK_2026-10-09.md and its installation manifest.

Pending: fixes and native revalidation, player driving channels, independent current-target corrected-distance checks, callback/FPS overhead, repeat circuit/session entry, tyre replacement, imported-run roundtrip, real two-PC Tailscale, and owner presentation acceptance. Narrow browser layout also crowds Engineering controls beneath the large focus panel; wide/responsive acceptance is pending.

README indexes this review; its before-state is under workspace_organization_audit/WEB_TELEMETRY_LIVE_ACCEPTANCE_BEFORE_2026-10-09. Canonical findings retain these results without rewriting older evidence. Central product status remains unchanged.

Wasted-effort check: no builds or broad tests. First combined shell command rejected by policy; simpler read commands succeeded. Browser creation timed out once; retry worked. A role locator failed; fresh AX indexed clicks worked. One documentation command failed parsing before mutation and was corrected. The 30 s health series was a bounded measurement, not idle polling. Initial no-record concern was corrected after checking lap-0 gate and player DSQ.
