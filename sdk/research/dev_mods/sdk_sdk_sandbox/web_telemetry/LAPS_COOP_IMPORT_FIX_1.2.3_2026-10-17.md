# Telemetry 1.2.3: laps kept, lapset import, lap strip slider, CO-OP remote laps (2026-10-17)

Reports:
- DVOA (Tester F), 2026-10-17. Screenshots plus `Downloads\f1c_laps_live_Minardi_20261017-1016.json` (3 MB, read only).
- Owner (Matias), 2026-10-17: "for CO-OP human drivers (clients) their lap times come from qualifying and they have a lot more laps than the rest".

Owner instruction: "(owner approval)".

## 1. Lost laps / fastest lap not on the strip (FACT, code)

**Cause.**
- Server ring `TRACE_LAPS 6` (7 slots: six complete + one open) and the viewer list `LIMITS.completedLapsKept: 6` keep only the newest six complete laps. Both overwrite the oldest one first.
- The "Fastest Lap" chip is `WB.fastestLap(displayLaps())`: the best of the laps still held, not of the session.
- So an early fastest lap (Badoer lap 2) vanished and the chip showed L5.

**Fix.**
- Server: `trace_best_slot` / `trace_next_head` (web_telemetry.c). The slot holding the car's fastest clean lap (COMPLETE, not PITTED or BREAKS) is never the next one written. On a tie the newest lap counts as fastest, so equal laps keep the old FIFO order; the existing `test_trace_retention` needed that.
- Viewer: `trimLaps` (workbench_ui.js) drops the oldest lap but never `fastestLap`.

**Limit.** Still six laps per car, live. Memory per car is fixed (TRACE_SAMPLES 3200 × 7 slots). All laps stay in the recording (PAST mode).

## 2. Export cannot be imported (FACT, code + the reporter's file)

**Cause.** Export writes `f1c.lapset/1` (a list of `f1c.trace/1` laps). Import accepted only a single `f1c.trace/1`, so it said "not a lap saved by Telemetry".

**Fix.**
- `Workbench.importSetInto` loads every non-open lap (max 60) into one side; the shown lap is the fastest.
- Viewer import branch for `f1c.lapset/1`.
- Checked with node on the reporter's file: 4 laps loaded (1, 4, 7, 10).

**Open (not changed).** Lap 4 of that file shows 93.599 s from its samples. The file says `lap_time` 93.484, but its last sample (1001.36) is 0.13 s after `start_sim + lap_time` (1001.229). loadTrace then keeps the sample range. Which value is right (game time vs crossing) is not proven.

## 3. Lap strip too wide (FACT, code)

**Cause.** The row already scrolled sideways, but with a thin dark scrollbar and no wheel support.

**Fix.**
- Visible 9 px slider (WebKit + Firefox colours).
- `lapRowEl()`: a vertical mouse wheel over a row that overflows scrolls it sideways.

**Not verified in a browser.** It was not checked which element the reporter's "P2 David C…" crop came from (likely the TV Director on-air card, which already has overflow:auto).

## 4. CO-OP remote laps from qualifying (FACT cause, DERIVED fix)

**Cause.**
- `g_rem_lap[]` (the remote car lap counter, 1.2.0) is a static per-slot counter.
- It was reset only when the car stopped being remote, never at a session change. Race laps continued the qualifying count.
- It was seeded from the game's `lap_number`, which on a client can be stale.
- Best/last lap come from the game's copy of the remote car (`OFF_BEST_LAP`/`OFF_LAST_LAP`), which the owner saw holding qualifying values.

**Fix (web_telemetry.c, compatibility_runtime.inc).**
- `remote_session_reset()` on every new broker session (worker, generation change).
- Seed `remote_seed_lap`: the game lap only when it is not beyond the local field's highest lap +1, else the field's highest lap.
- Remote lap times are timed from the line crossings seen here (interpolated crossing time). They are used only while the game still shows the values it had at session start. Once the game changes them, the game is trusted.

**Not live-tested.** Error of the derived time is about one sample (~60 ms) plus the pose projection.

## 5. Reported, NOT fixed (need the reporter's recording)

- **Qualifying replay.** Cars that finish their 12 laps drop from the replay timing tower, and their early laps become unselectable. The early laps are part 1 (ring limit). The tower drop is not investigated: the PAST frame likely lists only on-track cars (`on_track` = finish_state 0 and not retired).
- **"Not everyone's laps got saved. Minardi's didn't".** The player's (Badoer) row in the replay tower has no best time and tyre "C1". Not investigated; needs the recording `.f1t` from `<game>\Telemetry\`.
- **"Didn't get the option in LL to enable Director mode".** Older INIs are kept on update and have no `[Director]` rows. Lightloader 2.3.27 (built, not deployed) adds the missing keys. The README 1.2.3 tells players to add `[Director] Enabled=1` by hand.

## 6. Files

**Backups.**
- `.BEFORE_20261017_keepbest`: web_telemetry.c, workbench_ui.js, runtime_test.c, viewer_test.js, mod.json, web_telemetry.dll.mod.json, test_telemetry.py, VERIFICATION_MANIFEST.json, f1c_mod.json.
- `telemetry_viewer.js.BEFORE_20261017_keepbest`: copied from the gated 1.2.2 payload, SHA 1BCE2671, the exact pre-edit file.
- `compatibility_runtime.inc`: one added line (`remote_session_reset();`); the shipped 1.2.2 state is that file without the line.

**Version.** 1.2.2 → 1.2.3 in web_telemetry.c (TELEMETRY_VERSION, PRODUCT_LABEL), telemetry_viewer.js, mod.json, dll.mod.json and the tests.

**Tests.**
- runtime_test.c: `test_trace_keep_best`, `test_remote_lap_reset`.
- viewer_test.js: K1 checks.

## 7. Verification

See the release record `development\lightloader\v2.0.0-hub-dev.1\release_telemetry_1.2.3_2026-10-17\READY.md`.
