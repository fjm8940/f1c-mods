# H17: how AIW geometry becomes AI corner speed (2026-10-15)

Owner question (lap-time ingredient 5): how the AI turns AIW geometry into corner speed.
Method: static only. The game was not run; the Austria AIW was read
(values only). Labels: FACT (read in file or code), DERIVED (inferred), OPEN.

## 0. Summary

- FACT: the AIW has no curvature, radius or speed key. Per-waypoint data is
  position, lateral offsets, widths, sector and lap distance, and one corner multiplier.
- FACT: the AI corner target is a grip-table lookup, not v = sqrt(mu*g*r). No radius
  term appears in the traced corner-speed path.
- FACT: the racing line is the explicit dry lateral offset wp_path[0].
  wp_groove_lat and wp_galpha are parsed; no reader was found (OPEN).
- OPEN, most important: the writer of wp+0x70, wp+0x74, wp+0x78 (the grip-table
  inputs) is not in the AIW parser. Any geometry-to-speed link must live there.
- DERIVED: Austria's tightest corners have radii of about 15 m to 36 m (section 4).

## 1. Curvature and radius (question 1)

### 1a. What the AIW stores (FACT)

Austria record 0, `%USERPROFILE%\Desktop\F1\F1 Challenge V10\SeasonData\Circuits\Austria\2001_A-1_Ring\2001_A-1_Ring.aiw`:
L218 wp_pos (x, height, z); L219 wp_perp (road direction, unit xz); L220 wp_width;
L221 wp_dwidth; L222 wp_path (dry, wet); L223 wp_galpha; L224 wp_score (sector, lap
distance m); L225 wp_groove_lat; L226 wp_event (corner mult, special, data);
L227 wp_branchID; L229 WP_PTRS. Header: number_waypoints=1070 (L195),
lap_length=4281.659668 (L196).

### 1b. Parser stores (FACT)

`functions/sub_402690_00402690.c`, record stride 316 bytes, v4[N] = byte offset 4N:

| AIW key | Line | Slot |
|---|---|---|
| wp_branchID | L294 | v4[75] |
| wp_path | L295 | v4[6], v4[7] |
| wp_event | L296 | v4[33] (0x84), v4[77], v4[78] |
| wp_pos | L304-L307 | v4[0..2], copy v4[37..39] |
| wp_galpha / wp_groove_lat | L312 / L313 | v4[70] (0x118) / v4[71] (0x11C) |
| wp_score | L314-L315 | v4[18] dist, v4[17] sector |
| wp_perp / wp_width / wp_dwidth | L316 / L317 / L318 | v4[8..10] / v4[11..14] / v4[22..25] |
| WP_PTRS | L320 | v4[72..74] |

FACT: the parser never writes v4[15] (0x3C), v4[28] (0x70), v4[29] (0x74) or v4[30] (0x78).

### 1c. Neighbour chord (FACT, not curvature)

`functions/F_3DPositionDeltaComputation_004faec0.c` L17-L40 writes the vector from the
linked waypoint into this+0x0C..0x14. It is a chord vector, not a radius.

### 1d. Segment length at 0x3C (FACT read, writer OPEN)

`functions/AIRacingLine_00425920.c` L264 reads v5+60 (0x3C) as a segment length in
braking. No writer was found in the parser or remap code. It can be recomputed from
wp_pos (section 4).

Conclusion (1): no per-waypoint curvature or radius is stored or parsed.

## 2. Radius into target speed (question 2)

### 2a. Target function (FACT)

`functions/AIComputePassScore_0042a780.c` L3-L4:

    TireForceSolver(a3[29], (int)(a3 + 30), a3[28], 16.0);
    return (a2 * a1[4417] + a1[4416]) * a1[4448] * a1[4431] * a3[33];

a1 = AI object, a3 = waypoint, a2 = solver result. Offsets: a1[4416] 0x4500
CorneringAdd, a1[4417] 0x4504 CorneringMult, a1[4448] 0x4580 grip factor, a1[4431]
0x453C pace, a3[33] = wp+0x84 wp_event[0].

    v_target = (Grip(wp) * CorneringMult + CorneringAdd) * k4580 * pace * wp_event[0]

### 2b. Grip input (FACT code, origin OPEN)

`functions/TireForceSolver_004e8ce0.c` L60 `v6 = fabs(a4) * this[3903]`; L73 linear
interpolation in a 14-entry table (this+3904..3917), clamped at 128 (L80). The solver
argument a4 is a3[28] = wp+0x70. So the per-waypoint value at wp+0x70 is the lookup
key for corner grip. Its meaning and writer are OPEN. The table fill was not traced
(`functions/LoadingPhase1_004b87c0.c` L16 stores the resource pointer only).

Prior art (not re-derived): `ai-brain-corner-speed-braking-2026-10-02` (DERIVED) gives the
same formula. `ai-brain-corner-speed-live-confirmed-2026-10-02` (OBSERVED) replays it at
Monza, dry, n=3549, median logged/predicted 0.9993, using the logged tyre table and no
speed or car input. That is consistent with wp+0x70 being the only per-waypoint geometry
input.

