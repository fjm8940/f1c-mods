# SESSION 2026-08-04/05: ORGANIC PIT CALL + AIW PATHFINDING — FULL RECORD

Track: 1999_Spa_Francorchamps (and 1999_Hockenheim for cross-checks). Game PID
22164, Frida session, native_sc_ntmode.dll injected. Fork branch:
feat/native-sc-ntmode.

## OBJECTIVE (user spec, verbatim)
1. SC teleported to his pit box at race start — FOR IMMERSION ONLY. Physics and
   gameplay disabled (role -1). AESTHETICS ONLY.
2. 15 s after race start: SC teleported to the pitlane WP/XPITIN location,
   AI enabled, forced pit stop.
3. SC drives into his pitbox, doing an ORGANIC pitstop.
4. ORIENTATION MUST FACE THE PIT LANE ENTRY when teleported — otherwise it bugs.

## THE BIG DISCOVERY: the AIW file is the complete pit pathfinding graph

`wp_branchID=(N) // 0=main path, 1=pitlane, 2-???=pitstops for each slot` —
THE COMMENT IS IN THE FILE. Every waypoint record carries:

```
wp_pos=(x,y,z)           exact 3D coordinate (Y = track ground height, verified ±0.3 m)
wp_perp=...              perpendicular
wp_width=...             road widths
wp_score=(sector,dist)   lap distance — "how far along the lane"
wp_branchID=(N)          lane membership: 0 main, 1 pitlane, 2-25 stalls, 106-117 boxes
WP_PTRS=(prev,next,off,branch)  ← THE ROUTE LINKS
```

**WP_PTRS = (prev, next, branch-off, branch):**
- prev = previous waypoint in flow
- next = next waypoint in flow (the successor chain the AI walks)
- off = the branch-off link: for a LANE node, the box you can turn into; for a
  BOX node, the lane node you exit onto
- branch = the branch ID

**The AI pathfinds by:** nearest-node query (laneMode-filtered) → walk the
WP_PTRS successor chain → at the box's turn-in lane node, take off into the box
→ on release, take the box's off back onto the lane.

**Pit entry (branch-0 approach node):** the FIRST branch-1 record's `prev`
link = the branch-0 (main path) node the lane connects FROM. Verified:
- Spa: lane[0]=1226, prev=1148 (main), at (-385.24, -36.25, 156.25)
- Hockenheim: lane[0]=1225, prev=1149 (main), at (266.7, 158.4)

**Pit boxes = branch 106 + AIW teamIndex** (WP_BOXBASE=106). Verified live:
- Spa team 0 (Hakkinen) → branch 106 (-4.8, -12.7); he stopped at (-7, -12) ✓
- Spa team 9 (Gene) → branch 115 (136.6, -12.5); he stopped at (133, -12) ✓
- Spa team 11 (SC) → branch 117 (167.9, -12.6) ✓
- Hockenheim team 1 (Schumacher) → branch 107 (-48.9, 21.3); parked (-47, 21) ✓

**In-memory waypoint array:** BYTE-EXACT mirror of file records, file order,
stride 0x13C (316). Base derivable live: any driving car's curWp (0x4354)
minus its record index × 0x13C.

**Lane exit/merge:** the LAST branch-1 record's `next` = a branch-0 node = the
merge point (Spa: lane[1382] next=90, main). The game itself flips laneMode
1→0 at the merge (measured live).

## THE ORGANIC PIT CALL — the working mechanism (2 bugs fixed this session)

**The trigger (proven):** write ALL FOUR schedule slots + re-arm timer:
```
for i in 0..3: veh+0x4590 + i*16 = -1     (schedule stop lap; stride-16 array)
veh+0x413C = 0                            (one-shot eval timer; game writes FLT_MAX after eval)
```
Result next sample: `pit=1, callToBox=1` — the game's own call-in.

**BUG 1 (fixed): the schedule is a stride-16 array and the game evaluates the
CURRENT stop's slot.** `aiPitStrategyIndex = veh+0x342C` (measured 3 on the
1999 field). The game reads `0x4590 + stratIdx*16` = 0x45C0, NOT 0x4590.
Writing only 0x4590 did NOTHING (0x45C0 still held lap 44 → no call, timer
consumed, silent). Writing all 4 slots fixes it. Live proof: Hakkinen
callBox 0→1 one sample after all-4 write.

