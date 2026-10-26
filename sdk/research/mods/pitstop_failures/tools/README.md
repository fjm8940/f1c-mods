# Pit Stop Failures tools

Two-stage v3 extension tooling: `policy_v3_tests.c` (compiled acceptance
harness), `build_v3_variant.py` (hash-guarded generator for the v3 session
variant), `v3_jsonl_check.py` (independent JSONL row checker),
`pit_failure_sim.c` (`--preset calm|standard|dramatic`, `--fire-support
off|on`, v3 reports) and `build_test_bridge.py` (offline GUI bridge, v2 + v3).
See [the v3 API note](../docs/POLICY_V3_TWO_STAGE.md).

The missing GUI is preserved at `legacy/pit_failure_gui.py` with its origin and
SHA-256 in [the import manifest](../verification/port-2026-09-21/import-manifest.json).
It is historical direct-memory-write code, not an active supported entry point.
The legacy launcher fails closed; use [the development README](../README.md).

The old monitoring/freezing/probing tools remain read-only research evidence in
`f1c_ida_decompiled/mods/extended_vanilla_features/tools`; they are not build
 dependencies. Do not invoke those utilities directly. Runtime work follows the
canonical SDK front door and its scoped instructions.
