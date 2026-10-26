# CO-OP bots missing from the host saved replay - investigation (2026-10-13, Claude)

Owner observation (2026-10-13): host PC, CO-OP session. The saved replay shows both human
cars and none of the mod-added AI bots.

Scope: static analysis of `dll\coop_mode.cpp`, SDK decompiles, mod log, and replay files. **No game
launch, no writes to the game install or Documents folder, no code change, no deploy.**

Time convention: `multiplayer_overhaul_log.txt` stamps are UTC (`GetSystemTime`, coop_mode.cpp:226).
Local time = UTC+3 (the replay file name `17-43` equals log `14:43:16` UTC). All times below are UTC
unless marked local.

## Evidence chain (what happened)

| Time (UTC / local) | Event | Source |
|---|---|---|
| 00-00-00-00-00-00 / 17:42 | 22 bots prepared (slots 32-54), `prepared selected co-op field ... total=24` | mod log |
| 14:42:56 | bot slot 34 released for a joining human; 22 bots remain, humans=2 | mod log |
| 14:43:15 / 17:43 | `race grid: source=qualifying cars=24 moved=24`, bots in slots 32-54 except 34 | mod log |
| 14:43:16 / 17:43 | `session change: session_type=7 phase=4 coop=1 host=1` = live race start | mod log |
| 00-00-00-00-00-00 | live race; **no** `driver processing mask` lines in this window | mod log (grep) |
| 14:44:24 / 17:44 | `session_type=7 phase=5 session_time=68.52` (race running) | mod log |
| 14:44:56 / 17:44 | `Pit sync inactive`; CO-OP race recording ends | mod log |
| 14:45:00 / 17:45 | `race grid: source=qualifying cars=24 moved=17` (second grid build, same message as a race start) | mod log |
| 14:45:08 / 17:45 | `session_type=7 phase=4 net_state=0 coop=0`; `COOP-I02 inert: game mode 6 without CO-OP selected` | mod log |
| 00-00-00-00-00-00 / 17:45-17:48 | `driver processing mask: stock_count=2 ai_hidden=22` every call, each followed by `restore: ai_restored=22 active_total=24` | mod log |
| 17:44 (file mtime) | `AUSTRALIA 2002 - CO-OP - Race - 2026-10-13 17-43.Vcr`, 515,039 B | Replays folder |

FACT: the mod never released its 22 bots when the game left CO-OP. `g_ai_count` stays 22 (coop_mode.cpp:97),
and nothing clears it on the mode change. `release_bot` is only called for "host removed it",
"a human took this car" and "a human is joining" (coop_mode.cpp:2612, 2705, 3781).

## Replay file comparison (static, file level)

The `.Vcr` files are gzip (magic 1f8b). Decompressed in memory only; nothing written.

| File | gz bytes | raw bytes | VEH roster entries in header |
|---|---|---|---|
| AUSTRALIA 2002 - CO-OP - Race - 17-43 (host, CO-OP) | 515,039 | 1,054,690 | **24** (2 humans + 22 bots) |
| ADELAIDE STREET CIRCUIT 1995 - Race - 15-56 (single player, AI) | 582,636 | 833,867 | 22 |
| AUSTRALIA 2002 - Multiplayer - Race - 17-41 (plain MP, 2 humans) | 34,687 | 65,160 | 2 |

FACT: the CO-OP replay header names all 22 bots (for example Schumacher, Wurz, Burti, TesterA, Coulthard,
Sato, Montoya, Raikkonen, Button, Massa, Alonso, Diniz, Trulli, Lotterer; 15 of the 22 names checked).
So the bots were in the file's roster when it was written.

FACT: the header format is an IRSR block (SCN/AIW names, then `name + VEH + bmp` per car). It is documented
only at this level. The 52-byte record framing was guessed (class 1, length 52) and found 0 hits, so the
per-car pose records in the body are NOT decoded. Per-car body presence is OPEN.

