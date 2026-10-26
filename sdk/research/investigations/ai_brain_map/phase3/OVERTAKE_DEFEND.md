# OVERTAKE_DEFEND — D6: opponent-mode meaning from static code + live battles (BATCH B item 4)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d6_overtake.py`
(`watch_race2`, `watch_race2b`, `watch_race3`, `watch_race4fullc`, `watch_race4fulld`,
`watch_20261002T194606Z`), 20 Hz snapshots; outcomes judged from 1 Hz `race_position`.
Static: `functions/AISteering_004270a0.c` (mode state machine lines 940-1260), `map_Overtaking.md`,
prior art IR L1920/L2097/L2303/L3281/L5041/L3603 + `phase2/LIVE_RESULTS.md` Run C.

## 1. Static mode meaning

Mode is per opponent record (`this[28*slot + 17108]`), processed by `AISteering`; only modes
2/7 and 1/8 are passed to `Overtaking 0x42AF70` (L1202-1205 / L1246-1251). Prior art covers
1/2/7/8 (code + the lateral-bias writes); the readings for 3/4/5/16 below are this task's static
pass over the same function (tier DERIVED, GUESS on the human wording).

| mode | static assignment (AISteering) | proposed meaning | confidence |
|---|---|---|---|
| 0 | default | no opponent mode yet | high |
| 1 | `v86=1` when margin better than threshold; also `3 -> 1` conversion | pass attempt, side A (Overtaking accepted, 0x4420 bias **-3.0**) | high (prior art) |
| 2 | `v86=2` when blocked/slower margin; also `4 -> 2` conversion | pass attempt, side B (bias **+3.0**) | high (prior art) |
| 3 | transient: sets both side flags 1.0, adds `1.2*lateral` to a speed parameter, then becomes **1** | one-tick legacy conversion into state 1 | medium |
| 4 | transient: same shape, then becomes **2** | one-tick legacy conversion into state 2 | medium |
| 5 | early per-candidate: `lat < 4 m || gap <= threshold || gap < 0` -> state 5 and skip; also assigned as `v86=5` when neither attack nor approach condition holds (opponent state `+17756 <= 2`) | **hold/abandon** — do not force the move, slot in behind | medium |
| 7 | derived from state 2 side group (and the block that flips 16->7 when the previous record was 2); also entered from 1/2 when alongside | alongside phase, side B (Overtaking group {2,7}) | medium-high |
| 8 | derived from state 1 side group (16->8 when the previous record was 1) | alongside phase, side A (group {1,8}) | medium-high |
| 16 | assigned when closing margin OK, lateral gap >= 5.5 m and not yet alongside; on entry the stored lateral threshold `+17116` is multiplied by 0.85 | **approach / pull-up** — closing but not yet committed to a side | medium |

Key code constants: side-by-side cut 5.5 m / 4 m / 3.0 m; mode-16 threshold shrink 0.85; same-team
peer bonus (`18596` team match + `18600` score) reduces both margins by 95.0; interaction target
present (`this[4436]==1`) reduces them by 400.0.

## 2. Live transitions (6745 events, per mode index of the 5-record table)

| to mode | n | share | med dist | med \|lat_req\| | 0x45FC set | med closing m/s | pass/lost/none (10 s) |
|---|---|---|---|---|---|---|---|
| 16 | 2959 | 43.9 % | 33.7 | 0.00 | 0 % | +0.04 | 45 / 9 / 2891 |
| 5 | 2639 | 39.1 % | 31.5 | 0.00 | 0 % | -0.56 | 31 / 11 / 2584 |
| 2 | 426 | 6.3 % | 20.5 | 0.00 | 0 % | +0.21 | 19 / 2 / 393 |
| 1 | 350 | 5.2 % | 20.9 | 0.00 | 0 % | +0.32 | 6 / 0 / 336 |
| 7 | 156 | 2.3 % | 16.9 | 0.00 | 0 % | +1.07 | 5 / 4 / 144 |
| 8 | 155 | 2.3 % | 23.1 | 0.00 | 0 % | +0.34 | 6 / 2 / 146 |
| 4 | 40 | 0.6 % | 18.8 | 0.00 | 0 % | -0.02 | 0 / 4 / 31 |
| 3 | 20 | 0.3 % | 19.8 | 0.00 | 0 % | -0.79 | 3 / 0 / 17 |

