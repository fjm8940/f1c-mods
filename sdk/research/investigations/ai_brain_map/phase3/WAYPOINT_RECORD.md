# WAYPOINT_RECORD — D14: waypoint offsets read by the AI set (BATCH B item 11)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Live ranges: `phase3/analysis/d14_wp.py`
`watch_race4fullc`, `watch_race4fulld`, `watch_20261002T194606Z` (all 20-waypoint logs).
Static readers: `functions/*.c`; architecture cross-link:
[`docs/52-aiw-navigation-static-architecture.md`](../../docs/52-aiw-navigation-static-architecture.md)
(316-byte stride, pointer remap, `wp_event` at +0x84/+0x134/+0x138, branch IDs 0..117).

## 1. Field table

| offset | type | log key | meaning | reader (function + line) | tier | live range (min / med / max) |
|---|---|---|---|---|---|---|
| 0x20 | float | `rx` | waypoint right vector X (unit, 2D) | `sub_426430` lateral position `-(dot(car-wp, right))` (L~186); `sub_426100` steering target `+0x4408*wp.right` (doc 54 section 2) | OBSERVED | -1.000 / 0.546 / 1.000 |
| 0x28 | float | `rz` | right vector Z (right+8; Y at +0x24 unused) | same | OBSERVED | -1.000 / -0.002 / 1.000 |
| 0x34 | float | `wall_l` | left wall/width bound | `AISteering` corridor clamping (map_AISteering, corridor bounds) | DERIVED | 1.257 / 8.800 / 30.000 |
| 0x38 | float | `wall_r` | right wall/width bound | same | DERIVED | 1.301 / 10.400 / 29.294 |
| 0x3C | float | `seg_len` | node segment length (m) | `AIRacingLine` braking step `v -= seg_len/v*0x44CC*TireForce*0.915` (L263ff; DROPPED_ST0_SCAN row) | OBSERVED (D1/D11 replay) | 2.851 / 5.473 / 13.240 |
| 0x48 | float | `dist` | distance along the lap (m) | `AISteering` traffic/pit lane distance comparisons (`*(float*)...+72`) | DERIVED | 1.087 / 1249.1 / 5750.6 |
| 0x50 | float | `line_l` | racing-line left bound | `AISteering` candidate clearance (map_AISteering L512: `v47 / wp[+80] * next[+80]`) | DERIVED | -0.400 / 3.549 / 28.500 |
| 0x54 | float | `line_r` | racing-line right bound | same as +0x50 | DERIVED | -0.650 / 5.591 / 28.644 |
| 0x70 | float | `k112` | `TireForce` a4 / `TireForceSolver` a4 (seed input) | `AIRacingLine` braking step call args (`TireForce(v, wp+116, wp+120, wp+112)`); `AIComputePassScore` solver args (wp+0x70) | OBSERVED (D1/D11 replay) | -0.0848 / ~0 / 0.0940 |
| 0x74 | float | `k116` | `TireForce` a2 / solver a2 | same | OBSERVED (D1/D11) | -0.0094 / ~0 / 0.0064 |
| 0x78 | float | `k120` | `TireForce` a3[0] / solver a3[0] | same | OBSERVED (D1/D11) | -1.282 / ~0 / 0.866 |
| 0x7C | float | `k7c` | `TireForce` a3[1] (vertical/load term) | braking step call (`wp+124`) | OBSERVED (D11) | -9.810 / -9.810 / -9.654 |
| 0x80 | float | `k80` | `TireForce` additive `wp128` | braking step call (`wp+128`) | OBSERVED (D11) | -0.680 / 0.0449 / 1.212 |
| 0x84 | float | `speed_k` | per-node corner-speed factor; `wp_event` value A | `AIComputePassScore` return `* a3[33]`; `AIRacingLine` multiplies the limit (docs/52; D1/D11) | OBSERVED (D1/D11; constant 1.0 in these 4 runs) | 1.000 (all samples) |
| 0x94 | float | `x` | node position X | `sub_426430` locator; `sub_426100` steering target `wp.pos(+148)`; `AIRacingLine` | OBSERVED | -126.8 / 106.3 / 1086.2 |
| 0x98 | float | `y` | node position Y | same | OBSERVED | -11.76 / 1.576 / 31.10 |
| 0x9C | float | `z` | node position Z | same | OBSERVED | -1286.2 / 467.8 / 2057.4 |
| 0xA4 | float | `dx` | node direction X | `sub_426100` steering target `+ d*wp.dir(+164,+172)` | OBSERVED | -1.000 / 0.001 / 1.000 |
| 0xAC | float | `dz` | node direction Z (dir+8) | same | OBSERVED | -1.000 / 0.537 / 1.000 |
| 0x110 | _DWORD | (not logged; `ptr` is the record's own address) | next waypoint pointer | `sub_426430` L121 (`v15 = *(wp+272)`); `AIRacingLine` L68/L240 | OBSERVED (chain integrity, docs/52) | pointer, stride-aligned |
| 0x114 | _DWORD | (not logged) | alternate/branch pointer, used when branch id matches | `sub_426430` L120 (`*(wp+276)` + `[+300] == branch`); `AIRacingLine` L67/L238/L249 | OBSERVED (docs/52) | pointer |
| 0x12C | _DWORD | (branch id) | branch selector id compared with the active branch (0x454C) | `sub_426430` L120-121 (`*(alt+300) != v13`); docs/52 branch 0/1 + grid/pit pseudo IDs | DERIVED | int id |
| 0x134 | int | `branch_flag` | `wp_event` code A (int at parse; docs/52) | `AIRacingLine`/D1 bypass `wp+0x134 == 1 -> pit target 0x8ACFB4 or int wp+0x138` | OBSERVED for value 1/18 at Monza records 983-984 | mostly 0; 1 (1003 samples), 4/7/256 codes |
| 0x138 | int | `branch_val` | `wp_event` code B (int) | `AIRacingLine` `fild wp+0x138` (D1 bypass) | OBSERVED (Monza event records = 18) | 0, 18, 2, 256, plus non-int bit patterns |
| stride | 316 B | - | full record size (0x13C) | parser/array (docs/52; not an AI field) | OBSERVED | - |

Not in the AI-read set but part of the record: previous-pointer and the placement/path-blend fields
(docs/52 section 3); they are not consumed by the mapped AI functions and are out of scope here.

## 2. Notes and caveats

- The 6 base-open objects item is partly closed: the waypoint node consumed by the mapped AI set is
  now fully enumerated (position/right/dir/bounds/seg_len/dist/tyre inputs/speed_k/event/pointers).
  The remaining base-open consumers are not waypoint-node fields.
- `branch_flag`/`branch_val` are not always clean ints in the live chain: Monza shows real event
  records (1, 18) and small codes, but also float-bit patterns (e.g. `270012448`, `1125351552`) on a
  small fraction of records (race4fullc ~0.6 wp/snapshot). They were excluded by D1/D11's
  `branch != 0` filter, so the replays are conservative; the wet run is all zeros. Flagged for
  Claude: those records may be non-racing-line/pseudo nodes or a second bitfield syntax.
- `speed_k` is 1.0 in every sample of these four runs (consistent with docs/52: the `wp_event`
  records 983/984 have value A = 1.0); the D1/D11 formulas used it as a multiplier, so no effect here.
- Values come from all four 20-waypoint logs; k7c/k80 were logged from a later watcher revision
  (n = 2 384 900), the rest n = 3 152 620.

## 3. Cross-links

- `docs/52-aiw-navigation-static-architecture.md` — stride, pointer remap, branch IDs, event-node
  `(1.0, 1, 18)` values, live record semantics.
- `docs/54-ai-driving-brain-map.md` section 1 (locator) and 3b (braking/corner speed).
- `phase3/D1_CORNER_SPEED.md`, `phase3/D11_WET.md` — offsets used in the confirmed replays.