FACT: the ADELAIDE single-player file has no duration in the file and no race length was checked, so a
per-minute size comparison is not possible. Recording wall window for the CO-OP race is about 14:43:16 to
14:44:56 UTC (about 100 s, from the log).

## Answers

### 1. Which per-car condition makes the recorder include a car? Do CO-OP bots lack it?

- Recorder `sub_4B7A90` (IDA name PitTimingUpdate is wrong). Pose loop: `for i < dword_5C329C`, each car
  encoded with the 52-byte class-1 record, `car+18580` as id (PitTimingUpdate_004b7a90.c lines 20-40;
  SetupCarLightingAndRender_004c71c0.c lines 1-20). FACT: the loop body has **no role, controller or
  live-marker check**. The decompile hides the per-car argument (`SetupCarLightingAndRender(v15)`), so the
  car pointer is inferred from the disassembly that the prior investigation used (0x4B7B35). DERIVED.
- Encoder `sub_4C71C0`: no check of `+0x32C8`. It takes the car as `this`. FACT.
- The only role gate in this path is `veh+0x32C8 != -1`. It sits in `VehicleInstanceListBuilder`
  (`sub_4B5C50`, the `*(a2+13000) != -1` test) and in the per-slot extras `sub_4B6190`
  (`dword_5EF9A0[4656*a2] == -1` skip; 0x5EF9A0 = 0x5EC6D8 + 0x32C8). CO-OP bots have role 1 (host AI,
  g_ai_roles) or 2 (client remote), both != -1, so they pass. FACT.
- **Conclusion:** no condition in the recorder excludes CO-OP bots. The prior claim (recorder records all
  active cars) holds for this session. The roster evidence agrees (24 VEH entries).

### 2. Is the car list fixed at arm time?

- **No, not in the pose path.** `0x4BA6C0` calls the recorder through `sub_4B9870` with `a3 = &unk_5EC6D8`
  (non-zero) on every call. PitTimingUpdate therefore runs `VehicleInstanceListBuilder(this, a3)` on every
  call, and `dword_5C329C` is read live on each gated tick. FACT (decompile call chain).
- The per-slot extras `sub_4B6190` loop slots 0-103 in rotating blocks of 12 per tick, with the live
  `+0x32C8` check on each call. FACT.
- Arm `sub_4BA460` only sets the state (`*this = 1`, file name, `CreateDirectory`). The arm is at race
  start (`dword_7C56A4 == 7`, `session_type 7 phase 4` at 14:43:16). The bots were already prepared
  (00-00-00-00-00-00), so even a fixed-at-arm list would have included them. FACT.
- The one value captured at init is the player car id (`this+3868 = dword_7C54F0[...]+18580`,
  GameDataFileIO_004b7300.c). Bots are not affected. FACT.

### 3. Smallest safe fix options, ranked

The evidence points away from the recorder. The most likely cause is in the playback path, not the
recording path (see INFERENCE below).

1. **Release (unload) all owned bots when the game leaves CO-OP** (recommended). Trigger: the first
   `prepare_ai_field()` call that finds `coop_mode_is_selected() == 0` while `g_ai_count > 0`
   (coop_mode.cpp:2786 is where `COOP-I02 inert` is logged). Run the existing `release_bot` loop
   (coop_mode.cpp:2568) for every index, then `BuildActiveVehiclePointerArray`. That restores vanilla
   counts so the saved replay plays back like the single-player AI replay.
   Risks: must not fire during a live CO-OP race (trigger on mode change, not on session type alone);
   `release_bot` does a native unload, so run it on the game thread only, as now; a host that briefly
   shows the menu (game mode 5 to 6 and back) would unload bots and need a re-prepare. Needs a live test.
2. **Skip the hide/restore in `process_driver_entries_hook`** (coop_mode.cpp:3109) when
   `!coop_mode_is_selected()`, e.g. `if (!coop_mode_is_selected()) before_count = 0;` before the hide loop.
   This is the smallest code change, but it leaves the 22 bots as live cars during playback. It may show
   them at the wrong place or as doubles of the replay's own roster. Not preferred without a test.
