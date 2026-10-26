# P2 spec (2026-10-16) — pre-race apply, practice assessment, session conclusion

Phase P2 of `WIP_RELEASE_SPEC_2026-10-16.md` section 8. Owner 2026-10-16: no driving now -> build P2, then ONE combined
owner test for P1 + P2; "assign to haiku as much as you can". Decisions used: DD-01/03 Realistic (teammate, target
-0.3 %), DD-05/18 player's slider = factor 1.00, DD-08/20 store, DD-12 practice assessment + lock at qualifying + skipped
practice -> ladder "ESTIMATED", DD-13 ladder, DD-19/23 conclusion (file + manager panel + one in-game line).
Not in P2: in-race nudge and DD-24 calibration race (P3), manager panel and upload (P4), in-game line display (P3: needs
the message-strip path; P2 only produces the text), wet sensitivity (`austria_2001_wet` not used).

## 1. Facts and assumptions
- FACT: SessionType `0x7C56A4` = 7 race, 5 qualifying (findings.jsonl, live). ASSUMPTION (owner test confirms): every other
  non-zero value during an offline weekend is practice or warm-up; the DLL logs each new value.
- Track key = AIW stem (P1). Lever model alias (case-insensitive stem -> model key): `2001_A-1_Ring` austria_2001,
  `2001_Monaco` monaco_2001, `2001_Hungaroring` hungaroring_2001, `2001_Indianapolis` indianapolis_2001,
  `2001_Albert_Park` melbourne_2001, `2001_Monza` monza_2001. Values = `docs/lever_model.json` (S, baseline lap, max gain).
  Every other track: fallback S = -0.307 x ai_lap, cap = 0.041736 x ai_lap (DD-25 all seasons).

## 2. Oracle `tools/aad_decide.py` (new; Python is the reference, C must match exactly)
Imports `aad_model as m`, `aad_ratings as rs`, `aad_apply` (read only). `TRACK_ALIAS` dict as above;
`model_key(track) = TRACK_ALIAS.get(track.lower(), track)` (alias keys lowercase). `MODEL = m.load_model()`.
- `adaptive_step(summary, track, current, min_laps=5, max_ci=1.0, alpha=0.5)` -> `(factor, computed, why)` or
  `(None, None, why)`. Exactly `aad_apply.adaptive_step(summary, MODEL, model_key(track), current, 0.0, alpha, "teammate", -0.3)`
  except: the teammate-lap minimum and `check_evidence`'s n minimum are `min_laps`, its CI width limit is `max_ci`.
  (Write it out explicitly: view = teammate fields; refuse if `n_teammate_laps < min_laps`; refuse if any of
  teammate_median_clean, gap_teammate_median, ci_teammate_low, ci_teammate_high, n_player_clean is None; refuse if
  n_player_clean < min_laps; refuse if ci_high - ci_low > max_ci; gap = gap_teammate_median; ai_lap = teammate_median_clean;
  target = -0.3 / 100.0 * ai_lap; computed, status = m.factor_for_gap(gap, MODEL, model_key(track), ai_lap, target, current);
  factor = m.smooth(current, computed, alpha).)
- `weekend_start(store, track, slider)` -> `{"factor", "tier", "estimated", "why"}`: lap = baseline_ai_lap_s of the model
  entry or 80.0; S = m.sensitivity(MODEL, model_key(track), lap)[0]; `f, tier, why = rs.lookup(store, track, slider,
  "realistic", s_rel_pct=100.0 * S / lap, target_pct=-0.3)`; estimated = tier != "track".
- `lock_level(store, track, slider, practice, factor_in_practice)`: base = weekend_start(...). If practice is not None:
  `f, computed, why = adaptive_step(practice, track, factor_in_practice, min_laps=3, max_ci=2.0, alpha=1.0)`; if f is not
  None: if base tier == "track": f = m.smooth(base factor, f, 0.5); return {"factor": f, "tier": "practice",
  "estimated": False, "why": ...}. Otherwise return base (practice refused or skipped).
- `race_update(store, track, race, factor_used, when)` -> `(factor_new, updated, why)`: `f, computed, why =
  adaptive_step(race, track, factor_used)` (defaults 5 / 1.0 / 0.5); factor_new = f if f is not None else factor_used;
  `rs.record(store, track, race, "realistic", factor_used, factor_new, when)`.
- Summaries are the P1 summary dicts. Rounding: all factors pass through `m.clamp_round` / `m.smooth` (round 3).

## 3. C port (`dll/aad_decide.c`, header `dll/aad_decide.h`)
Line-by-line port of the oracle + the used parts of `aad_model.py` (`clamp`, `clamp_round`, `sensitivity`, `effect`,
`factor_for_gap`, `smooth`). Python `min/max/round` semantics; `aad_round3` from aad_rating.c; store calls from aad_store.c.
`AAD_LEVER_TRACKS[]` = the 6 dry tracks with the exact json numbers. `why` texts need not match Python.
Python `int(summary.get("n_teammate_laps") or 0)`: C field values. `smooth` raises on alpha outside (0,1]: C clamps to 1.

