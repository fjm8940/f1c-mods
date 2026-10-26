# Telemetry 1.2.2 record: CO-OP remote inputs + [Mod] Enabled wording, 2026-10-17

Stage: implement -> verify (offline). **Source only. Not installed, not released, not deployed. Owner live CO-OP check pending.**
Inputs: [CO-OP channel audit](COOP_REMOTE_CHANNEL_AUDIT_2026-10-16.md); findings `coop-remote-brake-binary-receiver-2026-10-16`,
`coop-remote-throttle-not-applied-2026-10-16`, `web-telemetry-remote-gate-hides-inputs-2026-10-16`,
`web-telemetry-ini-mod-enabled-unused-2026-10-17`; Lightloader spec D1a (owner accepted, see
`development/lightloader/v2.0.0-hub-dev.1/SPEC_2.3.27_TELEMETRY_SETTINGS_2026-10-17.md` line 3).
Owner live check list: [COOP_INPUTS_LIVE_CHECK_2026-10-17.md](COOP_INPUTS_LIVE_CHECK_2026-10-17.md) (+ read-only probe `coop_input_probe.py`).

## What changed (FACT: code)
DLL `web_telemetry.c`
- Steer read (~5386): `if (c->ai_control || c->controller_role == CTRL_REMOTE)`. A mode-2 car gets steer 0x3580 and car_valid bit 8 without the ai_control gate. The local player is unchanged (still no steer).
- `remote_gate` (~4818): clears only bits 2|16|32|64 (throttle, tread, accel). Brake (4) and steer (8) stay. New vf6 bit **4096** = remote brake is on/off only.
- Trace samples for a remote car (~1955): brake and steer are stored when car_valid allows them (same rule as speed). Lap charts can draw them.
- JSON: remote `steerwhy` is now "remote car (CO-OP): no valid network steer in this sample" (only when bit 8 is clear); `ch_tiers` gains a `br` entry and the `steer` entry names CO-OP remote DERIVED.
- Version 1.2.1 -> **1.2.2** (TELEMETRY_VERSION, PRODUCT_LABEL). API stays 10.
- Storage: `storage_writer.inc` NOT edited (R-S held). It already writes brake/steer by car_valid (lines 221-222), so new recordings carry remote brake/steer automatically; replay restores car_valid/vf6 (storage_api.inc ~1810), so PAST mode shows the same labels.

Viewer `telemetry_viewer.js`
- New pure helpers `inputBars(car)` and `steerLabel(car)` (exported). For `cr === 2`: THR label "THR n/a" (title: the game does not apply it on this PC); BRK label "BRK ON"/"BRK off" with an on/off title; Driver Focus bar text ON/off/n/a instead of percent; steer "-0.250 (network)" or "not in this sample".
- Wired into three places: the Viewer Live card (banner ~2083, bars, steer), the Console Live gauges card (the product UI), and the Driver Focus THR/BRK bars.
- Banner: "Brake shows only on or off and steering comes from the network (coarse); throttle, g-forces and tyres are not available here".
- PRODUCT_VERSION 1.2.2.

INI / README (D1a)
- `web_telemetry.ini` `[Mod] Enabled` comment: "Not used: nothing reads this key. Turn Telemetry on or off with the Lightloader Mods list switch." Key kept (old INIs and the 2.3.27 schema row stay valid). Dropping the key from new INIs is left to the release step.
- README draft for the 1.2.2 release folder: `web_telemetry_README_1.2.2_DRAFT.txt` (shipped 1.2.1 README untouched): removes "set Enabled=0", fixes the "first Enabled is Telemetry" line, adds a CO-OP section.
- Not changed: `schemas/telemetry.json`. A description-only edit (vf6 4096) tripped the contract pin ("schema set changed without a contract version bump"); reverted to keep API 10. The bit is documented here and in the C struct comment. Follow-up if the owner wants it in the schema: bump the contract version.

Tests
- `runtime_test.c` `test_coop`: remote car keeps bits 4|8, clears 2|16|32|64, steer -0.25 and brake 1.0 pass through, vf6 = 2|512|4096 only, local player gets no steer/no 4096; JSON has `"steer":-0.250,"steerwhy":""` and the `br` tier; out-of-range steer (9.0) clears bit 8 with the new reason and keeps brake; brake 0 keeps bit 4 and 4096. Fixture values restored (throttle 0.5, brake 0.1, steer 0).
- `viewer_test.js`: 12 new checks (labels, texts, titles, steer labels, banner text, all three surfaces use inputBars). VIEWER PASS 765 (was 753).
- Version pins updated in `runtime_test.c`, `test_telemetry.py`, `viewer_test.js`.

