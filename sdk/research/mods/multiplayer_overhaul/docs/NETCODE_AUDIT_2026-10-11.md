# Vanilla netcode audit vs modern standards (2026-10-11, Claude)

Owner request: "investigate the vanilla netcode and find out if there's room for improvements to bring it to
modern standards". Trigger: live CO-OP test 2026-10-11 with laptop "Test Man" on a mobile/NAT link
(public peer 203.0.113.1, Tailscale ping 51-143 ms; earlier Mullvad VPN rotated the peer IP every 20-30 s).
Builds on (read first): [INVESTIGATION_NET_SYNC_2026-10-09.md](INVESTIGATION_NET_SYNC_2026-10-09.md) (pipeline
send -> receive -> apply) and [SPEC_NETCODE_2026-10-09.md](SPEC_NETCODE_2026-10-09.md) (stages N0/N1/N2).
N0 (client `net rx` diagnostic) shipped 2026-10-10; N1 and N2 are NOT implemented.

Tiers: FACT-static = read from Hex-Rays `.c` of the canonical exe (1A70E5ED...), not live-proven.
DERIVED = interpretation of code. INFERENCE = not proven. Hex-Rays names below are often wrong; addresses rule.

## 1. What the vanilla netcode is (new facts this session)

| Area | Vanilla behaviour | Tier / source |
|---|---|---|
| Transport | Own Winsock layer (ISI NetComm.cpp). UDP datagrams (sendto 0x57D6C0, 1.5 KB buffer) plus TCP sockets with TCP_NODELAY + SO_KEEPALIVE (0x57C860). Socket events via `WSAAsyncSelect` window message 0x1A0A, i.e. handled by the window message loop. | FACT-static |
| Topology | Star: host relays every car's record to every client (server intake 0x465300, per-client distribution 0x465460). | FACT-static (2026-10-09) |
| Bandwidth budget | Per peer, from its **connection type** (0x464E90) unless `Throttle Upstream Override` > 0 (0x464EE0): 28K 1487 B/s, 56K 2975, ISDN 7200, cable/DSL 28 800 (~230 kbit/s), LAN 625 000. | FACT-static |
| Update rate class | Same table maps to 6 / 10 / 16 / 20 / 32 Hz, stored as ticks of 5 ms (0x464A60). | FACT-static |
| Per-car send interval | 0x462FF0: base interval, up to x2.25 slower under 25 mph, x4 if veh+0x48BC bit 2 clear, cap 0.5 s. Live 2026-10-10: bit set on all host cars; N0 measured **13-16 Hz per bot** on LAN. | FACT-static + live |
| Record | 52 bytes per car, quantised pose, velocities, wheels, rpm, flags; full state every time (no delta vs last ack). | FACT-static (2026-10-09) |
| Interest management | Host sorts cars per client by **squared distance** to the client's viewed car (0x464990, merge sort) and spends the byte budget nearest-first (0x465460). Far cars update less when the budget runs out. | FACT-static |
| Remote car display | 0x500560 evaluates the buffered record at **current** time: pure dead reckoning (pos + vel x dt), extrapolation capped 0.5 s, then a blend toward the new record (BoneSpring, PLR `BoneSpring Min/Max Constant`, `Damping Multiplier`; guide splines every `Spline Generate DT` 0.15 s). No interpolation delay / jitter buffer found. | DERIVED |
| Staleness | 0x460DD0: lag > 8 x expected interval (~2.4 s) or time error > 0.75 -> car hidden. | FACT-static (2026-10-10) |
| Collisions | PLR `Collision Threshold` 0.30 s: above that latency a client switches collisions off locally. | PLR comment, not traced |
| Master list | GameSpy protocol pointed at `master.openspy.net` (0x561BF0). | FACT-static |
| Security | No encryption or authentication of game packets found. | DERIVED (no crypto imports seen; not exhaustive) |

Host profile live values (Matias Driver.PLR): `Net Connection Type=4` (LAN), `Net Data Rate=32`,
`Throttle Upstream Override=-1`, `Collision Threshold=0.3`, `Spline Generate DT=0.15`, `BoneSpring Threshold=1.0`.
The laptop's values were not read (no access from the host).

## 2. Gap to modern practice

"Modern" here = common practice in current online racing / action games (general engineering knowledge,
not measured in a specific title): 30-60 Hz state, client-side interpolation with an adaptive jitter
buffer (render ~2 x jitter in the past), clock sync, delta / ack-based compression, packet-loss concealment,
unreliable channel for state + separate reliable channel, encryption.

