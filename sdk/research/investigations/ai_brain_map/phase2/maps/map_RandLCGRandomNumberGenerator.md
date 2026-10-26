# map_RandLCGRandomNumberGenerator — declared `sub_5884ED`

## Identity

- Address: `0x5884ED`; file `functions/RandLCGRandomNumberGenerator_005884ed.c`; 10 lines.
- Signature: `int sub_5884ED()` (no explicit arguments; state comes from a per-thread block).
- Callers (grep `functions/` for `RandLCGRandomNumberGenerator(`): `AIAbility` L54; `AIMainUpdate`
  L80/L89/L123/L125/L202; `AIOpponentSelection` L43; `AIPathPerformanceFunction` L100/L195;
  `CarCrashDamageProcessing` L24/L28/L65; `CloudSceneObjectRandomSelector` L34/L40;
  `DeterministicHashPseudoRandomGenerator` L26; `FileScanner` L11/L12; `FuelConsumptionAndWear` L267;
  `MemoryAllocation_9a50` L58; `ParticlePositionSetup` L70; `ParticleSoundEffectSpawner` L38; `PitExit`
  L100; `RaceEventMessageDispatcher` L6; `RandomNoiseTableInitialization` L20/L29;
  `RandomParameterDispatch` L6; `RandomPasswordKeyGenerator` L11; `RandomRangeTableInitializer` L46;
  `RandomStateMachineWithTimerAndTrigger` L16/L34; `RandomVariationScaler` L17/L28;
  `RandomWeightedTierSelection` L17/L41/L50; `ShortComputation_d360` L19; `SingletonInit` L13;
  `SoundEffectTrigger_89a0` L18; `SoundEffectTrigger` L29; `SoundEventDispatcher` L8/L9;
  `SteeringForceWithRandom` L32; `sub_40DE60` L36; `sub_424080` L62/L165/L182/L184; `sub_429960`
  L177/L186/L212; `sub_4A4110` L266; `sub_4B3400` L159; `sub_4BFF70` L121; `sub_4C83B0` L41;
  `sub_4D0110` L239/L289; `SuspensionAndWheelPhysics` L1282; `TelemetryDataDump` L110;
  `UtilityFunction_6010` L11 (39 files).
- Callees (from `offsets_RandLCGRandomNumberGenerator.csv` CALL rows): `TLSValueRetrievalWithInit`
  (`0x58E0C7`), `HIWORD` (16-bit shift macro / intrinsic).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x14 | 20 | _DWORD | L7 (`v0 + 20`) | LCG state word inside the block returned by TLSValueRetrievalWithInit |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x14 | 20 | _DWORD | L8 (`*(_DWORD *)(v0 + 20) = v1`) | LCG state word (advanced) |

## Constants

| value | lines | note |
|---|---|---|
| 214013 | L7 | LCG multiplier (MSVC `rand`) |
| 2531011 | L7 | LCG increment (MSVC `rand`) |
| 0x7FFF | L9 | 32767 result mask |
| 20 | L7, L8 | byte offset of the state inside the TLS block (0x14) |

## Logic

```
v0 = TLSValueRetrievalWithInit()                        [L6]
v1 = 214013 * *(_DWORD *)(v0 + 20) + 2531011            [L7]
*(_DWORD *)(v0 + 20) = v1                               [L8]
return HIWORD(v1) & 32767                               [L9]
```

## Meaning (GUESS)

- GUESS This is the MSVC CRT `rand()` linear congruential generator recompiled: state = state * 214013 +
  2531011, return `(state >> 16) & 0x7FFF` [L6-L9].
- GUESS The state is per-thread: `TLSValueRetrievalWithInit` returns a thread-local block and the state
  word is at block + 20 [L6-L8].
- GUESS Output range is 0..32767, so callers use `% N` for bounded random values (e.g. AI variation,
  damage, particles, sound) [L9; callers].
- GUESS The function is the single global game RNG used by many subsystems; no arguments means the state
  is fully implicit [L6].

## Questions

- Where is the seed written into the TLS block +20? No `srand` equivalent in the corpus front door.
- Is `HIWORD` a real call (macro artifact in the CSV) and does the shift use the 32-bit `v1` (result
  `(v1 >> 16) & 0x7FFF`)?
- Is `TLSValueRetrievalWithInit` really thread-local storage, or a lazily initialised singleton?
- Does the game use one shared RNG or one state per AI car / per subsystem?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
