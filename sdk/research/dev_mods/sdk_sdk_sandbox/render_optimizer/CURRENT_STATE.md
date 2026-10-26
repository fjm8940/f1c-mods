# Render Optimizer current state

Latest: corrected installed build reached ACTIVE with increasing C-dispatch calls (24,985 observed), but zero fused calls/vertices. Exact rejection cause remains open. Diagnostic-only candidate SHA256 2BEC473A30961340EFD30B9ED38861B19BBAFF9595471A5D6CC41FBDE424B896 built and full SDK gate PASS; not installed because fresh reader sees running PID 30544 at 12:06:06 UTC. Close game before staging. [Diagnostic evidence and next gate](VERTEX_REJECTION_DIAGNOSTICS_REVIEW_2026-10-13.md). Earlier observations below are historical.

2026-10-13 latest observation: private vertex trial installed, owner set Enabled=0. Live PID26132 startup log confirms OFF/no hook; 22-car race advances. FPS/frame times unavailable; 3DConfig is now confirmed to render its own scene and is unsuitable for this game-plugin A/B. See [benchmark result](THREEDCONFIG_BENCHMARK_RESULT_2026-10-13.md). See [live baseline](LIVE_OFF_BASELINE_AND_BENCHMARK_PLAN_2026-10-13.md).

Installation-time state: private vertex trial 0.2.0-dev.1 installed, originally Enabled=1 for next game launch. Pipeline: owner human validation pending after automated private-trial verification. Parent SDK product stage unchanged.

Installed DLL SHA256: D8FD10737BE3456A40D5685283D92BDCB0D6BDA2E357772E11BC41B0DB970BA5
Resolved installation: %USERPROFILE%\Documents\F1 Challenge V10\plugins\lightloader
Source binary: %TEMP%/f1c-render-vertex-0.2.0-dev1/render_vertex_optimizer.dll

[Installation and verification](VERTEX_TRIAL_INSTALLATION_2026-10-13.md) records exact scope, evidence and rollback. 1,600 captured-x86 differential cases, 2,000 kernel cases, test GPU D3D8 buffers, protected-DLL refusal smoke and full SDK gate PASS. Local EXE fingerprints match. No game launched in this task; activation, eligible hit frequency, FPS/frame times, visual correctness, device resets, mod compatibility and stability remain unverified.

Switch installed render_optimizer.ini [VertexOptimizer] Enabled between 1 and 0 only with game closed, then restart. OFF installs no vertex hook. Status in render_vertex_optimizer.log; nonzero fused_calls required to establish actual optimized work. Use guarded full weekend, never QuickRace. Rollback is Enabled=0 plus full restart. Original EXE/loader/broker/VBSTRATEGY/other mods unchanged.

Old state-cache candidate remains rejected, hard-disabled and uninstalled, SHA256 B04C12629D79B513AB22A6386194DCD6E364438273C5BD7393E2FF92CBC40792. It is separate from the installed vertex DLL.