3. **Re-arm or refresh the recorder's car list after bots join.** Not indicated: the recorder already
   re-reads the list each tick (answer 2). Do not change `sub_4B7A90`, `sub_4BA460` or `sub_4B7300`.

Scope rule: only CO-OP sessions set `g_ai_count > 0` (prepare_ai_field is gated by `coop_mode_is_selected`),
so the change should not touch Red Flag or single-player replays. Red Flag replays are a separate product
(`development\mods\red_flag`); verify with a RedFlag playback before any release.

### 4. What the client PC's replay would contain (INFERENCE)

- The client runs the same recorder. Its bots come from the host manifest (role 2, streamed), and the
  recorder captures the state the client applies (`0x4C73F0`, see prior investigation). So the client's
  header should also list the bots, and its body poses should exist for them.
- The client runs the same mod hook. If it plays back its own replay with bots still registered, the
  same missing-bot effect should appear there. Not tested.
- The client can have fewer bots if the manifest skipped some (`COOP-E10` / `COOP-E11`). Those would be
  absent from its header too.

## INFERENCE (the likely cause, not yet proven)

1. After the race the game went to game mode 6 (`ADDR_GAME_MODE` = 0x7D0410, coop_mode.cpp:267). The mod
   logged "inert" and kept its 22 bots registered.
2. From 14:45:08 the hide/restore hook (`process_driver_entries_hook`, coop_mode.cpp:3109-3160) ran on
   every call. It set 22 bots to role -1 and `VehicleCount` to 2 (stock), then restored them. This does
   not check the mode, so it runs in any game mode while bots are registered.
3. The 14:45:00 grid rebuild and the session 7 phase 4 start at 14:45:08 look like a replay start, not a
   live race (the race itself was over). Playback probably runs `ProcessDriverEntries` each frame. The
   log shows about one mask line every 6-7 ms from 14:45 to 14:48.
4. So during playback the vanilla driver-entry pass ran while the bots were hidden, and the replay's own
   car setup may have dropped the bots. This would explain "humans visible, bots not", and it fits the
   single-player AI replay (no mod bots, so the hook does nothing).

Why "no bots in playback" is not the same as "no bots in the file": the file has the 22 bot names in its
roster, and only the 14:45+ playback window had the hide hook active.

## OPEN (must be proven live)

- O1. Whether the replay body contains per-car pose records for the bots (needs the record framing).
  Decode the body or use an in-game playback check.
- O2. Whether the replay playback calls `ProcessDriverEntries` per frame (hook mask line count vs playback
  frames), and what "game mode 6" is (playback versus results versus other).
- O3. Whether the single-player AI replay playback shows AI (the owner says yes; not checked here).
- O4. Whether the host's in-game view is the same file as the Replays entry (the owner's viewing method was
  not recorded).
- O5. Client PC replay content (answer 4 is inference only).

## Pending live validation (owner, do not run by agent)

1. Host + client CO-OP race, few laps, save replay on both.
2. Watch the host replay from the menu (not directly after the race) and count the cars (humans + bots).
3. Check the mod log for `driver processing mask` lines during that playback (UTC 14:xx = local 17:xx+3h).
4. Pass = all 24 cars visible and moving in the host replay. Fail = log the mask line count and car count.

## Changed files and state

- New: this file.
- Appended: `f1c_ida_decompiled\findings.jsonl` (FACT/OPEN entries listed in that file, ids prefixed
  `coop-bots-replay-2026-10-13-`).
- Not changed: `coop_mode.cpp`, the game install, Documents, Replays, and no deploy. No governance file edited.
- Discoverability: this file is not yet linked from `development\mods\coop_mode\README.md`. Adding that link
  would edit an existing component document (preserve its before-state first). Owner decision.

## Verification performed

- SDK front door: `f1c_mod.py start --json` (state stale; recent entries read) and `evidence-query findings
  replay --json` (no replay-format finding).
