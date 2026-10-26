# BACKEND 3 — lifecycle, robustness and resource gates (2026-10-09)

Status: **BACKEND 3 READY**. Assignment:
`DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md` section BACKEND 3; owner CPU
rule at the end of that assignment. Candidate: Web Telemetry 4.2.0-rc.9
(`web_telemetry.dll` SHA256
`77E8B7987641D3C1F1946B0801274DE855216AE25278B4F41330258B5AF6C636`,
reproducible across consecutive builds, NOT installed).

## Lifecycle sequence fixtures (ordered, single-threaded by design)

`runtime_test.c` now replays the exact native Hockenheim
counter-before-wrap tuples (`NATIVE_BOUNDARY_REPAIR_2026-10-09`), Michael
(slot 0: 635.6 c5 d6775.6592 -> 635.7 c6 d6781.3198 -> 635.8 c6 d8.0820) and
Ralf (slot 1: 637.5 c5 d6770.5161 -> 637.6 c6 d6775.6592 -> 637.7 c6
d2.3410), 6784.1 m, and asserts exactly one complete lap per side (the first
mid-lap partial is dropped, never fabricated), stable `lid` on the completed
lap, and valid status/data documents.

`test_lifecycle()` covers, with explicit assertions each:

| Lifecycle case | Asserted behaviour |
|---|---|
| First partial lap / mid-lap selection | No completion until a witnessed crossing; the next full lap closes |
| Identity/control-mode change | Revision bump, ring cleared, old-revision data request refused `-4`, AI flag tracked |
| Session generation change | Revision bump, old-generation pin refused `-3`, laps gone |
| Track change (inside one generation) | Defensive identity reset (revision bump) so stale lap lengths are never served |
| Replay/rewind | Backwards sim time flags a break and clears the start witness; a lower lap number discards the open lap; nothing fabricated |
| Pause | Repeated captures store nothing; the resume gap is a grid hole (no line across it) |
| Pit/garage/out-lap | No samples in the box, open lap flagged PITTED, the lap spanning the stop completes with the flag |
| DNF/retire | Recording stops; completed data stays readable |
| Teardown/unload | Session inactive -> empty capture; `trace_update(0)` safe; completed laps survive until a real generation change |

The DLL unload path also stays covered by `test_real_dll` (load, refuse
`init(NULL)`, `FreeLibrary`, thread count restored).

## Measured resource gates (all logical cores)

`robustness_backend3.py` (new) runs the CPU-bound offline gates with worker
count = all logical CPUs (detected at runtime, `os.cpu_count()`; never
hard-coded), records worker count, logical CPUs, wall time, per-shard seeds
and measured CPU utilisation, and writes
`resource_results.json` beside the fuzz corpus in `%TEMP%`.

Run of record (final canonical gate after the cap-test fix and the
`--workers` override; numbers from that run; machine: 20 logical CPUs):

