# STOP 5 / rc.6 review (Claude, 2026-10-09)

Verdict: FIXES ACCEPTED, CANDIDATE NOT ACCEPTED (sidecar hash defect). Backend finalization not started.

Verified (fact): rc.6 DLL in %TEMP%\f1c-web-telemetry-4.2.0-rc.6 = 7E1A690B...626E; installed still rc.2; 4 SYNTHETIC screenshots + checklist present.

Defect: the staged sidecar %TEMP%\f1c-web-telemetry-4.2.0-rc.6\workbench_ui.js is 38,514 B, SHA256 D565B6E0013492F56B9308AB07DAF9A287A2E49A173965323DEB7C3C2E283A2A. The manifests and the source declare C095A07E...1921 (37,693 B, LF). The staged copy is the same file with LF converted to CRLF (821 lines = 821 extra bytes; stripping CR gives C095A07E). The staged pair the owner would install does not match its own manifest. Find what converts line endings (build.bat copy step, a test, an editor step), make the staging byte-exact, add a gate that hashes the STAGED sidecar against the manifest, and rebuild. Also make /health's sidecar hash and the install route compare the same bytes.

Not done: DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md stops 2-4 (frozen API contract + schemas + contract tests, lifecycle/robustness fixtures, soak/concurrency/fuzz on all cores, backend live-check script). Owner decision: the backend is finalized before the frontend; the STOP 5 screenshots do not replace these.

## BACKEND 1 review (Claude, 2026-10-09)

Verdict: ACCEPTED. Verified: rc.7 DLL FDE5D209...C53C; staged sidecar == source == C095A07E... (LF, byte-exact); installed still rc.2. Root cause (text-mode write_text in test_telemetry.py) is credible and fixed.

Conditions for BACKEND 2+:
1. Preservation lapse: rc.6 source before-states were not taken. Record this in the BACKEND 1 record as a policy deviation (done: disclosed). From now on take before-states first, every round.
2. Clean payload: the staging dir mixes install files with test artifacts (runtime_test.exe, *_test.js, workbench_ui_served.js, telemetry.json, race jsonl). Produce a separate payload dir with only DLL + workbench_ui.js + ini (+ manifest), and point the install route and hashes at it.
