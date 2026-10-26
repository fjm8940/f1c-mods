# P1 rating port spec (2026-10-16) — C rating engine + store in the DLL, log-only

Phase P1 of `WIP_RELEASE_SPEC_2026-10-16.md` section 8. Owner decisions: `DESIGN_DECISIONS.md` (DD-01/03 Realistic =
teammate reference, DD-07 clean-lap rules, DD-08/DD-20 per-track store, DD-13 fallback ladder, DD-22 no player name).
Oracles (do not change their logic): `tools/aad_rating_logger.py` (RatingEngine), `tools/aad_ratings.py` (store).
Work split (owner 2026-10-16: "task haiku agents to do majority of the work"): Claude wrote this spec and the headers
`dll/aad_json.h`, `dll/aad_rating.h`, `dll/aad_store.h`, `dll/aad_trace.h`; Haiku agents implement the files in
section 6; Claude reviews each file and does the DLL integration.

## 1. Track identity (P1a) — prior art, no new live run needed
- `0x008AC438` AIWFilePathBuffer: NUL-terminated ASCII, game-relative path of the loaded track's .AIW, e.g.
  `SEASONDATA\CIRCUITS\Austria\2001_A-1_Ring\2001_A-1_Ring.AIW`. FACT, live-read 2026-07-28 (Monza) and 2026-08-03
  (findings.jsonl `AIWFilePathBuffer ...` line 150, `file-search-path-rules`). Changes on track load.
- Track key used by the DLL store = the AIW file stem as written (`2001_A-1_Ring`); season = the stem text before the
  first `_` (`2001`, `2003RH`); the folder above the stem dir is the country (`Austria`). The lever model keys
  (`austria_2001`, `monza_2001`, ...) are hand-made; the alias table is a P2 task.
- Alternative (not used): `dword_8AC490 + 396*dword_8AC48C + 260` = scene name string (`2001_Monza`), FACT 2026-07-27.

## 2. Exact Python semantics the C port must reproduce
All arithmetic in IEEE double (the C files are compiled with `-msse2 -mfpmath=sse -ffp-contract=off`).
- `None` doubles are NAN in C (`aad_none()`, `aad_isnone()`); JSON writes them as `null`.
- `round(x, 3)` = correct decimal rounding of the exact binary value, half-to-even on exact ties (ties DO occur:
  x = m/16, m odd). C `aad_round3`: p = x*1000 with the exact product error e (Veltkamp/Dekker two-product,
  splitter 134217729.0); f = floor(p); frac = p - f (exact); decide nearest integer k of (frac + e) vs 0.5 exactly
  (d = frac - 0.5 is exact when frac is in [0.25, 0.75]; compare d with -e; equal = tie -> even k); return k / 1000.0.
- `statistics.median`: sort ascending; odd n -> middle; even n -> (a[i-1] + a[i]) / 2.
- Python float `%`: `mod = fmod(a, b); if mod != 0 and ((b < 0) != (mod < 0)) mod += b; if mod == 0 mod = copysign(0, b)`.
- `bootstrap_ci95(values)`: rng = `random.Random(0)`; 1000 times: n draws of `rng.choice(values)` -> median;
  sort the 1000 medians; lo = meds[25], hi = meds[975]; both `round(.,3)`; n == 0 -> None.
  `choice(seq)` = `seq[_randbelow(len(seq))]`; `_randbelow(n)`: k = bit_length(n); r = getrandbits(k) while r >= n;
  `getrandbits(k)` (k <= 32) = `genrand_uint32() >> (32 - k)`. `Random(0)` = MT19937 `init_by_array([0], 1)`.
- Engine (`RatingEngine.feed`), in this order:
  1. `if sim == last_sim: return []` (exact compare; last_sim starts None).
  2. jump = last_sim not None and |sim - last_sim| > 5.0; last_sim = sim.
  3. If session != s.session: if a session is open, emit summary(s.utc) (uses the OLD laps, slider, player_car,
     track); then reset: session = s.session, window_start = sim, new player tracker, empty AI trackers, empty
     ai_laps, empty session_laps. NOT reset: last_sim, lap_len, slider, player_car. (C only: at reset, if s.track is
     not "", engine track = s.track.)
  4. env = {yellow, jump}; if sample has a slider: engine slider = it.
  5. For each car in sample order with `ai and slot != 0`: its tracker (created on first sight) feeds; on a crossing
     append {sim, lap_time, car name, clean = no ai_reasons} to ai_laps.
  6. Player = first car with slot 0 (if none, skip). player_car = car name if non-empty else keep. lap_len =
     max(lap_len, every car's dist). If lap_len > 0: ahead = [(o.dist - p.dist) py% lap_len for o with slot != 0,
     pit == 0, not retired]; gap_ahead = min(ahead) / max(p.speed, 30.0) if ahead else None. Player tracker feeds;
     on a crossing emit a lap record.
