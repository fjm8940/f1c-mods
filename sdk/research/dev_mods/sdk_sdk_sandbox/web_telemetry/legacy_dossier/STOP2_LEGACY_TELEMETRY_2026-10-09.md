# STOP 2 record — VB6 p-code parsing, own CFB extraction, doc-vs-executable comparison

Date: 2026-10-09. Assignment: `DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`.
Preceded by Claude's STOP 1 review (`STOP1_CLAUDE_REVIEW_2026-10-09.md`); the three
review corrections were applied first (see STOP 1 record section 10). Authority:
root `AGENTS.md`, `development/mods/MOD_STANDARD.md`, `governance/*`,
`f1c_ida_decompiled/AGENTS.md`. Pipeline position: investigate → specify for the
legacy reconstruction; the Web Telemetry product itself remains verify (rc.3
native acceptance untouched). No viewer launch: `f1_telemetry.exe` was **never
executed** in this stop (it rewrites `telemetry.spt` and the layouts).

## 1. What was done

1. **Own VB6 p-code parser** (`tools/vb6_pcode.py`, read-only, no third-party
   decompiler, no downloads): parses the compiled structure chain
   `VBHeader -> ProjectData -> ObjectTable -> PublicObjectDescriptor[13] ->
   ObjectInfo -> method dispatch tables -> ProcDscInfo (RTMI) -> p-code byte
   streams`, plus the external (Declare) table. Structure layouts follow the
   published community documentation (Alex Ionescu, *Visual Basic Image
   Internal Structure Format*; Sergey Chubchenko, *VB P-Code Decompilation*)
   and every claim used by the parser was verified against this exact binary.
2. **Parser tests** (`tools/test_vb6_pcode.py`): 30 checks, all PASS, including
   the 195/195 RTMI→ObjectInfo back-pointer invariant, one ExitProcHresult per
   method (207/207), a known tiny method's exact stream bytes, determinism, and
   fail-closed negative cases.
3. **Minimal own CFB reader** under `%TEMP%\f1c-legacy-cfb\cfb_extract.py` (no
   `olefile`, no installs) extracting `Telemetry.doc` text via the Word 6.0/95
   FIB (`fcMin..fcMac`); raster/PICF scans for figures.
4. **Doc-vs-executable comparison** (`DOC_VS_EXECUTABLE_2026-10-09.md`) against
   the STOP 1 inventory/PE/coverage evidence and the layout INIs.

## 2. Verified structure facts (all against EXE SHA256 `0AA34C51…F3BEB`)

- **Compilation mode is p-code**: `ProjectData.aNativeCode = 0x0`; the VB header
  declares 233 thunks and 7 forms; LCID 1033; project name
  `F1_Challenge_Telemetry`; project version field `0x1F4` (5.00).
- **13 objects, 207 methods** with names resolving from the binary:
  `General_utility_routines` (50), `Data_management` (27), `home` (60),
  `General_Declarations` (1), `General_WinAPI_Declarations` (14), `FileReading`
  (15), `frmAbout` (2), `LapBrowser` (18), `LapOptions` (6), `LapInfo` (7),
  `General_Translation` (1), `frmPasswordBox` (3), `frmMsgBox` (3).
- **Method name tables are public-only**: `home` exposes exactly
  `redraw_graph`, `add_item_to_graphs`, `draw_item`, `redraw_all_data`,
  `refresh_timing_window`, `set_high_priority`; the other 54 entries are null
  (private/event methods), matching the published description that private
  entries are null in the name array.
- **195 distinct RTMI records**, every one back-pointing to its owning
  ObjectInfo (195/195); RTMI records are 0x30 bytes and the p-code stream
  follows the record, ending with an exit-like opcode (0x13–0x16/0x20) before
  the next RTMI (159/195 streams have an exit-like byte in their last 4 bytes).