**BUG 2 (fixed): the eval is `scheduled_lap <= lapNumber`, and the SC's lap
is exiled to last-1 → at race start that is -1.** Writing scheduled=0 gives
`0 <= -1` = FALSE → no call. Schedule at **-1**: always ≤ any lap, always
fires.

**CRITICAL RULE (the smoke incident): NEVER arm the pit call on a dormant
car.** The evaluation runs even on role -1 cars and puts the AI in a broken
state (observed: engine smoke). pace_probe's rule: "DO NOT ARM HIM UNTIL HE
IS ACTUALLY ROLLING" (pace_probe.c:4550 `if (speed < 3.0f) return`).

## THE CURRENT BUG (unsolved, next session's job)

**Symptom:** after SEND-IN pass 2 (teleport to entry), "he took off
instantly" — the AI was relieved (role 1) but should NOT be. And the pit
order "did not become visible".

**Root cause (analysis):** pass 1 sets role 1 + rebuild BEFORE the teleport.
So at pass 2 the AI is ALREADY driving him — he takes off from the entry
before the order registers. The whole sequence must be INVERTED:

**THE FIX (next session):**
1. RACE_PHASE 4: box teleport (aesthetic, dormant) — WORKS, keep
2. 15 s after green: teleport to entry **WHILE STILL DORMANT (role -1)** +
   register the pit order (4 slots = -1) + timer DISARMED (FLT_MAX)
3. Wait ~1 s (order-registration grace)
4. THEN role 1 + rebuild + arm timer (0) → the order is already on the car,
   the AI routes him in immediately.

**CAUTION (the re-pin concern):** the corpus warned "teleport-before-role
does not hold" (the game re-pins a dormant car's position during the START
SEQUENCE / grid hold). That was during RACE_PHASE 4 with lights. At 15 s
after green (RACE_PHASE 5, cars already rolling) the re-pin may not apply —
TEST IT. If the dormant teleport re-pins: alternative = keep role 1 from
pass 1 but pin maxSpeed=0 (crawl ~5 m/s) until the order is registered, then
release. (maxSpeed=0 does NOT fully hold a car — it floors at ~5 m/s — but
that may be enough for a 1 s window; the other lever is per-tick position
pin, which full_safety_car used for holds.)

## EXILE MECHANICS (all working, live-verified)

- `sc_exile_pos()`: SC's 0x4058 + 0x4060 pinned UNCONDITIONALLY to
  max(field grid/race pos) + 1 every tick (22 entrants → 23rd). Written in
  both the sort hook (0x4BF390 detour) and the keeper (sc_probe).
- `sc_exile_lap()`: SC's 0x403C pinned to max(field lap) − 1 (always 1 lap
  behind the last driver; at race start = -1).
- Both recomputed every tick from the live field; follow retirements.

## KEY OFFSETS (all FACT)
```
veh+0x4620   ASCII driver name (resolve slots by name, array reindexes!)
veh+0x32C8   role: -1 dormant, 0 player, 1 AI
veh+0x4354   curWaypoint pointer (into the 0x13C-stride waypoint array)
veh+0x4550   laneMode: 0 racing line, 1 pit-lane set
veh+0x4548   pit-lane route flag
veh+0x3450   pittingStatus: 0 track, 1 in-lap, 2 lane, 3 box, 4 parked
veh+0x454C   callToBox
veh+0x342C   aiPitStrategyIndex (measured 3; selects the schedule slot!)
veh+0x4590+16i  pit schedule slots (0x4590/0x45A0/0x45B0/0x45C0)
veh+0x413C   one-shot pit eval timer (float vs SessionTime; FLT_MAX = disarmed)
veh+0x3470   stopEndTime (release when SessionTime passes it; = -1 releases)
veh+0x34E0/4/8  authoritative position (Y = world)
veh+0x34BC..E0 3x3 heading matrix (M00 cos, M02 sin, M20 -sin, M22 cos)
veh+0x34F0   DERIVED yaw mirror — never write it
veh+0x403C   lap number (sort key 1)
veh+0x4044   lap distance (sort key 2)
veh+0x4058   plannedPositionAtStart (grid)
veh+0x4060   race position
veh+0x4538   maxSpeed (governor; pit limiter = 22.2 = flt_8ACFB4)
veh+0x44CC   PassingNorm (0.9 = never overtake)
veh+0x44F8   TrackAggression (NOT the overtake knob; following gap)
veh+0x48A4   AIW [PITS] TeamIndex (SC = 11)
veh+0xD58    retirement arming (0 + callToBox=1 = RETIREMENT — never for a call)
0x5EC6D8     vehicle array base, stride 0x48C0, 104 slots
0x7C56A8     race phase (4 start seq, 5 racing)
0x7C56A4     session type (5 quali, 7 race)
0x8ACEE0     Steward; +0x194 = XPITIN trigger obj, +0x198 XPITOUT
```

