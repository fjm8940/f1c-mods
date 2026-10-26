# TV Director car switch "crashed" telemetry: broker quarantine, 2026-10-17

Status: cause found in the shipped code (DERIVED, strong); fix in the 1.2.2 source. Not live-tested, not released.

## 1. Report


## 2. Dev build check

- Source tree = 1.2.2 (`TELEMETRY_VERSION "1.2.2"`, CO-OP remote brake/steer). `director_state.inc` (2026-10-14) and `compatibility_runtime.inc` (2026-10-14) have not changed since 1.2.0. The dev build therefore had the same bug as the shipped 1.2.0/1.2.1.
- The earlier 1.2.2 payload was only in `%TEMP%` and is gone. It must be rebuilt anyway.

## 3. Cause (shipped code)

FACT (code read):
- **Broker rule.** Lightloader broker (`replay_naming_dev_2026-10-13/game_update_broker.c` + `broker_callback_timing.h`) quarantines a client when:
  - one callback runs over 100 ms, or
  - 10 callbacks in a row run over 20 ms.
  A callback that called `F1CBrokerNativeBudget` gets a strict 250 ms limit instead.
- **What quarantine does.** It sets the client inactive and purges its queued tasks. Every later `F1CBrokerPost` with that token returns `F1C_BROKER_STALE_TOKEN`.
- **Telemetry's reaction (≤ 1.2.1).** The worker (`compatibility_runtime.inc`) cleared `capture_pending` and posted again, forever. It never re-registered and never logged anything.
  - With no capture for more than 1 s, the worker zeroes the cars.
  - The page then shows "Waiting for the game" until the game restarts. This matches the report.
- **The trigger path.** A TV Director car change calls `GameStateDispatchChain` 0x40D240 inside a telemetry broker task, with no native budget. The chain runs several native routines (decomp):
  - `SetPlayerVehicleIndex`
  - `CameraTargetUpdateOnPlayerChange`, which calls `AudioSoundEffectController` over every car slot
  - `CameraViewAssignment`
  - `VirtualDispatchConditionalAudio`
  - `AIOutOfTrackEvent`
  - `sub_4D8760(1,0)`

DERIVED: on a slower PC, one such call (camera/audio reassignment for the new car) took over 100 ms. The broker then quarantined telemetry for good.

Owner evidence (no failure on the owner PC):
- `Documents\F1 Challenge 99-02 (V10)\plugins\lightloader\web_telemetry_log.txt`, 2026-10-16: 17 car switches, all "applied". Capture held about 16/s the whole time.
- `f1c_game_update_broker_log.txt`: 617 telemetry callback warnings, max 23 573 µs, 0 quarantines.

Not proven: the reporter's broker log was not seen. Proof is the line `callback quarantine: client=web_telemetry` in their `f1c_game_update_broker_log.txt` (game folder).

## 4. Fix (1.2.2 source)

1. **Native budget for TV Director game calls.**
   - `f1c_broker_recovery_bind` runs after registration. Its result is logged: "broker native budget for TV Director game calls: available / not offered".
   - `telemetry_native_budget()` runs at the start of `dcam_cmd_task` (only once a command is taken), `dcam_watch_task` and `dcam_hud_option_task`.
   - With the budget, the broker limit for these callbacks is 250 ms instead of 100 ms. Older brokers are not bound, so nothing changes there.
2. **Self-heal after any quarantine.**
   - When the capture post returns `F1C_BROKER_STALE_TOKEN`, the worker calls `telemetry_reregister`.
   - It registers again, at most once every 2 s, and swaps the token atomically.
   - It also clears `capture_pending` and releases a lost TV Director watch step (`g_dcam_watch.posted = 0`), so the watch gives up after its 5 s guard.
   - It logs `broker: telemetry callbacks were stopped by the broker ... registered again, count N`.
   - A failed re-register is logged and retried after 2 s.
   - The broker reclaims the old slot itself, so no client slot leaks.

Effect: worst case, the page loses data for about 1-3 s instead of until restart. The TV Director command that triggered the quarantine reports a refusal or timeout.

Not done (noted): the director tasks take `g_telemetry_cs` with a blocking Enter on the game thread. If the worker holds it while formatting JSON, that wait adds to the callback time. This is a possible source of the 5-23 ms warnings and was left unchanged.

## 5. Files

Each file was backed up before its first edit as `<file>.BEFORE_20261017_quarantine`.

| File | Change |
|---|---|
| web_telemetry.c | include `f1c_broker_recovery.h`; `telemetry_recovery`, `telemetry_recovery_ok`, `telemetry_reregisters`, `telemetry_native_budget()` |
| director_state.inc | `telemetry_native_budget()` in `dcam_hud_option_task`, `dcam_watch_task`, `dcam_cmd_task` |
| compatibility_runtime.inc | `telemetry_reregister()`; recovery bind + log after register; STALE_TOKEN → re-register in the worker post loop |
| runtime_test.c | `test_broker_quarantine_reregister` (mock broker: re-register, 2 s throttle, failed register keeps token, native budget only when bound, with the current token) |
| f1c_ida_decompiled\findings.jsonl | `web-telemetry-broker-quarantine-no-recovery-2026-10-17` (backup `findings.jsonl.BEFORE_20261017_tvdquarantine`) |

Restore: copy each `.BEFORE_20261017_quarantine` file back, then rebuild.

## 6. Verification

Game closed during all runs.

- **First gate run.** It failed at `log_w` (it takes one argument). Fixed to `log_f`.
- **Second gate run.** The manifest pin was stale (DLL changed), which is expected. `pin_sidecars.py --payload` updated `VERIFICATION_MANIFEST.json` only; backup `.BEFORE_20261017_quarantine`. `--check` OK.
- **Third gate run** (`TELEMETRY_TESTS=1`, build dir `%TEMP%\f1c-web-telemetry-1.2.2-quarantine`): **EXIT 0**.
  - VIEWER PASS 765.
  - Soak and fuzz: 0 crashes.
- **Direct `runtime_test.exe`:**
  - `PASS broker quarantine self-heal (register again, 2 s throttle, failed register, native budget)`
  - `PASS TV Director camera change offline checks ...`
  - `PASS 7493147 checks`.
- **Payload DLL** `06DF8EB6534992849B0FB49310D084775C4C3C8C1D31ACC9E9AFF0EF89818D42` (in `%TEMP%` only, not installed). It supersedes the earlier 1.2.2 DLL `923927A0…`.
- **Not run:** a live game test, and a real broker quarantine (the offline mock covers only the client side).

## 7. Pending

- Ask the reporter for `f1c_game_update_broker_log.txt` from their game folder, to confirm the quarantine line and its elapsed time.
- Live (owner, game running, test build): TV Director cuts. The log must show "native budget ... available" at start and no quarantine. Force check: none available without a slow PC; the self-heal path is covered offline only.
- Release with 1.2.2 (already pending its CO-OP live check and diff review, see HANDOFF_TELEMETRY_LL_2026-10-17.md).
- Pin refresh (`pin_sidecars.py`) is only needed if sidecars change; this change is DLL-only.
