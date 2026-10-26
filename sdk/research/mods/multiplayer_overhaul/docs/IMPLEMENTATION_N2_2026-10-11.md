# N2 own bot stream — implementation record (2026-10-11)

Pipeline: **implement → verify → human validate pending**. Product remains
Multiplayer AI Mode (`coop_mode`), currently shipped/observe at 0.9.5. This is a
dev-source implementation only. No version, release, production, game-install,
or install state changed.

## Implemented

- Added `[Net] BotStream=0|1|2` (default 1) and `BotStreamPort=34501`.
  The host sends the selected stream mode in the manifest `NET` line; existing
  clients ignore the added field, and new clients treat a missing field as off.
  The port must match on both PCs.
- Standard (non-`COOP_DEV_TOOLS`) build now detours the existing state encoder
  and remote update entry points. The encoder hook calls the original, scans the
  mod's owned AI slots, and copies each owned bot's 64-byte record into its
  latest net-id entry. It does no socket work. Records older than 500 ms are not
  sent.
- A worker binds UDP and sends `CB01` packets at configured `SendHz` to client
  IPs learned by the existing manifest exchange, using `BotStreamPort` at the
  receiver. Packets have a 17-byte header (`magic`, sequence, QPC host clock,
  count), at most 18 `{net_id, 64-byte record}` entries, and remain under 1200
  bytes. Client input is accepted only from the host IP learned from the
  manifest.
- The client keeps 64 samples per 8-bit net id, estimates the host-clock offset
  from the minimum observed arrival offset in 10-second windows, and tracks
  packet rate, sequence gaps, reorders, late samples, fallback calls, and
  stream-driven applies. All UDP and periodic logging run on the worker;
  callbacks only perform bounded scans and short critical-section copies.
- With host `BotStream=2`, the remote update hook selects the newest sample at
  or before `now - offset - SmoothMs`. If no sample is available or the selected
  state is over 500 ms old, it calls the native trampoline. Otherwise it rewrites
  the record net-id and sample age and calls native apply `0x4C73F0`. Freshly
  stream-driven bot slots bypass the existing smoothing hooks. Stage 1 leaves
  native visual behavior unchanged.
- Added a pure Python packet round-trip/validation test and backward-compatible
  `NET` line parser checks.

## Function-level change summary

- `build_manifest`, `accept_manifest`: append/read the optional BotStream value
  on `NET`.
- `mfb_peer_seen`: protects the shared manifest peer table while the stream
  worker takes a short snapshot.
- `bot_stream_now`, `bot_stream_reset`, `bot_stream_slot_for_vehicle`,
  `bot_stream_has_fresh`, `bot_stream_pick`, `bot_stream_store_packet`,
  `bot_stream_send_tick`, `bot_stream_worker`: clock, ring, wire, peer routing,
  stats, and session lifecycle.
- `bot_state_encode_hook`: pass-through encode plus latest owned-bot copy.
- `bot_remote_update_hook`: stage-2 buffered native apply or native fallback.
- `smooth_applies`: bypasses N1b only when an eligible stream sample exists.
- `install_hooks`, `mod_worker`: install release hooks, load keys, initialize
  locks before detours, start and stop the worker.

## Preserved before-state files

Created before edits; originals remain unchanged as named copies:

- `dll/coop_mode.cpp.BEFORE_20261011_n2`
- `dll/coop_mode.ini.BEFORE_20261011_n2`
- `dll/ADDRESS_INVENTORY.md.BEFORE_20261011_n2`
- `README.md.BEFORE_20261011_n2`

## Evidence tiers and assumptions

- **FACT from source:** the standard build now contains the encoder/update detours,
  worker-owned UDP send/receive, per-net-id rings, native fallback, and stage-2
  call to the already inventoried apply entry. The corresponding binary built
  successfully and its hash is recorded below.
- **FACT from the existing live prototype record:** the COOP LINK/SPIKE used a
  64-byte record and native apply path for one car on 2026-10-09. This
  implementation reuses that wrapper record length; the static encoder's
  52-byte encoded payload length is not treated as the complete wrapper size.
- **DERIVED:** packet packing supports 18 entries per datagram (17 + 18×65 =
  1187 bytes); client buffer selection targets host-clock time minus the
  estimated offset and `SmoothMs`.
