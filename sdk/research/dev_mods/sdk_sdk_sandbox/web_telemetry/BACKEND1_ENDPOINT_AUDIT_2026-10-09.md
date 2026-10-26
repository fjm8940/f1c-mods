# BACKEND 1 — endpoint audit + sidecar staging fix, rc.7 (2026-10-09)

Status: **BACKEND 1 READY**. Assignment:
`DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md` (owner decision: backend first).
Items 1-2 were delivered in rc.6 and accepted by the STOP 5 review; this stop
fixes the review's sidecar defect and executes item 3 (endpoint audit).
Review answered: `STOP5_CLAUDE_REVIEW_2026-10-09.md`.

## Candidate

| Artifact | SHA256 |
|---|---|
| `web_telemetry.dll` (rc.7) | `FDE5D209804E1B3374F1D38D7A5DB7CD80918C5BEC97E049C428A85895A8C53C` |
| `workbench_ui.js` (sidecar, unchanged) | `C095A07E847AEC51DED10AB9EE957F647A11E50DE7C18737A992BB3D87411921` (37,693 B, LF) |

Two consecutive builds produced the identical DLL hash. NOT installed.

## 1. Sidecar staging defect (review item 1) — root cause and fix

- **Root cause (found, not guessed):** `test_telemetry.py` rewrote the served
  `workbench_ui.js` back into the build directory with Python text mode
  (`write_text`), which translates `\n` to `\r\n` on Windows. The staged file
  the owner would install was therefore CRLF (38,514 B, `D565B6E0…`) while the
  source and manifests declared LF (`C095A07E…`, 37,693 B; 821 lines = 821
  extra bytes). `build.bat`'s `copy` itself is byte-exact.
- **Fix:** the Node test now writes the served copy to
  `workbench_ui_served.js` with `write_bytes` (no newline translation), so the
  staged sidecar is never mutated by tests.
- **Byte-exact staging:** after a clean build the staged file is 37,693 B,
  SHA256 `C095A07E…`, byte-identical to the source.
- **New gates:**
  - `verify_sidecar.py` runs inside `build.bat` after the copy and fails the
    build unless `source == staged == manifest sidecar.sha256` (read-only).
  - `test_telemetry.py` asserts the same before starting the server and prints
    the three hashes; it also asserts `/health` equals the manifest hash.
- **/health and the install route compare the same bytes:** the DLL now embeds
  the manifest-pinned expected hash (`WORKBENCH_UI_EXPECTED_SHA256`) and
  `/health` reports
  `"workbench_ui":{"present":…,"bytes":…,"sha256":…,"expected":…,"matches":true|false}`.
  The install route (metadata `sidecars[]`, `PRIVATE_TEST_README`, the
  restoration map) pins the same hash; the gates keep all three in sync.
  Missing sidecar: `present:false`, `matches:false`, page shows the reinstall
  hint (tested).

## 2. Endpoint audit (review item 3)

| # | Finding | Fix | Test |
|---|---|---|---|
| A1 | Slowloris: a trickling header block could hold the single-threaded HTTP handler indefinitely (the retry loop only ended on `tries`) | absolute 5 s header deadline; incomplete requests answered `400 {"error":"incomplete request"}` | fresh-process test: partial `GET /hea` is dropped in <8.5 s, then the server serves `/health` normally |
| A2 | `qs_get` matched substrings anywhere (`xcar=A` aliased `car=A`; `xlap=1` aliased `lap=1`) | key must start the query or follow `&` | `?xcar=A&lap=1`, `?car=A&xlap=1`, `?xid=0` → 400 |
| A3 | A refused request with an in-flight body triggered TCP RST before the client could read the error (the rc.6 flaky `ConnectionResetError`) | `http_close`: half-close (FIN) + bounded 200 ms drain before `closesocket`, used by every handler exit | POST with body now returns its JSON status reliably; all refusal tests stable |
| A4 | Error shape inconsistent: 405/400/404 sent `text/plain` or empty bodies | all handler errors are JSON `{"error":…}`; HEAD is refused with no body (HTTP semantics) | PUT/DELETE → 405 JSON; HEAD → 405 no body; unknown path → 404 `{"error":"not found"}` |
| A5 | Oversized import body answered 400 | 413 Payload Too Large with JSON error (still bounded at 16 MB) | raw `Content-Length: 20000000` → 413 + “16 MB” |
| A6 | `/driver` id buffer could be read uninitialized in theory (short-circuit prevented it) | initialized (`id_text[32] = {0}`) | existing bad-id tests |
| A7 | Duplicate query keys: undefined | first occurrence wins (documented; contract will state it) | `car=A&car=B` returns side A |
| A8 | Numeric parsing | audited: `strtol` end/overflow checks, ranges (`id<MAX_CARS`, `lap<=100000`, `step` 1..50 clamp, `gen` equality) | overflow lap → 400; junk `step` falls back to the default grid step (compared against the default request) |

Audited clean and documented: request buffer 2047 B (oversized lines 400);
path capped at 255 chars (truncation can only 404); POST body ≤16 MB with a 3 s
read deadline; serial accept loop with `Connection: close` (implicit
connection cap 1 for plain requests) and SSE cap 16; CORS posture unchanged
from rc.6 (octet-stream import, no wildcard preflight, wildcard only on
`GET /events`); `/action` GET-only 403 refusal without CORS (behavior
unchanged, per review).

