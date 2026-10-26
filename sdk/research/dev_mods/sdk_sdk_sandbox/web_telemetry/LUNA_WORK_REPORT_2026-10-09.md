# Luna independent review — Web Telemetry, 2026-10-09

## Finding and repair

Inspection of `CarSide.loadTrace` showed that complete live `f1c.trace/1` imports derived `start` and `end` from the first and last sampled rows. The schema requires `start_sim` and `lap_time`, and the recorder computes both at interpolated physical line crossings. A reproducible live import (`start_sim=100`, `lap_time=90`, sample times 101 and 189) was therefore interpreted as 88 seconds. That can change fastest-lap ranking.

The importer now uses `start_sim` and `start_sim + lap_time` only for complete, non-open, non-truncated live imports with finite start_sim and finite positive lap_time values and boundaries consistent with the sample interval. Consistency allows 1 ms for the JSON's three-decimal timestamp serialization. It rejects overflow, an end before the final sample beyond that tolerance, or a start after the first sample beyond that tolerance. Partial live and legacy imports keep their previous sample-derived range. This does not change the trace schema or map coordinates.

## Coverage matrix

| Audit area | Source review | Executed evidence |
|---|---|---|
| Frontend lifecycle, roster, generation, selections and asynchronous track fetch | Inspected `Workbench.frame`, side reset and bootstrap generation/fetch-token paths. | `audit_regression_test.js`: 22/22 groups PASS; `audit_browser_test.js`: served bootstrap lifecycle and stale async track response PASS. |
| Completeness, gaps, validity flags, interpolation, import and fastest lap | Inspected recorded/imported lap fields, `lapIsFull`, `lapSeconds`, `fastestLap`, and render path gap handling. | Regression suite includes witnessed crossings, partial and malformed imports, live 90 s metadata, non-finite/overflow and inconsistent bounds, legacy fallback, and fastest ranking; 22/22 PASS after final fixture additions. |
| HTTP parsing/framing/deadlines | Inspected bounded request parsing, duplicate Content-Length / Transfer-Encoding rejection, and elapsed deadline checks. | Final RC14 component gate: 17 malformed socket requests returned 400 and health remained available; slow-client/header deadline, oversized line, SSE cap and cleanup PASS. |
| Capture raw validity and import-cache rollback | Inspected raw-value checks before sanitization, recorder ordering, changed-AIW reset and import transaction restoration. | Final RC14 component gate: 42,209 runtime assertions PASS, including raw NaN validity, missing AIW, and forced import formatting rollback. |
| Packaging, metadata and payload identity | Inspected TEMP-only preflight, resolved output/junction checks, exact four-file staging, source/staged/manifest equality, and three sidecar metadata pins. | Final RC14 gate: packaging negative/positive cases PASS; candidate staged byte-exactly; all metadata pins and payload hashes match. |
| Preservation | Compared RC12 files against `WEB_TELEMETRY_RC12_AUDIT_BEFORE_2026-10-09/RESTORATION.json`; compared RC13 against its published manifest. | All four RC12 hashes and all four RC13 hashes match. RC12 and RC13 payloads were not modified. |

Source inspection is not native gameplay evidence. The regression/runtime results are controlled synthetic tests. They do not establish actual browser appearance, game-thread hitch behavior, or acceptance of live lap crossings.

## Candidate and verification

The canonical build produced a distinct private candidate at `%TEMP%\f1c-web-telemetry-4.2.0-rc.14\payload`.

- DLL SHA256: `CDED101EF1867D14CCB506841FF2E44B413EAD6B0590BE2055C7009622D27C74`
- Sidecar SHA256: `2A0ECA305530088C033D573313DE0379DC6672584359C888973303D248276930`
- Component version and sidecar metadata now identify `4.2.0-rc.14`; the schema remains api_version 3.
- `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` passed with `TELEMETRY_TESTS=1`: packaging checks, 42,209 runtime assertions, 22 frontend groups, served bootstrap, endpoint/schema/SSE tests, 17 malformed HTTP requests, 20-worker x 2-hour synthetic soak (flat warm RSS), and 80,000 import fuzz decodes (zero crashes). RC14 results were captured at `%TEMP%\f1c-web-telemetry-import-fuzz-3x956hgq\resource_results.json` (SHA256 `5765744BDEC8E46083508C2B08FFEBBFEFE74A645B82E959BE99357AF6C5C706`): soak wall 15.85 s / CPU 244.75 s / 77.2% / max warm RSS delta 0 KB; fuzz wall 15.76 s / CPU 33.84 s / 10.7%; actual SSE 10.1 Hz. RC14 capture+read was p50 226 us / p95 236 us / max 247 us; callback-only was p50 176.3 us / p95 183.0 us / max 198.6 us. Earlier RC13 measurements remain historical in the audit record.
- A final direct Node run after adding overflow and late-sample fixtures passed 22/22 groups. Those last edits changed only the regression test file; the RC14 source and payload hashes stayed unchanged.
- Parent ran the separate full canonical `python tools/f1c_mod.py sdk-test` gate and reported PASS with seven regenerated files unchanged.
- Workspace validation remains at the baseline two errors (`.claude`, `plans`), zero warnings.

During setup, the first behavior comparison intentionally loaded the preserved RC13 sidecar and reproduced the 88-second result. Initial RC14 gate attempts also stopped at stale candidate metadata pins and an rc.13 assertion in the unchanged harness; the pins and version assertions were synchronized before the passing run. No failure was skipped.

## State, limits and restoration

No install, game write, production change, SDK ABI change, or new address research occurred. The installed state was not inspected. The rc.14 candidate is still at verify -> human validate, not promoted.

The pre-edit restoration map is `workspace_organization_audit/WEB_TELEMETRY_LUNA_REVIEW_BEFORE_2026-10-09/RESTORATION.json`. Restore individual mapped source/metadata files if required, then rebuild into a fresh dedicated TEMP directory. RC12 and RC13 payloads remain intact.

Pending: exact RC14 guarded full-weekend gameplay; early counter and witnessed crossing validation; pause, rewind, pit/garage, identity and track changes; actual-browser presentation and long-session heap; native capture overhead/hitches; and cross-PC Tailscale acceptance. The component README/CURRENT_STATE and API-contract wording need the parent-owned index/status update to name RC14 and the new timing behavior.
