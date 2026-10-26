# map_TireGripCoefficient — declared `sub_4E88B0` (Claude review GUESS name: `AIBrakeAmount`; old name kept)

## Identity

- Address: `0x4E88B0`; file `functions/TireGripCoefficient_004e88b0.c`; 117 lines.
- Signature: `double __thiscall sub_4E88B0(int this)`.
- Callers: `AIRacingLine` (`0x425920`) L343; `AISteeringCorrection` (`0x426CF0`) L101, L109; grep `functions/` for
  `TireGripCoefficient(` -> `functions/AIRacingLine_00425920.c`, `functions/AISteeringCorrection_00426cf0.c`;
  `inspect` agrees.
- Callees (from `offsets_TireGripCoefficient.csv` CALL rows): none.
- `this` = **vehicle + 0xC** (Claude review item 3; callers pass `this + 12`). All accesses are direct
  `*(float *)(this + N)` on that base; the vehicle-relative view below adds 0xC.
- The name is wrong (see Claude review notes); the old name is kept in the file name and callers.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x39B4 | 14772 | _DWORD | L22 | ? (mode selector, compared against 0/1/2) |
| 0x39C8 | 14792 | float | L30 | ? (shared term factor) |
| 0x39A8 | 14760 | float | L30 | ? (shared term factor) |
| 0x3900 | 14592 | float | L31 | ? (channel factor) |
| 0x38F8 | 14584 | float | L31 | ? (channel factor) |
| 0x38B4 | 14516 | float | L31 | ? (channel factor) |
| 0x3808 | 14344 | float | L32 | ? (channel factor) |
| 0x3800 | 14336 | float | L32 | ? (channel factor) |
| 0x37BC | 14268 | float | L32 | ? (channel factor) |
| 0x3708 | 14088 | float | L33 | ? (channel factor) |
| 0x36C4 | 14020 | float | L33 | ? (channel factor) |
| 0x3710 | 14096 | float | L33 | ? (channel factor) |
| 0x3610 | 13840 | float | L34 | ? (channel factor) |
| 0x35CC | 13772 | float | L34 | ? (channel factor) |
| 0x3618 | 13848 | float | L34 | ? (channel factor) |

All inputs are read-only. There are no pointer/dereference bases; every access is a direct `this + N` dereference.

## Vehicle-relative view (`this = veh + 0xC`; Claude review request)

Vehicle-relative byte offset = `N + 0xC`. Raw hex = offset on `this`.

| this hex | this dec | veh hex | veh dec | name |
|---|---|---|---|---|
| 0x39B4 | 14772 | 0x39C0 | 14784 | ? (mode selector 0/1/2) |
| 0x39C8 | 14792 | 0x39D4 | 14804 | ? (shared term factor) |
| 0x39A8 | 14760 | 0x39B4 | 14772 | ? (shared term factor) |
| 0x3900 | 14592 | 0x390C | 14604 | ? (channel factor) |
| 0x38F8 | 14584 | 0x3904 | 14596 | ? (channel factor) |
| 0x38B4 | 14516 | 0x38C0 | 14528 | ? (channel factor) |
| 0x3808 | 14344 | 0x3814 | 14356 | ? (channel factor) |
| 0x3800 | 14336 | 0x380C | 14348 | ? (channel factor) |
| 0x37BC | 14268 | 0x37C8 | 14280 | ? (channel factor) |
| 0x3708 | 14088 | 0x3714 | 14100 | ? (channel factor) |
| 0x36C4 | 14020 | 0x36D0 | 14032 | ? (channel factor) |
| 0x3710 | 14096 | 0x371C | 14108 | ? (channel factor) |
| 0x3610 | 13840 | 0x361C | 13852 | ? (channel factor) |
| 0x35CC | 13772 | 0x35D8 | 13784 | ? (channel factor) |
| 0x3618 | 13848 | 0x3624 | 13860 | ? (channel factor) |

## Outputs

None. The function returns a `double`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/TireGripCoefficient_004e88b0.c`.

| value | lines | note |
|---|---|---|
| 14772 | L22 | mode field index |
| 14792, 14760 | L30, L53, L82 | shared term indices |
| 14592, 14584, 14516 | L31, L54, L83 | channel indices |
| 14344, 14336, 14268 | L32, L55, L84 | channel indices |
| 14088, 14020, 14096 | L33, L56, L85 | channel indices |
| 13840, 13772, 13848 | L34, L57, L86 | channel indices |
| 0.5 | L30, L82 | shared term factor (mode 0 and 2) |
| 0.25 | L53 | shared term factor (mode 1) |
| 0.0 | L112, L113 | lower clamp |
| 1.0 | L114, L115 | upper clamp |

## Logic

NOTE: source uses byte-literal `*(float *)(this + N)` dereferences. Logic writes them as `this+N` hex names; each hex
token's decimal appears in the cited source line.

```
# mode dispatch
mode = *(int *)(this+0x39B4)                                          [L22]
if (mode == 0) goto CASE0                                             [L23,L80]
if (mode - 1 == 0) goto CASE1                                         [L25,L26,L52]
if (mode - 1 != 1) goto LABEL_23                                      [L28,L51]

