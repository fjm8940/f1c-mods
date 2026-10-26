# AI override API (mechanical_failures DLL) - 2026-10-02

Status: DEV / test tool. Not a public interface yet. Owner intent (2026-10-02): keep this as the ABI for future AI
driving manipulation or new AI pathfinding/driving built outside the game's AI.

## Transport
- Named shared memory `Local\MF_AI_OVERRIDE_V1`, created by the DLL at load (`ai_ovr_open`, engine_failure.c).
- Layout is APPEND-ONLY. Never reorder or resize existing fields; new fields go at the end. Writers that do not know a
  field leave it 0 (= off).
- Write order for clients: data fields first, `magic` last.

## Layout (little-endian, 4-byte fields)
| Off | Type | Field | Meaning |
|---|---|---|---|
| 0 | u32 | magic | `0x4D46414F` ('MFAO'); anything else = override off |
| 4 | i32 | slot | AI vehicle slot 1..MF_AI_MAX-1 (veh = 0x5EC6D8 + slot x 0x48C0) |
| 8 | i32 | brake_on | 1 = use `brake` |
| 12 | f32 | brake | 0..1 -> brake cmd 0x4438 + actuator 0x3584 |
| 16 | i32 | throttle_on | 1 = use `throttle` |
| 20 | f32 | throttle | 0..1 -> throttle cmd 0x4434 + actuator 0x357C |
| 24 | u32 | hook_calls | DLL -> client: +1 per applied tick (proof of life) |
| 28 | i32 | steer_on | 1 = use `steer` (since 7F8E47A6) |
| 32 | f32 | steer | -1..1 -> steer cmd 0x442C + actuator 0x3580 |
| 36 | i32 | gear_on | 1 = AI auto-shift (0x426330) skipped for this slot (since F67D486D) |
| 40 | u32 | gear_up_seq | each +1 = one upshift request |
| 44 | u32 | gear_down_seq | each +1 = one downshift request |

## Where it is applied
- Brake/throttle/steer: post-hook on AIMainUpdate 0x424E70, after the pull-off code. FACT (live 2026-10-02): full
  control - the AI's own writes all happen inside AIMainUpdate, so the post-hook value is what physics uses. Exe scan:
  no other writer of 0x3580/0x3584 (except a network/replay unpack cave), no reader of 0x442C outside AI code.
- Gears: detour on the AI auto-shift 0x426330 (thiscall veh, prologue `D9 81 10 3A 00 00`). With gear_on the original
  is skipped; a pending request calls the game's shift routine 0x4EF930(veh+0xC, gear) when the shift timer
  veh+0x3A10 <= 0. Gear veh+0x39FC (-1 R, 0 N, 1..top), top gear veh+0x3A04. Presses made before gear_on are ignored.
  Live: NOT yet tested.
- One slot at a time. Unload restores both prologues.

## Client
`tools/ai_brake_gui.py` (`ai_brake_gui.bat`, elevated shell): per-control "user controls" checkboxes, sliders, keys
I throttle / K brake / J L steer / U down / O up (global key state; a key acts only on a ticked control), steer
"fight" meter (reads of actuator 0x3580 that differ from the sent value).
