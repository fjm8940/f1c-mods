# map_sub_4C7BD0 — `sub_4C7BD0`

## Identity

- Address: `0x4C7BD0`; file `functions/sub_4C7BD0_004c7bd0.c`; 4 lines.
- Signature: `bool __thiscall sub_4C7BD0(_DWORD *this)`.
- Callers (grep `functions/` for `sub_4C7BD0(`): `AIVehicleDamageEvent` (`0x41A170`) L8;
  `OpponentProximityScanTeamRecognition` (`0x42A7D0`) L83; `SuspensionForceComputationWithPerCornerVirtualDi`
  (`0x4F8440`) L56 (`sub_4C7BD0(*this)`).
- Callees (from `offsets_sub_4C7BD0.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4894 | 18580 | _DWORD | L3 (`this[4645]`) | ? team/driver identity field (compared with the reference vehicle) |

Global reads: `dword_7C54F0` (slot vehicle array) at index `dword_7C54E4`.

## Outputs

None. The function returns a `bool`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/sub_4C7BD0_004c7bd0.c`.

| value | lines | note |
|---|---|---|
| 4645 | L3 | `this[N]` index (byte 18580, hex 0x4894) |
| 18580 | L3 | offset on the reference slot vehicle (0x4894) |
| 0, 1 | - | none in source |

No integer bit-pattern is written into a float field.

## Logic

```
return this[4645] == *(_DWORD *)(dword_7C54F0[dword_7C54E4] + 0x4894)     [L3]
```

## Meaning (GUESS)

- GUESS Compares vehicle field 0x4894 with the same field of the reference vehicle `dword_7C54F0[dword_7C54E4]`
  - a team/driver/setup identity check ("is this car on the reference team?") [L3].
- GUESS Consumer semantics: `OpponentProximityScanTeamRecognition` L83 applies team handling only when true;
  `AIVehicleDamageEvent` L8 picks driver-table entry 52 vs 196; `SuspensionForceComputationWithPerCornerVirtualDi`
  L56 selects between two tire-force curves [callers].
- GUESS The reference index `dword_7C54E4` is the player/observed vehicle slot, and `dword_7C54F0` is the slot
  vehicle array (also used by `FindNearestVehicle`, phase-2 map) [L3].
- GUESS Neighbouring field 0x48A4 is named TeamIndex in doc 54 mode 3; 0x4894 may be the team id itself or a
  car/driver id - see Questions.

## Questions

- Exact semantics of 0x4894 vs 0x48A4 (TeamIndex?) - both are on the same object or on `*(vehicle)` subobjects?
  Confirm with a live read or a writer search.
- What selects `dword_7C54E4`; is the "reference" always the player car?
- Does equality mean "same team", "same driver copy" or "same model"? The three consumers behave differently.
