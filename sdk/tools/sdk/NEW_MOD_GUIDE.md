# Creating a Native F1C Mod

> First-party Lightloader mods must also follow `development/mods/MOD_STANDARD.md` (runtime rules, usage reports via `mods/sdk/f1c_usage.h`, metadata, release pipeline). This guide covers SDK mechanics only.

The SDK supports one exact 32-bit executable build. Fixed addresses are not
portable to another executable, even when its filename is identical.

## Create and build

From the repository root:

```powershell
python tools/f1c_mod.py init weather_probe --title "Weather Probe"
python tools/f1c_mod.py build weather_probe
```

Creation fails if the destination already exists. The build emits only
`mods/sdk_sandbox/weather_probe/build/weather_probe.dll`; it cannot select or
overwrite an existing production mod.

Verify the game executable before deployment:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL repeats the PE-header compatibility check at runtime and unloads itself
on mismatch. A strict code-byte probe is logged separately, because another
properly installed mod may already have patched those bytes.

## Runtime rules

- Worker threads may observe stable scalar state and handle files/events.
- Calls into game functions and simulation mutations belong on the game thread
  unless that specific function has independent leaf-safety evidence.
- `F1CGameDispatch` is a single-producer/single-consumer queue. Post from the
  worker; only the SDK-owned hook broker may drain it. Mods must not install
  independent detours at the internal game-update anchor.
- A broker must call `f1c_dispatch_drain_bounded` with its declared per-update
  budget. The compatibility drain is capped at 63 tasks and never follows an
  indefinitely replenished queue.
- Never write fields tagged `output`; use reviewed command inputs or functions.
- A `LEGACY` function is an analysis lead, not a callable API.

`f1c_hook.h` provides a reversible x86 detour. Installation requires the exact
expected whole-instruction byte sequence and fails if anything differs.
Removal also fails if another component has changed the installed patch, so it
does not overwrite somebody else's hook. The caller remains responsible for
reviewing instruction boundaries and the hook function's ABI.

## Deploy and unload

Deployment refuses existing files by default and permanently protects the
production filename `pace_probe.dll`:

```powershell
python tools/f1c_mod.py stage weather_probe "C:\path\to\staging-or-game-directory"
```

An update requires both `--replace-sdk-owned` and the ownership sidecar created
by the first deployment. This authorizes replacement only of that generated
mod's own DLL.

Unload through the generated unique event before replacing a loaded DLL:

```powershell
python tools/f1c_mod.py unload F1C_WEATHER_PROBE_UNLOAD
```

## Test and promotion

Run the complete SDK workflow gate:

```powershell
python tools/f1c_mod.py sdk-test
```

It regenerates and validates the catalogue, compiles C/C++ headers, generates a
temporary mod, builds an x86 DLL, proves project overwrite refusal, and tests
guarded deployment/update behavior.

Keep prototypes under `mods/sdk_sandbox`. Promotion to a production mod is a
manual review step: copy only the proven behavior into the production source,
assign production configuration/packaging, rebuild there, and add a regression
specific to the feature. Never rename a sandbox DLL over a production DLL.

For a large C++ mod spanning several engine systems, use `tools/f1c_mod.py`
instead of running the individual commands manually. Its manifest and command
reference are documented in `LARGE_MOD_AUTOMATION.md`.
