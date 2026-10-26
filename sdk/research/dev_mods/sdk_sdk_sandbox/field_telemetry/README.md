**ABI2 candidate (2026-09-14), not installed:** header128/row400, orientation/up-dot/inverted seconds/suspected incapacitation and local SC/RF inversion reasons. Read-only provider requires broker15. Unknown/stale reasons remain unavailable; local qualification is not a final activation decision. [Implementation and verification](../../../../development/mods/full_safety_car/trials/v0.10.38_beta_controls/README.md).

# F1C Field Telemetry Service

First-party, read-only whole-field telemetry for LightLoader scripts. It is the
common observation layer intended for Red Flag, Safety Car, field viewers, and
future race-control scripts. It is test-ready but remains non-production until
the V10 gameplay checklist passes.

The provider:

- starts only after the LightLoader protected handshake succeeds;
- discovers the single game-update broker (ABI 15) and never reads game memory;
- publishes one immutable session snapshot through
  `Local\\F1C_FIELD_TELEMETRY_V2`;
- allocates capacity from the engine-reported slot count (no viewer car cap);
- clears the public row set on every session boundary;
- exposes stable session/object generations so stale vehicles cannot leak;
- logs startup, broker discovery, session start/end, and clean shutdown.

Native consumers include `tools/sdk/f1c_field_telemetry.h`. They must take no
gameplay action when `f1c_field_telemetry_read` returns zero. The Python viewer
uses the same ABI:

```powershell
python tools/f1c_mod.py field-viewer
python tools/f1c_mod.py field-viewer --once --json
```

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/field_telemetry
```

The broker accepts alternate F1C executable metadata only when the 32-bit image
layout, reviewed fingerprint, and both hook byte patterns match. A genuinely
different layout fails closed. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_FIELD_TELEMETRY_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/field_telemetry C:\path\to\staging`, which
refuses unowned overwrites.

For LightLoader testing, place both `f1c_game_update_broker.dll` and
`field_telemetry.dll` in `plugins\\lightloader`, with `field_telemetry.ini`
beside the provider. Load order is irrelevant because the provider waits for
the broker.
