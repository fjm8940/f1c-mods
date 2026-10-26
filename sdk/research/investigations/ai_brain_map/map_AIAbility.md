# map_AIAbility — section-4 name `AIAbility`, declared `sub_429470`

## Identity

- Address: `0x429470`; file `functions/AIAbility_00429470.c`; 191 lines. File header comment: "Recalculates
  this[4431] (+0x453C) every frame. 3 write sites: +/-0.01 adjustment + probability override. External writes to
  this[4431] do NOT persist."
- Signature: `int __thiscall sub_429470(float *this)`; `this` = F1CVehicle (`this[N]` byte offset = `N * 4`).
- Caller: `AIMainUpdate` (`0x424E70`) L216; grep `functions/` -> `functions/AIMainUpdate_00424e70.c` only;
  `inspect` agrees (caller count 1).
- Callees (from `offsets_AIAbility.csv` CALL rows): `RingBufferAccessWithWrap`, `AIDifficultyScalar`,
  `RandLCGRandomNumberGenerator`, `ForcePitExit`, `ConditionalGate`, `PitStopDecisionTireCompound`,
  `SpeedValueSetterWithGlobalMirror`, `PostPitCleanup`, `ParameterValueSetter`, `AIClearStateFlags`, `sub_426430`,
  `FuelMultiplierValue`, `FuelWeightLimitCheck`, `RacePhasePitEntryTrigger` (`LODWORD` rows are macro artifacts).
- Prior art: `0x453C-recalc` (DERIVED) — 0x453C (this[4431]) recalculated every frame; external poke does not
  hold. `0x44D4-0x44D8-raceability-derived` (CONFIRMED) — 0x44D4 = `1 - 0.030455*RaceAbility`,
  0x44D8 = `0.0268 + 0.006432*RaceAbility`, both read here. `f1c-pit-exit-is-ai-command-15` (CONFIRMED) — the
  L136-L157 block replicates the command-15 pit release (0x4544=0, ParameterValueSetter(18) maxSpeed,
  0x4550=1, 0x4548=1). `ai-lateral-request-4420-zeroed-each-tick-2026-10-02` (FACT) — 0x4420 is context for the
  AIMainUpdate zeroing.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4044 | 16452 | float | L26 | ? ring-buffer selector (`this[4113]`) |
