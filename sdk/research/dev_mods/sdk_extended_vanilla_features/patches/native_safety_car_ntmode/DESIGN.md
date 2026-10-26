# Native Safety Car — spawn/despawn framework (NEW, isolated from `full_safety_car/`)

## STATUS (2026-08-04, late) — READ `SESSION_2026-08-04_native_add.md` FIRST

That file is the factual record of the build session: what the DLL does now,
the four crashes and their proven causes, the best live result, and the damage
done to the safety car's own folder and how it was repaired.

Short version:
- **The native spawn WORKS** (run 21, owner-observed). The DLL adds the SC as an
  extra car at track load; the game's own passes mesh it, build its crew and
  tyre record; it parks dormant; a session change is survived; the player keeps
  grid position 1; `LOG/trace.txt` is clean.
- **The AI pool no longer steals a driver's seat.** The safety car's `.veh` now
  carries `Classes="Safety, DSGermany, 1999, NotAI"`, matching the SHIPPED
  originals (F1 2002 `Safgpxa.veh` = `"Safety, DSGermany, 2002, NotAI"`, NT2003
  `pacecar.veh` = `"PaceCar, ClosedWheel, 2003, NotAI"`). F1C honours the token:
  `AIOpponentSelection` (0x4BF790) builds `"%s NOT: NotAI"` and is called by
  `AIDriverCountSetup`. Live result: Bernd gone from the standings, Ricardo
  Zonta back in the field. **This breaks `full_safety_car`, which needs him as a
  grid entrant** — backup `1999_fia29.veh.bak_before_notai`.
- **Pit-box placement works** (owner-confirmed): position at
  `veh+0x34E0/34E4/34E8` + the 3x3 heading matrix at `veh+0x34BC..0x34E0`, box
  from `aiw_pits.json` via `tools/native_sc_pitbox.py`. **But a one-shot
  placement is undone by the start sequence** — he still owns a grid slot, so he
  is back on the grid at the lights. Fix = `full_safety_car`'s hand-over
  (`RACE_PHASE 4`, then `callToBox=1` + `pittingStatus=2` + `laneMode=1` +
  `curWaypoint=0` ONCE), not more teleporting. **IMPLEMENTED 2026-08-04** in
  `native_sc.c` as `sc_pit_assign()` (worker, gated on `SESSION_TYPE==7`,
  `RACE_PHASE==4`): re-arm role 1 + rebuild → box teleport → one-shot four-flag
  hand-over. Untested — needs a live race.
- **The extra entrant is STRUCTURAL, not a bug** (§17, read from NT2003's own
  code): NT keeps its pace cars in a **separate array** (`off_5BD508`, stride
  16672, own count `dword_5BD504`, own pointer table) so they are never counted,
  scored or grid-placed. F1C has no second list — a car must be counted to be
  built at all — so ours is listed. The only count lever, `drop_count`,
  **crashes on session change** (0x54A7ED through `VehgenScnFileLoader`, A/B
  proven) and is now default OFF. Removing him from the classification needs the
  UI row builder hooked.
- **Still open: deploy/recall (Numpad1/Numpad2) has never been pressed.**
- Root cause of the long crash run: `veh+0x32C8 = -1` during the load makes
  `LargeConfigFunctionWithPhysicsConstants` (0x4E45B0) early-return, skipping the
  `.hdv` parse and `SuspensionAndWheelPhysics` — the sole writer of the tyre
  record `veh+0x3978` — while `AIDriverStructInit` reads it with no null check.
  Carry the car at role 1 for the whole load; park after the render pass.
- The DLL must NOT call `VehgenScnFileLoader` or `SceneRenderUpdate` itself at
  `AIDriverCountSetup` time: the scene is empty there (probe: 0 `SLOTnnn` nodes)
  and the camera manager is not ready.
