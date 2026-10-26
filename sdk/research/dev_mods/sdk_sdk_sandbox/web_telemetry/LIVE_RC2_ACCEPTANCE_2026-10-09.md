# rc.2 live acceptance, 2026-10-09

Pipeline: verify; native comparison and human validation pending.
Capture UTC: 2026-10-09.539941+00:00. Read-only HTTP diagnostics on the owner's running race.

## Observed facts

- Loaded version 4.2.0-rc.2, broker ABI 15, generation 7; failed captures 0. Latest sample age 32 ms.
- Installed DLL SHA256 BA87BC2E6690C8E75D5E8EA96442E0DF7C743C420D85F30E3165FCCE50467FD4. Installation/restoration is recorded in REPAIRS_2026-10-09.md and the audit INSTALL_RESTORATION.json.
- 08:41:10.490619Z to 08:41:40.359233Z: sequence 2241 to 2817, maximum sample age 47 ms, failed captures zero. Combined sequential health+telemetry HTTP p95 42.52 ms (not native callback/FPS cost).
- Player unwrapped distance 1741.7 m versus integrated speed 1740.7 m, ratio 1.001. AI 561.2 m versus integrated speed 536.7 m, ratio 1.046 over a discontinuous early-race window; this is not precision calibration.
- Auto selection found Matthias Steppe (slot 0, non-AI control) and Takuma Sato (2002) (slot 10, AI control), both 2002 BAR, using documented unknown-team fallback.
- Both initial partial lap 1 recordings were discarded; both opened lap 2. Player unavailable steering; AI all ledger channels seen. No completed laps available yet.
- Engineering UI rendered the correct driver pairing and missing-channel label, with no reentry needed for status polling. Completed-lap automatic appearance and chart comparison remain untested live.
- Repeated observations held simulation time at 341.80 seconds and distances at player 6343.9268 m / AI 5613.2021 m, while transport sequence advanced. Open sample counts stayed 1725 / 1563 with zero sample losses. This establishes fresh transport but stopped gameplay, consistent with pause; it does not establish continued racing.

## Remaining gate and follow-up

Owner was asked to resume gameplay. Await both lap 2 closing crossings, then inspect native JSON/CSV, distance coverage, channel holes, elapsed delta, browser lap polling and chart rendering. Full-weekend lifecycle, FPS/handling and human presentation acceptance remain pending. No new binary changes or promotion occurred in this live check.

## Preservation and evidence

Raw bounded captures are disposable under %TEMP%/f1c-web-telemetry-live-20261009/rc2. This report retains the material evidence. README before-state is in workspace_organization_audit/WEB_TELEMETRY_REPAIR_BEFORE_2026-10-09/live_rc2_docs. No game state writes occurred.
