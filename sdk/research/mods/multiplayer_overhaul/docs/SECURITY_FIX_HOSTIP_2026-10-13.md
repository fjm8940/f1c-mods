# CO-OP host-IP trust fix (F-19, F-20) - 2026-10-13

Status: **dev build only, not shipped, not deployed, not live-tested.** Tier of the fix claim: DERIVED
(static code reading, headless tests, build). Live two-PC check pending (section 8).
Source finding: `f1c_ida_decompiled/mods/sdk_sandbox/web_telemetry/SECURITY_REVIEW_2026-10-13.md`, F-19
(CONFIRMED) and F-20 (CONFIRMED), must-fix M7.

## 1. Problem (FACT, from code read at dev tree 13:44 to 13:54)

- Before the fix, `manifest_net_thread` set `g_sync_host_ip` from the sender of any non-MF datagram that
  was not yet verified (`InterlockedExchange(..., from.sin_addr.s_addr)`), before the manifest verdict.
  While `g_sync_host_ip == 0`, MF datagrams were accepted from any sender.
- `content_check_thread` then connected over TCP to that address (`connect_host`) and downloaded
  SeasonData files, verified only against hashes supplied by that same address. A LAN sender that reaches
  the client UDP port could therefore become the content source (file overwrite, backups taken).
- Host side (F-20): `COOP1?` got the full manifest from any sender and registered that sender as an MF
  relay peer; MF datagrams from any sender were relayed to every registered peer.

## 2. Fix (what changed)

Trust anchor: the remote IPv4 of this process's established TCP session to the game port 34447
(`game_session_host_ip`, already present). It is a TCP peer address, not a datagram sender.

| Where (fix-only build, `coop_mode.cpp` line) | Change |
|---|---|
| 41 | `#include "coop_trust.h"` |
| 1847-1869 (new) | `CoopTrustLogState` logs (client/host, 1848-1849), `log_untrusted_datagram()` (1852-1869) with `COOP-W30`, rate-limited to one line per 10 s with a reject count |
| 1872-1879 (new) | `coop_host_peer_ok()`: sender must hold an established game TCP session to this host (`game_session_peer_ips`) |
| 2344 | Host: `COOP1?` registers an MF relay peer only for game peers (manifest reply still goes to any asker, see open item O2) |
| 2354-2355 | Host: MF datagrams relayed only from game peers; others logged as `COOP-W30` |
| 2417-2421 | Client: `session_host` = `game_session_host_ip()` looked up per received datagram; MF ingest only when `from_host` |
| 2426-2430 | Client: manifest text accepted only when `from_host`; `g_sync_host_ip` is set only from `session_host`, never from `from` |
| (new) `dll/coop_trust.h` | Pure functions: `coop_trust_client_sender_ok(host, from)` (host 0 = nothing trusted), `coop_trust_host_sender_ok(peers, n, from)`, `coop_trust_should_log()` |

Behaviour: no game TCP session = manifest and MF from UDP are ignored (no adopt-first-sender). A first
manifest reply is accepted only when the sender equals the game-session host. Wire format unchanged, so
old hosts keep working (the host still answers `COOP1?` and sends MF the same way).

## 3. Not changed (open, see section 7)

- Other sites still key off `g_sync_host_ip` and are only as strong as it is now: N2 bot stream
  `CB01`/`CB02` (`coop_mode.cpp` around the `from.sin_addr.s_addr == g_sync_host_ip` checks near 5214-5218
  in the shared tree) and link sampler peer update (around 5424, stats only).
- NET1 (plain MP) host and client gates unchanged.
- `sync_path_ok` review (section 5).

## 4. Tests and build

- New `tests/test_coop_trust.py`: 9 tests. Headless harness compiled in `%TEMP%` with the mod's
  32-bit MinGW g++ (`-std=c++17 -Wall -Wextra -Werror`). Covers: unknown host ignores every sender,
  spoofed sender rejected, real host accepted, zero sender rejected, host accepts only listed game peers,
  rate limit counts and reports suppressed lines. Four source guards: the old adopt-first-sender pattern
  is absent, `g_sync_host_ip` has exactly one writer (`session_host`), manifest and MF gated, host MF
  relay gated. The guards were checked against the before-state: they match the old vulnerable lines.
