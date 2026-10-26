# BACKEND 2 fixes — rc.9, api_version 2 (2026-10-09)

Status: **BACKEND 2 FIXES EXECUTED**. Answers
`BACKEND2_CLAUDE_REVIEW_2026-10-09.md` ("ACCEPTED WITH 4 CONTRACT FIXES (bump
to api_version 2)"). Assignment:
`DEEPSEEK_BACKEND_FINALIZATION_2026-10-09.md`.

## Candidate

| Artifact | SHA256 |
|---|---|
| `web_telemetry.dll` (rc.9) | `F2ED924A2C1CF46524837298B855120F7C9AD51561EE5F45EB5B895BBFA3E4B5` |
| `workbench_ui.js` (sidecar) | `C095A07E847AEC51DED10AB9EE957F647A11E50DE7C18737A992BB3D87411921` (unchanged) |
| payload `web_telemetry.ini` | `26F66E10C9F0C463CED5D41911969923F66A4A9DDBC390F4F3FCB12051778001` |
| payload `web_telemetry.dll.mod.json` | `1A0DBCB0076DFAAF28ABD70A31FD14C77A747257A6151FBB96753AF7E86EA55C` |

Two consecutive builds produced the identical DLL hash. NOT installed.

> **Supersession note (BACKEND 3, same day):** the rc.9 build was later
> extended with the BACKEND 3 lifecycle work (recorder track-change guard via
> `rec_lap_len`, plus the runtime/robustness additions). The final rc.9 DLL is
> `77E8B7987641D3C1F1946B0801274DE855216AE25278B4F41330258B5AF6C636`; the
> four fix behaviours below are unchanged and re-verified in the BACKEND 3
> gate. `F2ED924A…` was the rc.9 BACKEND 2 fixes-round build.

## Before-states first

All 33 files this round edited or relied on were copied and hash-verified into
`workspace_organization_audit/WEB_TELEMETRY_BACKEND2_FIXES_BEFORE_2026-10-09/`
*before* the first edit (`BEFORE_MANIFEST.json` lists every SHA-256; the four
payload files are recorded by hash rather than copied, per the no-binaries
rule). That set includes the 14 BACKEND 2 before-state files, 12 schemas +
`update_version.py`, the contract, the review being answered, and
`tools/workspace/retention.py` (whose one-time change is described in fix 4).

## The four review fixes

1. **RPM (units unambiguous).** `/telemetry` per-car now carries `rpm` /
   `max_rpm` as converted display RPM (`raw x 9.549296585513721`); `r`/`mr`
   stay raw and the schema spells "raw engine angular speed in rad/s (NOT
   display RPM)". The contract test enforces the rule in both directions: a
   property whose name contains `rpm` may not document rad/s, and any field
   documenting rad/s must say raw (`contract_validate.check_schema_contract`).
   Runtime and live-document tests compare `rpm` against `r x factor` and
   assert no RPM-labelled field carries 1000 rad/s.
2. **Reference lap: stable identity + explicit eviction + imported compare
   sides.** Every completed/imported lap and every status/compare side
   carries `lid` = `{gen, rev, slot, lap, src}` (`serial` for imports).
   `/trace/data` and `/trace/export` take optional `gen`/`rev` pins and
   answer a stale pin with 409 and an evicted lap with 404, both with
   `"evicted":true`; never-recorded answers `"evicted":false`. The recorder
   keeps a bounded `evicted[]` log published in `/trace/status` so a client
   holding an old reference is told it was evicted instead of silently
   getting a new lap. `/trace/compare` accepts `lapA=import` / `lapB=import`
   (the cached legacy import for that side, with its lid and serial), and the
   comparison grid can be built from imports alone (distance frame derived
   from the recorded distances when the live lap length is unknown).
3. **Player steering unavailable with a reason.** `/telemetry` adds
   `steerwhy` (empty when `vf` bit 8 is set; player text names the pending
   live check and the unproven `veh+0x3580` writer; AI text names an invalid
   sample) and `/trace/status` adds `unavailable_why` mapping every
   unavailable channel to a reason. AI steering (0x3580 AI input factor)
   keeps its documented valid path; schema + live contract tests assert the
   player `vf` bit stays clear, `st` stays `null`, and the reasons are
   present.
