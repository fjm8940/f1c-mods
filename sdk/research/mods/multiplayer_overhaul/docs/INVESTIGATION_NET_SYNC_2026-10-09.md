# CO-OP network car sync — static investigation (2026-10-09, Claude)

Owner request: find how the native game moves remote cars on a client, so CO-OP bots can sync better
(laptop saw no real-time bot updates; rubberbanding). Static read of `f1c_ida_decompiled/functions`
(Hex-Rays names are often wrong; addresses are authoritative). No live test yet.

## Native pipeline (FACT, static)
Vehicle structs: inline array 0x5EC6D8, stride 18624 (0x48C0). Controller subobject veh+0x0C;
controller mode at veh+13000 (0x32C8): 0 = local player, 1 = AI, 2 = remote (network), 3 = replay.

1. **Send** `0x4C77D0`: per car with mode <= 1 (simulated on this PC). In MP, a countdown per car
   (veh+18616) uses interval `0x462FF0`, then encodes a record into the outgoing event queue
   (0x8AAE64, 432-byte entries). So every PC sends records for the cars it simulates; host AI (mode 1)
   are sent by the host.
2. **Interval** `0x462FF0(netcomm, slot)`: base = *(*netcomm + 4) (connection setting), x(1.25-...)
   up to x2.25 below 25 mph, **x4 when veh+18620 bit 2 is clear**, clamped 0..0.5 s.
   Bit 2|4 of veh+18620 is set/cleared by `0x4C7AA0` (driver visible/on track).
3. **Encode** `0x4C71C0`: class 1, subtype 0, 52 bytes; id = veh+18580; timestamp = sender session time
   (0x8AFEA0); packed pose (quantised position/orientation), velocities, per-wheel data, rpm, flags.
4. **Receive** `0x4B3400` (generic event dispatcher, also handles msg 20 race start): class 1 subtype 0/7
   with id < 104 is accepted **only if the receiver's slot has mode 2**; stored by `0x463D10` into the
   per-slot buffer at 0x5F0708 + 18624*id.
5. **Apply** each frame for mode 2: `0x4E4930` (physics dispatcher: mode 2 runs no physics) ->
   `0x4F03E0` -> `0x463D50` -> `0x500560` (interpolates buffered records; extrapolation capped at 0.5 s)
   -> `0x4C73F0` (writes pose, velocity, suspension, wheels, rpm, flags).
6. **Render** `0x4F0430`: modes 1 and 2 drawn as pos + vel*(t - stamp) with angular velocity.
7. **Staleness** `0x460DD0`: for each mode-2 slot, lag/expected > 2 -> marked degraded; > 8 or large
   error -> car hidden (bits 2|4 cleared).
8. `0x4B7A90` is the **replay recorder** (intervals 1/6..1/24 s from replay quality), not network.

## Correction
Earlier today I wrote that the laptop simulates its own bots. WRONG: `coop_mode.cpp:1774` builds client
bots with controller mode 2 (remote), ids = host slots 32-52. Client bots are therefore driven by host
records through the native path above. The OPEN finding was corrected in findings.jsonl.

## Inference (not proven) for the owner's symptoms
- Records carry host session time. The removed race-start hold rewound host session time every step
  (build before 7f685d6c) -> host timestamps frozen while client clock ran -> client interpolation stalls,
  0.5 s extrapolation cap, staleness monitor degrades/hides cars = "not real time" / jumps.
- Bots may send at 4x the base interval if veh+18620 bit 2 is clear for AI -> choppy at distance.
- Rubberbanding of humans: native netcode + host load (21 AI + 190 MB logging) — unmeasured.

## Next (proposed)
1. Live retest with build 7f685d6c (hold gone): laptop watches bots; host logs record rate.
2. Live read host: veh+18620 for bots vs humans (bit 2). If clear, CO-OP hook on 0x462FF0 to drop the x4
   for host bots (bandwidth check: 21 cars).
3. Add CO-OP diag on client: per bot slot, last-record age (hook 0x463D10), logged every 5 s.
4. Only if native path proves insufficient: custom stream (previous proposal).