- **Opcode evidence (proven subset)**: across all streams the greedy scan counts
  7,474 variable references (0xFF), 5,706 FLdRfVar (0x04), 1,570 ILdRf (0x6C),
  989 LitStr (0x1B), 669 LitI4 (0xF5), 532 ImpAdCallFPR4 (0x0A), and exactly
  **207 ExitProcHresult (0x13) — one per method**. The linear decoder covers the
  same subset and stops at the first unknown opcode (recorded per stream).
- **Declare table** (external table at 0x408E60): 9 API declarations resolve
  with names — user32 (`GetWindowTextA`, `GetWindowTextLengthA`, `GetParent`,
  `GetWindow`, `GetSystemMetrics`, `EnableWindow`), kernel32
  (`GetCurrentThread`, `SetThreadPriority`), shell32.dll (`ShellExecuteA`).
- The 78 p-code dispatch stubs from STOP 1 are the native entry glue for these
  methods; the real logic is the extracted p-code streams (dispatch stub →
  RTMI → stream chain).

## 3. Telemetry.doc extraction

- Container: CFB v3; streams `WordDocument` 1,652,748 B, `\x01CompObj`,
  `\x05SummaryInformation`, `\x05DocumentSummaryInformation`; the FIB is
  **Word 6.0/95** (`wIdent 0xA5DC`, `nFib 104`) so there is no Clx table.
- Text: **12,773 characters** extracted (`fcMin 0x300`–`fcMac 0x34E5`), SHA256
  `34BF078C403EE3676D9661C27197638C72024D2C46E151C2DD6843BC3BC750CB`.
  The full text is the F1 Challenge 99-02 Telemetry manual (views, channel
  meanings, controls, analysis workflow).
- Figures: no raster images or Word 6/95 PICF records validated; two initial
  BMP-magic candidates were false positives rejected by strict header checks.
  Figure extraction is an explicit open item.
- Manual facts now recorded as documented intent: damper velocity sign
  (positive = rebound, negative = compression), temperatures in Celsius with
  compound optima, tyre wear scale stated 1.00–0.00, rain/dampness 0–100%,
  MPH claim that conflicts with the shipped `display_in_mph = 0` (kph) config.

## 4. Findings (tiered, appended to findings.jsonl)

- **FACT** — the viewer is p-code compiled (`aNativeCode = 0`) and the full
  structure chain parses with the published layouts; 13 objects / 207 methods /
  195 RTMI / 207 exits all cross-check.
- **FACT** — `home`'s public method names and the 9 Declare entries resolve
  from the binary; method name tables are public-only with null private slots.
- **FACT** — `Telemetry.doc` is Word 6.0/95; the manual text extracts fully
  with the own CFB reader; no figures validated.
- **DERIVED (documented intent)** — damper velocity sign convention and
  compound temperature optima are manual statements, not yet live-verified.
- **CONFIRMED (file evidence)** — the layout INIs are the view catalogue and
  repeat the manual's view names 1:1.

## 5. Verification performed

- `python mods/sdk_sandbox/web_telemetry/legacy_dossier/tools/test_vb6_pcode.py`:
  **30/30 PASS** (structure invariants, method names, opcode census, tiny-stream
  bytes, determinism, refusals).
- Parser is read-only and deterministic; two runs produce identical JSON
  (timestamp excluded). It opens the vendor EXE only for reading.
- CFB reader runs under `%TEMP%`; the vendor `.doc` is opened read-only; the
  doc hash is recorded before/after use in the extraction summary.
- Workspace validator: rerun after all edits — same 2 pre-existing errors
  (`.claude`, `plans`), 0 warnings (recorded in the checkpoint/README update).
- Not run here (deliberately): full `sdk-test` gate (no SDK capability changed
  in STOP 2; reserved for the implemented workbench at STOP 4/5).

## 6. Changed files and hashes

