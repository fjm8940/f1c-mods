# Archived subsystem reconstruction workflow (preserved)

> Engine reconstruction is paused. The direct commands below document how
> evidence was produced; do not execute them. Current SDK work uses only
> `python tools/f1c_mod.py` and the LLM operating contract.

# Subsystem Reconstruction Workflow (historical)

This is the path from an unknown binary feature to a source-like module that is
safe to rewrite.

1. Generate the canonical catalogue, runtime trace schema, reviewed C++ API and
   recovery index with `tools/sdk/tests/run_all.bat`.
2. Query the candidate control flow with
   `tools/query_recovery_index.py <function-or-address> --depth 2`.
3. Treat automatically inferred call edges, names and same-`this` ownership as
   leads. Confirm ownership from constructors, callers or direct layout use.
4. Add reviewed fields and ABIs to `f1c_sdk_overrides.json`; regenerate rather
   than adding local raw offsets to a mod.
5. Record vanilla before/after snapshots and compare them with
   `tools/compare_vehicle_snapshots.py`. Pay special attention to
   `command_input`, `output` and latched fields.
6. Capture a repeatable live transition sequence with `trace_sdk_vehicle.py`.
   `--pit-cycle` waits for and completes the canonical `0 -> 2 -> 3 -> 4 -> 0`
   lifecycle. Compare vanilla and rewrite JSONL with `compare_event_traces.py`.
7. Instrument trusted scalar changes in a DLL with `f1c_trace.h`. Function calls require
   an ABI-specific reviewed hook; do not install generic hooks at guessed
   instruction boundaries.
8. Choose the narrowest replacement boundary that owns the intended behavior
   and all required side effects. Keep the original system active until its
   consumers and cleanup paths are known.
9. Implement under `mods/sdk_sandbox`, add a vanilla/rewrite regression, and
   exercise clean hook removal and unload.
10. Promote only the proven implementation to production through manual review.

Generated source-like wrappers live in `f1c_reviewed_api.hpp`, intentionally
under `f1c::reviewed::game_thread`. Their namespace makes the thread contract
visible at every call site. A wrapper proves the ABI and owner type, not that an
internal transition is valid to invoke in isolation; lifecycle preconditions
and side effects still need subsystem review. Functions absent from that header
are not approved even at the ABI level, regardless of whether an address exists
in the legacy index.
