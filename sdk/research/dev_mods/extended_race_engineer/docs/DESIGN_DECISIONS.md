# Extended Race Engineer — design decisions

Owner decisions (quoted or paraphrased with date) and coordinator decisions, newest last.

| # | Date | Who | Decision |
|---|---|---|---|
| DD-01 | 2026-10-18 | Owner | New mod "Extended Race Engineer": make ALL unused race-engineer voice lines play in game "when they should play". |
| DD-02 | 2026-10-18 | Owner | Work done by many parallel Haiku workers ("task as many Haikus as you need"). |
| DD-03 | 2026-10-18 | Owner | The voice browser becomes the main tool for indexing and tracking which unused lines are already modded to play. |
| DD-04 | 2026-10-18 | Claude | New canonical home `development/mods/extended_race_engineer/`; EVF tree stays read-only history. Browser is copied (not moved) into `tools/`. |
| DD-05 | 2026-10-18 | Claude | Lines already played by another first-party mod stay with that mod; ERE indexes them and never double-fires them. |
| DD-06 | 2026-10-18 | Claude | Phase A = read-only research and trigger design per batch; Phase B (DLL) starts only after Claude reviews every batch. PWS eids 1..290 only; every eid isolation-tested before a trigger is wired. |
| DD-07 | 2026-10-18 | Claude | MF 1.0.7 uses 35 PWS eids as radio-queue tokens (no table writes). ERE must not queue an eid while MF has a line pending on it; MF token move proposed as follow-up. |
| DD-08 | 2026-10-18 | Claude (SUPERSEDED by DD-09) | PWS_SLOWCAR01 (eid 132, score 130) stays silent: Safety Car 0.10.93 `safety_car_guardrails.c:658` records the owner instruction "SLOWCAR01 MUST NEVER PLAY" (stated twice) and parks eid 132 at FLT_MAX; its native emitter fires constantly under caution. Ask owner: never, or allowed outside a caution? |
| DD-09 | 2026-10-18 | Owner | SLOWCAR01: the old "never play" order was because our SC mod did it wrong. It should play "when the player is approaching a car going on a lower pace than the others". ERE emits it for that case; the SC mod's blanket block must be narrowed (SC follow-up). |
| DD-10 | 2026-10-18 | Owner | AI penalty lines: leave AI silent (no AI penalties, no AI penalty lines). |
| DD-11 | 2026-10-18 | Owner | "Penalty system will be done later" — lines for infractions the game lacks (overtaking under yellow, 25 s / dangerous driving, investigation for dangerous driving) are excluded for now. |
| DD-12 | 2026-10-18 | Owner | AI "periodically slowing down" (eid 207): build from real lap-time loss. |
| DD-13 | 2026-10-18 | Owner | Listening session for the unknown clips: now. |
| DD-14 | 2026-10-18 | Owner | Do not use the Porsche lines (RE_PORSCHE_23/24/25/03, SDT 312-315): ERE status WONT_DO. |
| DD-15 | 2026-10-18 | Owner | Census correction: MF's borrowed eids (e.g. PWS_2001_17..20 'Xth run') are NOT played by MF. Marked `token_only`; browser no longer counts them as modded. Owner asked for a proven per-eid audit of what the game plays right now (6 Haiku audits, `docs/AUDIT_BRIEF.md`). Phase B spec waits for it. |
| DD-16 | 2026-10-18 | Owner | Audit buckets: NATIVE_ACTIVE (game activation works, e.g. AILAPPIN02-04) / NOT_ACTIVATED (proven never played, e.g. AILAPPIN01, YELLOW04) / MOD_ONLY (only a mod plays it, e.g. SC, MF lines). Flag lines the game was written not to play although the trigger exists (`suppressed_trigger_exists`). 7th worker covers clips with no PWS eid. |
| DD-17 | 2026-10-18 | Owner | "Each line should be analyzed individually and exhaustively throughout the decompiled corpus." Old 7 range audits stopped (no output). New: `tools/line_trace.py` dossiers for all 351 clips (`data/line_trace/`, regenerable), 33 batches of ~10 clips (`data/audit_batches.json`), mandatory `checked` list per row, run in waves of 12. |
| DD-18 | 2026-10-18 | Owner | Each not-playing line gets its own drafted play plan (meaning, when it should play, trigger, subject, technique, latch, exclusions, conflicts, live test) — e.g. PWS_2001_14. Added to the audit brief as `play_plan`. |
| DD-19 | 2026-10-18 | Claude | Installed MF = release 1.0.7 (sha256 5513874A..., matches `releases/v1.0.7/build`), engine family only: MF gear (80-93), suspension (75), electronics (76) lines are dev-only and do not play in the owner's game. Census rows updated. |
| DD-20 | 2026-10-18 | Claude | Owner game facts for the audit: exe on disk has EVF cooldown unblocks baked in (106-111=15.0, 117-119=6.0; 80 still FLT_MAX); Pit Stop Failures installed = 1.1.4-rc.1 (plays 43-46, 50, 51, 53, 54, 56, 57, 60-70, 117, 183-190); line_trace.py now also lists sub-clip resolvers (EnumValueTranslation 0x414010 appends SDT 113-134). Wave-1 rows that called clips 'second clip, picker unknown' get a re-check pass after wave 3. |
| DD-21 | 2026-10-18 | Claude | SLOWCAR01 (eid 132): no game function queues eid 132 (coordinator grep of all 40 emitters + B14). The Safety Car comment "the game fires eid 132 by itself" is unproven; the heard SLOWCAR01 was most likely the SC carrier-score leak it also documents. ERE builds DD-09 (player approaching a slower car) as its own emitter. RacePhase 5 = green racing (FACT) used to settle the yellow-clear verdicts. |
