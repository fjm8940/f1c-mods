# TV Director spec (2026-10-13)

Status: DRAFT for Claude review. Specification only. No code, no build, no game run, no live test, and no other file was edited.
Product: Telemetry (web_telemetry 1.1.0). Folder: `f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry`. Pipeline stage: define and investigate done for the camera read path; this document is the specify step. Implementation waits for storage v2 batch 4 (CONSOLE_BACKLOG_2026-10-12.md, "Owner request 2026-10-13").
Binding inputs: owner decisions D1-D7 in CONSOLE_BACKLOG_2026-10-12.md ("OWNER DECISIONS 2026-10-13").

Tags: FACT = read in source, decompile, or a recorded finding. DERIVED = inferred from source, not live-tested. OPEN = unknown; needs a test or an owner decision.

## 1. Objective and non-goals

1.1 Objective: a TV-broadcast style "TV Director" tab in the Telemetry web page. The operator picks a driver and a camera angle and cuts to it in the game. Opted-in clients follow the cut near-instantly (D4-D6).
1.2 Non-goals:
- No per-row camera icon in Session Overview (C1 dropped, D7).
- No accounts, login or permission model here. Viewer permissions belong to VIEWER_ACCESS_SPEC_2026-10-13.md. That file was NOT present in this folder when checked on 2026-10-13, so this spec only lists the permission hooks it needs (section 5.5).
- No change to Camera Editor, its profiles or Default.cfg.
- The browser never touches game memory. It sends intents; the DLL performs every game write on the game thread.
- No automatic director in phase 1.

## 2. Decisions in force and where they land

| Decision | Meaning | Section |
|---|---|---|
| D1 | Camera control lives in the telemetry DLL; copy the needed Camera Editor parts | 4 |
| D2 | New viewer = view-only until the host grants more | 5.5 |
| D3 | Identity = driver name + typed alias; link to CO-OP identity when connected | 5.4 |
| D4 | TV director is opt-in per player; off = nothing changes in that game | 5.2 |
| D5 | Host grants permissions; several directors allowed by a separate permission | 5.3 |
| D6 | Director commands near-instant (no 2 s manifest poll) | 5.2 |
| D7 | Top-level tabs "Telemetry" and "TV Director" | 3 |

FACT: `web_telemetry.dll.mod.json` description says "Read-only: it never changes the game." D1 contradicts that. The description must be changed with owner approval when phase 1 ships.

## 3. GUI

3.1 Top-level tabs: "Telemetry" | "TV Director". The Telemetry tab keeps its page set unchanged (RACE_PAGES at `telemetry_viewer.js:2284`: Session Overview, Timing & Race Pace, Driver Focus).

3.2 TV Director view. Phase 1 unless marked otherwise:
a. Driver list (phase 1): position, gap to leader, gap ahead, gap behind, battle flag, pit status, lap. Source: existing `/telemetry` JSON (`web_telemetry.c:8913`). Field names: OPEN, take them from API_CONTRACT_2026-10-09.md before coding. Battle and pit thresholds: OPEN (owner, Q4).
b. On-air strip (phase 1): current car name and current angle name, from `GET /director/state` (6.2). Phase 1 shows "own game" only.
c. Angle presets (phase 1): NOSECAM, COCKPIT, TVCOCKPIT, SWINGMAN (fixed director modes 0-3, `camera_editor.c:459`) plus ONBOARD names from the local Default.cfg in file order (`camera_editor.c:349-354`). Views disabled in the Camera Editor are greyed only when that DLL is detected (OPEN, 4.6).
d. One-click cut (phase 1): select a driver row and an angle, then press Cut. One CUT command goes to the DLL. Example: driver A on TVCOCKPIT to driver B on ONBOARD002 in one click.
e. Next-shot queue with Take (phase 3): stage a shot, then Take. Phase 1 has only immediate Cut.
f. Hotkeys (phase 1, minimal): Up/Down select driver, 1-4 fixed angles, Enter = Cut. Active only when the TV Director tab has focus and no text field has focus.
g. Auto-director suggestions (phase 4): battles, pit stops (`/pits`, `web_telemetry.c:8730`), incidents (`/incidents`, `web_telemetry.c:8731`). Suggest only. No automatic cut unless the operator enables a mode (OPEN).
h. On-air indicator (phase 2): which director is on air, the issuer name and time of the last command.
i. Follow switch (phase 2): "Follow TV Director" per PC, default OFF (D4), stored in the DLL ini. Changing it sends nothing.
j. Viewer without a local game: tab visible, controls disabled, tooltip "needs the game running on this PC" (C4; field `viewer_local`, 6.2).

