# AI Brain Lab - shareable app plan (2026-10-03, Claude)

 (plan). Pipeline stage: define / specify. Owner request: "redesign and plan the GUI to be in a
ready-to-share form. A simple run.bat which installs prerequisites so it works for anyone. As clear as possible with
as much adjustability as possible so people can start figuring out the AI behaviour as a joint effort. It should also
have the previous AI brain viewer live diagnostics."

## 1. What we ship (one zip, one folder)

```
AIBrainLab/
  run.bat                      double-click: checks / installs Python, elevates if the game is elevated, starts the app
  README.txt                   3 steps, safety notes, how to share an experiment
  app/                         the app (pure Python standard library: tkinter, ctypes, json - no pip packages)
    main.py                    unified window (viewer + lab)
    brain_core.py, brain_explain.py, tireforce.py, tireforcesolver.py    (copies, version-stamped)
    abl_shm.py, registry/ai_brain_abi.json, rules.json, presets/*.json
  plugin/ai_brain_lab.dll      Lightloader plugin (optional: the viewer works without it, read-only)
  install_plugin.bat           copies the DLL into <game>\plugins\lightloader after asking (game must be closed)
  experiments/                 user recordings land here (zipped for sharing)
```

Self-contained: no path into the owner's workspace. A `build_share.py` (developer side) assembles the folder from
the canonical sources, stamps versions/hashes into `app/VERSION.json`, and runs the offline tests.

## 2. run.bat behaviour (no prior knowledge needed)

1. Find Python >= 3.10 with tkinter (`py -3`, then `python`). If missing: explain, ask Y/N, then
   `winget install -e --id Python.Python.3.12` (official package; tkinter included). If winget is missing: open the
   python.org download page and stop with a clear message.
2. If the game runs elevated and this shell is not: relaunch itself elevated (one UAC prompt) - reading/controlling
   the game needs the same elevation.
3. Start `pythonw app\main.py` (no console window); errors go to `app\last_error.txt` and a message box.
No pip installs, no internet needed after Python exists. Alternative distribution (owner decision): a pre-built
PyInstaller folder (no Python at all; larger, may trip antivirus heuristics because it reads another process).

## 3. One window (mockup shown in chat 2026-10-03)

- Top bar: status pill (game found / DLL version / rules verified), car picker + Follow interesting, ARM switch,
  RELEASE ALL (Esc).
- Glance row (fixed slots, never bounce): Mode, Doing, Why, Next corner, Traffic, Line - same as viewer v3.
- Alert chips (grey when quiet) incl. "lever active on this car".
- Centre: car-up map + speed plan + pedals/steer + steering-law line (the v2 viewer picture the owner likes).
- Right drawer: This car (all per-car levers incl. Driver traits) / Rules (all AI cars, from rules.json) / Presets /
  Drive (owner 2026-10-03: the AI override car control, with user-changeable keys; implemented in v2.2).
- Bottom tabs: Timeline, Speed, Line and steering, Traffic, Modes, All fields (140, searchable, tiers), All cars,
  Watch (sparklines), Experiments, Share.
- Viewer-only mode when the DLL is absent: the drawer shows "install the plugin to control the AI" - diagnostics
  still work for everyone.
- Clarity rules: every control has a one-line plain-English meaning, the AI's own value next to it, the evidence tier
  (proven live / from code / guess) and a "beyond grip" style warning zone where the data says it breaks.

## 4. Joint-effort features (community research)

- Presets: save / load / share small JSON files (levers + rules + notes). Built-ins: Brave overtaker, Timid, Ignores
  blue flags, Late braker, Slow and careful, timed mistakes.
- Experiments: "Start experiment" records 20 Hz snapshots of the chosen car (+ 1 Hz all cars) before / during /
  after a lever change, with the exact settings, game build hash, track, session and a free-text note.
  "Share" zips it (`experiments/<date>_<track>_<topic>.zip`, typically a few MB) for Discord / forum.
- Compare view: open two experiment zips, overlay speed / line / pedal traces and corner targets.
- Field notes: users can attach notes to any field or rule ("this looks like the yield distance in pits"); exported
  with experiments so findings can flow back into the registry.
- Version guard: the app checks the game exe hash (1A70E5ED) and the DLL rules verification; unsupported builds run
  read-only with a clear message.

## 5. Safety and fairness (must-have before sharing)

- Single-player only: the DLL must refuse to act in multiplayer sessions (research item: identify the network /
  multiplayer flag; until proven, the plugin is documented as offline-only and the levers stay disabled when a
  network session is detected by any available signal).
- Everything off at start; ARM required; Esc releases; original values restored on release / unload (already in v2).
- Bounds in the DLL (already in v2) plus "warning zones" in the UI.
- Clear statement: offline modding tool for the player's own game; no anti-cheat bypass, no online use.

## 6. Owner decisions

Decided 2026-10-03 (owner, in chat):
1. Runtime: **run.bat + system Python** (winget install when missing). No PyInstaller bundle.
2. Audience: **public, everything** (viewer + control plugin), offline single-player only.
   Consequence: the multiplayer refusal (section 5) is a RELEASE BLOCKER, not a nice-to-have.

Still open:
3. Name and credits (product id `ai-brain-lab`?), licence text, where it is hosted (website / Discord).

## 7. Build order

1. Unified app (merge viewer v3 + lab panel into `app/main.py`, keep both old tools working).
2. run.bat + install_plugin.bat + build_share.py; test on a clean Windows user (no Python) - owner or VM.
3. Presets save/load, Experiments record + Share zip, Compare view.
4. Multiplayer refusal research + DLL v3 (and the batch-D rules).
5. Release gates per governance: version, hashes, clean-machine test, owner validation, then promote.