| 0x4508 | 17672 | float | L38 | ? timer / interval (`this[4418]`) |
| 0x44F4 | 17652 | float | L41 | ? ability floor (`this[4413]`) |
| 0x44D8 | 17624 | float | L46 | RaceAbility upper term (prior art) |
| 0x450C | 17676 | float | L52 | ? shared max speed (`this[4419]`) |
| 0x44D4 | 17620 | float | L54 | RaceAbility probability term (prior art) |
| 0x4064 | 16484 | _DWORD | L74 | ? state (`*((_DWORD *)this + 4121)`) |
| 0x3D6C | 15724 | _DWORD | L80 | carConditions (inspect) |
| 0x3984 | 14724 | float | L81 | fuelRateFactor (inspect) |
| 0x4000 | 16384 | float | L81 | ? (`this[4096]`) |
| 0x0CD4 | 3284 | _DWORD | L86 | ? sub-object field (`*(_DWORD *)this + 3284`) |
| 0x39FA | 14842 | _BYTE | L88 | ? flag (`*((_BYTE *)this + 14842)`) |
| 0x403C | 16444 | _DWORD | L88 | lapNumber (inspect) |
| 0x4040 | 16448 | _DWORD | L97 | ? (`*((_DWORD *)this + 4112)`) |
| 0x4440 | 17472 | float | L99 | Speed (doc 54) |
| 0x4354 | 17236 | _DWORD | L101 | current waypoint ptr (doc 54) |
| 0x4428 | 17448 | float | L103 | lateral position (doc 54) |
| 0x4068 | 16488 | _DWORD | L112 | ? (`*((_DWORD *)this + 4122)`) |
| 0x45E0 | 17888 | _DWORD | L124 | ? pit compound index (`*((_DWORD *)this + 4472)`) |
| 0x4594 | 17812 | float | L125 | ? compound speed table (`this[v11 + 4453]`) |
| 0x3988 | 14728 | float | L131 | ? speed cap (`this[3682]`) |
| 0x45F8 | 17912 | _DWORD | L174 | interaction target (inspect, doc 54) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4544 | 17732 | _DWORD | L27, L30, L77, L138 | ? pit/ability flag (1 then 0; prior art command-15) |
| 0x457C | 17788 | float | L31, L35, L78 | difficulty cap (`AIDifficultyScalar() * 0.75`) |
| 0x453C | 17724 | float | L51, L68, L92 | AI pace multiplier (prior art; file header) |
| 0x4508 | 17672 | float | L70 | timer / interval = 0 |
| 0x450C | 17676 | float | L71 | shared max speed = 0 |
| 0x4534 | 17716 | float | L102, L159 | ? `dbl_8AFEA0 + 120.0` / `+ 180.0` |
| 0x4420 | 17440 | float | L107 | lateral request (prior art) |
| 0x4579 | 17785 | _BYTE | L108, L122 | ? flag = 0 |
| 0x4068 | 16488 | _DWORD | L113 | ? = 6 |
| 0x45F4 | 17908 | _BYTE | L119 | ActiveFlag = 0 (inspect) |
| 0x4550 | 17744 | _DWORD | L140, L156 | laneMode = 1 (prior art) |
| 0x4548 | 17736 | _BYTE | L141 | ? override-enable byte = 1 (doc 54; AISteering reads it) |
| 0x443C | 17468 | float | L142 | ? one-shot request = 0 (prior art) |
| 0x34F8 | 13560 | float | L143 | velocity X = 0 (doc 54) |
| 0x3500 | 13568 | float | L144 | velocity Z = 0 (doc 54) |
| 0x34FC | 13564 | float | L145 | velocity Y = 0 (doc 54) |
| 0x43F8 | 17400 | float | L146 | ? limiting waypoint ptr = 0 (doc 54 profile) |
| 0x4510 | 17680 | _DWORD | L149, L151 | driverStatus = 4 (SessionType 7) else 5 (inspect) |
| 0x4524 | 17700 | float | L152 | ? = 0 |
| 0x4438 | 17464 | float | L153 | brake candidate = 1.0 (doc 54) |
| 0x4434 | 17460 | float | L154 | throttle candidate = 0 (doc 54) |
| 0x454C | 17740 | float | L155 | ? = 0 |
| 0x4424 | 17444 | float | L157 | hard lateral override = 0 (doc 54; AISteering epilogue) |
| 0x44B0 | 17584 | float | L189 | ? = NAN |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/AIAbility_00429470.c`.

| value | lines | note |
|---|---|---|
| 0.75 | L31, L78 | difficulty scalar scale |
| 0.0099999998 | L40, L47, L57, L64 | per-tick pace step |
| 0.5 | L54, L63 | RaceAbility term / floor factor |
| 100.0 | L54 | percentage scale |
| 0.85000002 | L92 | pace multiplier on carConditions 1 |
| 0.001 | L81 | fuelRateFactor floor |
| 5.0 | L99 | speed gate for pit servicing |
| 1.0 | L115, L127, L130, L133 | speed gate / compound default |
| 1065353216 | L135 | 1.0f bit pattern passed to SpeedValueSetter |
| 120.0, 180.0 | L102, L159 | time offsets added to `dbl_8AFEA0` |
| 6 | L113 | state written to 0x4068 |
| 2, 3 | L176, L180 | fuel multiplier bounds |
| 4, 5 | L149, L151 | driverStatus pit-wait values |
| -1 | L98 | lap/state compare |
| 2 | L112 | `*((_DWORD *)this + 4122)` compare |
| 14842, 14864, 14872 | — | see 0x39FA / other functions |

## Logic

NOTE: source uses scaled `this[N]` / `*((TYPE *)this + N)` indexing. Logic keeps source-literal indices; byte
offsets are in the tables above. No hex tokens are used on scaled lines. `[file]` cites the file header comment.

```
v18 = this[4113]                                                       [L26]
*((_DWORD *)this + 4433) = 1                                           [L27]
if (RingBufferAccessWithWrap(v18) == 2) {                              [L28]
    this[4433] = 0.0; this[4447] = AIDifficultyScalar() * 0.75         [L30,L31]
} else if (!RingBufferAccessWithWrap(this[4113]) || RingBufferAccessWithWrap(this[4113]) == 1) {  [L33]
    this[4447] = AIDifficultyScalar()                                  [L35]
    if (RacePhase == 7) {
        if (this[4418] == 0.0) { v3 = max(this[4431] - 0.01, this[4413]) }        [L38-L43]
        else { v3 = min(this[4406] + this[4413], this[4431] + 0.01) }             [L44-L50]
        this[4431] = v3                                                [L51]
        if (this[4419] != 0.0) {
            if ((this[4405] - this[4419]*0.5) * 100.0 >= rand()%100)
                v4 = min(this[4406] + this[4413], this[4431] + 0.01)   [L54-L60]
            else
                v4 = max(this[4413]*0.5, this[4431] - 0.01)            [L61-L67]
            this[4431] = v4                                            [L68]
        }
        this[4418] = 0.0; this[4419] = 0.0                             [L70,L71]
    }
}
if (*((_DWORD *)this + 4121) == 1) { this[4433] = 0.0; this[4447] = AIDifficultyScalar()*0.75 }   [L74-L78]
v6 = *((_DWORD *)this + 3931)                                          [L80]
if (v6 != 2 && this[3681] > 0.001 && this[4096] <= 0.0) {              [L81]
    if (v6 == 1) {
        if ((*this)[3284] && !(*this)[3348] || (*this)[3352] && !(*this)[3416]
            || this byte[14842] && *((_DWORD *)this + 4111) + 2 < TotalRaceLaps)
            ForcePitExit(this)                                         [L86-L91]
        this[4431] = this[4413] * 0.85000002                           [L92]
    }
    goto LABEL_61
}
if (RacePhase != 7 || *((_DWORD *)this + 4111) != TotalRaceLaps - 1
    || (*((_DWORD *)this + 4112) != 2 && != -1) || this[4368] < 5.0) { [L96-L99]
    v9 = *((_DWORD *)this + 4309)                                      [L101]
    this[4429] = dbl_8AFEA0 + 120.0                                    [L102]
    v10 = (this[4362] + v9[56] >= v9[52] - this[4362]) ? v9[56] : -v9[52]   [L103-L106]
    this[4360] = v10                                                   [L107]
    *((_BYTE *)this + 17785) = 0                                       [L108]
    if (RacePhase == 7) {
        ConditionalGate(this + 3)                                      [L111]
        if (this[4096] > 0.0 && !*((_DWORD *)this + 4122)) *((_DWORD *)this + 4122) = 6   [L112,L113]
    }
    if (this[4368] < 1.0) {
        if (RacePhase == 7) { *((_BYTE *)this + 17908) = 0; goto LABEL_61 }   [L117-L120]
        *((_BYTE *)this + 17785) = 0; PitStopDecisionTireCompound(this)       [L122,L123]
        v11 = 4 * *((_DWORD *)this + 4472); v12 = this[v11 + 4453]            [L124,L125]
        v14 = (v12 >= 1.0) ? v12 : 1.0                                        [L127-L130]
        v15 = (this[3682] >= v14) ? ((this[v11+4453] < 1.0) ? 1.0 : this[v11+4453]) : this[3682]   [L131-L167]
        SpeedValueSetterWithGlobalMirror(bits(v15))                        [L169]
LABEL_57:
        PostPitCleanup(this); this[4433] = 0.0                            [L137,L138]
        ParameterValueSetter(18)                                          [L139]
        *((_DWORD *)this + 4436) = 1; *((_BYTE *)this + 17736) = 1        [L140,L141]
        this[4367] = 0.0; this[3390] = 0.0; this[3392] = 0.0; this[3391] = 0.0   [L142-L145]
        this[4350] = 0.0; AIClearStateFlags(this)                         [L146,L147]
        *((_DWORD *)this + 4420) = (RacePhase == 7) ? 4 : 5               [L148-L151]
        this[4425] = 0.0; this[4366] = 1.0; this[4365] = 0.0              [L152-L154]
        this[4435] = 0.0; *((_DWORD *)this + 4436) = 1; this[4361] = 0.0  [L155-L157]
        sub_426430(this)                                                  [L158]
        this[4429] = dbl_8AFEA0 + 180.0                                   [L159]
        goto LABEL_61
    }
}
LABEL_61:
if (*((_DWORD *)this + 4478)) {                                       [L174]
    if (RacePhase == 7 && FuelMultiplierValue(this) < 2) {
        v16 = FuelMultiplierValue(*((_DWORD *)this + 4478))            [L178]
        if (v16 > 0 && v16 < 3 && !FuelWeightLimitCheck(*((_DWORD *)this + 4478))
            && !FuelWeightLimitCheck(this))
            ForcePitExit(this)                                          [L179-L185]
    }
}
result = RacePhasePitEntryTrigger(this); this[4396] = NAN; return result   [L188-L190]
```

## Meaning (GUESS)

- GUESS Per-tick AI ability update: the ring-buffer selector 0x4044 picks a difficulty expression written to
  0x457C and raises flag 0x4544; in SessionType 7 it adjusts pace multiplier 0x453C by ±0.01 per tick inside
  `[0x44F4, 0x44D8 + 0x44F4]`, with a probability override from 0x44D4/0x450C, then zeroes the timers
  0x4508/0x450C [L26-L72; prior art 0x453C-recalc].
- GUESS 0x44D4/0x44D8 are the per-driver ability terms derived from the .rcd RaceAbility attribute (prior art
  CONFIRMED), so the ±0.01 walk models slow form/pace drift while the bounds come from driver skill.
- GUESS carConditions 0x3D6C == 1 plus sub-object flags 3284/3348/3352/3416 or a late-race condition can trigger
  `ForcePitExit`, and resets pace multiplier to `0x44F4 * 0.85` — a "car problem -> retire to pits" path
  [L80-L94].
- GUESS When carConditions != 2, fuel rate factor > 0.001 and no pit request pending (0x4000 <= 0), it computes a
  lateral request `0x4420 = v9[56] or -v9[52]` around lateral position 0x4428 (v9 = waypoint object at 0x4354),
  then in SessionType 7 arms state 0x4068 = 6 when a pit request exists [L96-L114].
- GUESS The speed < 1.0 branch is a full pit-service release: it calls `PitStopDecisionTireCompound`, picks a
  compound speed from the table at `0x4594 + 4*index`, sets max speed, then performs the command-15 block
  (0x4544=0, maxSpeed 22.2, laneMode 0x4550=1, override byte 0x4548=1, clears requests 0x443C/0x43F8/0x4424,
  driverStatus 4 or 5) and re-runs `sub_426430` [L115-L160; prior art f1c-pit-exit-is-ai-command-15].
- GUESS Fuel check at the end: when the interaction target 0x45F8 exists, SessionType 7, fuel multiplier < 2 and
  both cars' fuel-weight checks fail, force a pit exit [L174-L187].
- GUESS The function always ends by evaluating the race-phase pit-entry trigger and marking 0x44B0 = NAN
  (a sentinel), so it is the per-tick "should this AI pit / how fast is this AI" updater [L188-L190].

## Questions

- What are 0x4508/0x450C/0x450C's counterpart 0x4508 called elsewhere (`this[4418]`, shared with
  AISteering's `this[4418]` corridor bound accumulator)? Name collision or same field?
- What exactly does `RingBufferAccessWithWrap(this[4113])` index (driver style, form history)?
- What is the v9 object at 0x4354 (doc 54 calls it current waypoint ptr) — the `+52/+56` fields used here look
  like left/right bounds, not waypoint geometry.
- Which write site wins for 0x453C in one tick when the ring-buffer branch, the `v6 == 1` branch and the
  SessionType-7 block all execute?
- 0x4424 is zeroed here [L157]; AISteering reads it as a hard lateral override and AIRacingLine writes it
  (wp+24 half/full). Confirm ordering with `ai-lateral-request-4420-zeroed-each-tick-2026-10-02`.