# CASE2 (mode == 2)
w = *(this+0x39C8) * *(this+0x39A8) * 0.5                             [L30]
v18 = *(this+0x3900) * *(this+0x38F8) * *(this+0x38B4)                [L31]
v15 = *(this+0x3808) * *(this+0x3800) * *(this+0x37BC)                [L32]
v14 = (*(this+0x3708) * *(this+0x36C4) + w) * *(this+0x3710)          [L33]
result = (w + *(this+0x3610) * *(this+0x35CC)) * *(this+0x3618)       [L34]
v5 = max(result, v14)                                                 [L35,L36,L37,L38]
v6 = max(v15, v18)                                                    [L39,L40,L41,L42]
if (v5 > v6) goto LABEL_11                                            [L43,L44]
if (v15 > v18) { result = v15; goto LABEL_24 }                        [L45,L46,L47,L48]
goto LABEL_23                                                         [L50,L51]

# CASE1 (mode == 1)
w = *(this+0x39C8) * *(this+0x39A8) * 0.25                            [L53]
v14 = (*(this+0x38F8) * *(this+0x38B4) + w) * *(this+0x3900)          [L54]
v16 = (*(this+0x3800) * *(this+0x37BC) + w) * *(this+0x3808)          [L55]
v18 = (*(this+0x3708) * *(this+0x36C4) + w) * *(this+0x3710)          [L56]
result = (w + *(this+0x3610) * *(this+0x35CC)) * *(this+0x3618)       [L57]
v8 = max(result, v18)                                                 [L58,L59,L60,L61]
v9 = max(v16, v14)                                                    [L62,L63,L64,L65]
if (v8 > v9) { if (result > v18) goto LABEL_24; goto LABEL_23 }       [L66,L67,L68,L69,L70,L71]
if (v16 <= v14) { result = v14; goto LABEL_24 }                       [L72,L73,L74,L75,L76]
result = v16                                                          [L78]

# CASE0 (mode == 0)
w = *(this+0x39C8) * *(this+0x39A8) * 0.5                             [L82]
v14 = (*(this+0x38F8) * *(this+0x38B4) + w) * *(this+0x3900)          [L83]
result = (w + *(this+0x3800) * *(this+0x37BC)) * *(this+0x3808)       [L84]
v17 = *(this+0x3710) * *(this+0x3708) * *(this+0x36C4)                [L85]
v11 = *(this+0x3618) * *(this+0x3610) * *(this+0x35CC)                [L86]
v18 = v11                                                             [L87]
v12 = max(v11, v17)                                                   [L88,L89,L90,L91]
v13 = max(result, v14)                                                [L92,L93,L94,L95]
if (v12 <= v13) goto LABEL_11                                         [L96,L97,L98]

# shared labels
LABEL_11: if (result > v14) goto LABEL_24; goto LABEL_12              [L99,L100,L101]
LABEL_12: result = v14; goto LABEL_24                                 [L74,L75,L76]
LABEL_23: result = v18; goto LABEL_24                                 [L105,L106,L107]
LABEL_24: if (result < 0.0) return 0.0                                [L112,L113]
if (result > 1.0) return 1.0                                          [L114,L115]
return result                                                         [L116]
```

## Meaning (GUESS)

- GUESS Selects the maximum of four force-like products of the shape `(a*b (+ shared w)) * c`, where the shared term
  is `w = this[14792] * this[14760] * (0.5 or 0.25)` and `c`/`a`/`b` come from four fixed channel groups
  [L30-L34,L53-L57,L82-L86].
- GUESS The mode field `this+0x39B4` (0, 1, 2) selects which four channels are combined and the w factor: modes 0 and
  2 share the 0.5 shape, mode 1 uses 0.25 and a different channel ordering [L22-L110].
- GUESS Return is clamped to [0,1]; callers consume it as a brake-like amount (AIRacingLine writes it to 0x4438 at
  L343-L347; AISteeringCorrection uses 0.5x at L101 and writes 0x4438 at L109) [L112-L116; callers].
- GUESS Channel groups pair a "torque" factor with a "grip" factor and a scale (e.g. 0x3610 * 0x35CC * 0x3618); the
  four groups likely correspond to front/rear and left/right [L30-L86].
- GUESS The old name `TireGripCoefficient` is wrong (review): the value is the brake amount, not a grip coefficient.

## Claude review notes (2026-10-02, DERIVED — do not re-tier)

Source: `REVIEW_2_GROUP1.md` item 3.

- `TireGripCoefficient 0x4E88B0` (`this` = veh+0xC) is the **brake amount** written to vehicle 0x4438.
- Rename (GUESS name): `AIBrakeAmount`; keep the old `TireGripCoefficient` name in files/callers.

## Questions

- What do the mode values 0/1/2 mean (surface, compound, brake balance)? Two of the three branches are near-identical
  (0 and 2 differ only in channel ordering), which suggests a flag that is usually 0 or 2.
- Channel fields are unnamed; are the four groups wheel positions with 12-byte stride (0x3610, 0x3800, 0x38F8,
  0x39C8 groups)?
- `w` uses `this+0x39C8` and `this+0x39A8`; `this+0x39A8` is also BrakeThermalModel's `this[3690]` on the same
  veh+0xC base. Shared parameter block?
