# STOP 1 record — legacy telemetry inventory, PE inspection, isolated viewer IDB, function coverage

Date: 2026-10-09. Assignment: `DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`
(owner-directed). Authority followed: root `AGENTS.md`, `development/mods/MOD_STANDARD.md`
(lifecycle, records, evidence separation; cited per assignment), `governance/README.md`,
`governance/PIPELINE.md`, `f1c_ida_decompiled/AGENTS.md`. Pipeline position for this
research: **investigate → specify** for the legacy reconstruction; the Web Telemetry
product itself remains **verify** (rc.3 native acceptance still open, untouched by this
stop). Canonical destination: this dossier under the existing Web Telemetry component;
no second product was created.

## 1. What was done

1. **Front-door capability added** (required because the viewer is not the game
   target; the existing `ida-prepare` copies only the game IDB and validates the
   game EXE digest):
   - `python tools/f1c_mod.py ida-new-project legacy_telemetry --exe "…\f1_telemetry.exe"`
     creates an isolated IDB from a non-game PE via IDA batch mode in `%TEMP%`
     (vendor file read-only, copied to TEMP, hash-verified), refuses overwrite,
     and writes a provenance JSON naming the target hash. It never touches the
     game IDB.
   - `python tools/f1c_mod.py ida-survey --idb PATH --output FILE.json` performs a
     one-pass read-only survey (functions, imports, exports, strings with
     addresses, string xrefs, function call edges) on a disposable IDB copy and
     verifies the source IDB hash unchanged.
   - Both are wired into `tools/f1c_mod.py`, `tools/ida_bridge.py`, and the tool
     catalogue; a new tooling test covers refusals, provenance, survey content
     and overwrite protection.
2. **Isolated viewer IDB prepared** and surveyed (see artifacts below).
3. **Full recursive inventory with SHA256, sizes, mtime, magic and category**
   for the vendor Telemetry tree plus `Options\Telemetry.doc` (read-only).
4. **PE inspection** (sections, imports, exports, resources, version metadata,
   VB markers) with machine-readable and readable outputs.
5. **Native function-coverage inventory**: every IDA-identified function
   classified with evidence; VB6 p-code boundary stated; unresolved list empty
   by evidence, not by assumption.

## 2. Evidence and artifacts