- **The native add has never been tested against a complete car folder** — every
  attempt ran while `RIM.BMP` was missing from
  `SeasonData\Vehicles\FIA\1999_CLK55\`, which is what produced the final
  `0x55D852` crash. `native_sc.ini` is currently `add=0`.

## STATUS (2026-08-04, earlier) — superseded by the block above

**Goal:** spawn the safety car as a pure DLL + car-files mod (no gdb/entry edit, no
grid slot consumed), instead of the current grid-entrant + runtime-VehicleCount-hack
version in `full_safety_car/` (which is not distributable and crashed on session
change).

**Proven and reusable (live):**
- Deploy/recall = write `veh+0x32C8` (−1 out / 1 in) + call the game's own
  `BuildActiveVehiclePointerArray` (0x42BC80). Clean, reversible, no crash, no
  VehicleCount change. `AIMainUpdate` self-gates on `0x32C8`.
- The roster `dword_8AC394` is the FULL 271-car SeasonData DB; the SC is index 262
  by its `.VEH` path — lookup-able without being a race entry. Ship the DLL + the
  `SeasonData\Vehicles\FIA\1999_CLK55\` folder.

**RETRACTED (2026-08-04, static, §5c): there is NO "spare `SLOTnnn` node" wall.**
The `SLOTnnn` node is not track data and not per-entry — **each car creates its
own**. `VehgenScnFileLoader` regenerates `vehgen.scn` from the car's own `.gen`
template (`Graphics=` in the `.veh`), substituting `<ID>` with `veh+0x4894`, and
`VehicleGraphicsSceneLoader` parses that file into the global scene
(`dword_8AFE88`) and then looks the node up **in that same scene**. So any
teamId works and there is no grid-capacity ceiling. §5b's ceiling claim is void,
and `scene_slot_probe.js` is no longer needed.

**The real (single) constraint is CONTEXT:** the scn parse + node creation only
succeed during track load. That, alone, is why every mid-race build crashed at
`SceneRenderUpdate` (null node at `+0x10`).

**The design (B), now implementable:** add the SC as ONE EXTRA CAR *inside* the
game's own load-time field build, and let the game's own passes mesh it.
`DynMan_Init` (0x42C1A0, runs per track load) does
`AIDriverCountSetup` (identities) → `for i < VehicleCount: VehgenScnFileLoader(veh_i,1)`
(meshes, and builds the crew) → then `DynMan_Restart` runs `SceneRenderUpdate` for
every counted car. So the hook is the **tail of `AIDriverCountSetup` (0x4BFD90)**,
where we only do the identity half for one extra slot and `++VehicleCount` +
`BuildActiveVehiclePointerArray()`. Everything after is the game's own code.
Park dormant (`0x32C8 = −1` + rebuild) once the mesh exists, deploy with the
proven toggle.

**Feasibility:** the spawn half is DONE and proven live (run 21) — see the STATUS
block at the top of this file and `SESSION_2026-08-04_native_add.md`. It ended up
being TWO hooks, not one: `AIDriverCountSetup` for the add, and `DynMan_Restart`
for the role re-arm and the park. Inject at the main menu, before a track load.
The deploy/recall toggle is proven from the 2026-08-04 Frida work but has never
been exercised through this DLL. See `findings.jsonl` (`native-sc-*`).

---


> **Why this exists.** `full_safety_car/` deploys the SC as a **grid entrant**, then
> at runtime lifts its pointer out of the active array `0x7C54F0` and **decrements
> `VehicleCount 0x5C329C`**. That count write is the crash source (held across a
> session change crashes the loader; count is published before cars are built),
> it consumes a grid slot, and the car gets box-placed. Not distributable, not
> reliable.
>
> This folder is a clean re-implementation using **the game's own field builder**
> to place the SC in a slot ABOVE the grid count, toggled by the game's own
> active-array rebuild. `VehicleCount` is never touched. Nothing here is wired
> into `full_safety_car/`; that version stays as-is.

Source of the framework: NASCAR Thunder 2003/2004 (Image Space gMotor, same
lineage as F1C). NT keeps a pace car as a normal roster vehicle in a slot above
the field and toggles a per-car existence bitfield. F1C's dynman is **literally
NT's dynman with the pace-car pass compiled out** — same `"Entered DynMan"`
debug string, same `d:\dev\F1 1999-2002\` source path (`DynMan_Init` sub_42C1A0).
Every primitive that pass needs still exists in F1C. This is the port of that
one removed pass.

---

## 1. The NT reference (extraction-tier)

NT2003 `sub_4B93A0` / NT2004 `sub_4E29B0` — the pace-car build pass, called
from dynman after the field is built:

```
for i in 0..paceCarListCount:
    slotAssign(2 or 1, 0)                      # garage/pit assign
    if i == 0:                                 # the pace car
        scan driver roster for _strcmpi(name, "pace car")   # NT03 stride 1128 @+147
                                                            # NT04 stride 1232 @+246
        loadRosterEntryIntoVehicle(matchIdx)
    else:
        loadRosterEntryIntoVehicle(randomDriver)
    setSlot(i + BASE)                          # NT03 BASE=10, NT04 BASE=48 (ABOVE the field)
