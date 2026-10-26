# Recorder split on ESC retire: record 2026-10-15

Worker R-S. Owner report: with the ESC pause menu mod, "retire to pits" mid-race makes the
game create a new Race recording. Owner: the race keeps going (AI keeps racing). Expected: one race file.

Scope: `storage_writer.inc` only. Not touched: `storage_api.inc`, `web_telemetry.c`,
`runtime_test.c`, viewer files, `CURRENT_STATE.md`, the game folder (no POST, no writes).

## 1. Verdict

- Owner view: the race continues. Not contradicted by the files, but not shown either.
- File view: each retire ends the race file with results, then a Warm-up (type 6) and a new
  Race (type 7) start at sim 0 with laps at 0. That fits a fresh session, not a continuation.
- Decision in the code: one file per race, held across the restart and rebased in sim time.
  If the owner's view is right the merged file is exactly what they expect. If the file view is
  right the merged file contains two race starts joined by a `session_resume` EVENT. Owner
  decides after the live check (open item 1).

## 2. Findings

### 2.1 Game-side evidence (read-only; GET requests to http://localhost:8080)

| Item | Value | Tier |
|---|---|---|
| Race file `AUSTRIA 2001 - Race - Race - 2026-10-14 22-42_3` | type 7, A1Ring, t 0.0 to 533.108, 161 events, laps 0 to 6 | FACT (replay/info) |
| Race file `... 22-51_2` | type 7, t 0.0 to 565.025, 149 events, laps to 6, first lap event 83.7 s | FACT |
| Race file `... 23-05_2` | type 7, t 0.0 to 1266.117, 368 events, laps to 16 | FACT |
| Lap counters inside each race file | never decrease (0 decreases in 3 files) | FACT |
| Warm-up `... 22-41_2` | type 6, t 0.415 to 35.307 | FACT (replay/info) |
| Restarted race `... 22-51_2` at t=30 | 23 cars, all lap 0, speed 57 to 85 | FACT (replay/frame) |
| Restarted race at t=300 | 18 cars, laps 3 to 4 | FACT (replay/frame) |
| Close reasons per file (results 3, type change 1, sim reset 2, stale 4) | reported by the owner's session review | not re-read from `.f1t` |
| `/recordings` now | `AUSTRIA 2001 - Race - 2026-10-14 23-58` holds 3.5 KB files with 0 ticks (Warm-up, Race, Race, Warm-up) and one open race at 00-01 | FACT |

The replay/info call returned `review index is building` for a while after a new file was
written. Retrying after 10 to 60 s worked. Not a failure of the game.

### 2.2 Prior art (searched, not re-derived)

- `tools/sdk/README.md`: pit retirement voice and native retirement trial; no session-flow note.
- `mods/esc_pause_menu` README (v1.0.2, 2026-09-23): RETIRE TO PITS goes through the shared
  broker, which "opens RACE RESULTS from the garage". So the ESC retire path ends at RACE RESULTS,
  which matches the results close (sim >= 1e8) in the recorder.
- `mods/esc_pause_menu` (2026-10-11): "retirement route went straight to results" was an earlier
  path that was corrected. The 2026-10-11 retire crash in d3d8.dll is still open and unrelated here.
- `findings.jsonl`: no entry on the session flow after results. The entry added here is the
  first one.

### 2.3 Facts and inferences

- FACT: a race file never contains a lap-counter reset (3 files checked).
- FACT: each Race file after a retire starts at sim 0 with laps 0 and a standing-start lap time.
- FACT: the restarted race shows laps 3 to 4 at t=300, so it runs as a fresh race, not as a
  continuation of laps 6 or more.
- INFERENCE: the game ends the race with RACE RESULTS, shows Warm-up and starts a new session.
  The owner's view that the AI keeps racing fits a new session in which the AI races again. This
  is not proven. A live check is needed (open item 1).
- FACT (code, before): the recorder closed the race file at results and opened a new one at the
  next advance. That split the race into three files.
- FACT (code, before): a Race opened by a sim drop to 0 and then waiting on the grid closed as
  stale after 30 s (`sw_sm_idle` / `SW_STALE_MS`), and the 3.5 KB and 36 KB junk files fit that.
- FACT (code, before): a sim reset of a race closed the file (reason 2). A 2.7 s fragment followed
  by a new race gave two files.

## 3. Changes (storage_writer.inc)

Before-copies: `%TEMP%\RSPLIT_before_2026-10-15\storage_writer.inc` (sha256
`a69b51a8bdde66f48669a8658c30d4d6f95444c59234a9edfdb9b7cfe3c11468`) and
`%TEMP%\RSPLIT_before_2026-10-15\findings.jsonl`
(sha256 `bda681f11a9765530702ac1900d5ef3e9caae64eeba7c20681d3291a8903a1bd`).

