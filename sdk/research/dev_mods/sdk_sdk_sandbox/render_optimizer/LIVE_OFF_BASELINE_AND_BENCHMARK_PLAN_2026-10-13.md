# Live OFF baseline and standardized benchmark plan — 2026-10-13

Product: SDK sandbox Render Optimizer, private trial installed, owner human validation pending. Owner asked for live data with optimization disabled, 22 owner-described vanilla cars; then proposed 3DConfig's benchmark to standardize comparison.

## Observed OFF baseline

PID 26132, canonical Online EXE SHA256 1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003. Process created 09:39:04.115203 UTC. Optimizer startup log written 09:39:04.588384 UTC reports OFF/no hook and zero counters; INI Enabled=0. Installed trial DLL remains hash 83223FDF4C1A42B5B3BB58F246F1CA487C058FFD0C11C9F0A60D759F2C02D286. This current observation supersedes the installation-time Enabled=1 setting.

Memory-reader samples at 09:42:51.321395 and 09:43:12.372313 UTC (12:42:51–12: advanced simulation time 167.6030353 to 188.6544789. Same PID/creation identity, game_state 5/session_type 7/race_phase 5, pause 0/player_out 0 and vehicle_count 22. Reader freshness advancing, evidence age 18.93/21.43ms at return; captures are historical after that time. Driver enumeration returned 22, with 20 moving above 1km/h at the ending sample. Player slot 0 was effectively stationary. Asset provenance/vanilla status is owner-reported, not independently verified. Replay_mode raw 1 is recorded without inferring replay usage.

Ten OS/GPU samples spaced about 2 s over 20.719 s: process CPU equivalent 156.71% of one logical processor (=1.567 logical cores), 7.84% of total 20-logical-processor capacity. Per-thread inventory returned empty, so no particular thread saturation established. Resident memory 627.84–628.00MiB. test GPU device-wide GPU utilization 10–12%, mean 11.6%; device VRAM 2363–2390MiB, graphics 2940–3037MHz. RTX 3090 sampled 0% and 0 MiB. NVIDIA readings include other applications/display memory and do not attribute GPU work or VRAM exclusively to F1C. Low device utilization does not prove a CPU bottleneck or graphics-device selection. Twenty seconds of stable RAM/advancing simulation is not a stability acceptance test. No FPS, frame-time or 1%-low measurement available from the session reader. Live telemetry HTTP health/telemetry endpoints on 8080 timed out; no settings or services were changed.

Read-only Config.ini: GRIDSIZE 22, OBJDETAIL 3, TEXDETAIL 3, WINDOWEDMODE 1, FSAA 0, VSYNC 1, VBSTRATEGY 2, RUNBENCHMARKS 0. VSync may explain low GPU load, so the baseline cannot identify a bottleneck from utilization alone. No runtime FPS cap or actual presentation behavior measured. Existing display settings were left unchanged.

Raw per-run output is disposable TEMP material: %TEMP%/f 1c-vertex-baseline-20261013/off-094312.json. Material evidence is summarized here so the result does not depend on retaining raw output.

## 3DConfig benchmark investigation

Read-only strings from installed 3DConfig.exe, SHA256 A7E0B8DF4B636669338E8FD1C6C24DB522028A84CC110EB52A8E2A517CF6DAEB, include Run Benchmark, Running benchmarks for, Benchmarks Completed, RUNBENCHMARKS, BMARK 00.MTS, BMARK.MAS and Direct3DCreate8. The game parser ConfigIniParser 0x 421CD 0 stores RUNBENCHMARKS as a boolean at config-object+213; writer 0x 422BE 0 emits it. These establish a built-in benchmark control, but do not establish which process owns its scene, that it loads Lightloader, or that it exercises the trial's >=128/source-colour specialization. No benchmark launched or configuration changed while the owner race runs.

Inference: a fixed benchmark scene can improve repeatability and can supplement gameplay validation. It qualifies for comparing this prototype only if the normal game process loads the trial and ACTIVE/fused_calls establish specialized work during the benchmark. A benchmark rendered solely by 3DConfig will not test a plugin confined to the game process. Exact launch/process behavior and output format remain OPEN. A benchmark with zero eligible fused calls cannot establish this optimization's benefit.

## Standardized procedure

1. Finish/close the current game before using 3DConfig. Preserve Config.ini and optimizer INI before any settings changes;3DConfig may rewrite display/configuration values.
2. Fix one resolution, display mode, AA, VSync/cap policy and VBSTRATEGY. Current VSync 1 is part of this baseline. For an uncapped throughput experiment, define a separate preserved configuration rather than mixing capped and uncapped results. Do not force VBSTRATEGY 2 for AMD from this evidence.
3. Qualify one benchmark run: identify its rendering process, verify the trial's startup status in that process and require increasing fused_calls while its workload renders. Record settings and exact candidate/executable hashes. If 3DConfig runs standalone or eligible hits remain zero, use it only for general renderer diagnostics and use a fixed native replay/full-weekend camera sequence for optimizer A/B.
4. Once qualified, warm up the same workload and measure at least 3 OFF and 3 ON runs, alternating states, with a full process restart for each. Record each built-in score, report median and range; if output lacks frame-time data, do not fabricate 1%-lows. Add actual frame timing before making stability/smoothness claims.
5. Retain a matched 22-car full-weekend gameplay check for mirrors/cameras/cars/track, menus/device resets, exit/restart and mod coexistence. Never QuickRace. Rollback is Enabled=0 plus full restart.

## State and documentation

No game memory writes, injection, toggle, restart, benchmark execution, settings or binary modifications occurred. Only SDK documentation/checkpoint/findings changed. Parent SDK product stage unchanged. Before-states: workspace_organization_audit/VERTEX_LIVE_BASELINE_BEFORE_2026-10-13. Workspace validator retains pre-existing .claude/plans errors, zero warnings. No full build/tests rerun for read-only observations and prose; existing candidate verification remains applicable.

Pending: benchmark qualification and real FPS/frame-time capture; matched ON run and native active-hit evidence; gameplay improvement/visual/stability acceptance. Wasted-effort check: repeated reading—bootstrap output overbroad/truncated; broad builds/tests—none; failed/retried commands—runtime-observe tool-info did not resolve a capability ID, source BMARK search returned no matches, two local HTTP endpoints timed out; polling/idle—one bounded 20 s performance capture, no unbounded loop. Future queries should summarize drivers and directory listings rather than printing inventories.
