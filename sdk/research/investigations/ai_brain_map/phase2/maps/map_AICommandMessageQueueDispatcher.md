# map_AICommandMessageQueueDispatcher — declared `sub_424CD0`

## Identity

- Address: `0x424CD0`; file `functions/AICommandMessageQueueDispatcher_00424cd0.c`; 79 lines.
- Signature: `void __thiscall sub_424CD0(int *this, int a2, int a3)`.
- Callers (grep `functions/` for `AICommandMessageQueueDispatcher(`): `sub_426430` (`0x426430`) L134
  (`AICommandMessageQueueDispatcher((int *)this, 14, *(_DWORD *)(this + 17744))`) - the manifest caller;
  also `AIDriverStructInit` L99, `CameraViewportFrustumInitialization` L55, `PitStopProgress` L79
  (command 15), `TimerFrameRateThrottle` L9, `TrackTemperatureDisplay` L14/L23 (commands 5/4).
- Callees (from `offsets_AICommandMessageQueueDispatcher.csv` CALL rows): `sub_4A46F0` (L13, L17),
  `SessionCleanupFlagClear` (L30), `SceneRenderUpdate` (L34), `StateChangeNotify` (L35),
  `ParameterValueSetter` (L38), `CommandQueueResponseHandler` (L57).
- `this` = AI vehicle/command-queue owner; `a2` = command id (switch: 1, 4, 5, 15); `a3` = command payload.

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4410 | 17424 | _BYTE | L29 | ? byte flag (if clear -> `SessionCleanupFlagClear`) |
| 0x44A8 | 17576 | int | L43 | command queue array pointer (`this[4394]`) |
| 0x44B0 | 17584 | int | L46 | queue write index (`this[4396]`; NAN sentinel writer is AIAbility per glossary) |
| 0x44AC | 17580 | int | L51 | queue capacity / bound (`this[4395]`) |
| 0x4064 | 16484 | int | L59 | ? state (`this[4121] == 2`); same field read at L24 via `*this` (unmatched) |

## Outputs

CSV access W rows, plus the queue slot writes.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4548 | 17736 | _BYTE | L16 | override-enable byte (`FIELD_GLOSSARY.md` 0x4548) |
| 0x413C | 16700 | float | L20 | `dbl_8AFEA0 + 2.0` (`this[4175]`; FLT_MAX in live prior art) |
| 0x4544 | 17732 | int | L37 | ? pit/ability flag = 0 (`this[4433]`) |
| 0x4550 | 17744 | int | L39 | laneMode / waypoint set selector = 1 (`this[4436]`) |
| 0x44B0 | 17584 | int | L52 | queue write index = v6 + 1 / -1 / v6 (`this[4396]`) |
| 0x44A8 | 17576 | int | L56 | queue slot payload write at `this[4394] + 8*this[4396] + 4` |
| 0x4548 | 17736 | _BYTE | L40 | override-enable byte = 1 |
| 0x45F4 | 17908 | _BYTE | L60 | ActiveFlag = 0 (`FIELD_GLOSSARY.md` 0x45F4) |

Queue slot command write `*(_DWORD *)(v5 + 8 * (v6 + 1)) = a2` (L55) has no CSV row (see Questions).

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 4, 5, 1, 15 | L12-L22 | command ids (`a2`) |
| 2 | L20, L59 | `dbl_8AFEA0 + 2.0`; `this[4121] == 2` state compare |
| 7, 8 | L23, L26 | `dword_7C56A4 == 7` / `dword_7C56A8 != 8` session states |
| 18 | L38 | `ParameterValueSetter(18)` argument |
| -1 | L58 | `this[4396] = -1` |
| 8 | L55, L56 | queue record stride (8 bytes: command + payload) |
| 16484, 16700, 17424, 17576, 17580, 17584, 17732, 17736, 17744, 17908 | L16-L69 | field offsets (see tables) |

## Logic

