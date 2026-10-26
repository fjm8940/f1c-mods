# Latest Silverstone capture review

Read-only review requested by Matias. Source: Documents/F1 Challenge V10/RaceData/SINGLE_Silverstone, 2002_20260914_193842-24152-1_20260914_193842.json. Exported 2026-09-14 (22:, collector0.2.1. The earlier file's write timestamp changed through formatting, so embedded capture/export identity was checked as well as filesystem recency. Source file unchanged.

Native output facts: 12 scheduled laps,20% race distance,24 classified competitors,15 finishers and9 DNFs. Kimi Raikkonen P1 20:09.620; Michael Schumacher P2 20:12.334; Juan Pablo Montoya P3 20:13.813. Montoya final best lap1:37.192; its lap number is unknown (0). Matias classified P23, DNF,0 completed laps, player=true. These are the collector's observed classification, not independently verified official results.

Capture quality: final=true but completeHistory=false;1014 samples,195 aggregate sampling-gap/missing-crossing counts, lostRecords0, lateJoin=false. Only26 completed-lap observations exist across the field. This is not a complete lap-by-lap race record. The gap count combines more than one kind of missing observation; it must not be described as195 separate polling outages.

SC/RF:12 state/ownership observations. Reported SC deploying at24.226 game seconds, pacing at105.210, then green at113.280. No in_this_lap/leader_restart transition observed; no complete normal recall cycle established. RF phase0 throughout recorded observations, request0, no observed RF takeover. Legacy phase mappings retain their unverified publisher-attribution caveat. Bernd is absent from all24 final competitor entries, supporting the exclusion path for this capture.

Verified defects: simulation clock reaches1,000,000,002.197 seconds. A final control observation records1,000,000,000. Coulthard's pit visit spans113.079 to1,000,000,000.148, yielding999,999,887.069 seconds; serviceObserved=false and duration=null. The collector is allowing a non-racing time domain to form an ordinary pit interval. Session elapsed-time text consequently becomes '-'. Fix by separating native finalization clock/sentinel transitions from elapsed race time and incomplete visits, while retaining final classification observations. Native accelerated-results/exit handling is a plausible explanation for the simultaneous classification completion and time jump, not proven by this JSON alone.

There are8957 tyre_condition_changed events. Sampled continuously changing raw wheel values produce excessive event noise and dominate the file. Do not infer8957 punctures or tyre changes. Revisit field semantics and record meaningful discrete transitions or lower-rate telemetry. The JSON is valid readable multiline output (92,320 lines,2,574,599 bytes), but formatting does not solve event overproduction.

One neutral hit-counter incident: Fernando Alonso at17.813seconds, hitsAdded4; other participant and blame unknown. Weather shows racing-path wetness0.193700→0.0034835 (19.37%→0.35%); rain intensity unknown. Final native fuel values remain provisional.

Decision: retain file as native evidence; use classification with qualifications, not a full event narrative. Prioritize sentinel-clock/pit handling, tyre-event semantics/volume, and observed-versus-simulated lap completeness. No implementation or installed-game changes made in this review. Native field/performance/lifecycle acceptance remains incomplete.

Documentation indexed through component README; existing README before-state preserved under workspace_organization_audit/SILVERSTONE_REVIEW_BEFORE_*. Root validator retains ten unrelated baseline errors. Wasted effort: first extraction printed too much metadata/control detail and was narrowed; no builds, retries or idle waits.
