# Problem statement for a fresh model — F1 Challenge 99-02, adding one extra car at track load crashes

You are helping reverse-engineer **F1 Challenge 99-02** (`F1 Challenge Online.exe`,
32-bit, **non-ASLR, image base 0x400000**, gMotor/ISI engine, same lineage as
NASCAR Thunder 2003/2004). Everything below is measured on the shipping exe or
observed live in the running game. Numbers are facts; prose is our reading of
them and may be wrong.

## The goal

Add ONE extra car (a safety car) to a race **at track load**, without editing any
season/track data and without it taking a grid slot. It must then be parked
dormant and deployed mid-race on a key press.

The deploy/recall half is already **proven live**: write `veh+0x32C8`
(`-1` = out of field, `1` = AI, `0` = player) and call the game's own
`BuildActiveVehiclePointerArray` (0x42BC80, ignores ecx, no args, returns the
active count). Reversible, no crash, driver-confirmed the car leaves/rejoins the
standings. **That part is not the problem.**

The unsolved half is **constructing** the car at load.

## The data model (all verified)

- Vehicle array: base **0x5EC6D8**, stride **0x48C0** (18624), 104 slots.
- `veh+0x32C8` int32 role: `0` player / `1` AI / `-1` not in field.
- `veh+0x4620` ASCII driver name.
- `veh+0x4894` = scene slot id, used to build the scene node name `"SLOT%03d"`.
- `veh+0x4898` = index into the roster DB `dword_8AC394` (stride **1060**,
  count `dword_8AC390` = 271 = every vehicle in SeasonData, not just entries).
- `dword_5C329C` = VehicleCount.
- `dword_8AC38C` = output of `VehicleLookupByName` (a global "last match index").
- Active pointer array `dword_7C54F0[104]`, rebuilt by 0x42BC80 from `0x32C8`.
- `veh+0xC` = the physics object. `veh+0xC+0x396C` (= `veh+0x3978`) = pointer to
  a tyre-brand record; `veh+0xC+0x3970` = compound index.

Scene note (proven static, corrects an earlier wrong theory): the `SLOTnnn` node
is **not** track data and **not** per-entry. `VehgenScnFileLoader` (0x4C6C40)
calls 0x4DDF90, which **writes** `<gamedir>vehgen.scn` from the car's own `.gen`
template (`Graphics=` in its `.veh`), substituting `<ID>` with `%03d` of
`veh+0x4894`; then `VehicleGraphicsSceneLoader` (0x4DFCE0) parses that file into
the global scene manager `dword_8AFE88` and looks the node up in the same scene.
The shipped `SeasonData\Vehicles\SafetyCar.gen` line 44 is literally
`Instance=SLOT<ID>`. So each car creates its own node; there is no ceiling.

## The load pipeline

`GameMainEntry` (0x45B200): `Game_Enter → Game_Setup → Game_Init → while(Game_Restart) ... → Game_Post → Game_Setup → Game_Init ...`
So `Game_Init` (0x42D6B0) runs **per track load**.

`Game_Init` → `DynMan_Init` (0x42C1A0), which does, in order:
1. `AIDriverCountSetup` (0x4BFD90) — builds the field's identities (only when
   `byte_7C5690 == 0`).
2. `BuildActiveVehiclePointerArray()` (0x42BC80).
3. `for (i = 0; i < dword_5C329C; ++i) VehgenScnFileLoader(dword_7C54F0[i], 1)`
   — the mesh/scene load, with a progress bar (verified at 0x42C301).
Then later `Game_Restart` → `DynMan_Restart` (0x42C3B0), which does
`for (i = 0; i < dword_5C329C; ++i) SceneRenderUpdate(dword_7C54F0[i], 1)`.

`SceneRenderUpdate` (0x4C6D90) begins with
`LargeConfigFunctionWithPhysicsConstants(this+3)` (0x4E45B0) → ... →
`SuspensionAndWheelPhysics` (0x4E5F60), which at **0x4E6461** does the ONLY
write in the whole exe to that tyre field:

```
lea eax, [edi + 0x88]     ; hdv + 136  = the TireBrand string
mov ecx, 0x8AD254         ; the tyre-brand DB
call 0x4D7880             ; TireBrandLookup(db, name) -> record or 0
mov [ebx + 0x396c], eax   ; <-- stays 0 if the lookup fails
```