| # | Gap | Effect the owner sees | Severity |
|---|---|---|---|
| G1 | Dead reckoning at "now", no jitter buffer (DERIVED) | On a jittery link (mobile: 56-143 ms swings) every late/early record becomes a visible correction: warping, rubber-banding | High (matches today's "very laggy") |
| G2 | 13-16 Hz per car measured; LAN class max 32 Hz | Coarse motion at 300 km/h (~5-6 m between updates) | High |
| G3 | Budget table from 2002 (cable = 230 kbit/s); a wrong connection type starves or floods | Wrong setting on either PC = far cars freeze or link saturates | Medium, config-fixable |
| G4 | Full 52-byte state, no delta | Wastes bandwidth; with 24 cars this hits the 2002 budgets | Medium |
| G5 | Socket events through window message loop (DERIVED) | Network handling stalls with frame hitches (load spikes, 8.5 s load frame seen today) | Low-Medium |
| G6 | TCP in the mix with TCP_NODELAY; which game data rides TCP is OPEN | If reliable game data uses TCP, one lost packet blocks everything behind it (head-of-line) on lossy mobile | Unknown, needs trace |
| G7 | Hide after ~2.4 s stale, collisions off at 0.30 s | Bad links lose cars or collisions instead of degrading gracefully | Medium |
| G8 | No encryption/auth | Packets spoofable on the internet | Low for private CO-OP, note for public servers |

## 3. Improvement options (cheapest first)

1. **C0 config (no code, test today):** both PCs `Net Connection Type="4"`, `Net Data Rate="32"`; try host
   `Throttle Upstream Override` (kbit/s) to see if the budget limits far cars; raise `Collision Threshold`
   to ~0.5 on the client for mobile links. Measure with the existing N0 `net rx` client log.
2. **N1 rate unlock (small DLL change, in SPEC_NETCODE):** CO-OP detour of 0x462FF0 to a fixed 30 Hz for all
   cars; lift the per-client budget for <= 24 cars. ~24 x 52 B x 30 Hz = ~37 KB/s per client: fine on
   broadband, borderline on weak mobile uplinks (host sends, client receives, so host upload matters).
3. **N1b interpolation delay (new, medium):** detour 0x500560 to evaluate records at `now - D` where D adapts
   to measured jitter (50-200 ms). This is the single biggest smoothness gain for bad links (G1). Cost:
   remote cars shown D ms in the past; collisions then need the existing native correction path.
   Feasibility not proven: the record buffer depth at 0x5F0708 + 0x48C0*slot must hold >= 2 records
   (check before building).
4. **N2 own UDP stream (large, in SPEC_NETCODE):** 60 Hz, compressed state, sequence numbers, clock sync,
   jitter buffer, loss concealment, native records kept as fallback. Solves G1-G6 together; most work and
   risk.
5. **Security (only if public servers):** packet authentication (session key in the manifest). Not needed
   for private CO-OP.

Recommendation: C0 now (free), then N1 + N1b as one build measured with N0 numbers on both the LAN laptop
and the mobile laptop. Go to N2 only if the owner still calls it not smooth.

## 4. Open questions (next investigation)

- O1: buffer depth per remote slot (0x463D10 store / 0x5F0708) - decides whether N1b is possible natively.
- O2: which messages go over TCP vs UDP (trace callers of 0x57D6C0 and the TCP send path in 0x57BF00).
- O3: exact BoneSpring blend code (callees of 0x500560, Hex-Rays names `AudioParameterInterpolation` / `AudioSampleCrossFadeInterpolation` are mislabelled).
- O4: laptop PLR values on the mobile test; N0 `net rx` numbers from the laptop log for this session.
- O5: where 0x462FF0's base `*(*netcomm+4)` is written (per-peer or session-wide).

Correction (2026-10-11): `0x7D108C` is the host session **Collision Threshold** (0.30 s; written from the
session config in 0x461900 as ms x 0.001, read by collision code 0x45C8F0), not an "expected send interval"
as IMPLEMENTATION_LOG_2026-10-10 called it. So 0x460DD0 degrades a remote car after 2 x threshold (0.6 s)
without a record and hides it after 8 x (2.4 s).

## 5. Changes made

Audit itself: none to code or game files. Read-only static analysis plus PLR reads. Records: this file, coop README index,
`f1c_ida_decompiled/findings.jsonl` entry `vanilla-netcode-budget-table-and-dead-reckoning-20261011`.

## 6. Implementation: CO-OP 0.9.5-dev2 "Modern" net mode (2026-10-11, owner: "do the fixes and remove any caps, add a Modern option")

- INI `[Net] Mode=Modern|Vanilla` (default Modern), `SendHz=10..100` (default 60). Logged as `config: Net Mode=...`
  and on first use `net mode: Modern active (...)`.
- Modern, only while CO-OP is selected: detour 0x462FF0 returns 1/SendHz for every car (removes the x2.25
  slow-speed, x4 hidden-car and 0.5 s factors); detour 0x464E90 returns 8 000 000 B/s for every connection type
  (removes the 2002 budget; distribution period becomes 1/64 s). Vanilla or non-CO-OP: trampolines, unchanged.
- Native event queue holds 1023 records (0x4C77D0 overwrites the last when full): 24 cars x 60 Hz = 1440/s,
  drained every frame, so it is not expected to fill at > 2 fps (INFERENCE).
- NOT done: interpolation delay / jitter buffer (N1b, needs O1), collision threshold, staleness monitor, own
  transport (N2). Mobile-link smoothness may still suffer from G1 until N1b.
- Before-states: `dll/coop_mode.cpp|coop_mode.ini|ADDRESS_INVENTORY.md.BEFORE_20261011_modernnet`.
- Build: `OUT=%TEMP%\coop_mode_build_dev2` clean with -Werror; coop_mode.dll SHA-256
  0c53b0e1df0c0f2a917b59e30447e394fb527cf36b226330b8b0e831c3b24a00. Tests: 5/5 PASS (address inventory includes
  the two new rows).
- INSTALLED on host 2026-10-11 (game closed): plugins\lightloader\coop_mode.dll = 0c53b0e1...; `[Net]` section appended to the
  installed coop_mode.ini. Backup of dev1 dll+ini: `workspace_organization_audit/WEB_TELEMETRY_430RC2_INSTALL_2026-10-11/coop_095dev1_before_dev2/`.
  Laptop package on USB `D:\coop_mode_0.9.5-dev2\` (dll, SHA256SUMS.txt, INSTALL_ON_LAPTOP.bat: backs up old dll+ini, keeps ini, appends `[Net]`).
- Pending live validation: host + laptop race; log shows `net mode: Modern active`; client `net rx` shows
  ~60 Hz per bot (was 13-16); no crash; owner verdict on smoothness; repeat with `Mode=Vanilla` for A/B.
- Restore: reinstall 0.9.5-dev1 (29FA411E1007...) or set `Mode=Vanilla`.

## 7. dev2 crash and dev3 fix (2026-10-11)

- Live: laptop on dev2 Modern crashed on join right after `scene exit slot=0` (log ends 09:28:09.657). With laptop
  `Mode=Vanilla` it joined, then the HOST (dev2 Modern) crashed at 09:33:00 on `scene enter` of the remote human
  (slot 1). Windows event 1000: c0000005 in F1 Challenge Online.exe at offset 0x64A6C = VA 0x464A6C.
- Cause (FACT, disassembly): 0x465430 does `mov ecx,[ecx]; push eax; call 0x464E90; ... lea ecx,[edx+ecx+0x8C];
  call 0x464A60`. Native 0x464E90 only touches EAX, so the caller keeps ECX across the call. The dev2 C hook
  clobbered ECX, so 0x464A60 wrote `[garbage+0x220]` at 0x464A6C. Runs in NetCommPlayerSlotSync when a player
  slot is set up, so every join crashed whichever PC ran Modern.
- dev3: `conn_budget_hook` is an asm wrapper (push ecx/edx, call C body, pop, ret 4). 0x462FF0 hook unchanged
  (its caller 0x4C77D0 only uses ESI after the call). Before-state `dll/coop_mode.cpp.BEFORE_20261011_ecxfix`.
- Build clean -Werror; dll SHA-256 f058ff4fdd19ea7f5ba9a750c38897c0659884c8905eea337bbd8c27b28233c2; tests 5/5 PASS.
  Installed on host (ini unchanged, Mode=Modern). USB `D:\coop_mode_0.9.5-dev3\` for the laptop.
- Lesson: a detour on a native helper must preserve every register its callers rely on, not just the C ABI set.
  Check the caller disassembly before writing a C hook on a leaf function.
- Pending live: join with both PCs Modern; `net mode: Modern active` on both; client `net rx` rates; no crash.

## 8. Live result dev3 (2026-10-11-09:45 UTC)

- Host dev3 Modern (`net mode: Modern active` 09:36:32); laptop Test Man joined 09:36:50 on mobile link, no crash.
- Host whole-PC network over 10 s: sent 56.6 KB/s (~88 UDP datagrams/s), received 42.9 KB/s (~74/s). Not
  isolated to the game process. INFERENCE: ~45-50 records/s per car (vanilla N0 13-16 Hz).
- Owner verdict (human validation): "looks smooth, AI is avoiding me now" (laptop view on mobile link).
- Not yet verified: laptop log (`net mode` line, `net rx` per-bot Hz), a full race distance, Vanilla A/B on the
  same link, bandwidth on a weaker uplink.
