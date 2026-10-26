# P-code semantics dossier — legacy viewer analysis math (parallel task C)

Date: 2026-10-09. Status: PARALLEL C DONE (review corrections applied 2026-10-09; see §8).
Target: `f1_telemetry.exe` SHA256
`0AA34C51C30C3113BF6B6C2F1B59A8B3346AD1322F6AB519E60757BAF8573BEB` (331,776 B).
Analysis surface: the isolated viewer IDB `sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.i64`
(SHA256 `3E66F6DC306FFC6B0086D784FA5F81169A4D7D15C333544957D153A0387E7985`) and the
vendor EXE read in place. The viewer was **never launched**; no vendor file was copied into
the workspace; no installs. All new files live under `legacy_dossier/pcode_semantics/`.
Existing tools (`legacy_dossier/tools/vb6_pcode.py`, `vb6_methods.py`, `legacy_formats.py`)
were imported read-only and are unchanged. No existing file was edited, so there is no
restoration needed beyond the before-states recorded by the prior STOP records.

## 0. Index

- `tools/build_opcode_table.py` + `fixtures/opcode_table_v1.json` — VB6 p-code opcode
  table (1536 slots) derived from the public vb-decompiler.org page (documentation read
  2026-10-09) with the lead-table size rule validated empirically.
- `tools/opcode_table.py` — table access + empirical overrides
  (`fixtures/opcode_table_overrides.json`, empty: no correction was needed).
- `tools/pcode_decode.py` — stream extractor + decoder + invariant checker + listing
  generator; resolves strings, import calls and internal method calls.
- `tools/test_pcode_decode.py` — 14 tests (table, 195/195 streams, invariants,
  call resolution).
- `tools/pcode_features.py` — clean-room Python reimplementation of the recovered math.
- `tools/test_pcode_features.py` — 16 tests; the reference-lap tests compare the
  reimplementation against the viewer's own saved output.
- `tools/export_dossier_listings.py` — regenerates `listings/`.
- `tools/README.md` — maintained tools vs scratch probes (the probes are not part of the
  tested pipeline; see the review correction in §8).
- `listings/` — full listings for the small/medium methods used, offset-bounded excerpts
  for the two huge methods (`FileReading#0`, `home#2`); see `listings/SOURCES.txt`.

## 1. P-code decoder (task item 1)

### 1.1 Stream location model (corrects the STOP 2R gap model)

Each method dispatch entry points to a 0x30-byte `ProcDscInfo` record at its RTMI VA.
The p-code of the method **precedes** its record (Chubchenko, *Decompiling P-Code*,
vb-decompiler.org), and the record carries the code size:

| Offset | Type | Meaning | Evidence |
|---|---|---|---|
| +0x00 | u32 | pointer to the owning ObjectInfo | 195/195 back-pointer |
| +0x04 | u16 | method flag/count word (raw) | observed (FMemStVar etc.) |
| +0x06 | u16 | FrameSize (stack frame bytes) | plausible per method; raw |
| +0x08 | u16 | **ProcSize = p-code byte count** | validated: 195/195 extents decode |
| +0x0A | u16 | raw (record size/flags; not needed) | raw |

code extent = `[rtmi - ProcSize, rtmi)`. Validation result: all 195 extents decode with
**zero unknown opcodes**, consume the whole extent, end with an exit-family instruction
and have **0-3 filler bytes** before the next record (histogram 0:47, 1:44, 2:50, 3:54).
The prior "next RTMI bounds the stream" model mixed unrelated tables into big-method
streams; it is superseded. `tools/pcode_decode.py --check` reproduces:

```
streams 195, streams_ok 195, total_instr 34227, total_code_bytes 114188
filler: {0:47, 1:44, 2:50, 3:54}
LitStr pool refs: 1191 ok / 0 bad
branch targets: 1274 ok / 0 bad (OnErrorGoto 0xFFFE = "On Error GoTo 0" special)
```

### 1.2 Opcode table

