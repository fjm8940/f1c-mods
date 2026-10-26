# map_OpponentProximityScanTeamRecognition — section-4 name, declared `sub_42A7D0`

## Identity

- Address: `0x42A7D0`; file `functions/OpponentProximityScanTeamRecognition_0042a7d0.c`; 247 lines.
- Signature: `char __thiscall sub_42A7D0(char *this)`.
- Callers: `AIMainUpdate` (`0x424E70`) L243; grep `functions/` for `OpponentProximityScanTeamRecognition(` ->
  `functions/AIMainUpdate_00424e70.c`; `inspect` agrees.
- Callees (from `offsets_OpponentProximityScanTeamRecognition.csv` CALL rows): `FindNearestVehicle`,
  `F_2DVectorDirectionProjection`, `sub_4C7BD0`.
- `this` = F1CVehicle. The function clears and fills five 68-byte proximity slots at 0x4174.., then runs team
  recognition over five 28-byte team records at 0x42CC...
- Field names marked "(inspect)" come from the navigation-only inspect view, not a tier review.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L59 | self vehicle pointer (FindNearestVehicle arg, inspect) |
| 0x34E0 | 13536 | float | L79, L92 | self world X (inspect) |
| 0x34E8 | 13544 | float | L80, L91 | self world Z (inspect) |
| 0x403C | 16444 | _DWORD | L88 | lapNumber (inspect, `this[4111]`) |
| 0x448C | 17548 | float | L91 | heading component (inspect) |
| 0x4494 | 17556 | float | L92 | heading component (inspect) |
| 0x4578 | 17784 | char | L232 | team-scan reset flag (inspect) |
| 0x41A8 | 16808 | float | L99 | slot 0 squared distance (also written) |
| 0x41EC | 16876 | float | L104 (slots shift) | slot 1 squared distance |
| 0x4230 | 16944 | float | L106 | slot 2 squared distance |
| 0x4274 | 17012 | float | L108 | slot 3 squared distance |
| 0x42B8 | 17080 | float | L110 | slot 4 squared distance |

Per-vehicle inputs scanned in the loop: `v7+13536`, `v7+13544`, `v7+17744`, `v7+16444`, and `v7+18580` (team id).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4174 | 16756 | _DWORD | L63, L173 | slot 0 vehicle ptr (or cleared 0) |
| 0x41A8 | 16808 | float | L62, L172 | slot 0 squared distance (999999.0 sentinel) |
| 0x41B8 | 16824 | _DWORD | L152 | slot 1 vehicle ptr |
| 0x41EC | 16876 | float | L151 | slot 1 squared distance |
| 0x41FC | 16892 | _DWORD | L135, L150 | slot 2 vehicle ptr |
| 0x4230 | 16944 | float | L134, L149 | slot 2 squared distance |
| 0x4240 | 16960 | _DWORD | L133, L148 | slot 3 vehicle ptr |
| 0x4274 | 17012 | float | L132, L146 | slot 3 squared distance |
| 0x4284 | 17028 | _DWORD | L113, L120 | slot 4 vehicle ptr |
| 0x42B8 | 17080 | float | L112, L119 | slot 4 squared distance |
| 0x42CC | 17100 | _DWORD | L51, L195 | team record flag (1 = active/team match) |
| 0x42C8 | 17096 | _DWORD | L221 | team record field (-1) |
| 0x42D0 | 17104 | _DWORD | L223 | team id |
| 0x42D8 | 17112 | _DWORD | L220 | team record field (-9999.0) |
| 0x42DC | 17116 | _DWORD | L222, L240 | team threshold (0, then 500.0) |
| 0x45FC | 17916 | _DWORD | L90 | interaction target (prior art) |
| 0x4420 | 17440 | _DWORD | L93, L95 | side bias -2.5 / +2.5 (inspect) |