- **INFERENCE / unproven:** the existing encoder hook fires for every owned bot
  on the modern CO-OP host path, and the rebased record time yields the intended
  one-interval forward application. Both require the exact-build live checks
  below; no gameplay claim is made here.

## Verification evidence

- Build command: `cmd /c development\mods\coop_mode\dll\build.bat`,
  output directed to `%TEMP%\coop_mode_n2_build`.
- Result: successful MinGW 32-bit build with the existing `-Wall -Wextra -Werror`
  flags; output tail: `Built ...\coop_mode.dll`; no new compiler warnings.
- Built DLL SHA-256: `F1EEE7BC4171C1074773F6DE1DA6A6AA7D2BD0C261FB3161AAC445A5D3344599`.
- All `tests/test_*.py` scripts: address inventory, bridge ABI, menu logic, N2
  protocol, target probes, and usage replay.
  - Address inventory: PASS, 71 addresses match.
  - Bridge ABI: PASS.
  - Menu logic: PASS (36 change cases, 18 display cases, 10 walks).
  - N2 protocol: PASS (3 tests).
  - Target probes: PASS (11 probes).
  - Usage replay with bundled fixture only: PASS.
  - Usage replay with the installed game log appended: 3 session-counter
    assertions failed (session/crash/exit); all other checks passed. This is
    environment-dependent existing-log evidence, not caused by this change; the
    bundled fixture-only run passed.
- No live game was run and no installation was performed, as required by the
  task prompt. Therefore runtime hook activation, real bot encoder coverage,
  UDP/firewall reachability, visual result, and the exact-build 2-PC behavior
  remain unverified.
- Workspace structure validation still reports its initial two errors:
  unexpected top-level `.claude` and `plans`; zero warnings. Neither was changed.

## Two-PC check and expected output

1. Use the exact built DLL on both PCs with matching `BotStreamPort=34501`.
2. First run host `BotStream=1` and let the client receive the host manifest. Check
   that the host reports nonzero send rate and peer count, and the client reports
   packet rate, bot count, loss/late measurements. This confirms whether the
   encoder hook produces current host-bot records.
3. Only after stage 1 evidence is sound, set host `BotStream=2` and repeat; the
   manifest carries this value to the client. Compare the client's stream-driven
   count and fallback count while watching bots for stutter/teleport behavior.

Expected host line:

`bot stream: host sent N pkts/s to K peers`

Expected client line (additional diagnostic counters are included):

`bot stream: client rx N pkts/s loss X% late avg A ms max M ms, bots B, offset O ms reorders R fallback F stream-driven D`

## Open questions / pending human validation

- Does `0x4C71C0` fire for every host-owned bot on the exact CO-OP path? If the
  stage-1 host send rate stays zero despite active bots and a known peer, stop
  and investigate this callback path; do not substitute another address.
- Does the exact-build client receive records for every host bot when player
  slot numbers differ? The implementation keys both ends by the record net id.
- Does `0x4C73F0` use the rebased record timestamp as expected when applying a
  sample with at most one send interval of extrapolation? Confirm with live
  movement and fallback logging.
- Is 150 ms `SmoothMs` an acceptable fixed delay in racing and collisions? Human
  review is required; smoothing bypass and fallback behavior must be observed.
- Validate stage 1 first, then stage 2. Verify Modern and Vanilla settings,
  session end/rejoin, firewall/port failure fallback, and two-PC behavior. Do not
  promote or copy this dev build into a release without the normal pipeline.


## Fix round 1 (2026-10-11)

Applied the review in `docs/REVIEW_N2_CODEX_2026-10-11.md`. These corrections
supersede the initial peer-routing and pacing details above:

- **B1 packet-copy bounds:** the second and later packet slices now copy from
  `&records[base][0]`, preserving row-major byte layout for the 2D array. A
  22-bot protocol test asserts two packets with 18 and 4 entries, exact entry
  ordering, and the source expression used by the C++ sender.
- **B2 NAT peer learning:** clients send `CB01?` plus a 32-bit nonce once per
  second from their bound stream socket to the manifest-learned host IP and
  `BotStreamPort`. The host learns the observed source IP:port and sends only
  to that endpoint. Its stream peer table holds at most 8 entries and expires
  peers after 5 seconds; a new peer logs `bot stream: peer a.b.c.d:port`.
  The MF bridge peer table and its temporary lock changes are no longer used by
  N2. The host firewall must allow inbound UDP on `BotStreamPort`; clients need
  no inbound firewall rule.
