# Pit Stop Modding Tools

All tools live in `mods/extended_vanilla_features/tools/`.
The EVF CLAUDE.md documents their usage.

## Live Monitoring

| Tool | Purpose |
|---|---|
| `watch_pitstop_crew.ps1` | Crew-by-crew animation tracker |
| `watch_pitstop_log.ps1` | Event logger with timestamps |
| `watch_pitstop.ps1` | Live dashboard table |

## Tracing

| Tool | Purpose |
|---|---|
| `trace_pitstop_enhanced.py` | Full struct diff trace (40ms poll) |
| `trace_pitstop_full.py` | Original trace tool |

## Tampering

| Tool | Purpose |
|---|---|
| `dump_crew_struct.py` | **Read-only crew roster from `actor+100` names — start here.** No Frida, no pit stop |
| `freeze_crew_byptr.py` | **Frida: per-actor freeze by exact channel pointer (current).** Fail-safe box hold |
| `freeze_pitstop.py` | Push stopEndTime, lock box forever ⚠ cumulative push — see note |
| `freeze_pitstop_frida.py` | Frida: hook RangeProp + box lock ⚠ cumulative push |
| `freeze_lollipop.py` | ~~Frida: per-actor freeze by timing signature~~ **DEPRECATED, corner labels inverted** |
| `freeze_selective.py` | Frida: freeze by baseStart range |
| `freeze_by_signature.py` | ~~Python: freeze by timing signature~~ **DEPRECATED, corner labels inverted** |

⚠ **Box-hold safety:** any tool that does `stopEndTime += N` strands the car in
the box if it is killed before its cleanup handler runs (`taskkill /F`, crash).
`freeze_crew_byptr.py` re-arms to an absolute `simTime + 3.0` each tick instead,
so the car frees itself within 3s no matter how the tool dies. Prefer it.

## Frida Probes

| Tool | Purpose |
|---|---|
| `probe_pitcrew_anim.py` | Crew event hooks (confirmed: zero events) |
| `probe_actors.py` | Dump actor info from sub_4A35F0 |
| `probe_actors_v2.py` | PhysicsChannelProcessingWithLocals hook |
| `probe_actors_v3.py` | Wide hook net with heartbeat |
| `probe_actors_v4.py` | RangeProp hook with send() |
| `log_actors.py` | Log-only channel dump |
| `activate_anim.py` | Attempt manual animation trigger |
| `probe_timer.py` | Timer function hooks |
| `probe_channels.py` | Log all channel baseStart values |

## Inspection

| Tool | Purpose |
|---|---|
| `poke_quick.py` | Quick state dump |
| `poke_tires.py` | Tire/damage inspection |
| `dump_crew_actors.py` | Python crew actor dump (works anytime) |
