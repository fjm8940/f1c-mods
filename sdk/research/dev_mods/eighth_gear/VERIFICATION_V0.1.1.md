# Verification record — v0.1.1 Ferrari ratio correction

Date: 2026-08-25  
Result: **AUTOMATED PASS; INSTALLED FOR HUMAN VALIDATION**

## Human evidence driving the correction

Matias confirmed that the v0.1 Ferrari 2005 shifted into eighth gear. The
selected `1.142857` ratio caused extreme RPM and engine failure, so gear
reachability passes while ratio safety fails.

## Corrected artifact

- Trial: `trials\2026-08-25\clean-v0.1.1-ferrari-ratio-hotfix`
- Scope: Ferrari 2005 HDV only
- Gear 7: catalogue index 61, ratio `1.238095`
- Previous gear 8: catalogue index 67, ratio `1.142857`
- Corrected gear 8: catalogue index 73, stock minimum ratio `1.000000`
- Output SHA-256:
  `0164d46281bc4c15a2bc52bc826cb2272473f0ab8779a82c4661afa14a50689e`
- Ratio catalogue changed: no
- Focused tests: 6 passed

## Installation and restoration

- Target: `F1 Challenge V10_clean`
- Installed files: 1
- Result: installed and post-copy hash verified
- Restoration record:
  `trials\2026-08-25\clean-v0.1.1-ferrari-install\RESTORATION_MAP.json`
- Preserved prior SHA-256:
  `cf0b18462d8b91eafc64b5279c0649cd78c6c4e52c5dc00e4908edf923e5df75`

## Remaining gate

In a guarded full-weekend Ferrari 2005 session, confirm that the 7th-to-8th
shift lowers RPM, the engine survives, the HUD remains correct, and restart
behavior is stable. This artifact is ``.

## Wasted-effort check

- Repeated large reads: the cold-start governance set was required.
- Unnecessarily broad builds/tests: none observed.
- Avoidable failed/retried commands: one test fixture initially had no ratio
  below seventh gear; the fixture was corrected without touching the target.
- Polling/waiting/idle: none observed.