| File | SHA256 |
|---|---|
| `legacy_dossier/tools/vb6_pcode.py` (new) | `28CD3C16EECD58D6AED6A95DB9CD56948EC4393BC1595B327A0FB475E09971B1` |
| `legacy_dossier/tools/test_vb6_pcode.py` (new) | `741F796DF813D2DD73347E14D1D790595F44B14C4C539B1AE6BD5E26FDA37B4C` |
| `legacy_dossier/PCODE_TABLES_f1_telemetry_2026-10-09.json` (new) | `0E5F967529F0F316EB1F8764316A7F80A3D7BA549A7F438B2EE5CE05187ACBC9` |
| `legacy_dossier/PCODE_SUMMARY_f1_telemetry_2026-10-09.md` (new) | `0B0CFD29EE3015931B53FC43151EE48826089D4174FEE59B3115E1368142D37D` |
| `legacy_dossier/TELEMETRY_DOC_EXTRACTION_2026-10-09.json` (new) | `4776D1FB85C1E60C1933616D091337825B144050EFBCC9BC6F240724D1489B99` |
| `legacy_dossier/DOC_VS_EXECUTABLE_2026-10-09.md` (new) | `773C4CA00BC205D64A2EF3C2FF47BC6956A71FC223E5BCFA1A84F44B4BAF4BCE` |
| `legacy_dossier/FUNCTION_COVERAGE_*.json` (regenerated per review) | `65F4ECE53D52A7AB63163E0B3738F930A0C60FB40521CE97450864F435487009` |
| `legacy_dossier/FUNCTION_COVERAGE_*.md` (regenerated per review) | `554AD4B32FDAF68BDB43D3E242C342A9A8FF7CBD4620AFC83C82B0D687D37ED9` |
| `legacy_dossier/STOP1_LEGACY_TELEMETRY_2026-10-09.md` (review addendum) | `70E7AE93F461F3D107F08CEE4D9B1F4459F23381D1BE4803984BED178B664C90` |
| `legacy_dossier/tools/classify_native_functions.py` (review reword) | `CD2CF782B39FE8EDEA64BE2728B8296ABFA70B35FAFB43263D5A09488CD0658F` |
| `web_telemetry/README.md` (STOP 2 index lines added) | `FFF0CC174E8CEB2099FCB856D43B5806CAA580397AC6C885A47C2B1732619817` |
| `tools/sdk/AUTOMATION_CHECKPOINT.json` (entry `legacy_telemetry_stop2_20261009` added; `updated_at` refreshed) | `BC18785D8223D1CE80DF07BCEE797EC6BEE33A3DC1E430837E53F0D122AC4CF8` |
| `%TEMP%\f1c-legacy-cfb\*` (disposable extraction work) | text + summary hashes above |

No game file, loader, broker, installed DLL, production artifact or vendor file
was modified. No viewer launch. No package installed. No new top-level directory.

## 7. Preservation and restoration

Before-states for this stop are under
`workspace_organization_audit/LEGACY_TELEMETRY_STOP2_BEFORE_2026-10-09/`
(Web Telemetry README, FUNCTION_COVERAGE JSON/MD, STOP 1 record, classifier
script, SDK checkpoint). The STOP 1 audit directory keeps the earlier
before-states. Restore by copying files back; new files are additions.

## 8. Wasted-effort check

- Repeated reads: the vendor EXE was read through one reusable `Image` class;
  exploratory dumps were consolidated into three prototype scripts that were
  superseded by the final parser rather than re-run.
- Broad runs: parser tests run once after the fix; no full SDK gate (nothing in
  the SDK changed).
- Retries: two real defects fixed (external-table pair semantics were pointer
  descriptors, not length strings; Word 6/95 has no Clx so the Word97 path
  initially failed) and one false-positive image heuristic replaced by strict
  validation. No speculative loops.
- Polling: none; all work was static/read-only.

## 9. Documentation gate

This record is indexed from `web_telemetry/README.md`; parser outputs, doc
extraction and comparison are linked; facts, documented intent and open items
are separated; viewer evidence stays tagged with the viewer SHA256 and out of
game exports. UTF-8 re-read performed. Validator result recorded above.
