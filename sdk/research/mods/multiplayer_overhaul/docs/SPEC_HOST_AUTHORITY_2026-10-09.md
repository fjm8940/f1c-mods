# CO-OP host authority: car replacement + season content sync (spec, 2026-10-09)

Owner request 2026-10-09 (chat). Status: specified; implementation in phases A and B.

## Owner intent
The CO-OP DLL is a de-facto server on the host and a client on joiners. The host's season
(drivers, cars, custom names) is the truth. A joiner is integrity-checked, told what is
missing for that season, and offered a download from the host. Existing files on the
joiner are backed up first; nothing is lost.

## A. A human takes a bot's car (replace)
- Observed: laptop human "Test Man" picked the Ferrari of bot Michael Schumacher. Host showed
  both cars (23), laptop 22 (its client build skipped the busy car).
- Rule: when a human's car (VehicleRosterIndex) equals an owned bot's, the host parks that bot
  (removed from the active field, slot remembered). The manifest stops listing it, so clients
  drop it too. Both screens then show the human in place of the bot.
- Human leaves during a session: the bot returns to its own slot, AI disabled, classified as
  retired (DNF), so results/classification stay consistent. Before a session starts (no laps
  yet) the bot simply returns as a normal bot.
- OPEN: what the stock game already does with a leaving human's classification row; the
  retire write path for a host-owned bot. Prior art: native retire route (Lightloader broker,
  game thread). Verify live before relying on it.

## B. Season content sync
- Season set (host decides): `SeasonData\F1_<season>.gdb` + icon, `SeasonData\Drivers\<season>\*`,
  and every vehicle team folder that contains a `.veh` of that season (2001 on the owner PC:
  about 62 MB). Excluded: logs (`*_log.txt`, `*.log`), `*.bak`, `Thumbs.db`.
- Host serves a file list (relative path, size, SHA-256) and the files themselves over TCP on
  ManifestPort. Only paths inside `SeasonData\` are served; `..`, absolute and drive paths are
  refused on both sides. Size cap per file and per transfer.
- Client: after joining in CO-OP, compares the list with its own files. If anything is missing or
  different it shows a Windows prompt (top-most): season, count, size, examples, and the backup
  location. Yes = download; No = no bots on this client (COOP-E10).
- Backup: every client file that would be overwritten is first copied to
  `CoOpBackup\<yyyymmdd-hhmmss>\<same relative path>` with a `RESTORE.txt` map. New files are
  listed there too so they can be removed to restore the previous state.
- Downloaded files land in `SeasonData` only after all files arrive and hash-check (staged in
  `CoOpBackup\<stamp>\incoming\`). The game loads season content at start, so the prompt ends
  with "restart the game and rejoin".

## Abort / safety
- Any hash failure or interrupted transfer: nothing is moved into SeasonData; staging kept.
- Client never sends files; host never accepts writes.
- Talent/driver data beyond the season set is not touched.

## Validation
- A: host 23 -> 22 when the human takes a bot car; laptop matches; leave mid-practice returns the
  bot; leave mid-race shows the bot as DNF on both screens.
- B: laptop with stock 2001 gets the prompt, downloads, backup folder complete, restart, rejoin,
  manifest verified, same 22 names on both screens.
