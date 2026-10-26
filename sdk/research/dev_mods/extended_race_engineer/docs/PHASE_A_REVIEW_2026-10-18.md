# Phase A review — 2026-10-18 (Claude, coordinator)

10 Haiku workers (census H0, designs H1-H8, browser H9). All static research; nothing run in game.
Per-batch files: `data/design/H*.json` + `docs/design/H*.md`; census `data/modded_lines.json` +
`docs/research/MODDED_LINES_CENSUS.md`; tool `tools/` (H9).

## Totals (245 non-vanilla clips)

| Technique | Clips | Meaning |
|---|--:|---|
| owned_by_other_mod | 122 | already played by a first-party mod (incl. MF's own 35 MF clips) — ERE indexes only |
| game_state_emitter | 80 | ERE detects the real situation and queues the PWS eid |
| hook_native | 29 | the game's emitter exists; ERE calls or un-gates it |
| carrier_score | 11 | no own event: play through a borrowed eid with its score rewritten |
| skip | 3 | T-car lines 112/113 (no T-car in the game), AI "periodically slowing" 207 (no real state) |

| Batch | Owned | Emitter | Native | Carrier | Skip |
|---|--:|--:|--:|--:|--:|
| H1 pit | 28 | 6 | | | |
| H2 penalties | 1 | 21 | 12 | | |
| H3 mechanical | 34 | | 6 | | 3 |
| H4 incidents | 6 | 13 | 2 | | |
| H5 race/standings | 1 | 11 | 2 | | |
| H6 weather/tyres | 8 | 19 | 2 | | |
| H7 session/flags | 8 | 10 | 5 | 1 | |
| H8 orphans | 36 | | | 10 | |

## What the coordinator checked (spot checks, FACT = read in code by Claude)

- PASS: Pit Stop Failures plays eid 60/190 rejoin lines (`pitstop_modding/live/live_client.cpp:534`) and
  MISC54 eid 46; MF electronics eid 76 (`mechanical_failures/dll/electronics_failure.c:117`).
- PASS: `PWS_MF01-35` are MF's own appended clips (`mechanical_failures/audio/engineer/MANIFEST.csv`).
- PASS: flag-clear lines gated off while racing (`PitSelectionStateMachine_0041b3c0.c` case 0).
- CORRECTED (H4, twice): spin heading is known (yaw matrix `veh+0x34BC..0x34E0`); the "network gate"
  for AI-AI collisions was wrong — `0x54FA90` is a contact-buffer read and `sub_41AD80` also emits the
  vanilla-heard player-hit lines.
- CORRECTED (census): MF 1.0.7 does not rewrite vanilla tables; it only uses donor eids as tokens
  (see census addendum, DD-07).
- CORRECTED (H5): it cited an August SC source; current SC 0.10.93 still blocks SLOWCAR01 by owner order (DD-08).
- NOT CHECKED by Claude: H2's claim that the native penalty tracker `sub_4BCCC0` is player-only
  (code iterates `dword_7C54F0` in places) — verify before building the 12 AI penalty rows.

## Facts vs inference

All design rows are DERIVED or lower. No eid in this phase is isolation-tested. Every trigger needs
(1) isolation test of its eid, (2) a live situation test by the owner.

## Owner decisions needed before Phase B

1. SLOWCAR01: never, or allowed outside a caution? (DD-08)
2. AI penalties: ERE running a penalty tracker for AI cars changes AI race results (time penalties,
   stop-go). Allowed? (H2)
3. Lines describing infractions the game does not have (overtaking under yellow, 25 s penalties,
   dangerous driving): define simple rules for them, or leave them silent? (H2)
4. Skips: AI "periodically slowing down" (207) — build from lap-time loss, or leave silent? (H3)
5. Listening session: `docs/design/H8_LISTEN_CHECKLIST.md` (10 clips, 6 alternate-pick tests).

## Cross-batch build rules found

- Double owners to settle: eid 119 (MF + player_retire_commentary), 117 (retire + pitstop).
- Never queue an eid while MF has a pending line on it (DD-07).
- Shared calibration list (live, read-only, not while owner drives a test): S/F threshold, pit box
  position, stop-go record, session clock, compound id map, AI sector source, gap thresholds,
  impact units (1250), points table, all-time lap record store.

## Next (Phase B plan, not started)

B0 owner answers above. B1 core DLL (MOD_STANDARD R1-R11, broker game-thread queue, per-line ini
switch, MF-token guard, eid isolation test mode). B2 modules per batch in build order: hook_native and
cooldown-type first (cheapest, 29), then game_state_emitter by batch, carrier last. Each module = one
Haiku worker writing one file, Claude reviews each STOP.
