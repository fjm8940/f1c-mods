# Telemetry 1.1.0 storage v2, batch 1 record (2026-10-13)

Scope: spec `STORAGE_MANAGER_SPEC_2026-10-13.md` section 12, steps 0, 1, 2, 3 only.
Stage: implement and verify (not promoted; coordinator review pending).
Not in this batch: writer thread, capture hook, folder layout, jobs, HTTP API, UI, viewer.
Runtime recording behaviour: unchanged (the new core is compiled but not called).

## 1. Facts (verified by command output)

- Game process check before every gate run: `tasklist | grep -i "f1 challenge"` returned nothing.
- Baseline before any edit (unmodified sources, `%TEMP%\f1c-web-telemetry-baseline-b1`):
  - `python tools/f1c_mod.py build` FAILED at `test_telemetry.py:93` (payload pin, see 4.1). Build stopped there.
  - `runtime_test.exe` (standalone): `PASS 46465 checks`. The handoff quoted 46,472; the difference is not explained.
- After batch 1 (`%TEMP%\f1c-web-telemetry-1.1.0-b1`):
  - `runtime_test.exe`: `PASS 49195 checks` (+2,730 new checks; storage v2 tests included).
  - Viewer (`node viewer_test.js`): `VIEWER PASS 416 checks` (unchanged; viewer sidecar not touched).
  - Official gate: FAILED at `test_telemetry.py:93` (payload pin, see 4.1). Quoted: `AssertionError: ('web_telemetry.dll', '35C9AA5D9E378ECCFDE6420038607310EF11E73E72D3CD566687E3832DFE68F4', '87AAA25B713CD7A288D9BF3E4EF560B72923D2EF6E3776ECDE3C1E53003CDC1C')`
  - Diagnostic run (copy of `test_telemetry.py` in `%TEMP%`, payload-pin assertion replaced by a printed line, component path pointed at the workspace): `rc=0`. Passed sections include `PASS payload gate`, `PASS storage v2 core`, `PASS non-loopback HTTP health`, `PASS contract: schemas validate every live document, SSE event and error; api_version 8 pinned`, `VIEWER PASS 416 checks`, `AUDIT MODEL 22/22 PASS`, `PASS BACKEND 3 fuzz: 40 shards x 2000 iters`.
  - Host 64-bit clang-cl AddressSanitizer build of the storage v2 tests (`%TEMP%\b1-host\asan_storage_v2.exe`): `host ASan/UBSan storage v2: 2730 checks`, no sanitizer report. UBSan runtime not available on this machine; see 4.4.
- DLL: `%TEMP%\f1c-web-telemetry-1.1.0-b1\web_telemetry.dll`, size 1,188,971 bytes, SHA256 `87AAA25B713CD7A288D9BF3E4EF560B72923D2EF6E3776ECDE3C1E53003CDC1C`.

## 2. Files

Before-state copies (complete, relative paths kept): `workspace_organization_audit\TELEMETRY_110_BATCH1_BEFORE_2026-10-13\f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry\`
(`web_telemetry.c`, `mod.json`, `web_telemetry.dll.mod.json`, `f1c_mod.json`, `runtime_test.c`, `test_telemetry.py`).

Edited:
- `web_telemetry.c`: `TELEMETRY_VERSION` 4.4.0 to 1.1.0; `PRODUCT_LABEL` "Telemetry 1.0.1" to "Telemetry 1.1.0"; one include line `#include "storage_v2.inc"` before `session_archive.inc`.
- `mod.json`: version 4.4.0 to 1.1.0.
- `web_telemetry.dll.mod.json`: version 1.0.1 to 1.1.0.
- `f1c_mod.json`: version 4.4.0 to 1.1.0. `build.dll` path left unchanged (see 4.5).
- `runtime_test.c`: storage v2 tests added before `main`; `test_storage_v2()` call added to `main`; expectation `"version":"4.4.0"` changed to 1.1.0; `Telemetry 1.0.1` expectation changed to 1.1.0.
- `test_telemetry.py`: version expectations changed to 1.1.0 (lines 125, 180, 183, 185, 195, console generator check at 205) and `Telemetry 1.1.0` (health product, line 431).

Created:
- `storage_v2.inc` (CRC32, zigzag and varint, Q16 quantizer, channel catalog and table, file header, block framing, DATA encode and decode, bounded reader `sv2_scan`, cabinet.dll adapter, AIW parser, session naming, session id, weekend grouping, collision suffix). Included only; no runtime call.
- This record.

Not touched: `README.md`, `CURRENT_STATE.md`, `VERIFICATION_MANIFEST.json`, `telemetry_viewer.js`, `build.bat`, schemas, `API_VERSION` (still "8"). `CURRENT_STATE.md` shows a modification time after this session began; not made by this batch.

## 3. Tests added (runtime_test.c, `test_storage_v2`)

- U-1 quantizer (1,000,000 values per Q16 channel, NaN and inf clamp with flag).
- U-2 zigzag and varint (edge values, length 1 to 5, malformed lengths refused).
- U-3 CRC32 check value `0xCBF43926`, incremental CRC.
- U-4 DATA round trip (400 random blocks, 104-car block, exact for Q16 indices and RAW32; truncation refused; AI car with player-only channel refused).
- U-5 header and table (bad magic, version, header_bytes, CRC, table CRC, declared count, duplicate id, tz and session_type round trip).
- U-6 block framing (magic, header CRC, type, oversize; seq gap and payload CRC inside a file).
- U-7 XPRESS and LZMS round trip, raw fallback for incompressible data, mixed file read back.
- U-8 AIW parser (spec 3.4 table of 8 rows, 200 generated strings plus well-formed generated paths).
- U-9 naming (stem, session id, collision `_9` after `_2`..`_8`, weekend key and grouping).
- F-1 truncation at every byte offset of three reference files (raw, XPRESS, LZMS).
- F-2 1,000,000 random mutations per reference file (3,000,000 total).
- B-1 cabinet forced unavailable at write time: raw blocks, flag set, archive probe refused.
- B-2 cabinet forced unavailable at read time: compressed block reported `DECODER_UNAVAILABLE`, other blocks read.

