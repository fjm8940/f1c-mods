# No Tab Pause

This mod was generated from the isolated F1C SDK starter. It is non-production
until its behavior and calling assumptions have been reviewed.

## Objective and acceptance criteria

Keep simulation running while the game loses foreground focus, while restoring
the game's original window procedure before close or unload so shutdown is not
delayed and no callback can enter an unloaded ASI.

The candidate must validate the exact supported executable, force only the
three focus-loss messages to active, restore before forwarding shutdown, pass
the initiating close message to the original game procedure, drain active
callbacks before cooperative unload, and refuse an unsafe unload.

## Investigation result

The active July 2026 ASI has no close/unhook path. A later archived source
attempted cleanup, but cleared its global original-procedure pointer before
forwarding the initiating close message and therefore fell through to
`DefWindowProc`. This candidate keeps a local original-procedure pointer,
restores first, and calls the game's original procedure with the exact close
message.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/no_tab_pause
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_NO_TAB_PAUSE_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/no_tab_pause C:\path\to\staging`, which
refuses unowned overwrites.

## Lifecycle and pending validation

Current stage: verification candidate. Static and SDK gates do not replace a
guarded full-weekend test. Matias must confirm that alt-tab never pauses and
that closing the game is immediate before production promotion.
