# Multiplayer Overhaul 1.0.0 — CO-OP player guide

CO-OP lets you and your friends race a full F1 Challenge V10 weekend **together against AI drivers**.
One player hosts; the host's PC runs the AI cars and everyone else sees them.

Version 1.0.0. Known issues are listed at the end.

---

## 1. What you need

- **F1 Challenge V10** installed on every PC (the same install base for everybody).
- The CO-OP files: `dinput8.dll`, `coop_mode.dll`, `coop_mode.ini`.
- For internet play: the host must be able to open (forward) ports on their router. See section 4.
- Every player must have a **player profile** (the normal F1C driver profile).

## 2. Installing (every player)

1. Close the game.
2. Copy `dinput8.dll`, `coop_mode.dll` and `coop_mode.ini` into the game folder, next to
   `F1 Challenge Online.exe`. Example: `C:\Games\F1 Challenge V10\`.
3. If Windows asks whether to allow the game through the firewall, click **Allow** (private and public).
4. Start the game. A new **CO-OP** entry appears in the main-menu mode selector, after MULTIPLAYER.

Uninstall: delete those three files from the game folder.

## 3. Important game setting (smooth cars)

In each player's profile, set the multiplayer connection type to **LAN**, even when playing over the internet:

- In game: the multiplayer connection setting in the options: **LAN**, or
- In `Save\<your name>\<your name>.PLR`, set `Net Connection Type="4"` (with the game closed).

With the default cable/DSL setting the game sends car positions too rarely and every car looks laggy.

## 4. Network setup

CO-OP uses the normal F1C multiplayer connection **plus** port **34500** (UDP and TCP) for the AI list and
content sync.

- **Same home network (LAN):** nothing to do.
- **Internet:** the **host** forwards these ports on the router to the host PC:
  - the normal F1C multiplayer ports (as for any F1C online game), and
  - **34500 UDP and TCP**.
  Clients do not need to forward anything.
- **Tailscale / VPN:** works like a LAN. Clients can list the host's addresses in `coop_mode.ini`
  (`[CoOp] HostAddress=`, comma separated) — usually not needed, the client finds the host automatically.

## 5. Hosting a CO-OP weekend (host)

1. Main menu → mode selector → **CO-OP**.
2. Choose your season, track and settings as for a normal multiplayer weekend and create the game.
3. The host's PC adds the AI field (the season's drivers) automatically.
4. Wait for your friends to join. When a friend takes the car of an AI driver, that AI car is removed.
5. Drive the sessions as usual. Use **NEXT SESSION** to skip sessions; AI results are simulated.

### Race start
- The race start countdown is **5 minutes** so everybody can get ready.
- As soon as the **host presses DRIVE**, the start moves to **10 seconds** later.
- The grid follows **qualifying times**. Players without a qualifying time start at the back.

## 6. Joining a CO-OP weekend (client)

1. Main menu → mode selector → **CO-OP** (important: select CO-OP, not MULTIPLAYER).
2. Join the host's game as for normal multiplayer.
3. The first time, your PC compares the host's season files with yours. If they differ, you are asked to
   **download the host's files**. Accept, wait until it says the content is installed, then
   **restart the game** and join again. A backup of every replaced file is kept in
   `CoOpBackup\<date>\` in the game folder.
4. Pick your car. If it is an AI driver's car, that AI car disappears for everyone.

## 7. During the weekend

- AI cars are driven by the host's PC; on clients they are shown from the host's data.
- Sessions, grid and start are controlled by the host.
- Your own car is driven by your PC as normal.

## 8. Settings (`coop_mode.ini`)

| Section / key | Default | Meaning |
|---|---|---|
| `[Mod] Enabled` | 1 | 0 turns CO-OP off |
| `[CoOp] ManifestPort` | 34500 | port for the AI list and content sync (host and clients must match) |
| `[CoOp] HostAddress` | empty | optional host address list for clients |
| `[CoOp] HostTimeWarp` | 1 | Host Ctrl+T time warp, only from the garage monitor; turns off when the host drives |
| `[CoOp] RefillBotMidSession` | 0 | 0 = when a player leaves, the bot for that car comes back at the next session (avoids a freeze); 1 = at once |
| `[CoOp] HostTimeWarpScale` | 10 | Warp speed 2..30 (30 = native, made AI physics unstable in MP on 2026-10-09) |
| `[Net] KickMessage` | 1 | host: before removing a player who has no Multiplayer Overhaul, one chat line to everyone says to get Lightloader (3 s before the removal). 0 = no chat, the removal still happens |
| `[Net] RequireMod` | 1 | host, plain multiplayer: a player who does not claim Multiplayer Overhaul is removed after about 10 s in the lobby (never during a race). 0 = no removal |
| `[Net] RequireModCoop` | 1 | host, CO-OP: same removal rule. 1 = real removal. 0 = dry-run (log "would boot", nothing sent). AI bot entries are excluded from removal by the bot guard (spec 14; live check pending) |
| `[Log] Debug` | 0 | **1 = very detailed log for bug reports** |
| `[Log] MaxMB` | 20 | log size limit; older part moves to `coop_mode_log.old.txt` |

Other keys are internal; leave them as shipped.

## 9. Reporting bugs

1. In `coop_mode.ini` set `[Log] Debug=1` (host **and** clients) before the session you want to report.
2. Play until the problem happens. Note the time (clock) and what you saw.
3. Send these files from the game folder of **every** player involved:
   `coop_mode_log.txt` (and `coop_mode_log.old.txt` if it exists).
4. The log contains player names and the IP addresses used for the connection. Only send it to us.

## 10. Known issues in this demo

- If a human leaves **during the race**, their AI car does not come back for that race.
- The client's on-screen start countdown keeps counting from 5:00 even after the host pressed DRIVE.
  The real start is still correct for everybody (the host sends the lights).
- The host's game closed silently once in testing (cause unknown). If it happens, please send logs.
- Internet play needs the host's port forwarding (section 4).
- Time warp (Ctrl+T, host, from the garage monitor) runs at x10 by default; clients are not synced.
- Qualifying order is kept: after the chequered flag NEXT SESSION no longer simulates extra bot laps; the race grid uses the captured best laps, saved as `plugins\lightloader\qualifying_<time>_<pid>_coop.tsv` (Grid Restore Ctrl+Shift+G can re-apply it).

## 11. Troubleshooting

| Problem | Fix |
|---|---|
| No CO-OP entry in the menu | files not in the game folder, or `[Mod] Enabled=0` |
| Client sees no AI cars | client did not select CO-OP, content differs (accept the download and restart), or port 34500 blocked |
| "RESTART THE GAME" message | content was installed; restart and join again |
| Cars laggy | connection type LAN on every player (section 3) |
| Two cars of the same driver | update to this version (fixed) |