## 4. Camera engine in the telemetry DLL (D1)

4.1 FACT: camera routines and globals sit at the same addresses in all six tested builds (finding `camera-code-signatures-all-builds-2026-10-04`, CONFIRMED). The resolver finds them by byte signature, not by hard-coded address (`development\mods\camera_editor\plugin\cam_sig.h:4-8`, patterns `cam_sig.h:46-53`). It refuses a build whose vehicle stride differs (`cam_sig.h:32`, check in `cam_resolve` at `cam_sig.h:114-146`).

4.2 Copy list (all paths relative to `development\mods\camera_editor\plugin\`):

| # | Part | Source | Phase 1 use | Tier |
|---|---|---|---|---|
| C-a | Hex pattern compiler and finder | `cam_sig.h:59-111` (cam_hex, cam_compile, cam_find, cam_one) | copy as is | FACT (tested by sig_test) |
| C-b | Resolver | `cam_sig.h:114-146` (cam_resolve), patterns `cam_sig.h:47-53` | copy; skip APPLY (`:46`) and LOCALCAM (`:40-43`) | FACT |
| C-c | Address struct | `cam_sig.h:16-28` | copy a subset: director_ptr, manager, dispatch, mirror, target_ptr, player_ptr, active_ptrs, veh_base, veh_stride, roster_count | FACT |
| C-d | Director offsets | `camera_editor.c:36-42` | MODE 0x34, COUNT 0x38, INDEX 0x3C, GENERIC 0x20, FIXED 0x10, MANAGER_MIRROR_READY 0x2D | DERIVED |
| C-e | Read of mode and index | `camera_editor.c:459-474` (fisheye_strength_now) | copy the read pattern only | DERIVED |
| C-f | ONBOARD index write and mirror rebuild | `camera_editor.c:615-633` (redirect_disabled: index write 627, mirror-ready byte 628), `camera_editor.c:636-647` (onboard_mirrors, mirror call 646) | copy the sequence | DERIVED |
| C-g | Name list | `camera_editor.c:186-243` (parse_file, LocalCam blocks), `:337-340` (is_fixed), `:349-354` (order), `:83-84` (g_root, g_save), `:825-835` (read_save_dir) | copy the name scanner only | DERIVED |
| C-h | NOT copied | profile merge `camera_editor.c:343-411`, write_view `:416`, rigid mount `:439-577`, fisheye apply `:450-458`, apply hook `:663-703`, detour `:712-733`, shared memory cam_shm.h | not needed for a cut | - |

4.3 Hook model (OPEN, Q1):
- Camera Editor runs on the game thread through a detour on the per-frame apply routine 0x409A00 (`cam_sig.h:46`, `camera_editor.c:663-703`, install `camera_editor.c:758-787`).
- FACT: web_telemetry.c has no detour or hook code today (grep for detour, VirtualProtect and hook, 2026-10-13).
- Option A: copy the detour (`camera_editor.c:712-733`) onto the apply routine. Run the command queue at the top of the hook. No new host dependency.
- Option B: use the Lightloader plugin host ABI (`lightloader_plugin_abi.h`). The telemetry mod requires Lightloader >=2.3.16 (`web_telemetry.dll.mod.json`). Needs a check that a telemetry mod can host a plugin hook.
- Recommendation: Option A for phase 1. Owner decides.
- Threading rule: every game call and every director write runs on the game thread, inside the hook (same model as `camera_editor.c:343` and `:663`). The HTTP handler (single-threaded accept loop, `web_telemetry.c:8764`, called at `:9309`) only enqueues. It never calls game code.

4.4 Exact-car switch (DERIVED; finding `camera-exact-car-retarget-chain-2026-10-13`):
Call GameStateDispatchChain_0040d240 (thiscall, ECX = manager 0x5EA510, argument = roster id). Verified steps from the decompile `functions\GameStateDispatchChain_0040d240.c:1-9`:
1. SetPlayerVehicleIndex (`functions\SetPlayerVehicleIndex_0042bbd0.c:1-15`). FACT: writes 0x7C54E0 (followed vehicle pointer) and 0x7C54DC (table index). It searches table 0x7C54F0 up to 104 entries; not found returns 104.
2. CameraTargetUpdateOnPlayerChange (`functions\CameraTargetUpdateOnPlayerChange_0040c7a0.c`). Retargets the camera. Details OPEN (not read in this spec).
3. CameraViewAssignment (`functions\CameraViewAssignment_0040c650.c:6-17`). FACT: if the target is the player's car, it runs GameActionStateMachine(view, 0) and clears the view state. Otherwise it runs ConditionalModeDispatchWithGlobalGate.
4. VirtualDispatchConditionalAudio (`functions\VirtualDispatchConditionalAudio_0040d090.c:1-10`). FACT: audio dispatch on dword_8AFE88.
5. AIOutOfTrackEvent (`functions\AIOutOfTrackEvent_0041a0c0.c:4-13`). FACT: for a non-player car, when dword_7D0308 and flt_7CF3C8 are nonzero, it inserts a driver data-table entry (InsertDriverTableEntry). OPEN: whether that entry changes race data or results. This is the main risk to the "camera only" rule (T2).
6. sub_4D8760 (`functions\sub_4D8760_004d8760.c`, gated setter). FACT: it writes *this = roster id and this[55] only when the gates pass (ID-order comparison, dword_5EA43C flag, Return0Stub). Otherwise it returns 1 and nothing changes. So a switch can silently fail. Phase 1 reads back 0x7C54DC and 0x7C54E0 and reports "refused". Gate conditions: OPEN.
- Roster id of a driver: `*(dword_7C54F0[N] + 0x4894)` (finding above; CameraViewAssignment reads the same field at line 6). Receivers map the driver name to their own roster id locally (5.2).
- Restore to own car: call the chain with the player's roster id, `*(player_ptr + 0x4894)`, player_ptr = 0x7C54E8 (`cam_sig.h:23`). Matches the numpadEnter behaviour (finding `numpadenter-returns-camera-to-player-car`, FACT).

4.5 Angle set (DERIVED):
- Preferred: ViewCameraModeDispatch_00409440(director, mode, dir, immediate), thiscall, ECX = director (*0x5EA528). It writes director+0x34 (mode) and +0x3C (index). Finding `camera-mode-setter-2026-10-13`.
- FACT remap: `functions\ViewCameraModeDispatch_00409440.c:21-30`. Mode 3 (SWINGMAN) becomes mode 2 (TVCOCKPIT) with dir 0 when byte_7CFE88 is 0, dword_8AB6E4 is not 2, and the followed car's byte at +13388 is set. The UI must show the mode read back, not the requested one.
- ONBOARD N: write +0x3C = N, clear manager+0x2D (MANAGER_MIRROR_READY), then call mirror setup 0x409890 (`camera_editor.c:627-628`, `:646`). Precedent exists. A direct write of mode 0-3 is unverified.
- Not used: keyboard injection (keys 199/201/209/210 via GameActionStateMachine_0040cde0; finding `keyboard-action-queue-background-input`). Injection is a trigger, not countable.
- Read-back: after each write, read mode and index on the next frame (`camera_editor.c:466-472` shows the reads). Mismatch = "not applied".

4.6 Coexistence with Camera Editor (DERIVED):
- Camera Editor refuses if the camera code was changed by another mod (`camera_editor.c:758-763`). Option B does not detour the apply routine. Option A must check the same bytes first (OPEN).
- Camera Editor redirects a disabled ONBOARD view to the next enabled one every frame (`camera_editor.c:615-633`). A cut to such a view will be redirected. Phase 1 reports "redirected" when read-back differs.
- Detection that Camera Editor is loaded: OPEN (its shared memory state, cam_shm.h). Needed before the greyed-out rule in 3.2c works.

4.7 Phase 0 prerequisites: T1 to T3 (section 7) must pass before any write code is merged.

## 5. Network, identity, permissions

5.1 Channels today (FACT, `development\mods\coop_mode\dll\coop_mode.cpp`):
- Manifest UDP on [CoOp] ManifestPort, default 34500 (`:767`). Client polls "COOP1?" every 2 s. Line format: slot, veh path, veh SHA-256, driver name (`:739-746`). The same number is used as a TCP content port (`:1281-1290`). NET line parse (`:906-925`).
- MF bridge: datagram tag "MFEV1", reliable events sent 3 times, resend queue of 16, 1400-byte packets (`:1949-1960`). Host send `:2050-2064`, resend pump `:2077-2100`. Pit channel uses tag "PFEV1" on the same bridge (`:1965-1969`).
- Bot stream UDP 34501, default off (`:759-761`).
- FACT: no camera or director message exists. Camera state is local only (finding `camera-state-local-only-mp-2026-10-13`, `NetCommPlayerSlotSync_0045fc50.c:255-269`).

5.2 DIR channel (proposed, phase 2):
- Port: new UDP port 34502 proposed. OPEN: grep the whole development tree for 34502 before use (only coop_mode dll sources were checked on 2026-10-13, none found).
- Not the 2 s manifest poll (D6). Reusing the MF resend loop (`coop_mode.cpp:2077-2100`) is acceptable with its own tag, like "PFEV1".
- Send: 3 copies per command, 100 ms apart, plus a re-send of the latest command every 1 s so late joiners sync (OPEN, numbers to tune).
- Wire format, one ASCII line per datagram, at most 512 bytes:
  `DIR1 <epoch> <seq> <issuer> <issued_ms> <kind> <driver_name> <slot> <angle> <mac>`
  kind: CUT (driver + angle), ANGLE (angle on the current car), CAR (car only), RESTORE (own car and saved mode).
  angle: NOSECAM | COCKPIT | TVCOCKPIT | SWINGMAN | ONBOARD:<name>. Use the name, not the index: index depends on each PC's Default.cfg order (`camera_editor.c:349-354`).
  driver_name: from the manifest name field (`coop_mode.cpp:741-742`). The receiver resolves its own roster id.
  mac: HMAC-SHA256 over the preceding fields with the session key. `sha256.c` and `sha256.h` already exist in the telemetry folder (FACT). Key distribution: OPEN (Q9). Proposal: sent in the host manifest over TCP, only after host approval.
- Receiver rules, in order:
  1. Follow OFF: ignore everything. No state change, no ACK (D4).
  2. Bad mac: reject and log.
  3. Epoch not equal to the current session epoch: reject (stops old replays).
  4. seq not greater than the last applied seq from that issuer: reject (latest wins per issuer).
  5. issued_ms older than TTL 2 s (OPEN): reject.
  6. Apply on the game thread at the next frame, read back, log the result (never the key).
- Sender: the director's own DLL applies the command locally first (same path), then sends.

5.3 Several directors (D5, phase 3): each director is an issuer identified by driver name plus a random id. Rule: latest issued_ms wins, ties broken by issuer id string. The on-air indicator shows the issuer. Host "single director" lock: OPEN (default off).

5.4 Identity (D3): typed display name plus alias. When connected to CO-OP, link to the driver name in the manifest. No login. Whether the alias is kept as well is OPEN (Q3; the backlog wording is ambiguous).

5.5 Permissions used here (reference VIEWER_ACCESS_SPEC for the full model):
- Viewer default view-only (D2). A CUT needs the "director" grant from the host.
- Phase 1: the director is the local user over loopback, so no grant is checked.
- Phase 2: the host grants "may direct" to send DIR1 (D5). Receivers check the mac and the issuer's grant. How grants reach receivers: OPEN.

5.6 Security:
- FACT: CO-OP UDP has no authentication (backlog research 2026-10-13). LAN spoofing is possible without a mac. The mac is required before phase 2 ships.
- FACT: HTTP listens on loopback by default (`web_telemetry.c:9239-9244`). LAN with [Mod] Remote=1 (`web_telemetry.c:846`, `:3925`). Write routes are loopback-only in phase 1, even with Remote=1.
- Keys are never written to logs or documents.

## 6. HTTP routes

6.1 Existing (FACT): GET /telemetry (`web_telemetry.c:8913`), /health (`:8919`), / and /index.html (`:8925`), /workbench (`:8931`), /viewer.js (`:8937`), /workbench.js (`:8947`), data routes `:8957-9154`. POST /sessions/delete (`:8818`) is loopback-only, needs a custom header, octet-stream body, 64 KB cap. POST /action returns 403 read-only (`:8908`). FACT: no Host-header check exists (grep, 2026-10-13).
Do not reuse /action: it is read-only by contract.

6.2 New routes:
- GET /director/state (phase 1). Returns: viewer_local (bool, from http_peer_is_local `web_telemetry.c:8691`), game_running, camera_engine (resolved / refused / encrypted-retry, same states as `camera_editor.c:736-756`), player roster id, current car name, mode, index, angle name, names list (fixed four plus ONBOARD), last applied seq, capture_time_ms, sim tick (to check the sim is not paused; memory rule), follow (phase 2), on_air (phase 2).
- POST /director/cut (phase 1). Body `{"driver":"<name or slot>","angle":"<key>"}`.
- POST /director/angle (phase 1). Body `{"angle":"<key>"}`.
- POST /director/restore (phase 1). Own car and saved mode.
- POST /director/follow (phase 2, local only). Body `{"on":true|false}`.
- POST /director/take (phase 3). Queue commit.
- Response: `{"status":"applied"|"queued"|"refused"|"not_applied","reason":"...","readback":{...}}`. Cut waits up to 1 s (OPEN) for the game thread, then returns "queued".

6.3 Rules for every POST:
- Custom header `X-Telemetry-Action: director`, same reason as `web_telemetry.c:8814-8817` (a custom header forces a CORS preflight, which the server never approves).
- Loopback peer only (`http_peer_is_local`, `web_telemetry.c:8691`). Remote writes are out of this spec.
- Content-type application/json, body cap 4 KB.
- The driver must map to a live roster id. The angle must be a fixed key or in the names list.
- One queue slot per request. The handler does not call game functions.

## 7. Live tests (owner runs them)

Rules: the owner runs the game. No automated gate, fuzz, or test program runs while a race is live (memory rule). Before judging any read, check that sim time or tick is advancing; identical samples mean a paused frame (memory rule).

T1 Read-only reads. Resolve addresses in a single-player race with no writes. Press the camera key 3 times. Pass: director pointer resolves, mode and index change with the key presses, player roster id matches `/telemetry`, capture time advances. Abort: crash or any write attempted.
T2 Car-switch side effects. With telemetry recording on, switch to driver B with the chain, then restore. Pass: the camera moves to B; positions, times and classification are unchanged for all cars; no new driver-table entry for B. Fail: any change in race data. Then car switch is blocked until a path without AIOutOfTrackEvent is found.
T3 Refused switch. Attempt a switch under a condition the gate should refuse (if reachable, OPEN). Pass: read-back shows no switch and the API returns "refused".
T4 Angle set via dispatch. Set modes 0, 1, 2 and 3 on the player car and on another car. Read back each. Record the SWINGMAN-to-TVCOCKPIT remap. Check the mirror view.
T5 ONBOARD write. Camera Editor not loaded. Set index N, clear +0x2D, call mirror setup. Pass: correct view, no crash for N=0 and N=count-1. Out of range is refused.
T6 Restore. After T2 to T5, restore. Pass: camera on the player car and mode back to the saved mode.
T7 Threading. Send 50 rapid cuts from HTTP. Pass: no crash, each game call logged with thread id = game thread, final state = last command.
T8 Coexistence. Camera Editor loaded: cut to a disabled ONBOARD view. Pass: redirect reported and no crash.
T9 Opt-out and follow (MP). Host cuts with the client Follow OFF: client view unchanged. Client Follow ON: client switches within the latency target (OPEN, proposal 0.5 s). Other viewers unchanged.
T10 Replay and spoof. Replay an old DIR1 line and a line with a wrong mac, crafted with a test script in %TEMP%. Pass: both ignored and logged.
T11 Host header. Request with `Host: evil.example` returns 403.
T12 Viewer without a game. /director/state shows viewer_local=false. POST returns 403. Controls disabled.
T13 Two directors (phase 3). Both send. Latest wins. On-air indicator changes. No crash.
T14 Record. Each test result goes in a dated operations record under the telemetry component, with capture times and the fact/inference split.

## 8. Acceptance criteria

- Phase 1: T1 to T7 pass. No car's race data changes except the camera. Tabs present. Every control matches its read-back.
- Phase 2: T9, T10, T12 pass. Measured latency recorded against the target.
- Phase 3: T13 passes.
- All phases: no other player's view changes unless that player opted in. No game write off the game thread. No secret in any log or document.

## 9. Abort and restore

Abort triggers: crash; race result or timing change not caused by the camera; a write from a non-game thread; a failed plausible() check (`camera_editor.c:152`); a DIR command from an unapproved issuer applied.
Restore: run 4.4 restore to the player roster id; restore the saved mode (read at the first game frame of the session); set follow OFF; stop sending.
Kill switch: a config flag in web_telemetry.ini (name OPEN, proposal `[Director] Enabled=0`) that disables the director code path at start.

## 10. Open questions

Q1 Hook method: detour (Option A) or Lightloader plugin ABI (Option B)? (4.3)
Q2 Accept the AIOutOfTrackEvent side effect, or find a route without it? (4.4 step 5, T2)
Q3 Keep the alias as well as the driver name? (5.4; backlog wording ambiguous)
Q4 Battle and pit thresholds for the driver list. (3.2a)
Q5 In-game overlay for on-air state, or browser only? (3.2h)
Q6 Allow Remote=1 LAN directors in phase 2? (5.6)
Q7 Default for Follow: proposal OFF. (3.2i, D4)
Q8 Approve the mod.json "Read-only" description change that D1 implies? (section 2)
Q9 Distribution of the HMAC key. (5.2)
Q10 Source of the saved mode for restore. (9)
Q11 Gate conditions in sub_4D8760 and whether a refused switch should be visible to the operator. (4.4 step 6)

## 11. Files that phase 1 would touch (not edited here)

- `web_telemetry.c`: routes (8764 onward), hook, command queue, Host check.
- `telemetry_viewer.js`: tabs (RACE_PAGES at 2284). Its SHA-256 is pinned in `web_telemetry.dll.mod.json` "sidecars", so the hash must be updated with every change.
- `web_telemetry.dll.mod.json`: description and version.
- `web_telemetry.ini`: follow and enable flags.
- web_telemetry.c and the storage files are also being edited by another agent. Coordinate before implementation.

## 12. Documentation pending (not done in this task; owner said edit no other file)

- Append to `f1c_ida_decompiled\findings.jsonl` (FACT, new): AIOutOfTrackEvent side effect (4.4 step 5); SWINGMAN-to-TVCOCKPIT remap (4.5); sub_4D8760 gated refusal (4.4 step 6); the chain has six steps, not the two in the backlog summary.
- Add a pointer to this spec in CONSOLE_BACKLOG_2026-10-12.md, owner request 2026-10-13 (owner to approve).
- Create the operations record for T1 to T14 when the owner runs the tests.
- VIEWER_ACCESS_SPEC_2026-10-13.md does not exist in this folder (checked 2026-10-13).
- Workspace validator (`tools\workspace\check-workspace-structure.ps1`) reports 2 errors, both unexpected top-level directories (`.claude`, `plans`). Not caused by this task.

## 13. Claude review (2026-10-13)

Verdict: ACCEPTED as the phase 0/1 basis, with these corrections.
- R1 Hook (Q1): add Option C and prefer it. Lightloader's broker already runs mod tasks on the game thread
  (`g_broker_post`, used by Red Flag `broker_replay_save_task`, `development\mods\red_flag\generated\redflag.c:2846`), and
  telemetry already registers with the broker (broker ABI 15 in /health). Option C needs no detour, so it cannot collide
  with Camera Editor's detour on the same apply routine 0x409A00 (Option A's main risk, 4.6). Phase 0 must confirm the post
  latency (one frame expected) and that telemetry's broker client may post tasks.
- R2 Section 1.2 is stale: `VIEWER_ACCESS_SPEC_2026-10-13.md` and `SECURITY_REVIEW_2026-10-13.md` exist in this folder.
  Phase 2 network work depends on SECURITY_REVIEW must-fix M1-M7 (incl. M3 Host allowlist, M7 HMAC); phase 1 needs at least
  the Host allowlist (6.3) and loopback-only writes.
- R3 Section 0 header says implementation waits for storage v2 batch 4: batches 4-6 are done (STORAGE_V2_BATCH5_RECORD).
  Coordinate edits of web_telemetry.c and telemetry_viewer.js (sidecar pin) with storage work: one editor at a time.
- R4 Q10 (saved mode for restore): read mode+index at the first game frame of each session (proposal in 9) - accept.
- R5 Phase 0 = a read-only build (resolver + GET /director/state, no write code) and owner tests T1; T2-T3 need the car-switch
  write and come in a separate gated build with the [Director] Enabled=0 kill switch default ON? -> owner decision D-TV5.
Owner decisions needed before phase 0: D-TV1 hook (C recommended), D-TV2 Q2 (accept AIOutOfTrackEvent side effect only if
T2 proves race data unchanged), D-TV3 Q4 thresholds (proposal: battle gap < 1.0 s, pit = in pit lane), D-TV4 Q8 description
change, D-TV5 kill-switch default, D-TV6 Q5 overlay (proposal browser only in phase 1).

## 14. Owner decisions (2026-10-13)
- D-TV1: camera commands through the Lightloader broker game-thread tasks (Option C). No detour.
- D-TV2: allow the car-switch chain only if live T2 proves race data unchanged.
- D-TV3: battle = the AI has taken repeated/continuous attack decisions against the car ahead (values to come from the
  Driver AI Toolkit / AI brain research, not a fixed gap); pit = car in the pit lane.
- D-TV4: approved: change the mod.json "Read-only" description when phase 1 ships.
- D-TV5: [Director] Enabled=0 by default until tested (write phases; phase 0 read-only build may ship enabled for T1).
- D-TV6: on-air display in the browser only for phase 1.
