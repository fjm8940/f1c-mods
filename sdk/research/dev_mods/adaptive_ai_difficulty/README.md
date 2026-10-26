# Adaptive AI Difficulty (`adaptive_ai_difficulty`)

Status: **WIP playtest phase started (2026-10-16).** Lever proven and calibrated (hook 0.85-1.10 linear, 6 tracks); WIP P1 done 2026-10-16: DLL 0.2.0-dev has the C rating engine + store (log-only, replay tests = Python oracle); installed with Enabled=0 + RatingLog=1 (AI vanilla, rating logged). WIP P2 done 2026-10-16: DLL 0.3.0-dev adds the weekend decision (practice lock, ladder, store update) + session conclusion file; installed with Enabled=0 + Adaptive=0 (decisions logged, AI vanilla). **2026-10-16 evening: DLL 0.3.3-dev (live monitor file, session record, best-laps fallback DD-28, realistic AI strength DD-29) installed in TEST mode; was 0.3.1-dev (`Enabled=1 Adaptive=1 ProxyDriver=Human Driver`, AI is scaled) for the proxy two-race test; restore Enabled=0 after it ([P2_BUILD_RECORD](docs/P2_BUILD_RECORD_2026-10-16.md) last section).** Then: ONE combined P1 + P2 owner drive ([docs/P2_BUILD_RECORD_2026-10-16.md](docs/P2_BUILD_RECORD_2026-10-16.md)).** Resume: [docs/HANDOFF_2026-10-16_EVENING.md](docs/HANDOFF_2026-10-16_EVENING.md).

> **Owner design decisions: [docs/DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md) is the single source of truth. Read it before building.** (Realistic mode DD-01: built in the Python tools 2026-10-16; the DLL version is WIP spec P1.)

Objective: keep single-player races close by scaling every AI car's pace live from the
player's lap times, so a strong player in a strong car (e.g. BAR at strength 93) no longer
wins easily, and a slower player is not lapped.

Non-goals: aggression/AI Realism, crash rate, strategy, multiplayer/CO-OP, changing .rcd/.PLR files.

## Records (newest first)
- [docs/DD26_COMPARABLE_TEAMS_SPEC_2026-10-16.md](docs/DD26_COMPARABLE_TEAMS_SPEC_2026-10-16.md) — DD-26 comparable-teams fallback spec; built + installed as DLL 0.4.2-dev (record: `dll/README.md` v0.4.2-dev), not verified live
- [docs/HANDOFF_2026-10-16_EVENING.md](docs/HANDOFF_2026-10-16_EVENING.md) — **current handoff** (0.4.1-dev installed in TEST mode, proxy race results, DD-26 session, RaceAbility fix)
- [docs/P3_INRACE_SPEC_2026-10-16.md](docs/P3_INRACE_SPEC_2026-10-16.md) — P3 in-race adjustment spec (DD-32)
- [docs/LIVE_MONITOR_SPEC_2026-10-16.md](docs/LIVE_MONITOR_SPEC_2026-10-16.md) — live monitor app + session record (upload payload); app in `tools/aad_monitor/` (run.bat)
- [docs/HOLES_REVIEW_2026-10-16.md](docs/HOLES_REVIEW_2026-10-16.md) — holes in the lap comparison (practice runs, AI/player failures, damage); fixes for P2.1
- [docs/P2_BUILD_RECORD_2026-10-16.md](docs/P2_BUILD_RECORD_2026-10-16.md) — P2 build/test/install record (DLL 0.3.0-dev, combined owner-drive checklist)
- [docs/P2_APPLY_SPEC_2026-10-16.md](docs/P2_APPLY_SPEC_2026-10-16.md) — P2 spec: weekend decision, practice lock, conclusion
- [docs/P2_DECIDE_ORACLE_RECORD_2026-10-16.md](docs/P2_DECIDE_ORACLE_RECORD_2026-10-16.md) — P2 Python oracle record
- [docs/HANDOFF_P2_2026-10-16.md](docs/HANDOFF_P2_2026-10-16.md) — short mid-P2 handoff (usage-limit cut; completed same day)
- [docs/P1_BUILD_RECORD_2026-10-16.md](docs/P1_BUILD_RECORD_2026-10-16.md) — P1 build/test/install record (DLL 0.2.0-dev, owner-drive checklist)
- [docs/P1_RATING_PORT_SPEC_2026-10-16.md](docs/P1_RATING_PORT_SPEC_2026-10-16.md) — P1 spec: track identity, C port semantics, trace format, harness
- [docs/P1_HAIKU4_TRACE_HARNESS_RECORD_2026-10-16.md](docs/P1_HAIKU4_TRACE_HARNESS_RECORD_2026-10-16.md) — trace/harness implementation notes
- [docs/HANDOFF_2026-10-16.md](docs/HANDOFF_2026-10-16.md) — cold-start handoff (current)
- [docs/DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md) — all owner design decisions (read first)
- [docs/WIP_RELEASE_SPEC_2026-10-16.md](docs/WIP_RELEASE_SPEC_2026-10-16.md) — WIP playtest release plan (phases P0-P5, open decisions)
- [docs/HANDOFF_NEXT_2026-10-15.md](docs/HANDOFF_NEXT_2026-10-15.md) — previous handoff (stacked 2026-10-15/16 resume lines)
- [docs/HANDOFF_2026-10-15.md](docs/HANDOFF_2026-10-15.md) — session trail, newest update on top
- [docs/HOOK_SPEC_2026-10-15.md](docs/HOOK_SPEC_2026-10-15.md) — strength hook spec; build in `dll/`
- [docs/SPEC_2026-10-14.md](docs/SPEC_2026-10-14.md) — spec with batches and STOPs
- [Investigation note](../../../f1c_ida_decompiled/investigations/adaptive_ai_difficulty/README_2026-10-14.md)
- [docs/baseline-2001-equal-ra-2026-10-14/README.md](docs/baseline-2001-equal-ra-2026-10-14/README.md) — 2001 all RA 1.0 + GDB laptime trace
