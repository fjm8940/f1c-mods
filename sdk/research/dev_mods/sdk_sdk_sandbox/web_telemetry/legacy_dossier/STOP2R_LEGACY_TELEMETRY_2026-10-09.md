# STOP 2R record — formats, method classification, exit mismatch, axis mode, layout grammar

Date: 2026-10-09 (revised submission after the STOP 2 review
`STOP2_CLAUDE_REVIEW_2026-10-09.md`, which was NOT ACCEPTED - INCOMPLETE).
The STOP 1 review corrections remain applied (STOP 1 record §10). The viewer
was never launched; no packages installed; no vendor file copied or modified.
Authority: root `AGENTS.md`, `development/mods/MOD_STANDARD.md`, governance
docs, `f1c_ida_decompiled/AGENTS.md`.

## Review item 1 — file schemas with offsets, tied to the reader/writer p-code

`tools/legacy_formats.py` (strict, fail-closed) + `FORMATS_f1_telemetry_2026-10-09.json`
+ `FORMAT_SPECS_f1_telemetry_2026-10-09.md`; tests `tools/test_legacy_formats.py`
**32/32 PASS** (real files read in place plus synthetic malformed/truncated
refusals; no vendor copies created anywhere).

- **.spt** header and descriptors as listed (0x00/0x04/0x08, four identity
  strings at 0x0C, channel count, 57 `code+name` descriptors ending at 0x4E0).
  CORRECTED (STOP 3 review item 1): the file was **being written by the GAME**
  (`F1 Challenge Online.exe`, live race, no viewer process), not the viewer
  (122,880 → 172,032 → 204,800 B growth = the game's native recorder flushing
  4 KB chunks, ~2.4 KB/s). The record stream is now fully decoded from the
  game's writer functions: 229-byte player records at 0.1 s (divisor 40 of the
  400-frame counter), 25-byte environment records at 1 Hz, 5-byte event
  records; stride matches growth × 0.1 s ≈ 231 B (the review's × 0.05 s
  premise gives ≈ 120 B and is superseded). Decode-time hash of the STOP 2R
  snapshot: `E8ECAB8DE6F186E7F4A7D0E7B0AAC392DFDFF2D198CCBED3A3ED5AFBDB6D79A8`.
- **.lap** decoded (STOP 3 review item 2 applied): `CHECKSUM=` line; the u32
  after it is the **metadata block length (113)**, not a channel count — the
  block is obfuscated with the byte-5 shift + reversal and decodes to
  `DRIVER=... VERSION=2003.14`; a weather block (172 B, same operation) carries
  `LAPTIME`, `RAIN`, `TRK_WET`, `AIDS=[...]`, `OFFTRACK=FAILED`; the channel
  count is 86 with a length-prefixed name table; 1,385 samples × 344 B at
  0.1 s; the trailer carries a byte-7 shift + reversal text block.
- **.dat** decoded (3 real files): u16 version 2, u16 point count, N × 5
  float32 (20-byte points), 6-byte trailer (purpose open).
- **.svm** text INI-like, 15 sections parsed. **.cfg** key=value parsed.
- **.ini** layout grammar: all 23 files decode (see item 5).
- Reader/writer ties (method index): `FileReading #6` = .spt reader
  (`reading from telemetry file`, `elapsed time`, `pos x`, `pos y`);
  `home #6` = `(*.spt)|*.SPT` load dialog; `home #35` = `telemetry.spt`,
  `lap=`, `t2-t1`; `LapOptions #1/#2` = `Saving lap...`; lap-check methods
  reference `OFFTRACK=...`, `This lap is CLEAN/FAILED`. Exact Open/Get/Put/Seek
  opcode sites cannot be named without the complete opcode table; that limit is
  recorded per method in the index rather than guessed.

## Review item 2 — all 195 RTMI / 207 methods classified