- Decompile reads: `PitTimingUpdate_004b7a90.c`, `CallWrapper_9870_004b9870.c`, `sub_4BA460_004ba460.c`,
  `sub_4B6190_004b6190.c`, `VehicleInstanceListBuilder_004b5c50.c`, `GameDataFileIO_004b7300.c`,
  `SetupCarLightingAndRender_004c71c0.c`, `TwoPathDispatch_004ba6c0.c`, `sub_4B7500_004b7500.c`.
- Mod source: lines 97, 109, 194-226, 267, 316, 2568, 2612, 2705, 2778, 2786, 3109-3165, 3781.
- Log: `multiplayer_overhaul_log.txt` (UTC), grep by window; no full-log read.
- Replays: file sizes and mtimes; gzip decompression in memory; name and VEH counts. No file written.
- Not verified: body record framing, live playback, client PC, race length of the single-player file.

## Wasted-effort check

- Repeated reads of the SDK functions (some re-read after the first decompile).
- The first replay-body guess (52-byte record framing) returned 0 hits and was not followed up.
- Log grep matched a 20k-line spam window; counted with grep -c instead of reading it.
- Nothing else material observed.

## Fix (2026-10-13)

Status: **built and pure-tested; live proof pending (owner).** Implements recommended fix 1, with a
hold time and a network-state guard added to make the release transition certain.

### Change

- `dll\coop_mode.cpp`, `prepare_ai_field()` inert branch (COOP-I02, log line ~2786 before the change):
  calls the new `release_bots_after_leaving_coop()`. That routine releases every owned bot through the
  existing `release_bot()` loop (reverse index order, as at the existing `update_car_replacements` loop),
  then rebuilds the active vehicle array. It logs
  `COOP-R01 released N bot(s) after leaving CO-OP (game mode M, network state 0 for T ms)`.
- Release condition, decided by the pure helper `coop_bot_release_due()` in `dll\coop_menu_logic.c`:
  inert (`coop_mode_is_selected() == 0`), at least one owned bot, `network_state() == 0`, and the inert
  run has lasted `COOP_R01_HOLD_MS` = 5000 ms (GetTickCount, wrap-safe).
- Once per inert run: latch `g_inert_released`. The latch and the inert timer reset when CO-OP is
  selected again (the same place `g_inert_logged` resets).
- `process_driver_entries_hook()`: `if (!coop_mode_is_selected()) before_count = 0;` before the snapshot.
  This skips the mask/hide, the restore and the log lines together, so no marker is changed without
  being restored. Outside CO-OP the hook does nothing but call the original.
- Unchanged: every line mentioning `g_sync_host_ip`, `COOP-W30`, `coop_trust`, `coop_host_peer_ok`
  (verified identical with diff). No change to `release_bot`, the recorder, or the network code.

### Why it is safe (reasoning, not live-proven)

- A live CO-OP race is never inert: CO-OP is `game mode 5 AND g_coop` (`coop_sanitize`), so the release
  path cannot run while CO-OP is selected.
- The post-race replay log shows `net_state=0` at the inert transition (14:45:08 UTC, `coop=0`). A live
  networked CO-OP session has state 1-3 (`network_state()` checks 1/2/3 across the file). So the
  network-state guard is the signal that separates "replay/menu" from "session still running".
- The 5 s hold removes a one-frame or short mode flicker from the trigger.

### Assumptions and open risks (stated, not verified)

- A: Between the P, Q and R sessions of one weekend, the game does not leave mode 5 + CO-OP, or does not
  reach network state 0 while doing so. If it does, the bots are released between sessions and are
  re-prepared when CO-OP is selected again (`prepare_ai_field`). The owner's live test below checks this.
- B: `release_bot()` during replay playback is a native unload while the replay is running. Not tested.
  It is the same native path the mod already uses mid-session, but the playback context is new.
- C: The 5 s window after the inert transition still has the bots registered and visible. The hide is
  skipped during that window, so they are not hidden (the replay may show them as live cars until the
  release). This is brief by design.
