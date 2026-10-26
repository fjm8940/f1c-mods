# Multiplayer AI Host

> **Current target:** a native, host-side multiplayer AI grid. It must add AI
> entrants inside one host process. The earlier `-noplayer` launcher scripts
> below are preserved prototype research and are not the intended solution.
> See `NATIVE_IMPLEMENTATION.md` for the active implementation boundary and
> `HOST_ORDER_PACKET_RESEARCH_2026-08-21.md` for the recovered node-order packet
> that is now excluded as an entrant channel.

> **Live correction, 2026-08-21:** per-client `+config`/`SAVEDIR` isolation is
> unsupported in the tested V10 installation. The redirected-root bot crashed
> in `Lua.dll`; only a normal shared-save client reached Select Team. The
> launcher material below is retained as the earlier research hypothesis, not
> as a working or recommended grid. See `LIVE_EVIDENCE_2026-08-21.md`.

Development-only host and grid wrappers for F1 Challenge's native
`-noplayer` mode. F1 Challenge has no native multiplayer “add AI drivers”
function: each AI driver is a separately launched no-player game client.

## What it does

`launch_ai_host.ps1` launches one game client with `-noplayer`. The switch is
parsed by the executable before networking starts. It does not edit any
season, vehicle, player, or game-install file. It creates one AI-controlled
network client; it does **not** create a complete AI grid.

The script deliberately does not guess undocumented `+host`/network command
line parameters. That keeps the host setup visible and preserves the game's
normal compatibility checks for every client.

## Start an interactive host

From a staging folder or this development folder, run:

```powershell
.\launch_ai_host.ps1 -GameExecutable "C:\path\to\F1 Challenge Online.exe"
```

Then use **Multiplayer → Host** in the game and let other players join
normally. Every participant and every no-player client must use compatible
season/vehicle/track content.

Use `-WhatIf` to validate the command without starting the game.

## Start a dedicated AI host

The executable's NetComm parser has native dedicated-server, game-name,
password, and maximum-player command-line paths. The dedicated wrapper exposes
those in a visible PowerShell server window:

```powershell
.\Start-F1CDedicatedAIHost.ps1 `
  -GameExecutable "C:\path\to\F1 Challenge Online.exe" `
  -ServerName "2001 AI Race" `
  -MaxPlayers 8
```

`-MaxPlayers` is the total network-slot capacity (the executable clamps it to
2–104); it is not the AI-car count. An optional password is accepted as
`-Password`, and `-WhatIf` prints the resolved launch without starting the
process.

This is a dedicated *host process*, not a proven headless server. The game may
still create its normal graphics/UI window; no safe evidence yet shows that its
simulation loop can run without it. The server console remains open, shows the
launch details and PID, and waits for the process to exit.

## Add AI drivers to a hosted session

First create the multiplayer host session manually with `noPlayer=false`. Then
launch one client per AI driver, pointing all of them at the host's reachable
address and game port:

```powershell
.\Start-F1CAIGrid.ps1 `
  -GameExecutable "C:\path\to\F1 Challenge Online.exe" `
  -HostAddress "192.0.2.10:34397" `
  -AIDrivers 20 `
  -EnableOPHNoPlayer
```

The game's command-line parser recognizes `+connect` and `+name`, so the
launcher joins each client and assigns a distinct network name automatically.
With `-EnableOPHNoPlayer`, it temporarily sets `noPlayer=true` while those
clients initialize, then restores the exact previous `oph.ini` contents. It
never changes the host process's already-loaded setting.

### Preserved profile-isolation hypothesis (unsupported on tested V10)

The following design was recovered statically and implemented in the prototype
launcher before live validation. The 2026-08-19 live trial disproved its current
usability: a custom-root client crashed in `Lua.dll` with BEX / `0xc0000409`.
Do not use this route for a grid unless the V10 Lua/config interaction is first
repaired and independently retested. The text remains here to preserve how the
failed experiment was constructed.

F1 Challenge also has a native `+config=<file>` startup path. Its `Config.ini`
has a `SAVEDIR` setting, and the profile scanner uses that configured directory
for every `.PLR` read and write. The grid launcher can therefore give each
client a separate save root instead of making all game processes share the
installation's `Save` folder.

First make safe, independent copies of the profiles you want to use. This never
alters the source profiles and refuses to overwrite an existing bot copy:

```powershell
.\Initialize-F1CBotProfiles.ps1 `
  -SourceSaveRoot "C:\path\to\F1 Challenge Online\Save" `
  -BotSaveRoot "C:\F1C-BotProfiles" `
  -ProfileNames "Michael Schumacher","Nicky Lauda"
```

Then give exactly one profile name to each launched client:

```powershell
.\Start-F1CAIGrid.ps1 `
  -GameExecutable "C:\path\to\F1 Challenge Online.exe" `
  -HostAddress "192.0.2.10:34397" `
  -AIDrivers 2 `
  -ProfileNames "Michael Schumacher","Nicky Lauda" `
  -BotSaveRoot "C:\F1C-BotProfiles" `
  -EnableOPHNoPlayer
```

The prototype requires a bot root with no spaces because the game's `+config` parser reads a
whitespace-delimited file name. The individual profile folders may have spaces.
The launcher creates only disposable per-client config files under
`C:\F1C-BotProfiles\_f1c_config`; it does not rewrite the installation's
`Config.ini` or `Save` directory. Static analysis predicted isolated setup and
result writes, but the tested runtime crashed before this could be accepted as
a safe isolation mechanism. The game also still enters its profile/team flow.

Use the host's **Port Start** value for `HostAddress` (the stock default is
`34397`). Do not use the GameSpy query port (`34297`): it can list a session
but cannot complete a game join.

