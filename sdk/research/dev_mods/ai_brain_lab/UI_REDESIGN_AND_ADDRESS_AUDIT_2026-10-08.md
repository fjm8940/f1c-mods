# Driver AI Toolkit 1.0.3-r2: UI redesign + AI address audit (2026-10-08, Claude)

Owner request (2026-10-08): (1) ready-made presets, e.g. "no 75 % throttle cap" loadable for every race; (2) a clear
"container" showing whether a change goes to one driver, selected drivers or the whole grid, and proof that it really
reached exactly those drivers (example: CORNERINGADD for M. Schumacher only); (3) layman explanations with a
SIMPLE / ADVANCED switch and a general info display; (4) audit that every editable AI memory address is in the tool.
Done on the latest unshipped version (1.0.3, not released). Stage: implement -> verify (offline) done; human
validation (live, in-game) pending. until the owner's live check.

## Result

| | Where | State |
|---|---|---|
| Package | `releases/driver_ai_toolkit/1.0.3-r2/driver_ai_toolkit-1.0.3.zip` `2E104E95...` | built, not installed, not published |
| Game plug-in in it | `dll/ai_brain_lab.dll` `6C60B666...` (the Option-3 rebuild, separate MF mapping; matches MF 1.0.3) | unchanged by this work |
| Offline test | `tools/test_abl_ui.py` | 25/25 PASS (own test shared-memory name, never the live plugin) |
| Layout test | `tools/test_abl_shm.py` | 0 failures |
| Manual | `MANUAL.md` regenerated (`tools/gen_manual.py`): containers, proof, presets, SIMPLE text + technical text per row | done |

Note: the earlier 1.0.3 package (`releases/driver_ai_toolkit/1.0.3/`, DLL `1596E305...`) predates the Option-3 ABI
change; 1.0.3-r2 carries the current dev plug-in `6C60B666...`. Use r2.

## 1. Containers and proof (fact: code + offline test)

- Three big buttons WHO DO CHANGES GO TO?: ONE DRIVER (cyan) / SELECTED DRIVERS (purple, "Choose drivers...") /
  WHOLE GRID (orange). A banner names every receiving driver; the frame around all controls has the same colour.
  With a group the driver list becomes "Show values of" and switches to a driver inside the container.
- Bug fixed (fact): the old panel pushed EVERY slider value to every Apply-to car on any change, and an unmoved
  slider carried the selected driver's own value. Ticking Pace for "all AI" therefore gave every car the selected
  driver's pace. New rule: a slider the user moved sends that value to all; an unmoved slider sends each driver his
  own value (moved values turn yellow). Same for the Everything tab (per-car ORIG). Only the changed lever's fields
  are written now.
- Proof per lever: "applied n/N" next to each Quick and Everything lever; hover = per driver: plug-in status
  (APPLIED / not applied + why / refused) and, for fields that hold one game value (pace, corner x/+, wet, braking,
  aggression, max speed, steering lock, watchdog, baseline, push, push gamble, crash), the value read back from that
  car's own memory compared with the value sent. The Check tab (was Active) lists every active change per driver with
  a green/red dot and that proof line. Memory reads are read-only (`brain_core` reader).
- Rules: a rule ticked with ONE DRIVER / SELECTED gets that car mask; the Check tab says who it acts on.

## 2. Presets (fact: code + offline test)