## 4. Session conclusion (`dll/aad_conclusion.c`, header `dll/aad_conclusion.h`)
Plain language, short lines, no jargon beyond "teammate", "clean lap", "AI level". Sections in this order:
1. Title: `Adaptive AI Difficulty - session report` + utc + version.
2. Session: track, your car, session kind (race / qualifying / practice / session N), AI strength slider.
3. Laps: driven N, clean used M; then "Not used:" one line per non-zero reason in plain words
   (pit = "pit lane", off_track = "off the track", cut = "corner cut", yellow = "yellow flag", traffic = "close behind
   another car", sim_time_jump = "game paused or skipped", first_lap_observed_partially / lap_number_below_2 = "first lap",
   no_speed_seen / distance_not_advanced = "car not moving", lap_time_out_of_range = "lap time not realistic").
4. Pace: vs teammate (same car): "+0.42 s (0.5 %) slower" / "0.31 s (0.4 %) faster" / "no teammate laps"; vs front-runners
   (fastest 5 AI) the same way; one line on how: "median of your clean laps vs the AI in the same car, same laps".
5. What the mod did: adaptive_on 0 -> "Log only: the AI was not changed (your normal level)."; 1 -> "AI level this
   session: +x.x %" where x = (factor_used - 1) x 100, "0.0 %" = your normal level.
6. Next race at this track: from `next`: "AI level +x.x %" + tier words (practice = "from your practice laps",
   track = "from your past races here", overall = "ESTIMATED from your other tracks", native = "ESTIMATED: no data yet,
   your normal level") + race_updated note; overall skill line when has_overall.
7. Data: wip_upload 0 -> "Nothing was sent anywhere (development build)."
`aad_conclusion_oneline`: "AI LEVEL NEXT RACE: +2.0 % (from your past races here)" (estimated -> "(ESTIMATED)").
Percent and seconds printed with 1 / 2 decimals; NAN fields print the "no ... data" wording, never "nan".

## 5. DLL integration (Claude)
- New ini key `Adaptive` (1 = the mod sets the AI level; default 0). Applies only with `Enabled=1` and `Adaptive=1`;
  otherwise every decision is logged as "would apply" (installed state stays vanilla).
- Weekend state (worker): new weekend = track key changed, or a non-race session after a race. At the first offline sample
  of a weekend: `aad_weekend_start` -> factor. Practice summaries (session != 5/7) are kept (latest with the most
  teammate laps). Entering qualifying or the race (first sample with 5 or 7): `aad_lock_level` once per weekend -> factor
  locked. Race summary: `aad_race_update` (store record, replaces the P1 log-only record) + conclusion file. Any summary:
  conclusion file (next = lookup for the next race).
- Apply = set every AI slot factor in memory to the decision (ini AllAIFactor ignored while Adaptive=1) and post the
  broker apply, the same path as an ini change.
- Session conclusion file `adaptive_ai_difficulty_last_session.txt` beside the DLL (overwritten each time) + log line.

## 6. Work split (Haiku agents, parallel)
| Agent | Files |
|---|---|
| A | `tools/aad_decide.py` + `tools/test_aad_decide.py` (oracle + unit tests) |
| B | `dll/aad_decide.c` |
| C | `dll/aad_conclusion.c` |
| D | harness modes `decide` and `conclusion` in `dll/test/aad_replay_harness.c`, `build_harness.bat` (add the 2 files), `tools/test_aad_decide_c.py` (C vs oracle) |
Claude: this spec, headers, DLL integration, review, records.

## 7. Harness modes (agent D)
- `harness decide OPS.jsonl OUT.jsonl`: in-memory store starts empty; ops: `{"op":"record",...}` as P1 store ops;
  `{"op":"weekend_start","track","slider"}`, `{"op":"lock","track","slider","practice":summary|null,"factor_in_practice"}`
  -> `{"factor","tier","estimated"}`; `{"op":"race_update","track","race":summary,"factor_used","when"}` ->
  `{"factor_new","updated"}`; `{"op":"factor_for_gap","gap","track","ai_lap","target","current"}` -> `{"factor","status"}`;
  `{"op":"store"}` -> the whole store. One output line per op.
- `harness conclusion IN.json OUT.txt`: IN = `{"conclusion": {...all AadConclusion fields, summary as P1 summary JSON,
  next as {"factor","tier","estimated","why"}}}` -> text + a last line with the one-liner.
Tests: oracle vs C on >= 500 random ops (gaps -5..+5 s, laps 60..120 s, all 6 alias tracks + unknown tracks + lowercase
stems, CI widths around 1.0/2.0, teammate laps 0..10, slider changes, clamp and cap edges); C table == lever_model.json;
conclusion: required phrases present, no "nan", every reason wording appears when its count > 0.
