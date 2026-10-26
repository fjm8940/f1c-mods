# BACKEND 4 — final candidate, install/restore, live checklist and live-check script (2026-10-09)

Status: **BACKEND 4 READY**. Answers
`BACKEND3_CLAUDE_REVIEW_2026-10-09.md` ("ACCEPTED. Proceed to BACKEND 4")
with its three additions. Assignment:
`DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md` section BACKEND 4.

## Candidate

| Artifact | SHA256 |
|---|---|
| `web_telemetry.dll` (rc.10, final candidate) | `1BA4C53236587C739FF332E6DD71B98349C3F0AD88F05781E8EED950C7311B5B` |
| `workbench_ui.js` (sidecar) | `C095A07E847AEC51DED10AB9EE957F647A11E50DE7C18737A992BB3D87411921` |
| payload `web_telemetry.ini` | `26F66E10C9F0C463CED5D41911969923F66A4A9DDBC390F4F3FCB12051778001` |
| payload `web_telemetry.dll.mod.json` | `B5C6B1ADC7D3E483F0B25B4BEA7592EF780B3E6DE4F9A6B8A0D71C1C56BDC0C3` |

Two consecutive builds produced the identical DLL hash. NOT installed;
install awaits the owner's closed-game step (L1).

## Before-states first

All 34 files this round edited or relied on were copied and hash-verified into
`workspace_organization_audit/WEB_TELEMETRY_BACKEND4_BEFORE_2026-10-09/`
before the first edit (`BEFORE_MANIFEST.json`), including the BACKEND 3
review being answered; the owner's reference lap
(`…\F1 Challenge V10\Telemetry\Reference\reference_2002_hockenheim.lap`,
`6AA795B5…`) is recorded by hash, not copied.

## The BACKEND 4 review additions

1. **Native game-thread capture cost.** The broker callback is timed with QPC
   inside the callback; a bounded ring (240 samples) feeds p50/p95/max and the
   samples-per-second rate into `/health` `"capture_cost_us":{samples,p50,p95,
   max,rate_per_s}` (api_version 3) and into the 10 s diagnostic log line
   (`capture_us p50=… p95=… max=… n=… rate=…/s`). Checklist limit: p95 < 500 us
   throughout the race and no owner-reported frame hitch (L12). Synthetic
   in-harness values: callback-only p50 174.0 us / p95 205.9 us / max 272.0 us;
   the live value is the gate.
2. **Capture time and freshness in the live check.** `live_check.py` polls
   `/telemetry` over a configurable window and records, for every poll, the UTC
   time, `sample_sequence`, `sample_age_ms` and the simulation time `st`.
   Freshness passes only when the sequence and the sim time advance and no
   sample is older than 5 s; identical samples are reported as a paused frame
   and fail the check (AGENTS.md live-diagnostics rule).
3. **api_version 2 exercises in the checklist and script.** Reference kept
   across a crossing (pinned `gen`/`rev` still served, or explicit
   `evicted:true`), eviction reporting through `cars[].evicted[]` + pinned
   request, imported lap on either compare side (`lapA/lapB=import`), and the
   player steering reason (`vf` bit 8 clear + `steerwhy`) with AI steering
   valid. The script performs these checks itself and records the raw values.

## Contract

`API_VERSION` is `"3"`; `/health` adds `capture_cost_us` and `memory`;
`schemas/update_version.py 3` re-pinned the schema set
(`A6DADFA2AAE9EF7A044FBD17632B818B2518C104AD3F27E8E268DD039A0F04B3`);
`API_CONTRACT_2026-10-09.md` carries the v2 → v3 change log.

## Install / restore

- Payload staging: `%TEMP%\f1c-web-telemetry-4.2.0-rc.10\payload` (DLL +
  sidecar + ini + mod manifest; the workspace keeps no payload; the gate
  refuses a workspace `payload/`).
- Map: `workspace_organization_audit\WEB_TELEMETRY_BACKEND4_INSTALL\INSTALL_RESTORATION.json`
  (before/after hashes for DLL, dll.mod.json, sidecar, ini; owner fills
  time/backup paths at install). Restore = close game, copy backups back,
  remove an added sidecar; nothing is deleted.

## Live acceptance pack

- `BACKEND4_LIVE_ACCEPTANCE_CHECKLIST_2026-10-09.md` — L1-L15 with exact
  pass/fail: install identity, Hockenheim crossing, port fallback with
  llama-server on 8080, live A/B via API, RPM scale, steering read + reason,
  rev-limit `veh+0x39A4`, freeze/resume (client-side pin), import + compare
  imported side, export round trip, memory flatness, capture cost, freshness,
  reference/eviction, and the `live_check.py` record gate.
- `live_check.py` — read-only (GETs plus the documented in-memory import
  POST); writes a dated `LIVE_CHECK_<date>_<time>.md` + `.json` result record;
  exit 0 when no FAIL row.

## Verification (rc.10)

| Gate | Result |
|---|---|
| Canonical build `TELEMETRY_TESTS=1` | **41,631 runtime checks PASS**; HTTP gates incl. `PASS contract: … api_version 3 pinned`, payload gate, port/robustness/SSE gates, BACKEND 3 resource gates (20×2 h soak flat RSS, 80,000-decode fuzz 0 crashes) |
| Full SDK gate | PASS; regenerated artifacts unchanged (7 files) |
| Workspace validator | same 2 pre-existing errors (`.claude`, `plans`), 0 warnings |
| Reproducibility | two builds identical (`1BA4C532…`) |

## Changed/new files

Changed: `web_telemetry.c` (capture-cost ring + `/health` fields + memory,
api_version 3, rc.10), `compatibility_runtime.inc` (callback timing + diag
line), `runtime_test.c` (health assertions, advancing synthetic serve feed),
`test_telemetry.py` (rc.10/api 3/health fields), `contract_validate.py`
(health rules), `schemas/health.json` + `version.json`, `build.bat` and the
three metadata files (rc.10), `API_CONTRACT_2026-10-09.md` (v3),
`VERIFICATION_MANIFEST.json`. New: `live_check.py`, the checklist, the
install/restore map, this record.

## Wasted-effort check

- One gate run failed on the (my own) health `samples` offset in the C test
  (`atoi` landing on the colon); fixed at the source and re-run.
- The synthetic live-check smoke first reported the reference check as PASS
  without observing a crossing; the check now distinguishes "crossing
  exercised" (PASS), "served but not exercised" (WARN) and "explicit eviction
  without an observed crossing" (WARN) so the owner's real race is the gate.
- The synthetic serve feed was extended to advance one sample per transmit so
  the freshness/SSE gates are exercisable offline; trace seeds stay untouched.

## Pending (owner, after install)

L1-L15 of the checklist on one full-weekend Hockenheim race, including the
native capture cost p95 < 500 us without a frame hitch, the live steering
source check, the rev-limit read, and the `live_check.py` record with no FAIL
rows (WARNs re-run later in the race). Results update this record, the
manifest `native_validation`/`installed` and `findings.jsonl`.

## Documentation gate

Indexed from `README.md`; the manifest links the record, checklist and script;
facts (hashes, gate output, smoke result), decisions (capture-cost ring,
freshness rule, install route) and pending owner steps are separated; UTF-8
lines re-read; no credentials or vendor bytes in the documentation. Paired
`findings.jsonl` entry and the SDK checkpoint carry the durable summary.

BACKEND 4 READY - check DS
