# Extended Race Engineer

Product id: `extended_race_engineer` (permanent). Follows `development/mods/MOD_STANDARD.md`.
Owner request 2026-10-18: "make ALL of the unused voicelines play in the game when they should play",
and make the voice browser the main tool that indexes which unused lines are already modded to play.

## Status

**START HERE: `docs/HANDOFF_2026-10-18.md`.** Stage: investigate COMPLETE — per-line audit 33/33 batches, stitch re-checked, merged into the voice index (browser shows verdicts), owner report `docs/AUDIT_REPORT_2026-10-19.md`; next: owner review, then Phase B (core DLL spec). (Earlier: specify, 2026-10-18.) Phase A research done and reviewed: `docs/PHASE_A_REVIEW_2026-10-18.md`.
122 of 245 lines already played by other mods; 120 designed for ERE; 3 skipped. Owner answered the 5 Phase B questions (DD-09..DD-13).
Nothing built. No DLL, no game changes.

## Objective

Every race-engineer clip in `PC_RE_MAIN.SDT` that vanilla never plays is spoken in game, at the moment
the real event happens, through one Lightloader mod with a per-line on/off table.

## Non-goals

- No invented events. A line plays only when its situation really happens (owner rules: AI fairness,
  vanilla+, severe calls need a real reason, no spam).
- No double-firing a line another first-party mod already plays (Pit Stop Failures, Mechanical
  Failures, Safety Car, Red Flag, Extended Damage Commentary). Those stay with their owner mod; ERE
  only indexes them.
- No TV-channel (eid > 291) work. No exe patching on disk.

## Acceptance criteria

1. Census: every non-vanilla line has one row saying which mod (if any) already plays it.
2. Design: every remaining line has a trigger design with sources and trust tier, or a recorded reason
   it cannot or should not play.
3. Build: one DLL meeting MOD_STANDARD R1-R11; each line switchable in `extended_race_engineer.ini`.
4. Verify: each line isolation-tested by eid, then heard by the owner in a real situation.
5. The voice browser shows, per clip: vanilla / modded-by-X / ERE planned / ERE live-verified.

## Baseline (from the browser's data, 2026-10-07, stale)

351 clips: 106 SHIPPED, 186 WIP, 43 SILENT, 16 DONE. 245 non-vanilla clips, split into 8 research
batches in `data/clip_assignments.json`. The data predates later mods, so the census decides the real
remaining count.

## Layout

- `docs/research/` census and findings; `docs/design/` per-batch trigger designs
- `data/` assignments, `modded_lines.json` (census), `design/*.json` (per-batch designs)
- `tools/` voice browser (main index tool), `src/` DLL source (phase B), `tests/`

## Records (newest first)

- 2026-10-19 — Audit close-out (handoff "After all 33 batches" steps 2-4). `tools/voice_index.py` loads
  `data/audit/B*.json` (skips `*.BEFORE_*`), merges per SDT index into a tool-owned `audit` field (verdict, reason,
  situation, mod, suppressed flag + note, play_plan summary, stitch_recheck); owner fields never written. New CLI
  `merge-audit` and `check-audit` (self-check: 351/351 merged, clip names match, plans present, owner fields
  unchanged: PASS). `data/voice_index.json` merged (diff vs backup = only the `audit` field). Browser: Verdict column,
  second chip row (Native active / Mod only / Not activated / Unknown / Suppressed), audit counts line, audit block in
  the detail pane; headless-instantiated (23 suppressed rows, detail OK), not clicked by the owner. `docs/VOICE_INDEX.md`
  regenerated (Verdict / Reason / Play plan columns, suppressed table). Tests 18/18 PASS (4 new). 11 findings appended
  (`ere-*`, 2026-10-19; backup `findings.jsonl.BEFORE_20261019_ere_audit`). Owner exe cooldowns re-read by Claude:
  1a70e5ed 106/111 = 15.0, 117/119 = 6.0; clean V10 de151845 = FLT_MAX (FACT). SDT 302 "heard" = owner's browser
  preview (H8), not in game — resolved. Report: `docs/AUDIT_REPORT_2026-10-19.md`. Backups `.BEFORE_20261019_audit`
  for voice_index.json, voice_index.py, voice_browser.py, test_voice_index.py, VOICE_INDEX.md, README.md, handoff,
  tools/README.md. Pending: owner review, listening/live checks (report section), Phase B.
- 2026-10-18 — Stitch re-check W1-W6 returned, Claude-verified against backups (row counts, verdict
  diffs, play plans present). 41 verdicts changed (e.g. SDT 215-219 NATIVE_ACTIVE -> NOT_ACTIVATED; B27 260-267,
  B29 282-288, B31/B32 orphans UNKNOWN -> NOT_ACTIVATED). **Final audit, 351 lines:** NATIVE_ACTIVE 94, MOD_ONLY 101,
  NOT_ACTIVATED 130, UNKNOWN 26, suppressed_trigger_exists 23. Owner listening item: voice_index says SDT 302
  (drizzle) was heard, but no game route reaches it.
