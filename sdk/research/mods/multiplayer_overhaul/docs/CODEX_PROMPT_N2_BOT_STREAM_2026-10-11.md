# Codex prompt: CO-OP N2 own bot stream (stages 1+2) - 2026-10-11

Owner: Matias. Author: Claude (spec + review). Implementer: Codex. Claude reviews the diff before
anything is built for the owner. Paste everything below the line into Codex.

---

You are implementing stage 1 and 2 of "N2 own bot stream" in the CO-OP mod for F1 Challenge 99-02
(32-bit Windows game, DLL mod built with MinGW). Work only in
`%USERPROFILE%\Desktop\F1\development\mods\coop_mode\`.

## Read first (mandatory)
1. `%USERPROFILE%\Desktop\F1\AGENTS.md` (preservation, encoding, documentation rules).
2. `development\mods\MOD_STANDARD.md`.
3. `development\mods\coop_mode\docs\SPEC_NETCODE_2026-10-09.md` (section N2),
   `docs\NETCODE_OPTIONS_SYNC_AI_2026-10-11.md`, `docs\AI_JITTER_CLIENT_2026-10-11.md`,
   `docs\NETCODE_AUDIT_2026-10-11.md`.
4. `dll\coop_mode.cpp`: the LINK dev code (`g_link_*`, `link_thread`, `link_pick`,
   `state_encode_hook`, `remote_update_hook`, around lines 4180-4400) is a WORKING prototype of exactly
   this for one car (proven live 2026-10-09). Reuse its approach. Also read the manifest UDP thread
   (`manifest_net_thread`, ~line 1911), the smoothing hooks (`remote_store_hook`, 0x500010/0x500560,
   ~line 4550-4780) and `dll\ADDRESS_INVENTORY.md`.

## Goal
Bots are simulated on the host only. Today their positions reach clients through the native 2002
netcode (one record per car, dead reckoning, can stall), so bots warp on the client when ping jumps.
N2: the host sends every bot's native state record over the mod's own UDP channel; the client draws
each bot from a buffer at a fixed delay in the past. Humans, join, sessions, garage, pit sync: unchanged.

## Native facts to use (DERIVED/proven, do not re-derive)
- `0x4C71C0` state encoder (thiscall vehicle, out): writes the 64-byte native car record; hook already
  exists as `state_encode_hook` (dev only). Record header dword low byte = net id; float at +4 = sender
  session time.
- Net id of a car = int32 at `veh+0x4894` (18580). Host and client slot numbers can DIFFER; map by net id
  (scan `f1c_vehicle_slot(0..MAX_VEHICLES-1)` and compare `veh+0x4894`), never by slot.
- `0x4F03E0` mode-2 (remote) car update (thiscall controller, float time); controller's first dword =
  vehicle. Hook already exists as `remote_update_hook` (dev only).
- `0x4C73F0` native remote apply (thiscall vehicle, record, float time). The LINK code shows how to
  rewrite record[0] low byte to the local `veh+0x4894` and the time at +4 before calling it.
- `slot_is_owned_ai(slot)`, `coop_host_now()`, `network_state()`, `coop_mode_is_selected()`,
  `g_stopping`, `log_line`, `f1c_hook_registry_install` already exist - use them.

## Stage 1 - stream + measure (no visual change)
- INI `[Net] BotStream=` 0 off, 1 send+measure only, 2 send+apply (stage 2). Default 1. Port: `BotStreamPort`
  default 34501 (UDP). Client gets the host's BotStream value via the manifest NET line (extend the
  existing `NET` line backward-compatibly: old clients must still parse it).
- Host: in the encoder hook, for every owned bot copy the 64-byte record into a per-net-id latest slot.
  A sender thread (or the existing thread pattern) sends one datagram per tick at SendHz (default 60) to
  every known client: header {magic 'CB01', u32 sequence, double host_clock (QueryPerformanceCounter
  seconds), u8 count} + count x {u8 net_id, 64-byte record}. Max datagram 1200 bytes: split into several
  datagrams if needed. Client addresses: reuse peers the host already knows from the manifest exchange.
- Client: receive thread, per net id ring of 64 samples {host_clock, arrival_clock, record}. Clock offset =
  minimum over the last 10 s of (arrival - host_clock) (same as LINK). Track per-second: packets,
  sequence gaps (loss), late (arrival - offset - host_clock), reorders.
- Log every 5 s on both PCs, one line: `bot stream: host sent N pkts/s to K peers` / `bot stream:
  client rx N pkts/s loss X% late avg A ms max M ms, bots B, offset O`.
- No hook may block the game thread; locks only around ring copies (CRITICAL_SECTION like LINK).

## Stage 2 - apply on client (BotStream=2)
- In `remote_update_hook` on the client: if BotStream==2 and the vehicle is a host bot with stream
  data: target = now - offset - SmoothMs. Pick the newest sample with host_clock <= target; if none
  or older than 0.5 s, fall back to native (call the trampoline, count `fallback`). Otherwise rewrite
  record[0] low byte to local `veh+0x4894`, write time so the native apply moves the car forward by
  (target - sample host_clock) only (max one sample gap), call `0x4C73F0`, and do NOT call the trampoline.
- Smoothing hooks 0x500010/0x500560 must skip cars that the stream drives (no double processing).
- Humans and the local player never go through the stream.
- Add `fallback N` and `stream-driven B` to the 5 s client log line.

## Rules
- Before editing any existing file, copy it to `<file>.BEFORE_20261011_n2` (do not overwrite an existing
  backup). Never delete files. UTF-8 without BOM, keep LF line endings.
- Any new raw address must be added to `dll\ADDRESS_INVENTORY.md` (test enforces it).
- Keep LINK/SPIKE code under `COOP_DEV_TOOLS` untouched; your code must work without that define,
  so move/duplicate only what you need into release code.
- Do not change version strings, release folders, the game install, or `production\`.
- Build: `cmd /c %USERPROFILE%\Desktop\F1\development\mods\coop_mode\dll\build.bat`
  (output `%TEMP%\coop_mode_build\coop_mode.dll`). Must build with no new warnings.
- Tests: run every `tests\test_*.py`; all must pass. Add a test for the datagram pack/unpack and
  NET-line parse if you can keep it pure (Python mirror or a small host-side check).
- Do not run or touch the game. Do not install.

## Deliver
1. Diff summary per function, new INI keys (also add them to `dll\coop_mode.ini` with comments in the
   existing style), build output tail, test results.
2. A record `docs\IMPLEMENTATION_N2_2026-10-11.md`: what changed, before-state files, sha256 of the DLL,
   what is untested (everything live), and expected log lines for the two-PC test.
3. Stop after stage 2. List open questions instead of guessing (for example: if the encoder hook does
   not fire for bots on the host, say so and stop; do not invent another address).