- D: `netcomm_slot_sync_hook()` (near line 3963) also runs the hide/restore block. It was NOT changed
  (the owner scoped the fix to `process_driver_entries_hook`). If the playback path also runs that hook,
  bots could still be hidden there. Needs the live log check below.
- E: The release reason check is inference from `net_state=0` in one log. The replay log in
  `multiplayer_overhaul_log.txt` after the fix is the evidence to check.

### Tests and verification (run in this task)

- Build: `cmd //c "...\dll\build.bat"` -> `Built ...\AppData\Local\Temp\multiplayer_overhaul_build\multiplayer_overhaul.dll`
  (32-bit MinGW, `-Wall -Wextra -Werror`), exit 0.
- DLL SHA256 (`%TEMP%\multiplayer_overhaul_build\multiplayer_overhaul.dll`):
  `ed17fa90c2d0290888f2b561bdbe52cf2f5b8ed3982b595e760b757a5c87bdc0`
- Pure test: `python -X utf8 tests\test_coop_menu_logic.py` -> PASS, 10 new COOP-R01 release-due cases
  (not live, not collected by pytest because the file has no `test_` function; run directly).
- Suite: `python -X utf8 -m pytest -q tests` -> 16 passed (same count as before the change).
- Not run: any game launch, live CO-OP session, replay playback, deploy, or write into the game install
  or Documents.
- Structure validator (`tools\workspace\check-workspace-structure.ps1`): 2 errors before this task,
  unrelated to this change (unexpected top-level `.claude` and `plans` directories). Not changed here.

### Restoration

- `dll\coop_mode.cpp.BEFORE_20261013_botrelease`, `dll\coop_menu_logic.c.BEFORE_20261013_botrelease`,
  `dll\coop_menu_logic.h.BEFORE_20261013_botrelease`, `tests\test_coop_menu_logic.py.BEFORE_20261013_botrelease`
  and this document's `...BEFORE_20261013_botrelease` hold the before-state. Copy back to restore.
- The `findings.jsonl` entry `coop-bots-replay-fix-r01-open-2026-10-13` was appended (append-only file).

### Owner live test (not run by agent)

1. Start a CO-OP race (host), finish it, go to the menu, then open the replay of that race in the same
   process. Pass: all humans and bots visible. Check the log for one `COOP-R01 released N bot(s)` line
   with N = the bots still owned (about 22 for a full field), and no `driver processing mask` lines
   after it.
2. Without restarting, start the next CO-OP session (same weekend, P->Q->R style). Pass: the bots are
   back and the session runs with its full field. Fail: bots missing in that session, or
   `COOP-R01` fired between sessions (assumption A).
3. Record the log lines with UTC times. Local time is UTC+3.

### Wasted-effort check

- The first structure-validator call failed on backslash paths in bash; rerun with a quoted path.
- `coop_mode.cpp` (6853 lines) was read by grep-targeted ranges, not whole.
- The first pytest run of `tests\test_coop_menu_logic.py` did not collect (no `test_` function); the
  file was run directly. Consider a `test_` wrapper later (not done here).


## Live result step 1 (owner, 2026-10-13, MO 1.0.0 ED17FA90)

- FACT (log `multiplayer_overhaul_log.txt`): 15:28:38.013 `COOP-I02 inert: game mode 6`; 15:28:43.016-.065 23 `released bot ... (left CO-OP (COOP-R01))` lines, then `COOP-R01 released 23 bot(s) after leaving CO-OP (game mode 6, network state 0 for 5000 ms)`. The mod's release loop took about 50 ms.
- FACT (owner): the game froze for 1-5 s while the replay was playing, at about the time the bots were released ("same kind of freeze as when CO-OP is loaded"). No crash. After the freeze the bots ARE visible in the replay: step 1 PASS for visibility.
- INFERENCE: most of the freeze is native work after the release (vehicle unload/rebuild), not the mod loop. Assumption B (release during playback) now shows a visible cost.
- Proposed follow-up (not done): release on the menu before the replay loads (for example, on the results/menu transition with network state 0), so playback never pays the freeze. Needs a signal that separates "menu after CO-OP" from "between P/Q/R sessions" (assumption A).
- Step 2 (next CO-OP session keeps bots) and the `driver processing mask` check after R01: pending.

