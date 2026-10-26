# Full Safety Car SDK Lab

This is an isolated fork for source-like SDK development. The production tree
at `../full_safety_car` is read-only and must never receive prototypes, trial
hooks, experimental binaries, or generated files.

Isolation guarantees:

- The fork has a distinct source and DLL name: `pace_probe_sdk_lab.*`.
- It uses `F1C_PACEPROBE_SDKLAB_UNLOAD`, `F1C_PACEPROBE_SDKLAB_CTL`, and
  `pace_probe_sdk_lab_log.txt`, so it cannot collide with production IPC/logs.
- `guard_production.py` checks the production source and DLL hashes before and
  after every lab build.
- The IDA experiment uses `analysis/sdk_lab_idb/F1_Challenge_Online_sdk_lab.i64`,
  copied from the primary database before any type import or retyping.

Build:

```powershell
cmd /c mods\extended_vanilla_features\patches\full_safety_car_sdk_lab\dll\build_sdk_lab.bat
```

Never deploy this DLL under the production `pace_probe.dll` filename.

Run the complete isolation, SDK, smoke-build, golden-pit, and lab-build gate:

```powershell
cmd /c mods\extended_vanilla_features\patches\full_safety_car_sdk_lab\run_lab_checks.bat
```

The lab IDB has separate `F1CVehicle` and `F1CPitMachine` types. Function
retyping consumes `tools/sdk/vehicle_retype_reviewed.json`, never the raw
offset-matcher candidates. The rejected `PitStopProgress` candidate is typed as
`F1CPitMachine*`, preventing the misleading whole-vehicle interpretation that
caused the original false positive.
