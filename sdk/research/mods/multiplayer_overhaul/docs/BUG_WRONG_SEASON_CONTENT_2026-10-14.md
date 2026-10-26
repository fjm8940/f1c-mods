# Bug report: CO-OP content download offers 2012 files in a 2000 season (2026-10-14)

Stage: investigate (no code change). Product: Multiplayer Overhaul 1.0.0 (DLL F4D3E01C, internal id coop_mode).

## Report
Tester Tester D (Discord, 2026-10-14): "co-op wants me to download stuff from 2012 even though i am in the 2000 season".
No logs received yet. Unknown: whether the tester was host or client, and whether the host ran a 2012 race earlier in the same game process.

## How the download list is built (FACT, code `dll/coop_mode.cpp`)
- The client downloads whatever the host lists (`content_check_thread` -> `LIST`). The host lists `build_season_set(g_season)`.
- `g_season` is the first 4-digit year token in the `Classes=` of the **host's own car** .veh (`vehicle_season` -> `classes_season`).
- `g_season` is only written in `build_season_candidates()`, during host bot preparation, and only when the season is known (> 0).
- `build_season_set` takes every vehicle folder in which **any** .veh lists that year, plus `SeasonData\Drivers\<year>*` and `SeasonData\F1_<year>*`.
- The vehicle catalog is built once at boot (all .veh, findings `phase4-gate-fully-closed-fopen-capture`), so roster indices are stable within one process; the roster cache in `build_season_candidates` is not a cause.

## Candidate causes (INFERENCE, ranked)
1. **Stale season.** If the host car's `Classes=` has no plain year token (for example a mod season using `F1_2000` or `2000RH`), `classes_season` returns 0, the log shows `COOP-E04 host car season unknown`, and `g_season` keeps the value from an earlier session in the same process (for example 2012). The host then serves the 2012 set. Code defect: unknown season should clear `g_season` (and the cached content set) so no content check is offered.
2. **First token wins.** The host car's `Classes=` lists 2012 before 2000 (shared or reused mod car).
3. **Folder rule.** A 2012 mod folder holds a .veh whose `Classes=` also lists 2000 (like the CLK55 safety car listing 1999-2002), so the whole folder is served for 2000.

Owner install `F1 Challenge V10` has no 2012 content; only `FIA\1999_CLK55` lists several years. The tester's install cannot be checked from here.

## Needed to confirm
Host and client `multiplayer_overhaul_log.txt` (1.0.0 name, `MOD_ID "_log.txt"`; older builds `coop_mode_log.txt`; Debug=1 not required). Decisive lines:
- host: `season source: roster=... veh=... classes='...'`, `season field: season=...`, `COOP-E04`, `content set: season=... files=...`
- client: `content check: season=... host files=... differ=...`
Also the host's car .veh `Classes=` line and the name of the 2000 season mod.

## Pending
- Logs from the tester. Then pick the fix (likely: reset `g_season` on unknown season; optionally match the season from the active `.gdb` instead of the car).
- Possible workaround (INFERENCE, not tested): Cancel the download window, since a 2000 race should not use 2012 files. If cause 1, restarting the host game before picking the 2000 season clears the stale value.

## Update: log bundles BR-20261013-8E79B2 and BR-20261013-CBB417 (read 2026-10-14)
Read-only on the VPS (`bug_reports.sqlite3` opened read-only, ZIPs read in place, nothing downloaded or changed). Both are "Send all logs" bundles from Manager 2.3.26, received 2026-10-13 and 21:17:44Z, no description. Different `ip_hash`, so two PCs: the tester pair. Neither bundle says which PC was host.

