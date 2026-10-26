# Netcode stabilisation options: can client PCs simulate the AI in sync? (2026-10-11)

Stage: investigate/define. No code changed.

## Q: run the AI on every client, in sync with the host?
Answer: not reliably. INFERENCE from engine facts:
- F1C physics/AI are not deterministic across PCs (float maths, frame-rate dependent steps, AI randomness,
  different human inputs/latency). Two copies of a bot diverge within seconds; in a fast corner a few
  metres.
- Lockstep (all PCs wait for all inputs each tick) would add the full ping to every frame - worse lag.
- "Predict locally + correct from host" still needs corrections; with 24 bots they would be the same warps.
Conclusion: host stays the only AI simulator; clients must DISPLAY host bots well.

## What can make client display stable
Already have: Modern 60 Hz, no 2002 budget (0.9.5); fixed 150 ms buffer on client (dev, untested).
Remaining native weaknesses (NETCODE_AUDIT G1/G5/G6/G7): one record per car, dead reckoning, socket events
via window messages, possible TCP head-of-line (O2 still open: send path is COM vtable-dispatched, not
resolved statically today), hide after 2.4 s.

Recommended: N2 own bot stream (spec exists in SPEC_NETCODE_2026-10-09):
- Host sends every bot's state over CO-OP's own UDP channel (same as ManifestPort) at 60 Hz with sequence
  number + host time. UDP only: a lost packet is skipped, never blocks the next (no TCP head-of-line).
- Client keeps a buffer per bot, clock-synced to host, and INTERPOLATES between two real states (true
  smoothing, full speed, fixed delay). Short loss: extrapolate briefly then blend back, no snap.
- Applied through the native remote apply 0x4C73F0, already proven in the 2026-10-09 SPIKE (a car driven by
  our own stream instead of NetComm). Native records kept as fallback.
- Also lets us log real ping/loss per second on both PCs.
Cost: large (several sessions), needs two-PC tests; risk medium. Pending owner OK.