- Build: the mod's own `dll\build.bat` run from a `%TEMP%` snapshot (`%TEMP%\coop_hostip_fix\src`) with
  `OUT=%TEMP%\coop_mode_hostip_fix`. Result: PASS (`-Wall -Wextra -Werror`).
- Snapshot content = before-state (13:54 copy) + exactly the fix edits (script `%TEMP%\coop_hostip_fix\apply_fix.py`,
  asserts a single match per anchor). Fix-only diff: `%TEMP%\coop_hostip_fix\fix_only.diff`.
- Existing suites on the live dev tree: `test_coop_bridge_abi` PASS, `test_coop_menu_logic` PASS,
  `test_n2_protocol` OK, `test_node_policy` PASS, `test_target_probes` PASS, `test_usage` PASS,
  `test_coop_trust` OK (9). `test_address_inventory` FAIL, **pre-existing**: it fails identically on the
  before-state copy (missing `0x00462300`, `0x00462940` from the require-mod-kick work; not caused by this
  fix). Needs an inventory update by the owner of that work.

Artifacts (dev build, not released):

- `%TEMP%\coop_mode_hostip_fix\multiplayer_overhaul.dll`
  SHA256 `3950BAEB F2C4AED6 0A95A55A 6E4F38E4 CFF68635 6A92ABB` (see section 6 for the full value)
- Source used: `%TEMP%\coop_hostip_fix\src\coop_mode.cpp` SHA256 `A251E881 33656088 F5434254 B30A5B31 B3BBA665 195E05DC 231DBDE1 EECE3`

## 5. sync_path_ok review (FACT)