Slot `+64` (byte offset 16820, the team index / -1 sentinel) is written through the local `v2 = this + 16820`
(L45, L52, L225); it has no row in `offsets_OpponentProximityScanTeamRecognition.csv` because the write goes
through the local, and it is cleared in the L50-L53 init block (see Logic).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/OpponentProximityScanTeamRecognition_0042a7d0.c`.

| value | lines | note |
|---|---|---|
| 5 | L47, L181, L204, L237 | slot / team record count |
| 1232348144 | L50 | float bit pattern = 999999.0 (distance sentinel) |
| 1065353216 | L62 | float bit pattern = 1.0 (nearest distance fast path) |
| 625.0 | L85 | 25 m squared gate for qualifying side-bias block |
| -1071644672 | L93 | float bit pattern = -2.5 (side bias) |
| 1075838976 | L95 | float bit pattern = 2.5 (side bias) |
| 999998.0 | L184, L207 | slot distance validity bound |
| -971228160 | L220 | float bit pattern = -9999.0 (new team record) |
| 1140457472 | L240 | float bit pattern = 500.0 (team threshold reset) |
| 18580, 16484, 16444, 17744 | L59-L240 | scanned vehicle offsets |
| 68, 28, 17, 7 | L199, L219, L228, L241 | slot / record strides (bytes) |

## Logic

NOTE: source uses scaled `this[N]` and local-pointer forms; the Logic keeps source-literal indices and local names
(`v2` = this+16820, `v3` = this+17100, `v28` = this+16756, `v32` = this+16820, `v37`/`v38` = this+28*n). Byte
offsets are in the tables above. No hex tokens are used in this section.

```
# clear 5 proximity slots and 5 team records
do {
    v2[-3] = 999999.0; v3[0] = 0; v2[0] = -1; v2[-16] = 0    [L50,L51,L52,L53]
    v3 += 7; v2 += 17; --v4                                   [L54,L55,L56,L57,L58]
} while (v4)

# fast path
v5 = FindNearestVehicle(this[1])                              [L59]
if (v5) { this[4202] = 1.0; this[4189] = *v5 }                [L60,L62,L63]
else
    # full scan over all vehicles
    for (i = 0; i < VehicleCount; ++i) {                      [L67]
        v7 = vehicles[i]
        if (this[0] == v7) continue                           [L69,L70]
        rel = (v7[13536] - this[3384], v7[13544] - this[3386])   [L72,L73,L74,L79,L80]
        v41 = relZ * relZ + relX * relX                       [L82]
        if (v7[17744] >= dword_5EA2CC) continue               [L75,L77]
        if (sub_4C7BD0(v7)) {                                 [L83]
            if (v41 < 625.0 && SessionType == 7) {            [L85]
                if (v7[16444] > this[4111]) {                 [L88]
                    this[4479] = v7                           [L90]
                    if (cross(v7 - self, this[4387]/[4389]) <= 0.0) this[4360] = -2.5
                    else this[4360] = 2.5                     [L91,L92,L93,L95]
                }
            }
        }
        if (v41 >= this[4270] || F_2DVectorDirectionProjection(this+4387, rel) >= -5.5) {   [L99,L100]
            # insertion: keep 5 nearest; shift lower entries up
            if (v41 >= this[4202]) {                          [L102]
                if (v41 >= this[4219]) {                      [L104]
                    if (v41 >= this[4236]) {                  [L106]
                        if (v41 >= this[4253]) {              [L108]
                            if (v41 < this[4270]) { this[4270] = v41; this[4257] = v7 }   [L110,L112,L113]
                        } else { this[4270]=this[4253]; this[4257]=this[4240];
                                 this[4253]=v41; this[4240]=v7 }  [L118,L119,L120,L121,L122]
                    } else { shift 4<-3<-2; this[4236]=v41; this[4223]=v7 }   [L127,L128,L129,L130,L131,L132,L133,L134,L135]
                } else { shift 4<-3<-2<-1; this[4219]=v41; this[4206]=v7 }    [L140-L152]
            } else { shift 4<-3<-2<-1<-0; this[4202]=v41; this[4189]=v7 }     [L157-L173]
        }
    }

