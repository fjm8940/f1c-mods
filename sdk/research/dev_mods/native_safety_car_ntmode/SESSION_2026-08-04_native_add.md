# Session log 2026-08-04 — native SC add at track load

Factual record. Every claim below is either read from the decompiled exe /
shipped data files, or observed live in `native_sc_log.txt` / the game's own
`LOG/trace.txt` / the Windows Application event log. Where something is not
known, it says so.

---

## 1. What was built

`dll/native_sc.c`, rebuilt over ~19 live loads. Final shape:

**Hook 1 — tail of `AIDriverCountSetup` (0x4BFD90)**, which `DynMan_Init`
(0x42C1A0) calls during a track load. There it does identity only:

```
prev = dword_8AC38C                       (saved, restored at the end)
VehicleLookupByName("...1999_FIA29.VEH")  -> dword_8AC38C = 262
NationalityDriverInfoLoader(veh, 262)     -> veh+0x4898 = 262
TeamIDSetter(veh, slot)                   -> veh+0x4894 = slot, veh+0x32D0 = slot
veh+0x32C8 = 1                            (role AI - see §2)
SessionStateTransition(veh, 0, 0)
++dword_5C329C
BuildActiveVehiclePointerArray()
```

It does **not** call `VehgenScnFileLoader` or `SceneRenderUpdate` (see §3, §4).

**Hook 2 — `DynMan_Restart` (0x42C3B0)**: before calling the original, set role
1 if the car is dormant; after the original returns, set role −1 +
`BuildActiveVehiclePointerArray()` (see §5, §6).

Both prologues are single 5-byte position-independent instructions
(`0x4BFD90: a1 9c 32 5c 00` = `mov eax,[0x5C329C]`; `0x42C3B0: push 0x26F`), so
the 5-byte detours steal cleanly.

Build: `dll/build_native_sc.bat` (sets INCLUDE/LIB by hand because
`vcvars32.bat` fails on this machine; links `user32.lib`). MSVC rejects
`__thiscall` in C, so every `__thiscall` target is declared
`__fastcall(this, dummy_edx, ...)`.

---

## 2. Root cause of the first crash: the role gate — FACT (static + live)

`LargeConfigFunctionWithPhysicsConstants` (0x4E45B0), which is
`SceneRenderUpdate`'s first statement, begins:

```c
result = *(_DWORD *)(this + 12988);      // this = veh+0xC  ->  veh+0x32C8 = role
if ( result ) { if ( result <= 0 || result > 3 ) return result; }   // -1 -> return
...
v3 = *(char **)(1060 * *(_DWORD *)(*(_DWORD *)this + 18584) + dword_8AC394 + 8);
TheSvmHdvVehicleFileParser(v3, v16, 0);          // the .hdv parse
TheSvmHdvVehicleFileParser(*(_DWORD *)this + 18324, v16, 1);
SuspensionAndWheelPhysics(v16, v8);
```

`-1 <= 0` is true, so a car with role −1 skips the `.hdv` parse and
`SuspensionAndWheelPhysics`. That function at **0x4E6461** is the only write in
the whole exe to the tyre-brand pointer:

```
lea eax,[edi+0x88]    ; hdv+136 = the TireBrand string
mov ecx, 0x8AD254     ; the tyre-brand DB
call 0x4D7880         ; TireBrandLookup
mov [ebx+0x396c], eax ; veh+0xC+0x396C = veh+0x3978
```

`AIDriverStructInit` (0x424790) has **no** role gate; it calls `sub_429960`,
which does `lea ecx,[esi+0xC]; call 0x4F1630` (`TireNameAccessor`) =
`CompoundIndexLookup(*(veh+0x3978), *(veh+0x397C))`, and `CompoundIndexLookup`
(0x4D7570) reads `[ecx+0x10]` with no null check.

Live confirmation of the whole chain, from the DLL's vectored exception handler:

```
ACCESS VIOLATION at 0x004D7579 (reading 0x00000010)  ecx=0
TireNameAccessor(0x4F1642) <- sub_429960(0x42996F) <- AIDriverStructInit(0x424A3D)
  <- SceneRenderUpdate(0x4C6E3D) <- DynMan_Restart(0x42C40E) <- Game_Init(0x42D836)
```