4. **Payload staging out of the workspace.** `build.bat` stages the install
   payload in `%TEMP%\f1c-web-telemetry-4.2.0-rc.9\payload`;
   `verify_payload.py` gates the file set, byte-equality, AND the absence of
   a workspace `payload/` dir (fails closed if one reappears). The former
   workspace `payload/` (rc.8 DLL `5764AEEE…` + 3 copies) was quarantined via
   `python tools/workspace/retention.py purge --quarantine
   %USERPROFILE%\F1_QUARANTINE_2026-10-09 --only
   f1c_ida_decompiled/mods/sdk_sandbox/web_telemetry/payload` into
   `build_staging/…` with `MANIFEST.jsonl` — not deleted. `retention.py`
   gained a narrowly scoped `BUILD_STAGING` entry (workspace payload dirs are
   disposable build output), a `--only` restriction so this purge touched
   exactly the named path, and skips the memory page store when `--only` is
   used. The rc.8 payload DLL hash in quarantine was re-verified against the
   before-state manifest.

## Contract version bump

`API_VERSION` is `"2"` and `/health` reports it; `schemas/update_version.py 2`
re-pinned the schema set (`737179FDD1082D8F8E5E31107E8CE726C76DEF50B6140F08D3A9117FE8563533`);
`API_CONTRACT_2026-10-09.md` was rewritten as api_version 2 with the
v1 → v2 change log. The gate fails if the schema set changes without the
matching bump.

## Verification (rc.9)

| Gate | Result |
|---|---|
| Canonical build `TELEMETRY_TESTS=1` | **41,584 runtime checks PASS** (+38 over rc.8); HTTP gates PASS incl. `PASS payload gate: staging in %TEMP%, workspace keeps no payload, exactly 4 install files, byte-exact, manifest hashes match` and `PASS contract: schemas validate every live document, SSE event and error; api_version 2 pinned`; all rc.7/rc.8 gates retained (port fallback, slow client, 16-client SSE cap, sidecar staging) |
| C runtime suite | PASS (eviction classification -2/-3/-4, lid patterns, import lid serial, compare import sides, steerwhy, rpm conversion) |
| Python suite | PASS (live rpm factor checks, player reason, identity pins, import compare, schema rules) |
| Node suites | `CMP PASS`, `ENGINEERING UI PASS`, `WORKBENCH PASS` |
| Full SDK gate | PASS; regenerated artifacts unchanged (7 files) |
| Workspace validator | same 2 pre-existing errors (`.claude`, `plans`), 0 warnings |
| Reproducibility | two builds identical (`F2ED924A…`) |

## Changed/new files

Changed: `web_telemetry.c` (fixes 1-3, api_version 2, rc.9, live footer
version), `runtime_test.c`, `test_telemetry.py`, `build.bat`,
`verify_payload.py`, `contract_validate.py`, 11 schemas + `version.json`,
`API_CONTRACT_2026-10-09.md`, `VERIFICATION_MANIFEST.json` (rc.9, api_version
2, `%TEMP%` payload staging), `README.md`, `PRIVATE_TEST_README.md`,
`.gitignore` (the workspace `payload/` ignore removed so a re-created one is
visible; the gate refuses it anyway), the three JSON metadata files
(rc.9). Workspace tool: `tools/workspace/retention.py`. New: this record.
Quarantined (not deleted): former workspace `payload/`.

## Wasted-effort check

- Three gate iterations were spent aligning the new schema rules with the
  contract wording (pinned fields' descriptions must not contain the
  forbidden unit); each was fixed at the source rather than weakening the
  rule.
- The first import-serial test hard-coded `serial == 1`; the runtime harness
  reuses one process for C tests and the HTTP phase, so Python now compares
  serials relationally (`>= 1`, equality with the served document).
- The dashboard footer/version banner were the last hard-coded rc.4/rc.8
  strings; they now splice `TELEMETRY_VERSION` at compile time.

## Pending

- BACKEND 3 (executed next in this session): lifecycle/robustness fixtures
  (Hockenheim counter-before-wrap Michael/Ralf sequences, first partial lap,
  mid-lap selection, identity/control-mode change, session generation, track
  change, replay/rewind, pause, pit/garage/out-lap, DNF/retire,
  teardown/unload, SSE disconnect/reconnect), measured resource gates
  (all-cores soak, 16 clients + slow client, worst-case payloads,
  game-callback CPU) and bounded `.lap`/`.spt` import fuzzing on all logical
  cores with recorded seeds.
- BACKEND 4: final candidate + install/restore + backend live checklist +
  read-only live-check script.

## Documentation gate

Indexed from `README.md`; the contract, schemas, manifest, review and this
record are linked; facts (hashes, gate output, quarantine path), decisions
(payload staging, identity scheme) and pending stops are separated; UTF-8
lines re-read; no credentials or vendor bytes in the documentation. Paired
`findings.jsonl` entry and the SDK checkpoint carry the durable summary.

BACKEND 2 FIXES READY - check DS