```

Existence in NT = one dword per car, `car[6324]` (offset 0x62D0):
`bit0` on-track, `bit1(&2)` **rendered** (the render dispatch `sub_525D30`
early-outs on `&2 == 0`), `|6` visible+LOD, `|8` audio/collision. NT renders
every slot every frame and each car self-gates on this bit.

---

## 2. The F1C mapping (all functions identified, extraction-tier)

| NT pace-car pass step | F1C function / offset | notes |
|---|---|---|
| dynman host `sub_42E180` | `DynMan_Init` **sub_42C1A0** | same code, pace pass removed |
| field builder `sub_4E29B0` sibling | `AIDriverCountSetup` **sub_4BFD90** | builds `unk_5EC6D8 + 18624*N`, calls the per-car builders below |
| roster scan by name | `VehicleLookupByName` **sub_4C3F80** | driver roster `dword_8AC394`, **stride 1060**, name via `sub_588410`; writes match index to `dword_8AC38C` |
| load roster entry → slot | `NationalityDriverInfoLoader(idx)` + `TeamIDSetter(slot)` | the two per-car build calls seen in sub_4BFD90 |
| per-car state advance | `SessionStateTransition(1, 0)` | seen in sub_4BFD90's build loop |
| slot-N assign | write vehicle fields at `unk_5EC6D8 + 18624*N` | base **0x5EC6D8**, stride **0x48C0** (=18624) |
| existence bit `car[6324] & 2` | **`veh+0x32C8`** | −1 = out of field, slot index = in. This is F1C's live flag (FACT) |
| existence rebuild | `BuildActiveVehiclePointerArray` **sub_42BC80** | game's own; scans every slot, adds those with `0x32C8 != −1` to `dword_7C54F0`, fills `[N..104)` with the inactive ones, returns active count |

Key difference from NT that makes F1C CLEANER: F1C does not render every slot and
self-gate — it renders only what is in the active pointer array `0x7C54F0`, and
that array is rebuilt from `0x32C8` by `sub_42BC80`. So the existence toggle is a
single int write plus one call to a function the game already owns. No per-frame
bit maintenance.

Capacity: 104 slots, grid ≤ 24 → slots 24..103 are free for the SC.

---

## 3. The framework

**Build (once, at load — hook the tail of `AIDriverCountSetup` sub_4BFD90 or dynman):**
1. `N = VehicleCount` (the first free slot above the grid).
2. `VehicleLookupByName("Bernd Maylander")`  → sets `dword_8AC38C` to his roster index.
3. `SessionStateTransition(1, 0)` → `NationalityDriverInfoLoader(dword_8AC38C)` → `TeamIDSetter(N)`.
4. Write `slotN + 0x32C8 = −1` (dormant — out of the field, uncounted, not placed).
5. **`VehicleCount` is left untouched.**

**Deploy:** `slotN + 0x32C8 = 1`; call `BuildActiveVehiclePointerArray()`.
Game renders + sims him. **VehicleCount untouched.**

**Recall/despawn:** `slotN + 0x32C8 = −1`; call `BuildActiveVehiclePointerArray()`.
Clean removal. **VehicleCount untouched.**

### FINAL (2026-08-04) — VehicleCount is NEVER changed at runtime. The sim self-gates.

An intermediate correction claimed the toggle had to pair a VehicleCount change,
reasoning that `MainGameTickUpdate` (0x42C690) bounds every per-frame loop at
`dword_5C329C`. That was **half-right and led to a harmful design.** The loop IS
count-bounded, BUT the per-car work self-gates on `0x32C8`:

```c
// AIMainUpdate (0x424E70), first statement:
if ( *(int *)(this + 0x32C8) > 1u )   // -1 == 0xFFFFFFFF > 1  -> inactive
    return;                            //   ... skipped: no AI, no physics
