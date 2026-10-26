# BACKEND 2 — frozen API contract, schemas, contract tests, clean payload (2026-10-09)

Status: **BACKEND 2 READY**. Assignment:
`DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md`. Owner conditions from the
BACKEND 1 review addressed first (see section 1). Review answered:
`STOP5_CLAUDE_REVIEW_2026-10-09.md`.

## Candidate

| Artifact | SHA256 |
|---|---|
| `web_telemetry.dll` (rc.8) | `5764AEEE54D08697B7920F72E134FC742DDDC8C33AB4FD04624677F690B8A434` |
| `workbench_ui.js` (sidecar) | `C095A07E847AEC51DED10AB9EE957F647A11E50DE7C18737A992BB3D87411921` |
| payload `web_telemetry.ini` | `26F66E10C9F0C463CED5D41911969923F66A4A9DDBC390F4F3FCB12051778001` |
| payload `web_telemetry.dll.mod.json` | `79580E50065DC2B177C98ADBC9945D257C33B6C5E2FB55EF1A063ECAAE7531F5` |

Two consecutive builds produced the identical DLL hash. NOT installed.

## 1. Owner conditions (BACKEND 1 review)

1. **Before-states first, every round:** all 14 files this round edited or
   referenced were copied and hash-verified into
   `workspace_organization_audit/WEB_TELEMETRY_BACKEND2_BEFORE_2026-10-09/`
   *before* the first edit (web_telemetry.c, runtime_test.c, test_telemetry.py,
   build.bat, verify_sidecar.py, the three JSON manifests, README,
   PRIVATE_TEST_README, VERIFICATION_MANIFEST.json, .gitignore,
   web_telemetry.ini, workbench_ui.js).
2. **Clean payload dir:** `payload/` (component root) now contains exactly the
   installable set — `web_telemetry.dll`, `workbench_ui.js`,
   `web_telemetry.ini`, `web_telemetry.dll.mod.json` — no test artifacts.
   `build.bat` populates it after the sidecar gate; `verify_payload.py` fails
   the build on any extra/missing file or byte mismatch against the build
   outputs; `test_telemetry.py` asserts the same and that the
   `VERIFICATION_MANIFEST.json` `payload.files` hashes match. The install
   route (`PRIVATE_TEST_README` step 3, manifest `payload`, restoration maps)
   points at `payload/`.

## 2. Frozen contract (BACKEND 2 items 1-4)

- **Document:** `API_CONTRACT_2026-10-09.md` — every endpoint and the SSE event
  with method, params, response fields, units, validity/sentinel rules,
  nullability, versions, error codes, size limits and rates; the
  frontend-needs map (item 4) covers A/B selection, open lap, completed laps,
  reference, freeze, delta by distance, intervals/sectors/statistics, track
  line and positions, G-G, converted RPM, inputs, import/export, run metadata
  (unknown = unknown) and the loss/truncation counters.
- **Schemas:** `schemas/` — `error`, `health`, `telemetry`, `track`, `laps`,
  `incidents`, `pits`, `driver`, `trace_status`, `trace_data`,
  `trace_compare` (JSON Schema 2020-12, strict `additionalProperties:false`
  and full `required` key sets) plus `version.json` pinning
  `api_version` 1 and the schema-set hash
  `71CDE7F44D0B5EF41A309CE60230A8CC6C3CE86A3414669CFF875C541FA0A737`.
- **Versioning (item 2):** `/health` now reports `"api_version":"1"`; the DLL
  carries `API_VERSION`; `schemas/update_version.py` re-pins the set hash and
  is required after any schema edit. Unknown/unavailable data is explicit:
  `/telemetry` uses the per-car `vf` bitmask, trace samples use `null` for the
  nullable channels and the per-sample `f` bitmask for the original ones, and
  the contract documents the sentinels (`-1` ids, empty strings, the dead
  `ms` field, unknown track sectors) — a value without its validity marker is
  never data (section 3 of the contract). This interpretation is deliberate;
  changing `/telemetry` to nulls would break the shipped dashboard and is a
  frontend-visible change the owner has not asked for.
