Multiplayer Overhaul 1.0.0
Last updated 2026-10-13 / Author Matias

Race an F1 Challenge 99-02 weekend online with your friends against a full AI field.
One player hosts. The host's PC drives the AI cars and every player sees them.
Tested with 2 players.

------------------------------------------------------------
1. INSTALL (every player)
------------------------------------------------------------
- Install Lightloader in the game, then install Multiplayer Overhaul from the Lightloader
  Mods page. Every player needs it, host and clients.
- If Windows asks whether to allow the game through the firewall, click Allow.
- Start the game. A new CO-OP entry appears in the main-menu mode selector, after MULTIPLAYER.
- Turn it off with its switch in Lightloader.

------------------------------------------------------------
2. IMPORTANT GAME SETTING (smooth cars)
------------------------------------------------------------
Every player sets the multiplayer connection type to LAN, even over the internet.
Or, with the game closed, in Save\<your name>\<your name>.PLR set: Net Connection Type="4"
With the default cable/DSL setting the cars look laggy.

------------------------------------------------------------
3. NETWORK
------------------------------------------------------------
The mode uses the normal F1C multiplayer connection plus port 34500 (UDP and TCP).
- Same home network (LAN): nothing to do.
- Internet: the HOST forwards on the router to the host PC:
    the normal F1C multiplayer ports, and 34500 UDP + TCP.
  Clients do not need to forward anything.
- Tailscale / VPN works like a LAN. If a client does not find the host, put the host's
  address in coop_mode.ini: [CoOp] HostAddress=  (comma separated list allowed).

------------------------------------------------------------
4. HOSTING
------------------------------------------------------------
1. Main menu, mode selector: CO-OP.
2. Pick season, track and settings as for a normal multiplayer weekend and create the game.
3. The full AI field (all the season's drivers) is added automatically.
4. Friends join. If a friend takes an AI driver's car, that AI car is removed.
5. Drive the sessions. NEXT SESSION skips a session; AI results are simulated.
   IMPORTANT: do NOT press NEXT SESSION quickly several times in a row. Wait until every
   client has loaded the new session first, otherwise clients get dropped from the game.

Race start:
- The start countdown is 5 minutes.
- When the HOST presses DRIVE, the lights come on 60 seconds later. Use the time for setups.
  Clients' countdowns follow the host (0.9.1); a client still on 0.9.0 keeps showing 5:00.
- The grid follows qualifying. Players without a qualifying time start at the back.

------------------------------------------------------------
5. JOINING
------------------------------------------------------------
1. Main menu, mode selector: CO-OP (not MULTIPLAYER).
2. Join the host's game as for normal multiplayer.
3. First time: if your season files differ from the host's, you are asked to download the
   host's files. Accept, wait for "content installed", restart the game and join again.
   A backup of every replaced file is kept in CoOpBackup\<date>\ in the game folder.
4. Pick your car.

------------------------------------------------------------
6. SETTINGS (Lightloader Mod settings page, or coop_mode.ini)
------------------------------------------------------------
[Mod]  Enabled=1         0 turns the mode off
[CoOp] ManifestPort=34500  port for the AI list and content sync (same for everybody)
[CoOp] HostAddress=      optional host address list for clients
[CoOp] HostTimeWarp=1    host Ctrl+T time warp from the garage monitor
[CoOp] HostTimeWarpScale=10   warp speed 2..30
[CoOp] PitSync=1         Pit Stop Failures 1.1.2: every PC shows the same pit failure
[CoOp] RefillBotMidSession=0  0 = a leaving player's AI car returns next session (no freeze)
[Log]  Debug=0           1 = very detailed log for bug reports
[Log]  MaxMB=20          log size limit; the older part moves to coop_mode_log.old.txt
Other keys are internal; leave them as shipped.

------------------------------------------------------------
7. REPORTING BUGS (opt in)
------------------------------------------------------------
1. Set [Log] Debug=1 on the host AND every client before the session you report.
2. Play until the problem happens. Note the clock time and what you saw.
3. From plugins\lightloader\ in the game folder of every player involved, send:
   coop_mode_log.txt (and coop_mode_log.old.txt if it exists).
4. The log contains driver names and connection addresses. Only send it to the developer.

Automatic reports (0.9.1, Lightloader 2.3.1 or newer): only if the Lightloader switch
"Send crash, bug and mod usage reports automatically" is ON, the mod writes a short summary
after each CO-OP session (and after a crash) into plugins\lightloader\reports\usage and
Lightloader sends it. It holds counts and timings only (sessions, race starts, players
joining/leaving, AI cars, errors, crashes, network delay), never driver names, file names,
paths or addresses. You can read every summary in that folder. Switch OFF = nothing is sent.

------------------------------------------------------------
8. KNOWN ISSUES
------------------------------------------------------------
- A human who leaves during the race does not get an AI car back for that race.
- Fixed in 0.9.4: the race grid keeps the final qualifying order (no simulated laps after
  the chequered flag; order saved to plugins\lightloader\qualifying_<time>_coop.tsv);
  host Ctrl+T time warp works (x10, garage monitor only); no freeze when a player leaves;
  Pit Stop Failures synced; ESC menu without pause online (ESC Pause Menu 1.1.2 +
  Lightloader 2.3.3). Safety Car and Red Flag stay off in multiplayer. Host AND clients
  should update to 0.9.4.
- Fixed in 0.9.3: every AI car now sets a qualifying time (cars without a time leave the
  garage first from 30% of the session); AI cars no longer leave the garage after the flag or
  too late to start a flying lap; AI cars leave the garage normally from the start of a session;
  AI cars use their setup files; AI cars judge human cars' speed correctly; no false fastest-lap
  calls. Host AND clients should update to 0.9.3.
- Fixed in 0.9.2: the host always gets the full season grid (24 cars in 2002);
  before, the cars listed in [HumanCars] were left out even when nobody drove them.
- Fixed in 0.9.1: clients' countdown now follows the host's DRIVE. Host and clients should
  all use 0.9.1; a 0.9.0 client still shows 5:00 (the start itself is correct).
- Pressing NEXT SESSION too quickly on the host drops clients from the game (see section 4).
- The host's game closed silently once in testing (cause unknown). Please send Debug logs.
- Only 2-player sessions tested so far.

------------------------------------------------------------
9. TROUBLESHOOTING
------------------------------------------------------------
No CO-OP entry in the menu    Mod off in Lightloader, or Lightloader not installed
Client sees no AI cars        client did not pick CO-OP, files differ (accept the download and
                              restart), or port 34500 blocked at the host
"RESTART THE GAME" message    content was installed; restart and join again
Laggy cars                    connection type LAN on every player (section 2)
