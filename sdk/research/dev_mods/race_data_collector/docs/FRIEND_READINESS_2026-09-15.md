# Friend readiness and lap-history audit

Request: verify whether the collector DLL can be shared and supplies data needed for championship articles, especially lap positions and changes. Product remains 0.2.2 development candidate in verify; this was a read-only installation/source/capture audit, not release promotion, installation or a new gameplay test. Evidence destination: this document and ../evidence/friend-readiness-20260915.json; reproducible audit helper ../tools/audit_friend_readiness.py.

## Verified

Installed DLL SHA256 edfc4d7bbc0b3f4a037fbda80a5f0fe4de2eecb35a74792ea050d03c829e1e5e equals tested build_20260914_224502_310. Every recorded build input still matches. Reviewed existing core/consumer/PE evidence: x86 DLL, Windows ADVAPI32/KERNEL32/msvcrt/USER32 imports, no Java or MinGW DLL runtime. Local compatible broker hash 29ac9f5e27da4c28a1e08b2700a4c4e91f5880284c50d94ccfe441300b3f7347. Runtime requires Lightloader and compatible broker ABI 15+ capabilities; DLL alone is insufficient.

The executable is strictly checked against SHA256 1a70e5ed0c4a059f7f66741b1682d0aedb14921aa6140cdb70b09549fdf3a003 plus PE headers. Friend's executable/loader/broker have not been inspected. Other game variants will be refused, not presumed compatible. No hard-coded Matias output path: files go under running game root/RaceData, recovery underneath it; writable folder required. Enabled INI and DLL belong in the configured Lightloader plugin directory. No server upload or LLM credential needed on friend's PC.

## Actual latest-day summary JSONs

| Capture | Recorded / final driver-laps | Order snapshots | Findings |
|---|---:|---:|---|
| Brazil 1995 | 341/341 | 101 | All numbered lap sequences present |
| Argentina 1995 | 204/206 | 127 | Coulthard and Diniz each missing one crossing; native finalization used |
| San Marino 1995 | 290/290 | 64 | All numbered lap sequences present |
| Magny-Cours 2002 | 0/189 | 3 | Final classification exists without lap history; native finalization used |

Driver-laps count each driver's completed laps, not race distance. All four flag completeHistory:false and game-clock sampling gaps; all report zero lostRecords. Full lap sequences do not prove all between-lap action was captured. Owner reported upside-down Coulthard and probable Diniz falling through the map; these remain owner observations, not explanations proven by missing laps.

lapChart records observed lap number/time, position, crossing clock and observation clock. lapPositions repeats lap/position; positionHistory records observed order snapshots. Nominal sampling is every 100 ms wall time. Accelerated simulation advances more game time between samples; short-lived order/pit/contact changes may be missed. Source deliberately refuses to invent skipped lap records. Position at observed crossing can lag the actual crossing until the next sample. Order change does not establish an on-track overtake; pits/retirements can cause it.

Other output: field/grid, observed classification, best laps, pits, weather, raw retirement/penalty/contact and SC/RF observations. Finish elapsed times/gaps remain unavailable placeholders; derived sectors, contact pairing/blame, decoded retirement causes and full red-flag continuation are not verified. Fuel/tyre semantics remain provisional. Collector bounds stored history to 100000 records and marks loss; no unbounded full-race guarantee.

Current Championship Universe race_evidence.py consumes lapChart for per-driver coverage/pace and keyEvents for observed leader changes and semantic events. It does not pass every order snapshot to the LLM or automatically render a lap-position chart. Raw JSON retains the data for that feature. Thus collection capability and current article presentation are different.

## Decision and next gate

Useful for a controlled friend trial on the same supported game setup, and for evidence-limited result articles. Not verified as a universal single-DLL release or as collecting everything needed for exhaustive race reconstruction. Before general distribution: compare friend's executable/loader/broker, assemble a verified dependency package, run a short race from before start through the last finisher, compare classification and lap/order history to gameplay, and confirm successful export. Ordinary-speed capture reduces sampling uncertainty; it is not claimed fully validated. Friend sends the completed RaceData JSON; recovery JSONL/log is useful only for troubleshooting. Do not use recovery checkpoints as final races.

No files sent, no software rebuilt or installed, no games launched, no source or raw captures modified. Existing build logs reviewed because exact artifact and source manifest match; redundant rebuild would not establish native completeness. One initial audit used the wrong laps key, then corrected to lapsCompleted before recording findings. Existing documentation preserved under workspace_organization_audit/COLLECTOR_FRIEND_AUDIT_BEFORE_20260915. Workspace gate remains ten known pre-existing findings. Owner/friend native acceptance remains pending.
