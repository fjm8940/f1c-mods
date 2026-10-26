# map_AIOvertake — section-4 name `AIOvertake`, declared `sub_4BEB10` (typed export says `SessionSimulationStep`)

## Identity

- Address: `0x4BEB10`; file `functions/AIOvertake_004beb10.c`; 42 lines.
- Signature: `void __thiscall sub_4BEB10(int this, _BYTE *a2, int a3)`.
- Callers: `sub_46BF10` (`0x46BF10`) L24 (`while ( AIOvertake(&v5, LODWORD(v3)) )`); grep `functions/` for
  `AIOvertake(` -> `functions/sub_46BF10_0046bf10.c`; `inspect` agrees (caller `0x0046BF10`).
- Callees (from `offsets_AIOvertake.csv` CALL rows): `AIBehaviorInitialization` (`0x4BF0A0`).
- **Not AI driving** — session-object step only, not a vehicle-AI function; excluded from the B5 brain field
  glossary (Claude review `REVIEW_3_GROUP2.md` item 2).
- **Name warning:** the section-4 file name `AIOvertake` conflicts with the typed export
  `functions_typed_20260930/SessionSimulationStep_004beb10.c` and finding
  `function-guide-stale-prose-conflicts-20261001` (filled 004BEB10 overtaking prose vs SessionSimulationStep). The
  body is a session-object step (offsets 0x342..0x360), not vehicle AI. Kept here as `AIOvertake` per the handoff
  file list; meaning stays GUESS.
- `this` = session object (not a vehicle); `a2` = byte output flag; `a3` = argument passed through to
  `AIBehaviorInitialization`.
- The decompile ends in three `JUMPOUT` tails (0x4BEC10, 0x4BEEF2, 0x4BEF10); their bodies are not in this file.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x358 | 856 | float | L10 | ? time threshold (compared with 0x354) |
| 0x354 | 852 | float | L16, L34 | ? accumulated time |
| 0x342 | 834 | _BYTE | L32 | ? activation flag (calls init when clear) |
| 0x350 | 848 | float | L34 | ? time increment |

Globals read: `dword_8ACF8C` TotalRaceLaps [L11], `dword_5C329C` VehicleCount [L12], `dword_7C56A4` SessionType
[L14, L18], `dword_7C54F0` VehiclePtrArray (scan) [L22].

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x35C | 860 | _DWORD | L11 | ? TotalRaceLaps snapshot |
| 0x360 | 864 | _DWORD | L14 | ? SessionType snapshot |
| 0x354 | 852 | float | L34 | ? accumulated time (`+= 0x350`) |

Also writes `*a2 = 0` [L9] (caller output byte).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/AIOvertake_004beb10.c`.

| value | lines | note |
|---|---|---|
| 7 | L18 | SessionType compare |
| 16484 | L22 | other-object state field (`0x4064`) |
| 0.0 | L16 | threshold compare |
| 856, 860, 864, 852, 834, 848 | L10-L34 | `this` byte offsets (hex 0x358, 0x35C, 0x360, 0x354, 0x342, 0x350) |

## Logic

NOTE: source uses byte-literal `*(TYPE *)(this + N)` dereferences; Logic writes them as hex names. Each hex token's
decimal appears in the cited source line.

```
*a2 = 0                                                              [L9]
v4 = *(float *)(this+0x358)                                          [L10]
*(_DWORD *)(this+0x35C) = TotalRaceLaps                              [L11]
v5 = VehicleCount                                                    [L12]
v7 = 1                                                               [L13]
*(_DWORD *)(this+0x360) = SessionType                                [L14]
v8 = 0                                                               [L15]
if (v4 <= 0.0 || *(float *)(this+0x354) < (double)v4) {              [L16]
    if (SessionType == 7) {                                          [L18]
        for (i = 0; i < v5; ++i)                                     [L20]
            if (*(_DWORD *)(vehicles[i] + 16484) == 1) v8 = 1        [L22,L23]
    }
}
else { v8 = 1; v7 = 0 }                                              [L29,L30]
if (!*(_BYTE *)(this+0x342)) AIBehaviorInitialization(a3, v8)        [L32,L33]
*(float *)(this+0x354) = *(float *)(this+0x354) + *(float *)(this+0x350)   [L34]
if (v7) { if (v5 > 0) JUMP tail A; else JUMP tail B }              [L35,L36,L37,L38,L39]
JUMP tail C                                                          [L41]
```

## Meaning (GUESS)

- GUESS Session/behavior step: latches TotalRaceLaps into 0x35C and SessionType into 0x360, then accumulates a
  timer 0x354 += 0x350 [L11-L34].
- GUESS `v7` (the "keep running" flag) is 1 while the accumulated time is below threshold 0x358 (or the threshold
  is <= 0); once `0x354 >= 0x358`, `v7 = 0` and the function jumps to the 0x4BEF10 continuation [L16,L29,L30,L35-L41].
- GUESS In SessionType 7 it scans all vehicles for a state field `+16484 == 1` and passes that as `v8` (human car
  present?) to `AIBehaviorInitialization` when flag 0x342 is clear [L18-L33].
- GUESS Despite the `AIOvertake` file name, this is not vehicle overtaking logic; the typed export name is
  SessionSimulationStep [identity warning].

## Questions

- JUMPOUT targets 0x4BEC10 / 0x4BEEF2 / 0x4BEF10 are not present in this decompile; their bodies need a separate
  export before this function can be mapped fully.
- What is the session object? Which caller (sub_46BF10) session phase calls it, and what does it do with `a2`?
- What are 0x342, 0x350, 0x354, 0x358, 0x35C, 0x360 semantically (timing vs state snapshots)?
- What does vehicle `+16484` state == 1 mean (human vs AI vs replay)?
