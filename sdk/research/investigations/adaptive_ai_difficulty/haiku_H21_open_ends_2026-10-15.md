# H21: four open ends from the AI lap-time recipe (static, 2026-10-15)

Scope: read-only static reading of `functions/*.c` and `ask.py` output. No live reads, no writes
except this file. Tiers: FACT, DERIVED, OPEN. Cites are `functions/<file>:line` unless stated.

Cold start: README and governance read. Validator: 0 warnings, 2 pre-existing errors (`.claude`, `plans` top-level dirs), not touched.

Not done (read-only scope): append of section 7 to `findings.jsonl`; label renames (section 1d).


## 1. RaceQualRatio = flt_5EA3E8 (the 0.995 factor). DERIVED

FACT, addresses:
- `0x5EA3E8 - 0x5EA2C0 = 0x128 = 296`. Parser writes `this+296` for key `RaceQualRatio`
  (`sub_402690_00402690.c:248`). String `0x5BE5A8`.
- Same offsets for the other AIW values: WorstAdjust +284 (`0x5EA3DC`), MidAdjust +288, BestAdjust +292.
  GDS = this[70] = +280 = `0x5EA3D8` (flt_5EA3D8).
- `AIDatabase_Init_004040a0.c:29-36` writes this[71..73] = 0.5/1.0/1.3 (the Worst/Mid/Best defaults) and
  this[0]=4.75, this[1]=4.3125, this[87..90]=1/104/52/3.
- `FunctionCallWrapper_00401080.c:19-25` writes the same values to `dword_5EA2C0[0..1]` and
  `unk_5EA41C..5EA428`, plus `flt_5EA3E8 = 0.995` (line 21). It looks like the static pre-init of the same
  block. Callers: none recorded.
- Only writer: line 21 above. Only reader: `AIDriverStructInit_00424790.c:130`.

DERIVED chain: default 0.995 at startup; track load `AIDatabase_Init` (from `Game_Init` 0x42D6B0) calls
`sub_402690`, which overwrites the slot if the `.aiw` has `RaceQualRatio` (192 install files, range
0.985..1.0); session init multiplies by the slot unless `dword_7C56A4 in {3,4,5}` (line 129).

Answer: **yes**, the factor at `AIDriverStructInit:129-130` is RaceQualRatio. Nuance: the code does not
make it race-only. It is skipped in sessions 3, 4, 5 and applied in all others (sessions 0-2, 6, 8 not checked).

Corrections: H1 (`haiku_H1...:7, 54-56, 89`) says RaceQualRatio has no reader. Wrong: H1 searched struct
offsets and missed the global alias. `aad-aiw-adjust-no-runtime-reader-2026-10-15` inherits it. The label
`AIInitTractionControl` on flt_5EA3E8 (`skeletons_filled/AIDriverStructInit_00424790.md:131`, H9 C4) is wrong.

OPEN:
- (a) If a track's `.aiw` lacks the key, the slot keeps the previous value. Whether `FunctionCallWrapper`
  runs once or per track is not established.
- (b) `AIDatabase_Init`'s `this` argument at `Game_Init` not read. Base rests on arithmetic and init match.
- (c) `sub_565870` return semantics assumed (true = key matched, value stored). File not in corpus.
- (d) Naming conflict: `ask.py global 0x5EA2C0` says `GarageTableOwner [CONFIRMED]`, and
  `VEHICLE_LIFECYCLE_REVIEW_2026-09-08` says it is the garage scene object (table at +0x158). Both cannot
  be the same owner without checking. Resolve before relabelling.
- Live check (read-only): read `0x5EA3E8` after loading a track with RaceQualRatio 0.985, and one without.

## 2. Push scalar 0x44D8: AIAbility vs AIPushLap

Index: this[4405]=0x44D4, this[4406]=0x44D8, this[4413]=0x44F4, this[4401]=0x44C4, this[4418]=0x4508,
this[4419]=0x450C, this[4431]=0x453C (live pace), this[4111]=0x403C (lapNumber), this[4175]=0x413C, this[4353]=0x4404.