Ruled out as causes, by measurement, before the gate was found:
- roster entry 262 is fully populated —
  `veh='...1999_FIA29.VEH' hdv='...1999_FIA.HDV' gen='SEASONDATA\VEHICLES\SAFETYCAR.GEN'`;
- the tyre-brand DB is healthy and contains the SC's brand — at `0x8AD254`,
  `count=20`, `TireBrandLookup(db,"generic") = 0x046C31D8`,
  `TireBrandLookup(db,"1999_Bridgestone") = 0x046C3028`, `("") = 0`.

Fix: carry the car at role 1 for the whole load. After that the tyre pointer was
populated live (`SC tireObj=046931D8`, the exact `"generic"` record).

---

## 3. `SceneRenderUpdate` cannot be called at `AIDriverCountSetup` time — FACT (live)

Calling it there crashed at **0x40CD68** (`SessionPhaseDecode`, deref of
`this[6]`). `SceneRenderUpdate` runs
`DelegationWrapperToFUN0040ed10(dword_5EA560, veh+0xC)` and `dword_5EA560` is
the camera manager, which is not ready at that point in the load.
`DynMan_Restart` calls `SceneRenderUpdate` for every `i < dword_5C329C` later,
which is the correct moment.

---

## 4. `VehgenScnFileLoader` cannot be called at `AIDriverCountSetup` time — FACT (live)

The DLL enumerated `SLOT000..SLOT039` via `EvaluateSceneNodeProperty`
(0x5453A0, `ecx = *(0x8AFE88)`) at that hook point and got
**`SLOTnnn present (0)`** — the scene is empty because the track scene has not
been loaded yet. `LOG/vehgen.scn` was written correctly (verified on disk:
`INSTANCE=SLOT022`), but the parse has no tree to attach to, so the lookup
returns null and `VehicleGraphicsSceneLoader` derefs it:

```
0x4DFE01 call EvaluateSceneNodeProperty("SLOT%03d")
0x4DFE0B mov [esi], eax        ; may be NULL
0x4DFE0D jne skip_log          ; NULL -> logs vehgfx.cpp 1724 "Could not find instance"
0x4DFE40 mov ecx, [esi]        ; NULL
0x4DFE4A call 0x54C030         ; -> 0x54C075 reads [ecx+0x3C]  = the crash
```

The game meshes every counted car itself at 0x42C301 inside `DynMan_Init`
(`for i < dword_5C329C: VehgenScnFileLoader(dword_7C54F0[i], 1)`), after the
scene exists. Letting it do that is what finally worked.

Note on `DESIGN.md` §5c: the claim that a car's own `vehgen.scn` creates its
`SLOTnnn` node is supported (disasm at 0x4C6C88-CA4 + 0x4DDF90 writing the file
from the roster's `Graphics=` template + `SeasonData\Vehicles\SafetyCar.gen`
line 44 `Instance=SLOT<ID>`). What was **wrong** in the earlier write-up was the
implied freedom to trigger it at any point of the load; it only works once the
scene exists.

---

## 5. The park must be deterministic, on the game thread — FACT (live)

Two async park attempts from the DLL's worker thread both crashed:

| trigger | what happened |
|---|---|
| `veh+0x32D4` (crew) non-null | crew is built in `DynMan_Init`'s mesh loop, i.e. **before** `DynMan_Restart`'s render pass; parking there put role back to −1 and the render pass then hit the §2 crash at 0x4D7579 |
| `veh+0x3978` (tyre record) non-null | that field is written at line 7 of `SceneRenderUpdate` while `AIDriverStructInit` is line 19 of the **same call**; the worker flipped the role mid-call and crashed at **0x4F1576** |

A "SessionTime is advancing" guard did not help: across a session change
`SessionTime` is already advancing while `DynMan_Restart` is still looping.

Final: park inside the `DynMan_Restart` detour, after the original returns. The
render pass is provably finished and we are on the game thread.

---

## 6. A session change does not re-run `AIDriverCountSetup` — FACT (live)

