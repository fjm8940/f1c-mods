# map_RenderCallWithGuard — declared `sub_4D1140`

## Identity

- Address: `0x4D1140`; file `functions/RenderCallWithGuard_004d1140.c`; 6 lines.
- Signature: `int __stdcall sub_4D1140(_DWORD *a1, _DWORD *a2, _DWORD *a3)`.
- Callers (grep `functions/` for `RenderCallWithGuard(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L35,
  L57; `Camera` (`0x4A3B50`) L61; `DistanceComparison` (`0x4EFFD0`) L11; `LoopOverTwoParallelArrays`
  (`0x4D1030`) L30; `RenderTickUpdate` (`0x4C8B70`) L29; `SceneRenderUpdate` (`0x4C6D90`) L13;
  `sub_4D1180` (`0x4D1180`) L10; `sub_4F0D20` (`0x4F0D20`) L14; `sub_4F1FE0` (`0x4F1FE0`) L141;
  `VehicleMainPhysicsUpdate` (`0x4441F0`) L860, L1309.
- Callees (from `offsets_RenderCallWithGuard.csv` CALL rows): `sub_4C6F10` (`0x4C6F10`),
  `IndexedStructDataRead` (`0x403FC0`).
- `a1` = some scene/render owner object; `a1[N]` byte offset = `N * 4`.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x48A4 | 18596 | _DWORD | L3 (`a1[4649]`) | ? index 1 (doc 54 mode 3 uses veh+0x48A4 as AIW point / pit TeamIndex) |
| 0x48A8 | 18600 | _DWORD | L5 (`a1[4650]`) | ? index 2 / secondary key |

Global read, not an offset (not in the CSV): `dword_5EA2C0` (shared table base) at L5.

## Outputs

None. (The guarded callee `sub_4C6F10` may write to `a1`, but this function itself stores nothing.)

## Constants

| value | lines | note |
|---|---|---|
| 4649 | L3, L5 | `a1[N]` index (byte 0x48A4) |
| 4650 | L5 | `a1[N]` index (byte 0x48A8) |

## Logic

```
if ((int)a1[4649] < 0)                                                     [L3]
    sub_4C6F10(a1)                                                         [L4]
return IndexedStructDataRead(dword_5EA2C0, a1[4649], a1[4650], a2, a3)    [L5]
```

## Meaning (GUESS)

- GUESS Guarded indexed read: if index 1 (`a1[4649]`, byte 0x48A4) is negative, it first calls
  `sub_4C6F10(a1)` to repair/resolve it, then reads a record from the shared table `dword_5EA2C0` using
  both indices and returns the result [L3-L5].
- GUESS The negative sentinel means "no selection yet" (e.g. no pit/team spot picked), which the repair
  call fixes [L3,L4].
- GUESS doc 54 maps veh+0x48A4 to the mode-3 pit/team AIW point index, so the indexed read likely
  resolves AIW/pit data rather than a renderer [doc 54; L3,L5].
- GUESS The phase-2 name "RenderCallWithGuard" comes from the render-heavy caller set (Camera, render
  ticks, scene update, vehicle graphics); the code itself is a table lookup wrapper [callers].

## Questions

- What does `sub_4C6F10` (`0x4C6F10`) do when the key is negative (allocate/derive index)?
- What is the table at `dword_5EA2C0` and the layout returned by `IndexedStructDataRead` (`0x403FC0`)?
- What are the semantics of the second key `a1[4650]` (byte 0x48A8)?
- Is the "Render" name correct? All call sites sit in render/scene/physics update loops, but the callees
  are index/lookup helpers.
- Compare with `SingleFunctionDispatch_10e0` (`sub_4D10E0`), which forwards to `sub_403CD0` with
  `(a1+16472) - 1` — same table `dword_5EA2C0`, different index field.
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