## 4. Deviations, interpretations and open issues

4.1 Payload gate fails before my changes and after (pre-existing). `VERIFICATION_MANIFEST.json` `payload.files` pins are from the 4.3.0-rc.2 build (2026-10-11). The manifest is not regenerated by any script in this component (`pin_sidecars.py` writes only sidecar pins). Coordinator action needed: re-pin the payload hashes for the final build (`web_telemetry.dll` 87AAA25B..., and `web_telemetry.dll.mod.json` 0C48D6F6127E3540436394C5530F32941F7287DCBE15A45E1D637E81814CDEF5). I did not edit the manifest.

4.2 Spec U-1 bound. The spec bound `(max-min)/131070 + 1e-7` cannot hold for decoded binary32 values with large magnitude (rpm 25000 has half an ulp of about 0.001). The test tolerance adds `1.1920929e-7 * max(|min|,|max|)`. Spec amendment proposed.

4.3 Interpretations to confirm:
- DATA `dt_ms`: stored as zigzag varints, `dt_ms[0]` must be 0; decoder refuses otherwise.
- Channel scope: `gvert` taken as ALL from the catalog, although 4.7 lists it as not stored for AI (OQ-7 open). `steer` is AI-only (catalog), which matches player steering being unavailable in the runtime test.
- AIW rule 4 (parent folder equal to base) is implemented as written; it is unreachable because rule 3 already matches that case.
- AIW sanitize keeps letters and digits only; `_` is dropped as well as other symbols.
- Weekend OQ-4 proposal (Test Day after a race splits the weekend) is implemented as written; it is a PROPOSAL, not signed off.
- Collision suffixes go to `_9999`; the spec test covers `_2` to `_9`.

4.4 Robustness. UBSan not run: no UBSan runtime for this toolchain (mingw64 has no libasan/libubsan; LLVM ships ASan for the MSVC target only). ASan run only. The 32-bit DLL build and runtime_test were not run under sanitizers.

4.5 Version labels. `web_telemetry_usage.h` has no version literal; it passes `TELEMETRY_VERSION` to `f1c_usage_render`, so usage reports now carry 1.1.0 automatically (spec step 0 said NOT FOUND). `build.bat` default `TELEMETRY_BUILD_DIR` still names `f1c-web-telemetry-4.4.0`; env var overrides it. `f1c_mod.json` `build.dll` path not changed.

4.6 Viewer footer. `telemetry_viewer.js` (untouched, pinned) shows `Telemetry 1.0.1` from its own `PRODUCT_VERSION` constant, not from `/health`. Spec step 0 says "verify", and the answer is no. The `VERSION = '4.4.0'` constant also remains. Needs a sidecar change and pin update in a later batch, or an explicit owner decision.

4.7 Memory layout. The in-memory car block uses a caller-owned value pool (`vals` pointers); the on-disk byte tables follow spec 4.4 exactly. Not in the spec; no runtime use yet.

4.8 Not measured: LZMS working-set size (spec 6.4, NOT FOUND); `cabinet.dll` exports on Windows 8 (OQ-1b). The adapter creates and closes a compressor per call; the writer batch should cache handles.

## 5. Restoration

- Remove the include line from `web_telemetry.c` and restore the six before-state files from the audit folder; delete `storage_v2.inc` (new file, safe to quarantine; agents do not hard-delete).

## 6. Documentation gate

- This record exists in the component folder. It is not yet indexed by `README.md` or `CURRENT_STATE.md`; those files were out of scope for this batch and the coordinator should add the link.
- Facts (section 1) are separated from interpretations (4.3) and open items (4.1, 4.4, 4.6, 4.8).
- Pending validation: coordinator review; payload re-pin (4.1); UBSan run (4.4); owner live test is not part of this batch (no game run, no live probe).


## Coordinator review (Claude, 2026-10-13)

- The gate failure at `test_telemetry.py:93` was not a stale manifest: the DLL changed, and the documented procedure (handoff section 5) is `python pin_sidecars.py --payload <build dir>` then the gated build. Claude ran it (manifest before-state copied to the batch-1 before-state folder). Result: gated build rc=0, all sections PASS, including `PASS storage v2 core: U-1..U-9, F-1, F-2, B-1, B-2`, viewer 416, AUDIT MODEL 22/22, soak and fuzz. Game closed (tasklist empty). DLL 87AAA25B713CD7A288D9BF3E4EF560B72923D2EF6E3776ECDE3C1E53003CDC1C.
- Accepted deviations: U-1 float tolerance term (spec to be amended), gvert/steer per catalog, dt_ms zigzag varints with dt_ms[0]=0.
- Carried to batch 4: viewer `PRODUCT_VERSION` still '1.0.1' (telemetry_viewer.js:32); change with pin_sidecars.
- The CURRENT_STATE.md edit at 11:33 was Claude's (spec index line), not a conflict.
