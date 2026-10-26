# map_UIFocusChange — `sub_54F7A0` (`UIFocusChange`)

## Identity

- Address: `0x54F7A0`; file `functions/UIFocusChange_0054f7a0.c`; 24 lines.
- Signature: `void __thiscall sub_54F7A0(_DWORD *this, int a2, int a3)`.
- Callers (grep `functions/` for `UIFocusChange(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L107, L113, L119
  (`UIFocusChange(dword_8AD07C, 1)`, `dword_8AD080`, `dword_8AD084`); `SphereProximityTag` (`0x4BDB10`) L16
  (`UIFocusChange(*(_DWORD *)(v8 + 32), 1)`); `SwitchBasedStateHandler` (`0x40F380`) L41, L49
  (`UIFocusChange(v5, 1)`). All `functions/` call sites show two arguments (receiver implicit, `a3` dropped).
- Callees (from `offsets_UIFocusChange.csv` CALL rows): `sub_54B570` L9; `RenderListIteration` L21;
  `SceneBatchRenderCheck` L22.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x1E0 | 480 | _DWORD | L6 (`this[120]`) | ? gate flag/handle on the UI object |
| 0x204 | 516 | _DWORD | L9 (`this[129]`) | ? scene node pointer |
| 0x14 | 20 | _DWORD | L11 (`*(_DWORD *)(v4 + 20)`) | node flag word (v4 = this[129]) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x14 | 20 | _DWORD | L14 (`*(_DWORD *)(v4 + 20) = v5 \| 536870912`) | node flag word, bit 29 set |

Global writes outside the tables: `dword_8C5F44 = 0` (L8); `dword_8C5F40 = RenderListIteration(...)` (L21).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/UIFocusChange_0054f7a0.c`.

| value | lines | note |
|---|---|---|
| 120 | L6 | `this[N]` index (0x1E0) |
| 129 | L9, L10, L17, L18 | `this[N]` index (0x204) |
| 20 | L11, L14 | node field offset (0x14) |
| 220 | L17 | `this[129] + 220` argument (0xDC) |
| 232 | L18 | `this[129] + 232` argument (0xE8) |
| 69 | L19 | `this + 69` argument (0x114) |
| 536870912 (`0x20000000`) | L12, L14 | node flag bit 29 |
| 4 | L15 | vtable slot `*(_DWORD *)dword_8B8F24 + 4` |

No integer bit-pattern is stored into a float field (`v4 + 20` is a `_DWORD` flag word).

## Logic

NOTE: source uses scaled `this[N]` for the array fields; the bit-29 flag literal is written in decimal
(536870912) because the source line carries only the hex token.

```
if (a2 != this && this[120]) {                                              [L6]
  dword_8C5F44 = 0                                                          [L8]
  sub_54B570(this, 0, this[129])                                            [L9]
  v4 = this[129]                                                            [L10]
  v5 = *(v4 + 0x14)                                                         [L11]
  if ((v5 & 536870912) == 0) {                                              [L12]
    *(v4 + 0x14) = v5 | 536870912                                           [L14]
    (*(vtable)(*(_DWORD *)dword_8B8F24 + 4))(dword_8B8F24, this[129] + 0xDC, this[129] + 0xE8, this + 0x45)  [L15,L16,L17,L18,L19]
  }
  dword_8C5F40 = RenderListIteration(this, a3)                              [L21]
  SceneBatchRenderCheck(this, a2, a3)                                       [L22]
}
```

## Meaning (GUESS)

- GUESS UI/scene focus toggle: when called with a different object `a2`, it clears global `dword_8C5F44`, calls
  `sub_54B570(this, 0, node)`, then sets bit 29 of the node flag word at `node+0x14` and fires a virtual
  callback on global manager `dword_8B8F24` with node sub-objects `node+0xDC`, `node+0xE8` and `this+0x114`
  [L6-L19].
- GUESS Callers pass scene-node handles (`dword_8AD07C/80/84`) with `a2 = 1` while toggling node visibility
  (`SceneNodeVisibilityToggle` immediately before/after), consistent with switching a viewed UI/camera element
  [callers].
- GUESS `dword_8C5F44` / `dword_8C5F40` are UI focus/render-list state globals; `RenderListIteration` returns
  the value stored in `dword_8C5F40` [L8,L21].
- GUESS The bit-29 test avoids re-running the callback while the node is already marked focused [L12-L14].

## Questions

- `a3` is declared and used at L21/L22, but all `functions/` call sites show only two arguments; the real
  third argument is lost by Hex-Rays.
- What is the virtual function at `(*(_DWORD *)dword_8B8F24 + 4)`?
- What object is `this+0x114` (`this[69]`) passed to that callback?
- Are `dword_8C5F44`/`dword_8C5F40` read elsewhere (renderer state)?
