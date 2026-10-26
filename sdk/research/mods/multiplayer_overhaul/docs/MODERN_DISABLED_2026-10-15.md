# Modern net mode disabled (2026-10-15)

Product: Multiplayer Overhaul (`coop_mode`), shipped 1.0.1 / observe. Stage of this change: investigate -> implement -> verify (automated only).

## Owner report and instruction
- Report: "Modern" makes AI drivers lag and spike for client users; connection type LAN removes all of these issues.
- Instruction: investigate why; if not fixable, disable the Modern option.

## What Modern does (FACT, `dll/coop_mode.cpp`, 1.0.1 source)
Modern is the spinner step after LAN. The native connection type stays 4 (LAN), so Modern = LAN plus two detours:
1. `0x462FF0` per-car send interval: returns 1/SendHz (60 Hz) for every car. Native LAN uses its adaptive interval
   (N0 measured 13-16 Hz per bot; x2.25 slower below 25 mph, x4 slower for hidden cars, cap 0.5 s).
2. `0x464E90` connection byte budget: 8 000 000 B/s instead of LAN 625 000 B/s. Distribution period is 1/64 s in both
   (budget > 28 800), so that part is identical to LAN.

So the only effective difference from LAN is the per-car send rate (and the budget that no longer limits it).

## Why clients see AI lag and spikes (cause)
- FACT (live, 2026-10-11, `NETCODE_AUDIT_2026-10-11.md` s8, `STALL_ANALYSIS_2026-10-11.md`): with Modern the host sent
  a steady ~120 packets/s to one client; whole-PC upload 56.6 KB/s for one client. The client saw loss ~0 but packets
  up to 0.4-2.1 s late, arriving in bursts (76-98/s dips then 150-163/s).
- FACT (owner line test): host upload under load adds +1.7 to +2.1 s latency (bufferbloat on the modem upstream).
- FACT (`AI_JITTER_CLIENT_2026-10-11.md`): the client keeps one record per remote car and dead-reckons from it at the
  current time (no jitter buffer), so every late/burst arrival becomes a visible freeze-and-jump.
- INFERENCE (strong, not A/B measured on one link): Modern multiplies host upload by roughly 3-4x per client and scales
  with client count, which pushes home uplinks into the queueing regime; native LAN rates stay below it. That matches
  the owner observation that LAN removes the problem.

## Fixable?
Not with a small change. A real fix needs both rate adaptation to the measured uplink (congestion control) and a
client jitter buffer with interpolation (N1b/N2 scale; earlier N2 attempt `IMPLEMENTATION_N2_2026-10-11.md` still had
stall warping). Lowering SendHz only moves Modern toward LAN with no proven gain. Decision: disable Modern.

## Change (dev, NOT shipped, NOT live-tested)
`dll/coop_mode.cpp`:
- `NET_MODERN_AVAILABLE = 0`; `g_net_modern` starts 0.
- INI `[Net] Mode` ignored (logged `config: Net Mode=... in INI ignored: Modern disabled in 1.0.2`).
- Client ignores host `NET Modern` manifest line and `NET1 Modern` reply, so a 1.0.2 client stays Vanilla under an old host.
  The 1.0.2 host advertises `NET Vanilla`, so 1.0.1 clients under it also switch to Vanilla.
- Connection-type spinner menu hooks not installed: no Modern step after LAN; spinner is native.
- Both detours stay installed: with Modern off they call the trampolines (native behaviour). The 0x462FF0 BotStream=2
  path for owned AI is unchanged (BotStream default 0).
- `dll/coop_mode.ini`: Mode=Vanilla, comments say Modern disabled. `dll/coop_mode.meta.json`: Modern sentence removed
  from the description. Version left at 1.0.1 in source (owner decides the 1.0.2 bump at ship).
- Player README already tells every player to use LAN; no change.

Before-states: `dll/coop_mode.cpp|coop_mode.ini|coop_mode.meta.json|coop_mode_README.txt.BEFORE_20261015_nomodern`
(source was identical to `releases/v1.0.1/src/coop_mode.cpp`).

## Verification
- Build: `build.bat` with `OUT=%TEMP%\mo_nomodern_build`, -Werror clean. `multiplayer_overhaul.dll` SHA-256
  `89f42abf760022d05b48835b3edf8a408e3a879fbf519df64a87589c9d6130b2`.
- Tests: 8/9 scripts PASS. `test_usage.py` 4 FAIL = the same stale 1.0.0 version assertions recorded in
  `CHECK_LATEST_2026-10-14.md` (pre-existing, not caused by this change).
- Not done: in-game run; two-PC race confirming no Modern step in the spinner and smooth AI on the client.

## Pending / follow-up
- Owner: live 2-PC race with this build (host + client both LAN): log shows Mode ignored line, spinner stops at LAN,
  client AI smooth.
- Owner: version bump to 1.0.2 and ship decision.
- Lightloader Manager still shows `[Net] Mode` "Online car sync" and `SendHz` (`manager_cs/ManagerSchema.cs:90-92`);
  the DLL ignores them. Removal needs a Lightloader release.
- Restore: copy the `.BEFORE_20261015_nomodern` files back, or set `NET_MODERN_AVAILABLE = 1`.

## Test package (2026-10-15, owner request)
- `%TEMP%\MO_NoModern_Test_2026-10-15.zip` (SHA-256 `4c4a98ac0d6be15d4e484f4c6c3e79c8f8331e517e9fa324b4125176b2fa14ab`),
  unpacked copy `%TEMP%\MO_NoModern_Test_2026-10-15\`. Disposable; not a release.
- Contents: `multiplayer_overhaul.dll` (89F42ABF), `SHA256SUMS.txt`, `INSTALL.bat`, `RESTORE.bat`, `TEST_STEPS.txt`.
- INSTALL.bat: default folder `%USERPROFILE%\Documents\F1 Challenge V10\plugins\lightloader`, or a folder given as
  argument / pasted; refuses while the game runs; saves the old DLL once as `multiplayer_overhaul.dll.BEFORE_nomodern_test`;
  INI untouched. RESTORE.bat copies that file back.
- Verified: install and restore on a dummy folder in `%TEMP%` (hash 89F42ABF after install, original content after restore).
  Not run on a game install yet. Host install currently has shipped 1.0.1 (B4073BFC), INI `Mode=Vanilla`.
- Test steps in TEST_STEPS.txt: spinner ends at LAN, both on LAN, client AI smooth, host log `config: Net Mode=Vanilla`
  and no `net mode: Modern active`, client log no `host uses Modern`.

## Shipped as 1.0.2 (2026-10-15)
Owner: "just ship it as a v1.0.2". Rebuilt with version 1.0.2: DLL 446876FB (the 89F42ABF test build above is superseded). Deployed, served hashes verified. Record: `../releases/v1.0.2/RELEASE_RECORD.md`.