```

So a **counted-but-inactive** car (within `[0..VehicleCount)` yet `0x32C8 == -1`)
is a no-op. The count bound does not need to move. This was already proven
empirically by V2's FIRST test: VehicleCount left at 23, only Maylander's
`0x32C8` flipped to -1 + rebuild → he vanished from the standings, no crash, and
(critically) **the grid was NOT re-sorted**.

The later paired-count test (VehicleCount 23↔22) is what ran a scoring pass and
bumped the player to grid 23 — `sub_42BC80` itself never writes `0x4058`/`0x4060`;
the VehicleCount write did. So:

**Rule: VehicleCount is fixed for the whole session. Deploy/recall = `0x32C8`
flip + `sub_42BC80()` and nothing else.** No count manipulation → no grid
re-sort → no loader-crash source. This is strictly simpler and safer than
`full_safety_car/`'s runtime decrement, and it is the distribution-safe design.

The SC still lives in a fixed slot that is counted for the whole session (so
VehicleCount = grid + 1 from load), and is kept dormant via `0x32C8 = -1` +
rebuild whenever it is not deployed.

Pacing / caution laws / commentary once he is on track are UNCHANGED from
`full_safety_car/pace_probe.c` — this folder only replaces spawn/despawn.

---

## 4. Live verification

### [V2] existence toggle — **PROVEN** (2026-08-04, Frida, 1999 race, 23-car field)

`0x32C8` flip + `BuildActiveVehiclePointerArray()` (0x42BC80, no args, returns
active count) cleanly adds/removes a car. Tested on Bernd Maylander at slot 6:

- `0x32C8` **latches** when written (`-1` held; the game did not rewrite it).
- Flag flip **alone** does NOT remove from the active array — the rebuild call
  is required (slot stayed in `0x7C54F0` until `sub_42BC80` ran).
- `0x32C8 = -1` then `rebuild()` → return **22** (was 23), slot 6 recompacted
  into the inactive tail `[activeCount..104)` exactly as the function designs.
  **Driver-confirmed: gone from the standings.** No crash.
- `0x32C8 = 1` then `rebuild()` → return **23**, slot 6 back at array index 6.
  Fully reversible.
- Confirmed live: base **0x5EC6D8**, stride **0x48C0**, active flag **0x32C8**
  (value = role: 0 player / 1 AI / -1 inactive), active array **0x7C54F0**
  (entries map to exact integer slots via `(ptr-base)/stride`).
- Note: an anomalous `ret=1` appeared once — it coincided with an in-game
  session restart tearing the field down between two calls, NOT a failure of the
  mechanism. Re-run in a stable session was clean both ways.

So deploy/recall of an already-built SC slot is solved and safe. `sub_42BC80` is
callable from a non-game thread (Frida) without crashing — a leaf pointer
rebuild, no alloc/callbacks.

### [V1a] paired deploy/recall transaction — **PROVEN** (2026-08-04, Frida)

The runtime half of V1 — the `{0x32C8, VehicleCount±1, rebuild()}` paired
transaction — is proven on the already-built Maylander (slot 6):

- Recall: `0x32C8=-1`, `VehicleCount 23→22`, `rebuild()` → **ret 22**, no crash.
- Restore: `0x32C8=1`, `VehicleCount 22→23`, `rebuild()` → **ret 23**, slot 6
  back at array index 6, no crash. Driver-confirmed field back to normal.

So deploy/recall of a counted car is solved and crash-safe. What remains is only
the LOAD-time build of a fresh car above the grid.

**HAZARD found in the same test — VehicleCount change re-sorts the grid.** Running
the paired transaction AT THE PRE-RACE GRID (RacePhase 0) re-sorted scoring and
bumped the uncounted car (the player) to LAST: player `0x4058`/`0x4060` both went
to 23. The vehicle array stayed clean; only the position fields shifted, and they
did NOT self-restore because pre-race grid positions are latched (`0x4060` is only
live-recalculated from track distance DURING racing). Recovery = race restart.
Implication for the DLL: run the paired transaction **only during racing** (where
positions self-heal from track distance), or explicitly preserve/repair the
counted car's `0x4058`/`0x4060`. The SC deploys mid-race anyway, so the racing-only
rule likely suffices — but verify the deploy does not perturb the leader/player
grid fields.

### [V3] live toggle during RACING — done (2026-08-04), and it reframed the goal

Toggled the already-built Maylander (slot 6) live at RacePhase 5:
- `0x32C8 = -1`, VehicleCount left 23 → **still on track, frozen, still P22 in
  standings** (AIMainUpdate self-gates so physics stops, but he is inside the
  count bound so he still renders + lists).
- then VehicleCount 23→22 + rebuild → **dropped from standings + sim (22
  entrants), but the MODEL stays on track, immovable.**

So neither `0x32C8` nor VehicleCount hides the car MESH — that is drawn by the
scene graph, independent of both (see finding
`native-sc-model-render-independent-of-count-and-32c8`). **But model-hiding is
NOT the goal:** `full_safety_car` never hides the model either — the SC simply
parks in its pit box until deployed. The real goal of native spawn is:

1. **No SeasonData/gdb entry** — ship as a pure DLL, no season/track data edits.
2. **No grid slot consumed** — the SC must not be an entry-list car that starts
   on the grid and alters the race.

That is why the chosen design is **runtime construction**: build the SC into a
free slot (23+) at runtime via the game's own per-slot builders, so it is never
in the entry list and never on the grid; park it, then deploy/recall with the
proven `0x32C8`(+optional count)+`sub_42BC80` toggle.

---

## 5. The runtime construction recipe (design = "runtime construction")

Traced from `DynMan_Init` (0x42C1A0), the game's own per-car build loop
(lines 65-72): after identity is built and the array rebuilt, it loops
`v6 < VehicleCount` calling `VehgenScnFileLoader(this=vehicle, 1)` per car.
So one car is built by this ordered sequence, all `__thiscall this = &veh[N]`
(N-th slot = `0x5EC6D8 + 0x48C0*N`):

| # | call | offset | effect |
|---|---|---|---|
| 1 | `VehicleLookupByName(".VEH path")` **[PROVEN live 2026-08-04]** | 0x4C3F80 | __stdcall, arg = `char*` .VEH path. Scans the FULL vehicle DB `dword_8AC394` (count `dword_8AC390` = **271**, stride 1060, name = `char*` at entry+0 = the `.VEH` path). Match index → `dword_8AC38C`. SC = index **262** = `SEASONDATA\VEHICLES\FIA\1999_CLK55\1999_FIA29.VEH`. Live: set `dword_8AC38C` 57→262, no crash. **The roster is the whole SeasonData DB, so the SC needs only its files on disk — NO race entry.** |
| 2 | `NationalityDriverInfoLoader(vehN, driverIdx)` | 0x4C78F0 | writes driver name/nationality into vehN; `vehN+18584 = driverIdx`. |
| 3 | `TeamIDSetter(vehN, teamId)` | 0x4C6960 | `vehN+18580 (0x4894) = teamId` (used as the scene TEAMDIR key), `vehN+0x32D0 = teamId` (slot index), + 3 more. |
| 4 | `SessionStateTransition(vehN, 0, 0)` | 0x4EF5A0 | pre-build state (arg 0). |
| 5 | `++VehicleCount (dword_5C329C)` | 0x5C329C | count the new car (the game does this itself in the add path). |
| 6 | `BuildActiveVehiclePointerArray()` | 0x42BC80 | re-pack the active array with the new car in it. |
| 7 | `VehgenScnFileLoader(vehN, 1)` | 0x4C6C40 | THE mesh/scene load: builds `.scn` path from `vehN+18580`, calls `VehicleGraphicsSceneLoader` (0x4DFCE0), sets driver number. |
| 8 | `SceneRenderUpdate(vehN, 1)` | 0x4C6D90 | render-instance setup for the new mesh. |
| 9 | `SessionStateTransition(vehN, 3, 0)` | 0x4EF5A0 | activate to state 3 (the built/running entry state). |
| 10 | crew (optional, for the pit stop): `sub_4E2A60(vehN)` → `PitCrewInstanceSetup` (0x4DDAD0) | 0x4E2A60 | 34 crew actors at `vehN+0x32D4` (0 on an unbuilt slot). |
| 11 | park dormant until deploy: `vehN+0x32C8 = -1` + `sub_42BC80()`; on deploy set role 1 + rebuild. | | |

**This is the game's own runtime add-a-car sequence**, lifted from
`ProcessDriverEntries` (0x4B8DA0) lines 124-129, which builds each entry with
exactly: `SessionStateTransition(0,0)` → `++dword_5C329C` →
`BuildActiveVehiclePointerArray` → `VehgenScnFileLoader(1)` → `SceneRenderUpdate(1)`
→ `SessionStateTransition(3,0)`. Steps 1-3 (identity via the roster index) are
the DynMan variant (`AIDriverCountSetup`) that lets us target the SC by `.VEH`
path without an entry-list node. All calls `__thiscall this = &veh[N]`, except
step 1 (`__stdcall`) and steps 5/6 (globals/no-arg).

Physics/AI child objects: slots 23-103 already carry a base sub-object
(non-null pointers at `+0xC`/`+0x32CC`/`+0x6F0`, distinct per slot — measured
live), so the shell exists; the steps fill driver identity, mesh, render and
crew. **NOT yet confirmed:** whether the physics/AI heap children are allocated
for a slot the entry loader skipped, or only for entry slots — **this is the
crash risk and the first thing the incremental test must check.** All engine
calls must run on the GAME thread (a tick hook), never a remote/Frida thread —
mesh load allocates and dispatches.

### [V1b] build slot 23 at runtime — OPEN (the remaining work)

Incremental live test plan (call ONE step at a time on slot 23, read back +
have the driver watch, between each — a crash isolates the offending step):
1. steps 1-3 (identity only), read `vehN+0x4620` name / `+18580` / `+18584`.
2. step 4, then 5 (mesh) — watch for a model appearing at the slot's position.
3. step 6 (crew) — read `vehN+0x32D4` non-null.
4. step 7 activate + `sub_42BC80` — does he appear and can the SC logic drive him.
5. cross-session: does a runtime-built slot survive / clean up on session change.

Because each step can crash, this must be done incrementally in a throwaway race,
NOT wired into a DLL until the sequence is proven end to end. Only then port the
sequence into a `DllMain`-spawned worker that runs it once at session start (from
the GAME thread — engine calls must not come from a remote thread).

---

## 5c. THE NODE CREATOR — FOUND (2026-08-04, static). §5b BELOW IS RETRACTED.

Chain, all read off the shipped exe + the shipped data files:

1. `VehgenScnFileLoader` (0x4C6C40), `this = &veh[N]`, builds the path
   `<gamedir>vehgen.scn` then calls **0x4DDF90** as
   `__fastcall(ecx = outPath, edx = veh+0x4898, stack: veh+0x4894, 0)`
   (disasm at 0x4C6C88-0x4C6CA4). So:
   - **`veh+0x4898` = index into the 271-car roster `dword_8AC394`** (stride
     1060; `v32 = dword_8AC394 + 1060*a2`, template path at entry+32).
   - **`veh+0x4894` = the scene slot id**.
2. 0x4DDF90 **writes** `vehgen.scn`: it opens the roster entry's `Graphics=`
   template (`.gen`) and copies it, substituting the `<...>` tokens —
   `<ID>` → `"%03d"` of the slot id, `<TEAMDIR>`, `<VEHDIR>`, `<SPIN>/<NOTSPIN>`,
   `<LOW>/<MED>/<HIGH>`, `<DASHLOW>/<DASHHIGH>`.
3. The shipped template proves it — `SeasonData\Vehicles\SafetyCar.gen` line 44
   is literally `Instance=SLOT<ID>`, and its own header comment says
   *"ID — this is replaced by the proper slot identification (000, 001, etc.)"*.
4. `VehicleGraphicsSceneLoader` (0x4DFCE0) then calls `sub_55B6B0`
   (`SceneFileTopLevelParser`) with **`ecx = dword_8AFE88`** (the global scene
   manager) to parse that file — creating the instance — and immediately
   `EvaluateSceneNodeProperty("SLOT%03d", 0)` with the **same** `ecx = 0x8AFE88`
   to fetch it (disasm 0x4DFD0A and 0x4DFDFB-0x4DFE01).

**Consequences:**
- The node is created by the car's OWN load, into the global scene. Grep finding
  "no `SLOT` in `1999_Albert_Park.SCN`" is consistent — it was never track data.
- No fixed number of renderable slots, no "spare index" requirement, no
  grid-capacity ceiling. **§5b is retracted.**

**CORRECTION (2026-08-04, live).** The sentence that used to end this section —
"the only requirement is that the parse runs in load context" — was too loose and
cost several runs. Two hard limits, both measured:

1. The scene must already EXIST. Calling `VehgenScnFileLoader` from the tail of
   `AIDriverCountSetup` produced `SLOTnnn present (0)` for all 40 indices
   (`EvaluateSceneNodeProperty`, `ecx = *(0x8AFE88)`): the track scene is not
   loaded yet at that point. `LOG/vehgen.scn` was still written correctly
   (`INSTANCE=SLOT022` verified on disk), but the parse had no tree to attach
   to, the lookup returned null, and 0x4DFE4A dereferenced it.
2. The node is only created if the parse RUNS TO COMPLETION. It aborts on the
   first missing asset, and the game says so:
   `vehgfx.cpp 1697: Unable to read LOG\vehgen.scn: Could not locate mesh file SATLFA.MTS`
   immediately followed by `vehgfx.cpp 1724: Could not find instance "SLOT022"`.
   The same happened to a pool-picked entrant (`SLOT007`), so this is not
   specific to the added car.

So the DLL must let the game's own loop at 0x42C301 do the mesh, and the car's
asset folder must be complete. `0x8AFE88` HOLDS the scene pointer — the game
does `mov ecx, dword ptr [0x8afe88]`; passing the address itself faults inside
`EvaluateSceneNodeProperty` at 0x5453E4.

## 5b. ~~THE SCENE-NODE CONSTRAINT~~ — RETRACTED, see §5c. (kept for the trail)

`VehicleGraphicsSceneLoader` (0x4DFCE0) attaches the car mesh to a **pre-existing
scene instance node named `SLOT%03d`**, indexed by the car's teamId
(`veh+0x4894`):

```c
StringFormat(v89, "SLOT%03d", *((int*)*this + 4645));   // teamId
this[1] = EvaluateSceneNodeProperty(v89, 0);            // 0x5453A0
if (!this[1]) log("Could not find instance \"%s\"", v89);
```

`SceneRenderUpdate` later derefs `this[1]` — and if it is null it crashes at
`+0x10`, **which is exactly the access violation that killed slot 23** (teamId 23
→ looked up `SLOT023`, absent on a 23-car grid whose scene defines only
`SLOT000..SLOT022`). So:

- The number of **renderable** car slots is fixed by how many `SLOTnnn` nodes the
  track scene defines — probably grid capacity (`aiw_pits.json`: 154 tracks cap
  the grid at 24, others `teams×garagespots` up to 36), NOT arbitrary.
- A native SC can render only if it takes an **existing but unused** `SLOTnnn`
  node → its teamId must be a **spare** scene-slot index.
- **If the grid is full (all `SLOT` nodes used), there is no spare node and native
  render is impossible on that track.** This is independent of load-context timing
  and may be the true feasibility ceiling.

**NEXT LIVE TEST (before trusting the DLL): `scene_slot_probe.js`** — enumerate
`SLOT000..SLOT103` in a loaded track, count how many exist and whether any are
spare `>= VehicleCount`. That number decides the whole approach:
- spare nodes exist → build the SC with teamId = a spare index (decouple it from
  the array slot; `TeamIDSetter` sets teamId and array-slot to the same value, so
  the DLL must set them separately or pick an array slot == spare scene index).
- no spare on a full grid → native render needs a scene edit (add `SLOT` nodes to
  the track/vehicle scene), which pushes back toward shipping a data file, OR the
  SC reuses a retired car's node.

## 6. Files

```
DESIGN.md                       this file
SESSION_2026-08-04_native_add.md  THE RECORD of the build session - read first
PROMPT_NATIVE_SC_CRASH.md       problem statement written for a fresh model
ANSWER_NATIVE_SC_CRASH.md       its reply; identified the role gate (§2). Correct.
build_harness.js                Frida validator (mid-race build path, disproven).
scene_slot_probe.js             OBSOLETE - §5b retracted, no spare-node question.
dll/native_sc.c                 design-B DLL. Spawn PROVEN live 2026-08-04.
                                Hook 1, tail of AIDriverCountSetup (0x4BFD90):
                                roster lookup by .VEH path →
                                NationalityDriverInfoLoader → TeamIDSetter(slot) →
                                role 1 → SessionStateTransition(0,0) →
                                ++VehicleCount → BuildActiveVehiclePointerArray.
                                It does NOT call mesh or render itself - the
                                game's own DynMan_Init loop and DynMan_Restart
                                pass do that.
                                Hook 2, DynMan_Restart (0x42C3B0): re-arm role 1
                                before the pass, park (0x32C8=-1 + rebuild) after
                                the original returns. Parking from the worker
                                thread was tried three ways and crashed each time.
                                Numpad1 deploy / Numpad2 recall (NEVER TESTED).
                                Self-ejecting via F1C_NATIVESC_UNLOAD.
                                native_sc.ini [sc] add=0|1 is the A/B switch.
                                Diagnostics still in: sc_probe, SLOTnnn
                                enumeration, brand-DB and roster dumps.