### 2c. Braking (FACT code)

`functions/AIRacingLine_00425920.c` L259 target; L263 `TireForce(v46, *(v5+116), v5+120,
*(v5+112))`; L264 `v46 -= (v5+60)/v46 * this[17612] * target * 0.915`, clamp below 0.5.
Braking uses the target and chord length, not a radius.

### 2d. AI Min Radius (OPEN)

AI Min Radius default 20.0 (0x41A00000), `functions/PlayerOptionsInitializer_004aa270.c`
L1107-L1108, is a .plr option, not an AIW key. Its notes say it bounds the
"theoretical performance tables" (H3 section 5). It does not appear in 2a-2c. OPEN:
whether it clamps wp+0x70 or the table index.

Conclusion (2): radius is not a term in the target speed. Corner speed =
grip_table(wp+0x70) * CorneringMult/Add * wet * pace * wp_event[0]. DERIVED: a
geometry link, if any, is inside the wp+0x70 writer (OPEN).

## 3. Groove, path and the line (question 3)

- FACT: wp_path[0] (dry) is the racing-line lateral offset; AIRacingLine reads it
  (v15+24, H3 section 3). Live Silverstone dry AI sit on wp_path[0] (median 0.30 m),
  not wp_path[1] (1.59 m) (`ai-dry-drives-wp-path0-live-20261012`, OBSERVED).
- FACT: wp_path[1] (wet) drives the wet line and wetness profile
  (`wetness-lateral-profile-per-wheel-table-20261012`, CONFIRMED).
- FACT: wp_galpha (0x118) and wp_groove_lat (0x11C) have no reader found in the corner
  path or in functions/AI*.c by offset grep. OPEN: their reader (candidate
  `AIRacingLinePathFollowing_004e9ca0.c`) was not traced.
- DERIVED: line = wp_pos + wp_path[0] * side(wp_perp). Sign convention OPEN.
- DERIVED: wp_width and wp_dwidth bound the road; not in the speed formula.

## 4. Corner radii from the Austria AIW (question 4)

Script: `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\investigations\adaptive_ai_difficulty\aiw_radius_check.py`
(read-only, stdout only). Radius = circumradius of (i-3, i, i+3) in xz (about 12 m
baseline), main branch only (741 of 1070). R_k6 uses (i-6, i, i+6).
Run: `python -I aiw_radius_check.py "<path to 2001_A-1_Ring.aiw>"`

    waypoints total=1070 main_branch=741 lap_length_header=4281.659668
    arc_length_xz=4265.2 m + closing=10.8 m -> loop=4276.0 m
    rank  R_k3(m)  R_k6(m)  wp_index  arc_m    wp_score_dist_m  sector
       1     14.8     20.8       267   1527.3          1384.9  1
       2     21.6     32.0       106    595.3           450.5  0
       3     26.6     37.3       710   4094.8          3954.9  2
       4     36.3     38.1       407   2350.9          2209.0  1
    R_k3 median=905.1 m  p05=60.1 m  min=14.8 m

- DERIVED: arc_m minus wp_score_dist_m is about 140 m for all four (141.9, 144.8,
  139.9, 141.9). wp_score[1] looks like arc length from a shifted origin. Not proven.
- Waypoint spacing is about 4 m, so radii under about 20 m are rough. Accuracy judgment:
  +/- 30 %, not measured.
- A and B are under the 20.0 m default of AI Min Radius (B at 21.6 m is close). Not a
  proven clamp.

## 5. OPEN

1. Writer and meaning of wp+0x70, 0x74, 0x78 (the grip inputs). Key to the link.
2. Whether wp+0x70 is a function of the section 4 radii (test: logged wp+0x70 at corners
   A-D against R_k3; logs cited in `ai-brain-corner-speed-live-confirmed-2026-10-02`).
3. Sign and use of wp_groove_lat and wp_galpha.
4. Whether AI Min Radius clamps the grip input or the table index.
5. Writer of the wp+0x3C segment length.
6. Lap-distance origin (the ~140 m offset).

## 6. Next steps (not executed)

1. Find stores to v4[15] and v4[28..30] (pointer arithmetic) via
   `functions/IndexRemappingWithStride_00403540.c` and the load path through
   `functions/AIW_004c54f0.c`.
2. Compare logged wp+0x70 with R_k3 at A-D. A monotone 1/r relation would support the
   curvature hypothesis.
3. Trace `functions/AIRacingLinePathFollowing_004e9ca0.c` for wp+0x11C and wp+0x118 readers.
4. Decode the 14-entry TireForceSolver table fill.

## 7. Documentation gate

- Changed files: this report and aiw_radius_check.py (same folder). No game file,
  AIW, governance file or findings.jsonl was modified.
- Pending: append FACT items 1b, 2a, 2b, 3 and OPEN item 5 to findings.jsonl (not
  done; read-only scope).
- Not run: workspace structure check, `f1c_mod.py start --json`. Not verified: live
  wp+0x70 or table values; wp_path sign; radius accuracy; start-line offset.
- Confidence: code and format facts high (line-cited); radii medium-low; link OPEN.
