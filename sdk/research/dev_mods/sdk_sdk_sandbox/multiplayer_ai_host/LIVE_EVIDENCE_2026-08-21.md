# Multiplayer AI host live-evidence correction — 2026-08-21

Status: investigation evidence only. No native multiplayer-AI host or
shippable artifact exists.

## Purpose

This note records the live results that supersede the earlier static inference
that per-process `+config` files and isolated `SAVEDIR` roots were a usable
multi-instance profile-isolation mechanism. The static path recovery remains
useful evidence about intended configuration flow, but it did not survive the
active V10 installation test.

## Test results

- A client using the installation's normal shared save/configuration path
  reached the multiplayer Select Team screen.
- A bot client launched with a custom `+config` and separate save root later
  crashed in `Lua.dll` with BEX / `0xc0000409`, reported offset `0x39435`.
- `LuaScript\main.lua` writes relative-root configuration state. The observed
  crash is consistent with that runtime path being incompatible with the
  redirected root in this installation, but the precise Lua failure mechanism
  is not yet proven.
- Therefore isolated `+config`/`SAVEDIR` roots are unsupported for the tested
  V10 installation. They must not be described as working until repaired and
  independently retested.

The failed trial does not erase the static evidence. It changes the conclusion:
the executable can parse the setting, but the complete V10 runtime does not
currently operate safely with the redirected root.

## Multiplayer join proof

- One `noPlayer=true` client joined a live normal multiplayer host through the
  host machine's LAN address and reached Select Team.
- This proves the stock network join and client team-selection transition. It
  does not prove team confirmation, entrant reservation, a host roster entry,
  or race participation.
- The host roster did not show a confirmed additional entrant before the test
  was interrupted.
- Loopback was not reliable in the tested multi-process setup. The successful
  route used the host's reachable LAN address and game port `34397`.

## Retained artifacts and trace limit

Relevant retained captures include:

- `build/team_confirm_screen_20260819_002.png`
- `build/team_confirm_trace_bot_20260819_001.log`
- `build/host_after_bot_20260819_001.png`
- `build/bot_after_confirm_20260819_001.png`
- `build/bot_after_confirm_20260819_002.png`

The passive Frida trace attached only `WS2_32!send` and `WS2_32!sendto`. It
logged attachment but no packet during the attempted UI interaction. This is
inconclusive: the game may use `WSASend`, another path, or transmit only after
a later state transition.

PID-specific window targeting for rectangle and mouse actions was observed to
select the requested game window. The screenshot/capture path was not made
PID-aware and still selected the first game window during the test, so the
capture filenames alone must not be treated as proof of which process was
captured without corroborating context.

## Current boundary

The multi-instance `noPlayer` path remains a research probe and possible manual
fallback only. It still has unresolved profile/save collisions and requires one
game process per entrant. The requested solution remains a host-owned AI roster
constructed before session publication, with human clients joining the same
race without extra bot processes.