After-state line numbers in `storage_writer.inc`:

| Lines | Change |
|---|---|
| 13-17 | header comment: RSPLIT hold, resume, rebase, grid rule |
| 27-29 | `SW_GRID_MS` 600000 (grid wait), `SW_HOLD_MS` 300000 (hold), `SW_RESUME_GAP_S` 1.0 |
| 40 | `SWE_RESUME` appended to the event enum (no renumbering) |
| 42 | `SWA_RESUME = 16u` |
| 143-175 | `sw_sm_t` gains `moved, held, hold_reason, hold_ms, open_track, sim_offset, seg_end`; `sw_obs_t` gains `track_id` |
| 177-183 | `sw_track_id()` (FNV-1a of the AIW path, pure) |
| 185-190 | `sw_sm_close()`: close now, clears hold and offset |
| 193-198 | `sw_sm_end()`: a race holds its file; other sessions close now |
| 201-268 | `sw_sm_step()`: held branch (resume or close), results/drop/type rules, open clears offset |
| 271-281 | `sw_sm_idle()`: hold timer; stale 30 s when moved, else 600 s |
| 374 | `sw_fill_sample()`: sample sim = raw + `sim_offset` |
| 449-458 | `sw_emit()`: `SWA_RESUME` claims a `SWE_RESUME` ring slot before the sample |
| 480 | `sw_capture_tick()`: `o.track_id = sw_track_id(g_telemetry.aiw_path)` |
| 1347-1354 | `sw_apply()`: `SWE_RESUME` writes `{"type":"session_resume","t":...,"session_type":7}` |

Rules after the change (type 7 race only; other sessions unchanged):

| Event | Before | After |
|---|---|---|
| results (sim >= 1e8) | close | hold |
| sim drop, same type | close | hold |
| switch to type 0 or 6 (menu, warm-up) | close | hold |
| switch to another type (1-5) | close | close |
| same type and track, moves again (< 1e8, cars > 0) within 300 s | new file | resume same file, rebased (+1 s gap) |
| other track while held | new file | close |
| hold expires (300 s) | none | close with hold reason |
| stale, not moved since open (grid) | after 30 s | after 600 s |
| stale, moved | after 30 s | after 30 s (unchanged) |

Sample rate and writer block logic are unchanged. The file is not deleted or renamed.

## 4. Tests

### 4.1 Standalone check (done)

Extracted the pure section from the edited `storage_writer.inc` into
`%TEMP%\rsplit_scripts\sm_full.c` and ran `sm_tests.c` (31 checks, all pass) and the paste-ready
block below (25 checks, all pass). The old state machine could not be run through the same harness
because the struct fields differ (no `track_id`, `held`, `sim_offset`); the retire test fails on
the old code by construction (old code emits CLOSE at results).

### 4.2 Compile check (done, into %TEMP%)

`gcc -c -O2 -std=c11 -Wall -Wextra -Wno-unused-function` on `web_telemetry.c` (the storage
writer is included at line 8091) into `%TEMP%\rsplit_build\`. Result: no error or warning from
`storage_writer.inc`. The translation unit fails at `web_telemetry.c:5896-5898` (`vehn_walk`,
backslash escapes in char/string literals). That region is in another worker's file and was not
touched by this task. `build.bat` was not run.

### 4.3 Paste-ready block for runtime_test.c (not pasted: file is locked)

Needs `#include <math.h>` (for `fabs`) in `runtime_test.c`, which the harness already uses. Paste
before `main`; call `rsplit_state_machine_tests();` in the test sequence. Uses `checks` as in the
existing b2 tests.