### Launch a complete season roster

`Start-F1CSeasonAIGrid.ps1` derives the roster directly from the installed
`SeasonData\Vehicles` files. It finds every numbered vehicle whose `Driver=`
entry belongs to the requested season, creates an isolated profile with that
vehicle and driver, and launches the complete roster. It never modifies the
template profile or the installation's `Save` tree.

```powershell
.\Start-F1CSeasonAIGrid.ps1 `
  -GameExecutable "%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe" `
  -HostAddress "192.0.2.10:34397" `
  -Season 2001 `
  -TemplateProfile "Michael Schumacher" `
  -BotSaveRoot "C:\F1C-BotProfiles" `
  -EnableOPHNoPlayer
```

The prototype creates durable isolated profiles under
`C:\F1C-BotProfiles\2001`; later launches reuse them unchanged. `-WhatIf`
prints the recovered roster and all planned copies/configs/clients without
writing or launching anything. The roster is the installed mod's roster, so a
season with alternate-history or replacement entries launches those entries as
defined by its own vehicle files.

Creating those copies does not make the launch route supported. The active V10
runtime test failed with a redirected save root, and shared-profile collisions
remain unresolved for multiple simultaneous clients.

`-AIDrivers` is therefore the real AI-client count. Begin with one client and
increase in small batches: each driver consumes a full game process, and all
clients must have the same season/mod/track content as the host. Use `-WhatIf`
to inspect the planned launches; use `-NoWait` if you do not want the console
to wait for the clients to exit.

The game has no command-line parameter for the client team/profile selection.
After autojoin it intentionally opens the client team-selection flow, so this
launcher cannot honestly claim to select a vehicle for each client. That final
step needs either a one-client live test to establish the repeatable key path
or a separate, guarded UI automation layer.

## Native co-op grid configurator

The active implementation is now host-authoritative and does not launch a game
process per bot. Its validated baseline is two human instances plus Tester A:
the host owns Tester A as stock role-1 AI and the other instance owns a role-2
network replica. Only Tester A drove, and normal collision/damage/retirement logic
remained active. More than one bot is still an experimental scale-up.

Build the project, then open:

```text
mods\sdk_sandbox\multiplayer_ai_host\build\multiplayer_ai_configurator.exe
```

In this workspace the configurator discovers `F1 Challenge V10` automatically.
For another installation, pass its game directory as the only command-line
argument. Its season selector is generated from the installation's numeric
`SeasonData\Drivers` directories. For the selected season, the window scans
the installed `.veh` files, includes entries whose `Classes=` contains that
season, excludes `NotAI` vehicles such as safety cars, and matches the exact
driver identity to that season's `.rcd`. It shows the resulting vehicle path,
raw RCD values, and source verification for every selectable entry.

1. Select the season and set the expected human count.
2. Mark every vehicle that a human will use as `HUMAN CAR`. Use vehicle identity,
   not the human profile/display name.
3. Mark the desired opponents as `AI BOT`, or fill all remaining slots.
4. Enable the co-op AI runtime and save.
5. Restart every game instance before creating the multiplayer session.

The GUI makes `HUMAN CAR` and `AI BOT` mutually exclusive. Independently, the
runtime resolves each selected `.veh` to `VehicleRosterIndex` and rejects any
index already occupied by an active human. Thus a Nicky Lauda profile using
Michael Schumacher's Ferrari prevents Michael Schumacher from being created as
a separate bot even if the GUI reservation was omitted.

The stock multiplayer player list is limited to 16 humans, but that is not the
vehicle-grid limit: the engine's vehicle array and multiplayer slot-sync mask
both contain 104 entries. Host-owned bots are hidden from the human player list,
so `Fill free AI slots` fills every non-human racing entry in the selected
season (for example, 20 bots plus two humans in a 22-car season). The runtime
retains the engine's 104-vehicle bound.

Bot admission is dynamic (`MinHumans`):

- The **host** adds bots as soon as `MinHumans` human cars are present
  (default `1`, so the host alone gets the grid immediately).
- **Clients** still wait for `ExpectedHumans` so their role-2 replicas are
  created at the same point as the host's role-1 cars.
- The admission check re-runs on every driver-entry pass and network slot
  sync, so bots can also appear after a player joins, and missing bots are
  retried after a player leaves until the requested count is reached.
- Bots are host-owned and persist when a human disconnects; only the local
  human count changes.
- A previous build required exactly `ExpectedHumans` humans and ran once;
  `MinHumans` supersedes that gate on the host.

The full talent/AI-path evidence is in
`AI_AUTHENTICITY_AUDIT_2026-08-23.md`. Preserve the one-bot configuration as the
restoration baseline and validate progressively through complete weekends before
claiming a full grid is stable; do not use Quick Race.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/multiplayer_ai_host
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_MULTIPLAYER_AI_HOST_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/multiplayer_ai_host C:\path\to\staging`, which
refuses unowned overwrites.

## Related records

- Multiplayer driver callouts (AI entrants called as team drivers):
  `CALLOUT_MP_INVESTIGATION_2026-09-20.md` — static evidence chain, hypotheses,
  and the live disambiguation plan. No code changed yet.

> **2026-10-09:** CO-OP as an offline race was evaluated and rejected; see `COOP_OFFLINE_RACE_FEASIBILITY_2026-10-09.md`.
> **2026-10-09:** CO-OP mode definition draft: `COOP_MODE_DEFINITION_2026-10-09.md`.
> **2026-10-09:** CO-OP mode moved to product `development/mods/coop_mode` (investigation + DeepSeek handoff).
