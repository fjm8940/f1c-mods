# STOP 4 test plan — workbench implementation and offline gates (2026-10-09)

Status: prepared during STOP 4 preparation. **Execute G6 and the final G7 only
after the owner confirms parallel port task A has landed and been reviewed**,
because they edit `web_telemetry.c`, `runtime_test.c`, `test_telemetry.py` and
`build.bat`. G1–G3 and G5's recorded-budget checks already run offline against
the preparation artifacts. Assignment:
`DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`; design authority:
`legacy_dossier/STOP3_LEGACY_TELEMETRY_2026-10-09.md` (STOP 3R, accepted);
review conditions: `legacy_dossier/STOP3_CLAUDE_REVIEW_2026-10-09.md` (STOP 3R
re-review).

## Artifacts under test

| Artifact | Location |
|---|---|
| C decoder port (not wired) | `legacy_dossier/c/legacy_formats.{h,c}` |
| C offline harness | `legacy_dossier/c/legacy_formats_test.c` |
| Workbench UI/JS draft (not wired) | `legacy_dossier/stop4_draft/` |
| Node unit tests | `legacy_dossier/stop4_draft/rpm_test.js` |
| Python reference decoder (unchanged) | `legacy_dossier/tools/legacy_formats.py`, `tools/test_legacy_formats.py` |
| Product integration (after A) | `web_telemetry.c`, `runtime_test.c`, `test_telemetry.py`, `build.bat` |

## Fixtures

| Fixture | Source | Identity |
|---|---|---|
| `telemetry.spt` (torn/tail case) | vendor tree, read-only in place | SHA256 `AA04E86E3C64311AC0D4AF93003E6AB82320162AAA5AB0B9F7FF6FE823B308E2`, 4,505,600 B |
| `reference_2002_hockenheim.lap` | vendor tree, read-only in place | SHA256 `6AA795B5FC05E3ADB8BAB5F4FE8350824AA095E4F3FA0A8434CC48E2CADE27AF`, 487,843 B |
| Synthetic live frames + crossing/pause/gap scenarios | generated in Node tests | `stop4_draft/rpm_test.js` builders |

No vendor bytes are copied into the workspace. `F1C_LEGACY_TREE` overrides the
vendor root for the C harness.

## Gates

| Gate | Scope | Command | Expected evidence |
|---|---|---|---|
| G1 | C decoder unit suite: real-file facts, synthetic round trips, hostile/truncated refusals, payload rows, RPM helper | `gcc -O1 -static ... legacy_formats.c legacy_formats_test.c -lm; legacy_formats_test.exe` | `70 checks, 0 failures` (preparation result; re-run at integration) |
| G2 | Python/C parity: both suites assert the same facts on the same vendor files | `python mods/sdk_sandbox/web_telemetry/legacy_dossier/tools/test_legacy_formats.py` | pass (unchanged 56 checks) + G1 pass |
| G3 | JS unit suite: RPM conversion and label guard, validity gating, segment breaks, crossing rotation, ring bounds, freeze/resume, mid-lap selection, trace import no-double-conversion, recorded budgets | `node mods/sdk_sandbox/web_telemetry/legacy_dossier/stop4_draft/rpm_test.js` | `18 passed, 0 failed` (preparation result) |
| G4 | RPM rule (owner condition): live `c.r`/`c.mr` are rad/s; convert ×9.549296585513721 before any RPM label; never label raw; file RPM channels already converted | G1/G3 RPM checks + integration: fix the two `dg(...)` gauge labels (`web_telemetry.c` ~4469/~4895) and audit `"eng":{"rpm"}` race-log keys; add an integration assertion that the served page contains the conversion constant and no raw-rad/s RPM label remains | all pass; integration test named in G6 |
| G5 | Budgets (owner condition): SSE remote cost recorded; A/B trace payloads bounded | recorded here + draft constants; integration asserts response bounds | `/telemetry` ≤ 262,144 B (measured 31,016 B at 22 cars), 10 Hz SSE ≈ **310,160 B/s ≈ 2.48 Mbit/s per client**, 16-client cap ≈ 4.96 MB/s aggregate; `/trace/*` ≤ 524,288 B (measured exports A 218,453 B / B 328,121 B for real laps), compare ≤ 1000 points on a 1–50 m grid; trace memory 80 B/sample × 2 cars ≤ 2.7 MiB; truncation counters present and reported |
| G6 | Integration (after task A lands and is reviewed): decoder wiring + workbench behavior | canonical build `TELEMETRY_TESTS=1`; `runtime_test.c`/`test_telemetry.py` additions | `POST /trace/import/legacy` bounded body → C decoder → `f1c.trace/1` conversion; conversion table with the parallel-C corrections (time→s, distance→m, velocity **mph→m/s (1/2.2371365)**, accel **g→m/s² (x9.8)**, pedals 0..100→0..1, engine rpm already RPM, gear float→int, yaw degrees→rad, spt distance/speed derived per DM#8, delta per DM#14 in the UI — every conversion listed in the document, never silent); real lap hash gate; torn spt; hostile refusals; open-lap serving bounds; live partial drawing; Hockenheim early-counter crossing fixture (reuse the rc.3 replay); freeze/resume leaves capture counters advancing; mid-lap selection revision reset; linked-cursor distance math shared A/B; G-G point bounds; reconnect shows no catch-up samples and no lines across gaps; browser screenshots (overview, inputs, cursor, freeze, A/B colors, narrow layout) |
| G7 | Final: full SDK gate, validator, doc gate, candidate hash | `python tools/f1c_mod.py sdk-test`; `powershell -File tools\workspace\check-workspace-structure.ps1`; `TELEMETRY_TESTS=1` build | SDK gate PASS; validator no new errors; docs/findings/checkpoint agree; exact candidate DLL SHA256 recorded in `VERIFICATION_MANIFEST.json`; no install without owner action |

**Executed 2026-10-09 on rc.5** (task A landed and was reviewed; owner
authorized the source edits): G1 70/70, G2 pass, G3 21/21, canonical build
41,536 runtime checks + HTTP/Node gates PASS, G7 SDK gate PASS, validator
unchanged, candidate `696E83CF…` reproducible. Remaining for STOP 5: owner
install and native/browser acceptance on the exact hash (and the steering /
rev-limit checks on the next race).

## Abort and refusal rules

- Any gate failure blocks STOP 5; no install, no replacement of the running
  DLL, and no lifecycle claim.
- Refuse (and test the refusal of) oversized files, overflowing counts,
  malformed/truncated blocks, unknown record types and non-ASCII embedded
  blocks. Never interpolate across missing/invalid/teleport segments.
- Do not edit `web_telemetry.c`, `runtime_test.c`, `test_telemetry.py` or
  `build.bat` before the owner confirms task A landed; G6 is the first gate
  allowed to touch them, and only to integrate/verify, never to duplicate
  task A's port work (STOP 3 review item 4).

## Open owner/live items carried in

- Player steering `P+0x000` (filtered) / `P+0x040` (unfiltered) stays OPEN with
  the exact next-race read-only check in STOP 3R section 8.
- Rev-limit band cross-check (`veh+0x39A4`, rad/s) with the steering check,
  next race (parallel B review note).
- spt header 0x00–0x07 finalization on graceful close; .lap trailer remainder;
  spt event payload semantics on a real event (STOP 3R open items).
