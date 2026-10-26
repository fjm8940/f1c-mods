# Legacy file format specifications (recovered, read-only)

Date: 2026-10-09 (STOP 3 revision; STOP 2R text preserved in
`workspace_organization_audit/LEGACY_TELEMETRY_STOP3_BEFORE_2026-10-09/`).
Decoders: `tools/legacy_formats.py`; machine data:
`FORMATS_f1_telemetry_2026-10-09.json`; tests: `tools/test_legacy_formats.py`
(56 checks, all pass). Reader/writer ties use
`METHOD_INDEX_f1_telemetry_2026-10-09.json` and the game decompile corpus
(`f1c_ida_decompiled/functions/*.c`) for the .spt writer chain.

Live-writer correction (STOP 3 review item 1): `telemetry.spt` is written by
the GAME (F1 Challenge Online.exe), not the viewer. The growth observed during
the STOP 2R analysis (122,880 → 204,800 B, ~2.4 KB/s in 4 KB chunks) was the
game's native recorder during a live race with no viewer process running. The
viewer's 12:43 UTC run is a separate earlier event; the current file is a
crash-truncated sample (header byte 0x00-0x07 finalization OPEN).

## 1. telemetry.spt — the game's native telemetry recorder store

Writer chain (game corpus, FACT): `TelemetryDataDump` 0x004D4A80 opens the
file and writes the header, identity strings, descriptors, 17 session hash
words and the XOR-encoded copy of the car's .VEH file. `DebugLogInit`
0x004D5960 + `sub_447F30` 0x00447F30 write each player record from the player
physics object `P = 0x7C7428`. `sub_4D59B0` 0x004D59B0 writes environment
records. `EncodedDataWriterWithXOR` 0x004D5900 writes 5-byte event records.
Cadence lives in `MainGameTickUpdate` 0x0042C690 lines 88-94.

| Offset | Type | Meaning | Evidence |
|---|---|---|---|
| 0x00 | u32 | 0 in the observed files (finalize-at-close OPEN) | decode |
| 0x04 | 4 bytes | recorder LCG seed (v40 bytes in TelemetryDataDump) | writer + decode |
| 0x08 | float32 | constant 0x3D4CCCCD (0.05) in this writer build; NOT the effective cadence | writer + decode |
| 0x0C | ANSI cstr | driver name ("Matthias Steppe") | decode |
| then | ANSI cstr | team ("2002 BAR") | decode |
| then | ANSI cstr | vehicle path (`SEASONDATA\VEHICLES\...`) | decode |
| then | ANSI cstr | track ("2002_Hockenheim") | decode |
| then | u32 | channel count (57) | decode |
| then | per channel | 1-byte code (0x04 in all 57) + NUL-terminated ANSI name | decode |
| 0x4E0 (observed) | 17 × u32 | session hash words (writer: 17 `sub_4D5BE0(&v38, 4)` calls) | writer + decode |
| then | u32 + bytes | size + XOR-encoded .VEH copy, keys 0x5C/0xB4/0x12 cycling | `FileReadWithXORDecode` 0x004D5AF0 |
| then | records | typed record stream (below) to EOF | writer + decode |

Channel names carry the units: `Elapsed Time (s)`, `Pos X/Y/Z (m)`,
`Yaw (rads)`, `Lat/Vert/Long Accel (m/s)`, `Engine RPM`, `Clutch RPM`,
`Unfiltered/Filtered Throttle/Brake/Steering/Clutch`, per wheel
(`FL/FR/RL/RR`) `Wheelspin (rads/s)`, `Suspension Deflection (m)`,
`Undertray Ride Height (m)`, `Tyre Left/Center/Right Temp (K)`,
`Tyre Pressure (kPa)`, `Tyre Wear`, `Corner Weight`, plus `Gear`,
`Water Temperature`, `Oil Temperature`.

### 1.1 Typed record stream (writer-proven sizes, decoded)

| Type | Size | Writer | Payload |
|---|---|---|---|
| 0x00 | 229 B | DebugLogInit 0x004D5960 | player record: 57 × float32 (sub_447F30) |
| 0x04 | 25 B | sub_4D59B0 0x004D59B0 | packed flags u32 + 5 × float32 (last two +273.16 K) |
| 0x01 / 0x02 / 0x03 / 0xFF | 5 B | EncodedDataWriterWithXOR 0x004D5900 | one dword XOR last player elapsed-time bits |

Player record field map (57 channels, writer-proven source offsets; P =
0x7C7428, S = `*(P+0x15F0)`, wheel base W(k) = `P+0x474+0x464*k`):

| # | Channel | Source |
|---|---|---|
| 0 | Elapsed Time (s) | f32 @ P+0x060 |
| 1-3 | Pos X/Y/Z (m) | 24 B block @ S+0x004 (f32 each) |
| 4 | Yaw (rads) | atan2 over rotation-matrix elements S+0x1C..+0x74 |
| 5-7 | Lat/Vert/Long Accel (m/s) | Mat3x3MulVec(S+0x1C, S+0x134) × f64 @ S+0xB4 |
| 8 | Engine RPM | f64 @ P+0x1C5C × 9.549296585513721 |
| 9 | Clutch RPM | f64 @ P+0x1E38 × 9.549296585513721 |
| 10-17 | Unfiltered/Filtered Throttle, Brake, Steering, Clutch | f64 @ P+0x048/0x008/0x050/0x010/0x040/0x000/0x020/0x018 |
| 18-53 | 4 wheels × 9 | spin W-0x12C; suspension W; ride height P+0x16F0+8k; tyre L/C/R W+0x164/+0x16C/+0x174; pressure W+0x18C; wear W+0x1AC; corner weight W+0x134 |
| 54 | Gear | int32 @ P+0x1EF0, stored as float32 |
| 55-56 | Water/Oil Temperature | f64 @ P+0x1D8C / P+0x1D4C |

