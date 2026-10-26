# Render Optimizer private vertex trial

**Installed private trial reaches ACTIVE, but fused work remains zero. Diagnostic counters are built and verified, awaiting game closure for installation; no FPS gain established.**

- [Installation, verification, exact hashes and A/B instructions](VERTEX_TRIAL_INSTALLATION_2026-10-13.md)
- [Installation specification and rollback](VERTEX_TRIAL_INSTALL_SPEC_2026-10-13.md)
- [Current state](CURRENT_STATE.md)
- [Standalone kernel review](VERTEX_PACK_REVIEW_2026-10-13.md)
- [Standalone repeat test](VERTEX_PACK_RETEST_2026-10-13.md)
- [Earlier rejected state-cache design](IMPLEMENTATION_REVIEW_2026-10-13.md)

Close the game before changing [VertexOptimizer] Enabled in installed render_optimizer.ini: 1 uses the private trial, 0 installs no vertex hook and uses original execution. Restart for every comparison. Use the same guarded full-weekend scenario; never QuickRace. Check render_vertex_optimizer.log for ACTIVE and nonzero fused_calls before interpreting an ON run.

The new vertex_plugin.cpp and SDK midpoint adapter leave native locking, dirty flags, lighting, effects and unlock in place. Only eligible source-colour groups of at least 128 vertices use vertex_trial.hpp/vertex_pack.hpp. Small and incompatible branches fall back. The hook can add overhead; net FPS gain is unproven.

Build only through `python tools/f1c_mod.py build render_optimizer`. Current build script is build_vertex.bat; output is %TEMP%/f1c-render-vertex-0.2.0-dev1/render_vertex_optimizer.dll. Captured-x86 integration comparisons, kernel cases, real D3D8 buffer and actual DLL refusal checks run during build. Full SDK gate passed.

Old plugin.cpp/state_cache.hpp/build.ps1 and preserved trial snapshots remain rejected historical research. Their render_optimizer.dll is hard-disabled and was not installed. The old state-cache design is not linked into the installed vertex trial. Source toggle-script drafts are not installed. No public distribution or promotion authorized.

- [Live OFF baseline and standardized benchmark plan](LIVE_OFF_BASELINE_AND_BENCHMARK_PLAN_2026-10-13.md)

- [3DConfig result: separate benchmark, unsuitable for plugin A/B](THREEDCONFIG_BENCHMARK_RESULT_2026-10-13.md)

- [Broker initialization fix and corrected installed hash](BROKER_STARTUP_FIX_2026-10-13.md)

- [Further FPS, texture and graphics-backend options](PERFORMANCE_OPTIONS_2026-10-13.md)

- [Zero-fused-work diagnostic counters and pending native cause](VERTEX_REJECTION_DIAGNOSTICS_REVIEW_2026-10-13.md)