`tools/vb6_methods.py` → `METHOD_INDEX_f1_telemetry_2026-10-09.json` and
`FUNCTION_COVERAGE_f1_telemetry_2026-10-09.{json,md}` updated (the "pending
STOP 2" placeholder is gone). Every method has: object, method index, public
name (or null), slot VA, RTMI VA, terminator-trimmed size, terminator kind,
padding, referenced strings (constant-pool evidence), candidate runtime
references, category and evidence. Categories: metadata 77, axis_distance 47,
lap_check 44, layout_io 39, upload 35, lap_management 26, file_io_spt_read 24,
filter 16, ui_event_handler 15, render 13, shutdown 9, render_transform 2,
unknown 33. The 12 methods without an RTMI are listed individually (9 null
slots, 3 inline-data slots at table starts).

## Review item 3 — 207 methods vs 159/195 exit-ending streams

Resolved with evidence. Stream terminators are **not** only `ExitProcHresult`:
195/195 streams end with a terminator — 94 × 0x13, 60 × 0x14, 6 × 0x16,
1 × 0x15, 34 × the lead2 `FD 95 10 00` form — followed by 0–3 bytes of filler
before the next RTMI (193/195; two streams have larger filler, flagged in the
index). The earlier 159/195 metric only looked for single-byte exits in the
last four bytes. The 207 vs 195 gap: exactly 12 method slots carry no RTMI
(9 zero, 3 inline-data) — matching `207 = 195 + 12`; the old "207
ExitProcHresult" scan count was a coincidence of the greedy byte scan and is
retracted.

## Review item 4 — time vs distance x-axis

`LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md`. The axis is a channel-name selection
in the graph engine: 29 methods reference `distance`; the axis paths
(`home #2/#23/#20/#19/#15`, `Data_management` graph methods, `FileReading
#8/#14`) reference `distance` together with `time`/`ZVALUE`/`dtime`. The .spt
carries time and position but **no distance or velocity channel**, so distance
is reconstructed in the graph/transform path (mechanism CONFIRMED at method
level; exact formula and UI control open — no opcode-level trace).

## Review item 5 — layout grammar from the INI-reading p-code

`LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md`. Parser identified as `home #2`
(67 constant-pool refs including `*.ini`, `/layouts/`, all flag tokens,
`#0.0/#0.00/#0.000`, `NULL`, `Layout exported to layout.ini`); writer/state as
`home #53` (`\layout.ini`, `_revert/_single_layout.ini`, `current layout`,
`single graph layout`). Grammar: title (optional `*NN_` prefix), description,
count, then per graph channels/ranges(+`#TRUE#|#FALSE#`)/rect. All 23 files
decode against it; the viewer-state files prove the writer emits the same
grammar.

## Verification

- `tools/test_legacy_formats.py`: 32/32 PASS.
- `tools/test_vb6_pcode.py`: 30/30 PASS (unchanged parser).
- `tools/vb6_methods.py`: deterministic regeneration; 207/195/12 accounting.
- All vendor files opened read-only; decode-time hashes recorded; no vendor
  copies in the workspace; viewer never launched; no installs.
- Workspace validator: same 2 pre-existing errors (`.claude`, `plans`),
  0 warnings.

## Artifacts and hashes

| File | SHA256 |
|---|---|
| `tools/vb6_methods.py` | `443AA5815B78E14FC45772930A311004A71D456F0D1818E92252C3ED3FF2E38A` |
| `tools/legacy_formats.py` | `80F17E7F5A2B75EF3C93B89402FDE1B5C8DEF526BD0840A05A12B175CC9D7EBB` |
| `tools/test_legacy_formats.py` | `BC230BBF61AB7A23107C8B60BDE650AD504D75B338382FEC32426F8DE3F4F345` |
| `METHOD_INDEX_f1_telemetry_2026-10-09.json` | `72E2C9B6513209ED3376A398FF3D27BE95607D881687A1655BC9EA484F2F37F4` |
| `FORMATS_f1_telemetry_2026-10-09.json` | `25E139ACB50AB07E93D299D9460B92223C5D57909543F73BBAE934048A43928B` |
| `FORMAT_SPECS_f1_telemetry_2026-10-09.md` | `77CCDFFBFC559A868C7CDC6FA1EC1AFE018EE20A9D437F20FE2440DB26195A20` |
| `LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md` | `DE503367A716C2E6415B9002C372F522DD95D4851316177CCD5C8BA2BCD1D645` |
| `FUNCTION_COVERAGE_*.json` (updated) | `B71BC1D6C556D9018149A19DBB61D19872031A5A1AC3FF281945CF3F658AED07` |
| `FUNCTION_COVERAGE_*.md` (updated) | `7C200335C73CFA586E510EA0A798F0EC6C38A3518602B3A8A8BBDBECC40D71AB` |

Before-states for this round: `workspace_organization_audit/LEGACY_TELEMETRY_STOP2_BEFORE_2026-10-09/`
(previous round) plus the same directory's copies for the files rewritten in
this round (FUNCTION_COVERAGE JSON/MD, README, checkpoint preserved before
edit). No file was deleted or moved.

## Wasted-effort check

- One failed attempt to fetch the crate's opcode CSV was avoided by policy
  (no third-party downloads); documentation pages were used instead.
- Two decoder defects were found by tests and fixed (dat trailer, layout
  description line); one heuristic (lap name-table scan) was hardened after a
  false first match. No speculative loops; the live .spt was snapshotted once
  to %TEMP% to avoid repeated torn reads.

## Documentation gate

This record is indexed from `web_telemetry/README.md`; specs, grammar/axis
evidence, machine data and tests are linked; facts, method-level evidence and
open items are separated; viewer evidence stays tagged with the viewer SHA256.
UTF-8 re-read performed.

## STOP 3 review corrections applied (2026-10-09, in place)

The STOP 2R review (`STOP2R_CLAUDE_REVIEW_2026-10-09.md`) was ACCEPTED WITH
CORRECTIONS; items 1-3 and the port conflict are handled in
`STOP3_LEGACY_TELEMETRY_2026-10-09.md`. This record was corrected in place:

1. Writer attribution corrected above; the game's .spt writer chain
   (TelemetryDataDump 0x004D4A80, DebugLogInit 0x004D5960, sub_447F30
   0x00447F30, sub_4D59B0 0x004D59B0, EncodedDataWriterWithXOR 0x004D5900,
   cadence in MainGameTickUpdate 0x0042C690) is recorded in
   `FORMAT_SPECS_f1_telemetry_2026-10-09.md` §1 with the per-field source
   offsets. Only closed files / hashed %TEMP% copies were decoded.
2. The .lap byte-5 + reversal decode is implemented in
   `tools/legacy_formats.py` and tested (real file + synthetic round trip).
3. RPM scale: no re-investigation; finding `engine-rpm-live-scale-20261009`
   (0x39A0 rad/s, RPM = × 9.549296585513721) is cited in the STOP 3 channel
   ledger.

Before-state for this edit: `workspace_organization_audit/
LEGACY_TELEMETRY_STOP3_BEFORE_2026-10-09/`.