# team recognition over the filled slots
v28 = this + 16756; repeat 5                                   [L180,L181,L182]
    if (*v28 && *(float *)(v28 + 52) < 999998.0) {             [L184]
        v30 = 0; v31 = this + 17104                            [L186,L187]
        while (*(_DWORD *)(*v28 + 18580) != *v31) {            [L188]
            ++v30; v31 += 7; if (v30 >= 5) goto NEXT           [L190,L191,L192,L193]
        }
        this[28 * v30 + 17100] = 1                             [L195]
        *((_DWORD *)v28 + 16) = v30                            [L196]
    }
NEXT: v28 += 68; --v29                                         [L199,L200]
until v29 == 0

# queue slots with no team match into a free team record
v32 = this + 16820; repeat 5                                   [L203,L204,205]
    if (*(float *)(v32 - 12) <= 999998.0 && *v32 == -1) {      [L207]
        v34 = *(_DWORD *)(*(v32 - 16) + 18580)                 [L209]
        v35 = 0; v36 = this + 17100                            [L210,L211]
        while (*v36) { ++v35; v36 += 7; if (v35 >= 5) goto NEXT2 }   [L212,L213,L214,L215,L216,L217]
        v37 = this + 28 * v35                                  [L219]
        v37[4278] = -9999.0; v37[4274] = -1; v37[4279] = 0;
        v37[4276] = v34; v37[4275] = 1                         [L220,L221,L222,L223,L224]
        *v32 = v35                                             [L225]
    }
NEXT2: v32 += 17; --v33                                        [L228,L229]
until v33 == 0

# queued-threshold reset
if (this[17784]) {                                             [L232,L233]
    this[17784] = 0                                            [L235]
    v38 = this + 17116; repeat 5 { *v38 = 500.0; v38 += 7 }    [L236,L237,L238,L239,L240,L241,L242,L243,L244]
}
return (char)v38                                               [L246]
```

## Meaning (GUESS)

- GUESS Per-tick traffic scan: clears 5 proximity slots and 5 team records, then keeps the 5 nearest other
  vehicles by squared distance using an insertion sort, gated by a forward-projection test (`>= -5.5`) and a
  per-car distance filter (`v7+17744 < dword_5EA2CC`) [L45-L179].
- GUESS Slot layout: 5 entries of 68 bytes; vehicle pointer at +0x00, squared distance at +0x34, team index at
  +0x40; team records are 5 entries of 28 bytes at 0x42CC (flag), 0x42D0 (team id), 0x42D8/0x42DC [L50-L53,
  L99-L173, L195,L196, L219-L225].
- GUESS In SessionType 7 only, a car inside 25 m (625 = 25^2) with a higher lap counter gets recorded as
  interaction target 0x45FC and a side bias of +-2.5 into 0x4420 [L83-L95; prior art
  `S1-ai-interaction-target-not-overtake`].
- GUESS Team recognition matches the slot car's team id (vehicle+0x4894) against the team-id list at 0x42D0; on a
  match it sets the flag at 0x42CC and stores the team index in the slot (+0x40, 0x41B4) [L180-L202].
- GUESS Slots with no team match are queued into a free team record (flag 1, id, -1, -9999.0, threshold 0)
  [L203-L231].
- GUESS When 0x4578 is set, all five team thresholds 0x42DC are reset to 500.0 and the flag clears [L232-L244].

## Questions

- What is `v7+17744` (compared against `dword_5EA2CC`, AiOvertakeDistanceThreshold in docs/12) and why does the
  whole scan sit behind it?
- Meaning of the `>= -5.5` projection gate and the 625.0 (25 m) qualifying gate needs live confirmation.
- Who consumes the team flags 0x42CC / team ids 0x42D0 / thresholds 0x42DC (AISteering reads nearby fields; map
  cross-reference needed)?
- Does the fast path `FindNearestVehicle` set only slot 0, leaving the other four slots for the next scan tick?