`SceneRenderUpdate` later calls `AIDriverStructInit` (0x424790), which at
0x424A45 calls `sub_429960` (the pit-strategy setup), which does
`lea ecx,[esi+0xC]; call 0x4F1630` (`TireNameAccessor`), which is:

```c
result = *(char**)CompoundIndexLookup(*(this + 14700), *(this + 14704));
```

`CompoundIndexLookup` (0x4D7570) is `if (a2>=0 && a2<this[4]) return this[5]+680*a2;`
— it reads `[ecx+0x10]` with **no null check**.

## The reference sequence (the game's own "add a car")

`ProcessDriverEntries` (0x4B8DA0), disasm 0x4B8F45-0x4B8F7F, per entry:

```
SessionStateTransition(veh, 0, 0)        ; 0x4EF5A0, __thiscall(veh, int, int)
++dword_5C329C
BuildActiveVehiclePointerArray()         ; 0x42BC80
VehgenScnFileLoader(veh, 1)              ; 0x4C6C40, __thiscall(veh, int)
SceneRenderUpdate(veh, 1)                ; 0x4C6D90, __thiscall(veh, int)
SessionStateTransition(veh, 3, 0)
```
Identity earlier in the same function is `sub_4C7B20(veh, "<name>.VEH")`, which
is literally `VehicleLookupByName(name); NationalityDriverInfoLoader(veh, dword_8AC38C);`

## What our DLL does

Injected at the main menu. It puts a 5-byte detour on `AIDriverCountSetup`
(0x4BFD90 — its prologue is exactly one 5-byte position-independent instruction,
`a1 9c 32 5c 00` = `mov eax,[0x5C329C]`), calls the original, then on the game's
own load thread runs:

```c
slot = VehicleCount;                       /* 23 on this grid */
VehicleLookupByName("SEASONDATA\\VEHICLES\\FIA\\1999_CLK55\\1999_FIA29.VEH");
drvidx = dword_8AC38C;                     /* = 262 */
NationalityDriverInfoLoader(veh, drvidx);  /* 0x4C78F0 */
TeamIDSetter(veh, slot);                   /* 0x4C6960 -> sets 0x4894 and 0x32D0 */
SessionStateTransition(veh, 0, 0);
dword_5C329C += 1;                         /* 23 -> 24 */
BuildActiveVehiclePointerArray();
VehgenScnFileLoader(veh, 1);
SessionStateTransition(veh, 3, 0);
/* deliberately NOT SceneRenderUpdate - see run 3 */
```
`veh+0x32C8` is left at `-1` the whole time (the slot was never in the field).
All `__thiscall` targets are declared `__fastcall(ecx, dummy_edx, args)` because
MSVC rejects `__thiscall` in C.

## What happened, run by run (all live, same track/session type)

| run | change | result |
|---|---|---|
| 1-2 | stop after `++count` + rebuild, let DynMan's own loops do mesh+render | crash **0x4D7579** at end of load |
| 3 | add `VehgenScnFileLoader` + `SceneRenderUpdate` + `STATE(3,0)` | crash **0x40CD68** *inside our own `SceneRenderUpdate` call* — it reaches `SessionPhaseDecode` via `DelegationWrapperToFUN0040ed10(dword_5EA560 = CamMan, veh+0xC)` and derefs `this[6]`. Conclusion: `SceneRenderUpdate` cannot be called this early in the load. |
| 4-8 | drop our `SceneRenderUpdate`, keep mesh + `STATE(3,0)` | our whole block logs OK, then crash **0x4D7579** at the very end of the load |

Crash detail (vectored exception handler in our DLL, run 6+):

```
ACCESS VIOLATION at 0x004D7579 (reading 0x00000010)
eax=00000000 ebx=00000000 ecx=00000000 edx=... esi=00000000 edi=006596D4 ebp=006596D4
stack:  0x004F1642  TireNameAccessor
        0x0042996F  sub_429960
        0x00424A3D  AIDriverStructInit
        0x004C6E3D  SceneRenderUpdate
        0x0042C40E  DynMan_Restart
        0x0042D836  Game_Init
        0x0045B2AB  GameMainEntry
```
So: `DynMan_Restart`'s own `SceneRenderUpdate` loop → AI init → tyre name →
`CompoundIndexLookup(ecx = 0)`, i.e. `veh+0x3978` was never filled for this car.

## Measurements that rule things out

