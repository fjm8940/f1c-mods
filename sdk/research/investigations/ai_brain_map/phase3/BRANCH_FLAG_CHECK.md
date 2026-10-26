# BRANCH_FLAG_CHECK -- E5: wp+0x134/0x138 type and the Monza float-bit records

Date: 2026-10-02. Author: DeepSeek V4.1 (BATCH C, offline). Task: handoff BATCH C item 5.
Question: D14 flagged `branch_flag` (wp+0x134) / `branch_val` (wp+0x138) records whose live values
look like float bits (e.g. 270012448, 1125351552). Are those records non-waypoint nodes? Is the field
an int flag or a float? Does brain_core's decode need a change?

L208-222; `phase3/WAYPOINT_RECORD.md`; `phase3/D1_CORNER_SPEED.md` L30-31.

## 1. Type at the consumer and at the parser: int32

AIRacingLine (capstone re-read this task):

| address | instruction | meaning |
|---|---|---|
| 0x425DA8 | `cmp dword ptr [edi+0x134], 1` | limiting-waypoint bypass test (int) |
| 0x425DB1 | `fild dword ptr [edi+0x138]` | code B used as a signed INT target (`fild`) |
| 0x425E81 | `cmp dword ptr [edi+0x134], 1` | projection-loop bypass test (int) |
| 0x425E8A | `fld dword ptr [0x8acfb4]` | code-1 projection override = global pit speed |

Parser `sub_402690` (file parser, this task):
- L296-301 `wp_event`: values parsed into doubles v44/v45, then `v4[77] = (int)v44; v4[78] = (int)v45;`
  (v4[77] = 0x134, v4[78] = 0x138) - integer stores.
- L303 `wp_wpse`: `sub_565760(aWpWpse, v4 + 77, v4 + 78, ...)` - two integer values directly at
  0x134/0x138.
- L295 `wp_path`, L294 `wp_branchid`, L319 `wp_bitfields` are separate fields.
- `AIDatabase_Init` 0x404117/0x404121 writes 0 to `[esi+0x134]/[esi+0x138]` only for the two synthetic
  fallback records.
- Whole-.text store scan for displacement 0x134/0x138 found no other writer into a waypoint record
  (the many other hits are unrelated structs with their own +0x134 field).

So: wp+0x134 is an **int32 discriminant with exactly one defined value, 1**; wp+0x138 is an **int32**
(pit-speed override on -1, or a target speed). No float consumer exists. brain_core.py already decodes
both as `<i` int32 (lines 114-115) - **correct, no type change needed**.

## 2. The odd records are real waypoints, not pseudo-nodes

Chain dumps (one per odd group) show every odd record is in a stride-316 sequence with its neighbours,
with sane x/z/wall/seg fields that move along the track. Example (watch_race4fullc, Massa t=50):
0x31b33fe8 .. 0x31b3575c all `d=316`; positions z 414.2 -> 546.9; the last record carries
flag=270012448. (watch_race3 Monaco: 20/20 chain records carry float-pattern values; positions and
walls are a normal track segment.) Not grid/pit pseudo-nodes: those exist (docs/52: records 1168-1203,
`wp_bitfields=1`) but they are not what these values are.

Groups seen in 128 928 chained snapshots:

| run | flag | val | n | distinct ptr | comment |
|---|---|---|---|---|---|
| race4fullc | 270012448 (0x10181020) | 270340128 (0x101D1020) | 14446 | 1 | single stationary record at the end of a normal chain |
| race4fulld | 7 | 0 | 14195 | 10 | small int; real-looking code but not Monza's known event (1,18) |
| race4fulld | 0 | 256 | 6493 | 4 | second word only |
| race4fullc | 4 | 2 | 1604 | several | - |
| race3 (Monaco) | float patterns | 0 | ~90k | many | every record of the segment; float values track the record's own +0x48 value - approx. 111 m |

Monaco example (flag reinterpreted as float): -12.08, -6.14, -0.37, 5.61 ... 95.25 while the record's
+0x48 value runs 99.45 ... 205.46 (difference ~110-112 m, i.e. the slot content is a distance-like
float, not an event code). Monza's real event records are 983/984 = (1, 18) (docs/52 L77, OBSERVED);
none of the odd values above is 1.

## 3. Explanation: unwritten slots on records without wp_event/wp_wpse

The parser writes +0x134/+0x138 ONLY when the AIW line supplies `wp_event` or `wp_wpse`. Records whose
file entry has neither key are never written, and no per-record zeroing exists (only the fallback
records get 0 from `AIDatabase_Init`). Those 4 bytes therefore keep whatever the waypoint array
allocation contained: mostly 0 on the Monza main path, but on some records/whole segments (Monaco)
they hold float-like leftovers from a previous allocation or pass. The values are stable per record
(distinct ptr = 1 per group across thousands of snapshots), stride-aligned, and move with the track -
properties of memory content, not of a second event syntax. [DERIVED, supported by: parser writes
conditional; init only for fallback; store scan; per-record stability; float values tracking +0x48.]

Consequence for gameplay: none. Both consumers test exactly `== 1`; 270012448, 7, 256 etc. all fail
the test and the normal AIComputePassScore path is used.

## 4. brain_core / recorder actions

- brain_core.py: int32 decode is correct; no change required for type. Recommended (Claude owns the
  edit): treat the pair as meaningful only when `branch_flag == 1`; expose a derived `branch_event`
  bool instead of letting raw nonzero flags look like events. Update the comment at brain_core.py
  line 28 ("branch_flag==1 -> pit speed target, branch_val used as int target") to state that other
  nonzero values are undefined slots, not codes.
- D1/D11-style replay filters should use `branch_flag == 1` (not `branch_flag != 0`): the current
  filter over-excludes (it drops Monaco's whole chain and ~0.6 wp/snapshot on Monza) while the
  consumer only cares about ==1.
- Recorder: keep logging the raw ints; do not reinterpret as float. Optional: a `branch_event` key.
- Result: correct type = int32; records are ordinary waypoints; no decode change needed beyond the
  semantic filter above. [E5 verdict]

## 5. Limits

- The .aiw source files were not read (game install is off-limits; docs/52 already established the
  parser semantics and the Monza (1,18) records). A definitive "uninitialized heap" proof would need
  a heap-watch or an .aiw dump; the DERIVED explanation above is the only one consistent with all
  current evidence.
- No game memory was written; no live work.
