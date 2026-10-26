# Network stall analysis, CO-OP race 19:10-19:14 UTC (2026-10-11)

Owner report: Tester A hit again, ran out of road, race ruined by spikes.
Sources: Tester A `coop_mode_log.txt` (copy on owner Desktop, 6.6 MB) and host
`Documents\F1 Challenge V10\plugins\lightloader\coop_mode_log.txt`. Times UTC.

## Facts (from logs)
- Host ran `BotStream=1` (Tester A log: "host uses Modern 60 Hz BotStream=1"). Tester A
  `stream-driven 0` the whole race: the bot stream drove NO cars. Bots on Tester A's
  screen came from native game netcode. The stream was only a line probe.
- Host side clean all race: sent steady 119-120 pkts/s to 1 peer, worst frame
  16 ms (one 421 ms frame at session load 19:10:39, before lights).
- Tester A side: loss ~0, but late max 0.4-2.1 s in 19 of 44 five-second windows.
  Packet rate dips (76-98/s) then bursts (150-163/s): packets held somewhere on
  the path, then delivered together. Classic queueing/buffering, not loss.
- First race same evening 19:07-19:09: clean (late max <= 40 ms).
- Stalls of the same shape all evening: 18:28, 18:30-18:31, 18:43-18:44, 18:47,
  18:55, 18:58, 19:05-19:06 - long before Claude's first live read (19:10:38).

## Conclusions
- Claude's live memory reads did NOT cause the stalls (disproven: stalls before
  any read; host send rate and frames unaffected). Retires handoff 11e Open item 1.
- Cause is the network path between the PCs, not the mod or host CPU. Host hands
  packets to Windows on time; they arrive up to 2 s late in bursts.
- Inference (unproven): native game packets share that path, so bots froze then
  jumped on Tester A's screen, and host AI saw Tester A's car freeze/jump. That fits the
  crash. Direction Tester A->host not measured.
- Location of the queue unknown: Tester A Wi-Fi/last mile, host uplink, or ISP route.
  Irregular timing; Wi-Fi background scans or an upload hog (cloud sync,
  Steam/Windows Update, stream) are common causes of this exact pattern.

## Dedicated server question
Helps only if the bad segment is the host's home line or the route between the
two ISPs. Does not help if it is Tester A's Wi-Fi/line (every packet still crosses
it). Also big build: F1C AI must still simulate somewhere (separate project
`project_f1c_dedicated_server`). Locate the queue first.

## Next (pending owner tests)
1. During next race both run `ping -t 1.1.1.1` and to each other; spikes on
   Tester A's 1.1.1.1 ping = Tester A side. Ask Tester A: Wi-Fi or cable; pause uploads.
2. Fix3 extrapolation through gaps (handoff 11e item 2a) masks stalls up to ~2 s
   only when BotStream=2 drives bots; does not fix native-netcode cars.

## Update: owner line test (Waveform, 2026-10-11 evening)
- Host (Matias) line: grade F. Unloaded 32 ms, download active +55 ms, upload
  active +1712 ms. Speed 149.7 down / 5.7 Mbps up.
- Upload-load latency (+1.7 s) matches the observed 0.4-2.1 s stalls host->Tester A.
  Strong evidence the queue is the host's upload buffer (router/modem bufferbloat).
- Bot stream cost (code, `coop_mode.cpp` ~4193/4377): 22 bots x 65 B + 2 x 17 B
  header per tick, 60 Hz = ~88 KB/s = ~0.73 Mbps per peer incl. UDP/IP. Native
  game netcode also sends (size not measured). Two peers ~1.5 Mbps stream alone.
- Inference: any upload burst (game, cloud sync, Discord, updates) fills the
  buffer and every packet waits up to ~1.7 s. Fix = router SQM/QoS upload cap
  ~85-90% of 5.7 (about 5.0 Mbps), then re-run Waveform (target A/B).
- Dedicated server on a VPS would also bypass the host upload. Fix the router first.
- Host modem checked (owner screenshots): DNA Mesh WiFi F-3896 (Sagemcom, sw
  FAST3896_DNA_sw203.0.113.1g). No QoS/SQM and no bridge toggle in My HomeBox or
  Access Control (Advanced Options = ping block, IPsec/PPTP, remote mgmt, UPnP,
  SIP ALG only). SQM needs own router; bridge mode must be requested from DNA.
- Unknown second wired LAN device 00-00-00-00-00-00 (192.0.2.10): identity
  pending; possible upload source.

## Update: BotStream=2 warping explained (Tester A log 18:42-18:55)
- Every stall window shows fallback 500-1450 per 5 s (e.g. 18:43:43 fallback 1166,
  18:55:04 fallback 1562); clean windows fallback 0. Code: `bot_stream_pick` drops
  samples older than 0.5 s, `bot_remote_update_hook` then calls native trampoline;
  delta capped at one send period so cars freeze first. Freeze -> native pose ->
  snap back to stream = the reported warping. Cause proven from log + code.
- Modern mode sends all cars natively at 60 Hz, 8 MB/s budget; with BotStream=2
  bots go twice over the 5.7 Mbps upload.
- Fix spec: CODEX_PROMPT_N2_FIX3_2026-10-11.md (no mid-race fallback, hold/extrapolate,
  blend recovery, BotStreamHz 30, native bots 10 Hz, CB02 capture times).

## Update: segment test (host PC, ~20:10 UTC, game process open, 120 pings each, parallel)
- modem LAN 192.0.2.10: p50 1, max 2 ms. DNA first hop 203.0.113.1: p50 23, p99 61,
  max 494. 1.1.1.1: max 480. VPS local: max 465.
- FACT: PC, cable and modem LAN side clean; spikes start between modem and DNA's
  first hop (cable/DOCSIS upstream). Whether self-induced bufferbloat or a line
  fault is not separated (game open, load unknown). Modem uptime 28 d.
- Next: owner power-cycles modem, re-run same test idle (game closed); if DNA hop
  still spikes on an idle line -> DNA line fault ticket (signal/upstream).

## Update: after modem power-cycle (~20:25 UTC)
- Waveform after restart: F, unloaded 23 ms, download +46, upload +2075 ms; 196.6/6.2 Mbps.
- Ping under Waveform load (ran concurrently): modem LAN max 1 ms; DNA first hop
  p90 159, max 1870 ms -> queue is on the modem->DNA upstream (FACT).
- Idle re-test 90 pings (game closed, owner idle; second LAN device 00-00-00-00-00-00
  not excluded): DNA first hop p50 21, p99 242, max 334 ms, 5/90 over 150 ms.
- Conclusion: restart did not fix it. Idle upstream spikes 0.2-0.3 s plus 2 s
  bufferbloat under upload. Next: DNA fault ticket (upstream signal/congestion),
  exclude the .100 device; SQM router covers only the bufferbloat part.
