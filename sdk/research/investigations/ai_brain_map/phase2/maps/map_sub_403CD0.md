# map_sub_403CD0 — `sub_403CD0`

## Identity

- Address: `0x403CD0`; file `functions/sub_403CD0_00403cd0.c`; 97 lines.
- Signature: `_DWORD *__thiscall sub_403CD0(_DWORD *this, int a2, float *a3, _DWORD *a4)`.
- Callers (grep `functions/` for `sub_403CD0(`): `AISteeringRacingLineCorrection` (`0x42B0E0`) L169, L200,
  L204; `SingleFunctionDispatch_10e0` (`0x4D10E0`) L3 (`return sub_403CD0(dword_5EA2C0, *(_DWORD *)(a1 + 16472) - 1, a2, a3);`);
  `SpecialFX_Init` (`0x4CF070`) L480; `SteeringInterpolationCameraFollow` (`0x434D60`) L88, L94;
  `TrackSplineExtrapolation` (`0x4349A0`) L71, L72. First argument is `dword_5EA2C0` (track spatial data).
- Callees (from `offsets_sub_403CD0.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x150 | 336 | _DWORD | L21, L59, L78, L90 (`this[84]`) | ? track node array base |
| 0x4 | 4 | float | L23, L33 (`v5[1]`, `v7[1]`) | ? node field 1 (zero-tested) |
| 0x8 | 8 | float | L23, L35 | ? node field 2 (zero-tested) |
| 0xC | 12 | float | L23, L37 | ? node field 3 (zero-tested) |
| 0x10 | 16 | float | L23, L39 | ? node field 4 (zero-tested) |
| 0x14 | 20 | float | L23, L41 | ? node field 5 (zero-tested) |
| (dynamic) `v4[7*v6 - 14]` | -56 | float | L49 (`v8 = &v4[7 * v6 - 14]`, `*v8 == 0.0`) | ? previous-previous node field 0 (negative index; no hex, dec -56) |

Each node is 7 floats (28 bytes): field 0 at +0 and fields 1..5 at +4..+20 (all six zero-tested).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | float/_DWORD | L57 (`a3[1] = v4[1]`) | output slot 1 (position/orientation) |
| 0x8 | 8 | float/_DWORD | L58 (`a3[2] = v4[2]`) | output slot 2 |

Outputs also write slot 0 of both buffers (`a3[0]`, `a4[0]`) at L56, L60, L74, L80, L87, L91, L92; the extractor
does not emit offset-0 rows.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/sub_403CD0_00403cd0.c`.

| value | lines | note |
|---|---|---|
| 84 | L21, L59, L78, L90 | `this[N]` index (0x150) node array base |
| 7 | L22, L28, L49, L68-L73 | floats per node (28 bytes) |
| 2 | L25, L43 | backward search step / `v6 < 2` |
| 14 | L44, L49 | previous-previous node stride (2 nodes) |
| 28 | L78, L90 | byte stride per node index (`28 * v6`, `28 * a2`) |
| 12 | L59, L78, L90 | orientation/aux sub-block inside the node (+12 bytes) |
| 0.0 | L23, L31-L41, L49-L54 | empty-node tests |
| 0/1 | L67 (implicit 2), L48 (`v6 < 2`) | - |

No integer bit-pattern is written into a float field (outputs are float copies).

## Logic

```
v4 = (float *)this[84]                                                 [L21]
v5 = &v4[7 * a2]                                                       [L22]
if (node v5 fields 0..5 all == 0.0) {                                  [L23]
  v6 = a2 - 2                                                          [L25]
  if (v6 >= 0) {                                                       [L26]
    v7 = &v4[7 * v6]
    do { if (v7[0] != 0) break; ... if (v7[5] != 0) break;
         v6 -= 2; v7 -= 14 } while (v6 >= 0)                           [L29,L30,L31,L32,L33,L34,L35,L36,L37,L38,L39,L40,L41,L42,L43,L44,L45,L46]
  }
  if (v6 < 2 || (v8 = &v4[7*v6 - 14], all six fields == 0.0)) {        [L48,L49-L54]
    a3[0..2] = v4[0..2]                                                [L56,L57,L58]
    v13 = this[84] + 12; a4[0..2] = v13[0..2]                          [L59,L60,L61,L62,L63]
  } else {
    v9 = (a2 - v6) / 2                                                 [L67]
    a3[0] = (v4[7*v6+0]-*v8)*v9 + v4[7*v6+0]                           [L69,L71]
    a3[1] = (v4[7*v6+1]-v8[1])*v9 + v4[7*v6+1]                         [L72,L75]
    a3[2] = (v4[7*v6+2]-v8[2])*v9 + v4[7*v6+2]                         [L68,L73,L77]
    v11 = 28*v6 + this[84] + 12; a4[0..2] = v11[0..2]                  [L78,L80,L81,L82]
  }
} else {
  a3[0..2] = v5[0..2]                                                  [L87,L88,L89]
  v14 = 28*a2 + this[84] + 12; a4[0..2] = v14[0..2]                    [L90,L91,L92,L93,L94]
}
return result                                                          [L96]
```

## Meaning (GUESS)

- GUESS Returns a track-node sample at index `a2` from the array `this[84]` (0x150), stride 28 bytes (7 floats):
  first 3 floats = position, +12 bytes = an orientation/aux triple [L21, L56-L63, L87-L94].
- GUESS When the requested node is all-zero (empty), it walks backwards by 2 to the nearest non-empty node and
  linearly interpolates between node `v6` and `v6-2` (`v9 = (a2-v6)/2`) to fill the gap in the sampled line
  [L23-L78].
- GUESS If no non-empty node is found (`v6 < 2` or the predecessor is empty too), it falls back to node 0
  [L48-L63].
- GUESS Callers: `SingleFunctionDispatch_10e0` passes `*(a1 + 16472) - 1` (current waypoint index - 1);
  `TrackSplineExtrapolation` samples consecutive indices; camera/steering interpolate the line
  [callers].

## Questions

- CSV row for `v4[7*v6 - 14]` has dec -56 with an empty hex cell (negative index; it is a previous-node
  field 0, not a byte offset) - confirm the extractor limitation.
- What are node fields 3..5 (+12..+20)? They are only zero-tested here, so they may be flags or padding.
- `a4` is `_DWORD *`; is +12 packed IDs or floats (orientation)?
- What is the array length behind `this[84]` and what writes indices into it (waypoint data?)
- `v9 = (double)((a2 - v6) / 2)` is integer division before float promotion in the decompile; confirm intent.