- **S1 send rate:** host pacing now uses QPC with an accumulating next-send
  deadline at `SendHz` and a 1 ms worker sleep; it no longer relies on
  `GetTickCount` resolution for the 60 Hz schedule.
- **S2 clock offset:** offset remains the minimum of the current and previous
  10-second window minima. A late packet at a window boundary cannot directly
  replace the render offset.
- **S3 diagnostics:** client reporting remains outside the successful-receive
  branch, so it reports `0 pkts/s` after a quiet 5-second window. The report
  timer is initialized at worker startup and reset on network-state transitions.
  The host line now reports packet rate, active peers, encoder calls per second,
  and fresh bot count: `bot stream: host sent N pkts/s to K peers, encodes M/s, bots B`.

Build: MinGW 32-bit `build.bat`, `-Wall -Wextra -Werror`; successful output tail:
`Built ...\coop_mode.dll`, no warnings. Fix-round-1 DLL SHA-256:
`93E6BA0263B5F1234C26F073B687A366BF9CA841112488446A9C6D4804E5909F`.

All six `tests/test_*.py` scripts passed in this round, including address
inventory (71 addresses), target probes (11), and five N2 protocol tests. The
usage replay was run with its bundled fixture only and passed. The earlier
installed-live-log usage-counter failure remains a separate environment result;
this build was not installed. No live game test was run.

Before-state copies created before this round's edits:

- `dll/coop_mode.cpp.BEFORE_20261011_n2fix1`
- `dll/coop_mode.ini.BEFORE_20261011_n2fix1`
- `tests/test_n2_protocol.py.BEFORE_20261011_n2fix1`
- `docs/IMPLEMENTATION_N2_2026-10-11.md.BEFORE_20261011_n2fix1`
- `README.md.BEFORE_20261011_n2fix1`

Pending live checks remain the same: confirm encoder coverage and peer discovery
on the exact host/client pair, validate stage 1 measurements, then exercise
stage 2 movement and fallback. No install or promotion was performed.

## Fix round 3 (2026-10-11)

This section supersedes the earlier Fix-round-1 behavior and the historical N1b
smoothing description above. Source at start of this round already contained the
fix2 432-byte zero-padded apply buffer; its SHA-256 was
`0473A7722505F05B0807772A33AFF8864369405AF3DA58A30201D7A41BF94235`.
No game install, production artifact, or product stage changed.

### Changes

- **Stall behavior:** after a net id is first stream-seen, the update hook never
  calls the native network-update trampoline for it during that session. It
  keeps applying the newest available 64-byte stream state in the existing
  432-byte zero-padded native buffer. Before an id is first seen, native updates
  remain enabled and are counted as fallbacks. Session/network reset clears the
  seen and playback state.
- **Decompile result (FACT):** the raw `0x4C73F0` apply routine takes its time as
  a separate argument and differences it against vehicle state at `this+18616`;
  the observed use is suspension weighting. It parses packed state beginning at
  `record+8` and writes vehicle state, but does not multiply position by record
  age. The inspected `0x500560` routine has no network-record parameter and
  does not read `record+4`; it compares a time-like argument with `this+444` and
  performs an interpolation/cross-fade path. The supporting raw decompiles and
  result are indexed in `f1c_ida_decompiled/findings.jsonl` as
  `n2-stream-gap-no-native-position-extrapolation-20261011` (FACT). There is not
  enough reviewed packed-coordinate semantics to synthesize a safe linear
  extrapolation, so a gap reuses the last pose. The record time field advanced
  by the hook is monotonic and delta is bounded by `BotStreamHoldMs` (default
  500 ms); `bot_stream_pick` uses `1/BotStreamHz` as the stream-period gap.
- **Recovery:** after a per-bot receive gap over 100 ms, playback target time
  advances from the held playback clock toward the live target over
  `BotStreamBlendMs` (default 400 ms). Sample timestamps are per-record capture
  times; out-of-order capture times are ignored, equal-time retransmissions
  refresh transport liveness without duplicating the sample ring. This is a
  clock blend, not coordinate interpolation. Static evidence does not establish
  that the native BoneSpring smooths a world-pose step; that visual result still
  needs human validation.
