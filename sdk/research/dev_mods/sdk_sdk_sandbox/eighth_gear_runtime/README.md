# F1C Eighth Gear Runtime

This mod was generated from the isolated F1C SDK starter. It is non-production
until its behavior and calling assumptions have been reviewed.

The ABI-12 broker holds the engine's shared gear-8 ratio slot on the reviewed
game thread. The current Ferrari/Australia calibration is `1.142857142857`,
the exact ratio selected by the stock 2005 Williams eight-speed configuration.
Ferrari seventh gear is `1.238095`, so the shift produces a 7.7% RPM drop. The
earlier `1.100` test used an incorrectly identified seventh-gear ratio and
produced an excessive 11.1% drop outside the useful power band.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/eighth_gear_runtime
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_EIGHTH_GEAR_RUNTIME_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/eighth_gear_runtime C:\path\to\staging`, which
refuses unowned overwrites.