```
switch (a2)                                                              [L10]
  case 4: sub_4A46F0(this, 1, 1); break                                  [L12,L13]
  case 5: *((_BYTE *)this + 0x4548) = 0; sub_4A46F0(this, 0, 0); break   [L15,L16,L17]
  case 1: *((float *)this + 4175) = dbl_8AFEA0 + 2.0; break              [L19,L20]
  case 15:                                                               [L22]
    if (dword_7C56A4 == 7) v4 = *(_DWORD *)(*this + 16484) == 0          [L23,L24]
    else v4 = dword_7C56A8 != 8                                          [L26]
    if (v4) { if (!*((_BYTE *)this + 0x4410)) SessionCleanupFlagClear(this) }   [L27,L28,L29,L30,L31]
    else { SceneRenderUpdate(0); StateChangeNotify(0) }                  [L32,L33,L34,L35,L36]
    this[4433] = 0; ParameterValueSetter(18); this[4436] = 1             [L37,L38,L39]
    *((_BYTE *)this + 0x4548) = 1                                        [L40]
    break    [L41]
v5 = this[4394]                                                          [L43]
if (v5) {                                                                [L44]
  v6 = this[4396]                                                        [L46]
  v7 = 0                                                                 [L47]
  if (v6 < 0) goto LABEL_21                                              [L48]
  v8 = (_DWORD *)this[4394]                                              [L69]
  while (*v8 != a2) { ++v7; v8 += 2; if (v7 > v6) goto LABEL_21 }        [L70,L71,L72,L73,L74,L75]
LABEL_21:                                                                [L50]
  v9 = v6 + 1 < this[4395]; this[4396] = v6 + 1                          [L51,L52]
  if (v9) {                                                              [L53]
    *(_DWORD *)(v5 + 8 * (v6 + 1)) = a2                                  [L55]
    *(_DWORD *)(this[4394] + 8 * this[4396] + 4) = a3                    [L56]
    CommandQueueResponseHandler(this)                                    [L57]
    this[4396] = -1                                                      [L58]
    if (!a2 && this[4121] == 2) *((_BYTE *)this + 0x45F4) = 0            [L59,L60]
  } else {    [L62]
    this[4396] = v6                                                      [L64]
  }    [L65]
}    [L67]
```

## Meaning (GUESS)

- GUESS Central AI command-message router: `a2` selects a command, side effects run first, then the
  command/payload pair is (re)queued into an 8-byte-per-entry array at 0x44A8 with write index 0x44B0 and
  capacity 0x44AC [L10-L78].
- GUESS Command 15 is the pit-release/session-render command: under session-type conditions it either
  clears session cleanup state (when the 0x4410 byte is clear) or issues `SceneRenderUpdate(0)` /
  `StateChangeNotify(0)`; then sets 0x4544 = 0, `ParameterValueSetter(18)`, 0x4550 = 1, 0x4548 = 1
  [L22-L41]. This mirrors the doc-54 pit-release sequence in `map_AIAbility.md` L136-L157.
- GUESS Commands 4/5 toggle `sub_4A46F0(this, flag, flag)` (pit-related on/off) and command 1 resets
  0x413C to `dbl_8AFEA0 + 2.0` [L12-L20].
- GUESS The tail performs dedupe/queue-advance: scan the queue for the same command id; if found (or the
  index is invalid), advance the write index; otherwise append and invoke `CommandQueueResponseHandler`
  (one-shot flush) and reset the write index to -1 [L43-L78].
- GUESS `this[4396]` (0x44B0) is used here as the queue write index while AIAbility writes a NAN to the
  same field each tick; the two uses may alias - flagged below.

## Questions

- Caller patterns are inconsistent with a 3-arg `__thiscall`: `AICommandMessageQueueDispatcher(0, 0)`
  (AIDriverStructInit L99, CameraViewportFrustumInitialization L55) and
  `AICommandMessageQueueDispatcher((int *)this, 14, *(_DWORD *)(this + 17744))` (sub_426430). Which
  argument wins as `this`, and is the first caller passing `this = 0` (early-out at L44)?
- The enqueue command write `*(_DWORD *)(v5 + 8 * (v6 + 1)) = a2` (L55) has no CSV row; only the payload
  write at L56 is extracted. `unmatched_AICommandMessageQueueDispatcher.txt` lists only L24.
- `*(*this + 16484)` at L24 is not in the CSV (`unmatched` lists it). Is `this[0]` really the vehicle
  pointer for the SessionType-7 check?
- 0x4410 (17424) is a byte read before `SessionCleanupFlagClear` but is not named in `FIELD_GLOSSARY.md`;
  what flag is it?
- Does 0x44B0 really double as queue index and AIAbility NAN sentinel, or are these two different object
  bases?
