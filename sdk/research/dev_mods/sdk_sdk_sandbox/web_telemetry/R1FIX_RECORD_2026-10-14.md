# R1-FIX record: review gate failure, 2026-10-14

Product: Telemetry (`web_telemetry`), author Matias. Stage: **implement** (debug fix of the R1-B gate).
Verification status: **INCOMPLETE. The full gate has not passed.** The game (`F1 Challenge Online.exe`) was
started by the owner while the work was running, so the last gate could not be run (game-closed rule).
Tags: FACT = checked in this batch. INFERENCE = reasoned, to verify. OPEN = owner or Claude decision.

## 1. Result

- The failure was not a recording problem. The gate's serve-mode server listed a test fixture left behind by
  the test suite, not the owner's recordings folder. That fixture is a 100-byte zero placeholder, so
  `SV2_R_HEADER` is the correct answer for it (FACT).
- Fix applied in source (2 code changes, 1 test-harness change). Not yet verified by a passing gate.
- Gate runs: run 1 stopped at the payload pin (expected after a DLL change); run 2 stopped at a malformed-id
  status (400 vs contract 404), now fixed in source. No `PASS R1-B` line exists yet.

## 2. Root cause (FACT, with evidence)

1. Repro (script `%TEMP%\R1FIX_repro\repro.py`, serve started exactly as `test_telemetry.py` starts it, cwd
   `%TEMP%\f1c-web-telemetry-4.4.0`): `GET /recordings` lists two closed sessions, `S1` (files `S1.f1a`,
   `S1.f1t`) and `S4` (`S4.f1t`). The first closed review id is `v2_6304d9571063e1c8`. Its state is `error`,
   reason `SV2_R_HEADER`, laps 0.
2. The id is FNV-1a 64 of `Rec_A` + `/` + `S1` (recomputed in Python, matches). So the session is folder `Rec_A`,
   stem `S1`, in the test fixture root.
3. That fixture is `%TEMP%\f1c-b2-b4api-<n>\game\Telemetry\recordings\Rec_A\S1.f1t`: 100 bytes, all zero
   (`xxd`). The test helper `b4_make` writes it (`runtime_test.c` near the `b4api` block, line ~3735).
4. The last `sw_root_init` call in the suite sets the storage root to that fixture (`runtime_test.c:3738`) and
   nothing restores it. Serve mode runs after the suite and lists that root.
5. The file opens and its size is read (100 bytes). `sv2_rd_header` then rejects it because it is shorter than
   the 192-byte header (`storage_v2.inc` ~1465). So the open did not fail (`CreateFileA` is not the cause).
6. The owner's real files are not what the gate lists. The Python check by Claude (valid F1CTEL02 headers) is
   correct for those files, but the gate never reads that folder. Premise corrected.
7. Side effect: about 144 `f1c-b2-b4api-*` fixture folders exist in `%TEMP%` from earlier runs. Disposable;
   none deleted.

## 3. Changes (file:line; endings kept)

| File | Lines | Change | Endings |
|---|---|---|---|
| `runtime_test.c` | 4637-4661 | new `serve_real_recording()`: fresh fixture root, copies `%TEMP%\R1_build\r1a_synthetic.f1t` (a valid v2 file, 16597 bytes, written earlier in the same run) to `Rec_R1\S1.f1t`, rebuilds the snapshot | CRLF |
| `runtime_test.c` | 4701 | serve branch calls `serve_real_recording()` before `serve_seed_archive()` | CRLF |
| `storage_api.inc` | 624 | open or size failure in `sa_rev_worker`: `r = -1` (maps to `SV2_R_OTHER` through the default branch of `sa_rev_reason_name`) instead of `SV2_R_HEADER` | LF |
| `storage_api.inc` | 730 | `/recordings/review` with a non-`v2_`+16-hex id: 404 "unknown or deleted session" instead of 400 (API contract section 17, line 515: 404 for "an id that is not v2_ + 16 hex") | LF |

