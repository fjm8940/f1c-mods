# Review: Codex N2 stages 1+2 (2026-10-11, Claude)

Diff reviewed: `dll/coop_mode.cpp` vs `coop_mode.cpp.BEFORE_20261011_n2` (+484 diff lines), ini, record
`IMPLEMENTATION_N2_2026-10-11.md`. Build re-run by Claude: OK. Tests: 5/6 PASS; `test_usage.py` FAIL when it
reads the installed live game log (session-counter assertions; Codex documented it, not N2-caused - unverified).
Verdict: **NOT READY for a live test.** Two blockers, three should-fix. Paste the fix prompt below into Codex.

## Blockers
B1. `bot_stream_send_tick`: `records + base * BOT_STREAM_ENTRY` on `uint8_t records[256][65]` advances by
    ROWS, not bytes. With 22 bots the second datagram (base 18) reads row 1170 -> past the stack array:
    garbage records sent, possible crash. Fix: `records[base]` (or `&records[0][0] + base * BOT_STREAM_ENTRY`).
B2. NAT: host sends unsolicited UDP to `<peer IP>:34501`, peer IP taken from the MF sync table. Tester A is on a
    mobile/NAT link (203.0.113.1); his router drops unsolicited inbound UDP, so the client never receives
    anything. Fix: client sends a small hello (`CB01?` + 4 bytes) from its 34501 socket to `<host>:BotStreamPort`
    every 1 s while network_state()==3 and BotStream>0; host records the source address:port of each hello
    (own peer table, TTL 5 s) and sends the stream there. Host must allow inbound UDP BotStreamPort (document in ini).
    Do not use the MF peer table any more (also drop the new g_mfb_peer_lock changes if no longer needed - keep
    them if harmless, but say which).

## Should fix
S1. Send pacing uses GetTickCount (~15.6 ms resolution): `tick - last_send >= 16` mostly needs two ticks ->
    ~32 Hz, not 60. Use `bot_stream_now()` (QPC) with an accumulating next-send time; Sleep(1) loop on host.
S2. Clock offset: every 10 s the offset is reset to the CURRENT sample (could be a late one) -> the render target
    jumps by the lateness, i.e. a visible bot jump every 10 s. Use two windows: offset = min(previous window min,
    current window min); roll windows every 10 s (same as `g_offset_min/g_offset_min_next` in smooth code).
S3. Client receive only runs in state 3 with a 50 ms SO_RCVTIMEO; report line only prints when a recv returns.
    Fine, but the 5 s report must also print when nothing arrives (that IS the useful failure signal:
    `client rx 0 pkts/s`). Check `tick - last_report` outside the `got` path (it already is - confirm) and make
    sure last_report is initialised (currently 0 -> first report at once; OK).

## Notes (no change needed)
- Net id match by `veh+0x4894` on both PCs: assumption the native MP id is the same on host and client.
  Plausible (native MP uses it as the record id), unproven; stage 1 log will show it (`bots N` should = bot count).
- N2 hooks are not installed in COOP_DEV_TOOLS builds (same addresses as the spike). Acceptable; noted.
- Open live question stays: does 0x4C71C0 fire for every owned bot on the host at the send rate? Add the
  existing `g_bot_encode_count` to the host 5 s line (encodes/s) so stage 1 answers it.

## Round 2 (fix1 reviewed 2026-10-11, Claude)
B1, B2, S1, S2, S3, encodes/s: all fixed as specified (diff vs `coop_mode.cpp.BEFORE_20261011_n2fix1`).
Peer table is worker-thread only (no lock needed); MF peer lock reverted. Minor, accepted: host loop uses Sleep(1)
(~1-16 ms depending on timer resolution) with catch-up, so send timing jitters a few ms.
Claude rebuild sha256 2b2e2bdc7579c22b587b401cc2be81ebc839b0b2e8647fac13dcf9049fc32219 (differs from Codex
93E6BA02...: separate build, not compared byte-wise). Tests 5/6 PASS, test_usage FAIL on live log as before.
Verdict: **READY for stage 1 live test** (BotStream=1, measure only). Staged `%TEMP%\coop_send_n2s1` (dll + ini),
both PCs. Host: allow inbound UDP 34501. Expected: host `bot stream: host sent ~60xD pkts/s to 1 peers,
encodes ~?/s, bots 22`; client `bot stream: client rx ... loss ... late ... bots 22 ... stream-driven 0`.

## Live stage 1 result (2026-10-11-18:40 log time, Tester A client log copied to owner Desktop)
OBSERVED client: `bot stream: client rx 119-120 pkts/s loss 0.0-0.4% late avg 8-12 ms max 17-40 ms, bots 22,
fallback 0 stream-driven 0` every 5 s. Host: `sent 120 pkts/s to 1 peers, encodes ~1300-1700/s, bots 22`.
Stage 1 PASS: stream complete, low loss, low jitter; net id match by veh+0x4894 works (bots 22 on client).
Sato (slot 39) still warped at the same time (BotStream=1 draws natively).
Material finding: Tester A's whole log (all runs, incl. fixed150 config SmoothMs=150) has ZERO `smoothing:` and
ZERO `net age` lines -> the 0x500560 draw hook never ran with a 0x5F0708 record on his client, so the 150 ms
buffer never worked (same as on host). Tester A runs a different exe build ("layout probes OK (other exe build)",
1 function already detoured by another module). Whether 0x4F03E0 (stage 2 apply point) runs on his client is
unknown; stage 2 counters answer it: stream-driven > 0 = works; fallback 0 and stream-driven 0 = hook not called.
Next: host sets BotStream=2 (client follows), one race.

