# Multiplayer profile-selection investigation

Status: static corpus evidence, 2026-08-19, corrected by live evidence on
2026-08-21. This covers the multi-instance OPH workaround, not the separate
native single-host bot-grid objective.

## Live correction: isolated roots are unsupported

The static analysis below correctly identifies a parsed `+config` path and a
configurable `SAVEDIR`, but its practical conclusion did not survive the V10
runtime test. A normal shared-save client reached Select Team; a bot using a
custom `+config` and separate save root later crashed in `Lua.dll` with BEX /
`0xc0000409` (reported offset `0x39435`). `LuaScript\main.lua` also writes
relative-root configuration state.

Treat per-process `+config`/`SAVEDIR` isolation as unsupported for the tested
V10 installation until the Lua/config interaction is repaired and independently
retested. See `LIVE_EVIDENCE_2026-08-21.md`. The remaining sections preserve the
earlier static reasoning and prototype design as research evidence.

## What the game actually does

- F1 Challenge enumerates the profile files under `Save\` using the `*.PLR`
  pattern. A profile is not just a display name: the selected profile loads
  `Save\<profile>\<profile>.PLR` and supporting files in that folder.
- The `.PLR` includes `Profile Vehicle File` and `Driver Name`; those values
  determine the vehicle/driver presented when the network client joins.
- The initial player-file load happens before the command-line autojoin loop.
  Once a connection succeeds, the game explicitly enters the client
  team-selection flow.

## Existing command-line capability

`NetCommParseCommandLine` accepts the following network options: `password`,
`dedicated`, `host`, `gamename`, `connect`, `name`, and `maxplayers`.

`+connect` and `+name` are therefore appropriate for the launcher. There is
no `profile`, `playerfile`, `save`, `vehicle`, or team-selection parameter.
Changing `+name` changes only the network name; it does not select a profile
folder or `.PLR`.

There is a separate, earlier startup capability: `+config=<file>`. The game's
main entry parses it before networking and passes the selected file to its
configuration loader. `Config.ini` contains `SAVEDIR=SAVE\`; the loader stores
the supplied `SAVEDIR` string after normalizing its trailing backslash. The
profile scanner then prefixes every profile enumeration/read with that stored
directory. Consequently, an absolute `SAVEDIR` in a per-client config gives
one process its own profile tree without modifying the shared game
installation. The config argument uses a `%s`-style, whitespace-delimited
parse, so the generated config's pathname itself must contain no spaces.

## Earlier static consequence (superseded for runtime use)

Static recovery suggested that renaming folders would not be necessary for
isolation: each process can receive an
independent `SAVEDIR` through `+config`. The stock UI remains the only
recovered way to choose an active profile *within* that save root, so the safe
launch layout gives each client a save root containing only its assigned,
writable profile. A generic file rename during multi-client startup remains
unsafe because it changes a tree other processes may be using.

The live crash means that layout is not currently a safe launch path. The
shared-save collision problem remains unresolved.

## Practical improvement paths

1. **Isolated config roots (implemented but unsupported prototype):** make one durable
   copy of each profile, then launch each child with `+config` pointing at a
   generated config whose `SAVEDIR` contains only that copy. This eliminates
   shared-save races in the static design, but the tested V10 runtime crashed.
   The development launcher exposes the preserved experiment as `-BotSaveRoot`
   plus `-ProfileNames`; it is not a validated operational path.
2. **UI automation (evidence probe only):** a normal-root client can be used to
   close the one-client selection/team experiment. This cannot resolve the
   profile collision or become the finished multi-client architecture.
3. **Profile-selector mod (better):** implement a guarded, version-validated
   load-time hook that maps a launcher-provided profile name to the game's
   active profile before `PlayerNameNeverUsedProfileFileCreateRead` and before
   autojoin. This would let the launcher assign a different existing profile
   to each child process without renaming folders. It needs ABI recovery and a
   one-client live validation; it is not implemented yet.
4. **Native host AI grid (end goal):** avoids profiles and extra processes
   entirely, but is a separate unfinished implementation.

## Non-solution

Do not point multiple simultaneous client processes at the same profile
folder. They can write the same `.PLR`, garage, setup, and session files, and
the current corpus contains no locking or isolation guarantee for that case.
