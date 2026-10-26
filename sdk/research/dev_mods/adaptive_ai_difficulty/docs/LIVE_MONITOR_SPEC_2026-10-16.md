# Live monitor spec (2026-10-16) — see what Adaptive AI is doing, live

Owner request 2026-10-16: "a GUI through which I can see how much the adaptive AI is working and what it is doing
live, how much it is slowing/speeding up the rest etc, a GUI which anyone could use when running a race".

Decisions (Claude, repo pattern): window app in plain Python + tkinter (stdlib only) with `run.bat`, the same pattern as
the Driver AI Toolkit (`development/mods/ai_brain_lab/SHARE_PLAN_2026-10-03.md`), shippable later through the Lightloader
Tools tab. The app NEVER reads game memory and needs no admin: the DLL writes a small live status file; the app only
reads it. Read-only: the app writes nothing anywhere (except its own `last_error.txt`).

## 1. Live status file (DLL -> app), contract `aad.live.v1`
Path: `adaptive_ai_difficulty_live.json` beside the DLL (`<game>\plugins\lightloader\`). Written by the DLL worker at most
once per second while the game runs (also in menus), atomically (write `.tmp`, then MoveFileEx replace). UTF-8 JSON,
one object. Unknown/absent values are `null`. Extra keys may appear later; the app ignores unknown keys.

```json
{
  "schema": "aad.live.v1",
  "version": "0.3.2-dev",
  "utc": "2026-10-16+00:00",
  "pid": 15980,
  "seq": 1234,
  "state": "active",            // "off" (Enabled=0, no rating) | "watching" (rates, AI not changed) | "active" (AI changed by the mod)
  "session": {"type": 7, "kind": "race", "track": "2002_A-1_Ring", "slider": 93, "offline": true, "sim_s": 412.5},
                                 // kind: "race" | "qualifying" | "practice" | "menu or practice" | "other" | "none"
  "rated": {"who": "proxy", "name": "Human Driver", "car": "2002 Arrows", "slot": 7},
                                 // who: "player" | "proxy" | null (proxy set but not found)
  "level": {
    "factor": 1.025,            // AI strength factor in force (1.000 = the player's normal level)
    "pct": 2.5,                 // (factor - 1) x 100
    "applied": true,            // false = decided but not applied (watching mode)
    "tier": "track",            // "practice" | "track" | "overall" | "native"
    "estimated": false,
    "locked": true,             // locked for this session (qualifying / race)
    "source": "locked from practice",   // last decision label: "weekend start" | "locked from practice" | "locked (practice skipped)"
    "why": "gap to AI teammate -0.55 s (target -0.22 s), reference lap 72.33 s, status ok"
  },
  "effect": {
    "ai_sec_per_lap": -0.74,    // AI lap change vs normal, seconds (negative = AI faster); model effect(factor)
    "basis": "measured",        // "measured" (lever track) | "estimated" (fallback 0.31 x lap)
    "ref_lap_s": 72.3
  },
  "laps": [                      // last 12 rated laps of THIS session, oldest first
    {"lap": 5, "time": 70.882, "clean": false, "reasons": ["traffic"], "teammate": 72.474, "gap": -1.592, "top5": 72.041}
  ],
  "session_stats": {"laps": 14, "clean": 0, "teammate_laps": 0, "median_clean": null, "gap_teammate_median": null,
                    "need_clean": 5, "need_teammate": 5},
  "cars": [                      // every local AI car the hook knows (slot 1..29)
    {"slot": 3, "car": "2002 Sauber", "native": 0.95342, "factor": 1.025, "applied": 0.97726, "proxy": false}
  ],
  "next": {"factor": 1.04, "pct": 4.0, "tier": "track", "estimated": false, "text": "AI LEVEL NEXT RACE: +4.0 % (from your past races here)"},
  "events": [                    // last 15 notable events, oldest first (the same lines as the log)
    {"t": "15:03:24", "text": "DECISION locked (practice skipped): APPLY factor 1.000 ..."}
  ]
}
```
Rules: `cars[].applied` = the strength value the mod wrote (null = untouched / native). `effect` = `aad_effect(factor,
S, cap)` with `aad_sensitivity(track, ref_lap)`, ref_lap = teammate median of the session, else the track baseline.
`laps[].reasons` are engine reason codes; the app maps them to plain words (P2 spec section 4 wording).

## 2. App (`tools/aad_monitor/`)
Files: `run.bat` (find `py -3` / `python` with tkinter, start `pythonw app\main.py <game folder or file>`; message if
Python is missing), `app/main.py`, `app/view.py` (pure functions: JSON -> display strings, unit-testable),
`app/README.md`, `tests/test_view.py`, `tests/fake_live.py` (writes a changing sample file for demos/tests).
Finding the file: argument, else `..\..\F1 Challenge V10\plugins\lightloader\` relative to known install paths, else
a "Choose game folder" button (remembered in `app/settings.json`).

Window (one screen, ~900x640, resizable, dark-neutral theme, big readable numbers, no jargon):
1. **Status bar**: big chip `ACTIVE - the mod is changing the AI` (green) / `WATCHING - the AI is not changed`
   (amber) / `OFF` (grey) / `NO GAME` (grey, file older than 5 s: "Game closed, paused or in a loading screen").
   Right side: track, session kind, AI slider, "updated 0.4 s ago".
2. **AI level card (hero)**: "AI is **2.5 % stronger** than your normal level" -> second line "about **0.74 s per lap
   faster**" (estimated basis adds "(estimate)"). Factor 1.000 -> "AI at your normal level". Not applied -> "Would be
   +2.5 % (watching only, not applied)". Below: where it came from in words (tier: "from your past races here" /
   "from your practice laps" / "ESTIMATED from your other tracks" / "ESTIMATED: no data yet") + locked/unlocked.
3. **Who is measured**: "You (2002 Arrows)" or "TEST DRIVER: Human Driver (2002 Arrows) instead of you"; teammate line.
4. **Your pace vs your teammate** (canvas chart): per lap bar or dot = gap to teammate (s), green below 0 (you faster),
   red above; clean laps filled, not-used laps hollow with the reason on hover; dashed target line. Text under it:
   "Clean laps 3 of 5 needed - teammate laps 2 of 5 needed".
5. **Lap table**: Lap | Your time | Teammate | Gap | Used? (plain reason: "close behind another car" ...).
6. **What the mod did to each AI car** (table): Car | Normal strength | Now | Change (+2.5 %), proxy row marked
   "test driver - never changed".
7. **Next race** line (`next.text`) + **Events** list (last 15, newest on top).
Refresh every 1 s; never crash on a bad/partial file (keep last good view, show "file unreadable, retrying").

## 3. Tests
`tests/test_view.py` (unittest): every state, null fields, tier words, reason words, pct/effect wording incl.
negative factors ("AI 3.0 % weaker ... 0.9 s per lap slower"), stale-file detection, bad JSON. `fake_live.py`
cycles through: menu/off, watching practice, active race with laps, proxy mode, stale.
Review gates (owner rules): function review + UI check "layman-clear, F1-engineer feel".

## 4. Not in scope now
Lightloader Tools-tab packaging and catalog entry (later, owner approval), in-game overlay, any control/write action.

## 5. Amendment (owner, same day): history + session record = the upload payload
Owner: "it should also show the data retroactively so people could see how the AI calibration works in real time,
comparing to the human driver level and how it adapted and if the challenge it provided was balanced or realistic
enough. This is the data which we need to make the mod send to us automatically when a race/any session ends -
basically a when, why, how, where."

### 5.1 Changes to the live file (built in DLL 0.3.2-dev)
- Top-level keys now: `seq, schema, version, utc, pid, state, when, session, rated, level, effect, laps, session_stats,
  balance, next, cars, events`. `session.sim_s` moved to `when.sim_s`; `when` = `{session_start_utc, now_utc, sim_s,
  sim_elapsed_s}`.
- `laps` = ALL laps of the session (max 128), each with `utc` and `level` (AI factor in force at that lap) and
  `level_applied` -> the app can draw how the AI level moved lap by lap against the driver's gap.
- `session_stats.gap_teammate_median_all_laps` (all laps incl. traffic, without pit/first lap).
- `balance` = `{target_gap_s, gap_s, gap_basis, verdict, position, teammate_position, cars_running}`. Verdict
  (PROPOSAL thresholds): within 0.3 s of the target = "balanced"; driver faster by more = "too easy"; slower = "too hard".
  gap_basis says whether clean laps or all laps (traffic, understates a fast driver) were used.

### 5.2 Session record `adaptive_ai_difficulty_sessions.jsonl` (schema `aad.session.v1`)
One JSON line appended beside the DLL when any session with rated laps ends (practice, qualifying, race):
same body as the live file (when, where = session, who = rated, how/why = level + effect + laps, balance) without
`cars`/`events`, plus `outcome = {factor_used, factor_next, rating_updated, why}`. The player's name is never written
(DD-22: `rated.name` only for the test proxy). **This line is the WIP upload payload (DD-17); nothing is sent in this
build** - sending = phase P4 (manager upload), owner open items: install id, retention, deletion contact.

### 5.3 App: History tab
Second tab "History": reads `adaptive_ai_difficulty_sessions.jsonl` (newest first; a bad line is skipped). Session list
(date, track, kind, car, AI level, verdict chip). Selecting one shows the same chart as the live view (gap per lap +
AI level per lap as a step line on a second axis) and a plain summary: "Where: A-1 Ring 2002, race, Arrows. How:
AI +2.5 % (about 0.6 s per lap faster, estimate). Why: from your past races here. Result: you were 0.8 s per lap
faster than your teammate (target 0.2 s) - too easy. Next race: AI +4.0 %." Plus a track trend: AI level and gap per
session for the same track over time.

## 6. Built 2026-10-16
App `tools/aad_monitor/` (Haiku agent build, Claude review). Claude review fixes: traffic-basis line never matched the
DLL text; realistic AI strength line added (DD-29); chart numbered laps by index (now real lap numbers); window opens
maximized (default size hid balance, laps and events). Tests `python -I -B tests\test_view.py`: 142 OK. Visual check:
screenshot with the real C writer output (live) - hero, chart with AI-level steps, TOO EASY verdict, strength line,
lap table with plain reasons, per-car table, events, History tab with 3 sample sessions (selftest OK). Not yet seen
with a live game. Start: `tools\aad_monitor\run.bat` (reads `F1 Challenge V10\plugins\lightloader\` by default).
