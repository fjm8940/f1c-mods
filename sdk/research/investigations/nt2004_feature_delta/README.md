# NT2004 → F1C feature-delta survey — everything that could be ported

**Date:** 2026-07-31 · **Corpus:** `nt2004_ida_decompile` (reference) + this one (target)
**Tier rule:** every "NT2004 has X" claim = extraction/DERIVED from `functions/*.c`
+ `strings_index.json`. Every "F1C has X" claim cross-checked against THIS corpus
(`config_schema.json`, `strings_index.json`, `findings.jsonl`). Nothing poke-verified
in F1C for these features — treat all of it as DERIVED until tested.

Method: string-token diff between the two corpora (same engine, same extraction
tooling), then manual read of every function behind a candidate token. 4,014 of
7,874 NT2004 functions already trace to an F1C twin, so the delta below is real
code, not re-invention.

---

## Verdict table — the whole delta at a glance

| # | Feature | NT2004 | F1C | Port candidate? |
|---|---------|--------|-----|-----------------|
| 1 | **Full-course caution / pace car** | complete steward machine | no race-control (only cosmetic per-zone yellow) | **YES — flagship** (see `docs/48`) |
| 2 | **Red flag** | machinery + rain trigger gated by a stub (`sub_460A50` returns 0) | machinery (waypoint assembly), no trigger; manual mod works | **YES — same shape, one gate from working** |
| 3 | **Yellow no-overtaking enforcement** | geometry check + penalty + lap deduction | none | **YES — ships with #1** |
| 4 | **Pit-lane speed policing** | into/out-of/into-and-out-of pit lane variants, + "Excessive Speeding" | only inside-pitlane speed | **YES — small, self-contained** |
| 5 | **Penalty types + stacking** | Stop/Go, Pass-Through, Start-At-End-Of-Longest-Line, +15 s stacking, "Cannot serve stop/go under yellow" | Stop/Go-style only, no stacking | **YES — small, self-contained** |
| 6 | **Riding the wall penalty** | warning → penalty under caution, lap-won't-count in race | absent | **YES — tiny** |
| 7 | **Cut-track / wrong-way penalties** | penalty under caution (not just lap-won't-count) | lap-won't-count + wrong-way disqualify | minor — ride along with #1 |
| 8 | **AI drafting / slipstream** | AI states "draft some car"/"break draft" + DRAFT HUD | **zero** draft/slipstream anywhere | **YES — the F1-relevant one (F1 has real slipstreaming)** |
| 9 | **Waypoint multi-line data** | 21 `wp_*` tokens incl. groove_lat, branchID, cheat, stockcarlat, pitlane, transitions | parses ~12 of the same; **lacks** `wp_cheat`, `wp_stockcarlat`, `wp_pitlane`, `wp_transitionlat`, `wp_lockedLats`, `wp_marks` | mostly NASCAR-specific; skip |
| 10 | **Caution light / HUD** | "Under Caution:", pace-car HUD, caution light timing | none | ride along with #1 |
| 11 | **Black-Only rule-enforcement level** | Flag Rules = Off / Black Only / All | Flag Rules exists, blue-only | minor — ride along |
| 12 | **Chase (playoff)** | NASCAR Chase | absent | NO — NASCAR rule, not F1 |
| 13 | **Career economy** | buy/sell cars, research, sponsors, reputation, entry fees | `[CAREER]` config section only | NO — game-mode scale, not a mechanic |
| 14 | **AI aggression knob** | global AI Aggression + driver slider | per-driver `TrackAggression` (.rcd, CONFIRMED) | **NO — already have it** |

**Shared (NOT deltas — do not port):** formation lap, race time/length, test day,
driving school, hot lap, warmup, reconnaissance, run sessions, weather/rain
(RainAbility, StartsWet/Dry), tire compounds (F1C has 5; NT2004 adds a 6th "Dry
Compound"), pitcrew push, debris/parts removal, spin recovery, crash recovery,
mechanical failures, quick race, grid walkthrough, points, session structure.

---

## 1. Flagship: the caution/pace/red system → `docs/48-nt2004-caution-flag-system-port.md`

Full map (all six layers, port dependency order) is the paper. Summary of what
F1C reuses vs builds:

| Layer | NT2004 | F1C already owns |
|-------|--------|------------------|
| Steward brain | `sub_4F9670`/`4F99C0`/`4F9920`/`4F93E0`/`4F95D0` | — (F1C steward = race management only) |
| Command bus | type-4 sub-commands on the **global async** 208-byte ring | **NO** — F1C's `sub_424CD0` is per-car/synchronous; caution commands must become DLL-side state (CONFIRMED 2026-07-31) |
| Pace-lap driver | `sub_4FB270`/`42F950`/`4FA660`/`4FA1A0` | — |
| Per-car flag modes + ordering | `sub_4F8D30`/`4F9F50`/`4FA900` | only per-car queue maps onto `sub_424CD0` |
| Pace car vehicle | roster entry "pace car", slot 48 | **half** — Maylander already in roster |
| Enforcement | `sub_4DF490` geometry + penalty queue | **partial** — penalty pipeline exists |
| Speed lever | per-car pace speed (`+0x5178`) | **YES (FACT)** — `sub_42ACD0(veh, N)` = veh+0x4538 maxSpeed, whole-field hold live-proven |
| Track config | `PaceCarReleaseDist` in .aiw | parser `sub_402690` ready |

F1C's own leftover proof it was cut, not never-built: track files ship the
`ultracarefulyellow` .aiw waypoint token with **no parser and no samples** —
the yellow-AI hook survived in the data.

> **Live port status (2026-07-31):** the port project is already underway at
> `mods/extended_vanilla_features/patches/full_safety_car/` (`PLAN.md`, Phases
> 0–6). Two hard runtime pieces are FACT-proven there: the whole-field speed
> lever (21 cars @ 30 m/s, ~143 s, zero overrides) and that `sub_424CD0` is
> per-car (~15 Hz, cmds 0/1/4/5/14/15). Also: `veh+0x443C` is a **one-shot gap
> recompute**, not a following mode (the old `oph.c` name `SafetyCarFollow` is
> wrong), and NT2004's `dword_75CBA8-B8` is ONE session-state struct whose
> race-phase setter `sub_42F6C0` is the enter-caution entry point. Read that
> project before building on this map.

## 2. Penalty-system enrichment (small, self-contained port)

NT2004's `sub_4DDB70` is the structural twin of F1C's `sub_4BCCC0` (same pit-speed
accumulator, off-track cut detector, wrong-way detector, blue-flag-following loop,
pit-selection state machine). The deltas that would drop into F1C's existing
pipeline:

- **Pit-lane speed is directional**: "Sped Into Pit Lane", "Sped Out Of Pit Lane",
  "Sped Into And Out Of Pit Lane" (`sub_4DDB70` picks by which side of the pit
  boundary the speed accumulator tripped). F1C only catches speed *inside* the
  lane. The `+0x40C8`-style flag (NT2004 `*(veh+17412)`) is the "was in pit lane"
  marker.
- **Penalty types**: `sub_4DC690` posts type byte + value byte on command-queue
  sub-cmd 0x17; `sub_4DC900` accumulates: **Stop/Go**, **Pass-Through**,
  **Start At End Of Longest Line**, and a second Stop/Go adds **15 s**. F1C's
  penalties are all "Stop/Go"-flavoured with no type distinction or stacking.
- **Riding the wall** (`sub_4DDB70`, `v4+19229 && v4+17564 < -15.0`): warning →
  penalty under caution, "Lap Time Will Not Count" in normal race.
- **Cut track / wrong way / ignored blue become *penalties* under caution**
  (not just lap-won't-count warnings). The geometry that triggers each is the
  same as F1C's (`>1.75` track-width excursion, `>125` units wrong-way, 30 s
  blue-flag follow). Only the *consequence* is richer.
- **Lap deduction** as a penalty: `sub_4DF420` does `car.laps -= n` directly and
  announces "%d Laps Also Deducted" (the yellow-pass path). F1C has lap counting
  but no deduction primitive.

Port shape: extend F1C's `sub_4BCCC0` cases + add the penalty-type byte to
F1C's existing penalty pipeline. F1C's `StopGoPenaltyHandler`-family already does
the announcement; the missing bits are type/stacking/lap-deduction.

## 3. AI drafting / slipstream — F1-relevant because F1 has real slipstreaming

NT2004 has an AI drafting behaviour F1C lacks entirely (zero draft/slipstream
strings in F1C):
- AI state names: **"draft some car"**, **"break draft"**, **"draft"** (with
  sibling states "pass some car", "let some car pass", "block some car",
  "maintain with car", "fastest line", "left/right/super line", "passing line").
- HUD: `DRAFT_01.bmp` / `DRAFT_02.bmp` — an on-screen draft indicator.
- The strings are orphaned (referenced only through a dev-console/test table —
  `+draft`, `+break draft` console commands exist), so the *physics* is in the
  AI driver's decision code and was not pinned to a function this pass.

**Open target**: find the NT2004 function that grants the draft benefit (drag
reduction or power boost when close behind a car). The aero tokens are shared
(`BodyDragBase`/`RadiatorDrag`/`FWDragParams`... both parse them), so the draft
is an *addition* on top of the shared drag model. For F1C, slipstream at high
speed is historically huge — this is the most F1-shaped of all the NT2004-only
features and worth a dedicated hunt.

## 4. Waypoint data — mostly shared, NASCAR-only extras

Both games parse the same `.aiw` `[WAYPOINT]` section with a 348-byte waypoint
struct. F1C (`sub_402690`) already reads `wp_pos/width/dwidth/score/groove_lat/
galpha/perp/bitfields/path/branchID/wpse/event/ptrs`. NT2004 (`sub_402C00`)
additionally reads:
- `wp_cheat` — an AI "cheat" line (allows cutting a corner)
- `wp_stockcarlat` — a second, wider lateral line for stock cars
- `wp_pitlane` — per-waypoint pit-lane marker
- `wp_transitionlat`/`wp_translat` — lateral transition lines
- `wp_lockedLats` — locked lateral lines
- `wp_marks` — painted groove marks
- `wp_alpha` (F1C has only `wp_galpha`)

These are NASCAR-shaped (wide stock cars, oval lines). Skip for F1C — the
multi-groove AI line system (`wp_groove_lat`) already exists in F1C.

## 5. Race conditions — effectively a tie

`RACECOND_*` token sets are near-identical (flag_rules, formation,
reconnaissance, timescaled_weather, walkthrough, weather both). NT2004 adds a
per-track `RunSessions` and `Temperature` option; both cosmetic/session config.
No port value.

## 6. What the unmatched-function list says is new (for completeness)

3,860 NT2004 functions have no F1C twin. Keyword clusters in the ranked subset:
AI (37), Collision (14), Tire (11), Race (9), Suspension/Wheel (12), Lap (6),
Season (6), Track (5), Driver (5), Pit (3), Pace (2). Most of the AI/vehicle
cluster is the caution system + AI driver personality (aggression/skill
per-controller in `sub_41FB90`) — the personality is a re-architecture of the
same per-driver ability model F1C already has via `.rcd`. Collision/tire/
suspension deltas were not individually chased; they are 2003-build physics
tuning, not new features.

---

## Recommendations for F1C

1. **Port the caution system** — that is the one feature with no F1C
   counterpart, a clean six-layer decomposition, and two of six layers already
   present. Blueprint: `docs/48`.
2. **Port the penalty enrichment** — smallest effort (extend `sub_4BCCC0` +
   penalty pipeline), immediate value (pit-speed policing is directional,
   penalties can stack, lap deduction exists).
3. **Hunt the NT2004 draft physics** — F1-shaped, but the implementation site
   is not yet pinned. Worth a dedicated trace session on the AI driver
   decision code (follow `sub_5A2930`'s consumers in the 0x50xxxx module).
4. **Skip** Chase, career economy, waypoint extras, black-flag level — either
   NASCAR-specific or already superseded by F1C's own systems.

## Open items

- NT2004 draft physics function not located (strings are data-table-only).
- `sub_4FB270`'s per-car `+0x5178` pace-speed formula not reduced to constants.
- Whether NT2004's `sub_4DF490` runs for AI cars or player-only at runtime
  (static says per-frame per-car; the caller `sub_4DE8F0`/`sub_4DE880` are
  per-car driver ticks).

---
*Changelog: 2026-07-31 — initial sweep.*