FACT (bundle contents):
- **No Multiplayer Overhaul log in either bundle.** The DLL writes `<MOD_ID>_log.txt` beside itself; Manager `FullBundle.TextFile()` (2.3.25 source freeze) accepts only `.log/.ini/.cfg/.plr/.wer`, `*.log.txt`, `*.ini.txt`, `trace.txt`, so `*_log.txt` is excluded. The season lines needed for this bug are therefore missing. Manager gap, separate fix.
- Both installs use a multi-season mod with year folders (`SeasonData\Vehicles\2000\2000_B200\...`, `...\2021\...`, 1956, 2013 and others). The owner install layout (team folders) is different.
- Last game process on both PCs (Lightloader 2.3.25 start 2026-10-13): only 2000 sessions, cars `2000_BENETTON11.VEH` and `2000_MINARDI21.VEH`, Mechanical Failures "field of 2 cars" = no bots spawned. The 8E79B2 PC ran 2021 and 1956 fields only in earlier processes.
- 8E79B2 `multiplayer_overhaul.ini`: `BotSource=season`, `Mode=Modern`. CBB417: `Mode=Vanilla`, has an empty `HostAddress=` key.

INFERENCE:
- Cause 1 (stale season from an earlier race in the same process) is now unlikely: both last processes saw only 2000 sessions. Restarting the game is not a fix.
- Causes 2/3 now lead: in this mod a 2000 car's `Classes=` may list 2012 first, or a 2012 folder holds a .veh that also lists 2000 (shared safety/medical car or reused chassis). Need the host's `Classes=` lines.

Ask the tester for (both PCs): `plugins\lightloader\multiplayer_overhaul_log.txt` sent by hand, and the `Classes=` line of `SeasonData\Vehicles\2000\2000_B200\2000_BENETTON11.VEH` and `...\2000\2000_M02\2000_MINARDI21.VEH`, plus which PC hosted.

## Update: full Discord thread and roles (2026-10-14)
Owner forwarded the thread. BR-20261013-8E79B2 = Tester D, BR-20261013-CBB417 = Tester E. Both on Multiplayer Overhaul 1.0.0. Three issues reported:

1. 2012 download in a 2000 season (above).
2. "When Pascal hosts the AI cars are teleporting" (rFactor with AI runs fine for them).
3. "When I [comedypur] host he [Pascal] gets kicked out every time" with the message "you got removed from the game".

### Issue 2: AI teleporting when Pascal hosts
- FACT: Pascal's `multiplayer_overhaul.ini` has `[Net] Mode=Vanilla`; comedypur's has `Mode=Modern`. Clients follow the host's Mode (manifest NET line), so a Pascal-hosted session runs original 2002 netcode for the bots.
- INFERENCE: Vanilla netcode update caps explain jumping AI; Modern exists for this. Not proven for this pair; Modern still had small client AI jitter (N1b, 0.9.5 record).
- Ask: Pascal sets Multiplayer settings > Connection type one step past LAN = Modern (or `Mode=Modern`), retest.

### Issue 3: Pascal removed when comedypur hosts
- FACT (code `require_mod_tick`, `manifest_net_thread`): in CO-OP the host boots every human node that has not claimed the mod within `NET1_NODE_GRACE_MS` = 10 s (`[Net] RequireModCoop`, default 1; notice chat then boot 3 s later). The claim is a client UDP `COOP1?` datagram to the host's ManifestPort (34500), counted only if its source IP equals a game peer IP (`coop_host_peer_ok`).
- INFERENCE (likely): the claim from Pascal never reaches comedypur's PC (UDP 34500 inbound blocked by Windows Firewall/router), or arrives from a different IP than the game connection (VPN/tunnel/NAT). Then a client that has the mod is booted as if vanilla. Pascal hosting works, so Pascal's 34500 is reachable.
- Product defect: a blocked side channel turns into a kick of a modded player. Fix options: longer grace or boot only on positive evidence of vanilla; client-side warning when no manifest reply arrives; host chat text that names the port. Not decided.
- Workaround (owner choice to send): host comedypur allows inbound UDP 34500 (and TCP 34500 for content) in Windows Firewall and router; or host adds `RequireModCoop=0` under `[Net]` in `plugins\lightloader\multiplayer_overhaul.ini` (dry-run, nobody booted). Not verified for this pair.
- Evidence missing: host log lines `COOP-K01 booted native node`, `COOP1? node claim` untrusted-datagram lines, client `COOP-E12`; not in bundles (Manager `_log.txt` gap above).
