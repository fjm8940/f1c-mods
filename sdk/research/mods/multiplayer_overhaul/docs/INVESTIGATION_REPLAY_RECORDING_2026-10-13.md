# CO-OP / multiplayer replay recording — static investigation (2026-10-13, Claude)

Owner question: do CO-OP / multiplayer sessions record replays correctly?
Method: static read of Hex-Rays output + bounded disassembly of `F1 Challenge Online.exe`
(`f1c_mod.py exe-disassemble`). **No live test. No code changed.**

## Facts (static)
- Recorder `0x4B7A90` (IDA name `PitTimingUpdate` is wrong) is called from `0x4BA6C0` via
  `0x4B9870` only while `byte_7C56AC` (freeze flag) is 0 and recorder state == 1.
- Per tick (wall clock > 0.025 s and session time moved > 0.001) it loops `i < [0x5C329C]` over the
  active vehicle pointer array `[0x7C54F0 + i*4]` (disasm 0x4B7B35) and encodes each car with
  `0x4C71C0` (52-byte class-1 pose record, id = veh+18580, time = `dbl_8AFEA0`).
  **No controller-mode filter**: local (0), AI (1), remote (2) cars are all recorded.
- Slot side (`0x4B5C50` identity list, `0x4B6190` per-slot extras) walks slots 0..103 with
  controller != -1, so CO-OP bots in slots 32+ are registered.
- Incoming network event ring: class-1 subtype 0/7 (car pose) is skipped; other classes
  (2/3/4/6, class-1 other subtypes) are stored. Remote cars are recorded from their applied state, not
  from the packet.
- Remote cars (mode 2, client bots and remote humans) get pose from native apply `0x4C73F0`
  (vanilla path and CO-OP N2 stream both use it), so the recorder captures the interpolated pose the
  local PC displays.

## Inference (not proven)
- Each PC records its own replay; a client's replay of bots = host stream as it arrived (N2 stalls,
  degraded/hidden cars and jumps are baked in). Host replay of bots = true simulation.
- Host `HostTimeWarp` (garage only, non-race sessions): recorder samples on wall clock, so at x10
  records are ~10x sparser in session time; warped stretches replay coarse. Race unaffected.
- SP-style freeze (`byte_7C56AC`) pauses recording; broker MP ESC menu does not freeze, so MP
  recording continues while menu open (expected).
- Known OPEN bug `replay-ai-bug-open` (AI simplified after instant replay) may hit host bots if host
  watches an instant replay mid-race. Not checked in CO-OP.

## Pending live validation (owner)
1. Host + client CO-OP race, few laps, save replay on both.
2. Load each: all humans + all bots (slots 32+) present, names right, moving smoothly, no frozen car.
3. Host: instant replay mid-race (R), then check bots drive normally afterwards.
Pass = every car visible and moving in both replays; fail details to this file.
