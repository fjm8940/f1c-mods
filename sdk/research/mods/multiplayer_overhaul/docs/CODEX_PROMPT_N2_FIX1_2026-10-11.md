# Codex prompt: N2 review fixes round 1 (2026-10-11)

Paste below the line into Codex.

---

Claude reviewed your N2 stage 1+2 change in `%USERPROFILE%\Desktop\F1\development\mods\coop_mode`.
Read `docs\REVIEW_N2_CODEX_2026-10-11.md` and fix B1, B2, S1, S2, S3 and the encodes/s note exactly as described.
Same rules as `docs\CODEX_PROMPT_N2_BOT_STREAM_2026-10-11.md` (backups: `<file>.BEFORE_20261011_n2fix1`, no
deletes, UTF-8 no BOM, LF, ADDRESS_INVENTORY for any new address, build with build.bat, all tests, do not run or
install the game).

Specifics:
- B1: fix the 2D-array pointer arithmetic in `bot_stream_send_tick`; add a test or static assert that a 22-bot
  tick produces 2 datagrams with the right entries (pure Python mirror in `tests\test_n2_protocol.py` is fine).
- B2: hello/peer learning as written in the review. Hello format: `"CB01?"` + u32 client nonce. Host peer table
  max 8, TTL 5 s, log `bot stream: peer a.b.c.d:port` once per new peer. Client sends hello to the host IP it
  already uses (`g_sync_host_ip`) on `BotStreamPort`. Update the ini comment: host must allow inbound UDP
  BotStreamPort; clients need nothing.
- S1: QPC pacing at SendHz.
- S2: two-window minimum offset.
- Host 5 s line: `bot stream: host sent N pkts/s to K peers, encodes M/s, bots B`.
Update `docs\IMPLEMENTATION_N2_2026-10-11.md` with a "Fix round 1" section (what changed, new sha256, tests).
Stop and report.