`sync_path_ok` (coop_mode.cpp, near 1097 in the before-state) allows only paths starting with `SeasonData\`
(case-insensitive) and rejects `..`, `:`, `/`, a trailing `\`, and program files (`.dll .exe .asi .bat .cmd .ps1`).
Result: no traversal out of SeasonData. Reasons: a drive prefix needs `:` (rejected); UNC or rooted paths
cannot start with `SeasonData\`; `..` is rejected anywhere; `absolute_game_path` only uses a drive-letter branch
for `X:`-prefixed input. Residual (OPEN, low):
- Reserved DOS device names inside SeasonData (`NUL`, `CON`, `COM1`, `AUX`) are not rejected.
- No extension allowlist: other executable-like types (`.com .scr .js .vbs .lnk .msi .reg`) are allowed inside
  SeasonData. The game does not load them from there as far as known (INFERENCE, not checked).
- Paths longer than `MAX_PATH` are truncated by `lstrcpynA` (safe, but the truncated name is what gets written).
- Hash checks come from the same host (no independent check). Not changed by this fix; the trusted host now
  limits who can supply them.
- `accept_manifest` does not call `sync_path_ok`: manifest paths are only hashed read-only (existence oracle
  for a spoofed manifest). Nothing is written from that path (grep: writes come only from the `sync_path_ok`
  filtered list in `content_check_thread`).

## 6. Hashes

- DLL `multiplayer_overhaul.dll` (snapshot build): SHA256 `3950baebf2c4aed60efe0a555a95a26a4582fa93d5e4f38e4cff686356a92abb`
- `coop_mode.cpp` used (snapshot): SHA256 `a251e88133656088f5434254b9a30a5b31b3bba665195e05dc231dbde1eece5c`
- Combined live tree (fix plus the concurrent node-id work as it stood at 13:58:58; that work is not reviewed
  here): `%TEMP%\coop_combined_check\multiplayer_overhaul.dll` SHA256 `c3e8214a9840bd94082855db0959f0a5a7211b72e9aec3ff03ed4cad43cb219f`,
  built with `dll\build.bat` (PASS). Source `dll\coop_mode.cpp` SHA256 `231fe67e07e54c82605040015045bd1e9746837becac196469c44d150d384b67`.
  Use the snapshot DLL (section 6, first bullet) for the F-19 fix alone; the combined DLL needs the node-id owner's review.
- Before-state copies and `SHA256_BEFORE.txt`: `workspace_organization_audit\COOP_HOSTIP_FIX_BEFORE_2026-10-13\`
  (`coop_mode.cpp`, `README.md`, `findings.jsonl`).

## 7. Open issues (not fixed here)

- O1 (High, residual): UDP source-IP spoofing. A LAN attacker can still send a datagram with the host's IP as
  source. Manifest text and MF datagrams stay unauthenticated, so manifest text and MF events can be forged
  (the content download itself goes to the real host over TCP). Fix = M7: HMAC with a session key. That changes
  the protocol and needs a separate spec.
- O2 (Medium, F-20): the host still answers `COOP1?` (full bot manifest, paths and hashes) to any sender.
  Gating the reply would need the client to be in the TCP peer table on the host; a false negative would break
  bot sync for that client. Left for a live-tested change.
- O3: N2 bot stream `CB01`/`CB02` and the link sampler accept by IP only (see section 3).
- O4: plain-MP NET1 host answers any sender (verdict-only effect; can force a Vanilla fallback).
- O5: host-side TCP peer lookup assumes the host listens on 34447 and that the client's UDP source IP equals its
  TCP peer IP. Multi-adapter hosts (LAN plus Tailscale/Hamachi) may differ. Verify live.
- O6: `sync_path_ok` residuals in section 5.
- O7: the live shared tree at `development\mods\coop_mode\dll\coop_mode.cpp` contains a concurrent node-id
  change (`coop_node_policy.h`, `net1_claim_list`, `COOP1? <node>` requests, written after 13:54). At 13:58 it
  did not compile (`net1_claim_list` unused, `-Werror`); at the later check it compiled (combined hash in section 6).
  That change is not part of this fix and was not modified or reverted. Its own review and live test are the
  node-id owner's; this fix's live test should use the snapshot DLL or a combined build agreed with the owner.
- O8: `test_address_inventory` pre-existing failure (section 4).
- O9: release version. Released 0.9.6 is unchanged. Candidate label only: a `0.9.7` line or `0.9.6-sec1`;
  the owner decides. `coop_mode.meta.json` and `PRODUCT_STATUS.json` not changed.

## 8. Live test steps for the owner (not run yet)

Use the built DLL hash in section 6 on both PCs. Game closed before copying the DLL.

1. Host PC: start CO-OP, host a race. Normal session: host grid and bots appear.
2. Client PC: join. Expected in `multiplayer_overhaul_log.txt`: `manifest request`, then
   `host manifest verified`, then the content check (`content check: season=...`). No `COOP-W30` lines
   from the host's address. Race start works, MF events flow (if MF is installed).
3. If the client logs `COOP-W30 manifest text ... (trusted host none)` before the join finishes, that is expected
   noise (rate-limited, one line per 10 s). If it persists after the join, send the log: the host session
   lookup failed (multi-adapter or port issue, O5).
4. Negative test (optional, on a third LAN PC): send one UDP datagram to the client's port 34500 with a fake
   manifest from another IP. Expected: `COOP-W30 manifest text from <third PC> ignored`, no change in the
   client's bot list. Report the result.
5. Record in this file: PASS or FAIL per step, with log excerpts (no names or paths beyond the mod's own log lines).

## 9. Documentation gate

- Durable record: this file. Indexed from the mod README (Records section and status line).
- Facts vs inference: section 1 and 5 are FACT from code read; sections 4 and 8 cite evidence; the claim that the
  game does not load other executable types from SeasonData is INFERENCE (marked in section 5).
- Pending validation: section 8 (live) and O1-O9.
- `f1c_ida_decompiled/findings.jsonl` entry `coop-manifest-host-ip-fix-2026-10-13` (tier DERIVED until the live test).

## Coordinator review (Claude, 2026-10-13, game closed)
- Fix-only DLL SHA256 re-hashed: 3950baeb...a92abb = report.
- g_sync_host_ip now written only from session_host (coop_mode.cpp:2449, live tree); no write from datagram `from`. PASS.
- Verdict: ACCEPTED as dev candidate, DERIVED until owner live test (steps in report). Residuals O1-O5 open;
  real fix for UDP spoofing = M7 HMAC (own spec). Live tree also carries another workstream's unreviewed
  node-id changes; release must be built from reviewed tree only.
