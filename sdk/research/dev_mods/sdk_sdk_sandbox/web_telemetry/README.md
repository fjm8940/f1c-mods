# Web Telemetry 4.3.0-rc.1

Development record, 2026-10-11.
Pipeline: verify -> human validation pending.
[Current candidate, exact hashes and remaining gates](CURRENT_STATE.md).
[4.4.0 multi-driver (2026-10-12): the console at `/` is the only product UI. `/workbench`, `/viewer` and `/classic` return 404 (sources kept, compiled out). Up to six sides (A to F) in the console; see the multi-driver section of the console backlog.](CONSOLE_BACKLOG_2026-10-12.md)
[4.4.0-rc.1: unified console at `/` styled after f1_telemetry.exe (race, analysis, legacy layouts in one window); classic dashboard moved to `/classic`](CONSOLE_4.4.0_2026-10-12.md).
[4.3.0: unified premium Telemetry Viewer (`/viewer`), acceleration/braking/tyre channels, RESEARCH capture of load/force/suspension/slip candidates, api_version 4](VIEWER_4.3.0_2026-10-11.md).
[Status check 2026-10-10 (rc.14 payload present, not installed)](STATUS_CHECK_2026-10-10.md).
[rc.12 payload audit and rc.13 repairs](RC12_AUDIT_REVIEW_2026-10-09.md).
[Luna verification, rc.14 import timing repair and independent review](LUNA_FIX_REVIEW_2026-10-09.md).
Installed state was not inspected in this audit; installation statements below
are historical and do not establish the currently loaded build.
[Owner-directed DeepSeek assignment: legacy reconstruction and integrated live workbench](DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md).
[Claude handoff 2026-10-09: review trail, candidates, next steps](CLAUDE_HANDOFF_2026-10-09.md).
[Inspection/handoff evidence](LEGACY_REFERENCE_HANDOFF_2026-10-09.md). Assignment prepared; not dispatched or completed. The exploratory live_reference/ files remain unverified draft code.
[STOP 1 legacy dossier: inventory, PE inspection, isolated viewer IDB, function coverage](legacy_dossier/STOP1_LEGACY_TELEMETRY_2026-10-09.md) — [dossier directory](legacy_dossier/), [isolated viewer analysis project](../legacy_telemetry/README.md).
[STOP 2 legacy dossier: VB6 p-code tables, own CFB manual extraction, doc-vs-executable comparison](legacy_dossier/STOP2_LEGACY_TELEMETRY_2026-10-09.md) — [p-code tables JSON](legacy_dossier/PCODE_TABLES_f1_telemetry_2026-10-09.json), [p-code summary](legacy_dossier/PCODE_SUMMARY_f1_telemetry_2026-10-09.md), [Telemetry.doc extraction](legacy_dossier/TELEMETRY_DOC_EXTRACTION_2026-10-09.json), [manual comparison](legacy_dossier/DOC_VS_EXECUTABLE_2026-10-09.md).
[STOP 2R revised: file-format specs, all-method index, exit mismatch resolved, axis mode and layout grammar](legacy_dossier/STOP2R_LEGACY_TELEMETRY_2026-10-09.md) — [format specs](legacy_dossier/FORMAT_SPECS_f1_telemetry_2026-10-09.md), [machine formats](legacy_dossier/FORMATS_f1_telemetry_2026-10-09.json), [method index](legacy_dossier/METHOD_INDEX_f1_telemetry_2026-10-09.json), [layout grammar + axis evidence](legacy_dossier/LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md).
[STOP 3R: spt writer correction, .lap byte-5/byte-7 decode, player/AI channel ledger with the native position-exposure correction, import plus LIVE player/AI workbench design mapped onto the existing `/trace/*`, `/track` and SSE surfaces, and live/import budgets](legacy_dossier/STOP3_LEGACY_TELEMETRY_2026-10-09.md). Port work is owned by the [parallel port task](DEEPSEEK_PARALLEL_A_PORT_2026-10-09.md) and is outside that dossier's scope.
[STOP 4 preparation (not wired; awaits parallel port task A): C decoder port, synthetic fixtures, workbench UI/JS drafts and the full test plan](legacy_dossier/STOP4_PREP_2026-10-09.md) — [test plan](legacy_dossier/STOP4_TEST_PLAN_2026-10-09.md), [C decoder](legacy_dossier/c/README.md), [UI draft](legacy_dossier/stop4_draft/README.md).
The viewer is a VB6 p-code application (isolated IDB under `../legacy_telemetry/`); viewer evidence never enters game exports.
[rc.3 native crossing fix and immediate live display](NATIVE_BOUNDARY_REPAIR_2026-10-09.md).
[rc.4 configurable HTTP port: `[Mod] Port`, 8081-8085 bind fallback, HTTP-disabled path, `/health` and footer actual port](PORT_CONFIG_2026-10-09.md). rc.4 was reviewed and accepted (candidate `22062C6D…`); its port and crossing behavior is preserved unchanged in rc.5.
[STOP 4 implementation — legacy workbench in rc.5: decoder + POST /trace/import/legacy, converted RPM/gear/track trace channels, open-lap serving, /workbench page, RPM label corrections, parallel-C formulas (mph, g, DM#8/DM#14)](STOP4_WORKBENCH_IMPLEMENTATION_2026-10-09.md). Candidate `696E83CF…`, built and offline-verified; NOT installed.
[STOP 5 acceptance — rc.6 fixes the two review items: sidecar `workbench_ui.js` carried by the metadata/install/restore map with `/health` presence+hash, and the cross-origin import guard (octet-stream only, no wildcard preflight); synthetic screenshots + live checklist](STOP5_ACCEPTANCE_2026-10-09.md) — [pack](STOP5_ACCEPTANCE_2026-10-09/README.md), [checklist](STOP5_ACCEPTANCE_2026-10-09/LIVE_ACCEPTANCE_CHECKLIST_2026-10-09.md). Candidate `7E1A690B…`; NOT installed (superseded by rc.7).
[BACKEND 1 — rc.7: sidecar staging byte-exact (CRLF defect fixed; `verify_sidecar.py` + test gate; `/health` expected+matches) and the endpoint audit (slow-client deadline, strict query keys, JSON error shape, 413, graceful close)](BACKEND1_ENDPOINT_AUDIT_2026-10-09.md). Candidate `FDE5D209…`; NOT installed (superseded by rc.8).
[BACKEND 2 — rc.8: frozen API contract (`api_version` 1, `schemas/` + `contract_validate.py` gate, `API_CONTRACT_2026-10-09.md`) and the clean install payload dir (`payload/`: DLL + sidecar + ini + mod manifest, no test artifacts; `verify_payload.py` gate)](BACKEND2_API_CONTRACT_2026-10-09.md). Candidate `5764AEEE…`; NOT installed.
[BACKEND 2 fixes — rc.9: the four accepted review fixes and the contract bump to `api_version` 2](BACKEND2_FIXES_2026-10-09.md): converted `rpm`/`max_rpm` in `/telemetry` (raw `r`/`mr` stay rad/s and are documented raw), player steering explicitly unavailable with a machine-readable reason (`steerwhy`, `unavailable_why`), stable lap identity `lid` (gen+rev+slot+lap+src) with explicit ring-eviction reporting and pinned data/compare requests, `/trace/compare` accepts an imported lap on either side, and candidate payload staging moved out of the workspace into `%TEMP%` (the rc.8 workspace `payload/` was quarantined via `retention.py`, not deleted). Candidate `F2ED924A…`; NOT installed.
rc.2 is installed but rejects native laps when counter increments precede distance wrap; rc.3-rc.9 were never installed; rc.10 is the BACKEND 4 final candidate (workbench, review fixes, api_version 3 contract, out-of-workspace payload staging, BACKEND 3 lifecycle/robustness gates, capture-cost/memory report and the live acceptance pack) and awaits the owner's install + one full-weekend race.
[Frontend to old-viewer parity — rc.11](FRONTEND_PARITY_2026-10-09.md): frontend-only candidate (sidecar `workbench_ui.js` + its pinned hash; no capture/endpoint/contract change, `api_version` 3). Full-window layout engine driven by the 23 embedded legacy layouts (`gen_workbench_layouts.py`), Chassis Overview 2x2 default, lap strip with fastest/reference, separate input traces, linked cursor/zoom/pan, dtime, plain de-duplicated notes, explicit unavailable channels. Candidate DLL `465DB2E9…`, sidecar `7353B8EB…`; built, offline-verified and INSTALLED 2026-10-09 (owner-authorized; rc.10 backed up).
[BACKEND 3 — lifecycle and robustness](BACKEND3_ROBUSTNESS_2026-10-09.md): exact Hockenheim Michael/Ralf counter-before-wrap fixtures; identity/control-mode, session-generation, track-change, replay/rewind, pause, pit/garage/out-lap, DNF/retire, teardown and SSE disconnect/reconnect tests; measured all-cores resource gates (`robustness_backend3.py`: 20×2 h soak with flat RSS and bounded rings, 80,000-iteration bounded import fuzz with deterministic seeds and corpus in `%TEMP%`, worst-case payload sizes, game-callback CPU per sample) and the deterministic 16-clients-plus-slow-client SSE cap test.
[BACKEND 4 — final candidate and live acceptance pack](BACKEND4_FINAL_2026-10-09.md): rc.10 (`api_version` 3) publishes the native game-thread capture cost (p50/p95/max, samples/s) and the process working set in `/health` and the 10 s log line; ships [the owner live checklist](BACKEND4_LIVE_ACCEPTANCE_CHECKLIST_2026-10-09.md) (L1-L15, incl. the api_version 2 exercises and the capture-cost limit p95 < 500 us with no frame hitch), the read-only [live-check script](live_check.py) that records capture time/freshness (identical samples = paused, not a pass) and writes a dated result record, and the install/restore map `workspace_organization_audit/WEB_TELEMETRY_BACKEND4_INSTALL/INSTALL_RESTORATION.json`.
[Native-review repairs and verification](REPAIRS_2026-10-09.md).
The installed/running rc.1 failed native comparison acceptance. rc.2 is installed and loaded; [live retest observations and pending comparison gate](LIVE_RC2_ACCEPTANCE_2026-10-09.md) record the current evidence.
Current owner target: Lightloader manager **2.2.7**, protected host ABI v2 / Hub 2,
broker ABI 15. Its release record pins the same 2.2.6 loader payload used by the
original candidate target. Native telemetry acceptance is still pending. No
Lightloader binary was changed.

[Investigation, specification, evidence and remaining gates](AUDIT_2026-10-08.md).
[Private testing and restoration instructions](PRIVATE_TEST_README.md).

[Full UI/corpus/mod review, 2.2.7 clarification and prioritized roadmap](UI_CORPUS_REVIEW_2026-10-08.md).
Investigation only: 20 correctness findings; no UI or DLL changes in that review.

4.2.0 adds the player/AI engineering comparison and the dashboard corrections
that its data path requires (C02, C06, C07, C08, C09, C10, C15):

- [Channel ledger and performance budget](CHANNEL_LEDGER_2026-10-09.md) - the
  bounded specification: evidence tier, units, signs, ownership and
  missing-value rule per channel, and the measured synthetic budget.
- [Implementation record and verification](ENGINEERING_COMPARISON_2026-10-09.md) -
  what shipped, test evidence, artifact hashes, restoration and open gates.

A new **Engineering** tab compares two recorded laps against cumulative lap
distance (live player/AI or exported `f1c.trace/1` files): speed, throttle,
brake, steering (AI only), lateral/longitudinal acceleration, yaw rate and
tyre tread, with per-channel validity, cursor time delta, interval
entry/min/exit speeds, sector buttons and run statistics. Unverified channels
(player steering, per-wheel suspension/loads/forces) are shown as unavailable
with their evidence recorded, never as zeros. Read-only endpoints:
`/trace/status`, `/trace/select`, `/trace/data`, `/trace/export` (CSV),
`/trace/compare`. `/trace/export` defaults to CSV; use `fmt=json` for portable file import.

rc.5 adds the **live player/AI workbench** (`/workbench` page retired in 4.4.0, see top; renderer still served at
`/workbench.js` from `workbench_ui.js` beside the DLL) on top of the same
endpoints: A/B sides select the player or any AI, a Chassis Overview (track
line, speed vs distance, G-G scatter, RPM vs distance), an Inputs view,
live partial-lap drawing with segment breaks, LIVE/PARTIAL/COMPLETED/FROZEN
states with freeze/resume, a linked distance cursor with the viewer's own
delta interpolation, zoom/pan/reset and explicit missing-data reasons. Trace
records now carry `rpm` (converted display RPM), `gear` and `tx/tz`
(80 B/sample); `GET /trace/open` serves the lap as it is driven; the RPM
gauges, cross-check readouts and race logs no longer label raw rad/s as RPM
(race log: `"rpm"` converted, `"rpm_rad_s"` raw).

Legacy import: `POST /trace/import/legacy?car=A|B` (bounded ≤16 MB) decodes a
viewer `.lap` or game `.spt` with the proven C decoder and returns an
`f1c.trace/1` document with every unit conversion listed
(`velocity mph->m/s (1/2.2371365)`, `accel g->m/s2 (x9.8)`, `yaw deg->rad`,
pedals `0..100->0..1`, gear float→int; spt distance/speed derived per the
viewer's DM#8 formula). `GET /trace/import?car=A|B` serves the cached document;
the workbench Import button posts `.lap`/`.spt` files directly.

The historical `mods/f1cui_web_remote` source and DLLs are preserved. This
candidate retains the v4 observation dashboard, with read-only HTTP, game-thread
capture into owned buffers, and bounded JSON/network handling. Race-control
mutations are refused; HTTP listens on all IPv4 interfaces at a configurable
TCP port, including Tailscale. The default is 8080 (`web_telemetry.ini`
`[Mod] Port=8080`, range 1024-65535, read once at startup). If the port is
already held, the listener tries the next five ports (8081-8085), logging each
failure and the port it binds; if none binds it logs
`HTTP disabled: no port available` and capture keeps running. `/health` and the
page footer report the actual bound port.

From the canonical repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry
$env:TELEMETRY_TESTS='1'
python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry
python tools/f1c_mod.py sdk-test
```

The `TELEMETRY_TESTS=1` gate also runs the BACKEND 3 resource gates
(`robustness_backend3.py`: all-cores soak and bounded import fuzzing, corpus
and `resource_results.json` under `%TEMP%`) and the SSE lifecycle checks.

Build and test output goes to `%TEMP%\f1c-web-telemetry-4.2.0-rc.14`. Set
`TELEMETRY_BUILD_DIR` for a dedicated compiler output directory inside TEMP;
preflight rejects other locations. Keep the manifest DLL path in
sync if overriding it. The build copies `workbench_ui.js` beside the DLL, runs
`verify_sidecar.py` (byte-exact sidecar vs manifest and all metadata pins), then stages the clean
install payload in `%TEMP%\f1c-web-telemetry-4.2.0-rc.14\payload` (DLL +
`workbench_ui.js` + `web_telemetry.ini` + `web_telemetry.dll.mod.json` only)
and runs `verify_payload.py` (file set, byte-equality, and no workspace
payload dir — candidate staging never lives in the workspace; BACKEND 2
review fix 4). The install route and the manifest hashes point at the `%TEMP%`
payload. The manifest canonical EXE digest is required by the SDK build front
door, not used as the runtime compatibility gate.

Active implementation: `web_telemetry.c`, `legacy_formats.c` (C port of the
legacy decoders), `sha256.c` (sidecar integrity for `/health`),
`compatibility_runtime.inc`, the copied latest `lightloader_plugin_abi.h`, and
canonical `tools/sdk/f1c_broker.c`. `workbench_ui.js` is the workbench
renderer served beside the DLL at `/workbench.js` (the private package must
include it; `/health` reports its presence and SHA256).
`web_telemetry.cpp` and the generated planning files are preserved SDK-starter
scaffolding and are not compiled or shipped. `web_telemetry.ini` is the DLL's
settings file beside the DLL; the DLL reads `[Mod] Port` from it once at startup
(a missing file or invalid value keeps the default 8080 and logs the reason).

Logs appear beside the installed DLL as `web_telemetry_log.txt` and
`web_telemetry_race_YYYYMMDD_HHMMSS.jsonl`. `/health` reports build identity,
broker ABI, capture sequence/age, session generation and failed captures.
The page displays LIVE, STALE DATA, TRANSPORT STALE, WAITING FOR GAME, or
disconnected status.

The DLL is pinned until process exit. Restart the game to replace it. The old
injection/self-eject workflow is unsupported; there is no hot unload promise.
`Local\f1c_web_telemetry_stop` stops this candidate's services, but does not
unmap the module. No injection tools are included in the private package.

[4.1.0-rc.2 remote-access correction and verification](REMOTE_ACCESS_RC2_2026-10-08.md).
The earlier localhost-only restriction was an incorrect change to the owner workflow.

[DeepSeek plan: player/AI engineering telemetry](DEEPSEEK_AI_COMPARISON_PLAN_2026-10-09.md).
The plan's phases 0-2 and the phase-4 workflow are implemented in 4.2.0; phase 3
(advanced channel research) stays evidence-gated and its results are in the ledger.

[Live check and startup staging, 2026-10-09](LIVE_CHECK_2026-10-09.md). Native validation awaits owner restart.

[Native acceptance review, 2026-10-09](LIVE_ACCEPTANCE_REVIEW_2026-10-09.md): capture works; engineering comparison fails on AI distance, selection/refresh, identity and partial-lap handling. Corrections and revalidation required.

Engineering polls recorder status every two seconds while open. Changing a car
changes its recorder and discards that side's old laps. Wait for a complete lap
between two observed crossings after changing cars. Imported files require rc.2
track/completeness/time metadata; older ambiguous exports are refused.
Focused synthetic HTTP uses port 18080, leaving the running game's port 8080 intact.

Historical installation notes (superseded for current-candidate selection by
CURRENT_STATE.md; installed state not rechecked):
2026-10-09: exact tested rc.2 installed after owner closed the game;
rc.1 preserved for rollback. Guarded startup and native retest pending.
rc.10 was installed 2026-10-09 (owner-authorized). The rc.11 candidate is built and offline-verified only; the installed DLL is
unchanged (replacement is not allowed while the game process pins it). The
frozen API contract is `API_CONTRACT_2026-10-09.md` (`api_version` 3,
schemas under `schemas/`). The owner acceptance pack is
`BACKEND4_LIVE_ACCEPTANCE_CHECKLIST_2026-10-09.md`, runnable against the live
server with the read-only `live_check.py`.