## R01 menu release (2026-10-13)

Status: investigated, NO code change (stop per task rule: no reliable earlier trigger found).
Owner goal: release the bots before the replay plays, so playback never pays the freeze. The logs do not
give a signal that is both (a) after the CO-OP session is truly over and (b) before the mode-6 replay
playback. The mode-6 path (current COOP-R01) stays unchanged.

### Change

- None by this task. No edit to dll\coop_mode.cpp, dll\coop_menu_logic.c/.h or the tests.
- Coordinator change is present in the tree and included in the build below: coop_mode.cpp lines
  2895-2911 (human placement; roster resolved into g_season_rosters[candidate]; backup
  coop_mode.cpp.BEFORE_20261013_humanplace). Not reviewed in this record.
- Documentation only: this section, and one findings line (id prefix coop-r01-menu-) in
  f1c_ida_decompiled\findings.jsonl.

### Trigger evidence (log multiplayer_overhaul_log.txt, UTC; grep only, no full read)

Main-thread `session change` lines are written only when session_type, phase or net_state changes
(not when coop or game mode changes). Worker-thread `Pit sync active/inactive` lines come from
mfb_set_state (coop_mode.cpp ~2155), driven by the network loop with `coop ? state : 0` (~2333). So an
inactive line means game mode left 5 OR network state left 1-3. The log cannot separate the two.

Run 1 (CO-OP race, 14:42-14:45):

| UTC | Line | Thread / source |
|---|---|---|
| 14:42:31 | menu: CO-OP selected | menu hook |
| 14:42:39 | Pit sync active (host, mod attached) | worker |
| 00-00-00-00-00-00 | session types 2..6 phase 0, net 2, coop 1 | main (weekend sessions, same network session) |
| 14:43:15 | race grid ... moved=24 | session-7 setup, coop 1 (race start) |
| 14:43:16 | session 7 phase 4, net 2, coop 1 | main |
| 14:44:24 | session 7 phase 5, net 2, coop 1 | main |
| 14:44:56 | Pit sync inactive (host, mod attached) | worker |
| 14:45:00 | race grid ... moved=17 | session-7 setup (replay setup) |
| 14:45:08.649 | COOP-I02 inert: game mode 6; session 7 phase 4 net 0 coop 0 | main, first net 0 |
| 14:45:08.591 | driver processing mask / restore | driver hook (playback running) |

Run 2 (short test race, 15:27-15:28): Pit sync active 15:27:51; session 7 phase 4 net 2 coop 1 at
15:28:08; race grid moved=24 at 15:28:08; Pit sync inactive 15:28:25; race grid moved=0 at 15:28:30;
COOP-I02 inert game mode 6 and session 7 phase 4 net 0 coop 0 at 15:28:38; COOP-R01 released 23 bot(s)
at 15:28:43 (= first inert + 5000 ms hold).

Negative and cross-check facts (FACT, same log):

- session change lines with net_state=0 and coop=1: 0 in the whole log (all CO-OP sessions today,
  including the weekend sessions at 14:19-14:24, 14:40-14:45 and 15:27-15:28).
- Between P sessions of one weekend the worker logs Pit sync inactive at 14:20:29 and 14:21:08 (active
  again at 14:20:43 and 14:21:15) while the main thread logs net 2 and coop 1 throughout. So an inactive
  transition on its own happens between sessions. It is NOT a session-over signal.
- The first main-thread net 0 is at the replay start in both runs (14:45:08, 15:28:38). The main thread
  logged net 2 right up to that point.
- The current release is gated on the first driver-hook call that sees inert (playback start) plus the
  5000 ms hold, so it happens about 5 s into playback. The hold, not the detection, moves the release
  inside the playback window.

### Candidate triggers (ranked; why each was kept or rejected)

