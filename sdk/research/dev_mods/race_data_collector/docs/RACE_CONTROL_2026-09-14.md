# Safety Car / Red Flag integration — 0.2.0 local trial

Owner requested mod integration in the already-installed collector. Scope: read published mod state, keep Safety Car out of competitor classification, protect finalization during RF takeover, link restart segments, include evidence in the normal Narrative GUI, and upgrade the installed collector. No SC/RF gameplay code, controls, license handling or configuration changes. Product remains verify/candidate; native and human validation pending.

## Reviewed interfaces and limits

The canonical SDK f1c_race_control.h ABI1 defines a mapping scoped by game PID and process creation time. Its owner/request/registered/SC generation/SC vehicle fields are read with OpenFileMapping(FILE_MAP_READ), never created or acquired by the collector. SC ownership is a control lease even during normal green running; it is explicitly not a deployment signal. Matching generation plus exact published vehicle pointer identifies the SC; no name heuristic or gameplay memory write is used. That identity is retained for the same broker object identity through temporary mapping/lease changes, and cleared on a new session generation.

Existing SC PaceCtl telemetry is 444 bytes, magic PACE, ABI1/size at offsets436/440. Reviewed source: full_safety_car/trials/v0.10.51_startup_integrity/safety_car_guardrails.c, PaceCtl and CT_OFF..CT_RESTART; independently matches tools/safety_car_status.py. Reported phases: green, deploying, pacing, in_this_lap, leader_restart. Vehicle phase, caution laps, governed count, slot and heartbeat accompany them. Heartbeat advancing/unchanged is reported; unchanged never proves idle or advancing gameplay.

RF monitor layout is the first27 aligned fields (108 bytes) of RFMonShared in red_flag/trials/2026-09-14/messages-v83/redflag.input.hardened.c. Sequence phases0/1/2/3/10, raw stop reason and incident-car count are collected. Phase10 is the player's pit-box wait; phase2 awaits native restart/grid and phase3 restores the post-restart field. Raw codes stay raw in the JSON rather than guessing causes.

Both legacy phase mappings are session-wide and contain no owner PID; RF monitor has no heartbeat or ABI header. Registration in the process-scoped coordinator and loaded-module presence gate reads, and two identical byte copies reduce torn snapshots, but do not prove publisher attribution or multiword atomicity. Every legacy phase observation is labelled with this limitation. Multiple simultaneous game processes remain an attribution risk. Wrong SC ABI/size/magic and unknown RF phase are unavailable, not zero. Reader work occurs off the game callback; no new hook, command or mod DLL is loaded.

## Output and lifecycle

The 41 existing sections remain; a 42nd `raceControl` section contains latest source state, sampled transition events and limitations. Events also appear in keyEvents and the append-only journal, with game time, leader lap, pause flag and monotonic source-read time. They are observations, not exact event timestamps or an authoritative incident/blame feed. Source observations are read after the queued native frame, not atomically with it.

After the process-scoped SC identity becomes known, its vehicle is excluded from standings/grid/qualifying/lap summaries, current ordering and completion checks. If identity becomes available late, earlier raw observations may already contain it; native early-start testing remains required. No SC-only field can count as a finished race.

RF owner or takeover request blocks a final export, including native result boundaries. Mod transitions can be recorded while paused or on the same simulation snapshot without inventing lap progress. A native generation/reset observed during RF control closes the previous segment as incomplete and attaches previousCaptureId/continuationReason to the next segment. Linked segments are marked incomplete-history. They are not automatically merged into a fabricated whole-race classification; a complete cross-restart reconstruction remains future work. If an entire transition falls between samples or occurs while native frames are unavailable, it may not be observed.

Season Narrative's installed bridge now includes these events and source caveats in its prompt context. It explains linked segments rather than silently presenting the restarted portion as complete history. The standard gui.py integration remains in place.

## Verification and installation

Exact candidate: [build_20260914_222402_567](../trials/build_20260914_222402_567/), DLL SHA256 `fee924aada1e412c4db5afa855f97047f249987e678f1518e792525fa09ec21d`.

- Core tests pass: missing/bad SC/RF buffers, source bounds, observed transitions, pause without sample inflation, SC exclusion and completion, RF takeover blocking, restart-link completeness, inherited collector regressions.
- [Actual Windows mapping test](../trials/build_20260914_222402_567/race-control-mapping.log) passes: absent mapping not created, exact test-process mapping read, invalid ABI/owner rejected, owner and all bytes unchanged, absent provider unavailable. The fixture owns only its own process mapping; no live mod state touched.
- DLL negative-host guard and strict JSON checks pass. Actual Narrative parser/report tests pass with the new raceControl section.
- [Installed normal-GUI test](../trials/build_20260914_222402_567/installed-control.log) parses SC pacing/RF takeover fixtures and writes real reports with provenance only inside this trial.
- [Full canonical SDK gate](../evidence/sdk-gate-race-control-20260914.log) exited0 with actual reviewed executable. SDK source/broker ABI unchanged; gate-regenerated artifacts preserved before execution.
- First component run failed because the new completion fixture remained in pre-race phase3. Corrected the fixture to phase5, then the entire component gate passed. Failed output retained. Compiler warnings remain logged; not a warning-free claim.

Installed exact hash to Documents/F1 Challenge V10/plugins/lightloader/race_data_collector.dll and upgraded the adjacent Narrative bridge. [Installation manifest](../trials/build_20260914_222402_567/installation_20260914_222625.json) records source/destination/hashes and complete before-states. Existing enabled INI, gui.py bootstrap, Safety Car DLL/INI, Red Flag DLL/INI, JAR and season data unchanged. No game was running; no game launched. Binary/schema activation remains pending next start.

Restoration: backups under workspace_organization_audit/RACE_DATA_CONTROL_INSTALL_BEFORE_20260914_222625 contain the previous installed collector and bridge. After a normal game exit, preserve any later revisions before copying those before-states back. Setting Collector/Enabled=0 and restarting also disables collection without deletion. Maintained documents/SDK artifacts preserved under RACE_DATA_CONTROL_BEFORE_20260914_222442. No production promotion.

Native acceptance still required: actual deployment→pace→recall→leader restart, RF handoff/cancellation/pit wait/restart, full-field classification without SC, unavailable/stale providers, frame overhead, and useful Narrative output across linked RF segments. Automated fixtures do not establish these.

Documentation gate: indexed through component README/status; evidence and installation manifest linked. Validator remains at the ten pre-existing unrelated workspace errors. Wasted-effort check: initial repository searches/reads returned overly broad historical output and were narrowed; one fixture-phase failure corrected; one full SDK gate required, component rerun followed pause-path changes; bounded tool waits used while progressing work, no material idle polling.
