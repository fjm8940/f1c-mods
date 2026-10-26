# Adaptive AI live monitor

A small read-only window that shows what the Adaptive AI Difficulty mod is doing
during a race or practice: whether the AI is changed, by how much, how the
AI's pace compares with your teammate, and whether the challenge was balanced.

## What it shows

- **Live tab**: a big chip (ACTIVE / WATCHING / OFF / NO GAME), the AI level in
  plain words, the lap-time effect, where the level came from, who is measured
  (you, or the test driver), a chart of your gap to your teammate per lap with
  the AI level per lap, the balance verdict with positions, the last 12 laps,
  each AI car's normal and current strength, the next race estimate, and recent
  events.
- **History tab**: every finished session with rated laps (newest first), a
  plain when / where / how / why / result summary for the selected session, and
  a per-track trend of AI level and gap over time.

## How to start

Double-click `run.bat` in the `tools\aad_monitor` folder. The window looks for
the game's status files in:

1. a folder or `.json` path given on the command line,
2. the game folder remembered in `app\settings.json`,
3. `%USERPROFILE%\Desktop\F1\F1 Challenge V10\plugins\lightloader\`.

If none is found, press **Choose game folder** (bottom right) and pick the game
folder. The choice is remembered in `app\settings.json`.

Needs Python 3.10 or newer with tkinter. No packages are installed. If Python is
missing, `run.bat` says so and waits for a key.

## Files it reads

Both are written by the game mod, in the same folder:

- `adaptive_ai_difficulty_live.json`: refreshed about once a second while the game runs.
- `adaptive_ai_difficulty_sessions.jsonl`: one line per finished session (History tab).

If the live file is older than 5 seconds the chip shows NO GAME (game closed,
paused or on a loading screen). A half-written or unreadable file shows
"file unreadable, retrying" and keeps the last good view.

## What it does not do

It only reads those files. It never reads game memory, never sends anything,
and never changes the game or the mod. The only file it writes is
`app\settings.json`, and only when you choose a game folder.

## Demo without the game

From the `tools\aad_monitor` folder:

    python -I -B tests\fake_live.py

This writes sample files to `%TEMP%\aad_monitor_demo` (never to a game folder),
cycling through menu, practice, race, test-driver and stale states. Start the
window on that folder with `run.bat "%TEMP%\aad_monitor_demo"`.

## Tests

From the `tools\aad_monitor` folder:

    python -I -B tests\test_view.py

The wording and logic live in `app\view.py` (no window needed). The DLL sample in
`tests\fixtures\` is real output from the C writer.
