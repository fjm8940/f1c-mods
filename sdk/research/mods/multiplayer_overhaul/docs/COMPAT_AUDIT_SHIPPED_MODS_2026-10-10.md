# Shipped mods vs Multiplayer AI Mode (CO-OP) — compatibility audit, 2026-10-10

Stage: verify (static + today's live logs). Scope: every product under `production/mods/`.
Evidence = source grep for multiplayer gates / vehicle iteration + installed mod logs from the
2026-10-10 CO-OP host session (Silverstone 2002, 22 bots in slots 32+, client Tester A).

| Mod | In CO-OP | Evidence | Verdict |
|---|---|---|---|
| mechanical_failures 1.0.5 | Active; iterates the active pointer array 0x7C54F0/0x5C329C (sees slots 32+); CO-OP event sync bridge (coop_mf_bridge.h ABI v1, test_coop_bridge_abi PASS) | mechanical_failures.log `MODEL: session live (type=7)` | COMPATIBLE. Cross-PC sync needs `[CoOp] MfSync=1` (set on owner PC 2026-10-10; not in-game tested) |
| pitstop_failures 1.0.1 | Active on host bots | pitstop_failures.log: Burti (slot 35) stop processed | WORKS ON HOST. OPEN: runs independently on each PC, no sync; client effect on host-driven bots not checked |
| extended_damage_commentary 1.0.5 | Active | log `6/6 sites active` each session | COMPATIBLE (no MP gate, no slot loops found) |
| camera_editor 1.0.4 | Active; loads profiles per car incl. bots | camera_editor_log 22:46 profiles for bot cars | COMPATIBLE |
| esc_pause_menu 1.0.5 | Off by design in MP (`mp_state=2` -> vanilla ESC) | esc_pause_menu_log | INERT BY DESIGN (no pause in network sessions) |
| red_flag 1.1.0 | Multiplayer lockout, fail-closed (`is_multiplayer`: state != 0) | gr44.c is_multiplayer/mp_blocks | NOT AVAILABLE IN CO-OP (by design) |
| safetycar 0.10.93 | Guards refuse outside offline (`refused: thread/offline/race/field guard`); f1c-memory SC gates all false in CO-OP | pace_probe_standings_log, live read 19:46 UTC | NOT AVAILABLE IN CO-OP |
| driver_ai_toolkit 1.0.2 (ai_brain_lab) | SESSION GUARD releases all levers when state != 0 | ai_brain_lab_log `state 2 -> NOT offline, everything released` | NOT AVAILABLE IN CO-OP (safe) |
| clk55 1.0.2 | Content only (FIA 1999 CLK55 vehicle files, no DLL) | package listing | COMPATIBLE (season content sync carries files when used) |

Not shipped but installed on owner PC: qualifying_preservation (championship mode 3 only, inert in CO-OP;
logic now inside coop_mode), grid_restore (works with coop qualifying_*_coop.tsv), race_data_collector,
web_telemetry (not audited).

## Conclusions
- No crash or conflict found. Nothing needed code changes today.
- Three products refuse to run in multiplayer by design: Safety Car, Red Flag, Driver AI Toolkit. Making them CO-OP aware
  (host-only, synced to clients) is a separate project per mod.
- Pending live: MfSync=1 two-PC test; pitstop_failures behaviour on the client.
- Limits: static grep, not a full code review; client-side logs (Tester A's PC) not read.
