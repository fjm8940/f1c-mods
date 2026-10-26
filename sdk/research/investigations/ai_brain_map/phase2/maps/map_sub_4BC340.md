# map_sub_4BC340 — `sub_4BC340`

## Identity

- Address: `0x4BC340`; file `functions/sub_4BC340_004bc340.c`; 10 lines.
- Signature: `bool __thiscall sub_4BC340(_DWORD *this)`.
- Callers (grep `functions/` for `sub_4BC340(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L32
  (`sub_4BC340((_DWORD *)(*(_DWORD *)this + 16436))`); `LapSplitTimeEvent` (`0x4BC390`) L28
  (`sub_4BC340(this)`); `sub_4BCCC0` (`0x4BCCC0`) L135 (`sub_4BC340(this)`).
- Callees (from `offsets_sub_4BC340.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x30 | 48 | _DWORD | L7 (`this[12]`) | ? view/replay mode (1 and 3 rejected) |

Globals read: `dword_7C569C` (L6), `dword_7CEF84` and `dword_7C56A4` (L7), `dword_7C56A8` (L8).

## Outputs

None. The function returns a `bool`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/sub_4BC340_004bc340.c`.

| value | lines | note |
|---|---|---|
| 12 | L7 | `this[N]` index (0x30) |
| 5 | L6, L7, L8 | session phase / mode comparisons (`dword_7C569C`, `dword_7C56A4`, `dword_7C56A8`) |
| 3 | L7 | rejected view mode (twice) |
| 6, 8 | L8 | accepted `dword_7C56A8` values |
| 1 | L7 | rejected view mode |
| 0 | - | none directly |

No integer bit-pattern is written into a float field.

## Logic

```
result = dword_7C569C == 5
      && ( (v1 = this[12], v1 != 1 && v1 != 3)
           || (dword_7CEF84 && v1 != 3 && dword_7C56A4 == 5) )
      && (dword_7C56A8 == 5 || dword_7C56A8 == 6 || dword_7C56A8 == 8)     [L6,L7,L8]
return result                                                              [L9]
```

## Meaning (GUESS)

- GUESS Session/view gate: true only when global session phase `dword_7C569C == 5`, the object's mode `this[12]`
  is neither 1 nor 3 (or the `dword_7CEF84` override with `dword_7C56A4 == 5`), and session state `dword_7C56A8`
  is 5, 6 or 8 [L6-L8].
- GUESS Used to decide whether a replay/camera path object should update: `AIRacingLinePathFollowing` calls it
  on a pointer derived from the vehicle owner before doing its render/path work; `LapSplitTimeEvent` and
  `sub_4BCCC0` also gate on it [callers].
- GUESS `this[12]` (0x30) is a view/target mode where 1 and 3 are special (e.g. cockpit vs replay) [L7].

## Questions

- What object is `this`? `AIRacingLinePathFollowing` passes `*(_DWORD *)this + 16436`, not the vehicle itself -
  confirm the receiver of this call and which object owns field 0x30.
- Meaning/values of globals `dword_7C569C`, `dword_7CEF84`, `dword_7C56A4`, `dword_7C56A8` (session type vs
  replay state).
- Why is `v1 != 3` tested both inside the first disjunct and again in the `dword_7CEF84` branch?