Going qualifying → race produced no new `[add]` lines and crashed at 0x4F1576
with the car still dormant. The field is reused, so hook 1 never fires again.
Hook 2 therefore re-arms role 1 for every restart pass. After that, four restart
passes were logged clean.

---

## 7. Best live result reached (run 18)

With 21 entrants + player, SC added at slot 22, `dword_5C329C` 22 → 23:

```
[add] slot 22, roster idx 262 ...
[add] role set to 1 for the load
[add] identity done, mesh left to the game's own loop
[scene] at restart: SLOT022 -> 2F7D7F70 (game's own mesh loop)
[add] deferred STATE(3,0) done
[restart] slot 22 parked dormant after the pass (crew=314E05C0 tireObj=047231D8)
          | SC 0x4058=23 0x4060=23 | player 0x4058=1 0x4060=5
[restart] slot 22 re-armed to role 1 for the render pass
[restart] slot 22 parked dormant after the pass ... | player 0x4058=1 0x4060=5
```

So the game's own pipeline meshed the added car, built its crew and tyre record,
and the car was parked dormant, with the player keeping grid position 1.
`LOG/trace.txt` had no mesh errors on that load.

**That run still ended in a crash at 0x55D852**, and §9 shows that crash was a
missing texture, not the native-add path. **The native add has therefore never
been tested with a complete safety car.** Numpad1/Numpad2 deploy/recall have
never been exercised at all.

---

## 8. Entrant-pool interaction — observed, mechanism NOT established