Top transitions: **5<->16 (2420 + 2399)**; attack entries 16->2 (233), 16->1 (177), 5->2 (103),
5->1 (97); side conversions 2->7 (109), 1->8 (79); exits 8->16 (103), 7->16 (87); direction swaps
2->1/1->2 (38/35). Median time in the new mode before the next change: **0.21-0.30 s for 5/16** vs
**~1.0-1.3 s for 1/2/7/8/3/4** — the approach/hold pair is a fast oscillation, the attack and
alongside states are persistent.

Live-worded verdict: 16 and 5 are the standing states of racing near another car (approach vs
hold), flipping with the gap; a committed pass shows as a persistent 1/2 (then 7/8 alongside);
3/4 are one-tick migration states (0.3-0.6 %, ~1 s).

## 3. Battle-correlated fields (GUESS_FIELDS.md 0x455C, 0x4570..0x4579) at mode transitions

| mode | 0x455C med | 0x455C nz% | 0x4570 nz% | 0x4574 nz% | 0x4578 nz% | 0x4579 nz% |
|---|---|---|---|---|---|---|
| 16 | 3 | 98 % | 2 % | 2 % | 10 % | 10 % |
| 5 | 2 | 87 % | 3 % | 3 % | 8 % | 8 % |
| 2 | 2 | 99 % | 0 % | 2 % | **54 %** | **54 %** |
| 1 | 2 | 98 % | 1 % | 1 % | **54 %** | **54 %** |
| 7 | 2 | 98 % | 1 % | 4 % | 19 % | 19 % |
| 8 | 2 | 100 % | 1 % | 1 % | 18 % | 18 % |
| 4 | 3 | 100 % | 15 % | 10 % | **78 %** | **78 %** |
| 3 | 3 | 100 % | 5 % | 0 % | 30 % | 30 % |

- 0x4578/0x4579 (bitfield {0,1,256,257}) fire mostly in the attack modes 1/2 (54 %) and the
  transient 4 (78 %), rarely in 5/16 (8-10 %): live support for "pass/alongside state bits".
- 0x4570 is rare everywhere, most often in mode 4 (15 %); 0x4574 likewise (<=4 %).
- 0x455C is 2 in the attack/alongside states and 3 in the approach/transient states — consistent
  with a steering/battle state counter, not a pass Boolean.

## 4. Overtake/defend rules for consumers (do not misread)

- A mode change is **not** an overtake decision: 5<->16 churn dominates, and 0x45FC was never set
  at a transition in 6745 events across 6 folders (0/6745); the only live 0x45FC sample in this
  corpus remains Massa -> Villeneuve (same-team fallback, REVIEW_15).
- `lateral_request` 0x4420 is 0.00 at essentially every transition (the +/-3.0 write happens at the
  acceptance instant inside `Overtaking`, between samples); a zero log value does not mean "no
  intention" (f1c-memory note).
- PassingNorm 0x44CC is braking tuning, not intent (prior note).
- Outcome check: a completed pass within 10 s is rare across all modes (45/2959 for 16, 31/2639 for
  5, 19/426 for 2); the battle situations mostly capture proximity, not guaranteed passes.

## Limitations

- The 5-record mode table is team-record indexed; rank pairing in `opponents[]` is approximate
  (C1b Run C: 74 % rank/record mismatch on changes). Transitions are therefore counted per table
  index k (stable record), with context from the rank-k entry — context medians carry that caveat.
- Rank/team-record remap inside one situation can produce a false mode change for index k; the
  5<->16 churn is partly real threshold flapping, partly this remap.
- SC/RF mods active on Imola/Monza runs; no SC marker in logs.