- **Contract tests (item 3):** `contract_validate.py` (standard `jsonschema`
  4.25.1, present in the gate environment) validates every live document
  fetched in the gate — `/health`, `/telemetry`, `/track`, `/laps`,
  `/incidents`, `/pits`, `/driver`, `/trace/status`, `/trace/data`,
  `/trace/open`, `/trace/import`, `/trace/compare` — plus the SSE event
  payload and an error document, with a self-test proving the schemas
  actually reject invalid documents. The version gate fails when the schema
  set changes without a bump.

## 3. Verification (rc.8)

| Gate | Result |
|---|---|
| Canonical build `TELEMETRY_TESTS=1` | **41,546 runtime checks PASS**; HTTP gates PASS including `PASS payload gate: exactly 4 install files, byte-exact, manifest hashes match` and `PASS contract: schemas validate every live document, SSE event and error; api_version 1 pinned`; all rc.7 audit/robustness/port/workbench gates retained |
| C decoder suite | 73 checks, 0 failures |
| Python parity suite | 58 checks, 0 failures |
| Node draft suite | 21 passed, 0 failed |
| Full SDK gate | PASS; regenerated artifacts unchanged (7 files) |
| Workspace validator | same 2 pre-existing errors (`.claude`, `plans`), 0 warnings |
| Reproducibility | two builds identical (`5764AEEE…`) |

Schema-gate iterations fixed during this stop (each root-caused): the
top-level `ll` (leader lap) key was missing from the telemetry schema; the
legacy import documents lacked `"open":false`; the harness's synthetic
`/incidents` canary fixture emitted empty `ty` values (fixture now writes
`SYNTH`). The robustness block's dedicated serve process now runs with
`TELEMETRY_PREVIEW=1` because the slow-client test consumed most of the
12 s default lifetime.

## 4. Changed/new files

New: `schemas/` (12 files incl. `update_version.py`), `contract_validate.py`,
`verify_payload.py`, `payload/` (generated), `API_CONTRACT_2026-10-09.md`,
this record. Changed: `web_telemetry.c` (api_version, import `open:false`),
`runtime_test.c` (api_version check, incidents fixture), `test_telemetry.py`
(payload + contract gates, robustness process budget), `build.bat` (rc.8 +
payload), metadata files (rc.8), `README.md`, `PRIVATE_TEST_README.md`,
`VERIFICATION_MANIFEST.json` (payload + contract sections). Hashes in the
manifest `source_sha256` and `payload.files`.

## 5. Pending

- BACKEND 3: lifecycle/robustness fixtures (Hockenheim sequences, selection,
  identity, generation, track change, replay, pause, pit, DNF, teardown, SSE
  reconnect), measured resource gates (all-cores soak, 16 clients + slow
  client, worst-case payloads, game-callback CPU), bounded import fuzzing on
  all logical cores per the owner CPU rule.
- BACKEND 4: final candidate + install/restore + backend live checklist +
  read-only live-check script.

## 6. Wasted-effort check

- Three schema mismatches were found by the new gate itself (ll, open,
  incidents fixture) — exactly the gate's purpose; each was fixed at the
  source rather than loosening a schema.
- One manifest-update script initially ran in the wrong working directory and
  was re-run correctly; one front-door invocation used the component dir as
  cwd and was repeated from the repo root.
- The robustness process-lifetime failure was diagnosed from the connection
  refused at a known time budget, not retried blindly.

## 7. Documentation gate

Indexed from `README.md`; the contract, schemas, manifest and STOP records are
linked; facts (hashes, gate output), design decisions (validity mechanisms,
payload route) and pending stops are separated; UTF-8 lines re-read; no
credentials or vendor bytes in the documentation. Paired findings entry and
the SDK checkpoint carry the durable summary.

BACKEND 2 READY - check DS