- The roster entry for the safety car (index 262) is **fully populated**:
  `+00 '...\1999_FIA29.VEH'`, `+08 '...\1999_FIA.HDV'`,
  `+16 '...\HEADPHYSICS.INI'`, `+24 '...\F1SOUNDS.SFX'`,
  `+32 'SEASONDATA\VEHICLES\SAFETYCAR.GEN'`.
- The tyre-brand DB is healthy and **contains the SC's brand**: at `0x8AD254`,
  `count = 20`, `TireBrandLookup(db,"generic") = 0x046C31D8`,
  `TireBrandLookup(db,"1999_Bridgestone") = 0x046C3028`,
  `TireBrandLookup(db,"") = 0`. (`1999_FIA.hdv` has `TireBrand=generic`, and
  `generic.tbc` exists and is used by no other car.)
- A normal car (slot 5) has `veh+0x3978` = a valid brand record; ours is 0.
- The SC's mesh **does** get processed: the game's trace log shows, right after
  `dynman.cpp 525: Entered DynMan::Init()`, three lines
  `util.cpp 265: Unable to adjust texture for material: wcpitbody/wcpithelmet/wcrefueler,
  tex0=SEASONDATA\VEHICLES\FIA\1999_CLK55\Mercedes-Benz*.bmp` — i.e. our car's
  pit-crew materials. (Those .bmp files genuinely do not exist in that folder.)
- `trace.txt` also prints `tire_manager 1080: Non-existent tire brand ""` six
  times, but those occur during `Options::Setup`, long before our code runs, and
  the owner confirms they appear in normal unmodded loads too. **Do not treat
  them as the diagnosis** — they cost us a wrong turn already.

## The open questions

1. **Why is `veh+0x3978` still 0 after the load?** The single writer is inside
   `SceneRenderUpdate → LargeConfigFunctionWithPhysicsConstants → SuspensionAndWheelPhysics`.
   `DynMan_Restart` does call `SceneRenderUpdate` for every `i < VehicleCount`,
   and our car is counted. Either that loop never reaches our car (is our car
   actually at `dword_7C54F0[23]`, given it is inactive with `0x32C8 == -1` and
   0x42BC80 packs actives first and inactives into the tail?), or the write ran
   with an empty/garbage `hdv+0x88`, i.e. **the .HDV was never parsed into the
   physics object** for our slot. Which step of the real entry pipeline parses
   the `.hdv` into `veh+0xC`? Candidates seen calling `TheSvmHdvVehicleFileParser`
   (0x4F4270, `HDVehicle::parse`): 0x433ED0, `TireModelInit` 0x43E050, 0x45C400,
   `TelemetryDataDump` 0x4D4A80, 0x4E3360, `LargeConfigFunctionWithPhysicsConstants` 0x4E45B0.
2. **Is keeping `0x32C8 == -1` during the load the actual mistake?** A counted
   car that is not in the active region may be skipped by exactly the pass that
   would have initialised it, while still being visited by another. Should the
   car be built as a normal AI (`0x32C8 = 1`) for the whole load and only parked
   afterwards?
3. **Unexplained and possibly important:** after our block, slot 5's driver name
   read `'Bernd Maylander'` in one run and `'Damon Hill (1999)'` in another, with
   identical code. `dword_8AC38C` is a global that we set via
   `VehicleLookupByName` and never restore — could a later pass stamp identity
   from it onto other cars? Is our identity write landing in more than one slot?
4. Is `AIDriverCountSetup`'s tail the wrong insertion point entirely — should the
   car instead be injected as a real entry earlier (e.g. into whatever list
   `ProcessDriverEntries` 0x4B8DA0 walks: it iterates the vehicle array with a
   per-slot 16-byte node list at `this+16+16*k`, and builds any slot whose
   pending `.VEH` name is non-empty)?

## Constraints and what is available

- The game is a real, running 32-bit process; we can inject a DLL, hook, log,
  and read/write memory. Full Hex-Rays decompilation of all 7,138 functions is
  available locally, plus a live Frida/x64dbg/Cheat Engine bridge.
- Each live test costs a human loading a race and reporting the result, so
  prefer answers that a static read or a single instrumented run can settle.
- Do not propose editing season/track data as the fix; the whole point is a
  data-free mod. Diagnosing *with* a data edit to prove a hypothesis is fine.

## What we want from you

Identify **why the added car never gets its tyre-brand record**, and what the
minimal correct insertion point + call sequence is for adding one car at track
load so that the game's own passes initialise it exactly like an entry. Cite the
function/offset evidence for whatever you claim, and say clearly which parts are
inference rather than read from the code.