dll/build_native_sc.bat         MSVC x86 build (sets INCLUDE/LIB by hand -
                                vcvars32.bat is broken on this machine; links
                                user32.lib).
```

Note: MSVC rejects `__thiscall` in C, so every `__thiscall` engine call is
declared `__fastcall(this, dummy_edx, ...)`.

### Building + testing native_sc.c (needs the game + a 32-bit MSVC)
```
cl /LD /O2 native_sc.c            (x86 Developer Command Prompt -> native_sc.dll)
```
Ship alongside `SeasonData\Vehicles\FIA\1999_CLK55\` (the SC car files).
TEST PROTOCOL (first inject) — throwaway race, expect a possible crash:
1. Inject at the MAIN MENU, before loading a track (the hook must be armed
   before DynMan_Init runs). Any injector; the DLL self-ejects on the named
   event via `tools/dll_unload_signal.py F1C_NATIVESC_UNLOAD`.
2. Load a race. Read `native_sc_log.txt` (written to the game folder):
   - `[hook] AIDriverCountSetup detoured` — armed.
   - `[add] queued: name='Bernd Maylander' teamId=N roster=262 VehicleCount=N+1`
     — the extra car went in during the field build.
   - `[park] slot N meshed (crew=...)` — the game's OWN mesh pass processed him.
     THAT LINE IS THE WHOLE EXPERIMENT: it means design B works.
   - no `[park]` line (crew stays null) = the game's mesh loop skipped him;
     next move is to hook the tail of `ProcessDriverEntries` (0x4B8DA0) instead,
     or add him earlier (before `AIDriverCountSetup`'s own count is consumed).
3. In the race, Numpad1 = deploy (SC should appear), Numpad2 = recall.

Watch for, in order of likelihood:
- **grid placement**: he is counted at load, so the grid/scoring pass may place
  him and shift the field. If so, park him earlier or repair `0x4058`/`0x4060`.
- **session change**: the hook re-fires per load, so he is rebuilt each time;
  verify no double-build (the detour resets `g_built`/`g_pending` every call).

### OPEN validation points (all first-inject)
- **Load-context assumption**: does the mesh actually load when constructed right
  AFTER DynMan_Init returns? (The whole design hinges on this.)
- **Placement**: a car added after the grid-placement step may spawn at origin or
  an unassigned spot rather than a pit box. Where does the runtime SC appear?
- **Parked ghost**: with VehicleCount = grid+1 and the SC parked via 0x32C8=-1,
  does his mesh render as a frozen ghost? If so, also drop VehicleCount by 1 when
  parked (restore on deploy) — a count change DURING racing self-heals positions
  (the grid re-sort hazard is pre-race only).
- **Session change / restart**: does the runtime slot survive or need rebuilding
  each load? (The hook fires every DynMan_Init, so it rebuilds per load — verify
  it does not double-build or leak.)
- Then: wire `full_safety_car/pace_probe.c`'s caution logic to `g_sc_slot`.

### Validation status (2026-08-04)
- steps 1-4 (identity + `SessionStateTransition(0,0)`) — **PROVEN live**, clean,
  slot 23 = "Bernd Maylander", 0x32C8 stayed -1, VehicleCount unchanged.
- step 7 (`VehgenScnFileLoader` mesh) — **returned without throwing**.
- step 8 (`SceneRenderUpdate`) — **CRASHED** (access violation at null+0x10) when
  steps 5-6 were SKIPPED. Two confounded causes: (a) render needs the car in the
  active array first (5=++VC, 6=rebuild), which the game does before the mesh;
  (b) Frida-thread execution of allocating calls. `build_harness.js` was rewritten
  to remove both confounds: it runs the FULL ordered sequence 1..STEP_LIMIT on the
  **game thread** (tick hook on 0x42C690) and waits within one call.
- step 6 partial (`++VC` + rebuild, no mesh) — **CRASHES on the next frame**.
  Counting slot 23 before it has a mesh makes the following tick process a bare
  shell → null child. **The build must be ATOMIC in one tick.**
- full atomic 1-9 mid-race — **CRASHES at step 8 (render)**. `VehgenScnFileLoader`
  (mesh) returns but does NOT populate the scene/render child mid-race, so
  `SceneRenderUpdate` null-derefs at `+0x10` regardless of count/rebuild order.

### VERDICT: mid-race construction is DISPROVEN. Build at TRACK LOAD.
(finding `native-sc-midrace-mesh-load-fails-must-build-at-track-load`.) The
per-car mesh/scene load needs the track-load scene-parser context
(`ProcessDriverEntries` runs during load with `byte_7C5690`/scene-parser state)
that a mid-race call lacks — `VehgenScnFileLoader` silently no-ops the mesh
mid-race. Identity (steps 1-4) is context-free and works anytime; the mesh is
the load-only part.

**So the shipping design is:**
1. A DLL injected before track load, hooking the END of the field build (after
   `DynMan_Init` 0x42C1A0 / `ProcessDriverEntries` 0x4B8DA0 finish their loop,
   while still in load context) — run the full construction (steps 1-10) for
   slot `N = VehicleCount` there, so the mesh loads correctly.
2. Immediately park him dormant: `slotN+0x32C8 = -1` + `sub_42BC80()` (so he is
   not on the grid / not counted during green running). VehicleCount handling
   TBD — either build at count and drop by 1 when parking, or build then exclude;
   the deploy toggle is the proven `0x32C8`(+count if needed)+rebuild.
3. Deploy/recall mid-race via the proven toggle (no mesh rebuild needed — the
   mesh already exists from load).

**This can no longer be validated by Frida pokes** — the load is a one-time event
and the MCP tears down hooks between calls. Next step is to WRITE the load-hook
DLL and test by injecting before a track load. Author `dll/native_sc.c`:
`DllMain` → trampoline hook on the field-build tail → construct slot N in-context
→ park dormant → self-eject support → deploy/recall keys. Follow
`full_safety_car/dll/pace_probe.c` for the hook/eject/logging skeleton.
- After end-to-end proof: write `dll/native_sc.c` (tick-hook build at session
  start + `0x32C8`+rebuild deploy/recall + self-eject), then wire the existing
  `full_safety_car/pace_probe.c` caution logic to the runtime-built slot.

Prior art / findings: `python tools/ask.py findings native-safety-car` and
`native-sc`. Do not edit `full_safety_car/` from here.