```c
/* ---------- RSPLIT 2026-10-15: retire keeps one race file; grid wait; fragment merge (pure state machine) ---------- */
#define RS_NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-6)
#define RS_CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr, "FAIL rsplit %s:%d: %s\n", __FILE__, __LINE__, #c); exit(2); } } while (0)
#define RS_TRK_A 111u
#define RS_TRK_B 222u

static sw_act_t rs_step(sw_sm_t *m, int type, int veh, double sim, uint32_t ms, uint32_t trk) {
    sw_obs_t o;
    o.session_type = type; o.race_phase = 0; o.vehicle_count = veh;
    o.sim = sim; o.now_ms = ms; o.track_id = trk;
    return sw_sm_step(m, &o);
}

/* ESC retire: results hold the race file, warm-up is waited out, the restarted race resumes the SAME file (rebased). */
static void rs_retire_one_file(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 0.0, 0, RS_TRK_A);
    a = rs_step(&m, 7, 20, 0.05, 1000, RS_TRK_A);
    RS_CHECK(a.acts & SWA_OPEN);
    a = rs_step(&m, 7, 20, 533.108, 534000, RS_TRK_A);
    RS_CHECK(a.acts & SWA_SAMPLE);
    a = rs_step(&m, 7, 20, 1.0e8 + 5.0, 535000, RS_TRK_A);       /* RACE RESULTS */
    RS_CHECK(a.acts == 0 && m.held && !m.open);                  /* no CLOSE: the race is held */
    a = rs_step(&m, 6, 20, 0.4, 540000, RS_TRK_A);               /* Warm-up */
    RS_CHECK(a.acts == 0 && m.held);
    a = rs_step(&m, 6, 20, 35.3, 575000, RS_TRK_A);
    RS_CHECK(a.acts == 0 && m.held);
    a = rs_step(&m, 7, 20, 0.0, 580000, RS_TRK_A);               /* the race starts again at sim 0 */
    RS_CHECK((a.acts & SWA_RESUME) && (a.acts & SWA_SAMPLE));
    RS_CHECK(!(a.acts & SWA_OPEN) && !(a.acts & SWA_CLOSE));
    RS_CHECK(!m.held && m.open && RS_NEAR(m.sim_offset, 534.108));
    a = rs_step(&m, 7, 20, 0.05, 581000, RS_TRK_A);
    RS_CHECK((a.acts & SWA_SAMPLE) && RS_NEAR(m.seg_end, 534.158));
}

/* A held race with no resume closes with the hold reason after SW_HOLD_MS; idle drives the timer. */
static void rs_hold_expires(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 0.0, 0, RS_TRK_A);
    rs_step(&m, 7, 20, 0.05, 1000, RS_TRK_A);
    rs_step(&m, 7, 20, 100.0, 2000, RS_TRK_A);
    rs_step(&m, 7, 20, 1.0e8 + 1.0, 3000, RS_TRK_A);
    RS_CHECK(m.held);
    a = sw_sm_idle(&m, 3000 + SW_HOLD_MS);
    RS_CHECK(a.acts == 0 && m.held);
    a = sw_sm_idle(&m, 3000 + SW_HOLD_MS + 1);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_RESULTS && !m.held && !m.open);
}

/* Another track never resumes a held race. */
static void rs_other_track_closes(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 0.0, 0, RS_TRK_A);
    rs_step(&m, 7, 20, 0.05, 1000, RS_TRK_A);
    rs_step(&m, 7, 20, 1.0e8 + 1.0, 2000, RS_TRK_A);
    a = rs_step(&m, 7, 20, 0.0, 3000, RS_TRK_B);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_TYPE && !(a.acts & SWA_RESUME));
    RS_CHECK(!m.held && !m.open);
}

/* Grid wait: a race opened at sim 0 is not stale before it moves; stale after SW_GRID_MS, not before. */
static void rs_grid_wait_not_stale(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 5.0, 0, RS_TRK_A);
    a = rs_step(&m, 7, 20, 0.0, 1000, RS_TRK_A);                 /* drop to 0 opens the race */
    RS_CHECK(a.acts & SWA_OPEN);
    a = sw_sm_idle(&m, 1000 + 60000);                            /* 60 s on the grid: kept */
    RS_CHECK(a.acts == 0 && m.open);
    a = sw_sm_idle(&m, 1000 + SW_GRID_MS + 1);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_STALE);
}

/* A race that is moving still goes stale after SW_STALE_MS. */
static void rs_moving_stale_30s(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 0.0, 0, RS_TRK_A);
    rs_step(&m, 7, 20, 0.05, 1000, RS_TRK_A);
    rs_step(&m, 7, 20, 2.0, 2000, RS_TRK_A);
    a = sw_sm_idle(&m, 2000 + SW_STALE_MS - 1);
    RS_CHECK(a.acts == 0 && m.open);
    a = sw_sm_idle(&m, 2000 + SW_STALE_MS + 1);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_STALE);
}

/* A short fragment (sim reset after 2.7 s) followed by the same race joins the same file. */
static void rs_fragment_merges(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 7, 20, 0.0, 0, RS_TRK_A);
    a = rs_step(&m, 7, 20, 0.05, 1000, RS_TRK_A);
    RS_CHECK(a.acts & SWA_OPEN);
    rs_step(&m, 7, 20, 2.7, 3700, RS_TRK_A);
    a = rs_step(&m, 7, 20, 0.0, 4000, RS_TRK_A);                 /* reset: held, not closed */
    RS_CHECK(a.acts == 0 && m.held);
    a = rs_step(&m, 7, 20, 0.05, 4500, RS_TRK_A);
    RS_CHECK((a.acts & SWA_RESUME) && !(a.acts & SWA_OPEN));
}

/* Non-race sessions keep the old rules: results closes at once; race to qualifying closes at once. */
static void rs_non_race_unchanged(void) {
    sw_sm_t m; sw_act_t a;
    memset(&m, 0, sizeof m);
    rs_step(&m, 5, 20, 0.0, 0, RS_TRK_A);
    rs_step(&m, 5, 20, 0.05, 1000, RS_TRK_A);
    a = rs_step(&m, 5, 20, 1.0e8 + 1.0, 2000, RS_TRK_A);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_RESULTS && !m.held);
    rs_step(&m, 7, 20, 0.0, 3000, RS_TRK_A);
    rs_step(&m, 7, 20, 0.05, 4000, RS_TRK_A);
    a = rs_step(&m, 5, 20, 10.0, 5000, RS_TRK_A);
    RS_CHECK((a.acts & SWA_CLOSE) && a.reason == SWR_TYPE && !m.held);
}

static void rsplit_state_machine_tests(void) {
    rs_retire_one_file();
    rs_hold_expires();
    rs_other_track_closes();
    rs_grid_wait_not_stale();
    rs_moving_stale_30s();
    rs_fragment_merges();
    rs_non_race_unchanged();
    RS_CHECK(sw_track_id("C:\\a.aiw") != sw_track_id("C:\\b.aiw"));
}
/* ---------- end RSPLIT ---------- */
```

