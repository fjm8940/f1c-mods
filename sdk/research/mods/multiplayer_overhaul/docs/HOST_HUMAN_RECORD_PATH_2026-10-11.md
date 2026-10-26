# Host path for the remote human's car record (2026-10-11, handoff 11d item 4)

Goal: owner wants host AI constantly aware of remote humans. First step: find
where the HOST applies the client's car. Run 2 proved 0x463D10 gets 0 calls on host.

## Static chain (DERIVED, raw Hex-Rays, no live test yet)
1. `0x45F710` drains the NetComm receive loop, per record: `0x45C8F0` (record time
   clamp to now / now - 0x7D108C) then `0x45D890`.
2. `0x45D890`: class-4 header (`& 0xE0000000 == 0x80000000`) = control messages.
   Everything else is copied into ring `0x7D2E50` (1024 x 432 B) ONLY if
   `this[32] == 0` and (header byte3 & 7 == session byte `0x7C56A4` or bit `0x8000000`).
3. `0x45F710` again: if `0x461440` (phase 1/2 and `0x7D1074`) the ring is copied into
   the per-car store `0x7CD2A8` (`0x465300`, 560 B per car, newer wins via `0x464B40`);
   else each ring record is re-sent to every other peer (relay).
4. `GamePostMainLoop 0x42D8A0` -> `BufferReset(0x7D2E50)` `0x4B3B40` -> `0x4B3400` per record.
   Car records (sub 0/7) go to `0x463D10` only if `veh+0x32C8 == 2`.
5. `0x4B3400` sub 1..6 -> `0x410BC0` = DynMan objects (wheels/parts), not cars.

So host 0 calls on 0x463D10 means one of: records never reach `0x45D890`, fail the
session-byte check, or the human's slot mode on host is not 2. Unknown which.

## Probe (dev build, not released)
`dll/coop_mode.cpp` host probe: detours `0x45D890` and `0x4B3400`, CO-OP host only,
logs every 5 s per slot:
`host probe: slot N NAME: arrive A session_ok S apply_seen P mode M (now X) hdr H session B`.
Read: A=0 -> client car not sent as sub 0/7 to host; S<A -> session byte drop;
P>0 and mode != 2 -> slot mode gate (look what mode host uses for humans).
Build sha256 41b10359...a013, staged `%TEMP%\coop_send_hostprobe`, installed on owner PC
2026-10-11. Tester A keeps fixed150 (probe only logs on host). Tests 5/5 PASS.
Before-states: `dll/coop_mode.cpp.BEFORE_20261011_hostprobe`, `dll/ADDRESS_INVENTORY.md.BEFORE_20261011_hostprobe`.

## Pending
Two-PC session with Tester A (same test as fixed150). Owner sends host
`coop_mode_log.txt` lines `host probe:`. Then: measure age on the real path, then
option 3 (AI safety margin around humans).

## Live result 1 (2026-10-11 log time, two-PC, Tester A = slot 1, OBSERVED)
`host probe: slot 1 Tester A: arrive 300 session_ok 300 apply_seen 300 mode 2 (now 2) hdr 21003401 session 1`
every 5 s. So the client's car records (class 1 sub 0) reach the host at ~60/s, pass the
session byte, reach 0x4B3400, and the slot mode is 2. Per static code that means
0x463D10 -> 0x500010 (store into veh+0x4030) runs on the host. Correction: the earlier
"0 calls on host" came from the net age report, which only runs inside the 0x500560
draw hook (client-oriented); it never proved the store is skipped. Inference: on host the
human's record is stored but not drawn via 0x500560, or the report never fired.
Host bots/slot 0 also show apply_seen 300 (local ring 0x83EE60 also runs 0x4B3400; mode 1/0, not applied).
Next probe: count 0x463D10 / 0x500010 / 0x500560 per slot on host, and record age at store.