- CarTracker.feed: first sight: line = car.line, acc = observe(new_acc(partial=1)); no output. Same line (exact ==):
  observe. Else: lap_time = round3(car.line - line); done = observe(acc); line = car.line;
  acc = observe(new_acc(partial=0)); output (car.laps, lap_time, done). (The crossing sample is observed into BOTH.)
- observe: n++; pit |= pit != 0; yellow |= env.yellow; jump |= env.jump; speed |= speed > 0; dmin/dmax of dist;
  if gap_ahead not None and < 1.0: traffic_n++ (player only); if has_pf: offtrack |= offtrack == 1; cut_lo/cut_hi
  min/max of cut.
- player_reasons, in this order: first_lap_observed_partially (partial), lap_number_below_2 (laps < 2), pit,
  off_track, cut (cut_lo set and cut_lo != cut_hi), yellow, traffic (n and traffic_n / n > 0.25), sim_time_jump,
  no_speed_seen, distance_not_advanced (dmax None or dmax <= dmin), lap_time_out_of_range (not 50 < t < 200).
- ai_reasons: first_lap_observed_partially, pit, yellow, lap_time_out_of_range. Clean = no reasons.
- Lap record (window = ai_laps with window_start < a.sim <= sim): clean_ai = sorted clean lap times;
  ai_med = median(clean_ai); top5 = median(first 5 of clean_ai); mates = clean window laps whose car == player_car
  (only when player_car non-empty), tm = median(mates). Then window_start = sim; ai_laps keeps only a.sim > sim.
  Record keys: type "lap", utc, session_type, track, lap, lap_time, clean, reasons[], ai_median_clean = round3(ai_med),
  ai_top5_median_clean = round3(top5), gap_to_ai_median = round3(lap_time - ai_med) (unrounded ai_med), player_car,
  teammate_clean = round3(tm), gap_to_teammate = round3(lap_time - tm). None stays None.
- summary(utc): clean = clean lap records. Lists use the ROUNDED record values: times = lap_time; ai = ai_median_clean
  (non-None); gaps = gap_to_ai_median (non-None); top5 = ai_top5_median_clean (non-None); top5_gaps = lap_time -
  ai_top5_median_clean (unrounded difference); tm_laps = teammate_clean; tm_gaps = gap_to_teammate. Keys:
  type "summary", n_player_clean, player_median_clean = round3(median(times)), ci_low/ci_high = CI(gaps),
  ai_top5_median_clean = round3(median(top5)), gap_top5_median = round3(median(top5_gaps)), ci_top5_low/high =
  CI(top5_gaps), slider, player_car, n_teammate_laps = len(tm_gaps), teammate_median_clean, gap_teammate_median,
  ci_teammate_low/high = CI(tm_gaps), utc, session_type, track, player_laps_total, player_clean_laps,
  player_median_clean_lap (= player_median_clean), ai_median_clean = round3(median(ai)), gap_median =
  round3(median(gaps)), gap_ci95 = [lo, hi] = CI(gaps) or null, gap_ci_resamples 1000, gap_ci_seed 0.
- Store (`aad_ratings.py`): follow `record`, `_med`, `pct`, `overall`, `lookup` line by line. pct(gap, ref) = None if
  gap None or ref None/0, else round3(100.0 * gap / ref). Equality of sliders follows Python (null == null; numbers
  compare numerically; null != number). `entry` truthiness: a non-empty object. New entries: keys in the Python order.
  `player` stays null (DD-22: no name). JSON written with indent 1, `\n` newlines, trailing newline.