- 2026-10-18 — Cron `2659edf7` fired; usage 5 h = 1%, weekly = 37%. Wave 2 (B28-B33, SDT
  270-350, 6 Haiku workers) launched. All 6 returned; Claude-reviewed and accepted
  (spot-checks: eid 264 `sub_41A0C0`, eid 284 `sub_4BCCC0` -> DisplayMessage0x11c). 33/33 batches done.
  **Key finding (PROVEN, findings.jsonl `pws-subclip-bank-is-stitch-2026-10-18`):** every sub-clip comes from
  `pc_re_stitch.sdt` (bank dword_5BF8CC), never `pc_re_main.sdt` (dword_5BF8C8). All earlier "plays as a
  sub-clip" verdicts are index collisions (91 rows in B13-B32). `docs/AUDIT_BRIEF.md` gained section SUB-CLIP
  BANK FACT (backup `.BEFORE_20261018_stitch`); all batch files backed up `.BEFORE_20261018_stitch`; 6 Haiku
  re-check workers (W1-W6) re-judging those rows. Also settles the listening questions for SDT 153-169 / 215-229
  (the code wanted stitch number / laps words, not these main clips).
- 2026-10-18 — B16-B27 (SDT 150-269) done, all Claude-reviewed and accepted (B19 C3 note
  corrected by coordinator). 5 h window had reset early, so cron 22:43 was deleted and the wave launched by
  hand. Weekly limit hit 100% mid-wave; B28-B33 not launched. 27/33 batches done (270 lines). Resume: durable
  one-shot cron 6144bd6a, 2026-10-19. Review notes: `docs/HANDOFF_2026-10-18.md` "Update 19:10".

- 2026-10-18 — Handoff written: `docs/HANDOFF_2026-10-18.md`. B13-B15 reviewed (B15 corrected:
  RacePhase 5 = racing, flag-clear lines NOT_ACTIVATED + suppressed). 150 lines audited; cron 22:43 resumes.

- 2026-10-18 — Wave 1 (B01-B12, SDT 0-119) done and reviewed (corrections sent to B03, B07, B10).
  Usage 72% of 5 h after wave 1, so only B13-B15 launched; one-shot durable cron  (reset +3 min)
  launches B16-B33. Dossiers regenerated with sub-clip resolvers.

- 2026-10-18 — Per-line exhaustive audit (DD-17): `tools/line_trace.py` built (finds the 40 game functions that
  queue commentary and every literal/dynamic path to each eid); 351 dossiers in `data/line_trace/` (generated,
  rerun `python tools/line_trace.py all`). 33 Haiku batches; wave 1 (B01-B12) running.

- 2026-10-18 — Owner correction: census counted MF's 35 borrowed token eids as "modded". Fixed
  (`mechanism: token_only`, backup `data/modded_lines.json.BEFORE_20261018_token`; `tools/voice_index.py`
  ignores token_only in is_modded, backup `.BEFORE_20261018_token`). Real to-do now 158. Launched 6 Haiku
  "plays right now" audits over PWS eids 1-290 (`docs/AUDIT_BRIEF.md`, outputs `data/audit/`, `docs/audit/`).

- 2026-10-18 — Owner listening: 7 orphan transcripts saved (`docs/design/H8_LISTEN_CHECKLIST.md`,
  `data/voice_index.json`, `data/design/H8_orphans.json`); Porsche lines WONT_DO (DD-14). Census merged
  into the index: 125 modded, 130 real to-do. `docs/VOICE_INDEX.md` regenerated; tests 14/14 OK.

- 2026-10-18 — Owner decisions DD-09..DD-13 recorded (SLOWCAR01 = approaching a slower car; no AI
  penalty lines; game-missing infractions excluded until the penalty system; eid 207 from lap loss;
  listening now). Voice browser smoke-launched by Claude: opens, 351 rows, closes clean; it saved the
  auto-found SDT path into `data/voice_index.json`. Listening results pending.

- 2026-10-18 — Phase A reviewed by Claude: totals, corrections, owner questions, Phase B plan in
  `docs/PHASE_A_REVIEW_2026-10-18.md`. 5 findings appended to `f1c_ida_decompiled/findings.jsonl` (`ere-*`).

- 2026-10-18 — Batch H9 (tool): voice index built in `tools/` (`voice_index.py` data layer,
  `voice_browser.py` GUI, `browse_voices.bat`), seed `data/voice_index.json` (351 rows from EVF
  progress.json: 245 not vanilla, 51 modded = 16 EVF DONE + 35 mechanical_failures MF01-35 that are not
  in the vanilla SDT, 194 to-do). Generated view `docs/VOICE_INDEX.md`. Headless tests 14/14 PASS
  (`tests/test_voice_index.py`). GUI compiled, not launched. Caveat: vanilla = EVF SHIPPED, and most of
  those were auto-classified (reachable, not heard). Structure check reports 4 pre-existing errors
  unrelated to this mod (`.claude`, `LOG`, `plans` top-level dirs; a missing randomizer productionPath),
  left untouched. Usage: `tools/README.md`.
- 2026-10-18 — Phase A started: 10 Haiku research batches (census, 8 trigger-design batches, browser
  elevation). Brief: `docs/HAIKU_BRIEF.md`. Results pending review.

## Prior art (read, do not repeat)

`f1c_ida_decompiled/mods/extended_vanilla_features/` (archived EVF project): `docs/RESTORATION_PLAN.md`,
`docs/EMITTER_SPECS.md`, `docs/VOICE_LINES.md`, `docs/SPEC_EVF-*.md`, `data/emitter_index.json`,
`data/full_event_dump.json`, `dll/README.md` (6 injection traps); `f1c_ida_decompiled/docs/56-voice-line-reference-map.md`;
SDK `tools/sdk/f1c_pit_voice.h` + `PIT_VOICE_REVIEW.md`.
