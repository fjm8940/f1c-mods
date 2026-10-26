# Vanilla Features Restoration — live dump results

Game: F1 Challenge Online.exe, SessionType=7 (Race), CommentaryEnabled=1
Date: 2026-07-27

## Raw counts

| What | Count |
|------|-------|
| SDT clips in PC_RE_MAIN.SDT | 316 |
| Non-empty event table entries | 520 |
| Events mapped to SDT clips | 482 |
| Alive (playable, not blocked) | 456 |
| Blocked (cooldown=FLT_MAX) | 26 |
| SDT clips reached by >=1 event | 293 |
| Orphaned SDT clips (no event) | 23 |
| Blog bolded clips | 68 |
| Blog bolded + ALIVE | 57 (see correction below) |
| Blog bolded + ORPHANED | 11 (see below) |
| Wired but NOT bolded (never heard/unconfirmed) | ~229 |

> **Correction, 2026-07-27:** the 65/13 split above was originally produced by
> `crossref_blog.py`'s substring match, which silently mismarks clips whose name
> contains a bolded code as a prefix — e.g. `2001_163/164/165` and `2001_218`
> collide with the bolded `2001_16` / `2001_21`, and `TEAMPIT02`/`AITYRES01/02`
> collide with `PIT02`/`TYRES01/02`. That inflated ALIVE+ORPHANED to 78, more
> than the 68 bolded codes that exist — a mathematical tell the numbers were
> wrong. `progress.py`'s `is_blog_confirmed()` uses exact match (after stripping
> the SDT's `PWS_` prefix) instead: 57 alive + 11 orphaned = 68 exactly, a clean
> 1:1 accounting with no residue. This also fixed one further naming gap: the
> blog writes `MISC_12/14/16` with an underscore the SDT doesn't have
> (`PWS_MISC12`), which is why the raw bolded-code count only crosses to 68
     after that correction too.
>
> **This is the fix for "voices classified as heard that I've never actually
> heard in-game":** the browser's `SHIPPED` column previously meant "an event
> can reach it", not "confirmed heard" — that flagged ~200 wired-but-silent
> clips as SHIPPED. `SHIPPED` now means blog-bolded (human-verified) **and**
> reachable; everything else starts in `SILENT`/the work queue, where it
> belongs until someone actually hears it.

## The 13 bolded-but-orphaned (blog grouped alternates)

Every orphaned bolded clip has a sibling that IS wired. Blog author grouped
alternate takes together:

| Orphaned | SDT | Alive sibling | SDT |
|----------|-----|---------------|-----|
| PWS_2001_09 | 32 | PWS_2001_08 | 31 |
| PWS_2001_110 | 177 | Win clips (WIN01-19) | various |
| PWS_2001_161 | 290 | PWS_2001_160 | 289 |
| PWS_2001_223 | 304 | PWS_2001_224 | 303 |
| PWS_2001_225 | 305 | PWS_2001_224 | 303 |
| PWS_2001_219 | 302 | PWS_2001_218/220/221/222 | 300-302 |
| PWS_HIT09 | 115 | PWS_HIT08 | 114 |
| PWS_AIPIT05 | 189 | PWS_AIPIT04 | 188 |
| PWS_AIPIT07 | 191 | PWS_AIPIT04/06 | 188/190 |
| PWS_AIRETIRE01 | 253 | AIRETIRE02-05 | 250-254 |
| PWS_AIWIN03 | 277 | PWS_AIWIN01/02 | 276/278 |
| PWS_MISC_12 | 211 | PWS_MISC_14 | 210 |

All 13 follow the pattern: variant A wired, variant B orphaned. Blog author
assumed both play.

## The 219 wired-but-silent (emitters exist, conditions never met)

These clips have valid event→score→SDT mappings. The game CAN play them. But
the emitter functions that fire those events either never fire in normal
gameplay, are gated behind session-type checks, or require conditions that
never occur.

Breakdown by category:

### Player mechanical issues (28 clips)
MECHPROB20-33 (lost/stuck gear x6 each), MECHPROB06-09 (puncture per wheel),
MECHPROB14 (electronics), MECHPROB16 (suspension), HIT01-06 (wing/wheel loss),
RETIRE01-04 (player retirement), MECHANWARN0 (meatball flag),
RE_PD_STALLED (engine stall), RE_PD_STALLED_T (T-car stall)

### AI mechanical/gear (18 clips)
AIMECHPROB* (lost gear + stuck gear for 1-6), RE_OD_LOST_7, RE_OD_STUCK_7,
RE_OD_PUNCTURE

### Penalty system — self + AI (22 clips)
PENALTY02/03/04/05/07/08/14 (penalty types), AIPENALTY* (AI penalty variants),
MISC_02-09 (investigation notices), RE_PD_25_DD, RE_PD_DQ, RE_PD_INV_DD,
RE_PD_SG_CUT, RE_PD_SG_DD, RE_PD_SG_SPEED, RE_OD_* equivalents

### Pit stop detail — all silent (16 clips)
2001_68-77 (tire change + slow + jammed), 2001_78-79 (refuel), 2001_80 (stall),
PIT01/03/06/08/09/10/12/13 (limiter, brake, gear, go, countdown)

### AI incidents (10 clips)
AIINCID01 (grid stall), AIINCID02-08 (off/spun/collisions), AIOFFTRACK0,
OD_HOLDING

### Weather commentary (6 clips)
2001_218-222 (weather transitions), 2001_310-312 (AI tire in rain)

### Session phase (8 clips)
2001_102 (personal best), 2001_159 (session ends), 2001_163-165 (25/50/75%),
RACEFAST03 (all-time lap record)

### Blue flags / lapping (5 clips)
AIBLUE01-03 (approach/pass chain), AILAPPIN01 (AI approaches player),
AILAPPIN05 (AI passes player)

### Qualifying detail (6 clips)
2001_13-15 (out/flying/in lap), 2001_17-20 (starting run N),
2001_22/24/25 (rain strategy), QUAL28-30 (traffic density)

### Championship/WDC (6 clips)
2001_33 (position secures WDC), 2001_138 (AI WDC position),
2001_141 (team WCC position), 2001_308-309 (driver orders),
2001_149 (AI held up)

### Per-tire blistering (8 clips)
RE_PD_FL/FR/RL/RR_BAD_BL (severe blistering per corner),
RE_PD_FL/FR/RL/RR_BLIST (light blistering per corner)

### Pit strategy / misc (25 clips)
AIPIT08-09 (1/2 stop strategy), AIPIT11 (stop-go), AIPIT13 (rejoin position),
MISC_18 (stop next lap), MISC_25-30 (race pace commands),
MISC_46-48 (AI laps AI, investigation), MISC_54 (box),
OIL01-03 (oil on track), SLOWCAR01, TEAMPIT01-02, CHANGEPIT01,
GREEN01, POSUPDATE01, OFFROAD02, OVERTAKE02/03, RACEFAST03, RED01

## Files generated

| File | Content |
|------|---------|
| event_clip_map.json | 482 event IDs -> clip names |
| full_event_dump.json | All 520 events with priority/cooldown/score/blocked/sdt_index |
| orphaned_clips.json | 23 SDT clips with no event |
| crossref_blog.py | Script that produced this analysis |
| dump_full_catalog.py | Reads live game memory, cross-references SDT |
