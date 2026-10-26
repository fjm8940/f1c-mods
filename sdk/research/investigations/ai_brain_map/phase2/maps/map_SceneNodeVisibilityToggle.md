# map_SceneNodeVisibilityToggle — declared `sub_54FB40`

## Identity

- Address: `0x54FB40`; file `functions/SceneNodeVisibilityToggle_0054fb40.c`; 13 lines.
- Signature: `int __thiscall sub_54FB40(_DWORD *this, int a2)`.
- Callers (grep `functions/` for `SceneNodeVisibilityToggle(`): `AIRacingLinePathFollowing`
  (`0x4E9CA0`) L106, L108, L112, L114, L118, L120; `ShortComputation_e770` (`0x40E770`) L16;
  `SwitchBasedStateHandler` (`0x40F380`) L29, L37, L42, L47, L50; `VehicleGraphicsSceneLoader`
  (`0x4DFCE0`) L120; `sub_40FE50` (`0x40FE50`) L263.
- Callees (from `offsets_SceneNodeVisibilityToggle.csv` CALL rows): none.
- `this` = node-owner object; `this[129]` is the scene node pointer, then direct offsets on it.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x204 | 516 | _DWORD | L6 (`this[129]`) | ? scene node pointer |
| 0x14 | 20 | _DWORD | L7 (`result + 20`) | ? node flags word (bit 6 toggled) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x14 | 20 | _DWORD | L9, L11 | ? node flags word, bit 6 set when a2 != 0, cleared when a2 == 0 |

## Constants

| value | lines | note |
|---|---|---|
| 129 | L6 | `this[N]` index (byte 0x204) |
| 20 | L7, L9, L11 | node flags byte offset (0x14) |
| 0x40 | L9 | bit 6 set (decimal 64) |
| 0xFFFFFFBF | L11 | bit 6 clear mask (decimal 4294967231 / signed -65) |

## Logic

```
result = this[129]                                     [L6]
v3 = *(_DWORD *)(result + 0x14)                        [L7]
if (a2) *(_DWORD *)(result + 0x14) = v3 | 64           [L8,L9]
else    *(_DWORD *)(result + 0x14) = v3 & 4294967231   [L10,L11]
return result                                          [L12]
```

## Meaning (GUESS)

- GUESS Toggles bit 6 (0x40) of the flags word at `node + 0x14` for the scene node pointed to by
  `this[129]` (byte 0x204): `a2 != 0` sets the bit, `a2 == 0` clears it [L6-L11].
- GUESS The bit is a node visibility/enable flag (name from the phase-2 target set); callers pass literal
  1/0 from racing-line path following and state-handler switches [callers].
- GUESS The function returns the node pointer so callers can chain further node operations [L12].

## Questions

- What exactly is bit 0x40 in the node flags at +0x14 (visibility, shadow, collision, LOD)?
- What node object is stored at `this[129]` (byte 0x204) and who owns it?
- Are there other bits written elsewhere in the same flags word that this function must preserve?
  (It uses read-modify-write, so only bit 6 is touched.)
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