| Artifact | Path / value |
|---|---|
| Viewer EXE SHA256 (matches assignment's known evidence) | `0AA34C51C30C3113BF6B6C2F1B59A8B3346AD1322F6AB519E60757BAF8573BEB`, 331,776 bytes |
| Viewer IDB (isolated, front-door created) | `mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.i64`, SHA256 `3E66F6DC306FFC6B0086D784FA5F81169A4D7D15C333544957D153A0387E7985` |
| Provenance | `mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.provenance.json` |
| Inventory (171 files, 11,098,901 bytes) | `legacy_dossier/INVENTORY_f1_telemetry_2026-10-09.json` |
| PE report (JSON + markdown) | `legacy_dossier/PE_REPORT_f1_telemetry_2026-10-09.json`, `legacy_dossier/PE_INSPECTION_f1_telemetry_2026-10-09.md` |
| IDA survey | `legacy_dossier/SURVEY_f1_telemetry_2026-10-09.json` (134 functions, 50 imports, 1,249 strings, 582 string refs, 82 call edges) |
| Function coverage (JSON + markdown) | `legacy_dossier/FUNCTION_COVERAGE_f1_telemetry_2026-10-09.json` / `.md` |
| Dossier tools (re-runnable, read-only) | `legacy_dossier/tools/{inventory_legacy_tree,pe_report_f1_telemetry,classify_native_functions}.py` |

Inventory counts recomputed (not assumed): 1 EXE, 1 CFG, 23 INI (21 vendor +
2 viewer-written), 136 DAT, 1 LAP, 1 SPT, 1 SVM, 1 JPG, 1 LOG, 4 TXT, plus the
manual = 171 files. Categories: 136 vendor_track_data, 21 vendor_layout,
4 vendor_placeholder, 1 each vendor_application/vendor_config/vendor_layout_image/
vendor_manual, and user-side data: 1 user_recording_store (`telemetry.spt`),
1 user_reference_lap, 1 user_setup_svm, 1 viewer_log, 2 viewer_state
(`_revert_layout.ini`, `_single_layout.ini`).

User-data hashes at this capture (they may change; viewer writes them):
`telemetry.spt` `950082A1F61AB9EE92FDA360CBEF196D54FF714D0F2410E88DAA27486302EDB0`
(1,187,709 B), `reference_2002_hockenheim.lap` `6AA795B5FC05E3ADB8BAB5F4FE8350824AA095E4F3FA0A8434CC48E2CADE27AF`
(487,843 B), `2360498.svm` `7FAF010D6A3FD8328FCA790EBE0A4B12325D3412D4A20559CFE36BD57793FE82`,
`debug.log` `BC0B9B55A0D793139B2FC4A622E1C67D9D296CB812EBC6975B473D60CDA06560`,
`Options/Telemetry.doc` `3A0B8B1FD84181DCF5A552A54915B6FBF0B40FB62495EC69C7AA285D5C4FD541`.

## 3. Findings (tiered)

- **FACT — the viewer is a Visual Basic 6 p-code application.** Imports are
  MSVBVM60.DLL only (50 imports); the `VB5!` project header is present at
  0x00408F04; `.text` holds 319,488 raw bytes but only 1,646 bytes of native
  functions; native stubs follow the p-code dispatcher pattern
  (`mov edx, off_42xxxx; mov ecx, ProcCallEngine; jmp ecx` / `push
  MethCallEngine; retn`). Application logic is interpreted p-code, so IDA
  static analysis alone cannot enumerate application procedures.
- **FACT — native layer fully accounted:** 134 IDA-identified functions = 50
  MSVBVM60 runtime thunks (file I/O, string, UI, time, math, random, system,
  error, engine, entry, support groups) + 78 p-code dispatch stubs (call edge to
  MethCallEngine/ProcCallEngine) + 6 evidence-backed native-glue items (four
  `DllFunctionCall` lazy-import trampolines for kernel32/user32, one IDA
  data false-positive at 0x0041C964, one VBA6.DLL-style far-jump thunk at
  0x0041F178). Accounting: native layer 134/134 classified, 0 unclassified
  native functions; p-code procedures 0/N classified (N pending STOP 2). One
  glue item (0x0041F178) has an open semantic purpose — a semantics question,
  not an unclassified function.
- **FACT — the PE has no Win32 dialog, menu, accelerator or string-table
  resources** (only icon, group-icon, version). VB6 form/menu definitions live
  in the VB project structures, so UI recovery is a p-code/VB-structure task,
  not a classic resource-extraction task. This disproves the naive expectation
  in the assignment's PE checklist for this binary.
- **FACT — build identity:** linker 6.0, GUI subsystem, timestamp
  2003-04-03, no exports, image base 0x400000. No packing (text
  entropy 4.967).
- **OBSERVED — the viewer ran on 2026-10-09–12:54** (owner-side, before
  this stop): it rewrote `telemetry.spt` (12:43:25), created the reference lap
  (12:44:43), wrote the two `_*.ini` viewer-state files (12:53:57) and
  `debug.log` (12:53:59: "file=telemetry\telemetry.spt / reading from telemetry
  file"). The `.spt`/`.lap` are current user data, suitable as real-format
  samples for STOP 2 — in place, read-only.
- **CONFIRMED (file evidence) — the viewer expects engine RPM in the
  thousands:** `Layouts/overview.ini` plots `"time","engine rpm"` with y-range
  `7000,20000`. This agrees with the assignment's known evidence
  (`SpeedMultiplier_0049b8d0` = rad/s × 9.5493 = RPM) and with the legacy
  layout, and it contradicts the "raw values around 1700" presentation the
  modern UI previously showed. The modern workbench must not plot raw angular
  speed under an RPM label.
- **CONFIRMED (file evidence) — the layout language is now partially visible**
  from read-only inspection: a layout is `*<order>_<name>`, a display title, a
  graph count, then per graph a channel row (`"x","z","flags"` with flags
  `NOGRID NOXAXIS NOYAXIS DOTS NOLINES`), x/y ranges, an autoscale flag
  (`#TRUE#/#FALSE#`) and a normalized window rect (x0,x1,y0,y1). The viewer
  itself writes `_revert_layout.ini`/`_single_layout.ini` in the same language.
  `overview.ini` uses a literal `time` x-channel; the assignment's screenshot
  shows a distance axis, so the time/distance axis mode selection is p-code
  behavior to recover in STOP 2/3, not a different layout file.
- **CONFIRMED — file name evidence for formats and stores** (string table,
  viewer SHA256 tagged): `telemetry.spt`, `(*.spt)|*.SPT`, `.lap` +
  `(*.lap)|*.lap` + `*.lap`, `.dat`, `out.data` ("data saved - out.data"),
  `.svm`, `\f1_telemetry.cfg`, `\debug.log`, `\layouts\`,
  `\Reference\Reference_`, `\Reference\reference_`, `\Reference\Default_`,
  `STOP - old format telemetry detected` (versioned format evidence).
  `f1_telemetry.cfg` content observed: `display_in_mph = 0` with `//` comments.

## 4. Open items (carried to STOP 2)

1. VB6 project/p-code parsing: forms, controls, procedures and event handlers;
   map each p-code dispatch stub to its VB procedure and argument record
   (`off_42xxxx` blocks) — the real function inventory.
2. `.spt` and `.lap` binary schemas from the live samples (headers, strides,
   channel descriptors, sentinels) with byte offsets and decoding examples;
   `.dat`, `.svm`, `.cfg`, `.ini` schemas; layout language completion.
3. Telemetry.doc (OLE/CFB) extraction under `%TEMP%` with a reviewed tool
   (`olefile` not yet installed on this Python; install and record versions),
   doc-vs-executable comparison.
4. Time/distance axis mode and cursor/reference behaviors (p-code + behavioral
   evidence).
5. The six evidence-backed native-glue items are classified structurally; only
   the 0x0041F178 far-jump thunk's purpose remains semantically unresolved.

## 5. Verification performed

- New tooling test `tools/sdk/tests/test_ida_external_targets.py`: PASS
  (missing-target refusal, IDB+provenance creation on a compiled probe,
  digest checks, overwrite refusals, survey schema/entry/string/import/edge
  checks). It is registered in both `--suite tooling` and `run_all.bat`, so the
  next full `sdk-test` gate covers it.
- `python tools/f1c_mod.py sdk-test --suite tooling`: **PASS** — all 9 tooling
  tests including the canonical front-door contract, IDA bridge, recovery
  index, status consistency, address harvest/tiering and the new external-target
  test.
- `python tools/f1c_mod.py ida-survey` verified the source IDB hash before and
  after (read-only guarantee). `ida-new-project` verified the TEMP copy hash
  against the vendor file and never wrote to the vendor tree.
- Workspace validator: **2 pre-existing errors (.claude, plans), 0 warnings** —
  same as baseline recorded at orientation; no new errors.
- Full `sdk-test` gate: not run in this stop (focused suite + explicit new test
  run; the full gate is reserved for the implemented workbench per assignment
  STOP 4/5). State this as pending, not as a pass.

## 6. Changed files and hashes

| File | SHA256 |
|---|---|
| `tools/f1c_mod.py` | `614BA95B1A267306CD058F667A0493E09E13F22D1D5253541EA8DFE1F74D13C8` |
| `tools/ida_bridge.py` | `91F7813CD1462A82B36A3F32223C412328C349135EFC628829A9AF9911F7AFB2` |
| `tools/sdk/F1C_TOOL_CATALOG.json` | `A0AA5B7B7978B9634F6E4F5EE39C440FF45940FEB65AD97AFEA4B30AD0D58D88` |
| `tools/sdk/tests/test_ida_external_targets.py` (new) | `3FDFFDF11498E790134D9F11F84BCFD36CD6E614CA1AC24C1AB25703424F4A18` |
| `tools/sdk/tests/run_all.bat` (new test line added to the full gate) | (before-state preserved; one `|| exit /b 1` line added) |
| `mods/sdk_sandbox/legacy_telemetry/README.md` (new) | (project doc) |
| `mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.i64` (new) | `3E66F6DC306FFC6B0086D784FA5F81169A4D7D15C333544957D153A0387E7985` |
| `mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.provenance.json` (new) | (provenance) |
| `legacy_dossier/SURVEY_…json` (new) | `F6A7C0F5F23BE0BB7D7A49C83502A2A6B3472F3C81B4328DB18E9BC3A1C1101E` |
| `legacy_dossier/INVENTORY_…json` (new) | `BC715F38067F82DE2B28F49795EDB90BCE5BFBEA284D11346601E6FCC04BC112` |
| `legacy_dossier/PE_REPORT_…json` (new) | `969446508C605DEF5DC72B598D1E3F18B457106888C7F7C69956B4007D3D8ED9` |
| `legacy_dossier/FUNCTION_COVERAGE_…json` (new) | `244E363A4F5C402F17B13A118AB8E23253E630F9BF8770E6DF2C34412CC57CBE` |
| `legacy_dossier/tools/*.py` (new, 3 files) | re-runnable read-only dossier tools |
| `tools/sdk/AUTOMATION_CHECKPOINT.json` (entry `legacy_telemetry_stop1_20261009` added; `updated_at` refreshed) | `DFA8B2C604A4408CA3FE472179A986CA6C9B7310A94DAF1647E3FD786EA4BC05` |
| `web_telemetry/README.md` (index line added) | before-state preserved (below) |

No game file, loader, broker, installed DLL, production artifact or vendor file
was modified. No new top-level directory was created. The viewer IDB lives in
the existing sandbox project area, not in `analysis/` at the repo root.

## 7. Preservation and restoration

Before-states of every edited existing file are preserved under
`workspace_organization_audit/LEGACY_TELEMETRY_BEFORE_2026-10-09/` mirroring
their workspace-relative paths (`f1c_ida_decompiled/tools/f1c_mod.py`,
`…/tools/ida_bridge.py`, `…/tools/sdk/F1C_TOOL_CATALOG.json`,
`…/tools/sdk/AUTOMATION_CHECKPOINT.json`, `…/tools/sdk/run_all.bat`, and the
Web Telemetry README).
Restore any file by copying it back. The new files and the IDB are additions;
removing them is unnecessary for restoration but, if ever wanted, only through
the owner's quarantine procedure.

## 8. Wasted-effort check

- Repeated reads: orientation docs were read once; the large `f1c_mod.py` was
  read in targeted windows, not fully re-read.
- Broad runs: only the focused tooling suite was run (the full gate is
  deliberately deferred to STOP 4/5).
- Retries: two real defects were found and fixed during capability
  development (IDA batch output is `<input>.exe.i64`, not `<stem>.i64`; an
  argparse help string containing `%` would break `--help`), and one operator
  error (two `ida-query` runs on the same disposable IDB copy in parallel)
  caused a transient idalib open failure that was corrected by sequential runs.
  No speculative loops.
- Polling: none; all native-side work was static/read-only.

## 9. Documentation gate

This record is indexed from `web_telemetry/README.md`; the dossier outputs are
linked; facts, file evidence and open items are separated; viewer evidence is
tagged with the viewer SHA256 and kept out of game exports. UTF-8 re-read
performed after writing. Validator result recorded above.

## 10. Review corrections applied (2026-10-09, after Claude STOP 1 review)

Claude's review (`STOP1_CLAUDE_REVIEW_2026-10-09.md`) accepted this stop with
three corrections; all three are applied before any STOP 2 work:

1. **FUNCTION_COVERAGE accounting reworded** — the misleading "Unresolved: 0"
   now reads "Native layer: 134/134 classified (50 runtime thunks, 78 p-code
   dispatch stubs, 6 evidence-backed glue items; 0 unclassified native
   functions). P-code procedures: 0/N classified (N pending STOP 2)". The
   0x0041F178 contradiction is resolved by wording: its native classification
   is certain; only its semantic purpose is an open item. Regenerated files:
   `FUNCTION_COVERAGE_f1_telemetry_2026-10-09.json`
   `65F4ECE53D52A7AB63163E0B3738F930A0C60FB40521CE97450864F435487009`,
   `FUNCTION_COVERAGE_f1_telemetry_2026-10-09.md`
   `554AD4B32FDAF68BDB43D3E242C342A9A8FF7CBD4620AFC83C82B0D687D37ED9`,
   generator `tools/classify_native_functions.py`
   `CD2CF782B39FE8EDEA64BE2728B8296ABFA70B35FAFB43263D5A09488CD0658F`.
2. **Findings correction appended** — `legacy-viewer-rpm-scale-correction-20261009`
   downgrades `legacy-viewer-rpm-scale-20261009` from CONFIRMED to DERIVED
   (static/inferred) until live or native-tachometer evidence exists. The
   original line was not edited.
3. **Discoverability fixed** — `web_telemetry/README.md` now links both
   `../legacy_telemetry/README.md` (isolated viewer analysis project) and
   `legacy_dossier/` (dossier directory).
