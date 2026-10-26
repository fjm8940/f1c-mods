# STOP 1 review (Claude, 2026-10-09)

Verdict: ACCEPTED WITH CORRECTIONS. Fix items 1-3 before STOP 2 work.

Verified by Claude (fact):
- Dossier files exist; no vendor binaries/DATs copied into workspace (only the viewer IDB `mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.i64`, which embeds viewer bytes: private, never distribute).
- Vendor tree writes on 2026-10-09 (telemetry.spt 12:43, reference_2002_hockenheim.lap 12:44, _revert/_single_layout.ini + debug.log 12:53-12:54) all predate DS work (dossier 13:21+). DS did not modify the vendor tree.
- `ida-new-project` present in `tools/f1c_mod.py`; `tools/sdk/tests/test_ida_external_targets.py` passes and is wired into `run_all.bat`.

Corrections:
1. FUNCTION_COVERAGE "Unresolved: 0" is misleading. It covers only the native layer (134 stubs/thunks). Application logic (VB6 p-code procedures) is 0% classified. Reword: "native layer: 134/134 classified; p-code procedures: 0/N classified (N pending STOP 2)". Also sub_41F178 says "purpose unresolved", contradicting 0 unresolved.
2. Finding `legacy-viewer-rpm-scale-20261009` tier CONFIRMED overreaches. Layout y-range 7000..20000 is design intent; SpeedMultiplier is static source. No native tachometer/limiter/live read yet. Downgrade to static/inferred tier (append a correction entry; do not edit the old line) until live or native-tachometer evidence.
3. Link `mods/sdk_sandbox/legacy_telemetry/README.md` and `legacy_dossier/` from web_telemetry README so they are discoverable.

STOP 2 guidance:
- P-code: parse VB5!/VB6 structures yourself (ObjectTable, object descriptors, method/ProcDscInfo tables, p-code opcode streams) with tests against this binary. Do not download third-party VB decompilers or install packages without owner approval.
- `olefile` is not installed. Installing it is a download: request owner approval, or parse the CFB container with a small own reader under %TEMP%.
- Do not launch f1_telemetry.exe; it rewrites telemetry.spt/layouts.
