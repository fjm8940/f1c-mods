# Safety Car AI Observer v0.10 Trial

This mod was generated from the isolated F1C SDK starter. It is non-production
until its behavior and calling assumptions have been reviewed.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/safety_car_ai_observer
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_SAFETY_CAR_AI_OBSERVER_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/safety_car_ai_observer C:\path\to\staging`, which
refuses unowned overwrites.
