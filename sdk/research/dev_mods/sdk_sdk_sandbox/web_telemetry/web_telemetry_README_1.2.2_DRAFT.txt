Telemetry 1.2.2 - live race telemetry in your browser
Last updated 2026-10-17 / Author Matias

What it is
Live timing tower, track map, weather forecast, lap analysis and driver comparison for
F1 Challenge 99-02, live while you drive. Every session is recorded, so you can find it again
on the Recordings page. By default it only reads the game.

How to open it
- Lightloader: Tools tab > Telemetry > Open (opens your browser).
- Or open http://127.0.0.1:8080/ in any browser while the game runs.
  The port can be changed in plugins\lightloader\web_telemetry.ini ([Mod] Port).

LIVE and PAST
- The buttons at the top right switch between LIVE (this session) and PAST SESSION.
- PAST SESSION plays a recording back like a tape: timing tower, track map with every car,
  lap analysis and driver pages, with driver names and YOU / HUMAN badges. Back to live
  returns to the current session.
- Driver A and driver B are picked from the lists in the top bar. Pick "B off (show A only)"
  in the B list to show one driver only.

Recordings
- Page menu > Data > Recordings: every recorded weekend as a card with its sessions
  (Practice 1, Qualifying, Race...). Search, filter by mode, select several, and move
  recordings you do not need to the Windows Recycle Bin (nothing is deleted for good).
  The session that is recording now cannot be deleted.
- The oldest recordings are removed when the folder passes 3000 MB.
  Change it with MaxStorageMB in web_telemetry.ini (0 = keep everything).

TV Director (off by default)
- The TV Director tab shows the TV camera state. With [Director] Enabled=1 in
  web_telemetry.ini it can change the TV camera angle from this PC; CarSwitch=1 also lets it
  jump to any car. MuteCarChangeCommentary=1 (default) keeps the commentator quiet on those
  car changes. With Enabled=0 (default) nothing is ever sent to the game.

CO-OP (two PCs)
- The other human's car shows its brake as ON or off only (the game sends one on/off flag,
  not the pedal position) and its steering from the network (coarse steps).
  The game does not apply the other car's throttle, g-forces or tyre data on your PC,
  so the page cannot show them.

Other devices (LAN / Tailscale)
Set Remote=1 AND BindAddress=<this PC's private address> in plugins\lightloader\web_telemetry.ini
and restart the game, then open http://<that-address>:8080/ from the other device.
Remote=0 (default) = this PC only. Without a valid BindAddress, Remote=1 stays this PC only.

Settings
- Lightloader > Mods > Telemetry > settings (gear): every on/off option is a switch with a
  short explanation. The TV Director Enabled switch is the camera control. The [Mod] Enabled
  key is not used.
- Updating keeps your web_telemetry.ini and its values. New installs get the new layout.

Turning it off
- Lightloader Mods tab > Telemetry > turn the mod off, then restart the game.
  ([Mod] Enabled in web_telemetry.ini does nothing; it is kept only so old files stay valid.)
The page only exists while the game (and the mod) is running.

Reports
With the Lightloader switch "Send crash, bug and mod usage reports automatically" on,
anonymous usage and crash summaries help fix problems faster. Details on the privacy page.

Public release: Telemetry 1.2.2. Telemetry engine diagnostic version: 4.4.0, API 10.
