# Restart fuel and penalty mode review — 2026-09-21

Static ABI and isolated native-code evidence; native gameplay acceptance pending.
RF89 remains an incomplete development candidate. Nothing installed or promoted.

## Fuel evidence and contract

`f1c_restart_fuel.h` centralizes the reviewed fuel leaf at 004F1890, with
`this = vehicle + 12` and one float stack argument, callee `RET 4`. The complete
33-byte function is checked, including the player-global destination. It writes
body+14712 (vehicle+0x3984); role zero also updates the double player physics
mirror. Restoring only the vehicle float is insufficient for that player branch.

The native `SuspensionAndWheelPhysics` initializer at 004E5F60 computes
body+14716 from the HDV FuelRange base + step * (count - 1). This is vehicle
0x3988, the model capacity used by native AI refuelling decisions. The adapter
rejects unknown/NaN/nonpositive capacity, capacity over 1000, negative fuel,
fuel above capacity, short extents, invalid code, and a missing function. It
does not silently clamp saved state or invent a universal tank size.

RF calls the adapter only from its existing broker green and post-green
callbacks, while owning RF race control. The complete surviving field and
native code pass preflight before the green callback publishes roster changes,
and before post-green garage placement. The earlier GRID_READY worker mirror
is retained; it invokes no new native function. This change restores saved fuel
correctly. It is **not** implementation of full-service settings or the owner's
latest-pre-service-state policy.

## Penalty-mode evidence

`f1c_restart_penalties.h` now checks all 65 bytes of the 004A5660 service-mode
selector, including both relative calls and `RET 4`, before RF uses it.
Executable-page permissions alone were not enough to establish this ABI.
Restoration failures now release the claimed callback state, preventing a
permanent "transaction in progress" wait. Existing queue snapshots remain
unchanged: new post-announcement penalties and native deadline/service handling
during pit return still need an event-aware transaction. A queue memory-copy
test does not establish those behaviors.

## Verification and limitations

Canonical executable hash:
`1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`.
`exe-disassemble` supplied the complete native bodies. `test_restart_fuel.py`
verifies both byte fingerprints and every individual byte mutation, capacity
and buffer failures, and the +12 receiver. It executes the native fuel bytes
inside an isolated test process, relocating only the absolute player-mirror
destination to private test storage: player restoration updates the mirror;
AI restoration leaves it unchanged. No game process or fixed game page is
accessed. The complete SDK gate passes (`completion-work/sdk-test-r3.txt`).

Evidence, source/build hashes, exact changes, restoration maps and outstanding
owner scope are indexed in the
[RF/SC completion record](../../../development/mods/red_flag/trials/2026-09-21/policy-v89/completion-work/README.md).
These checks do not prove native weekend, pit strategy, repeated restart,
different executable variants or full damage/tyre service acceptance.