- **Upload rate:** `BotStreamHz` defaults to 30 and clamps to 15..60, independently
  of `SendHz`; the host communicates it in the manifest. With host `BotStream=2`,
  owned AI slots send native state at `NativeBotHz` (default 10); human slots use
  `SendHz` in Modern mode, and non-Modern human behavior remains native. Host
  diagnostics now report aggregate successful UDP payload bytes per second.
- **CB02 protocol:** stream packets use magic `CB02`; each 69-byte entry is
  `{net_id, float capture_offset_ms, 64-byte record}`. The client derives sample
  time as packet host clock minus the per-record offset. A client rejects a
  legacy `CB01` stream packet and logs that condition once. With 22 bots at
  30 Hz, 17 records fit in one 1190-byte datagram, so the host sends 60 packets/s
  per peer and 46,560 UDP payload bytes/s per peer (46.6 decimal KB/s, excluding
  UDP/IP headers).
- **Diagnostics and cleanup:** client 5-second output includes `holds N longest
  M ms recover-blends R` and retains fallback counts. Removed the unused
  `0x500010`/`0x500560` client smoothing hooks and the temporary host probe
  hooks. Removed their obsolete addresses from `dll/ADDRESS_INVENTORY.md`;
  66 remaining source addresses match the inventory. The INI defaults to
  `BotStream=2`, documents host inbound UDP 34501, and no longer contains
  `SmoothMs`, `SmoothMaxMs`, `[Bots]`, or `[HumanCars]` legacy list sections.
  The README documents that only the host needs inbound UDP 34501. The usage
  replay now uses its bundled fixture by default and reads an extra log only
  when `COOP_USAGE_LOG` is explicitly set.

### Verification evidence

- Release build (`build.bat`, `-Wall -Wextra -Werror`): PASS. DLL SHA-256
  `7175A191DCCB7CF969CDBD5164E0E72F136F4FB35E849431615999D69A4BA01E` in
  `%TEMP%\coop_mode_n2_fix3_release_done\coop_mode.dll`.
- `COOP_DEV_TOOLS=1` build: PASS. DLL SHA-256
  `FD268356C6CD8AF4AEF7DC9BD4BA8C05EA0A5BD3B70E6C6BC0A91DA97512B5C4` in
  `%TEMP%\coop_mode_n2_fix3_dev_done\coop_mode.dll`.
- All six component test scripts: PASS. Address inventory 66; bridge ABI;
  menu logic 36/18/10; N2 protocol mirror 5 cases; target probes 11; usage
  replay fixture-only. Protocol tests cover 1.5-second gap/hold/recovery without
  fallback, monotonic burst handling, repeated 30 Hz packets carrying a 10 Hz
  capture, CB02 round-trip, and 22 bots at 30 Hz (60 packets/s, 46,560 B/s).
- Workspace structure check: 2 pre-existing errors (`.claude`, `plans`), zero
  warnings. No structural change was made. No live game was run and no binary
  was installed or promoted.
- Verification corrections: the dev build initially surfaced `-Werror` for the
  release-only hook functions; they are now explicitly marked unused in dev
  builds, and both configurations rebuilt. Making the usage fixture the default
  exposed an unset-environment (`None`) path check; the guard now handles an
  unset `COOP_USAGE_LOG`, and the fixture-only replay passes. The final test run
  was clean after these corrections.

### Preservation and remaining validation

Before-state copies were created before edits with suffix
`.BEFORE_20261011_n2fix3` for `dll/coop_mode.cpp`, `dll/coop_mode.ini`,
`dll/ADDRESS_INVENTORY.md`, `tests/test_n2_protocol.py`, `tests/test_usage.py`,
this implementation record, the component README, and
`f1c_ida_decompiled/findings.jsonl`.

Pending human validation: exact-build two-PC run with host inbound UDP 34501;
confirm fallback reaches zero after each bot is first stream-seen, stream/encode
rates and payload bandwidth, no warp through a >1 s stall and recovery, and
whether the native visual path smooths the pose-clock blend. Verify full-race
behavior and use the owner’s visual verdict before any promotion. Product status
remains shipped/observe; no maturity or lifecycle evidence changed.
