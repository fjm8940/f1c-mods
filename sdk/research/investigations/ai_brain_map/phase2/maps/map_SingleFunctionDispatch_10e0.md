# map_SingleFunctionDispatch_10e0 — declared `sub_4D10E0`

## Identity

- Address: `0x4D10E0`; file `functions/SingleFunctionDispatch_10e0_004d10e0.c`; 4 lines.
- Signature: `_DWORD *__stdcall sub_4D10E0(int a1, float *a2, _DWORD *a3)`.
- Callers (grep `functions/` for `SingleFunctionDispatch_10e0(`): `AISteeringRacingLineCorrection`
  (`0x42B0E0`) L148; `SceneRenderUpdate` (`0x4C6D90`) L11; `sub_4BC9B0` (`0x4BC9B0`) L33;
  `sub_4D1180` (`0x4D1180`) L5; `sub_4D0110` (`0x4D0110`) L251.
- Callees (from `offsets_SingleFunctionDispatch_10e0.csv` CALL rows): `sub_403CD0` (`0x403CD0`).
- `a1` = the object whose index field (byte 0x4058) selects the record; `a2`, `a3` forwarded unchanged.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4058 | 16472 | _DWORD | L3 (`*(_DWORD *)(a1 + 16472) - 1`) | ? index/counter (decremented by 1 before the call) |

Global read, not an offset (not in the CSV): `dword_5EA2C0` (shared table base) at L3.

## Outputs

None (returns the callee's pointer).

## Constants

| value | lines | note |
|---|---|---|
| 16472 | L3 | byte offset of the index field (0x4058) |

## Logic

```
return sub_403CD0(dword_5EA2C0, *(_DWORD *)(a1 + 0x4058) - 1, a2, a3)      [L3]
```

## Meaning (GUESS)

- GUESS Thin forwarder: reads the index at `a1 + 0x4058`, subtracts 1 (converting a 1-based selection to
  a 0-based index), and calls `sub_403CD0` on the shared table `dword_5EA2C0` with both forwarded
  arguments [L3].
- GUESS Companion of `RenderCallWithGuard` (`sub_4D1140`), which calls `IndexedStructDataRead` (0x403FC0)
  with the 0x48A4/0x48A8 indices; `sub_403CD0` (called here) is likely the other indexed reader of the
  same table [L3].
- GUESS Callers span render/scene update and AI racing-line correction, so the dispatch reads a
  per-object record (graphics or racing-line data) [callers].

## Questions

- What is the difference between `sub_403CD0` (used here) and `IndexedStructDataRead` (0x403FC0, used by
  RenderCallWithGuard)? Same table `dword_5EA2C0`, different reader?
- What index is stored at byte 0x4058, and why is it decremented (1-based selection)?
- Is the phase-2 name "SingleFunctionDispatch" accurate, or is this an indexed record read?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
