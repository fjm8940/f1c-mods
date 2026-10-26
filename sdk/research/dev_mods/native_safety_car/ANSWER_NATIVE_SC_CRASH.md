# Native SC load crash — diagnosis and fix (2026-08-04)

Companion to `PROMPT_NATIVE_SC_CRASH.md`. Answers its two asks: why the added
car never gets its tyre-brand record, and the minimal correct insertion point +
call sequence. Root cause is read directly from the decompiled code (static,
FACT-tier). The fix sequence is strong inference from the same code and needs
one instrumented live load to confirm.

## Root cause — proven from code

Keeping `veh+0x32C8 == -1` during the load **is** the mistake (Open Q2 = yes).
A single role gate skips the entire physics/HDV init for a dormant car.

**1. `SceneRenderUpdate` (0x4C6D90) line 7:**
```c
LargeConfigFunctionWithPhysicsConstants(this + 3);   // this+3 = veh+0xC
```

**2. `LargeConfigFunctionWithPhysicsConstants` (0x4E45B0) lines 22–31:**
```c
result = *(_DWORD *)(this + 12988);   // veh+0xC+0x32BC = veh+0x32C8 = ROLE
if ( result )
{
    if ( result <= 0 || result > 3 )
      return result;                  // role = -1  ->  EARLY RETURN
}
else
    TireModelInit((int)&dbl_7C7428, *(_DWORD *)this);   // role = 0 (player)
/* role 0,1,2,3 fall through: */
TheSvmHdvVehicleFileParser(rosterHDV, v16, 0);          // <- the .hdv parse
TheSvmHdvVehicleFileParser(*(_DWORD *)this + 18324, v16, 1);  // .svm override
SuspensionAndWheelPhysics(v16, v8);                     // <- the tyre write
```
The HDV path it uses is `roster[veh+0x4898]+8`, so with `veh+0x4898 = 262`
(written by NationalityDriverInfoLoader) it would resolve `1999_FIA.HDV` fine.
The parse never runs because the call itself is gated out.

**3. Sole writer of `veh+0x3978` — `SuspensionAndWheelPhysics` (0x4E5F60) line 424:**
```c
*(_DWORD *)(this + 14700) = TireBrandLookup(&dword_8AD254, a3 + 136);
/* this+14700 = veh+0xC+0x396C = veh+0x3978
   a3+136     = v16+0x88 = the TireBrand string from the parsed .hdv        */
```

**4. The crash — same `SceneRenderUpdate` call, line 19 → `AIDriverStructInit`
(0x424790).** It has **no role gate**: in its `LOBYTE(a2)` branch it calls
`sub_429960` (0x429960, the pit-strategy parse) for every car →
`TireNameAccessor` (0x4F1630):
```c
CompoundIndexLookup(*(_DWORD **)(this + 14700), ...);   // *(veh+0x3978) = 0
```
`CompoundIndexLookup` (0x4D7570) line 3 reads `this[4]` = `[0x0 + 0x10]` →
**0x4D7579, read of 0x00000010**. Matches the live dump exactly: `ecx=0`,
stack `TireNameAccessor ← sub_429960 ← AIDriverStructInit ← SceneRenderUpdate
← DynMan_Restart`.

The `.hdv` was never parsed for our slot **because the parser call was skipped
on the role gate** — not because the parse failed. Roster entry 262 fully
populated and brand `generic` present in the DB were both correctly eliminated
as red herrings in the prompt.

## Why every run died the same way

| run | role during load | what happened |
|---|---|---|
| 1–2 | -1 from the start | DynMan_Restart render pass: LargeConfig early-return → tyre 0 → AIDriverStructInit null deref at 0x4D7579 |
| 3 | n/a | different bug — own `SceneRenderUpdate` at AIDriverCountSetup time, CamMan (`dword_5EA560`) not ready → 0x40CD68. Correct to drop that call; DynMan_Restart calls it at the right moment |
| 4–8 | -1, parked **too early** | mesh OK (`VehgenScnFileLoader` is role-agnostic, crew appears in DynMan_Init), worker parked on `crew != 0` → role back to -1 **before** DynMan_Restart's render pass → same 0x4D7579 crash "at the very end of the load" |

Note the packing question from Open Q1 is moot: with count = 24 our car sits in
the inactive tail at index 23, inside `i < dword_5C329C`, so it **is** visited
by both DynMan loops. Being visited was never the problem; the role was.

## Minimal correct insertion point + call sequence

The insertion point (tail of `AIDriverCountSetup` 0x4BFD90) is **correct — keep
it**. Two changes, both in `dll/native_sc.c`:

**Fix 1 — set role = 1 in `sc_add_entry`, before `STATE(0,0)`:**
```c
NATLOAD(veh, 0, drvidx);
TEAMID(veh, 0, slot);
V_I32(slot, OFF_ACTIVE) = 1;     /* ADD: physics-init gate requires 0..3 */
STATE(veh, 0, 0, 0);
VEHCOUNT = VEHCOUNT + 1;
REBUILD();
SCN(veh, 0, 1);
STATE(veh, 0, 3, 0);
```
Normal AI entries run this exact pipeline with role 1 and come out fully
initialised. Role 1 is the known-good value through every pass (mesh, render,
AI init). Writing `0x32C8` directly is proven latchable from the deploy/recall
work.

**Fix 2 — park on the tyre record, not the crew.** In `sc_park_when_meshed`:
```c
if (!V_PTR(slot, OFF_CREW)) return;
if (!V_PTR(slot, OFF_TIREOBJ)) return;   /* ADD: render pass not done yet */
V_I32(slot, OFF_ACTIVE) = -1;
REBUILD();
```
Crew appears in DynMan_Init's mesh loop; the tyre record appears **later**, in
DynMan_Restart's render pass. Parking on crew alone restores role -1 before
that pass — which is precisely the run 4–8 crash. `veh+0x3978 != 0` is the
per-car "physics initialised" signal; parking on it guarantees our car's
`SceneRenderUpdate` already completed.

## What this does NOT settle (inference, needs live confirmation)

- **Grid placement.** Role 1 + count 24 through the load means the
  grid-placement pass may hand him a spot. DESIGN.md already lists this
  hazard. If it happens: repair `veh+0x4058/0x4060`, or detour
  `SceneRenderUpdate` to force role 1 only around our own car's call (restore
  -1 right after) so physics initialises without him ever reading active to
  an outside pass. Which is needed depends on whether the placer iterates
  `[0, activeCount)` or `[0, VehicleCount)` — not read yet; one live run
  answers it.
- **Open Q3 (slot-5 name anomaly).** Separate issue, not the crash cause.
  `dword_8AC38C` is a global last-match left dirty at 262; cheap insurance is
  save/restore around `sc_add_entry`.
- **Open Q4.** Not needed. AIDriverCountSetup tail + the game's own DynMan
  loops are sufficient once role is 1. `ProcessDriverEntries` entry-list
  injection is the fallback only if grid placement proves unfixable.

## Confidence

- Root cause + role gate: read directly from decompiled code (FACT-tier
  static). Crash address, register state and stack all reproduced from it.
- Fix 1 + Fix 2 sequence: strong inference from the same code. One
  instrumented live load (`native_sc_log.txt` + the existing `[diag]` tyre
  reads) settles it — exactly the single-run kind the prompt prefers.