- Source: `https://www.vb-decompiler.org/vb_pcode_table.htm` ("VB5.0/6.0 P-Code Public
  Opcode Table", DotFix, 2006-08-25), read 2026-10-09, page SHA256 recorded in the JSON.
- 1536 slots = 6 dispatch tables, lead bytes 0xFB..0xFF prefix the secondary tables.
- Parser corrections found by tests: multi-digit decimal sizes (e.g. `11`, `27`) must not
  be read as hex references (only `DOC` rows reference another opcode); a byte-order bug
  in the first probe misread little-endian operands.
- **Lead-table size rule** (empirical, confirmed by decode): the public page's size column
  for lead-table opcodes counts operand bytes + 1; total size = listed + 1
  (e.g. `FD E6 FMemStVar` listed 5 → 6 bytes; `FD 95 ExitProcCb` listed 3 → 4 bytes,
  matching the observed `FD 95 10 00` terminator form).
- Variable-length opcodes (listed -1; e.g. `FFreeVar`): opcode + u16 payload byte count +
  payload. The rule matches Chubchenko's `FFreeStr`/`FFreeVar` example and the decoder
  never needed it outside filler (all streams decode with fixed sizes).
- Operand resolution in listings: `LitStr`/`LitVarStr` → pool index ×4 → BSTR text (two
  layouts observed: length-at-target-4 chars-at-target, and [0][len][chars] at target);
  `ImpAdCallFPR4` → pool index + stack adjust → import name (IAT thunk) or internal
  `Object#method` (the stub's `BA <ProcDscInfo> B9 <thunk> FF E1` form).

## 2. Feature recovery (task item 2)

Evidence tiers: **CONFIRMED** = p-code listing + reproduced numerically from real viewer
output or the test suite; **DERIVED** = listing-level, not checked against viewer output;
**OPEN** = unknown/unresolved. Numeric checks use the viewer-saved reference lap
`Reference/reference_2002_hockenheim.lap` (mtime 2026-10-09, matching the viewer run;
86 channels; 1,385 samples at 0.1 s) — its derived channels are the viewer's own output.

### 2a. Time → distance axis reconstruction (spt has no distance channel)

**Formula (CONFIRMED).** During load/derive (`Data_management #8`, RTMI 0x444E14,
FrameSize 1364) the viewer, per sample i (1-based) over the loaded arrays:

```
dx = x[i] - x[i-1];  dz = z[i] - z[i-1];  dy is NOT used
d  = sqrt(dx*dx + dz*dz)                                  ; p-code: two MulVar + AddVar + rtcSqr
dist[i] = dist[i-1] + d                                    ; cumulative; NO lap reset branch
dt = t[i] - t[i-1]; if dt < 1e-7 then dt = 1e-7            ; constant 48AFBC9AF2D77A3E = 1e-07
if dt > 0.01 then v[i] = d / dt * 2.2371365                ; constant D48041D2A7E50140 = 2.2371365
              else v[i] = v[i-1]                           ; hold during stalls (0.01 @0x058F)
```

Listing anchors (full listing in `listings/Data_management_8_0x00444E14.txt`): loop setup
0x0000-0x0203, three position differences squared/summed/Sqr at 0x04F7-0x0547, dt floor
0x0552-0x0581, threshold branch 0x0585-0x05B5, mph multiply 0x05B9-0x05CC, first channel
store 0x05CE-0x0607, distance accumulation and store 0x0612-0x065E. Derivation runs only
when the data source check `Ary1LdI2 == 2` passes and a velocity channel exists
(0x0009-0x001A, 0x037C-0x0397). `Data_management #3` = channel-lookup-by-name
(called with "time", "x", "y", "z", "velocity", …, "distance", wheelspins).

- Units: distance in metres (path length); velocity in **mph** using the viewer's constant
  2.2371365 (not the exact 2.2369363) — the checked ratio `|dpos|/dt / velocity` is
  0.4470005 = 1/2.2371365.
- Lap reset: no reset in the derivation; the saved lap's distance starts at 172.153 m
  (the recording began before the lap slice) while time starts at 0.0025 s. Whether the
  viewer re-bases time at lap save is recorded OPEN (§6).
- Numeric checks (`test_pcode_features.py`): distance max error 0.000246 m over 7,264 m;
  velocity max error < 0.1 mph against the stored channel.
- Pedigree: this supersedes the STOP 2R open item "distance formula unknown
  (no opcode-level trace)" in finding `legacy-viewer-axis-and-layout-evidence-20261009`.

### 2b. Smoothing / filtering

- Damper velocity itself: **not** smoothed in the stored data — exact derivative
  (see 2f). The "smoothing" is a display/derivation filter language:
- `Data_management #4` (RTMI 0x440310) parses channel references of the form
  `abs[name]` and `filter[name, w]` (offsets 0x0027-0x0062 "abs", 0x00B9-0x014C
  "filter", number parsed with `rtcR8ValFromBstr` @0x018F). After extracting the named
  channel it applies:
  - `abs[...]` → element-wise `Abs` (0x0868-0x08A5, `FnAbsR4`);
  - `filter[name, w]` → **centred moving average with half-width w samples**
    (0x08A6-0x09D9): `out[i] = mean(in[max(1,i-w) : min(N,i+w)])`, window clamped at both
    ends via `General_utility_routines #6 = Max` / `#5 = Min` (verified listings).
- With the 0.1 s cadence, `filter[x, 2]` ≈ a 0.5 s centred box filter. No other filter
  widths or IIR/EMA were found.
- Tests: `test_filter_window` (clamps), `apply_abs`.
- Tier: **DERIVED** — p-code listing + own unit tests only; not checked against viewer
  output from real files (review correction 2026-10-09; correction finding
  `legacy-viewer-pcode-semantics-correction-20261009`). The damper-velocity row that is
  real-file checked lives in §2f, not here.

### 2c. Reference-lap subtraction / incremental time difference (`dtime`, label `t2-t1`)

**Algorithm (CONFIRMED for alignment/interpolation; real-data check in the test suite).**
`Data_management #13` (RTMI 0x43ECF4) builds the delta channel; `Data_management #14`
(RTMI 0x433138) is the interpolation helper. For each sample of the current lap:

```
t_ref_at_d = interpolate(ref_time, ref_distance, cur_distance[i])   ; DM#14
delta[i]   = cur_time[i] - t_ref_at_d
delta[i+1] = delta[i]                                              ; segment written to both endpoints
pass2:     for i = 1..n-1: delta[i] = delta[i] - delta[1]          ; sequential: zeroes only delta[1]
```

The literal second pass runs sequentially: iteration i=1 zeroes `delta[1]`, and later
iterations subtract the already-zero `delta[1]`, so no other sample is rebased (the
earlier "capture base" reading was corrected by the real-data check below, which shows
strictly-moving samples must come out exactly zero).

`Data_management #14` (78 lines; `listings/Data_management_14_0x00433138.txt`):
scan i while `ref_distance[i] <= target`; k=i-1;
`f = (target - d[k]) / (d[i] - d[k])` if `|d[i]-d[k]| > 0.0001` (constant
2D431CEBE2361A3F) else `f = 1`; result = `t[k] + f * (t[i]-t[k])`. So the **alignment is
by distance** (not time) and the interpolation is **linear on the reference lap's
distance→time curve**.

Listing anchors in DM#13: time/distance extraction for a lap 0x018E-0x01C4; per-lap loop
0x01CF; distance interpolation call 0x04E6-0x04FB; subtraction/store to delta[i] and
delta[i+1] 0x0507-0x0540; re-zeroing pass 0x054D-0x0586; delta graph rows
(dtime, distance, time) 0x05E3-0x0760; max-delta via `General_utility_routines #6`
0x0766-0x0798. UI: `home #2` prints the legend label `t2-t1` for a graph whose channel is
`dtime` (0x16F1-0x1806), and `home #39` is a file-open/save-settings handler, not the
delta computation (the method-index 2-byte string scan had flagged it; false positive).

Tests: `test_interpolation_linear` (linear + degenerate-gap rule),
`test_delta_of_shifted_lap_keeps_first_sample_zero` (only the first sample is zeroed) and
`test_delta_self_alignment_real_lap` — the real-file check required for the CONFIRMED
tier: run on the real reference lap's 1,385 distance/time samples, samples with genuine
movement (> 1 mm) come out exactly 0 (max |err| < 0.0005 s); samples inside float-plateau
(standing) stretches keep the run-end interpolation value, bounded by the run duration
(max 3.80 s at the standing start). The `distance[r] <= target` scan (advance past equal
distances) is what produces the plateau behaviour.

### 2d. Lap validity checks (`OFFTRACK=…`), boundaries

**Rule (DERIVED — p-code listing + own unit test only; not checked against viewer output
from real files).** `Data_management #23` (RTMI 0x434EF8) and the Check button handler
`home #30` (RTMI 0x437534) implement the same flow:

1. strip an existing `OFFTRACK=PASS`/`OFFTRACK=FAIL` result from the lap text
   (InStr + Left$);
2. run the off-track detector `Data_management #16` twice (with the literal arguments
   `5` and `9999`; DM#23 0x00F2-0x0136, home#30 0x00C1/0x00DF);
3. `count = a + b`; if `count == 0` append ` OFFTRACK=PASS`, else append
   ` OFFTRACK=FAILED` (DM#23 0x013D-0x01B7; `This lap is CLEAN` / `This lap has FAILED`
   MsgBox strings in home#30 0x027C+).
   The reference lap's B2 metadata does contain `OFFTRACK=FAILED`, but its provenance
   (viewer check run vs. session marker) was not proven against a viewer check run, so
   the rule stays DERIVED (review correction 2026-10-09).

**Off-track detector.** `Data_management #16` (RTMI 0x43C878) extracts x, z, time with
`Data_management #4`, loads the track geometry, walks samples and track points
(ForStep loops 0x0199-0x02C6), tracks the minimum distance/outside condition and labels
the result `offroad` (0x02D6), then writes it via `Data_management #18` (0x02F3).
The meaning of the two threshold literals (5, 9999) is **OPEN** — read as detector
parameters, not proven.

**Lap split/boundary detection: NOT recovered (OPEN).** No method found that slices the
record stream at lap crossings; the data show time re-based to ≈0.0025 s at the saved
lap's first sample while the distance channel continues from the recording start
(172.153 m), so the boundary logic is in the load/save path (candidates `FileReading
#0`/`#14`, `home #35` "lap=" parsing) but the exact site is unresolved. Recorded with the
data evidence instead of guessed.

### 2e. Statistics and autoscale

- The graph redraw `home #2` (RTMI 0x449778) prints per-graph legend text
  `max.` / `avg.` / `min.` (0x1572, 0x1618, 0x16BE) followed by a formatted double, plus
  the `t2-t1` label when the channel is `dtime`. Tier: **DERIVED** (labels and drawing
  loop confirmed; the exact min/max/avg reduction sites inside the 10 KB method were not
  isolated).
- Autoscale: layout range rows carry an optional `#TRUE#`/`#FALSE#` token (STOP 2R
  grammar). `home #12` (window init) seeds the default property string `"TRUE"`
  (0x010B-0x010E) — i.e. autoscale defaults on unless the layout says otherwise.
  The numeric autoscale padding/rounding rule is **OPEN**.
- Tier: DERIVED for the mechanism, OPEN for the exact math.

### 2f. Derived channels (listing formulas + numeric checks)

All in `FileReading #0` (RTMI 0x44E478; the .lap channel assembler) unless noted.
Channels labelled ` temp` get `-273` (integer, not 273.15; 0x173A-0x1742) after assembly;
spt metres are scaled by 1000 to mm (0x119E, 0x11C2, 0x11E6, 0x120A) for ride height and
suspension deflection.

| Derived channel | Formula | Listing | Tier | Numeric check |
|---|---|---|---|---|
| velocity | §2a (DM#8) | DM#8 0x05B9 | CONFIRMED | max err < 0.1 mph |
| distance | §2a (DM#8) | DM#8 0x0612 | CONFIRMED | max err 0.000246 m |
| `{w} tyre average temp` | (L+C+R)/3 − 273 | 0x14B8 (fl), 0x1465… | CONFIRMED | err 4e-5 °C |
| `{w} tyre crown temp` | center − (left+right)/2 | 0x136B (fr), 0x13A2 (fl), 0x13D6 (rr), 0x140D (rl) | CONFIRMED | err 1.5e-5 |
| `{w} tyre camber temp` | left−right (right wheels), right−left (left wheels) | 0x146F…0x14B8 | CONFIRMED | exact |
| `front/rear ride height` | (left+right)/2 of undertray heights (mm) | 0x15AC / 0x15DC | CONFIRMED | err < 0.001 |
| `damper velocity …` | (deflection[i] − deflection[i−1]) / dt | 0x1664-0x16F1 | CONFIRMED | err < 0.001 mm/s |
| `rear wheel speed differential` | value(rl wheelspin) − value(rr wheelspin) | 0x16F4-0x170B | DERIVED/OPEN | stored lap mismatch (max err ≈ 12) — lanes/units upstream unresolved |
| `aids1..4` | pass-through of four variables | 0x160C-0x1661 | OPEN | stored values {0,42,170,234}/{0,145,149}/{0,64}/{0,8}; provenance of the source vars not found |
| `rain`, `track dampness`, `air temp`, `track temp` | env-record values | 0x1310-0x1365 | DERIVED | from spt env records (STOP 3 provenance) |

Tests: all CONFIRMED rows are asserted in `test_pcode_features.py`
(`test_average_temp_formula`, `test_crown_formula`, `test_camber_formula`,
`test_ride_height_formula`, `test_damper_velocity_formula`).

## 3. Listings index (`listings/`)

Full: `Data_management_{4,8,13,14,16,23}_*.txt`, `home_{12,30,35,39}_*.txt`,
`FileReading_8_*.txt`, `General_utility_routines_{5,6}_*.txt`.
Excerpts: `FileReading_0_excerpt_*` (registration 0x0000-0x0A70; conversion 0x1067-0x14C0;
crown/camber 0x136B-0x14C0; averages/ride heights 0x14B8-0x15AC; aids/damper/rear-diff
0x160C-0x1710; unit fix-ups 0x170E-0x17A0) and `home_2_excerpt_*` (plot setup
0x08E3-0x0AC7; legend/delta labels 0x1559-0x1790). Regenerate any listing with
`python tools/pcode_decode.py --disasm 0x<RTMI>` or all of them with
`python tools/export_dossier_listings.py`.

## 4. Methods still not understood (with reason)

1. `Data_management #16` — off-track/corner-cut detector: role and call graph confirmed;
   the exact geometry test and the meaning of detector arguments 5/9999 unresolved
   (the method mixes object property calls for the track table; needs step semantics).
2. `Data_management #8` secondary blocks — acceleration ÷ 9.8→? conversions
   (0x0669-0x0710) and the source of the aids/channel flags were not fully traced.
3. `home #2` autoscale padding math and the exact min/max/avg reductions inside the
   legend path — method is 9 KB and mixes UI property calls; only labels/flow confirmed.
4. `home #43` / `home #15` / `home #23` render/timing windows — classified at
   method level (STOP 2R) but their render-state math was not decoded in this task.
5. Lap slice/boundary detection site (§2d) — not found.
6. `Data_management #16`-adjacent `Data_management #18` "offroad" result writer
   (0x43A430) — call site confirmed, body not decoded.
7. Twelve method slots without an RTMI (9 null, 3 inline-data) — unchanged from STOP 2R;
   no code to disassemble.

## 5. Tools, tests, artifacts

Verification (2026-10-09):

- `python test_pcode_decode.py` → **14/14 PASS** (195/195 streams ok, 0 unknown opcodes,
  filler ≤ 3, all 1191 LitStr refs resolve, all 1274 branch targets land, deterministic).
- `python test_pcode_features.py` → **16/16 PASS** (real-lap numeric checks above +
  constant/behaviour unit tests + the real-file delta self-alignment check).
- Existing suites untouched: `tools/test_vb6_pcode.py` 30/30, `tools/test_legacy_formats.py`
  56/56 (not re-run here; unchanged files).

| New file | SHA256 |
|---|---|
| `tools/build_opcode_table.py` | `9C0C913573E05D6BA9D6A8868FA8D4FEC6219236B863156026F0B4AEB79AD84F` |
| `tools/opcode_table.py` | `557309CC8C788680B3DA93D394EB045105875E06679C0BED69C996B64BDA0914` |
| `tools/pcode_decode.py` | `F7AAA25C852BE1443A2B699629C02B9328644E16E82491EC4C9A8539AF05DC38` |
| `tools/pcode_features.py` | `2DC2C44FB7AF3D2F6E82F2B8BE03FEF2A3119DFB55A0279031E6E91A0B15D825` |
| `tools/test_pcode_decode.py` | `067FF2067EA4558DA244A15381F8D102460935543BD10BBAE40DED9A4DBD4B55` |
| `tools/test_pcode_features.py` | `2C8A91F490047CEFE2BDD754939BD77C03D58BCB022DD7E5ED15F903DF059E69` |
| `tools/export_dossier_listings.py` | `4A04E726FD75CA3FCAC8E821636B54D5BA17509518E37050F4ABF7592B61186C` |
| `fixtures/opcode_table_v1.json` | `A15D0F5C1838D3D78AE1CFF0BB3BB9A068961E86C320A5C9A6F89D76B1A12F21` |
| `fixtures/opcode_table_overrides.json` | `531B43615B8D0DD9FC0973546AC607F3537C798CADF569B02CC9EBAC81B449B4` |

Scratch probes (working aids, not tested tools — see `tools/README.md` for their purpose):
`recon_methods.py`, `probe_rtti_layout.py`, `probe_extent.py`, `probe_stubs.py`,
`probe_violations.py`, `probe_dm8_map.py`, `probe_bad.py`, `ref_lap_probe.py`,
`ref_lap_probe2.py`, `ref_lap_probe3.py`.

External read-only inputs used: vendor EXE (hash above, in place); reference lap
`Reference/reference_2002_hockenheim.lap` (read in place; SHA256 in the STOP 3 record);
public opcode page (documentation, read 2026-10-09; page hash recorded in the fixture).

## 6. Open items

1. `rear wheel speed differential` numeric mismatch (formula from listing vs stored lap).
2. `aids1..4` source variables and meaning.
3. Off-track detector parameters 5/9999 and the DM#16 geometry test.
4. Lap slice/boundary detection and time re-basing at lap save.
5. Autoscale computation/padding and the exact min/max/avg reductions.
6. Delta behaviour on duplicate-distance (standing) samples: the run-end clamp is
   documented and bounded (max 3.80 s at the standing start) but not checked against
   viewer-rendered delta output (none exists in a saved file).
7. Acceleration g-conversion blocks in DM#8; whether stored accel channels are g.
8. `Data_management #18` "offroad" writer body.
9. 12 method slots without RTMI remain code-less.

## 7. Wasted effort, methods, documentation gate

- One full re-derivation of the stream-extent model was needed after the probe showed the
  "gap" model mixed unrelated tables (GUI dispatch data) into big methods; the
  ProcDscInfo/ProcSize model replaced it and is fully validated. Probes are kept and
  documented as scratch in `tools/README.md` (review correction 2026-10-09).
- Two table-parser bugs (decimal vs hex sizes, little-endian operand reading in a probe)
  were caught by the stream invariants instead of shipping silently.
- The method-index 2-byte string scan was demonstrated to produce false positives
  (`home #39` "dtime/t2-t1/avg."); all semantics in this dossier come from disassembled
  instructions, not the scan.
- Documentation gate: this record plus listings, tools, tests and fixtures are all under
  `legacy_dossier/pcode_semantics/`; facts vs inference vs open items are separated;
  two appended `findings.jsonl` entries (original + correction, after duplicate checks);
  workspace validator baseline unchanged (2 pre-existing errors `.claude`, `plans`).

## 8. Review corrections (Claude, 2026-10-09 — ACCEPTED WITH 2 CORRECTIONS)

1. **Tier downgrades.** §2b (smoothing/filter mechanism) and §2d (OFFTRACK validity rule)
   are listing + own unit tests only, with no check against viewer output from real
   files; both are now **DERIVED**. CONFIRMED remains only where a real-data error is
   stated: §2a distance/velocity, §2c delta core (new real-file self-alignment test),
   §2f derived channels (real-lap errors). The original findings line is untouched; the
   correction is appended as `legacy-viewer-pcode-semantics-correction-20261009`.
2. **Scratch probes.** The 10 exploration scripts are documented as scratch in
   `tools/README.md` with their purpose and are not maintained tools; the maintained set
   is `opcode_table.py`, `pcode_decode.py`, `pcode_features.py`, the two test modules,
   `build_opcode_table.py` and `export_dossier_listings.py`. Nothing was deleted or
   moved (no purge was needed under this option).

Delta core refinement made while applying correction 1 (kept CONFIRMED because it is now
real-data checked): the literal second pass `delta[i] = delta[i] - delta[1]` runs
sequentially and only zeroes the first sample; the earlier "capture base" reading was
wrong, and the real-lap self-alignment test now pins the behaviour (strictly-moving
samples exactly zero; standing plateau samples keep the run-end value, max 3.80 s).
