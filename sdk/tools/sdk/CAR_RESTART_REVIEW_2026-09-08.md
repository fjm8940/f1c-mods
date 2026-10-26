# Experimental native per-car restart

Canonical header: f1c_car_restart.h. Exact supported executable SHA256
1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003.
This is an experimental native-call wrapper, not native gameplay certification.

Canonical inspect traced DynMan_Restart to per-vehicle SceneRenderUpdate004C6D90.
Exact-target exe-disassemble verifies ECX outer vehicle receiver, one stack dword
argument and ret4 at004C6E7A. Native code tests BL; pass integer1, not float1.0.
Full operation calls VehicleRespawnReset on vehicle+0x6F0, rebuilds physics/setup
on vehicle+12, resets per-car race data, runs placement, AIDriverStructInit and
per-car pit/scene state updates. It also invokes global scene/force-feedback
services; this is not a pure isolated memory transformation.

The wrapper calls the same per-car routine used by race restart, not global
DynMan_Restart (which resets simulation counters and every entrant). It does not
call vehicle-removal004C6E80, which frees resources and clears driver identity.
LargeConfigFunctionWithPhysicsConstants internally releases/rebuilds physics
resources; repeat-cycle native sound/resource safety is still unvalidated.

After original saved placement, separate AI rebind uses AIDriverStructInit00424790,
ECX same outer vehicle, integer1 stack argument, ret4 at00424AB2. This reacquires
waypoints and rebuilds profile/command/proximity state at the new pose. No old
waypoint or heap pointer is copied back. Entry/tail bytes of both functions are
checked before calls; consumer also enforces exact executable, stable game
thread, owned nonplayer AI, role1 and live resources.

Safety v0.10.4 calls restart/rebind once per pit return, then restores its parked
control settings and hides the car. Idle maintenance must not repeat native
initialization. Failure after mutation begins is latched and blocks redeployment;
it cannot be safely undone by raw copying the vehicle object. Test doubles verify
sequence, exact receiver/flag, refusal, no repeated native calls during parked
maintenance and failure latch. They do not emulate native allocation, sound,
pit crew, network/replay or full physics state. Full SDK gate required.

Static ABI evidence files: development/mods/full_safety_car/trials/v0.10.4_native_reset/
restart_abi.txt and ai_init_abi.txt (created through canonical exe-disassemble).
Product behavior/evidence record: NATIVE_RESET_2026-09-08.md in full_safety_car.
Native repeated departure, resource lifetime and callback duration remain the
next gate. No whole-object or global-world status equality is claimed: native
initialization itself uses current session time and random state.
