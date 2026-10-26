# ESC-exit false "Watch for the pit lane entrance" (2026-09-29)

Product: Pit Stop Failures 1.0.5 (`release_candidates/pitstop_failures-v1.0.5-rc.1`).
Stage: investigate. No code, build, or install changed.

## Owner report

In practice/qualifying/warmup, choosing ESC menu EXIT (teleport to garage) makes
the race engineer say "Watch for the pit lane entrance" (EID 50).

## Evidence (FACT, `Documents\F1 Challenge V10\plugins\lightloader\pitstop_failures.log`)

Every 1.0.5 session since 2026-09-28 shows the same pair on ESC exit:

    220.281372 Matias: XPITIN vicinity acknowledged by pit_road at 56.481869 m; active pit call; EID 50 armed
    220.281372 Matias: XPITIN EID 50 accepted by native voice queue
    220.306019 Matias: r10 teleport detected; pit-entry call suppressed

Same 56.48 m distance each time (garage position), teleport guard logged one
tick (~25 ms) after the voice was already queued. Lines 1430-1506 of the log.

## Root cause (FACT from source, `source/live_client.cpp` 1226-1316)

- Pit-road geometry (`on_pit_road`, line 1276) uses the live position
  `VEHF_WorldPositionX/Y/Z(v)`.
- r10 teleport guard (line 1287) uses the snapshot `s.world_x/s.world_z`, which
  lags one tick.
- On the teleport tick geometry already sees the car in the garage (on the
  pit-road line, <= 60 m from XPITIN) while the guard still sees the old
  position, so `approach_edge` fires and EID 50 queues. Guard trips next tick,
  too late.

Not session-type specific; race ESC-exit would behave the same (INFERENCE).

## Proposed fix (not yet applied)

In the r10 block use the live position instead of the snapshot:

```cpp
const float live_x=VEHF_WorldPositionX(v),live_z=VEHF_WorldPositionZ(v);
const float dx=live_x-c.last_x,dz=live_z-c.last_z;
...
c.last_x=live_x;c.last_z=live_z;c.pos_valid=true;
```

The block already runs before `approach_edge`, so the same tick is suppressed.

## Implementation (2026-09-29, later same day)

- New candidate `release_candidates/pitstop_failures-v1.0.5-rc.2`: copied
  `source`, `src`, `build.bat`, INI, `f1c_mod.json`, `compatibility-profile.json`
  from rc.1 (`diff -r source` equal before patch). rc.1 untouched.
- Patch applied exactly as proposed above (r10 block, live position).
- `build.bat` (`-Werror`) with `F1C_SDK_DIR=f1c_ida_decompiled\tools\sdk`: PASS.
  `live_tests`: PASS (`build/live_tests.txt`). No fixture covers the teleport
  tick (limitation).
- DLL `build/pitstop_failures.dll` sha256
  `fd4b158e515b4b09f7502e263dda3b6f4d67382690c2fc6efbb27fe46030dedb`.
- NOT run: SDK gate, compat probe, verify_candidate, installer, payload assembly.
- NOT installed: agent copy into game was denied by permission classifier.
  Installed DLL remains rc.1 (`3fbc2d9f…5e29`, equals rc.1 payload = restore source).
- Owner then said "install": rc.2 DLL installed 2026-09-29 (game
  closed) to `F1 Challenge V10\plugins\lightloader` and
  `Documents\F1 Challenge V10\plugins\lightloader`; read-back sha256 `fd4b158e…dedb`
  both. Restore: rc.1 payload DLL.
- until owner in-game validation.

## Next steps / pending validation

1. Copy rc.1 build inputs to a new `pitstop_failures-v1.0.5-rc.2` (rc.1 stays sealed).
2. Apply patch, build (`-Werror`), run `live_tests`, SDK gate, compat probe.
3. Human in-game test: ESC exit in practice/qualy/warmup gives no EID 50;
   normal driven pit entry still gives EID 50 once.

Blocker this session: Bash tool unavailable (auto-mode classifier errors), so
no copy/build was possible.
