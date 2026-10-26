# AI cars jitter/warp on client (Tester A: Marcel Lasee) - 2026-10-11

Stage: investigate. No code changed for this item.

FACT (owner report): Tester A (client) sees Marcel Lasee (bot slot 40, host-driven) lag and jitter.
FACT (host log 16:51-16:52): host 135-148 fps, worst frame 16 ms, 24 cars, Modern 60 Hz - host side is smooth.
Missing: Tester A's coop_mode_log.txt (`net rx` rates, frame times). Why Lasee more than others is NOT known.

DERIVED (raw Hex-Rays, finding `remote-car-record-store-depth-one-20261011`):
- 0x500010 keeps ONE record per remote car (0x5F0708 + 0x48C0*slot); older records dropped.
- 0x500560 draws the car at the CURRENT time by dead reckoning from that one record (pos + vel*dt,
  cap 0.5 s), blended from the last drawn pose.
- So every late/early packet on the internet link (Tailscale) becomes a visible correction = jitter/warp.
  Sending more often (Modern 60 Hz, done) cannot remove this; timing variation still shows.

Fix that can work (N1b, own buffer, both PCs, client side only matters):
1. Hook 0x500010: copy every accepted record into a per-car ring (e.g. 16 records, host timestamp).
2. Hook 0x500560: draw remote AI at `now - D` by interpolating between the two buffered records around
   that time (D adaptive 60-150 ms from measured arrival jitter); fall back to native when the ring
   has < 2 records or the car is a human (keep humans responsive).
3. Log per-car ring stats (late/early, D) for diagnosis.
Cost: remote AI shown ~0.1 s in the past (bots only, no effect on the client's own car); collisions
with bots use the native path (may feel ~0.1 s off at contact). Medium work, needs live two-PC test.
Pending owner OK to build.

## Built 2026-10-11 (owner "go"), NOT live-tested
`dll/coop_mode.cpp` (before-state `.BEFORE_20261011_smooth`, also ini + ADDRESS_INVENTORY):
detour 0x500010 (prologue `55 56 57 8B 7C 24 14`) copies each accepted record (440 B from vehicle+0x4030)
into a 32-deep ring per slot; detour 0x500560 (prologue `81 EC B0 01 00 00 53`) for host bots on a client:
swaps in the newest buffered record with time <= now - SmoothMs, calls native with time now - SmoothMs,
restores the live record. Fallback native when < 2 records or none old enough. INI `[Net] SmoothMs=100`
(0 = off, max 300). Log: `config: Net SmoothMs=...`, `smoothing: active ...` once.
Simplification vs plan: no interpolation between two records; native dead reckoning from an older, already
arrived record (short dt) instead. Fixed D, not adaptive.
Assumptions (unproven): `now` and record time share one time base (native compares them at +444);
client bots count as `slot_is_owned_ai`. If the `smoothing: active` line is missing on the client, the
second assumption is wrong.
Build sha256 875556214542964f6ee53eeab1884f6c48a58d5b041aa479258b1710179bf4f5 (includes garage-box fix);
tests 5/5 PASS (address inventory 69). Staged `%TEMP%\coop_send_smooth` (dll + ini). Owner ini installed; owner DLL installed later same day (sha verified).

Live test: Tester A client log shows `smoothing: active`; Lasee/other bots smooth; try SmoothMs 150 if still jittery,
0 to compare.

## Risk note (2026-10-11, after owner question "will this cause lag?")
INFERENCE: caller 0x463D50 feeds the 0x500560 output into PhysicsIntegrationStep 0x4FE620-path and
PhysicsFloatUpdateWithCompare, so the delayed pose is likely also the bot's COLLISION pose on the client,
not only the drawn one. At 300 km/h, 100 ms = ~8 m. Contact between the client's car and a bot may happen
where the bot was 0.1 s ago. CPU cost is negligible (440 B copy per bot per frame).
Test plan: wheel-to-wheel with a bot on the client at SmoothMs 100 / 50 / 0; if contacts feel wrong,
lower it or limit smoothing to the drawn pose only (needs a split of draw vs physics, not built).
AI awareness of humans (host side) is a separate path: the host AI sees the client's car via the same
single-record dead reckoning plus RemoteCarTracking (speed/waypoint). Not changed by SmoothMs (host is excluded).

## Net age diagnostic (owner 2026-10-11 "do 2") - built, installed on owner PC, not live-run
Hooks 0x500010/0x500560 (already detoured) now also measure, per remote car: age = draw time - record time,
updates/s, max gap between record times. Every 5 s: host logs `net age: host sees human slot N <name>: avg/max ms,
updates/s, max gap`; client logs one bots summary line (`net age: client sees N bots ... worst <name>`) plus the
host's car. Before-state `dll/coop_mode.cpp.BEFORE_20261011_netage`. Build sha256
b444a59b7c57574b0a8fe4bbbf2a7ce302a876eb10905248da64bf0365094b4a, tests 5/5 PASS. Staged `%TEMP%\coop_send_netage`.
Unproven: that 0x500560 runs for remote humans on the host (no line = it does not; then other hook needed).
Age includes the record's own timestamp offset; read the trend, not as exact one-way latency.

## Live run 1 (2026-10-11-17:04 UTC, host pid 4404, Practice, Tester A drove a lap)
FACT: `config: Net SmoothMs=100` logged (new DLL loaded); garage hook logged `Tester A ... back to own team box
team 2 bay 2` (so detours installed, incl. 0x500010/0x500560 which install before it).
FACT: 0 `net age` lines on host. Static: 0x4B3400 calls the store 0x463D10 only for role==2 cars; draw
0x500560 is reached via 0x4F03E0 -> 0x463D50. INFERENCE: hooks fire but every age sample is outside the
0..10 s filter (different time bases) - unproven. Diagnostic build a40c9cd0... logs raw now/record values
(6 draws, 3 stores); staged `%TEMP%\coop_send_netage2`, needs game restart (host only is enough).

## Live run 2 (2026-10-11-17:07 UTC, host pid 29560, diagnostic DLL a40c9cd0 installed and verified)
FACT: 0 `net age raw` lines, including the store line that has no time filter. So on the HOST neither
0x500010 nor 0x500560 runs for Tester A's car. The time-base inference above is DISPROVEN for the host.
INFERENCE: the host takes the client's car through another path (to find). The 0x4B3400 -> 0x463D10 path may be
client-only. Open: whether smoothing runs at all on the client - Tester A's log decides (`net age raw` /
`smoothing: active` lines).