## 3. Environment observation during the audit (recorded, not touched)

The game was running while this stop was executed and the game rewrote the
vendor `telemetry.spt` (live user data):

- 4,505,600 B torn sample (`AA04E86E…`, the STOP 2R/3 fixture) → 651,264 B
  (`226F90C1…`) → 749,568 B (`8E79BB8F…`) within minutes; a 1998 Silverstone
  session (driver “Matthias Steppe”, team “1998 Williams”).
- The 749,568 B read decoded: 57 channels, descriptor end 0x4EE (different
  identity strings), 3,222 player records at 0.1 s, 2,315.9 B/s (matches the
  expected rate), 75-byte torn tail, and **non-zero event records for the
  first time** (`0x01`×2, `0x02`×2, `0x03`×2) — the STOP 3 open item “event
  records present but zero in the observed stream” now has a real sample.
- Consequence: both decoder suites now pin their exact-fact block to the
  preserved hashed TEMP torn copy (`AA04E86E…`) and decode the tree file
  leniently (57 channels, ≥1 player record). C suite **73/0**, Python suite
  **58 checks, 0 failures**. The `telemetry.spt` writer file is mutable by
  design; no future gate may pin facts to its live content.

## 4. Verification (rc.7)

| Gate | Result |
|---|---|
| `build.bat` staging gate | `source == staged == manifest C095A07E…`, 37,693/37,693 B, OK |
| Canonical build `TELEMETRY_TESTS=1` | **41,545 runtime checks PASS**; HTTP/Node gates PASS including the new endpoint-audit and robustness blocks (slow client, oversized line, 16-client SSE cap), sidecar staging/hash, missing-sidecar, workbench/import/port retained |
| C decoder suite | 73 checks, 0 failures |
| Python parity suite | 58 checks, 0 failures |
| Node draft suite | 21 passed, 0 failed |
| Full SDK gate | PASS; regenerated artifacts unchanged (7 files) |
| Workspace validator | same 2 pre-existing errors (`.claude`, `plans`), 0 warnings |
| Reproducibility | two builds identical (`FDE5D209…`) |

## 5. Changed files and hashes (rc.7)

`web_telemetry.c` `AAF2C2C4…` · `sha256.c` `B64AB649…` (uppercase hex) ·
`sha256.h` `10DB6E4D…` · `legacy_formats_test.c` `654C6BC8…` ·
`runtime_test.c` `3E37BB75…` · `test_telemetry.py` `DF2DEC47…` ·
`build.bat` `7CB644BB…` · `verify_sidecar.py` `481AB0D8…` (new) ·
`f1c_mod.json` `82C17B80…` · `mod.json` `C01469E9…` ·
`web_telemetry.dll.mod.json` `BDC1CA31…` · `README.md`/`PRIVATE_TEST_README.md`/
`VERIFICATION_MANIFEST.json` updated (hashes in the manifest);
`legacy_dossier/tools/test_legacy_formats.py` re-pinned.
Full values: `VERIFICATION_MANIFEST.json` `source_sha256`.

Process correction recorded honestly: the rc.6 source bytes were not preserved
as a before-state copy before this round's edits (their hashes are in the rc.6
manifest and the change set is fully described). Before-states will be taken
first for BACKEND 2 onward.

## 6. Pending

- BACKEND 2: frozen API contract (`API_CONTRACT_<date>.md` + JSON schemas +
  contract tests + `api_version` in `/health`; every frontend need incl.
  run metadata and counters).
- BACKEND 3: lifecycle/robustness fixtures, soak/concurrency/fuzz on all
  logical cores (owner CPU rule; workers/seed/utilization recorded).
- BACKEND 4: next-rc candidate, install/restore for the guarded route, backend
  live acceptance checklist + read-only live-check script.

## 7. Wasted-effort check

- Three test iterations in this stop were caused by my own new assertions
  (HEAD body read by urllib, grid-adjusted compare step, lowercase vs
  uppercase hash) — each was root-caused, not retried blindly; the hash-case
  mismatch was exactly what the new expected/matches gate exists to catch.
- The earlier rc.6 `ConnectionResetError` flake was root-caused (A3) and fixed
  rather than retried away.
- The vendor-file rewrite cost one suite re-run; the fixture pinning prevents
  recurrence.

## 8. Documentation gate

Indexed from `README.md`; the manifest, STOP 5 pack and checklist are linked;
facts (hashes, counts), fixes (A1-A8), the environment observation and pending
stops are separated; UTF-8 lines re-read; no credentials or vendor bytes in
the documentation (the live sample is described by hash/size only). Paired
findings entries (`web-telemetry-backend1-rc7-20261009`,
`spt-live-rewrite-event-records-20261009`) and the SDK checkpoint entry
`web_telemetry_zz_backend1_rc7_20261009` carry the durable summary (the `zz_`
prefix keeps same-date checkpoint keys ordered chronologically: the briefing
sorts same-date keys lexicographically).

BACKEND 1 READY - check DS
