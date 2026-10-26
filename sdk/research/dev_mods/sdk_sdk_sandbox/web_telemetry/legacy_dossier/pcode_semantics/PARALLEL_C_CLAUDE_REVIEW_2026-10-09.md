# Parallel C review (Claude, 2026-10-09)

Verdict: ACCEPTED WITH 2 SMALL CORRECTIONS. Strong work.

Verified (fact): test_pcode_decode.py and test_pcode_features.py pass; opcode-table source (public vb-decompiler.org documentation page) is recorded; read only, nothing downloaded or executed.

Corrections:
1. Tiers: (b) smoothing "CONFIRMED (listing)" and (d) validity rule are listing + own unit tests only, with no check against viewer output from real files. Downgrade to DERIVED, via an appended findings correction (do not edit the old line). (a), (c) core and (f) were checked against the real reference lap: they may stay CONFIRMED where real-data error is stated.
2. Lean workspace: ref_lap_probe*.py and probe_*.py (9 exploration scripts) are not product. Either mark them as scratch in a tools/README with their purpose, or move them out via `tools/workspace/retention.py purge` (quarantine, never delete). Keep only the decoder, features and tests as maintained tools.

For the workbench (main agent, STOP 4): reuse (a) distance, (c) delta by distance with linear interpolation, and (f) tyre/damper formulas for legacy-import parity. Velocity in the viewer is mph (x2.2371365).
