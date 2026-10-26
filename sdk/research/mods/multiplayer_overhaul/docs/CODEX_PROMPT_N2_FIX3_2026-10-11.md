# Codex prompt: N2 fix round 3 - stalls, warping, upload diet (2026-10-11)

Paste below the line into Codex.

---

Work in `%USERPROFILE%\Desktop\F1\development\mods\coop_mode`. Read first:
`docs\STALL_ANALYSIS_2026-10-11.md`, `docs\REVIEW_N2_CODEX_2026-10-11.md`, `docs\HANDOFF_CLAUDE_2026-10-11e.md`,
`docs\NETCODE_AUDIT_2026-10-11.md`. Same rules as `docs\CODEX_PROMPT_N2_BOT_STREAM_2026-10-11.md` (backups
`<file>.BEFORE_20261011_n2fix3`, no deletes, UTF-8 no BOM, LF, ADDRESS_INVENTORY for any new address, build with
build.bat, all tests, do not run or install the game). Base: n2fix2 source (dll sha256 06169ac6...).

## Facts you must design around (from live logs, not guesses)
- Host upload is 5.7 Mbps with +1.7 s bufferbloat under load. Host sends on time; client receives 0.4-2.1 s
  gaps followed by bursts, loss ~0.
- With BotStream=2 every gap > 0.5 s makes `bot_stream_pick` return 0, `bot_remote_update_hook` falls back to the
  native trampoline (500-1450 fallbacks per 5 s window), then snaps back to the stream when the burst lands. Owner:
  "cars warping like crazy". During the gap `delta` is capped to one send period, so the car also freezes.
- Modern mode (`send_interval_hook`, `conn_budget_body`) makes native NetComm send all 24 cars at 60 Hz with an
  8 MB/s budget. With BotStream=2 bots are therefore sent twice (native + stream ~0.73 Mbps per peer).

## Tasks (priority order)
1. **No mid-race native fallback.** In `bot_remote_update_hook`, once a net_id has been stream-driven in this
   session, never call the trampoline for it again while BotStream=2 and network_state()==3 (only on session
   reset or bot_stream_reset). If no sample is fresh, re-apply the newest sample with growing extrapolation.
2. **Extrapolate through gaps.** First verify in the decompile (f1c_ida_decompiled front door: `inspect 0x4C73F0`,
   `inspect 0x500560`) how the record time at +4 and the `time` argument are used: does a larger
   `time - record_time` give dead reckoning (pos + vel x dt)? Record the answer as a finding in
   `f1c_ida_decompiled/findings.jsonl` (tier + evidence). If yes: allow delta up to `BotStreamHoldMs` (INI,
   default 500, matches native 0.5 s cap), then hold the pose (delta stays at the cap) until data returns. If no:
   implement linear extrapolation of position from the last two samples only if the record position fields are
   proven in the decompile; otherwise hold pose and say so. Never teleport backwards in time.
3. **Recovery without snap.** When samples return after a gap > 100 ms, ignore burst samples older than the
   current target (already true) and catch up over `BotStreamBlendMs` (INI, default 400): move the target clock
   from the held time to the live target linearly instead of jumping. Document whether native BoneSpring already
   smooths the step.
4. **Upload diet (host).**
   a. New INI `[Net] BotStreamHz` (default 30, range 15..60): stream send rate, independent of SendHz.
      `bot_stream_pick` max_gap uses the stream period.
   b. When the host runs BotStream=2 (from the host's own ini), `send_interval_hook` returns
      1/`NativeBotHz` (INI, default 10) for owned AI slots (`slot_is_owned_ai`), SendHz for humans. Clients still
      need native state for bots that are not yet stream-seen; 10 Hz covers that.
   c. Host 5 s line adds stream bytes/s: `bot stream: host sent N pkts/s B KB/s to K peers ...`.
5. **Per-record capture time (protocol CB02).** Each entry carries its own capture clock as a float offset (ms,
   host_clock minus captured_clock) after the id byte. Bump magic to `CB02`; reject CB01 with a one-time log.
   Client stores `host_clock - offset` as the sample time. Update the Python protocol mirror test.
6. **Diagnostics (client 5 s line):** add `holds N longest M ms recover-blends R` and keep fallback count (must be 0
   after the first stream-driven frame in a race).
7. **Clean-up:** BotStream=2 default; remove dead 150 ms client buffer code (0x500010/0x500560 hooks never ran on
   the client) and the host probe; clean ini (no 2002 lists); README: host must open inbound UDP 34501; fix
   `tests\test_usage.py`.

## Tests
- Pure tests for: pick with a 1.5 s gap (no fallback, hold, then blend), burst after gap (no backward step),
  CB02 encode/decode, 22 bots at 30 Hz = 60 pkts/s and expected bytes/s.
- Build both release and dev configs.

Update `docs\IMPLEMENTATION_N2_2026-10-11.md` with a "Fix round 3" section (what changed, decompile answer for
task 2, new sha256, tests, INI keys). Stop and report.
