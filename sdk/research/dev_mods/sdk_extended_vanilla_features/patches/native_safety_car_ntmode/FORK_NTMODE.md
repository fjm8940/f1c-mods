# native_safety_car_ntmode — the NT2003 mode-gate fork (2026-08-04)

Fork of `native_safety_car/` on branch `feat/native-sc-ntmode`. Implements the
NT2003 pace-car mechanism researched this session, instead of the parent's
pit-machinery workarounds.

## The research that motivated the fork

NT2003 keeps its pace car in a separate array (off_5BD508) AND carries a
**mode field** `this[2674]` = offset `0x29C8` that **11 engine passes gate on**:

| NT gate | what it protects |
|---|---|
| sub_4E9340/4E9370/4E9630/4E9670 | the four "report to world" calls |
| sub_4E95E0, sub_4EA070 | **zero the pace car's position** every pass |
| sub_4E9570 | standings/classification |
| sub_4E53C0 | the session state machine |
| sub_4E9BB0 (1214 B) | the per-frame pace-car driver |
| sub_4E75B0 | timer/flag reset |
| sub_4E7280 | stall teleport; reports to world only if mode == 0 |

Nonzero mode = "special car, skip me". F1C is the same codebase with the
pace-car pass compiled out: **the struct still has the field at the same
offset (`veh+0x29C8`, zero writers, zero readers in the exe)** but the gates
were stripped. F1C kept exactly ONE gate — `AIMainUpdate` self-gates on
`0x32C8` (the role field) — which is why the parent's SC gets driven but
still listed: driving gate survived, everything else didn't.

F1C gate-site mapping (by behavior, not address — F1C and NT2003 are
different builds, addresses shifted):

| NT effect | F1C hook site (this fork) |
|---|---|
| never grid-placed | **0x4D10E0** (grid placement thunk → sub_403CD0). Skip mode cars |
| no classification row | **0x4D9690** (row renderer), **0x4DA9E0** (standings row), **0x4DA0D0** (pairwise) |
| no scoring/standings entry | **position zeroing keeper** on 0x4058/0x4060 (F1C port of sub_4E95E0/4EA070) |
| not driven | already covered by F1C's own surviving gate (0x32C8) |
| not commanded by the world | no F1C equivalent (no caution world object) |

## What the fork does differently from the parent

1. **Mode marker**: `veh+0x29C8 = 1` written at add time (sc_add_entry). The
   dead NT field, resurrected.
2. **Five gates** (hook detours, all skip mode-marked cars):
   - `0x4D10E0` grid placement (never placed on the grid / never re-pinned)
   - `0x4BF390` AIDriverSorting (never receives a classification position —
     the F1C form of NT's `sub_4E9570` standings gate)
   - `0x4D9690` row renderer (no classification row)
   - `0x4DA9E0` standings row (no standings row)
   - `0x4DA0D0` pairwise renderer (no player-vs-SC row)
3. **Position exile**: after park, `0x4058`/`0x4060` are pinned to **24**
   (above the field). AIDriverSorting ranks 1..23 and the row loops request
   positions ≥ 1 — 24 matches nothing, so the SC is excluded from the
   standings structurally without touching VehicleCount.
   (Pinning to 0 was TRIED and REMOVED: the grid renumber sorts by 0x4058
   ascending, so 0 put him on pole and pushed the player to 23 — live
   log `[mode] 0x4060 forced to 0 (was 1)` ×7, player 0x4058=23.)
4. **Pit-box teleport simplified**: no four-flag hand-over. With the mode gate
   the SC is never grid-placed, so the start sequence never re-pins him — a
   plain teleport into the box at RACE_PHASE 4 is all that's needed.
5. Spawn/hook/park/eject machinery inherited unchanged from the parent
   (design B, proven live run 21).

## Files

- `dll/native_sc.c` — the fork. `.bak_designB` = the parent's version it was
  copied from.
- `FORK_NTMODE.md` — this file.
- Everything else inherited from the parent; DESIGN.md / SESSION docs there
  describe the proven spawn machinery (still accurate).

## Status

LIVE-TESTED 2026-08-04, two iterations:

- **v1 (keeper zeroing)**: broke. Zeroing 0x4058/0x4060 made the grid
  renumber sort the SC to POLE (0 = smallest key) — he showed as hidden P1,
  Hakkinen P2, player bumped to 23, and the classification screens flashed
  (keeper fighting the renumber every 20 ms). Keeper removed.
- **v2 (gates only, no exile)**: SC not visible (row gates work) but still
  counted in the standings (POS 22/23) — he held a real position the
  classification looked up.
- **v3 (current)**: sort gate + position exile to 24. UNTESTED — loaded in
  the running game, waiting for a fresh race.

Watch `native_sc_log.txt` for:

- `[add] mode marker veh+0x29C8 = 1` — the marker went in
- `[hook] gridplace gate ... / sort gate ... / row gate ... / rowpage gate ...
  / pair gate ...` — the five gates armed
- `[mode] 0x4058/0x4060 exiled to 24 (was N)` once at park — the exile took
- grid screen: 22 rows, no Bernd, he sits in his pit box (ini
  `[pitbox] enabled=1` + coords)

Expected outcome, per mechanism:
- No standings entry — sort gate keeps him unranked, exile 24 matches no rank
  1..23, row gates hide any residual row.
- No grid box — the placement thunk returns before sub_403CD0 for mode cars.
- Player untouched — the gates only skip mode-marked cars.
- Deploy (Numpad1) sets role 1 + rebuild while the marker stays 1: he enters
  the active array, AIMainUpdate drives him (the surviving gate), but he is
  never classified — exactly NT2003's behavior for a deployed pace car.
