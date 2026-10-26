# MF13 fixed-address inventory (Mechanical Failures engine, 2026-10-07)

Audit item MF-13 (`AUDIT_SAFETY_CAR_MECHANICAL_FAILURES_2026-10-07.md`): the aggregate
executable gate (`mf_engine_compat`) must not be read as general support for every fixed
address the engine family reads or writes. This inventory lists every fixed address literal in
`dll/engine_failure.c` and `dll/mf_hud.h` and states how each is covered.

Test: `tools/mf13_inventory_test.py` asserts the machine-readable set below equals the address
literals in the two source files, so a new hard-coded address fails the gate until this file is
updated. Addresses are the executable's 0x00xxxxxx-shaped absolute addresses.

## Machine-readable source address set

<!-- MF13-SOURCE-ADDRESSES:BEGIN -->
0X00411840
0X00424E70
0X00424E76
0X00426330
0X0043EF20
0X00447AB0
0X00447B10
0X00447B16
0X0049B7A0
0X0049B9A0
0X0049BA50
0X004CBC50
0X004EC1A0
0X004EF930
0X00541CD0
0X00541CD8
0X00566650
0X005BEBB0
0X005BEE08
0X005BF9A0
0X005C329C
0X005EA618
0X005EC000
0X005EC6D8
0X00640000
0X007C54DC
0X007C54E0
0X007C54E4
0X007C54E8
0X007C54F0
0X007C56A4
0X007C56A8
0X007C7428
0X007C7430
0X007C749F
0X007C74A4
0X007C75C4
0X007C75D4
0X007C75DC
0X007C8C50
0X007C8C58
0X007C9078
0X007C9081
0X007C9084
0X007C9094
0X007C90A4
0X007C90AC
0X007C90B4
0X007C90BC
0X007C9104
0X007C910C
0X007C9114
0X007C9124
0X007C9130
0X007C9174
0X007C917C
0X007C9184
0X007C918C
0X007C9194
0X007C919C
0X007C91A4
0X007C91AC
0X007C91B4
0X007C91BC
0X007C91C4
0X007C91D4
0X007C91DC
0X007C91E4
0X007C91EC
0X007C91F4
0X007C91FC
0X007C9204
0X007C920C
0X007C9214
0X007C921C
0X007CD204
0X007CE254
0X007CEA5C
0X007D0014
0X008AAE74
0X008AB6E4
0X008ACEA0
0X008ACF8C
0X008AFEA0
0X008AFEF0
<!-- MF13-SOURCE-ADDRESSES:END -->

## Inventory

| Address | Symbol | Kind | Access/purpose | Compatibility coverage |
|---|---|---|---|---|
| `0X00411840` | `VN_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00424E70` | `AI_PULLOFF_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00424E76` | `AI_PULLOFF_RESUME` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00426330` | `AI_GEAR_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X0043EF20` | `OVR_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00447AB0` | `TrigTableLookup` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00447B10` | `FUELCUT_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00447B16` | `FUELCUT_RESUME` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X0049B7A0` | `SHIFT_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X0049B9A0` | `LAMP_ENG_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X0049BA50` | `LAMP_FUEL_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X004CBC50` | `ParticleTrailEmitter` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X004EC1A0` | `IMP_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X004EF930` | `AI_GEAR_SHIFT_FN` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00541CD0` | `CAM_FUNNEL_SITE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00541CD8` | `CAM_FUNNEL_RESUME` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00566650` | `VN_STRCPY` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005BEBB0` | `CM_LOOKUP_PWS` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005BEE08` | `CM_STITCH_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005BF9A0` | `CM_EVENT_TABLE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005C329C` | `AI_ACTIVE_COUNT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005EA618` | `CM_CONTROLLER` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005EC000` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X005EC6D8` | `VEH_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X00640000` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C54DC` | `LAMP_VIEWED_SLOT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C54E0` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C54E4` | `LAMP_PLAYER_SLOT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C54E8` | `IMP_PLAYER_ID` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C54F0` | `AI_ACTIVE_PTR_ARRAY` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C56A4` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C56A8` | `RACE_PHASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C7428` | `PLAYER_PHYS_OBJ` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C7430` | `ENG_TORQUE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C749F` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C74A4` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C75C4` | `FUEL_NOW` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C75D4` | `FUEL_CONS` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C75DC` | `FUEL_MULT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C8C50` | `RAD_AIR_K0` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C8C58` | `RAD_AIR_K1` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9078` | `ENG_BRAKE_MAP` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9081` | `ENG_RUNNING` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9084` | `ENG_RPM` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9094` | `ENG_INERTIA` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C90A4` | `IDLE_THROTTLE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C90AC` | `IDLE_RPM_LO` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C90B4` | `IDLE_RPM_HI` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C90BC` | `REV_SHIFT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9104` | `ENG_REV_LIMIT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C910C` | `REV_LO` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9114` | `REV_HI` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9124` | `TQ_TABLE_PTR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9130` | `TQ_TABLE_COUNT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9174` | `OIL_TEMP_K` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C917C` | `OIL_LIMIT_K` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9184` | `COMB_HEAT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C918C` | `OIL_SPEED_HEAT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9194` | `OIL_MIN_COOL` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C919C` | `OIL_PRESS_NATIVE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91A4` | `OWT_K0` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91AC` | `OWT_K1` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91B4` | `WATER_K` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91BC` | `RAD_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91C4` | `RAD_AIR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91D4` | `ENG_STRESS` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91DC` | `ENG_LIFE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91E4` | `LT_RPM_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91EC` | `LT_RPM_INVR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91F4` | `LT_OIL_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C91FC` | `LT_OIL_INVR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9204` | (inline) | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C920C` | `ENG_BANK1` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C9214` | `ENG_BANK2` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007C921C` | `ENG_BANK3` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007CD204` | `HUD_OUTER_PTR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007CE254` | `UI_SCREEN_UP` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007CEA5C` | `PLR_FAILRATE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X007D0014` | `IMP_DMG_MULT` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008AAE74` | `MF_NOISE_TABLE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008AB6E4` | `REPLAY_MODE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008ACEA0` | `SPECIALFX_BASE` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008ACF8C` | `TOTAL_LAPS_PTR` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008AFEA0` | `SESSION_TIME` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |
| `0X008AFEF0` | `AMBIENT_C` | player-state read/write | player physics object/engine state (same base as PLAYER_PHYS_OBJ/VEH_BASE) | same static player object as PLAYER_PHYS_OBJ/VEH_BASE; wrong base fails the aggregate session-global plausibility (clock/type/phase) or yields out-of-range model inputs (fail-safe) |

Count: 85 fixed addresses.
