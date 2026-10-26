# CO-OP netcode — modern car-state sync (spec, 2026-10-09, Claude)

Owner request: "modern optimized netcode". LAN connection type already made cars much smoother but
"still not smooth enough". Native session flow (menu, CO-OP entry, join, team select, chat, phases,
race start msg 20, collisions, results) stays native. Only **car state** transport changes.
Mod standard: `development/mods/MOD_STANDARD.md`. Background: `INVESTIGATION_NET_SYNC_2026-10-09.md`.

## Native facts this builds on (static, see investigation)
- Send `0x4C77D0` per locally simulated car (mode <= 1); interval `0x462FF0` = base rate from
  connection type, x<=2.25 below 25 mph, x4 if veh+18620 bit 2 clear, max 0.5 s.
- Record `0x4C71C0`: 52-byte class-1 record, id veh+18580, sender session time.
- Server intake `0x465300` (per id < 104), per-client distribution with byte budget `0x465460`.
- Receive `0x4B3400` -> buffer `0x463D10` (only mode-2 slots) -> interpolate `0x500560`
  (0.5 s extrapolation cap) -> apply `0x4C73F0`. Staleness `0x460DD0` hides cars > 8x late.

## Stages (each its own build + live test; stop when owner says smooth)

### N0 — measure (no behaviour change)
- Client: hook `0x463D10`; per remote slot keep last-arrival time; every 5 s log
  `net rx: slot=.. avg_ms=.. max_ms=..` for all remote cars (humans + bots). Host: count records per
  car per second leaving `0x4C77D0`.
- Accept: numbers in both logs during a race. Gives the real update rate per car.

### N1 — native rate unlock (cheap)
- CO-OP only: detour `0x462FF0` to return a fixed interval (INI `[Net] SendIntervalMs`, default 33)
  for every car, ignoring the x4/x2.25 factors.
- Remove the host per-client byte budget limit (find the budget source in `0x465460`; prefer
  `Throttle Upstream Override` equivalent in memory over code patch).
- Native interpolation unchanged.
- Accept: N0 numbers show ~30 Hz per car on the client; owner judges smoothness.

### N2 — own transport (only if N1 not enough)
- UDP port INI `[Net] Port` (default 34501). Host = relay + authority.
- Sender: each PC sends its locally simulated cars (host: own human + bots; client: own human) at
  60 Hz: header {magic "C2", version, session id, sequence, sender session time}; per car {slot,
  position float3, orientation 3x int16 smallest-three quaternion, velocity float3, angular velocity
  int16x3, rpm u16, gear i8, throttle/brake/steer u8, flags (lights, pit, finished)} ~40 B/car.
  22 cars ~ 900 B/packet ~ 55 KB/s per client: fine on LAN and broadband.
- Host forwards all cars to every client (no relevance culling for <= 24 cars).
- Clock: client estimates host-time offset with ping/pong every 1 s (keep min-RTT sample of last 10).
- Receiver: per-car jitter buffer; render time = host time - delay (default 100 ms, adaptive
  2x measured jitter, 50..200 ms); cubic Hermite position from pos+vel, slerp orientation;
  extrapolate max 250 ms then hold; snap if error > 5 m.
- Injection: on the client, CO-OP slots stay mode 2; detour `0x4F03E0` (mode-2 apply): if the stream
  has data for that slot, write the interpolated state through the native apply `0x4C73F0` by
  building a native record (re-use encoder layout of `0x4C71C0`), else fall through to native.
  Native records keep flowing (fallback + anything we do not carry).
- Accept: no visible stutter at 100-300 km/h side by side on LAN; packet loss 5% test still smooth;
  fallback works with port blocked.

## Risks / abort
- Writing pose outside the native apply path breaks wheels/sound -> always go through `0x4C73F0`.
- Collision is native and uses native-buffer state; N2 must keep native records flowing.
- Abort a stage if the game crashes or cars teleport; restore = previous DLL hash.

## Validation per stage
Build + tests (address inventory, probes, menu), DLL hash recorded, host + laptop live race,
N0 numbers before/after, owner verdict on smoothness.