## Verification
- Game closed (tasklist, no F1 process). All builds/tests in `%TEMP%\f1c-web-telemetry-4.4.0`.
- `pin_sidecars.py` (viewer pin 652E99EC... -> 1BCE2671...), gate run 1 STALE payload pin (expected), `pin_sidecars.py --payload`, run 2 contract pin failure (schema edit, reverted), run 3 failed at test_telemetry.py:904 (18085 serve not healthy in time; all earlier blocks PASS incl. contract API 10 and VIEWER 765), run 4: see Gate result below.

## Gate result
- Runs 3 and 4 failed in serve start (test_telemetry.py:904, then the `listen_address` block): `poll_health` got None because the
  serve process exited. Reproduced by hand: `FAIL line 3154: t2_rep.ticks == expect_samples` in `t3_check_file`, the known OPEN
  storage soak flake (TAPE_CLAUDE_REVIEW_2026-10-14 item 9; storage code not touched here). The PC was also running the owner's
  llama-server (~1 GB RSS) during the runs (inference: CPU load makes the flake more likely).
- Standalone `runtime_test.exe` from %TEMP%: 3 of 3 PASS (7.1-7.3 M checks, count varies with the timed loops), about 63 s each.
- **Run 5: EXIT 0, 45 PASS lines** (contract API 10, VIEWER PASS 765, soak 20 workers flat RSS, fuzz 0 crashes). `pin_sidecars.py --check`: pins OK.
- Payload `%TEMP%\f1c-web-telemetry-4.4.0\payload` (temporary, rebuild is reproducible):
  DLL `923927A0EA37591644F2CA136110B7360851DCB46DA616DCEFBD37B9000F9675`,
  telemetry_viewer.js `1BCE2671BDC4A320BE7F06432D521CDEAADF1C662A97FB7ADC2755D5F6BB8306`,
  workbench_ui.js `C4EF4BB4...` (unchanged), web_telemetry.ini `63F62FD6...`, web_telemetry.dll.mod.json `D6B614E5...`.
  Pins rewritten by `pin_sidecars.py` in web_telemetry.c, VERIFICATION_MANIFEST.json, f1c_mod.json, mod.json, web_telemetry.dll.mod.json.
- Probe `coop_input_probe.py`: parses and runs; checked against a fake local /telemetry server (summary shows remote br 0/1.0 and
  steer) and fails soft with no server. Not run against a real game.

- Workspace validator (`tools\workspace\check-workspace-structure.ps1`) after the doc changes: 3 errors, none from this task
  (unexpected top-level `.claude` and `plans`, missing productionPath for f1c-randomizer-2.6.0).

## Evidence tiers
- DERIVED (static): remote brake is 0/1.0 and steer is written by receiver 0x4C73F0; throttle never written. Not live-proven.
- FACT (code): the mod changes above; offline tests.
- OPEN: live CO-OP check C1-C5/H1-H2; steer sign and scale; lap +2 cause (mod seed vs game).
- Inference: the remote steer value range is about -1..+1 (7-bit packet); the check list asks for it.

## Limits / not done
- Throttle stays unavailable (needs a native detour decoding the unused 5-bit packet field; only after H1 shows the host value follows the pedal).
- Lap +2 not changed; needs C5 data.
- Not installed in the test game, no release folder, no catalog, no deploy (owner gate).

## Restoration
Before-copies: `workspace_organization_audit\TELEMETRY_122_COOP_INPUTS_BEFORE_2026-10-17\` (SHA256SUMS.txt, RESTORATION.md).
Copy each file back to restore 1.2.1 source. New files (this record, the check list, `coop_input_probe.py`, the README draft) can stay as evidence.

## Next
1. Owner runs the live check list in one CO-OP session (two PCs) with a 1.2.2 test DLL on the client (for C3) or 1.2.1 (C1, C2, C4, C5, H1, H2).
2. Results into this record and the findings; then release prep 1.2.2 (release folder from the README draft, catalog, deploy only on owner go).
   Release-prep note (from the 2.3.27 session, 2026-10-17): the 1.2.1 `make_release.py` writes catalog.json with indent=1; the live
   catalog used indent=2 until 1.2.0 and Lightloader 2.3.27 (built, not deployed, based on live A28168E8) writes indent=2. Use
   `json.dumps(cat, ensure_ascii=False, indent=2)`, accept either indent on input, and whichever of Telemetry 1.2.2 / Lightloader
   2.3.27 deploys second rebuilds on the then-live catalog. Lightloader 2.3.27 labels `[Mod] Enabled` "Old on/off (not used)" under Advanced,
   which matches the 1.2.2 INI comment.