With the SC present as a normal pool entrant, a load can contain both the
pool-picked Bernd Maylander and the DLL-added one (owner: *"we now have two
Maylanders, the other one started without AI"*). Slot 5's driver name varied per
load across runs (Damon Hill / Ralf Schumacher / Eddie Irvine / Bernd Maylander).

Also observed once, with `Classes` temporarily edited so the pool could not pick
him: 22 requested AI produced **two Jean Alesis**. 1999 has 11 teams × 2 = 22
drivers; with one taken by the player, 21 remain. Whether the duplicate is
caused by the pool running short is **not established** — it was seen once, in a
process whose roster had been parsed with the edited file.

---

## 9. Damage done to the car folder during this session, and the repair

This is recorded in full because it cost the owner several crashed loads.

**What was wrong with the folder when the crashes started:** at 17:11 the game
logged `Could not locate mesh file SATLFA.MTS`, i.e. the 16 `SAT*.MTS` were not
in `SeasonData\Vehicles\FIA\1999_CLK55\`. They are present in the folder listing
taken at the start of this session. I cannot determine from the shell history
what removed them between those two points.

**What I did, in order:**
1. `cp -n _disabled_vehicles/1999_SafetyCar/SAT*.MTS` into the CLK55 folder
   (16 files). `-n` does not overwrite, so this either restored them or was a
   no-op.
2. Concluded from a header comparison (`SATLFA.MTS` starts `02 00 00 00 "RIM"`,
   while `BTLFA.MTS` from `CDB.MAS` starts `CUBE_MTS_4.10`) that the files were
   the wrong format, and ran `rm -f SAT*.MTS`. **That conclusion was wrong** —
   see below — and the deletion removed whatever was in the folder at that time.
3. Created `SeasonData\Vehicles\SafetyCar_native.gen` (tyres repointed at stock
   F1C meshes) and set `Graphics=` in `1999_fia29.veh` to it. The owner rejected
   this; both changes were reverted — the `.gen` deleted, and the `.veh`
   restored from `1999_fia29.veh.bak_classes` (verified byte-identical by
   `diff`). An earlier `Classes="SafetyCar"` edit was reverted by the same
   restore.
4. Restored the 16 `SAT*.MTS` from `_disabled_vehicles/1999_SafetyCar/`.
5. Found `RIM.BMP` and `TREAD.BMP` absent from the CLK55 folder — the game was
   logging `Could not locate bitmap file RIM.BMP` 53 times — and restored both
   from `_disabled_vehicles/1999_SafetyCar/`. **This is what fixed the game**;
   the owner confirmed the car then loaded correctly.

**Correction to the "wrong format" claim in step 2:** the identical `SAT*.MTS`
set ships inside the stock archive `SeasonData\Vehicles\1994_MAS\1994_cdb.mas`,
with the same `02 00 00 00 "RIM"` header and the same sizes. A byte comparison
of all 16 restored files against the copies extracted from that archive returned
**16 identical, 0 different**. So that header is a normal F1C mesh variant and
the meshes were never malformed.

**Unverifiable:** whether the `SAT*.MTS` that were in the CLK55 folder before
step 2 were byte-identical to the `_disabled_vehicles` copies. They are the only
other copies of those files on the machine (a full-disk search found no others,
and no archive contains them). If the folder's copies had ever been edited in
place, that edit is not recoverable.

**Current folder state (verified):** `1999_fia29.veh` byte-identical to the
backup; `Graphics=SafetyCar.gen`, `GenString=SAFETYCR`,
`Classes="FormulaOne, OpenWheel, 1999"`; 16 `SAT*.MTS` identical to the
`1994_cdb.mas` copies; `RIM.BMP` + `TREAD.BMP` present (49,208 bytes each);
`SafetyCar_native.gen` deleted; `TEAM.MAS`, `.hdv`, `engine.ini`, `damage.ini`,
`sc_gears.ini` never touched.

The three `util.cpp 265: Unable to adjust texture for material: wcpitbody /
wcpithelmet / wcrefueler` lines refer to `Mercedes-Benzpitbody.bmp`,
`Mercedes-Benzpithelmet.bmp`, `Mercedes-Benzrefueler.bmp`, which do not exist in
the folder. They were present in traces from before this session's edits too, so
they are pre-existing and were not introduced here.

---

## 10. State at the end of the session

- `native_sc.ini` is `[sc] add=0`, so the DLL adds nothing even if injected.
- `dll/native_sc.dll` is built and copied to the game folder.
- Per-run logs kept in the game folder as
  `native_sc_log_run1..19*.txt`.
- The safety car loads correctly as a normal entrant again (owner-confirmed).

## 11. Open items

1. Re-test the native add now that the car folder is complete. Set
   `native_sc.ini` `add=1`. Unknown whether it now completes, because every
   previous attempt ran against a folder missing `RIM.BMP`.
2. Deploy/recall (Numpad1/Numpad2) has never been exercised.
3. The duplicate-Maylander interaction of §8 needs a decision: the DLL adds a
   car unconditionally, and the pool may also pick the same car.
4. `sc_probe()` in the DLL still reads `veh+0x3978`/`0x397C` every poll; harmless
   but it is a leftover diagnostic.

---

## 12. The entrant-pool problem SOLVED: `NotAI` — FACT (static + shipped data + live)

§8 recorded that the safety car was being picked as a normal AI entrant, so a
real driver lost a seat. The mechanism and the fix are both the game's own.

**The token exists in F1C's code.** `strings_index.json`: the literal
`"%s NOT: NotAI"` (`aSNotNotai`, `0x5D266C`) has exactly ONE code xref,
`0x4BF7B3`, inside `AIOpponentSelection` (0x4BF790):

```c
StringFormatWithBufferOverflowCheck(v32, "%s NOT: NotAI", a4);   // a4 = the class filter
sub_4C4D50(v32);                                                 // apply the filter
```

`AIOpponentSelection`'s callers are **`AIDriverCountSetup` (0x4BFD90)** — the AI
field builder this DLL hooks — and `GameFilterDataLoader` (0x4C0DB0).

**Both sibling games ship their safety/pace car with it**, and in a non-racing
class:

| game | file | `Classes` |
|---|---|---|
| NT2003 | `Season03\Vehicles\PaceCar\pacecar.veh` | `"PaceCar, ClosedWheel, 2003, NotAI"` |
| F1 2002 | `Season02\Vehicles\FIA\Safgpxa.veh` (Driver `"Bernd Maylander"`) | `"Safety, DSGermany, 2002, NotAI"` |
| F1 2002 | `Season02\Vehicles\FIA\Medgpxa.veh` (medical car) | `"Safety, DSGermany, 2002, NotAI"` |
| F1 2002 | `Season02\Vehicles\Ferrari\Bg01gpxa.veh` (M. Schumacher) | `"FormulaOne, OpenWheel, 2002"` |

NT2003's pace car is additionally named `Driver="Pace, Car"`, which is what the
NT build pass scans the roster for (`_strcmpi(name, "pace car")`, DESIGN §1).

**What could NOT be sourced:** no original source file. `strings_modules.json`
(which recovers 2002 `.cpp` names from `__FILE__` literals) has no entry for
`AIOpponentSelection`, so the source file it was compiled from is unknown. The
token is confirmed in the shipped exe and in shipped data, not in source.

**Applied** to `SeasonData\Vehicles\FIA\1999_CLK55\1999_fia29.veh`
(backup: `1999_fia29.veh.bak_before_notai`), matching the shipped originals:

```
Classes="Safety, DSGermany, 1999, NotAI"
```

The previous line was `Classes="FormulaOne, OpenWheel, 1999"` with the comment
"MUST include season class so it loads/shows". That comment is correct **only**
for `full_safety_car`, which needs him picked as a grid entrant. **This edit
breaks `full_safety_car` until reverted.**

---

## 13. Result of the `NotAI` load (run 21) — the first fully correct run

21 AI + player requested. Live, measured:

- `standings.py`: 22 cars, **no Bernd**, and **Ricardo Zonta (1999) is back at
  P22**. The only real driver still absent is Zanardi, because the player
  occupies a seat (22 real 1999 drivers, player takes one, 21 AI fill the rest).
- `VehicleCount = 23`: the 23rd is the DLL's safety car at slot 22.
- `[scene] at restart: SLOT022 -> 383A71E8 (game's own mesh loop)`
- `[restart] slot 22 parked dormant after the pass (crew=2D53FDA8 tireObj=046E31D8)`
- `player 0x4058=1 0x4060=5` — grid position untouched.
- `LOG/trace.txt`: **0** mesh/instance errors.

So the native spawn now does what it was designed to do: the safety car is a
real, fully-built car above the field, it costs no driver's seat, and it needs
no gdb/track/entry-list edit — only its own `.veh` carrying the same `NotAI`
the shipped F1 2002 and NT2003 cars carry.

**Owner-observed limit:** the game's entry/grid screen shows **23 entrants and
Bernd in grid box 23**. Two different counts are in play — the active pointer
array (22, he is excluded by `0x32C8 = -1`) and `dword_5C329C` (23, which
includes him). The DLL's own log agrees he is given a real grid slot:
`SC 0x4058=23 0x4060=23`. So "no driver displaced" is achieved; "no grid box
occupied" is not.

Three options were put to the owner (not implemented, no decision recorded):
1. leave him at P23 (closest to `full_safety_car`'s behaviour);
2. teleport him into his pit box at load, reusing the proven
   `full_safety_car` mechanism — box via `veh+0x48A4` (AIW TeamIndex) against
   `aiw_pits.json`, position `veh+0x34E0/34E4/34E8`, heading via the 3x3 matrix
   at `veh+0x34BC..0x34E0` (NOT the `0x34F0` mirror, which reverts);
3. drop `VehicleCount` while parked — **only during racing**; a count change at
   RacePhase 0 re-sorts scoring and bumped the player to last (2026-08-04
   finding).

The owner asked for option 2 and then stopped the work before it was started.
Nothing was written for it.

---

## 14. Corrections this session made to earlier project claims

| earlier claim | status now |
|---|---|
| `DESIGN.md` §5b: renderable slots are capped by pre-existing `SLOTnnn` scene nodes, so a full grid leaves no spare | **RETRACTED.** Nodes come from each car's own generated `vehgen.scn`; no ceiling. |
| `DESIGN.md` §5c (as first written): the mesh only needs "load context" | **TIGHTENED.** It needs the scene to already exist AND the parse to run to completion; see §4 and §9. |
| intra-session: the SC's `SAT*.MTS` are wrong-format F1 2002 files | **RETRACTED**, my error. The identical set ships in stock `1994_MAS/1994_cdb.mas`, 16/16 byte-identical. Acting on it broke the car. |
| `1999_fia29.veh` comment: "MUST include season class so it loads/shows" | True for `full_safety_car` only. For native_sc the shipped-original convention (`Safety, ... , NotAI`) is correct. |

---

## 15. Current state (end of session)

- `native_sc.ini` = `[sc] add=1`; DLL built and injected in the running game.
- `1999_fia29.veh` = `Classes="Safety, DSGermany, 1999, NotAI"`; backups in the
  car folder: `.bak_classes` (pre-session original) and `.bak_before_notai`.
- Car folder complete: 16 `SAT*.MTS` (identical to `1994_cdb.mas`),
  `RIM.BMP`, `TREAD.BMP`.
- Per-run logs `native_sc_log_run1..21*.txt` in the game folder.

## 16. Open items

1. **Deploy/recall (Numpad1/Numpad2) has still never been pressed.** This is the
   feature; everything else is scaffolding for it.
2. Grid box 23 is still occupied — see §13 options.
3. `full_safety_car` is broken while the `NotAI` line is in place. Decide whether
   the two mods coexist (they cannot, as written) or the old one is retired.
4. Strip the diagnostics from `native_sc.c` (`sc_probe`, the `SLOTnnn`
   enumeration, the brand-DB dump, the roster string dump) once the feature is
   proven — they exist only to have found the crashes above.
5. Wire `full_safety_car/pace_probe.c`'s caution logic to `g_sc_slot`.

---

## 17. NT2003 reference, read directly (sanity check) — extraction-tier

Source: `F1C_ghidra_project/source_material/Nascar Thunder 2003/decompiled/`
(7,623 functions). The retail exe is SafeDisc-packed, so raw disassembly of
`NASCAR_Thunder_2003.exe` yields garbage — use the decompiled set.

**The pace-car build pass `sub_4B93A0`** (NT2004's `sub_4E29B0`):

```c
for ( i = 0; i < dword_5BD504; ++i ) {
    sub_4E7090(v4, 0);          // v4 = 2 if dword_61539C==2 else 1; 3 if dword_6675A4==2
    if (i)  sub_4C0970(sub_4B9D80(1));       // other entries: random driver
    else    /* roster walk, stride 1128, name at +147, _strcmpi "pace car" */
            sub_4C0970(v6);                  // index 0: the pace car
    sub_4BFDE0(i + 10);         // ID = i + 10, written into 5 fields
}
```

`sub_4BFDE0(car, n)` sets `this[4157]`, `this[8]`, `this[13]`, `this[2688]`,
`this[3518]` — five copies of the same id.

**`sub_4E7090` is NOT a placement.** It writes a mode field and fires a state
call; there are no coordinates in it at all:

```c
result = *((_DWORD *)this + 2674);
*((_DWORD *)this + 2674) = a2;                 // 1 / 2 / 3
...
else if ( !result && a2 ) { sub_41FEB0(*(_DWORD **)this, 14); sub_41FED0(...); }
```

`this+2674` is read as a gate elsewhere (`sub_4E9340`: `if (!this[2674]) ...`),
i.e. it switches the car out of on-track behaviour.

**The decisive part — `sub_421790` (NT2003 dynman init):**

```c
// racing field: stride 4168, bounded by dword_60FC28
while (1) { ... v4 += 4168; if ((int)v4 >= (int)dword_60FC28) goto LABEL_10; }

sub_4B93A0(v14);                       // build the pace cars

if ( dword_5BD504 > 0 ) do {           // a SEPARATE array
    v10 = (char *)off_5BD508 + v9;     //   stride 16672
    dword_611388[v8] = (int)off_5BD508 + v9;   // its own pointer table
    sub_4C0030(v10, 1);
    *(off_5BD508 + v9 + 16644) = 43;
    *(off_5BD508 + v9 + 16668) = 1;
    v9 += 16672;
} while (v8 < dword_5BD504);
```

**NT2003 keeps its pace cars in a dedicated array with its own count and its own
pointer table, entirely separate from the racing field.** The pace car is never a
field entry: not counted, not scored, not grid-placed, not in the classification.
There is nothing to hide and no count to manipulate.

### What this means for the F1C port (honest assessment)

F1C has **no separate vehicle list** — one array (`0x5EC6D8`, stride `0x48C0`)
plus `dword_5C329C` and the active pointer array `0x7C54F0` (DESIGN §2). To get
the game to build a car at all it must be counted (`DynMan_Init`'s mesh loop is
`for i < dword_5C329C`). Therefore:

- the extra entrant in the classification is **structural to this design**, not a
  defect that can be tidied away;
- the only count lever (`drop_count`) **crashes on session change** (§18);
- removing him from the classification needs a different mechanism — hooking
  whatever builds the classification rows. Prior art already says as much:
  *"it needs the UI row builder hooked; `FinishState=2` only relabels the row
  DNF"*.

Also corrected by this reading: NT's "garage/pit assign" is a **mode write**, not
a teleport, which lines up with `full_safety_car`'s proven F1C recipe (hand the
car over once with `callToBox=1` + `pittingStatus=2` + `laneMode=1` +
`curWaypoint=0` and let the game park it) rather than with the coordinate
teleport implemented in §18.

---

## 18. Pit-box placement and the count drop — built, tested, results

Both added to `native_sc.c` this session, ini-gated.

**Placement** (`[pitbox] enabled=1`): after the render pass, write the box
position to `veh+0x34E0/34E4/34E8` and build the 3x3 heading matrix at
`veh+0x34BC..0x34E0` (cos, sin, -sin, cos) from the AIW yaw. `veh+0x34F0` is
deliberately NOT written — it is a derived mirror that reads back stale.
Coordinates come from `tools/native_sc_pitbox.py` via `aiw_pits.json`.

**REWORKED 2026-08-04 (supersedes the above):** the one-shot teleport in the
`DynMan_Restart` detour was REMOVED and replaced by `sc_pit_assign()`, a
worker-thread port of NT2003's garage/pit mode write (`sub_4E7090`, this[2674]
= 1/2/3 — a mode write, no coordinates). F1C's proven equivalent
(`full_safety_car` HANDOFF_2026-08-03 §5, driver-verified: "he did a pit stop
sequence, his tires were changed"): during **RACE_PHASE 4** (the start
sequence) re-arm role 1 + rebuild, teleport to the box (position + heading
matrix from the same ini keys), then hand over **ONCE** with `callToBox=1`
(0x454C) + `pittingStatus=2` (0x3450) + `laneMode=1` (0x4550) +
`curWaypoint=0` (0x4354). The game runs its own stop (2→3→4) and parks him
indefinitely. Gated on `SESSION_TYPE==7` (race, not quali); re-armed per start
when the phase drops below 4. Untested — needs a live race.

**LIVE 2026-08-04 (driver-confirmed):** hand-over fired
(`[pit] RACE_PHASE 4: slot 22 handed over ONCE ... game drives 2->3->4 and
parks him`), Bernd did a real pit stop and sat in his box. **BUT he does not
STAY pitted — when the stop ends the game releases him (PitStopProgress
command 15 fires when SessionTime passes stopEndTime 0x3470) and he leaves.
Known hold from prior art (`full_safety_car` HANDOFF_2026-08-03c §9 item 2):
re-arm `veh+0x3470` (stopEndTime) into the future AFTER the car settles —
`PitStopReadyCheck` overwrites it when the car comes to rest, so a write
before settling is lost. Clear it on deploy. NOT IMPLEMENTED.**

- **Result: WORKS, owner-confirmed** (*"he was"* in a pit box).
  `[pit] slot 22 placed at (10.497, 0.353, 506.505) yaw 3.1390 (ini team 11,
  car's own 0x48A4 = 11)` — the car's own AIW TeamIndex matched the box picked.
- **But it does not survive the start.** The car still owns a grid slot
  (`0x4058 = 23`) and the start sequence re-pins every car to its grid box, so
  the owner reported *"maylander is back on grid"*. This matches the existing
  finding that a pre-lights teleport is undone by the start sequence.
- **Correct fix (not implemented):** `full_safety_car`'s proven recipe — place
  during `RACE_PHASE 4` and hand over ONCE with `callToBox=1` +
  `pittingStatus=2` + `laneMode=1` + `curWaypoint=0`; the game then runs its own
  pit sequence and parks him indefinitely. Two cautions from that work: write the
  flags ONCE (re-applying pins `pittingStatus` at 2 so the stop never completes),
  and do NOT arm his retirement.

**Count drop** (`[sc] drop_count`): after parking, `dword_5C329C -= 1`.

- It does what it claims: `VehicleCount 22`, 22 rows in `standings.py`, no Bernd,
  model still on track.
- **It crashes on session change.** race → menu → back crashed at `0x54A7ED`
  (WER offset `0x14a7ed`), reading a garbage pointer (`esi=0x00DC00DC`), stack
  through `VehgenScnFileLoader` (`0x4C6CA9`) — the teardown/reload walking the
  added car's scene data with a count that no longer covers it.
- **A/B proof:** the identical sequence with `drop_count=0` did not crash
  (owner-confirmed). Same failure `full_safety_car` documented for its runtime
  decrement, reached by a second route. **Default is now 0 and should stay 0.**

**A claim of mine that the A/B disproved:** I reported that the count drop had
re-sorted the grid and pushed the player to last (`0x4058=22 0x4060=22`). The
owner pointed out that starting last is correct with no qualifying time set, and
the `drop_count=0` run showed `0x4058=22` as well. The count write is innocent;
the log even recorded `player 0x4058 1->1 0x4060 5->5` across it. The earlier
`0x4058=1` readings came from qualifying sessions, so comparing them to a race
grid was invalid.

---

## 19. New tool

`tools/native_sc_pitbox.py` — writes `[pitbox]` x/y/z/yaw/team into
`native_sc.ini` from `aiw_pits.json`. `--list [substr]` browses tracks,
`--team N` chooses the box, `--garage BAY` is the garage variant. Preserves
whatever `add=` is set to.

**This is a dev-side crutch, not shippable.** A distributed mod cannot require
running a Python script per track. Before shipping, the DLL should parse the
`.aiw` itself (`[PITS]` is plain text; `full_safety_car/pace_probe.c`'s
`CreateFileA` hook already learns the current track's `.aiw` path). That belongs
in C, in `native_sc.c`; there is no reason to introduce C++.

---

## 20. FINAL STATE (session end)

**Working, owner-confirmed:**
- Native spawn: SC added as an extra car at track load; the game's own passes
  mesh it and build its scene node, crew and tyre record; parked dormant;
  session change survived; clean `LOG/trace.txt`.
- `NotAI` in the SC's `.veh` keeps it out of the AI entrant pool, so no real
  driver loses a seat (Zonta restored).
- Pit-box placement puts it in a real box with the correct heading.

**Known limits:**
- The SC is listed as an extra entrant (23) and owns a grid box; §17 explains why
  that is structural, and the only count lever crashes.
- Placement is undone by the start sequence; needs the `RACE_PHASE 4` hand-over.
- **Deploy/recall (Numpad1/Numpad2) has still never been pressed.**
- `full_safety_car` is broken while the `NotAI` line is in place
  (backup: `1999_fia29.veh.bak_before_notai`).

**Files:**
- `dll/native_sc.c` + `native_sc.dll` (built), `dll/build_native_sc.bat`
- `native_sc.ini` in the game folder: `add=1`, `drop_count=0`, `[pitbox]` Monza
- `tools/native_sc_pitbox.py`
- logs `native_sc_log_run1..22*.txt` in the game folder

**Car folder (owner's, restored and verified):** `1999_fia29.veh` differs from the
pre-session original ONLY in the `Classes` line; 16 `SAT*.MTS` byte-identical to
`1994_cdb.mas`; `RIM.BMP` + `TREAD.BMP` present.

## 21. Next steps, in order

1. Press Numpad1/Numpad2. Everything else is scaffolding for this.
2. Replace the one-shot teleport with the `RACE_PHASE 4` + flag hand-over.
3. Decide on the classification row: accept 23, or hook the UI row builder.
4. Move `.aiw` parsing into the DLL and drop the Python step.
5. Strip the diagnostics from `native_sc.c`.
6. Wire `full_safety_car/pace_probe.c`'s caution logic to `g_sc_slot`.
