# CO-OP remote-human channel audit, 2026-10-16

Owner request (2026-10-16): CO-OP human player data is missing on the client, at least throttle and brake. Haiku audit of every channel.
Method: Haiku read-only static audit (no game, no writes). Claude review spot-checked the key claims. Nothing is live-verified.
Findings: `f1c_ida_decompiled/findings.jsonl` `coop-remote-brake-binary-receiver-2026-10-16`,
`coop-remote-throttle-not-applied-2026-10-16`, `web-telemetry-remote-gate-hides-inputs-2026-10-16`.

## Claude review (what was re-checked)
- FACT (code): `remote_gate` (web_telemetry.c ~4800-4820) clears `car_valid` bits 2|4|8|16|32|64 for `CTRL_REMOTE` cars
  (throttle, brake, steer, tread, accel). Steer also needs `ai_control` (~5383). The viewer hides THR/BRK bars on those bits
  (telemetry_viewer.js ~2118). The banner (~2083) states that throttle, brake, steering, g-forces and tyres are not available.
- DERIVED (static decompile, re-read): receiver `0x4C73F0` writes `this+13700` (= brake `0x3584`) as 1.0 or 0 from packet flag
  `a2+56 & 0x4000000` (lines 166-171). It writes steer `this+13696` (`0x3580`, line 123). It never writes `this+13692` (throttle `0x357C`).
- Not re-checked by Claude (Haiku only, DERIVED): sender `0x4C71C0` packs throttle as 5 bits (x*31, payload word 11 bits 12-16)
  that no decoder reads. Sender brake flag is set when brake > 0.01.
- INFERENCE (Haiku): the CO-OP remote lap +2 offset may come from the mod. `remote_gate` seeds `g_rem_lap` from the raw lap counter
  at the first projection and adds one per line crossing. Confirmed: the code shape matches (~4807-4808). Cause not proven.

## Per-channel summary for a remote human (client side)
| Channel | Remote human | Basis |
|---|---|---|
| Speed | DERIVED from pose (raw 0x4440 stays 0) | code |
| Throttle 0x357C | not applied by the game on the client | DERIVED |
| Brake 0x3584 | in memory as on/off; the mod hides it | DERIVED + code |
| Steer 0x3580 | in memory (7-bit); the mod hides it (bit 8, ai_control gate) | DERIVED + code |
| g-forces | not applied by the game (inference); the mod clears them | inference + code |
| Lap distance / lap | DERIVED from pose; lap +2 suspect | code + OPEN finding |
| rpm, gear, yaw, track_x/z, position, pit, finish, fuel, life/damage | sent unchanged; whether they are current is UNKNOWN (likely stale) | needs live read |
| Tread/wear | cleared by the mod; game value UNKNOWN | code |
| Tyre temps, carcass, chassis (pch), clutch | player-only object; not for AI or remote | code |
| Lap/sector times | UNKNOWN (last_lap -1 in the lap-offset finding) | OPEN |

## Gaps ranked for a driver
1. Brake: quick win. Show it as on/off for mode-2 cars. Needs one live check that it toggles.
2. Throttle: needs a native detour that decodes the unused packet bits into 0x357C. First confirm on the host that 0x357C follows the pedal.
3. Steer: relax the gate for mode-2. The source of the steer value the sender transmits is OPEN.
4. Lap count and gaps: check whether the +2 comes from the mod's seed logic before blaming the game.
5. Lap/sector times, g from velocity differences, damage from the 14 subsystem bits: later candidates.

## Live CO-OP checks needed (client unless noted)
1. Remote 0x357C while the host sweeps the throttle (static = gap confirmed).
2. Remote 0x3584 toggles 0 / 1.0.
3. Host: local 0x357C follows the pedal.
4. Remote ai_control 0x45F4.
5. Log the g_rem_lap seed, increments and raw 0x403C.
6. Whether rpm 0x39A0, gear 0x39FC, pit 0x3450 and finish 0x4064 change on the remote copy.

## Decision status
Not in Telemetry 1.2.0 unless the owner says so. Nothing changed in code for this audit.