## 3. JSON (`aad_json.c`)
Parser: RFC 8259 incl. `\uXXXX` (surrogate pairs -> UTF-8), numbers via strtod (is_int = literal without `.`/`e`/`E`),
NaN/Infinity literals accepted (Python writes them), depth limit 64. Writer: compact `json.dumps` style
(`", "`, `": "`) when indent < 0, otherwise Python `indent=N` style. Strings escape `"` `\` and control chars
(`\n \r \t \b \f`, else `\u00XX`); bytes >= 0x80 pass through. Doubles: `aj_fmt_double` tries `%.1g`..`%.17g`
and takes the first that strtod reads back to the same bits; adds `.0` when the text has no `.`, `e`, `n`, `i`.
Integers (is_int and |v| < 2^53): `%.0f`. Every file defines `__USE_MINGW_ANSI_STDIO 1` before includes
(mingw's correct printf/strtod; msvcrt's are not exact).

## 4. Trace format (text, one record per line, `\n`)
```
AADTRACE 1
T <track key bytes as lowercase hex, or ->
S <utc> <sim double bits, 16 hex> <session int> <slider int or N> <yellow 0|1> <ncars>
C <slot> <ai 0|1> <laps> <line bits16> <dist bits16> <speed bits16> <pit int> <retired int> <pf> <car>
```
`<pf>` = `-` (no player fields) or `<offtrack int>,<cut int>`; `<car>` = car name bytes as lowercase hex or `-`.
Doubles are the IEEE bits of the double (`%016llx` of the uint64), so values are exact both ways. A T line applies
to every following S. `ncars` C lines follow each S line. Unknown line types = format error.

## 5. Replay harness (`dll/test/aad_replay_harness.c`, console exe, built into %TEMP%)
- `harness replay TRACE OUT.jsonl CLOSE_UTC`: engine track = first T line value (""= None); feed every sample; then
  close(CLOSE_UTC); write each emitted record as one compact JSON line.
- `harness store OPS.jsonl OUT.jsonl STORE_PATH`: start from `aad_store_load(STORE_PATH)` (missing = empty); ops:
  `{"op":"record","track","summary":{..},"style","factor_used","factor_new","when"}` -> writes the returned entry;
  `{"op":"lookup","track","slider","style","s_rel_pct","target_pct"}` -> `{"factor","tier"}`;
  `{"op":"overall","style","slider"(null ok)}` -> `{"overall": value or null}`; `{"op":"save"}` -> saves to
  STORE_PATH, writes `{"saved":1}`. One output line per op.
- `harness numfmt IN.txt OUT.txt`: each input line = a Python `repr(float)`; output `aj_fmt_double(strtod(line))`;
  `harness round3 IN.txt OUT.txt`: each line `repr(x)` -> `aj_fmt_double(aad_round3(x))`;
  `harness mt N OUT.txt`: first N `aad_mt_u32` of seed 0, then N values of `aad_mt_randbelow` for n = 1..N (fresh seed 0).
Exit code 0 = ok, 2 = usage/format error.

## 6. Files and owners
| File | Owner | Notes |
|---|---|---|
| `dll/aad_json.c` | Haiku 1 | section 3 |
| `dll/aad_rating.c` | Haiku 2 | section 2 engine, round3, median, MT, bootstrap, summary JSON |
| `dll/aad_store.c` | Haiku 3 | section 2 store; save = temp file + `MoveFileExA(MOVEFILE_REPLACE_EXISTING)` |
| `dll/aad_trace.c`, `dll/test/aad_replay_harness.c`, `dll/test/build_harness.bat` | Haiku 4 | sections 4, 5 |
| `tools/aad_trace.py`, `tools/test_aad_replay_c.py` | Haiku 5 | trace read/write + Python oracle replay; tests |
| `dll/adaptive_ai_difficulty.c`, `build.bat`, ini, meta, records | Claude | DLL integration (section 7) |

Build flags for the new C files: `-O2 -Wall -Wextra -Werror -std=gnu11 -msse2 -mfpmath=sse -ffp-contract=off`,
32-bit MinGW `C:\msys64\mingw32\bin\gcc.exe`. No other dependencies. Output only to `%TEMP%`.

## 7. DLL integration (log-only, no factor change)
- New ini keys: `RatingLog` (1 = run the rating engine; default 0 when absent), `RatingTrace` (1 = write the sample
  trace to `%TEMP%\aad_trace_<utc>_pid<n>.txt`), `RatingIntervalMs` (default 500, clamped 100..2000).
- `Enabled=0` and `RatingLog=1`: no patch, no broker, worker runs the rating only, entry returns TRUE.
  `Enabled=0` and `RatingLog=0`: unchanged (returns FALSE, vanilla).
- Worker samples only while the session guard reads offline (state 0); reads are in-process, read-only.
- Records: `adaptive_ai_difficulty_rating.jsonl` beside the DLL (lap + summary lines); short RATING lines in the log.
- Summary of a race session (session_type 7) with >= 1 clean lap -> store record (style realistic, factor_used =
  factor_new = the factor in force: 1.0 without the hook). Then a lookup is logged as "would apply" (not applied).
- Game exit: summary written at the unload event or best effort at DLL_PROCESS_DETACH (skipped if the worker died
  inside a feed).

## 8. Acceptance (P1 done)
DLL and harness build clean with -Werror; `python -I -B tools\test_aad_replay_c.py` passes (identical records on
synthetic traces incl. every reason, ties, pauses, jumps, session changes, teammates, 22-car random races; identical
store states and lookups; number format and round3 and MT checks); old Python tests still pass; records updated.
Then STOP: owner drives 2 races with `RatingLog=1 RatingTrace=1`; the owner traces become replay fixtures.
