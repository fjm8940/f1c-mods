# Large-Mod Automation

`tools/f1c_mod.py` is the manifest-driven front end for multi-subsystem native
mods. It coordinates the canonical SDK, recovery graph, generated C++ API,
target verification, guarded hooks, runtime tracing, regression comparison,
builds, isolated IDBs and protected deployment.

## Quick start

```powershell
python tools\f1c_mod.py init championship_overhaul --title "Championship Overhaul"
```

This creates `mods/sdk_sandbox/championship_overhaul` as C++17 by default and
adds `f1c_mod.json`. Edit that manifest to select subsystem tags, seed
functions, hooks, command-field writes, tracing and regressions.

Run the complete local pipeline:

```powershell
python tools\f1c_mod.py pipeline championship_overhaul --callers
```

That validates policy, creates the dependency slice, regenerates SDK-derived
artifacts, runs all SDK/orchestrator tests and builds the project. It never
stages or injects automatically.

## Manifest

```json
{
  "schema_version": 1,
  "id": "championship_overhaul",
  "title": "Championship Overhaul",
  "production": false,
  "language": "cpp",
  "sdk_major": 1,
  "target_sha256": "1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003",
  "subsystems": ["pit", "ai", "session"],
  "seeds": ["RequestPitEntry", "PitStateUpdateMachine"],
  "hooks": [
    {
      "function": "RequestPitEntry",
      "handler": "request_pit_entry_hook",
      "stolen_length": 7,
      "expected_bytes": "83 3D A4 56 7C 00 07"
    }
  ],
  "writes": [
    {"object": "F1CVehicle", "field": "CallToBox"}
  ],
  "tracing": {
    "driver": "Bernd Maylander",
    "seconds": 180,
    "hz": 20,
    "wait_phase": 5,
    "pit_cycle": true,
    "semantics": ["command_input", "output", "latched_control"]
  },
  "regressions": []
}
```

Hooks may target only reviewed ABIs. Their expected bytes must match the
canonical executable exactly and cover 5–32 bytes. Overlapping hooks and hooks
claimed by another managed sandbox project are rejected.

Writes may target only trusted fields classified as `command_input` or
`latched_control`. Output/state/cache writes fail validation.

## Commands

```text
init          create a non-overwriting C++ or C project and manifest
validate      check schema, target, ABIs, hook bytes, writes and conflicts
discover      traverse callers/callees from manifest seeds
generate      emit bindings, guarded hook scaffolds, command setters and plans
build         validate, generate and compile the isolated x86 DLL
trace         execute the manifest's SDK-driven live trace scenario
compare       gate rewrite traces against vanilla traces
test          run the canonical SDK gate and build this project
pipeline      discover plus the complete local test/build pipeline
ida-prepare   copy the primary IDB and apply reviewed types/names to the copy
stage         deploy through SDK ownership markers; never injects
memory-build  join static object layouts to runtime evidence
memory-fingerprint  inspect one provenance-aware object layout
memory-match  match a multi-field layout against a runtime capture
memory-correlate  rank event-specific flat and child-object changes
memory-front-wing  regenerate the front-wing pilot report
memory-snapshot  capture compressed module/process pages read-only
memory-diff   compare snapshots by module RVA and find state transitions
variant-profile  fingerprint a PE executable and its sections
symbol-recipes-build  generate canonical relocation recipes
resolve-symbols  resolve a variant or refuse ambiguous evidence
```

Examples:

```powershell
python tools\f1c_mod.py search "front wing" --corpus raw
python tools\f1c_mod.py inspect 0x004F24F0
python tools\f1c_mod.py memory-fingerprint VehicleDamageModel
python tools\f1c_mod.py memory-correlate wing_loss --control ordinary_driving
python tools\f1c_mod.py variant-profile "C:\path\modified.exe"
python tools\f1c_mod.py resolve-symbols "C:\path\modified.exe"
python tools\f1c_mod.py discover championship_overhaul --depth 2 --callers
python tools\f1c_mod.py generate championship_overhaul
python tools\f1c_mod.py trace championship_overhaul --print-only
python tools\f1c_mod.py ida-prepare championship_overhaul
mkdir C:\staging
python tools\f1c_mod.py stage championship_overhaul "C:\staging"
```

`search` reads the source files themselves on every invocation. It can search
raw Hex-Rays C, annotated C, deterministic skeleton facts and DERIVED filled
analyses independently or together. Multiple plain terms use AND semantics by
default; `--mode any`, `--regex`, `--case-sensitive`, `--json` and `--output`
are available for scripted investigations. `--corpus translated` is an alias
for the filled DERIVED analyses and retains that lower-trust label.

`inspect` resolves an exact function name or address and joins the raw body,
available annotations/analyses, callers, callees, object offsets and SDK ABI
status. Large bodies are explicitly truncated; pass `--full` to show all lines.
Raw Hex-Rays C remains the canonical code evidence. A readable DERIVED analysis
is never treated as ABI proof.

See [SEMANTIC_MEMORY_LINKER.md](SEMANTIC_MEMORY_LINKER.md) for deterministic
layout fingerprints, capture matching, identity-propagation limits and event
correlation.
See [VARIANT_RESOLUTION.md](VARIANT_RESOLUTION.md) for deduplicated process
snapshots, controlled `0→1` searches and executable-independent symbol recipes.

Generated project files live under `generated/`:

- `discovery.json` and `discovery.md`: bounded dependency slice
- `mod_bindings.hpp`: selected reviewed C++ functions
- `hooks.hpp`: exact-byte hook install/original-trampoline declarations
- `commands.hpp`: setters only for manifest-approved command/control fields
- `hook_plan.json` and `trace_plan.json`: auditable execution plans

## Safety boundary

The orchestrator refuses projects inside existing `mods/*` trees except
`mods/sdk_sandbox`. It refuses unreviewed function hooks, output-field writes,
incorrect executable bytes, overlapping hooks, cross-project ownership
conflicts, existing project/IDB/trace overwrites and the protected production
DLL name.

`stage` is deliberately separate from `pipeline`; a successful build never
changes the game installation or another mod. Injection is also deliberately
outside the orchestrator. The staging directory must already exist. Replacing
a staged DLL requires both its SDK ownership marker and
`--replace-sdk-owned`.
