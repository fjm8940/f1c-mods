# SESSION 2026-08-04: SPA LANE DISCOVERY — organic pit call + AIW lane mapping

Track: 1999_Spa_Francorchamps (1999 season, 22 cars). Game PID 22164, Frida session,
native_sc_ntmode.dll injected.

## 1. Driver-name field (STOP flailing — documented field)

`veh+0x4620` = ASCII driver name (PROMPT_NATIVE_SC_CRASH.md:27). Read it per slot,
never guess roster layout. Live roster at Spa 1999:

| slot | driver | team |
|---|---|---|
| 0 | Matias (player) | 2 |
| 1 | Mika Hakkinen | 0 |
| 2 | Michael Schumacher | 1 |
| 3 | Rubens Barrichello | 7 |
| 4 | Eddie Irvine | 1 |
| 5 | Ralf Schumacher | 2 |
| 6 | Damon Hill | 3 |
| 7 | David Coulthard | 0 |
| 8 | Giancarlo Fisichella | 4 |
| 9 | Pedro Diniz | 5 |
| 10 | Jacques Villeneuve | 10 |
| 11 | Jarno Trulli | 8 |
| 12 | Alexander Wurz | 4 |
| 13 | Johnny Herbert | 7 |
| 14 | Heinz-Harald Frentzen | 3 |
| 15 | Olivier Panis | 8 |
| 16 | Jean Alesi | 5 |
| 17 | Luca Badoer | 9 |
| 18 | Toranosuke Takagi | 6 |
| 19 | Ricardo Zonta | 10 |
| 20 | Pedro De La Rosa | 6 |
| 21 | Marc Gene | 9 |

## 2. The organic pit call — TWO INT WRITES, NO RETIREMENT

Proven live (pace_probe HANDOFF_2026-08-03c, re-verified here on Hakkinen + Gene):

```
veh+0x4590 = 0   (scheduled lap of current stop, 0 = now)
veh+0x413C = 0   (re-arm the one-shot decision timer; game writes FLT_MAX after each eval)
```

- `0x413C` is a FLOAT compared against SessionTime by AIMainUpdate (`AIPushLap` fires
  when SessionTime > veh+0x413C).
- Result next sample: `pit=1, callToBox=1` (0x454C) — the game's own call-in.
- NO `veh+0xD58=0`. That is the RETIREMENT arming (pace_probe.c:4528: "that arming
  is what turns a pit call into a RETIREMENT"). Using it on a real driver retires him
  (Coulthard incident, this session: finish=2, aictrl=0, parked at (-2.2,-16.5);
  revived with the sc_revive recipe, fields held but car static — engine crank
  (ANGVEL/RPM=1500) was the missing final step).
- Track the car's laneMode: it flips 0→1 by itself at the pit entry (measured
  wp 0x364A8B38 / pos (-214, 38) for Gene's run at Spa).

## 3. The full organic stop — what the game does (Spa, live)

Hakkinen (slot 1, team 0), complete trace at 400 ms/sample:

```
pit=1  lane=0  in-lap, full circuit at ~78 m/s
pit=1  lane=1  laneMode flips at entry approach
pit=2  lane=1  crossed the entry line (wp 0x364AB2B8, pos (-59,-3)); limiter 22.2 m/s
pit=2  lane=1  crawls the lane at 22 m/s
pit=3  lane=1  stopped in box (wp 0x364ABF10, pos (-7,-12)) — service
pit=4  lane=1  box exit, rolls out at limiter, accelerating
pit=0  lane=1  merged onto track, out-lap
```

No retirement, no damage, `retired=0 finish=0` end to end.

## 4. The lane waypoint chain (Spa, live)

Node struct stride 0x13C (316). pos at node+0 (x), +8 (z). Lane nodes are the
branch-1 records, in order:

```
0x364A5B14 (-374, 148)   laneMode=1 starts
0x364A8B38 (-214,  38)
0x364A9DBC (-144,   5)
0x364AA79C (-103,  -2)
0x364AB2B8 ( -59,  -3)   pit=2 entry line
0x364ABF10 (  -7, -12)   team 0 box (Hakkinen)
0x364AE1A0 ( 133, -12)   team 9 box (Gene)
0x364AEA44 ( 171,  -7)   team 11 box node (SC) — 9.7 m ahead of SC box
0x364AE908→0x364AEDF8   lane continues past SC box (166→184, -6→-14)
0x364AC04C→...           pit-exit chain, pit=4, accelerating 22→47.8
```

## 5. THE AIW FILE LITERALLY DOCUMENTS ITS OWN LANE ENCODING

`1999_Spa_Francorchamps.AIW` (465,223 bytes, INI-style text, 1419 waypoints):

```
[Features]
pitlanes=1
...
[Waypoint]
number_waypoints=1419
...
wp_pos=(-15.2200,-21.5790,11.6612)
wp_perp=(-0.0063,-0.0008,-1.0000)
wp_width=(2.350,10.386,5.770,10.386)  // road left, road right, far left, far right
wp_score=(2,6889.476)                 // sector, lap distance
wp_branchID=(0) // 0=main path, 1=pitlane, 2-???=pitstops for each slot
WP_PTRS=(1225,1,-1,0)
```

Branch census (1999 Spa):
- branch 0: 1226 records = main racing line
- branch 1: 157 records = THE PIT LANE, entry (-381.8,152.5) → exit (52.6,-116.1),
  each with lap distance (6469.7 → 373.5 crossing the s/f line)
- branches 2-25: 24 records = pitstop stalls (2 per box)
- branches 106-117: 12 records = GARAGE/BOX positions, indexed 106 + teamIndex

Live cross-check of the 106+team rule:
- Hakkinen team 0 → branch 106 = (-4.8,-12.7); he stopped at (-7,-12) ✓
- Gene team 9 → branch 115 = (136.6,-12.5); he stopped at (133,-12) ✓
- SC team 11 → branch 117 = (167.9,-12.6) ✓ matches the live lane node 9.7 m ahead
  of his box.

## 6. The Spa deploy fix (design, not yet implemented)

At deploy, hand the SC:
- curWaypoint (0x4354) = the branch-1 lane node just AHEAD of his box, resolved
  from the AIW: branch 106 + teamIndex is the box; the lane node to drive is the
  branch-1 record nearest ~9-10 m past the box position.
- laneMode (0x4550) = 1, 0x4548 = 1, held every tick while pittingStatus < 2
  (the SC never crosses the entry trigger, so pittingStatus stays 0 — the earlier
  g_deploy_inlane gate on pit>=2 never fired; that was the Spa derp root cause).
- Release to laneMode 0 when he merges (detect via speed > ~40 m/s or by the
  game's own laneMode clear at the pit exit).

## 7. NEXT — cross-track validation

Prove the branch-106 rule on a second track (e.g. Monza 1999 or another):
1. Parse that track's .aiw: confirm [Features] pitlanes=1, count branch-1 records,
   confirm branches 106..(106+teams-1) exist and sit on the lane.
2. Call a real driver in organically (0x4590=0 + 0x413C=0), trace to box, compare
   parked pos vs branch 106+team record — must match within ~1 m.
3. Read the SC's live lane node ahead of the box; verify it's the branch-1 record
   nearest boxPos+10 m along the lane.
4. Only then implement the DLL lookup (no per-track constants).
