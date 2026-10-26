# T4 research: viewer HUD toggle (key "2") and TV Director HUD policy

Date: 2026-10-14. Author: Matias (research by agent, static only). The game was running; no live tool, attach, or poke was used.
Tiers: FACT = read directly in decompile or docs. DERIVED = inference from FACT. OPEN = needs a live test.

## 0. Bottom line

- Key "2" is scan code 0x03 (DIK_2). It is consumed by `sub_4D8450` at 0x004D8450, not by the camera key mapper. The mapper at 0x0040BE70 has no case 3 (FACT).
- Each queued press flips `dword_7CF788` (0x007CF788, int32) between 0 and 1 and prints "HUD On" or "HUD Off" (FACT).
- 0x007CF788 is NOT read by the main HUD renderer `HUDFlagRendererMain` 0x004D82D0 (FACT, reader list). It gates the on-screen display controller at 0x008AD298 (FACT, via `sub_4D8880`). Whether that hides the timing board the owner means is OPEN (live test).
- Do not use `byte_7CE254` (0x007CE254, "DisableHUDFlag"). It gates physics, audio, restart and session logic (FACT). Its label is misleading.
- Recommended mod approach: write 0x007CF788 directly from the game thread with save/restore, and call the game's own `sub_4D8880(-1)` once when turning the HUD off, if the live test needs it. Never inject DIK 3 through the key queue.

## 1. Key-2 handler (FACT unless noted)

Queue: `word_7CB21C[]` (int16 DIK codes), `dword_7CB218` (count). The count is not self-clearing (tools/GAME_CONTROL.md section 2). The consumer table there lists scancodes 3 and 4 for `sub_4D8450`.

Excerpt, `functions/sub_4D8450_004d8450.c`. Called per frame from `ShortComputation_f070` 0x0049F070, which is called from `GamePostMainLoop` 0x0042D8A0:

```c
v7 = word_7CB21C;
while ( *v7 != 3 )            /* 3 = DIK_2 */
{
  if ( *v7 == 4 ) { ... broadcast display toggle on dword_7CF638 ... }
  ...
}
v14 = dword_7CF788 == 0;      /* toggle 0 <-> 1 */
dword_7CF788 = v14;
if ( v14 )  { /* message "HUD On"  */ }
else
{
  IndexValidationSetter(-1);  /* sub_4D8880, clears on-screen display index */
  /* message "HUD Off" */
}
```

Facts about this branch:
- The flag is an int32 (dword). It is written only here (writer list of 0x7CF788: sub_4D8450 only). Value 1 = the "HUD On" branch (FACT from the message order).
- Each queue entry toggles once. A re-armed count toggles again on the next frame (FACT, GAME_CONTROL section 2 gotcha). So injecting DIK 3 is unsafe for the mod.
- Messages go to `MessageDisplayBuffer` 0x005EC4CC through `TextLineWrappingAndRendering` 0x0045BD10 (FACT).
- DIK 3 = "2" is the standard DirectInput value (DERIVED from the DIK table, not live-tested).

Callees: `HUDFlagRendererMain` 0x004D82D0 (called on every call of sub_4D8450; not related to the flag), `IndexValidationSetter` 0x004D8880 (below), `TextLineWrappingAndRendering` 0x0045BD10.

## 2. The flag and its readers

`ask.py global 0x007CF788` (FACT): writers 1 (sub_4D8450). Readers 3:

- `sub_4D8880` 0x004D8880 IndexValidationSetter (FACT):
  ```c
  if (a2 == -1) this[2] = -1;
  else if (this[a2+3] && dword_7CF788) this[2] = a2;
  ```
  With the flag at 0, the on-screen display cannot select any index. `this` is the on-screen controller `dword_8AD298` 0x008AD298 (DERIVED: `sub_4D8450(&dword_8AD298)` from ShortComputation_f070). The ECX is omitted in the decompile, so this[2] = 0x008AD2A0 (DERIVED).
- `ShortComputation_3330` 0x00473330 (FACT): stores `&dword_7CF788` in `this[199]`, adds the string `aOn` and the list entry `off_5BE704`. This is an options/menu object bound to the flag (DERIVED: the on/off item of the DISPLAY options screen; GAME_CONTROL section 4c lists "CONTROLLER ASSIGNMENTS / DISPLAY (Options)").
- `sub_4D8450` (itself).

Not a reader: `HUDFlagRendererMain` 0x004D82D0 (its reads are 0x7CFEA0, 0x5EA43C, 0x8AB6E4, 0x7C56A8, 0x8AD394, 0x8B8DC8). The timing/telemetry board draw path has not been traced in this task. Therefore "HUD off" in the game means on-screen display off. Whether the owner's viewer HUD is that display is OPEN.

Persistence (FACT): no per-frame rewrite of 0x007CF788 was found. The only writer is the key-2 branch, so a value the mod writes stays until the next key-2 press or the next menu write (menu write path not traced; OPEN).

## 3. Other globals checked and rejected for this use

| Address | Name in docs | Finding |
|---|---|---|
| 0x007CE254 | DisableHUDFlag (docs/12, docs/26) | FACT: writer GlobalStateSetter 0x004A0D30 (callers SessionScoringUpdate 0x4643C0, sub_4A0030, sub_4A00A0, sub_4A0250, SystemOSAPICall 0x4A03F0). Readers include VehiclePhysicsIntegrationStep 0x441650 (line 37), Gearbox 0x443B70, RestartGate 0x4A0FD0, GamePostMainLoop 0x42D8A0, RenderTickUpdate 0x4C8B70, audio 0x4E0ED0. DERIVED: it means "UI screen up" (GAME_CONTROL and the sub_4A00A0 / sub_4A0250 writers). Do NOT write it. |
| 0x007CF638 | AuthorizationBypassFlag (DERIVED label) | Key 4 "Broadcast Displays" toggle in sub_4D8450. Not the HUD. |

