# Live all-driver trace and owner report — 2026-10-12

Pipeline: investigate -> implement -> partial verify. Native validation and promotion are pending because F1 Challenge Online is running with the installed 4.4.0 DLL. Source and staged candidate only were changed here. Before-states: `%USERPROFILE%\Desktop\F1\workspace_organization_audit\WEB_TELEMETRY_OWNER_LIVE_FIX_BEFORE_2026-10-12\`.

## Live evidence

Read-only HTTP snapshots from the running 4.4.0/API 5 process showed `trace_mode=all`, `trace_cars=24`, `trace_bytes=86027712`, and fresh capture sequences. Across two snapshots six seconds apart, exactly 6 of 24 slots gained trace samples. The other 18 were stationary/garage at the time; this count alone does not show that 18 moving drivers failed to record. Darren Manning (slot 23) had total/open samples 0 at both captures despite lap number 5. His telemetry showed speed 0 and garage phase (`pt=4`); his trace revision was 49. Sarrazin and five other moving cars advanced. A further ten-second three-snapshot check showed Manning unchanged and Sarrazin increasing 5208 -> 5298 -> 5394. The loaded DLL was still collecting for moving cars, but had lost previous laps for Manning.

## Source finding and change

The capture worker cleared **every** trace on a roster pointer change as well as on generation change. Garage return/rejoin can change a pointer within a session. In addition, slot identity compared a temporarily absent vehicle pointer as driver ID -1 and compared empty names, discarding that slot's trace. The precise event that erased Manning's earlier laps was not observed, but these source paths are proven data-loss bugs. `trace_capture_epoch` now clears only on generation change; slot identity ignores temporarily absent identity fields. Browser history identity no longer uses the transient AI-active flag; retired AI keeps its previous history. Viewer seeding now requests fresh trace status and retries empty seeds, so a driver selected before its first sample can populate later.

Other owner-requested changes in this candidate: Session Overview label; non-race sessions suppress lapped gaps and blue-flag presentation; same-team unanimous tyre-brand fallback for a custom player name; player engine-life countdown from `G_EngineLifeSeconds` exposed as `/telemetry.pel` in API 6, with AI's vehicle life countdown displayed separately and more available engine fields. HP, torque and boost have no proven game source and remain unavailable.

Race Control integration check: live `/health` confirmed Mechanical Failures mapping present, host verified, activated, and four families started. This establishes running status, **not** synchronized failure events. Pit Stop Failures DLL/log were present and active, but Web Telemetry has no Pit Stop Failures event bridge or health probe. Native pit-stop inference in Race Control is not that mod's failure feed. Full cross-mod event sync is OPEN.

## Verification and release state

`node viewer_test.js`: PASS, 273 checks. `cmd /c build.bat`: PASS after repinning both changed sidecars; staged five-file payload hashes match the manifests. The first build correctly rejected stale sidecar pins. The game process was still running (`F1 Challenge Online`, PID 32612), so no runtime test, full gate, native reload, or install was performed. The 4.4.0/API 5 DLL in the process cannot acquire these fixes until restart. Pending: full offline gate with game closed, then exact-hash install, then live all-slot trace showing each moving driver accumulating and retained laps through garage/roster transitions; owner visual checks of Session Overview, tyres, Engine Life, and Race Control.

## Follow-up after game close

The game closed later. `TELEMETRY_TESTS=1` completed with exit 0: 46,392 checks in the principal run, API 6 schema validation, 273 viewer checks, bounded endpoints, 20-worker logical two-hour soak with flat RSS, 80,000 fuzz iterations with zero crashes. The first gate run rejected old payload hashes in `VERIFICATION_MANIFEST.json`; these were updated to the exact newly staged bytes before the passing run. The exact five-file candidate was installed locally after preserving the prior files and hashes in `workspace_organization_audit/WEB_TELEMETRY_API6_LOCAL_INSTALL_2026-10-12/INSTALL_RESTORATION.json`. DLL SHA256: `7B033B6247298DC62F8E8B57E6A8EA93187A95B4BEF5C34795896A9ADD600080`. No native game validation or VPS upload yet.

Owner's subsequent release instruction: once the all-driver and other reported defects are fixed and validated, ship publicly as **Extended Telemetry v1.0.0**. This supersedes the earlier proposed public name/version Web Telemetry 4.4.0. Current code and local test candidate retain their development identity; public package, metadata, catalog, production path and deploy script must be rebuilt for the new name/version after the native gate and cross-mod failure-event work. Existing 4.4.0 release staging is stale and must not be uploaded.
