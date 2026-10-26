# rc.3 native boundary repair, 2026-10-09

Development record. Pipeline: verify, then owner native/human validation. rc.2 remains installed and fails completed-lap acceptance. rc.3 is built, not installed.

## Native failure and correction

CONFIRMED from read-only HTTP capture of the owner's advancing Hockenheim race: selecting Ralf Schumacher slot 1 and Michael Schumacher slot 2 resets their bounded recorders. Both collected lap 4 without sample losses, but it was discarded on lap 5. This disproves the earlier inference that only waiting for full laps would resolve the blank UI.

At simulation 635.6 Michael had counter 5, distance 6775.6592; at 635.7 counter 6, distance 6781.3198; at 635.8 counter 6, distance 8.0820. Ralf: 637.5 counter 5/distance 6770.5161; 637.6 counter 6/distance 6775.6592; 637.7 counter 6/distance 2.3410. Track length 6784.1 m. Thus native counter increments precede distance wrapping. rc.2 incorrectly required simultaneous increment and wrap.

rc.3 defers a +1 counter change while both consecutive distances remain in the final 50 m, time is continuous (0 < dt <= 1 second), and distance advances. The subsequent physical wrap closes the old lap and begins the new one, preserving crossing interpolation and two-boundary completeness. Larger jumps, missing boundaries, backwards distance, invalid gates and overflows retain refusal behavior. Fixture replays early counter ordering and verifies a subsequent complete lap is retained. Native fix acceptance still requires rc.3 installation and live crossing retest.

## Usability repair

Engineering now shows selected cars' live speed (km/h), throttle/brake (%), AI steering and lateral/longitudinal acceleration (m/s²) during collection. Each value respects the channel validity bit; longitudinal acceleration gains its own validity bit 64. Snapshots refresh with the existing 2-second Engineering poll. Status explains lap number, progress percentage, sample count, completed-lap count and the recording reset on selection. Empty canvas is hidden; loaded charts have a bounded 560px height. Changing selection hides old charts immediately. Live snapshots do not claim recorded lap histories.

## Verification and limits

Focused suite PASS: 41,078 assertions, including early-counter native sequence regression, existing partial/crossing/retention/pause/overflow and read-only HTTP gates; Node controller checks immediate live display, progress and hidden empty canvas. Synthetic capture+read p50 227 us, p95 267 us, max 286 us; native FPS/callback cost unmeasured. IAB synthetic preview confirms live values, progress and loaded charts render. Preview is synthetic and explicitly labels stale fixture data. Full SDK gate result is recorded in VERIFICATION_MANIFEST.json after completion.

Two builds produced DLL SHA256 733DDB1DD10CEF61523BE2048353AD6A165C71AE8FDBB84218DEC4453C5C9029. Installed rc.2 is unchanged; game process pins its DLL, so owner must close the game before replacement under the existing README lifecycle rule. Guarded startup, both complete native laps, CSV/JSON/compare and human presentation remain pending. No promotion.

## Preservation and changed files

Before-states: workspace_organization_audit/WEB_TELEMETRY_USABILITY_BEFORE_2026-10-09. Changed source web_telemetry.c, runtime_test.c, test_telemetry.py; version/build metadata build.bat, f1c_mod.json, mod.json, web_telemetry.dll.mod.json; maintained README, private test instructions, manifest, SDK checkpoint and appended findings. No loader/broker/game/settings edits. Restore those preserved files to recover rc.2 source; installed rc.2 rollback is unnecessary because it was not replaced.

Raw 115-second crossing capture (1,138 advancing observations) and synthetic screenshot are disposable TEMP artifacts under f1c-web-telemetry-live-20261009/rc2; material crossing evidence is retained above. Test development failures were fixture initialization (trace_select does not set used until trace_update) and overflow fixture context; corrected explicitly, no failed assertion bypassed. Full runtime then passed. One invalid PowerShell brace path search was corrected. Existing workspace validator exceptions .claude/plans remain unrelated.

Documentation gate: record indexed in README, exact DLL/source hashes in manifest, findings and checkpoint distinguish native failure from synthetic repair verification; rc.3 native acceptance pending. Full canonical SDK gate PASS. Validator: same 2 pre-existing errors (.claude/plans), 0 warnings. UTF-8 edited lines reread; no new mojibake detected.
