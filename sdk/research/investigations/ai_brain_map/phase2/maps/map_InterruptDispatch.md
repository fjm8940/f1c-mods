# map_InterruptDispatch — `sub_58957A` (`InterruptDispatch`)

## Identity

- Address: `0x58957A`; file `functions/InterruptDispatch_0058957a.c`; 4 lines.
- Signature: `int __thiscall sub_58957A(void *this)`.
- Callers (grep `functions/` for `InterruptDispatch(`): 35 matches. Representative: `AISteeringRacingLine
  Correction` (`0x42B0E0`) L118; `AudioDSPStateUpdate` (`0x448330`) L19; `CollisionNoiseGenerator`
  (`0x4962C0`) L9; `D3DAnimationTimeUpdate` (`0x550310`) L35; `EulerAngleToRotationMatrix` (`0x53B240`)
  L13, L15, L17; `Gearbox` (`0x443B70`) L275; `PitTimer` (`0x4D2E50`) L264; `RandomColorGenerator`
  (`0x49B200`) L14; `RandomCollisionEvent` (`0x49E520`) L6; `RandomRotationMatrixFromEulerAngles`
  (`0x541DB0`) L19, L20, L21; `SteeringInputSmoothingWithSaturation` (`0x4569B0`) L18, L22;
  `sub_42BDD0` (`0x42BDD0`) L39, L75, L111; `sub_46B830` (`0x46B830`) L40; `sub_46BC20` (`0x46BC20`)
  L72, L103; `sub_4E43D0` (`0x4E43D0`) L59; `sub_4F0430` (`0x4F0430`) L301; `sub_4FCEE0` (`0x4FCEE0`)
  L137; `sub_4FD8B0` (`0x4FD8B0`) L126; `sub_54B450` (`0x54B450`) L3, L5, L7; `sub_54B4D0` (`0x54B4D0`)
  L6, L8, L10; `VehicleStateUpdate_19d0` (`0x4E19D0`) L217; `VehiclePhysicsIntegrationStep` (`0x441650`)
  L84.
- Callees (from `offsets_InterruptDispatch.csv` CALL rows): `Cintrindisp2` L3 (with `&unk_5E6550`).

## Inputs

None at the field level. The only input is the `this` pointer (passed straight through to `Cintrindisp2`).

## Outputs

None. The function returns the `Cintrindisp2` result.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/InterruptDispatch_0058957a.c`.

| value | lines | note |
|---|---|---|
| `&unk_5E6550` | L3 | global data object passed as the second argument (address, not a numeric literal) |

## Logic

```
return Cintrindisp2(this, &unk_5E6550)      [L3]
```

## Meaning (GUESS)

- GUESS A one-line thunk that forwards to a CRT/numeric dispatch routine `Cintrindisp2` bound to the
  global object `unk_5E6550` [L3].
- GUESS The phase-2 name `InterruptDispatch` comes from "Cintrindisp2" (C interrupt dispatch); the call
  sites do not support an actual hardware-interrupt role — it is called on floats (e.g.
  `PitTimer` L264 `InterruptDispatch(dbl_8AFEA0)`, result compared `> 0.5`) [callers].
- GUESS Likely a compiler/CRT helper (e.g. an x87 or integer conversion/emulation trampoline) that the
  decompiler turned into a `__thiscall void *` call because the real prototype is register-based
  [signature].
- GUESS `unk_5E6550` is a static dispatch/function table or state block for that helper [L3].
- GUESS Widespread use across audio, collision, camera and physics code indicates a generic runtime
  primitive, not game logic [callers].

## Questions

- What is `Cintrindisp2`? No map or doc name; `unk_5E6550` contents are unknown.
- Why do many call sites pass a float/double (e.g. `InterruptDispatch(dbl_8AFEA0)`, `InterruptDispatch(v9)`
  where `v9 = *((int *)this + 1) * 8.5`)? Either the prototype is wrong or the function is a
  NaN/float-format helper.
- `PitTimer` L264 uses the result in a float comparison (`v14 > 0.5` after
  `v14 = dbl_8AFEA0, InterruptDispatch(dbl_8AFEA0)`) — confirm what value is actually returned.
- Is `0x58957A` adjacent to `FloatFunctionPassThrough` (`0x5894A0`) / `sub_5894BD` part of one CRT helper
  cluster (0x5894xx-0x5895xx)? A cluster map would explain both stubs.