1. Worker Pit sync inactive at race end (14:44:56, 15:28:25). Earliest, 8-13 s before the inert line.
   Rejected: it also fires between P sessions (14:20:29, 14:21:08) with net 2; it cannot tell game mode
   from network state; and it runs on the worker thread, so the release would still have to run on the
   game thread (rule R2).
2. Session-7 setup with coop 0 (race grid line at 14:45:00 and 15:28:30, game thread, before playback).
   Kept as the best candidate, NOT proven. The race grid line prints no game mode, coop or net state; the
   same line also fires at race start with coop 1 (14:43:15, 15:28:08); the main thread logged net 2 at
   that moment, so the existing net-0 guard would block it; dropping that guard means releasing bots
   while the network session is still up (clients not checked). The coop 0 at 14:45:00 is an inference
   from the 14:44:56 inactive line and the 14:45:08 inert line.
3. Main-thread net 0 plus mode not 5 (current design). Reliable (never seen with coop 1), but the first
   observation is the playback start itself. No earlier than the current driver-hook path.
4. Remove the 5000 ms hold on path 3. Moves the release to the first playback frame, not before it.
   Not the goal. Owner decision only.

Decision: no candidate meets both rules with evidence in the logs. Per the task rule, stop without a code
change. Candidate 2 is the next item to prove.

### Tests and verification (run in this task)

- Build: cmd //c "...\dll\build.bat" on the current tree (includes the coordinator human-placement change)
  -> Built %USERPROFILE%\AppData\Local\Temp\multiplayer_overhaul_build\multiplayer_overhaul.dll
  (-Wall -Wextra -Werror; output in %TEMP% only).
- DLL SHA256 (current tree, no R01 change from this task):
  c9a993442f5ba79e90ffa9440a6649d78325184c04b83d58bc98718a4908ec4d
  (the earlier R01 hash ed17fa90... in the Fix section is superseded by the coordinator change).
- python -X utf8 tests\test_coop_menu_logic.py -> PASS: 36 change, 18 display, 10 walks, 10 COOP-R01
  release-due cases.
- python -X utf8 -m pytest -q tests -> 16 passed.
- Not run: any game launch, live CO-OP session, replay playback, deploy, or any write to the game install
  or Documents. The game is running; no runtime exe was started.

### Restoration

- No code file changed by this task. Doc before-state: INVESTIGATION_COOP_BOTS_REPLAY_2026-10-13.md.BEFORE_20261013_r01menu.
- f1c_ida_decompiled\findings.jsonl.BEFORE_20261013_r01menu holds the before-state of the append-only
  findings file. Restore only by removing the coop-r01-menu- line.

### Next step and owner live test (needs owner approval, a new build and a live run; not run by agent)

- Log-only dev build: at the session-7 setup point (race grid block) log game mode, coop, net state and
  owned bot count; add mode and net to the Pit sync transition line. No release behaviour change.
- Owner: host CO-OP race, finish it, watch the replay. Pass for the diagnostic: the setup line for the
  replay shows coop 0 and the net state, before the first driver processing mask line; the race-start
  setup line shows coop 1 net 2. If the replay setup shows net 2 throughout, the net-0 guard must change
  (owner decision) before any earlier release.
- Also repeat the Fix-section check: P/Q/R weekend, no COOP-R01 line between sessions (assumption A).

### Open risks

- R-A: Pit sync inactive between P sessions shows that the worker sees a mode or state drop that the main
  thread does not log. The cause (mode flicker or state) is unproven.
- R-B: A release on path 2 would run while the network session is still up (net 2). Effects on a
  connected client are not checked.
- R-C: The 5000 ms hold delays the release into playback. Freeze timing is unchanged by this task.
- R-D: Only lines 3380-3445 of coop_mode.cpp were read for the race grid block. Confirm its latch before
  any use.
- R-E: The coordinator human-placement change (18:42) is in the built DLL and is not covered by this record.

## Owner decision (2026-10-13 evening)
The 1-5 s freeze at replay start is acceptable ("main thing is that things work"). No further R01 trigger work; the diagnostic log build is not wanted.
