# Track Status Monitor

This mod was generated from the isolated F1C SDK starter. It is non-production
until its behavior and calling assumptions have been reviewed.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/track_status_monitor
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_TRACK_STATUS_MONITOR_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/track_status_monitor C:\path\to\staging`, which
refuses unowned overwrites.

## Live player GUI

After the DLL is loaded, run:

```powershell
python mods/sdk_sandbox/track_status_monitor/gui/track_status_gui.py
```

The window maps `F1C_TRACK_STATUS_CTL` **read-only** and shows the game's own
`ON TRACK` / `OFF TRACK` state in real time. It defaults to player slot 0 and
also offers every observed vehicle for inspection. `OFF TRACK` means the game
has judged the vehicle outside the racing surface; it does not identify a
specific material such as grass versus gravel.