## Adaptive delay (owner 2026-10-11: "lag spikes when ping rises momentarily") - built, not live-tested
Evidence the client path is live: earlier N0 `net rx (client)` lines counted bot records through 0x463D10 on
Tester A's PC; 0x463D10 -> 0x500010 per callgraph. Host path for Tester A's car remains unknown (run 2).
Change: per stored record, offset = arrival clock - record time; baseline = min offset over ~10 s; lateness =
offset - baseline; peak lateness held, decays 10%/s; target delay = peak + 30 ms within [SmoothMs, SmoothMaxMs].
Delay slews to target: up 0.25 s/s (bots briefly at 75% pace instead of jumping), down 0.05 s/s.
Log: `smoothing: ping spike, record N ms late; delay now N ms` (first 50), net age line shows current delay.
INI `[Net] SmoothMaxMs=300` (max 500). Before-state `dll/coop_mode.cpp|coop_mode.ini.BEFORE_20261011_adaptive`.
Build sha256 dbff1dddfcfc19579116d47ba34d70b4990ebcd911214e2a799ad023314cafd, tests 5/5 PASS, staged
`%TEMP%\coop_send_adaptive`. Owner ini installed; DLL locked (game running).
Limits: the FIRST spike after a calm stretch still shows (delay reacts on the late record); repeated spikes are
absorbed. Collision pose likely delayed too (see risk note). Host-side AI awareness not addressed.

## Owner decision 2026-10-11: no slowing of bots - fixed delay default
Owner: bots must NOT be drawn slower (fatal in fast corners). Default now fixed delay: `SmoothMs=150`,
`SmoothMaxMs=150` (code default SmoothMaxMs = SmoothMs, so the adaptive slew is off unless raised in the ini).
Fixed delay never changes display pace; a ping gap shorter than 150 ms is covered by buffered records, a
longer one falls back to native extrapolation (old behaviour) without a time jump. Bots' real driving is on
the host and was never affected. Build sha256 5579cc4fec4fcfcba03164678bd7c6af8083a7fcdf62447b2ab66520a066b7b,
tests 5/5 PASS, staged `%TEMP%\coop_send_fixed150`. Owner ini installed, DLL locked (game running).
