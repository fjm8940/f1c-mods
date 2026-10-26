# AI pit-strategy restart preservation review - 2026-09-22 (revision 2)

Primitive: `f1c_pit_strategy.h`. Status: reviewed static layout plus bounded
call-closure evidence; isolated and hardened-adapter tests pass; native
gameplay acceptance pending. No native function is called and no hook is added.

## Static evidence (canonical decompiled functions)

| Function | Fact used |
|---|---|
| `sub_429960_00429960.c` | Full AI reset rebuilds the plan, writes `this[4450]` count, fills 16-byte entries from `this[4452]`, writes a terminator at entry `count`, and zeroes `this[4472]` (cursor). |
| `AIPushLap_0042a180.c` | Requests a stop when `entries[cursor].lap <= current lap` (`+0x403C`). |
| `ForcePitExit_0042a110.c` | Native final-lap guard; raises the request through the setter. |
| `AIStateFlagSetter_0042ac70.c` | `__thiscall this[4435] = value` for value<=1; the reviewed request latch is `+0x454C`. |
| `PitStopDecisionTireCompound_0042a3d0.c` / `PostPitCleanup_0042a6f0.c` | Rewrite the current entry and advance the cursor on a completed service or a forced-retirement parking; callers must not mix the two. |
| `AIMainUpdate_00424e70.c` | Treats `cursor >= count` as plan complete in non-race sessions. |
| `PitStopBlockingCondition_0042aee0.c` | Reads the entry mark. |

## Layout, bounds and extent contract

- `+0x4588` int32 planned-stop count, 0..4 (race strings parse 1..4; generated
  plans are 3..4). Slot 0 means "no stops" when no strategy is configured.
- `+0x4590` five 16-byte entries: `[0]` lap, `[4]` fuel, `[8]` compound,
  `[12]` mark. Entries `0..count-1` are planned stops; entry `count` is the
  native terminator; entries beyond it are initialization remnants. Five
  entries exactly fill `0x4590..0x45DF`.
- `+0x45E0` int32 cursor, `0..count`, immediately after the entries block.
- `+0x454C` int32 native request latch, 0/1.
- **Extent contract (review R1):** `bytes` is the caller's declared extent from
  the vehicle base, so the minimum is `F1C_PIT_STRATEGY_REQUIRED_EXTENT`
  (`0x45E4`, one past the cursor). Both helpers refuse before any access when
  the declared extent is smaller; a guard-page test in the RF suite proves a
  short declared extent touches nothing. Callers pass `VEH_STRIDE`.

## Ownership and exclusions

The plan belongs to one vehicle, not a slot, and is only consulted for active
cars. The helper never widens ownership; the caller passes the active survivor
list, so retirees and per-car invalidity are excluded. The differently named
SDK `aiPitStrategyIndex` at `+0x342C` is unrelated and not touched.

## Fail-safe behavior

`f1c_pit_strategy_capture` refuses (and accesses nothing) when the declared
extent is shorter than `REQUIRED_EXTENT` or count/cursor/request are outside
the reviewed bounds. `f1c_pit_strategy_restore` additionally requires the live
target block to pass the same validation before any write, so an
unsupported/mismatched layout can never be overwritten with preserved bytes. A
captured request of 0 is never written, so a legitimate request raised after
the restart is not cleared.

## Current consumer

`development\mods\red_flag\src\gr44.c` captures at red-flag trigger
(`snapshot_positions`), refreshes only genuine progress at the game-thread
restart gate (cars named by the dedicated per-sequence contamination list,
latched at pit-retirement arming, keep the trigger plan; the list survives the
DriveToPits->Countdown timeout fallback's transient retirement/arrival clears
and resets only at the sequence boundary), and writes once in the broker's
game-thread green transaction, which the broker drains before the original
frame update runs `AIMainUpdate`. The v70 Y checkpoint carries the same state.
Bounded evidence with exact search commands:
[trial `bounded-native-evidence.txt`](../../../development/mods/red_flag/trials/2026-09-22/ai-pit-strategy-preservation/bounded-native-evidence.txt).
Review responses:
[R1-R4](../../../development/mods/red_flag/trials/2026-09-22/ai-pit-strategy-preservation/RESPONSE_TO_REVIEW.md),
[r3 remaining R2](../../../development/mods/red_flag/trials/2026-09-22/ai-pit-strategy-preservation/RESPONSE_TO_R3_REVIEW.md).

## Limits

Decision semantics above are static facts from the extracted corpus plus
isolated extracted-function tests; the customer race is not reproduced and no
live game process was opened, so the timing contract is bounded
static/call-closure evidence rather than a native trace. Protection boundaries
were reviewed but the new adapters are intentionally not hardened targets.
