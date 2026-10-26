# map_BufferEntryClear — declared `sub_54FAE0`

## Identity

- Address: `0x54FAE0`; file `functions/BufferEntryClear_0054fae0.c`; 34 lines.
- Signature: `void __thiscall sub_54FAE0(_DWORD *this, int a2)`.
- Callers (grep `functions/` for `BufferEntryClear(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L103 and
  L135 (`BufferEntryClear(0x8000000);`) - manifest caller; also `CollisionDetectionSystem` L146/L422,
  `FloatDoubleCoordinateTransform` L22, `TimerFrameRateThrottle` L10, `TrackTemperatureDisplay` L30/L46,
  `VehiclePhysicsVelocityAngularVelocityIntegrator` L405/L504.
- Callees (from `offsets_BufferEntryClear.csv` CALL rows): none.
- `this` = buffer manager; `this[120]` (0x1E0) points to a descriptor whose +4 is an entry count, +8 the
  entry data base. `a2` = entry index, or `0x8000000` to clear all entries. Entry stride is 116 bytes
  (29 dwords).

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x1E0 | 480 | _DWORD | L8 | buffer descriptor pointer (`this[120]`) |
| 0x8 | 8 | _DWORD* | L13 | descriptor data base (`*(v2 + 8)`) |
| 0x4 | 4 | int | L15 | descriptor entry count (`*(v2 + 4)`) |
| 0x1E0 | 480 | _DWORD | L24 | descriptor pointer re-read for the loop bound (`this[120]`) |
| 0x8 | 8 | _DWORD | L29 | descriptor data base (`*(v2 + 8)`, indexed form) |

## Outputs

CSV access W rows. All writes clear state; no new value is stored.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x40 | 64 | _DWORD | L20 | entry dword 16 (`v3[16]`) = 0 |
| 0x40 | 64 | _DWORD | L31 | entry dword 16 (`v5[16]`) = 0 |

The bit-clear writes `*v3 &= ~1u` (L19) and `*v5 &= ~1u` (L30) have no CSV rows (they set no offset
constant; see Questions).

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 120 | L8, L24 | `this` dword index (0x1E0) of the descriptor pointer |
| 8 | L13, L29 | descriptor data-base offset |
| 4 | L15, L24 | descriptor count offset |
| 29 | L21 | entry stride in dwords (`v3 += 29`) |
| 116 | L29 | entry stride in bytes (`116 * a2`) |
| 16 | L20, L31 | cleared dword index inside an entry (+64) |
| 134217728 (`0x8000000`) | L11 | sentinel argument: clear every entry |

## Logic

```
v2 = this[120]                                        [L8]
if (v2) {                                             [L9]
  if (a2 == 134217728) {                              [L11]
    v3 = *(_DWORD **)(v2 + 8); v4 = 0                 [L13,L14]
    if (*(int *)(v2 + 4) > 0) {                       [L15]
      do {    [L17]
        *v3 &= ~1u; v3[16] = 0; v3 += 29; ++v4        [L19,L20,L21]
      } while (v4 < *(_DWORD *)(this[120] + 4))       [L24]
    }    [L25]
  } else {    [L27]
    v5 = (_DWORD *)(*(_DWORD *)(v2 + 8) + 116 * a2)   [L29]
    *v5 &= ~1u; v5[16] = 0                            [L30,L31]
  }    [L32]
}    [L33]
```

## Meaning (GUESS)

- GUESS Clears the "active/claimed" bit 0 and dword 16 of one 116-byte entry of a buffer managed through
  descriptor `this[120]`; `a2 == 0x8000000` is a clear-all sentinel that sweeps every entry using the
  descriptor count [L8-L31].
- GUESS The 116-byte record shape is 29 dwords with a flag word at offset 0 and a cleared state dword at
  +64; the descriptor layout is +4 count and +8 data pointer [L13-L15,L20/L31].
- GUESS Callers use clear-all on racing-line/session resets (`AIRacingLinePathFollowing`,
  `FloatDoubleCoordinateTransform`, `VehiclePhysicsVelocityAngularVelocityIntegrator`) and single-entry
  clears with an index from collision code (`CollisionDetectionSystem` L146/L422,
  `TimerFrameRateThrottle`, `TrackTemperatureDisplay`) [callers].
- GUESS The return type is void; the manifest name "BufferEntryClear" matches the observed semantics
  (no data is produced, only flags reset) [L19-L31].

## Questions

- The extractor has no rows for `*v3 &= ~1u` (L19) and `*v5 &= ~1u` (L30); what does bit 0 mean (in-use,
  dirty, valid) and what is dword 16 (+64)?
- Caller `AIRacingLinePathFollowing` L103/L135 shows `BufferEntryClear(0x8000000)` with one argument for a
  2-arg `__thiscall`; which object is `this` there?
- Is the entry stride really 116 bytes (29 dwords), and does the record have more fields between offset 4
  and +64 that are never touched here?
- Which subsystem owns `this[120]`? The function is at 0x54FAE0 (far from the AI block) but is called from
  racing-line code - is the manager a session-global object?