FACT: 0x44D8 is written at parse time by `sub_424080`. Live value-match 21/21:
`0x44D8 = 0.0268 + 0.006432 x RA = 0.67 x (0.04 + 0.01 x GDS x RA)` at GDS 0.96 (`ask.py offset 0x44D8`).
Baseline 0x44F4 = `(1 - RA/100) x GDS` (live).
FACT: `AIComputePassScore_0042a780.c:4` multiplies target speed by a1[4431]. So 0x453C is a **speed**
multiplier: higher = faster. Higher RA gives a lower multiplier (see sign note).

**AIAbility** (`AIAbility_00429470.c:36-72`, race only, dword_7C56A4 == 7):
- Called from `AIMainUpdate_00424e70.c:213-216` every 4 s sim time.
- `this[4418]` (0x4508) is incremented by `AISteering` (`AISteering_004270a0.c` lines 636, 639, 672, 1135,
  1204, 1248; trigger not traced) and zeroed by AIAbility each tick (line 70).
- Accumulator 0: walk 0x453C toward baseline by 0.01 per tick (lines 40-43).
- Accumulator nonzero: walk up toward the **push ceiling** `0x44D8 + 0x44F4` by 0.01 per tick (lines 45-50).
- `this[4419]` (0x450C), set by `Overtaking_0042af70.c:13-17` to max(other car's 0x44C4, own), gives a
  probability branch (lines 52-68): `(0x44D4 - 0.5 x 0x450C) x 100 >= rand%100` steps up, else down.
- So **in the race 0x44D8 is used** (H9 said the race is random walk only). The ceiling needs a traffic
  event in the last 4 s. A 0.09 push (RA 10) takes about 9 ticks, about 36 s. DERIVED.

**AIPushLap** (`AIPushLap_0042a180.c:14-31`, non-race only, role 1):
- `cap = this[4413] + this[4406]` (line 23) = `0x44F4 + 0x44D8`.
- `this[4431] = min(cap, this[4431] + 0.003 x min(lapNumber, 10))` (lines 24-31).
- Qualifying = 5, practice = 3, 4. Session 8 goes to `AIStateFlagSetter(1)` (AIMainUpdate:188).

**Does a slower driver push harder in qualifying?** DERIVED, yes, relative to race. Cap =
`GDS(1-RA/100) + 0.67(0.04 + 0.01 GDS RA)`; at GDS 0.96: `cap = 0.9868 - 0.00317 RA`; race = `0.96 - 0.0096 RA`.

| RA | race (0x44F4) | push (0x44D8) | quali cap | gain over race |
|---|---|---|---|---|
| 0 | 0.9600 | 0.0268 | 0.9868 | +2.8 % |
| 1 | 0.9504 | 0.0332 | 0.9836 | +3.5 % |
| 5 | 0.9120 | 0.0590 | 0.9710 | +6.5 % |
| 10 | 0.8640 | 0.0911 | 0.9551 | +10.5 % |

Push grows with RA, so high-RA (slower) drivers gain more. The RA 0-10 spread falls from 0.096 (race) to 0.032 (quali).
Sign note: assumes higher RA = lower speed multiplier (code plus H8, not checked live).

OPEN: trigger and rate of `AISteering`'s increments on 0x4508. Corpus globals list `AIQualifyPushFrequency
0x59E968` as read by AIAbility, but `functions/*.c` has no textual reference: treat as artifact until shown.


## 3. AIPushLap cadence and this[4111]

FACT gate: `AIMainUpdate_00424e70.c:185`: if sim time > this[4175], then line 187 sets this[4175] = FLT_MAX
(disarm), line 191 calls `AIPushLap` (unless session 8). Requires `sub_426430` true and role 1 (line 183).

FACT arm sources:
- A: `AIMainUpdate:67-71`: this[4175] = now + 2.0, in the session-change block when `this[17908]==1` and `byte_5C3160==1`.
- B: `sub_424CD0` case 1 (`AICommandMessageQueueDispatcher_00424cd0.c:19-20`): this[4175] = now + 2.0.
- Command 1 sent by `sub_4D3740` (`TimerFrameRateThrottle_004d3740.c:6-9`): after `LapSplitTimeEvent` it dispatches
  `FlagPropertyGetter(2048) + 1`. So command 1 fires when that flag is 0 at a timing event. Flag meaning OPEN.

DERIVED cadence: **once per arm**, event-driven. Not every frame, not per 4 s. Live rate OPEN.

FACT/CONFIRMED label: this[4111] = lapNumber (0x403C), incremented per car on a lap event (`AITickUpdate_004bf4f0.c:30`).
A completed-lap count (final-lap test `AIAbility:97` fits).

DERIVED effect of the lap term: this[4431] starts at 1.0 (`AIDriverStructInit:55`). Cap is about 0.95-0.99, so
the first AIPushLap call gives `cap` regardless of lap. The lap term matters only when this[4431] < cap, for
example after `AIAbility:92` (carConditions==1 sets 0.85 x baseline). Then it recovers at up to
`0.003 x min(lap,10)` per arm. H9's "+0.003 per call" is right about the call, not a per-lap ramp.


## 4. AI Additional Fuel Mult (PLR, default 0.95)

Answer: **no reader found.** Nothing traced multiplies AI fuel by it.

FACT option: key `0x5D0454` "AI Additional Fuel Mult"; description (launcher `options_menu.json:2495`):
"Additional fuel multiplier for AIs because of their driving style".
FACT registration: `PlayerOptionsInitializer_004aa270.c:1103`:
`MultiFieldSetterWithStringCopy(aAiAdditionalFu, 0.95f, 0.1f, 2.0f, aAdditionalFuel, 1)`
(1064514355 = 0x3F733333 = 0.95; 1036831949 = 0x3DCCCCCD = 0.1; 0x40000000 = 2.0).
FACT setter `MultiFieldSetterWithStringCopy_004b1d90.c:1-12`: this[6] = value (+0x18), this[7] = min (+0x1C),
this[8] = max (+0x20), this[5] = default (+0x14, flag 1). Receiver is implicit (ECX): record base in the options
object not recovered.
FACT: `.plr` value 0.95000 in 6 install files. No config-schema match; no code reads the key. Same state as AI Brake/Corner Grip Usage (no reader, CONFIRMED run 4).

FACT traced fuel path (does not use this option): `FuelMultiplierValue_0042ad20.c:3-6` returns floor(FuelLevel 0x3984 x LapsPerLitre 0x3448 - 0.2) when `dword_7D0B64` (PLR "Fuel Mult", `GameOptionsWriter_004c1e80.c:102`, copy into global not traced) is set, else 100. Fuel writers under the flag: `SuspensionAndWheelPhysics_004e5f60.c:562`, `TheSvmHdvVehicleFileParser_004f4270.c:558`. AI setup fuel from `.svm` (H4, DERIVED).

DERIVED: AI race fuel = svm fuel quantity x flag path. The 0.95 multiplier is in no traced fuel computation: either
a reader via the options record not found yet, or a dead option. Not decided.

Next tests: (1) find the reader via the options base and the receiver of line 1103. (2) Live read-only: AI `FuelLevel` (0x3984) at race start, key 0.95 vs 2.0.

## 5. Corrections and conflicts

| Item | Earlier | Corrected |
|---|---|---|
| RaceQualRatio reader | H1: none | `AIDriverStructInit:130` via alias flt_5EA3E8 |
| Slot label | AIInitTractionControl | RaceQualRatio slot (`0x5EA2C0+0x128`) |

## 6. Findings to append (proposed; not written)
RaceQualRatio-slot [DERIVED]; aiw-racequal-reader-correction [FACT]; push-ceiling-race-accumulator [DERIVED];
quali-cap-formula [DERIVED]; aipushlap-arm-cadence [DERIVED]; ai-additional-fuel-no-reader [OPEN].

## 7. Limitations

Static only. RA sign and push cadence need one live read. Not read: `sub_565870`, `AISteering` trigger, `FunctionCallWrapper` callers, `AIDatabase_Init` call-site argument, options record base.
Restoration: delete this file (only file changed).