Also needed in `runtime_test.c`: every existing `sw_obs_t` built there must set `track_id`
(the existing b2 harness does not). Otherwise an uninitialized value decides whether a held race
resumes. Type-7 expectations in `b2_run_scenario` / `t3_session_replay` for results, stale and
grid will change; they must be re-derived by the test owner (see open item 3).

### 4.4 Not done

- Live run (the game is running; no test was run against it). No write to the game folder.

## 5. Changed files

- `mods/sdk_sandbox/web_telemetry/storage_writer.inc` (151 changed diff lines; the file was
  LF, UTF-8, no BOM before and after; checked).
- `findings.jsonl` (one appended entry: `esc-retire-race-session-flow-2026-10-15`).
- New: this record.
- Scratch in `%TEMP%`: `RSPLIT_before_2026-10-15\`, `rsplit_scripts\`, `rsplit_build\`.

## 6. Open items

1. Owner live check (game running, owner-run): retire to pits mid-race, then watch whether the AI
   cars keep their positions or return to the grid. The file data says restart; the owner says
   continue. The rebase is only correct if the restart is a re-entry of the same race.
2. The `session_resume` EVENT type is new. Check that the replay reader (`storage_api.inc`,
   `/replay/info` event walk) ignores unknown EVENT types, as it does for `session_start`.
3. `runtime_test.c` expectations for type-7 results, stale and grid need updating (see 4.3).
4. A held race keeps its file open up to 300 s after results. The recordings list shows it as
   `open` in that time. Decide if that is acceptable.
5. Two different races on the same track within 300 s merge into one file (same type 7 and
   track). Decide if a time gate or a track-plus-weekend gate is wanted.
6. Fragments with no predecessor (the 3.5 KB Race files with 0 ticks at 23-58 and 23-59) are not
   merged or marked. They were left on disk. Marking needs the index (`storage_api.inc`), which is
   out of scope.
7. A merged file has lap counters that restart at the join. The viewer needs to show the segment
   boundary (`session_resume`) or the owner decides the second segment is a separate race.
8. Grid timeout is now 10 min (`SW_GRID_MS`). Owner to confirm the value.

## 7. Restoration

To restore the pre-change state: copy `%TEMP%\RSPLIT_before_2026-10-15\storage_writer.inc` over
`mods\sdk_sandbox\web_telemetry\storage_writer.inc` (sha256 in section 3). Restore
`findings.jsonl` from the matching before-copy, or delete only the last line
(`esc-retire-race-session-flow-2026-10-15`). The `%TEMP%` copies are not durable. If they are
needed beyond this session, copy them to an owner-approved preserved location.

## 8. Documentation gate

- Record exists in the component directory (this file). Not indexed in CURRENT_STATE.md (not
  edited by request).
- Fact and inference are separated in section 2.3; the restart reading is INFERENCE.
- Pending validation: open items 1 to 3.