Deviations:
- **D-R1FIX-1 `SV2_R_OPEN` not added.** The reason enum in `schemas/recordings_review.json` is closed, and
  that schema is outside the four files allowed by the task. Emitting a new name would break the schema. The
  open failure uses `SV2_R_OTHER` now. OPEN: owner to allow a schema edit (plus `update_version.py`) if the
  name is wanted.
- **D-R1FIX-2 `test_telemetry.py` not changed.** Its assertions were right: `ready` for the real file, 404 for
  malformed ids (per the contract). The block was fed a fixture, and the test's SKIP condition ("no closed
  recording") never fired because the fixture counts as closed. After this fix the block always runs on the
  real synthetic file (the gate now always exercises the R1-B path, not SKIP).
- **D-R1FIX-3 `storage_v2.inc` not changed.** The header decoder is correct for this file.
- **Line endings (correction):** `storage_api.inc`, `storage_v2.inc`, `test_telemetry.py` are LF; `runtime_test.c`
  is CRLF. Checked byte by byte (Python), after an earlier grep check gave a wrong answer.

## 4. Before-state copies

`%TEMP%\R1FIX_before_2026-10-14\`: `storage_api.inc` (sha256 `071E489A...`), `storage_v2.inc` (`EC6A4E95...`),
`runtime_test.c` (`C044820A...`), `test_telemetry.py` (`88D59ED6...`), `VERIFICATION_MANIFEST.json` (copied before
the pin; the pin changed only its `payload` entry). Nothing was deleted.

## 5. Gate results (full gate: `pwsh -NoProfile -Command '$env:TELEMETRY_TESTS="1"; .\build.bat'`)

- Run 1 (`%TEMP%\R1FIX_repro\gate1.log`): EXIT=1 at `test_telemetry.py:105`, payload pin for `web_telemetry.dll`
  (manifest `C500534C...`, staged `62DC67F5...`). Expected after the DLL changed. Action: `python pin_sidecars.py
  --payload "%TEMP%\f1c-web-telemetry-4.4.0"` (writes the manifest `payload` entry only).
- Run 2 (`%TEMP%\R1FIX_repro\gate2.log`): EXIT=1 at `test_telemetry.py:531`:
  `('v2_ABCDEF0123456789', 400, 'the review id is not valid')` on `/recordings/review`. Fixed at
  `storage_api.inc:730` (section 3). Everything before that line in the R1-B block passed in run 2 (index ready,
  schema validation, `/laps`, `/trace/status`, `/trace/data`, `/telemetry`, the 404 routes, the first bad id).
  The block has no PASS line yet because the assertion stops it.
- Run 3: not run. The game process was running (`tasklist`: `F1 Challenge Online.exe`, PID 20088).
- Compile after all edits (not a gate): `web_telemetry.c` and `runtime_test.c` pass `gcc -fsyntax-only -Werror`
  (exit 0). The `web_telemetry.c` check was rerun after the line-730 edit (exit 0).

## 6. Open (to do with the game closed)

1. Confirm `tasklist | grep -i "F1 Challenge"` prints nothing.
2. Run the gate (about 15 min). It will stop at the payload pin again (the DLL changed). Run
   `python pin_sidecars.py --payload "%TEMP%\f1c-web-telemetry-4.4.0"`, then the gate again.
3. Paste the final EXIT and the `PASS R1-B v2 review` line into this record.
4. Owner decision: `SV2_R_OPEN` (schema edit) or keep `SV2_R_OTHER`.
5. Documentation index: this file is not linked from `CURRENT_STATE.md` (not in the allowed file list). Claude to
   add the index line. `API_CONTRACT_2026-10-09.md` section 17 already states the 404 rule.

## 7. Documentation gate (AGENTS.md)

- Durable record: this file, component root. Not yet indexed (see 6.5).
- Facts and inferences are tagged above. Pending validation: the full gate (section 6).
- `governance\PRODUCT_STATUS.json`: unchanged (no stage change).
- No credentials, `api1.txt` or private keys read or copied.
- `findings.jsonl`: no entry (no game behaviour measured).
- Wasted effort: two full gate runs (about 15 min each). Run 1 stopped at a pin that the build could not avoid.
  Run 2 was needed to find the 400 result, which the earlier review of the test code did not show.