## FILES
- DLL source: patches/native_safety_car_ntmode/dll/native_sc.c
- Build: dll/build_native_sc.bat (vcvars broken, manual INCLUDE/LIB)
- Deployed: F1 Challenge V10/native_sc_ntmode.dll + native_sc_log.txt + native_sc.ini
- Session doc: patches/native_safety_car_ntmode/SESSION_2026-08-04_spa_lane.md
- Parent (proven SC): patches/full_safety_car/ (pace_probe.c = the oracle;
  its sc_revive/sc_ai_calm/hold recipes are all proven)
- NT2003/NT2004 decompiled: F1C_ghidra_project/source_material/

## NEXT SESSION — ONE JOB
Invert the SEND-IN sequence (dormant teleport + order first, driving second)
per the fix above. Test the dormant-teleport re-pin question at Spa. Then
cross-track at Hockenheim. Commit when green.

## ADDENDUM 2026-08-05: THE PIT FLIP (0->2) POINT — LIVE-ANCHORED ON TWO TRACKS

Method: dense player trace (80 kHz sampling) while crossing the pit entry.
Each flip is a plane crossing at near-zero speed (0.04 m / 0.001 m crawl
between samples) - the XPITIN trigger is a PLANE test, not a zone.

LIVE FLIP FRAMES:
  SPA    pit 0->2 @ (-67.46654, -23.15665, -2.37721)  -> lane wp 1297 (0.67 m), dist 6829.1
  MONZA  pit 0->2 @ ( 0.88753,  -0.70116, 261.85696)  -> lane wp 1086 (1.74 m), dist 5453.0

STRUCTURAL PATTERN (both tracks, from .aiw alone):
  - the flip waypoint is a branch-1 (pitlane) record
  - it sits where the LANE runs PARALLEL to the MAIN path at the same lap
    distance (Spa: lane 6829.1 vs main 6828.6; Monza: 5453.0 vs 5451.7)
  - it is the lane record nearest the START/FINISH straight region
  - after it, the lane chain has no more branch-off links (off=-1)
  - index differs per track (1297 vs 1086) - NEVER hardcode; resolve from
    the .aiw: the lane record whose dist is nearest the main path's
    parallel segment at the s/f straight.

TELEPORT PLAN (user spec): place AI drivers at the flip waypoint position
(or 1-2 m before it on approach), AI on, and pit 0->2 fires naturally as
they cross the plane. NO teleport-to-pitbox needed for the organic entry.

TOOLS:
  tools/trace_pit_flip.py      - dense flip capture (player slot 0)
  tools/sc_teleport_gui.py     - coordinate manipulator (live AI on/off,
                                 nudge X/Y/Z, yaw)
  sc_teleport_gui.bat          - launcher (game folder)

GUI LESSONS:
  - veh+0x34E0 (auth), 0x1C (render mirror), 0x744 (phys mirror) - write
    ALL THREE or the render/phys disagree
  - frozen (role -1) cars hold any position; LIVE cars need small deltas
    (large live teleports kill ground collision -> free-fall, Y drops to
    -1300; measured 4 cars). Small +1 X deltas on a live car are safe.
  - yaw bearing: forward = (cos, sin) in X-Z; yaw = atan2(dz, dx) (NOT
    atan2(dx,dz), NOT +pi - both wrong, car faces the wall / drives away)
  - AIW node Y is NOT always the ground; use a slightly-high Y and let
    physics settle, or read the player's Y nearby