- Schema `driver_ai_toolkit.preset.v2` (`tools/abl_presets.py`): target `grid` (always every AI car) or `current`
  (the container at load time); quick levers (unlisted fields keep each driver's own value), Everything levers, rules,
  global switches. v1 saves still load.
- Ready-made (`presets_builtin/`, shipped): No 75 percent throttle cap, Traction control OFF, Full throttle through
  corners, Tailgate before passing, Racier grid (all WHOLE GRID); Brave overtaker, Timid, Ignores blue flags, Late
  braker, Slow and careful (container).
- LOAD turns ARM on. "Load at every session start" (settings `autoload_preset`) applies once per session (plugin
  session counter / cars appearing). Save captures the reference driver's active levers + rules + 75 % fix.

## 3. Explanations (fact)

`tools/abl_help.py`: a SIMPLE text for all 16 Quick fields, 4 line controls, 5 policies, all 29 catalog levers, all
23 rules and the 75 % switch (test: none missing); ADVANCED = the existing technical text (+ field/evidence notes).
Switch SIMPLE / ADVANCED at the top (saved in settings); the info box at the bottom shows the last pointed-at
control in the chosen mode.

## 4. Address audit (source: `f1c_ida_decompiled/investigations/ai_brain_map/phase3/WRITE_READINESS_V12.csv`, 140 fields)

Covered by the tool (fact): every field with a known behaviour meaning that has a proven or derived write path:
0x4510 mode, 0x3A04 top gear, 0x4428/0x4408 read-back, 0x4420/0x4424/0x4548 line, 0x4504/0x4500/0x4580/0x453C/0x4554
corner + pace, 0x44CC/0x44C4 braking, 0x4434/0x4438 pedal scale, 0x4484 governor, 0x44B8 launch, 0x4558 watchdog,
0x44B4 steering, 0x42D4 overtake records, 0x45FC (clear only), 0x44C8/0x44C0/0x44FC/0x44D0-0x44F8 driver traits,
0x4538 max speed, 0x4588/0x458C/0x4590 pit plan, 0x3D70-0x3D7C reliability clocks, 0x4170 stall time; plus 23
code-constant rules and the 0x457C 75 % fix (global switch).

Not in the tool, and why:
- Gap worth adding (inference): **0x457C throttle cap per car** (finding `ai-throttle-cap-457c-latches-after-traffic-
  target-2026-10-03`): today only the grid-wide fix and the per-car "clear 0x45FC" exist; a per-car cap lever would
  give a direct "throttle ceiling" per driver. Needs a plug-in (DLL) lever = DeepSeek work.
- Protected / unsafe (fact, CSV risky=yes or SKIPPED_PROTECTED): 0x3D6C car_conditions, 0x4064 finish_state, 0x4410
  retired_flag, 0x454C call_to_box (retires the car), 0x45F4 ai_control_flag.
- Physics / pose / per-tick outputs, not behaviour settings (fact): position/velocity/yaw 0x34D4-0x3500, act_* 0x357C-
  0x3584, cmd_steer 0x442C, speed 0x4440, waypoint pointers, opponent pointers, watchdog counters/timers, command queue.
- Unnamed LATCHED fields with no proven meaning (fact: no finding names them): 0x4000, 0x4040, 0x416C, 0x441C, 0x444C,
  0x4458, 0x4460, 0x4488, 0x4490, 0x4524, 0x4544, 0x4550, 0x455C, 0x4568-0x4579, 0x4584, 0x45A0, 0x4894. Not
  exposable until research names them.
- 0x4508 / 0x450C pace traffic accumulator (fact: internal state of the 4 s pace model) - driven by Pace / push levers.
- 0x3A10 is not a shift timer (finding: dead write) - not exposed.

## 5. Bug found in the shipped client (fact; latent)

`tools/abl_shm.py` line 32 reset `RULE_MASK_OFF = None` after defining it (also in released 1.0.2 and the 1.0.3
package). Any per-driver rule write would raise TypeError. Latent: no plug-in build implements the v4 rule-mask block
(no RULE_MASK in `dll/`), so `v4` is never true live and the call is never reached. Fixed in source (line removed;
before-copy `abl_shm.py.BEFORE_20261008_ui`). Consequence (fact): per-driver RULES do not exist in any plug-in yet; with
the current plug-in every rule acts on the whole grid and the GUI says so. Per-driver rules = DLL work (DeepSeek).

## Files

Changed (before-copies `*.BEFORE_20261008_ui`): `tools/ai_brain_lab_gui.py`, `tools/abl_scope.py`,
`tools/abl_everything.py`, `tools/abl_shm.py`, `tools/build_toolkit.py`, `tools/gen_manual.py`, `MANUAL.md`.
New: `tools/abl_help.py`, `tools/abl_presets.py`, `tools/test_abl_ui.py`, `presets_builtin/*.json` (10).

## Pending (owner, live)

1. Install 1.0.3-r2 with the game (needs MF 1.0.3 for the matching race-control mapping).
2. WHOLE GRID: load "No 75 percent throttle cap" -> Check tab shows it; tick "load at every session start", restart a
   session -> it loads by itself.
3. ONE DRIVER = M. Schumacher: tick Corner speed +, move it -> "applied 1/1", hover shows "in game ... OK"; Check tab
   lists it only under Schumacher.
4. SELECTED (2 drivers): tick Pace without moving -> each keeps his own pace (hover values differ); move it -> both get
   the moved value.
5. SIMPLE / ADVANCED switch and the info box read well.
Follow-up for DeepSeek (not started): per-car 0x457C throttle-cap lever; v4 per-driver rule masks in the plug-in.

## 6. Verification status of the values (owner question 2026-10-08; fact = catalog tiers)
- Catalog levers (28 usable): FACT 2, CONFIRMED 13, DERIVED 13 (read from code, not proven live: quali_skill,
  driver_variant, corner_target_mult, launch, pit_stops, next_tyres, engine_life, stall_time, top_gear, pace_push,
  push_norm, brake_ramp, watchdog_limit).
- Rules (23): CONFIRMED 9, DERIVED 14 (yield radius, side-step offsets, traffic lift factor, slow off-track throttle,
  TC brake gain, all 9 following/overtake-decision constants).
- Quick policies EXPERIMENTAL: overtake commitment, cooperation; DERIVED: speed governor, throttle/brake scale.
- What the new proof shows: the value is WRITTEN and held in the car (applied + memory read-back). It does not prove
  the driving effect claimed in the text; that needs per-lever live A/B tests (not done).
- Coverage: complete for fields with a known meaning and a write path (section 4); NOT complete for all writable
  memory: ~25 unnamed LATCHED fields are unresearched, per-car 0x457C is missing, and per-driver rule masks are not
  in the plug-in.

## 7. Live check: Barrichello trains behind R. Schumacher (owner report 2026-10-08)
Read-only elevated sampler, 10 Hz, sim 300-390 s. Finding `ai-follower-stuck-in-hold-mode5-chase-train-2026-10-08`.
- FACT: Barrichello's record for Ralf = mode 5 (hold) in all 400 samples of run 2 (and all of run 1); never attempt modes (1/2/7/8).
- FACT: straight-line lifts are single ticks at ~311 km/h, 7th gear, steer 0, while closing (gap ~15 m): throttle 0.39 / 0.79.
- FACT: 0x457C cap = 1.000 for every car (not the 75 % latch). Exit part throttle 0.86-0.93 = steer lift (steer 0.10-0.16), and Ralf shows 0.91 too.
- FACT: Ralf has aggression 1.50 / passing 1.15 (not game defaults; looks like a toolkit or preset value). Barrichello 0.90 / 0.98.
- INFERENCE: hold mode makes him match Ralf's speed (the lifts), and the pull-out decision never fires. Which term blocks it is not proven.
Excerpt (t | RB km/h thrCmd thrAct brk cap | RS km/h thr | gap m | record slot:mode):
```
365.59 | 311 0.39 0.39 0.00 1.000 | 304 thr 1.00 | 15.8 | 1:5
386.50 | 311 0.79 0.79 0.00 1.000 | 308 thr 1.00 | 15.5 | 1:5
373.13 | 189 0.90 0.90 0.00 1.000 steer +0.14 | 198 thr 0.92 | 21.2 | 1:5
```
Tool today: Overtake "Commit" only keeps an attack that has started, so it does not help. Rules "Following & overtake decision" act on
the whole grid (no per-driver rules in the plug-in). DeepSeek item 5 in the 2026-10-08 toolkit prompt maps the blocking term.

## 8. Post-release fix (dev only, 2026-10-08): info box cut long ADVANCED texts
Owner report on live 1.0.3: the bottom info box showed only ~5 lines in ADVANCED mode. Fixed in `tools/ai_brain_lab_gui.py`:
the box is now a read-only Text with its own scroll bar (7 lines visible, mouse wheel scrolls). test_abl_ui 25/25; screenshot
check with an 800-character text OK. NOT shipped: goes out as Driver AI Toolkit 1.0.4 (changelog entry first).