Observed closed stream (crash-truncated file, 4,505,600 B): 19,447 player
records at 0.1 s cadence (divisor 40 of the 400-frame counter), 1,945
environment records at 1 Hz, zero event records, 78-byte partial final record.
Measured 2,312 B/s vs expected 229 B × 10 Hz + 25 B × 1 Hz = 2,315 B/s.

Stride vs growth check (review item 1): the review's "growth × 0.05 s"
premise gives ≈ 120 B, which does not match the 229-byte stride; the effective
cadence is 0.1 s (129 B per 0.05 s tick), so the correct cross-check is
growth × 0.1 s ≈ 231 B ≈ stride + environment share. The stored 0.05 float is
a writer constant, not the cadence.

**P-code ties:** `FileReading #6` references `reading from telemetry file`,
`Loading Telemetry ...`, `elapsed time`, `pos x`, `pos y` — the .spt reader;
`home #6` (`(*.spt)|*.SPT`), `home #35` (`telemetry.spt`, `lap=`, `t2-t1`),
`LapInfo #1` (upload metadata).

## 2. reference_*.lap — saved lap

| Offset | Type | Meaning | Evidence |
|---|---|---|---|
| 0x00 | text line | `CHECKSUM=<decimal>\r\n` (digits validated; algorithm OPEN) | decode |
| 0x17 | u32 + bytes | metadata block: length 113, byte-5 shift + reversal → `DRIVER=... VERSION=2003.14` | decode (review-proven op) |
| then | 73 B | fixed field block: u32 2, u32 0, 5 × f32 (first = lap time 133.419 s), u32 7, f32 1.0, u32, 5 × u32 0, name `lap desc$` | decode; lap-time cross-check |
| then | u32 + bytes | weather block: length 172, byte-5 shift + reversal → `LAPTIME=2.13.419` `SECTOR1/2`, `AIR_TEMP`, `TRK_TEMP`, `RAIN`, `TRK_WET`, `AIDS=[...]`, `OFFTRACK=FAILED`, `UPLOAD_ALLOWED=NO` | decode |
| then | u32 + u32 | table marker 1, channel count 86 | decode |
| then | records | u32 length + ANSI name × 86 (`time`, `x`, `y`, `z`, `yaw`, ... `rear wheel speed differential`) | decode |
| then | samples | 1,385 × 344 B (86 × float32), 0.1 s cadence, time 0.0025 → 138.4025 s | decode |
| then | trailer | 9,381 B: f32 1.0 + u32 165 + byte-7 shift + reversal text block (identity + end-of-lap weather); remaining trailer bytes OPEN | decode |

The STOP 2R "count 113 / 82 names / obfuscated block open" misread is
corrected: 113 is the metadata block length, the name table has 86 entries
after its count field, and both obfuscated blocks are now decoded
(byte-5 + reverse; trailer byte-7 + reverse).

**P-code ties:** `home #35` (`lap=`, `t2-t1`, `FilterIndex`),
`LapOptions #1/#2` (`Saving lap...`), `Data_management` lap-check methods
(`BTN_LapCheckCorners`, `OFFTRACK=...`, `This lap is CLEAN/FAILED`).

## 3. Tracks/<season>_<track>.dat — track geometry

| Offset | Type | Meaning | Evidence |
|---|---|---|---|
| 0x00 | u16 | version = 2 (all observed files) | decode |
| 0x02 | u16 | point count (2562 / 1828 / 3413 in decoded files) | decode |
| 0x04 | N × 20 B | 5 × float32 per point | decode; size = 4 + 20·N (+6-byte trailer) |
| end | 6 B | trailer, purpose open (present in every observed file) | decode |

Field semantics (candidate, recorded as open): `[x, z, ?, ?, 1.0]`; the first
four floats look like two coordinate pairs, the fifth is 1.0 across samples.
Decoded ≥3 real files (2 required).

## 4. *.svm — saved setup (text)

INI-like text with LF line endings: `[SECTION]` then `key=value` lines.
Sections observed: GENERAL, LEFTFENDER, RIGHTFENDER, FRONTWING, REARWING,
BODYAERO, SUSPENSION, CONTROLS, ENGINE, DRIVELINE, FRONTLEFT, FRONTRIGHT,
REARLEFT, REARRIGHT, BASIC. Example: `FuelSetting=64`, `NumPitstopsSetting=3`.

## 5. f1_telemetry.cfg — text settings

`key = value` with `//` comments; observed `display_in_mph = 0` (comment says
the default is kph; the manual says MPH — recorded as an open discrepancy in
`DOC_VS_EXECUTABLE_2026-10-09.md`).

## 6. Layouts/*.ini — see `LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md`

All 23 files (21 vendor layouts + 2 viewer-state files) decode with zero
errors: title (optional `*NN_` order prefix), description, graph count, then
per graph channel row, range row (with optional `#TRUE#/#FALSE#` autoscale
token) and normalized rect row.

## 7. Decode budget (measured, STOP 3)

| Item | Measured |
|---|---|
| decode_spt of 4,505,600 B | 0.48 s, peak process memory 9.0 MB |
| decode_lap of 487,843 B (1,385 × 344 B) | 0.13 s |
| One imported lap in memory (float32) | ≈ 0.42 MB samples + names/meta |
| Torn-file behavior | decodes up to the partial tail, reports its byte count |

Imports run on the HTTP/worker side, never the game thread (workbench design
in `STOP3_LEGACY_TELEMETRY_2026-10-09.md`).