## 4. Proposal for director_state.inc (game-thread task, same model as dcam_set_circuit line 721 and dcam_set_angle line 766)

Address constants (add next to DCAM_LOCKCHECK_ADDR):
- `DCAM_HUD_FLAG_ADDR` 0x007CF788, int32, read/write, valid values 0 or 1 (1 = On).
- `DCAM_HUD_OFF_FN` 0x004D8880 (IndexValidationSetter, thiscall, ECX = 0x008AD298, a2 = -1). Use only if the live test shows the board stays up with the flag at 0.
- Code-byte guard: compare the first 16 bytes of 0x004D8880 and of 0x004D8450 with a copy taken from the reviewed build (same pattern as DCAM_LOCKCHECK_PROLOGUE). Refuse on mismatch.

State (statics, game thread only):
- `g_dcam_hud_saved` int32 = -1 (not saved). Save it only when it is -1, on the first cut that turns the HUD off. Do not overwrite it on later cuts, otherwise a second ONBOARD cut would save 0.
- `g_dcam_hud_applied` int (0/1), so we know we turned it off.

Transitions (hook points):
1. ONBOARD (mode 4 in dcam_set_angle, line 778) and CIRCUIT (dcam_set_circuit, line 721): if the flag is 1 and not saved, save it. Write 0 to 0x007CF788. Optionally call sub_4D8880(-1) with ECX 0x008AD298 (see the live test in section 6).
2. NOSECAM, COCKPIT, TVCOCKPIT (modes 0 to 2): if we applied the HUD off, write `g_dcam_hud_saved` back and clear the saved state. SWINGMAN (mode 3) is not covered by the owner's request: ASK before deciding.
3. Restore command (DCAM_CMD_RESTORE, line 1027 path): same as 2, restore the saved value.
4. Write only on transitions, not every tick, so a user press of key 2 is not fought every frame. If the owner presses 2 during a cinematic, the saved value is stale; state this in the spec as an accepted limit.

Do not inject DIK 3 through word_7CB21C (it toggles and re-arms; see section 1).

Guards before any write: race live (the existing director gates), `byte_7CE254` read-only check equal to 0 (driving view; FACT that the byte is "UI up" when 1), value at 0x7CF788 in {0,1}, and the prologue checks above.

## 5. Risks

- R1 (HIGH): The flag may not hide the owner's timing HUD. It may hide only the on-screen display controller (section 2). Live test required before calling the feature done.
- R2 (MED): `IndexValidationSetter(-1)` from the mod writes this[2] = -1 on the on-screen object; the on-screen board may not come back on Restore until the game selects an index again. Live test.
- R3 (MED): The owner's own key 2 or the Options menu can change the flag; the mod then restores a stale value.
- R4 (HIGH if misused): `byte_7CE254` writes alter physics, audio and restart. Never use it.
- R5 (LOW): The default of 0x7CF788 at race start is unknown statically. The mod must read it, not assume 1.

## 6. OPEN items (static analysis cannot close them)

1. Default value of 0x007CF788 at race start, and whether it changes between sessions (live read, or a trace of the menu writer at 0x473330).
2. Which draw routine reads the on-screen controller index (this[2]) and whether it draws the timing/telemetry board the owner calls the HUD. Trace the consumers of 0x008AD298 and 0x008AD2A0 (OnScreen_Post 0x49F0D0, UtilityFunction_8720).
3. ECX of the IndexValidationSetter call in sub_4D8450 (assumed 0x008AD298).
4. Whether the owner's "HUD" in the viewer is the on-screen board, the flag/yellow-blue strip (HUDFlagRendererMain), or the telemetry overlay of the mod itself.
5. Whether SWINGMAN should turn the HUD off or restore.

Live test (owner-run, guarded, not done by this task): with race live, read 0x7CF788 (expect 1), press key 2 once (expect "HUD Off" text and the board gone), press again (expect "HUD On"). Then cut ONBOARD and compare.

## 7. Evidence list

- functions/KeyboardInputActionMapper_0040be70.c (no case 3; only 74, 78, 156, 199, 201, 209, 210)
- functions/sub_4D8450_004d8450.c (key 2 branch, toggle, messages, IndexValidationSetter call)
- functions/IndexValidationSetter_004d8880.c (sub_4D8880)
- functions/ShortComputation_3330_00473330.c (menu object bound to 0x7CF788)
- functions/GlobalStateSetter_004a0d30.c (byte_7CE254 writer)
- functions/HUDFlagRendererMain_004d82d0.md (reader list)
- functions/ShortComputation_f070_0049f070.c (caller of sub_4D8450)
- tools/GAME_CONTROL.md section 2 (queue consumer table, count gotcha)
- mods/sdk_sandbox/web_telemetry/director_state.inc (dcam_set_circuit line 721, dcam_set_angle line 766, restore path line 1027)
- Static queries: `python tools/ask.py global 0x007cf788`, `python tools/ask.py func 0x004D8450`, `python tools/f1c_mod.py evidence-query findings "key 2" --json` (no prior HUD toggle finding existed; this file is the first)

## 8. Documentation gate

- Durable record: this file. Indexed through findings.jsonl (ids t4-hud-*).
- Fact vs inference: tiers above.
- Pending validation: section 6 (live test).
- No code or game state was changed by this task.
