# Nurburgring manual final classification - 2026-10-11

Product: Race Data Collector, verify/candidate. Task: owner-directed data edit and verification; no lifecycle change or code change. Scope: separate desktop upload copy; original untouched.

Evidence: owner screenshot (preserved as NURBURGRING_RESULTS_REFERENCE_2026-10-11.png) shows lap 22/30 and positions 1-21. Owner explicitly instructed: use screenshot as final results, 21 finishers, other three unclassified. This is a manual final-result decision, not native final-race evidence. Original export had Manning P1, TesterA retired P21; capture is already finalized and no continuation repair is needed.

Output: `%USERPROFILE%\Desktop\Nurburgring_2002_EDITED_RESULTS\SINGLE_Nurburgring, 2002_20261008_194850-12076-1_20261008_194850.json`.
Source: `%USERPROFILE%\Desktop\SINGLE_Nurburgring, 2002_20261008_194850-12076-1_20261008_194850.json`.
Source SHA256: `6b58eb07434a276be063bc44d15252e275234fbdb9c1f8cceb3e9b746edc7adf`.
Output SHA256: `c738a79b65bbf7b402497b8476549fc7aff8300e1b539f9f392eeabc060e071e`.

Changes: gameResults.drivers reordered, driverStandings mirrored; first 21 Finished, last three Ret/unclassified for importer compatibility. Their relative order is unknown; positions 22-24 are storage placeholders only. Final lap counts/times cleared to null; time strings cleared; native finish/retired flags and position-change summaries cleared. Retirement lists replaced with unspecified unclassified entries. Fastest lap removed because the historical capture does not establish it for the requested result. Manual schema f1c.manual-results/1.0 permits unknown laps; provenance and distinct capture identity recorded, classification review flag retained. Original per-driver best laps, grid and historical telemetry retained as original capture evidence, explicitly not verified against the manually declared result. No causes of retirement, final lap totals or finish times invented.

Classification:

1. Rubens Barrichello (2002)
2. Michael Schumacher (2002)
3. Ralf Schumacher (2002)
4. Takuma Sato (2002)
5. Marcel Lasee (2002)
6. Juan Pablo Montoya (2002)
7. Jarno Trulli (2002)
8. Fernando Alonso (2002)
9. Nick Heidfeld (2002)
10. Luciano Burti (2002)
11. Matteo Bobbi (2002)
12. Ricardo Zonta (2002)
13. Andre Lotterer (2002)
14. Luca Badoer (2002)
15. Jirko Malcharek (2002)
16. Justin Wilson (2002)
17. David Coulthard (2002)
18. Tester A
19. Felipe Massa (2002)
20. Stephane Sarrazin (2002)
21. Pedro Diniz (2002)
22. Kimi Raikkonen (2002) - unclassified (order unknown)
23. Jenson Button (2002) - unclassified (order unknown)
24. Darren Manning (2002) - unclassified (order unknown)

Verification: original byte equality passes; all 24 source identities accounted for exactly once. Actual Core.gs normalize passes under Node. Actual score with 2002 points verifies 21 classified finishers and three unclassified scoring zero. Screenshot order manually compared including Schumacher identities and TesterA P18. Owner Sheets upload/presentation review pending; actual end-of-race laps and times unknown. No SDK query necessary: consumer file editing only, no native game investigation. Workspace validator before task had .claude/plans unexpected directories, unchanged at closure.

Restoration: original source remains intact; README complete before-state in workspace_organization_audit/NURBURGRING_RESULTS_BEFORE_2026-10-11. No production, game installations, source binaries or credentials changed. Documentation gate: this report and reference image exist, report indexed in README, facts and owner decisions distinguished; pending validation named. Wasted-effort check: clarification needed because screenshot is mid-race and original retirement statuses conflict; no repeated broad testing.
