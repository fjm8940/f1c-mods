# map_AICollision — declared `sub_4BEF60`

## Identity

- Address: `0x4BEF60`; file `functions/AICollision_004bef60.c`; 43 lines.
- Signature: `int __thiscall sub_4BEF60(int this, int a2, float a3)`.
- Callers (grep `functions/` for `AICollision(`): `EngineFailureCheck` (`0x4BEC55`) L41
  (`v10 = AICollision(a5, 1.0)`). TARGETS.md lists no mapped caller ("no mapped caller; explicit handoff
  target"); the grep caller above is the only occurrence in `functions/`.
- Callees (from `offsets_AICollision.csv` CALL rows): `DriverSetupParamLookupByID` (L27).
- `this` = AI/session controller (only bytes 0x340/0x341 and dwords 0x344/0x35C read). `a2` = vehicle
  object (fields 0x2100 array, 0x3D6C, 0x403C, 0x44D0, 0x44DC read). `a3` = float scale factor
  (caller passes 1.0).

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x40 | 64 | _BYTE | L18 | byte flag in the 4 x 296-byte records at `a2 + 2100` (`v6 + 64`) |
| 0x3D6C | 15724 | _DWORD | L24 | ? (compared with 2; also in `map_PhysicsGearShiftLogicWithThresholds.md`) |
| 0x403C | 16444 | _DWORD | L27 | lapNumber (inspect; `map_AIAbility.md`, `map_Overtaking.md`) |
| 0x340 | 832 | _BYTE | L28 | ? per-car flag A (enables first random roll) |
| 0x44D0 | 17616 | float | L30 | ? ability-like term (neighbors 17620/17624 RaceAbility per prior art) |
| 0x44DC | 17628 | float | L37 | ? grip/ability term (neighbors 17620/17624 RaceAbility per prior art) |
| 0x35C | 860 | int | L30, L37 | ? laps count (divisor; TotalRaceLaps in `map_AIOvertake.md` session use) |
| 0x341 | 833 | _BYTE | L35 | ? per-car flag B (enables second random roll) |
| 0x344 | 836 | float | L37 | ? scale for the second roll |

The 296-byte records at `a2 + 2100` are read as `*(_DWORD *)v6` (record present) and `*(_BYTE *)(v6 + 64)`
(flag); the array's own rows are not in the CSV beyond L18.

## Outputs

None. The function only reads and returns 0/1/2.

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 7 | L12 | `dword_7C56A4` (SessionType) gate - early return 0 |
| 2100, 296, 4, 64 | L15, L21-L23, L18 | 4 x 296-byte record array at `a2 + 2100`, flag at +64 |
| 15724 | L24 | `*(a2 + 15724) == 2` -> return 2 |
| 2 | L24, L27 | return value / compare |
| 3 | L26 | `dword_7CEA5C == 3` branch |
| -1 | L27 | `DriverSetupParamLookupByID(...) <= -1 ? 0 : 2` |
| 0.0099999998 | L30, L37 | percent-to-fraction scale (1/100) |
| 17616, 17628 | L30, L37 | vehicle ability fields |
| 832, 833, 836, 860 | L28-L37 | controller/vehicle fields |

## Logic

Direct `a2 + N` / `this + N` forms keep hex names (decimal present in cited lines); no scaled forms here.

```
v3 = 0                                                            [L11]
if (dword_7C56A4 != 7) return 0                                   [L12,L13]
v5 = 0; v6 = a2 + 2100                                            [L14,L15]
do {    [L16]
  if (!*(_DWORD *)v6 || !*(_BYTE *)(v6 + 64)) return 1            [L18,L19]
  ++v5; v6 += 296                                                 [L20,L21]
} while (v5 < 4)                                                  [L23]
if (*(a2 + 15724) == 2) return 2                                  [L24,L25]
if (dword_7CEA5C == 3)    [L26]
  return DriverSetupParamLookupByID(a2, *(a2 + 16444)) <= -1 ? 0 : 2   [L26,L27]
if (*(this + 832)) {                                              [L28]
  v7 = *(float *)(a2 + 17616) * 0.0099999998 / (double)*(int *)(this + 860) * a3   [L30]
  v8 = flt_8AAE74[(unsigned __int8)dword_8AAE70++]                [L31]
  if (v8 <= v7) v3 = 1                                            [L32,L33]
}    [L34]
if (*(this + 833) && v3 != 1) {                                   [L35]
  v9 = (1.0 - *(float *)(a2 + 17628) * 0.0099999998) / (double)*(int *)(this + 860) * a3 * *(float *)(this + 836)   [L37]
  v10 = flt_8AAE74[(unsigned __int8)dword_8AAE70++]               [L38]
  if (v10 <= v9) return 2                                         [L39,L40]
}    [L41]
return v3                                                         [L42]
```

## Meaning (GUESS)

- GUESS Session-gated collision/damage-likelihood roll: only in SessionType 7 (`dword_7C56A4 == 7`);
  otherwise returns 0 [L12-L13].
- GUESS First it validates four 296-byte records at `a2 + 2100`; a missing record or clear byte at +64
  returns 1 immediately (a "not applicable / damaged" state) [L17-L23].
- GUESS `*(a2 + 15724) == 2` and the `dword_7CEA5C == 3` branch are special race-state gates: the latter
  returns 2 unless `DriverSetupParamLookupByID(a2, lapNumber) <= -1`, i.e. driver setup lookup decides the
  outcome [L24-L27].
- GUESS Two independent random rolls driven by difficulty/ability fields: first uses `a2[0x44D0] / 100 /
  laps * a3`; when it fails, a second uses `(1 - a2[0x44DC]/100) / laps * a3 * this[0x344]`. Both draw
  from the shared LCG table `flt_8AAE74[dword_8AAE70++]` and return 1 or 2 on success [L30-L40].
- GUESS Returns are collision severities (0 = none, 1 = minor, 2 = major) consumed by `EngineFailureCheck`
  L41 as `v10 = AICollision(a5, 1.0)` followed by failure handling.
- GUESS The `this[0x340]`/`this[0x341]` bytes enable the two roll variants per car; `this[0x836]` biases
  the second (grip) variant [L28,L35,L37].

## Questions

- Caller `EngineFailureCheck` L41 shows `AICollision(a5, 1.0)` - two arguments for a 3-arg `__thiscall`;
  which argument is `this` (the controller at 0x340/0x341/0x344/0x35C) and which is `a2` (the vehicle)?
  The same caller computed `RandomVariationScaler(a3, ...)` with `a3 + 844`, suggesting a vehicle there.
- What are the 296-byte records at `a2 + 2100` (4 of them; likely per-wheel or per-driver entries)? The
  extractor only captured the byte at +64.
- What distinguishes return 1 from return 2 semantically? Both trigger `EngineFailureCheck` paths?
- Are 0x44D0 (17616) and 0x44DC (17628) the RaceAbility-neighbor fields, or different stats? Prior art
  (`map_AIAbility.md`) names 0x44D4/0x44D8 as the RaceAbility terms.
- `flt_8AAE74`/`dword_8AAE70` are the same shared random ring table used by `AIRingBufferFieldInit`; is the
  index collision-checked anywhere (unsigned byte wrap only)?