| Gate | Measured result |
|---|---|
| Worst-case payload sizes | `/telemetry` 129,028 B (bound 262,144); `/trace/data` 524,160 B, CSV 324,111 B, compare 211,762 B, status 2,949 B (bound 524,288); `/track` 66,019 B (bound 131,072) — all bounded |
| Soak | 20 workers = 20 logical CPUs (`--workers N` overrides), one single-threaded 22-car shard each, **2.00 h of race time per shard** (144,000 ticks at 20 Hz), wall 15.4 s, CPU 77.8%, working-set warm delta **0 KB** (flat), laps >= 50/side (60), ring evictions <= 8 (= `TRACE_EVICT_MAX`) |
| 16 SSE + 1 slow client | 16 clients kept drained and verified receiving (~1 MB/s each), 17th refused by the cap; one slow non-reading peer dropped by the 200 ms send budget; server stays responsive (`/health`) |
| SSE disconnect/reconnect | Measured ~9-10 Hz per stream; sequences non-decreasing within a stream; after reconnect the first frame is never older than the last seen (no catch-up) |
| Game-callback CPU per sample | `capture_on_game_thread` alone (the game-thread path, single-threaded by design): p50 170.7 us, p95 196.1 us, max 290.6 us (synthetic mirror copy 1.9 MB; p95 < 500 us asserted). Capture+read: p50 223 us, p95 283 us |
| Bounded import fuzzing | 40 shards x 2,000 iterations = **80,000** decodes on 20 workers (worker count = logical CPUs, `--workers N` overrides), deterministic seeds 2001-2040, **0 crashes**; 21,082 accepted, 58,918 refused (fail-closed). Corpus + base fixtures (`.lap` DD9937C6…, `.spt` 84C222D9…) + `resource_results.json` under `%TEMP%\f1c-web-telemetry-import-fuzz-grxfev4p\`; the current candidate input is persisted per iteration so a future crash always leaves the exact crasher (`runtime_test.exe import-test <file>` replays it) |

CPU-rule exceptions, stated: the sequence fixtures above and the callback
per-sample measurement are ordered/serial by design; the import fuzz phase is
I/O-bound because every iteration persists its candidate input for crash
preservation (and OS file scanning serialises small writes), recorded with
that note in `resource_results.json`; the mutation work itself runs as one
process per shard with worker count = logical CPUs.

## Defect found and root-caused during this stop

The pre-existing SSE cap test was timing-dependent: it opened 16 streams but
never drained them, and the runtime harness serves ~129 KB per frame, so
clients could be dropped for send-budget reasons while the test assumed all
16 were still held. The old loop then read the 17th stream until the server's
whole test lifetime elapsed (~180 s), surfacing as an intermittent
`/health` connection refusal. Root cause: the test, not the server (the
server correctly refuses the 17th once 16 are held; the refused client
receives the SSE headers before the cap check, so "any bytes" was the wrong
pass condition). Fixed: the cap test now drains all 16 in threads, verifies
they receive, and treats the 17th as refused only when no `data:` frame
arrives. Three consecutive full gates pass after the fix. (No server change
was needed; behaviour is identical.)

## Verification

| Gate | Result |
|---|---|
| Canonical build `TELEMETRY_TESTS=1` | **41,626 runtime checks PASS** (+9 over the fixes round) plus all payload/contract/audit/port/workbench gates and the new BACKEND 3 PASS lines; four consecutive clean runs after the cap-test fix, the last one on the final bytes |
| C runtime suite | PASS (Hockenheim Michael/Ralf, lifecycle matrix, eviction/identity classification, sizes, soak/fuzz/corpus/import-test modes) |
| Python suite | PASS (SSE lifecycle, cap fix, `robustness_backend3.py` orchestration with `--workers`, PERF/PERFG parsing) |
| Node suites | `CMP PASS`, `ENGINEERING UI PASS`, `WORKBENCH PASS` |
| Resource results | `%TEMP%\f1c-web-telemetry-import-fuzz-grxfev4p\resource_results.json` (seeds, per-shard wall/CPU/RSS, corpus hashes) |
| Reproducibility | `77E8B798…` across consecutive builds |
| Full SDK gate | PASS; regenerated artifacts unchanged (7 files); workspace validator unchanged (2 pre-existing errors, 0 warnings) |

## Changed/new files

Changed: `runtime_test.c` (lifecycle fixtures, sizes/soak/fuzz/corpus/
import-test modes, callback-only perf, `-Wl`-linked psapi use), `web_telemetry.c`
(track-change identity reset via `rec_lap_len`), `test_telemetry.py` (SSE
lifecycle, deterministic cap test, PERFG, BACKEND 3 orchestration). New:
`robustness_backend3.py` (this record's resource gate), this record.

## Wasted-effort check

- Three gate iterations were spent on the SSE cap flake; each was root-caused
  (server lifetime, stale-list timing, headers-before-cap) rather than papered
  over, and the final test is deterministic.
- One manual soak run and one resource-script rerun regenerated measured
  numbers; the final record cites the last canonical gate's corpus/results.

## Pending

- BACKEND 4: final candidate + install/restore + backend live checklist +
  read-only live-check script; owner install and one full-weekend race.

## Documentation gate

Indexed from `README.md`; the manifest carries the resource section and this
record; facts (measured numbers, file paths, hashes), decisions (track-change
reset, fuzz preservation design) and the pending stop are separated; UTF-8
lines re-read; no credentials or vendor bytes in the documentation. Paired
`findings.jsonl` entry and the SDK checkpoint carry the durable summary.

BACKEND 3 READY - check DS
