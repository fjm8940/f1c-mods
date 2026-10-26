# AI Brain ABI v1 - README

Machine-readable registry of the F1C AI driving brain: `ai_brain_abi_v1.json`
(schema `f1c.ai_brain_abi.v1`), built by `build_abi.py` from `offsets_ALL.csv`,
the per-function `map_*.md` files, `FIELD_GLOSSARY.md`,
`docs/54-ai-driving-brain-map.md`, the `REVIEW_*.md` records and `findings.jsonl`.
Every hex/dec/type/writer/reader is resolved from the generated CSVs; the build
fails if a field offset is missing or a cited source does not contain its hex.
Acceptance test: `python investigations/ai_brain_map/test_abi.py`.

This is a read-only registry. No GUI and no DLL code are part of B8.

## How to read a car

- Vehicle base: `veh = 0x5EC6D8 + 0x48C0 * slot` (doc 54; the live slot table
  reindexes between sessions, so always resolve the slot from the car you mean).
- Driver name: NUL-terminated string at `veh + 0x4620` (live-verified 2026-10-01,
  finding `veh-4678-car-4714-veh-path-live-2026-10-01`).
- `base = "veh"` fields are byte offsets from that base.
- `base = "opp_slot"` fields are slot-relative: the five 68-byte slots start at
  `veh + 0x4174`; `hex` is the slot-0 vehicle-relative address, `count` is 5 and
  `stride` is 68. `base = "wp"` is reserved for waypoint-node fields (none in v1).

## Groups

In order: `state`, `perception`, `plan_speed`, `plan_line`, `decision`,
`watchdog`, `derived_views`. Each field carries `id`, `group`, `base`, `hex`,
`dec`, `type`, `count`, `stride` (arrays), `units`, `meaning`, `tier`, `source`,
`writer`, `writers` and `readers`. `derived_views` entries give formulas a GUI
can compute from the fields instead of reading extra memory.

## Tiers (GUI contract)

- `OBSERVED` - live-observed in doc 54 section 2.1 or in a live finding.
- `DERIVED` - statically derived or prior-art documented (doc 54 / findings);
  correct on the evidence but not necessarily live-proven for every track/car.
- `GUESS` - map-level transcription, no review yet.

**Fields with tier `GUESS` must be shown as GUESS in any GUI.** Do not present a
DERIVED or GUESS field as live-confirmed truth; show the tier and the `source`.

## Readers from all CSV forms (REVIEW_4 fix)

`writers`/`readers` are taken from every CSV form: `base = this`, the `a1`
vehicle argument of `AIComputePassScore`, the `v37` team-record alias of
`OpponentProximityScanTeamRecognition`, and direct/array/indexed/arith/deref2
rows. This fixes the B5 glossary gap: readers of `0x4500`, `0x4504`, `0x4580`
and `0x453C` now include `AIComputePassScore`.

## Scope caveats (REVIEW_4 + map questions)

- `AIRacingLinePathFollowing` writes are a placement/visual path walk, not
  per-tick driving; the placement copies it writes (`placement_pose_*`) are tier
  `GUESS`. The live world position the brain uses is `position_x/y/z`
  (0x34E0/0x34E4/0x34E8), tier `DERIVED`.
- `AISteeringRacingLineCorrection` is a recovery/re-placement service
  (session-type gated), not a normal racing term; treat its writes as recovery
  output, not steady-state driving.
- `0x4468` conflict: doc 54's live check calls it the steering look-ahead index
  (0..3 by speed); finding `organic-pit-trace-full-field-map` calls it a
  pit-phase enum. The registry keeps the doc 54 meaning and flags the conflict.
- Slot team index (slot-relative `+0x40`) is documented in
  `map_OpponentProximityScanTeamRecognition.md` but has no CSV row, so it is not
  a v1 field. Slot stride 68 and base `0x4174` are.
- Writers of `0x4408` and `0x44B4` are still OPEN (doc 54 section 7); the fields
  are live-observed but no plain store was found.

## Rebuild and verify

```
python investigations/ai_brain_map/build_abi.py
python investigations/ai_brain_map/test_abi.py
```

Do not edit `ai_brain_abi_v1.json` by hand; edit `build_abi.py` and rebuild.

## v1.5 (REVIEW_18, 2026-10-03)

`ai_brain_abi_v1_5.json` = v1.4 plus 5 value-type fixes and nothing else (same fields, offsets, tiers, meanings).
Built by `phase3/merge_abi_v1_5.py`; `test_abi.py` checks it (`OK v1.5`). The fixes come from the decompile:
0x441C float, 0x4450 float (units m, copy of waypoint +72), 0x4528/0x452C float (+/-1.0), 0x4590 int (lap).
They were found when the AI Brain viewer showed float bit patterns as integers. Finding:
`ai-brain-registry-v15-type-fixes-2026-10-03`. `brain_core.py` now loads v1.5 first, and replay retypes older
recordings to v1.5 types.