## Live stage 2 attempt 1 (2026-10-11 host start, BotStream=2) - FAIL, fixed by Claude (fix2)
Owner: all AI disappeared on Tester A's screen at race start. Host stream normal (120 pkts/s, 22 bots).
Inference: the apply point 0x4F03E0 IS called on the client (stage 2 took over), but `bot_remote_update_hook`
passed a 64-byte stack buffer to 0x4C73F0, which reads a full 432-byte record (proven spike used zeroed
uint32_t[108]) -> stack junk in the record -> cars invisible. Fix2 (Claude, small): zeroed uint32_t[108] buffer.
Before-state `dll/coop_mode.cpp.BEFORE_20261011_n2fix2`. Build OK, tests 5/6 (test_usage live-log as before).
Staged `%TEMP%\coop_send_n2s2` (dll sha256 06169ac69cf7216b8561e72c9d3ed7e98438e286b308d3fdce6d855cb1834b3d,
ini BotStream=2). Not live-tested yet; Tester A's log of the failed race still wanted (fallback/stream-driven counts).

## Live stage 2 attempt 2 (fix2, 2026-10-11-18:50, Tester A log) - WORKING (visual quality pending owner/Tester A)
OBSERVED client: `client rx 119-120 pkts/s loss 0-0.2% late avg 9-12 ms max 21-39 ms, bots 22, fallback 0,
stream-driven ~4400` per 5 s (= 880/s = 22 bots x ~40 applies/s; apply rate per bot = 0x4F03E0 call rate).
All bots `visible again` at 18:47:28 (session start), no HIDDEN lines after. Stage 2 applies every bot from the
stream with zero native fallback. Pending: Tester A's visual report (warps on ping jumps, contact feel).

## SmoothMs=80 trial (2026-10-11, Tester A client ini) - FAIL by owner/Tester A visual report
"cars were jittering and warping really bad", Tester A reverted to 150. Tester A log of that run not yet read.
Suspect (inference, unverified): host stamps every datagram with SEND time (worker loop, Sleep(1) pacing), not the
time each record was CAPTURED by the encoder hook, so record time vs content jitters by up to one send period;
plus the delta cap (1/SendHz) freezes a car when the chosen sample is older than one period. 150 ms hides this,
80 ms exposes it. Fix candidate for the clean-up round: per-record capture time (game session time or QPC at
encode) in the datagram, and interpolate between the two samples around the target instead of nearest-before.

## Warp cause found (Tester A log 18:53-18:59 + host log, 2026-10-11)
OBSERVED client: mostly `late max 20-37 ms, fallback 0`, but stalls: 18:55:04-09 (late max 1622/1410 ms,
fallback 1562+1254), 18:58:27-32 (max 1406/1397 ms, fallback 682+418, then 153 pkts/s burst), 18:58:47
(max 810 ms, fallback 176). Loss 0 in the stalls -> packets held then delivered in a burst (buffering on the path).
Host side same minutes: Tester A->host records steady 298-301 per 5 s. So: one-way freezes host->Tester A of 0.8-1.6 s,
upstream fine. Both SmoothMs 80 and 150 runs had them -> the 80 trial failure is NOT proven to be caused by 80
(the earlier capture-time suspicion stays a minor issue). Where the buffering is (host upload, ISP, Tester A mobile
downlink) is unknown.
Code consequence: during a stall > 0.5 s the bot falls back to native (also frozen) -> warp, then snaps on
recovery. Fix (next Codex round, priority 1): on stall keep extrapolating from the last two stream samples (cap ~2 s),
never hand back to native mid-race; on recovery blend over ~0.3-0.5 s; drop samples that arrive in a burst
older than the target.

## Race 19:07 (BotStream=1) + Claude AI capture (2026-10-11-19:15 UTC)
Tester A log: 00-00-00-00-00-00 no stall > 200 ms; from 19:10:42 stalls every 10-60 s, late max 0.4-2.1 s (owner saw
"over 500 ping"). Claude's live reads began 19:10:38 (f1c_drivers, then 5 Hz memory-mcp subprocess loop from 19:11,
elevated 10 Hz 1 MB ReadProcessMemory dumper 19:13-19:14). Host frames stayed clean (worst 16 ms) and earlier
stalls (18:55, 18:58) happened without reads, so causation is NOT proven; correlation is strong. Rule until tested:
no live memory capture during a two-PC test unless that is the test. Test: one race with no readers, compare.
AI capture (`%TEMP%\coop_ai_capture`, disposable): bots DO register Tester A - his vehicle is in their 5-car proximity
list (0x4174) and opponent records (0x42CC stride 28, id = veh+0x4894 = 1) with modes 2 (attempt) / 5 (hold) /
16 (approach), same as for bots. Close passes 2.4-4 m at 150-280 km/h (Alonso 95.4 s, Malcharek 117.3 s, Button
82-83 s), no Raikkonen contact inside the captured window. memory-mcp target_present/lateral_request were 0 for
every bot slot >= 32 all capture (also bot-vs-bot) -> those reader fields do not cover slots 32+; not evidence.
Released bot slot 37 keeps a world position equal to Tester A's (d=0.0) with empty AI lists; inactive (not in drivers
list) - noted, not linked to incidents.
