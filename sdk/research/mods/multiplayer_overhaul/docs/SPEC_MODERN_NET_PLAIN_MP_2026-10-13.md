# SPEC: Modern net mode for plain multiplayer (2026-10-13)

Stage: **investigate + specify** (no code edits, no game run, no install, no release).
Product: Multiplayer AI Mode (`coop_mode`), shipped 0.9.6, dev source in `dll\coop_mode.cpp`.
Governing standard: [MOD_STANDARD.md](../../MOD_STANDARD.md) (every mod follows it; this spec is the
"cite it in specs" record for this change).

Evidence tiers: **FACT** = read in source or docs (file:line given). **INFERENCE** = reasoned, not proven.
Line numbers refer to `dll\coop_mode.cpp` unless stated.

## 1. Objective

Owner goal: the **Modern** connection type (uncapped car sync, `[Net] Mode=Modern`, `SendHz` 60) must
also be selectable for **ordinary multiplayer sessions with humans only and CO-OP AI off**, not only
when CO-OP is picked on the main-menu mode spinner.

Success = a plain MP host and a plain MP client, both with the mod, run Modern (client logs the host's
Modern value); a plain MP session with a client that lacks the mod falls back to Vanilla without errors.

## 2. Non-goals

- No bots, no BotStream (N2), no CO-OP grid/quali/pit changes. Bot code stays CO-OP-only.
- No new menu page and no change to the main-menu mode spinner (`0x5A2400`, 9 items). Rejected: adding a
  separate "MULTIPLAYER MODERN" item there (changes the 9-item index layout and `coop_display_index`, FACT
  coop_mode.cpp:238-331, higher risk for no gain).
- No change to the native connection-type value (`0x7D0E50`). Writing a value above 4 is unsafe (jump table
  at 0x464127, FACT NET_SETTINGS_UI_2026-10-11.md section 3). Modern stays a mod-side override.
- No own transport (N2 UDP) for plain MP. No interpolation delay (N1b).

## 3. Gate inventory (where Modern is gated on CO-OP)

`coop_mode_is_selected()` = `coop_sanitize(game_mode, g_coop)` = game mode 5 AND CO-OP item chosen
(coop_mode.cpp:285-288; coop_menu_logic.c:34-37). Plain MP = mode 5 with `g_coop == 0`.

| # | Location | What it does | Needs CO-OP for Modern? | Change for plain MP |
|---|---|---|---|---|
| G1 | coop_mode.cpp:5298-5301 `net_modern_now()` | Master switch: `g_net_modern && !g_stopping && coop_mode_is_selected()`. Feeds both hooks and the one-time log. | **Yes (blocker)**. The logic itself works for plain MP. | Replace the predicate with `net_modern_scope()` = `!g_stopping && game mode == 5 && net_peers_ok()` (see section 4.2). |
| G2 | coop_mode.cpp:5315-5319 `send_interval_hook` (0x462FF0) | Modern returns `1/SendHz` per car. Uses G1. | Gated via G1 only. | None beyond G1. |
| G3 | coop_mode.cpp:5312-5313 bot-stream branch in same hook | Native bot rate when BotStream=2. | Bot-only, CO-OP correct. | Keep CO-OP-only. Must not be triggered in plain MP (no bots). |
| G4 | coop_mode.cpp:5329-5331 `conn_budget_body` (0x464E90) | Modern returns 8 000 000 B/s for every connection type. Uses G1. | Gated via G1 only. | None beyond G1. Note: ignores peer type (see R3). |
| G5 | coop_mode.cpp:5373-5376 `conn_label_hook` | Shows "Modern" when `g_net_modern && coop_mode_is_selected()`. | **Yes (display)**. | Use `net_modern_scope()` for the display gate. |
| G6 | coop_mode.cpp:5387-5401 `conn_change_hook` | Intercepts the Connection-type spinner only when `coop_mode_is_selected()`; forward from LAN (value >= 4) sets Modern; back sets Vanilla. Otherwise calls native. | **Yes (menu)**. | Gate on "game mode 5 and rules page" (not `g_coop`). Keep the LAN/back rules. |
| G7 | coop_mode.cpp:1959 `manifest_net_thread` loop top: `state = coop_mode_is_selected() ? network_state() : 0` | Sets state 0 for plain MP, so the UDP manifest server (`ManifestPort`, default 34500, line 1966-1983) and the client request loop (line ~2000-2013, `state == 3`) never run. | **Yes (hard blocker)**. Host never serves a NET line in plain MP. | Keep the full manifest CO-OP-only. Add a NET-only reply path (section 4.1) that runs in plain MP. |
| G8 | coop_mode.cpp:1981-1983 `COOP1?` handler and `build_manifest` (line 819, NET line at 849) | Full manifest (bots, GRID, NET). | CO-OP only (correct). | No change. Plain MP must never send the bot manifest. |
| G9 | coop_mode.cpp:856 `accept_manifest` NET parse (lines 905-925) | Client applies host Mode/SendHz/BotStream. | Reached only through G7. | Reuse the NET parse in a new NET-only accept function. Do not call `accept_manifest` in plain MP (it downloads/verifies content and bot lists). |
| G10 | coop_mode.cpp:6013-6017 INI read `[Net] Mode`, `SendHz` | Read at load. | No gate. | None. |
| G11 | coop_mode.cpp:6112-6113 `install_menu_hooks()`, `install_conn_menu_hooks()` | Installed unconditionally at load. | No gate (FACT). | None. The spinner hook exists in plain MP already and is inert. |
| G12 | coop_mode.cpp:2380 inert gate, 3235, 3318, 3392, 3444, 3592, 3791, 3991, 4402, 4728, 4824, 4879, 5625-5763, 5822 | Bot arming, grid, quali, bot stream, names, session. | Bot-only (correct). | Keep. |
| G13 | `docs\NET_SETTINGS_UI_2026-10-11.md` section 4 | Documents "CO-OP MODERN" label. | Docs stale: code says "Modern" (coop_mode.cpp:5376). | Fix the doc wording in the implementation record (owner decision 10-11c: label became "Modern"). |

Also relevant to menus: the Connection-type gizmo `APP_SpinBox_MultPlay_ConnectionType` lives on the
host rules page `MPRULESSETTINGS` (F12K2.osc lines 20145 and 20462, FACT; NET_SETTINGS_UI section 3). That
page is the same for CO-OP and plain MP, so the spinner is already shown in plain MP (INFERENCE that
plain MP uses this exact page; the mode-5 page flow was not traced live).

## 4. Proposed design

### 4.1 Negotiation (host to client), plain MP only

- New UDP request `NET1?` (6 bytes) on `ManifestPort` (34500). Reply `NET1 <Modern|Vanilla> <SendHz> <BotStream=0> <StreamHz=30>` then `END`. No bot list, no GRID, no file list, no content path.
- Host: `manifest_net_thread` binds the port when game mode is 5 and `network_state()` is 1 or 2, whether or not CO-OP is picked (changes G7). Serves full `COOP1?` only when CO-OP is selected (G8 unchanged).
- Client: in plain MP (`network_state() == 3`, game mode 5, CO-OP off) sends `NET1?` to the same host addresses as the manifest request (`HostAddress` list, else LAN broadcast). On reply, applies `g_net_modern`, `g_net_send_hz` (runtime only, as in dev4). Never calls `accept_manifest`.
- Logs: `net mode (plain MP): host uses Modern 60 Hz` on change; `net mode (plain MP): no reply from <ip>, staying Vanilla` on timeout.

### 4.2 Fallback when a human lacks the mod (safety rule)

Problem (FACT): the send-interval (G2) and budget (G4) hooks are **host-global**: `send_interval_hook` gets a slot,
not a peer, and `conn_budget_body` gets a type, not a peer. The host cannot run Modern for one peer and Vanilla for another.

Rule (proposed):
1. Host runs Modern in plain MP only when **every connected human peer has replied to `NET1?` in this session**
   (peer table from `mfb_peer_seen`, coop_mode.cpp:1814, keyed by IP).
2. Any peer that does not reply within 10 s (or joins later without a reply) forces the host to **Vanilla for the rest of the session**, with a log line `net mode: Vanilla (peer <ip> has no mod reply)`. Re-enable only at the next session.
3. Clients never run Modern without the host's value (client value comes only from the host NET1 reply).
4. Vanilla peers see the same packets they would see from a vanilla host (no new packet types on their side; the NET1 request is sent by modded clients only).

INFERENCE: a vanilla client receiving 60 Hz from a Modern host is probably fine (native record path), but this is
not proven (see test T5 and open question Q6).

### 4.3 Menu (owner UI)

- Keep one spinner entry: MPRULESSETTINGS Connection type. Forward from LAN = Modern; back = Vanilla (existing G6 rules). Display "Modern" (G5).
- Scope change: the intercept applies to any game mode 5 rules page, not only CO-OP. Both CO-OP and plain MP write the same INI `[Net] Mode`.
- Plain MP shows no CO-OP-only labels. The spinner label stays native otherwise.
- Owner decision needed (Q5): a separate label such as "Modern (plain MP)" if the owner wants it visibly different.

### 4.4 Lightloader settings impact

- Source: `development\lightloader\v2.0.0-hub-dev.1\manager_cs\ManagerSchema.cs` lines 86-92. Rows: `[Mod] Enabled` (line 88), `[Net] Mode` Choice "Online car sync" (line 90), `[Net] SendHz` (line 92), all under the Multiplayer AI Mode settings page.
- Mode help text at line 90 ("Friends use the host's choice.") becomes incorrect for plain MP. Proposed help: "Modern = smooth 60 updates a second in CO-OP and in normal multiplayer. Vanilla = the 2002 game. Friends use the host's choice. Any player without the mod = Vanilla."
- `[Mod] Enabled=0` still turns everything off (FACT coop_mode.cpp:6003-6006 path): plain MP is then fully vanilla.
- No new INI key is required for the feature (`[Net] Mode`, `SendHz` already exist). If a `PlainMpNet` key is wanted as a kill switch, add it as `[Net] PlainMP=1` (proposed; owner decision Q7).
- Lightloader version bump (2.3.5 by the NET_SETTINGS note) only at release prep, not for this dev change.

## 5. Compatibility and fallback summary

| Peer | Host behaviour | Client behaviour |
|---|---|---|
| Mod 0.9.6 (or dev with NET1) | Serves NET1 reply | Sends NET1?, follows host Mode/SendHz |
| Vanilla / no mod | No reply, host forced to Vanilla for the session | Native (no change) |
| Mod 0.9.5-dev4 or older | No NET1 reply (old code ignores unknown text), host Vanilla | Old code sends COOP1? only in CO-OP; in plain MP it sends nothing |
| CO-OP session | Unchanged full manifest | Unchanged |

Open risk: old 0.9.4 clients and CO-OP "NET" parse (mixed versions are already unsupported per NET_SETTINGS section 4).

## 6. Risks

- **R1 (FACT/INFERENCE):** `name_resolver` flips the game mode to RACE for one call (coop_mode.cpp:5822-5824). A hook that reads game mode while that flip is active can see the wrong scope. Mitigation: read the scope once per hook call; test in race start.
- **R2 (INFERENCE):** host-global hooks cannot be per peer; a mid-session join without the mod forces Vanilla for the whole session (section 4.2). Owner may not like it; it is the safe default.
- **R3 (FACT):** Modern budget ignores connection type (G4). A plain MP peer on a dial-up link would receive more than its link can carry. Rough load (INFERENCE): 2 humans x 60 Hz x 52 B = about 6 KB/s per direction, which is about the 56K line rate (about 7 KB/s). Mitigation: keep the audit's G3 warning in the UI help; Modern not offered if the host sees a peer with type 28K/56K (proposed check, needs the peer's type; open Q3).
- **R4 (FACT):** crash history. dev2 Modern crashed on join (ECX clobber, fixed dev3, coop_mode.cpp:5322-5331 comment). 0.9.5 live client mid-race crashes were unattributed (README.md line 9). Plain MP would add the same detour path to more sessions. Do not ship plain MP Modern before the 0.9.5 crash capture is read.
- **R5 (FACT):** a new UDP listener on 34500 in plain MP. The host must allow inbound UDP 34500 (already documented for CO-OP). Plain MP users without the rule would see only "no reply" = Vanilla, which is safe.
- **R6 (FACT):** the connection-type arrow direction is an inference (NET_SETTINGS section 3). Test before owner UI copy is final.
- **R7 (FACT):** docs say "CO-OP MODERN"; code says "Modern" (coop_mode.cpp:5376). Fix docs (G13).
- **R8 (INFERENCE):** a mod-side Connection-type value does not change what the native game negotiates for peers. If native uses the native LAN value for budgets elsewhere, Modern in plain MP may only affect this mod's hooks. Test T2 checks the effective rate (`net rx` in the client log) rather than the menu.

## 7. Implementation outline (after owner approves this spec)

1. Preserve before-state: `dll\coop_mode.cpp.BEFORE_20261013_plainmodern`, `dll\coop_mode.ini.BEFORE_20261013_plainmodern`, `dll\ADDRESS_INVENTORY.md.BEFORE_20261013_plainmodern` (and the docs being edited, under `workspace_organization_audit\` per AGENTS.md).
2. Add `net_modern_scope()`, `net_peers_ok()`, NET1 server/client path, peer-reply tracking. Replace G1, G5, G6 gates. Keep G3, G7 for CO-OP.
3. Add tests: NET1 encode/parse (python, like `test_n2_protocol.py`), menu-logic scope cases in `test_coop_menu_logic.py`, address inventory if new addresses are added.
4. Update ManagerSchema help text (section 4.4) and SelfTest checks.
5. Build with `dll\build.bat` to `%TEMP%` (no game run by agent).

## 8. Test plan (2-PC LAN, owner runs the game; agent does not run it)

| ID | Setup | Pass |
|---|---|---|
| T0 | Build + `tests\*.py` | All PASS; no new COOP-E line |
| T1 | Plain MP, host with mod Modern, client without mod | Host log `no reply ... staying Vanilla`; session plays normally; no bots (COOP-I02 inert line) |
| T2 | Plain MP, both mod, host Modern | Both logs `net mode: Modern active` / `plain MP: host uses Modern 60 Hz`; client `net rx` about 60 Hz per remote car (N0 method) |
| T3 | Same as T2, CO-OP off | No bot manifest fetched (no `manifest request` lines with COOP1?), no content download |
| T4 | Host Modern, join by a no-mod PC mid-session | Host logs Vanilla fallback; both PCs keep racing without a crash |
| T5 | Host Modern, client without mod, both in the same race | Client smoothness observation (owner) plus fps; decides Q6 |
| T6 | Menu on MPRULESSETTINGS, plain MP: LAN -> Modern -> LAN | Label and INI `[Net] Mode` update; native spinner unchanged for other values |
| T7 | Regression CO-OP host + 0.9.6 client | Full manifest still works; bots present; N2 BotStream unchanged (0) |
| T8 | `[Mod] Enabled=0` | Spinner native, no NET1 socket, plain MP vanilla |
| T9 | A/B same link: Vanilla vs Modern in plain MP, 2 laps | Owner verdict + fps + worst frame |

Evidence to record per run: exact DLL SHA-256, both logs (excerpts only, per retention policy), owner verdict.

## 9. Abort conditions

- Any COOP-E line, access violation, or crash in plain MP on either PC (stop, keep Vanilla default).
- A plain MP session with a no-mod peer runs Modern (fallback failed).
- Host fps or worst frame clearly worse in Modern than Vanilla on the same PC (owner verdict).
- The Connection-type value written above 4 or the native jump-table path reached (guard against regression of 0x464127 risk).
- Bot manifest or content download observed in plain MP (scope leak).
- The 0.9.5 mid-race crash capture is not read yet (do not ship; R4).

## 10. Open questions (owner decisions)

- Q1: Does native netcode use the connection-type value for any peer decision besides the two hooked functions? (Test T2.)
- Q2: Is game mode 5 used only for multiplayer rules/race? (Trace `game_mode_ptr` writers; not done.)
- Q3: Should Modern be refused when a peer's native type is 28K/56K (needs peer type read)?
- Q4: Host-only enable with "all peers must reply" (proposed) vs client choice. Confirm.
- Q5: Label: "Modern" (proposed) or "Modern (plain MP)".
- Q6: Vanilla client receiving 60 Hz from a Modern host: safe? (T5.)
- Q7: Add kill switch `[Net] PlainMP=1` (proposed) or no switch?
- Q8: Arrow direction on the rules page (NET_SETTINGS inference).

## 11. Documentation gate

- Record: this file; one line in `README.md` "Records (newest first)" linking here.
- Before-state of `README.md`: `README.md.BEFORE_20261013_modernplain` (created).
- Not appended to `f1c_ida_decompiled\findings.jsonl`: no new game RE result this session. All addresses cited were already inventoried; the gate analysis is mod-source FACT, not a new game finding. Revisit if T2 shows native behaviour (Q1).
- Pending validation: everything in section 8. No live result exists for plain MP Modern.
- No code, INI, DLL, release, production, or game-install state changed in this phase.

## 12. Implementation record (0.9.7-dev1, 2026-10-13)

Status: implemented in the dev source and built to `%TEMP%` only. Not run in game, not installed, not released. The agent did not run the game or load any DLL. Pending validation: section 12.6.
Before-states: `dll\coop_mode.cpp.BEFORE_20261013_plainmp`, `dll\coop_mode.ini.BEFORE_20261013_plainmp`, `dll\coop_mode.meta.json.BEFORE_20261013_plainmp`, `dll\coop_mode_meta.rc.BEFORE_20261013_plainmp`, `dll\ADDRESS_INVENTORY.md.BEFORE_20261013_plainmp`, `README.md.BEFORE_20261013_plainmp`, `docs\NET_SETTINGS_UI_2026-10-11.md.BEFORE_20261013_plainmp`, `docs\SPEC_MODERN_NET_PLAIN_MP_2026-10-13.md.BEFORE_20261013_plainmp`, `f1c_ida_decompiled\findings.jsonl.BEFORE_20261013_plainmp`.

### 12.1 Owner decisions applied
- Built now on the latest dev source; crash analysis not awaited.
- `[Net] PlainMP=1` kill switch (default on). `PlainMP=0`: plain MP is Vanilla and the spinner is native.
- Label "Modern" everywhere. "CO-OP MODERN" fixed in `README.md`, `dll\ADDRESS_INVENTORY.md` and `docs\NET_SETTINGS_UI_2026-10-11.md` (dated text otherwise unchanged).
- Every human peer must answer the NET1 check. A silent or late peer keeps the session Vanilla for its whole life. The native connection type (0x7D0E50) is never written, so Vanilla means the host's own native value.
- Bandwidth guard (INFERENCE, needs live tuning): a peer or the host with native link type 0 or 1 (modem classes, below 56K) is refused, and so is a peer whose NET1 round trip exceeds 250 ms.
- CO-OP path unchanged: same scope, same full manifest, same bot code. The CO-OP content-request gate in the client loop is preserved (`state == 3 && (!coop || !g_manifest_refused)`).

### 12.2 Changed code (`dll\coop_mode.cpp`, MOD_VERSION 0.9.7-dev1)
- `ADDR_CONN_TYPE` moved up next to `ADDR_GAME_MODE` (same value 0x007D0E50; the old definition in the spinner section was removed).
- New globals `g_plain_mp` (INI) and `g_plain_ok` (verdict), and `plain_mode_now()` (mode 5, not CO-OP).
- New `game_session_peer_ips()`: established TCP rows whose local port is `GAME_TCP_PORT` (34447).
- New NET1 block before the Mechanical Failures sync: `net_link_type_now`, `net1_reset`, `net1_set_verdict`, `net1_peer`, `net1_host_evaluate` (host verdict, once per second), `net1_host_answer`, `net1_client_tick`, `net1_client_reply`.
- `manifest_net_thread`: `coop` and `plain` scope; the state is network state for CO-OP or plain MP; MF channels are CO-OP only; `COOP1?` is answered only in CO-OP; `NET1?` is answered in plain MP; the client sends NET1 in plain MP; the bot manifest is accepted only in CO-OP.
- `net_modern_scope()` (CO-OP as before, or plain MP with `g_plain_ok`) and `net_menu_scope()` (CO-OP, or mode 5 with PlainMP). `net_modern_now()`, `conn_label_hook()` and `conn_change_hook()` use these.
- Config read: `[Net] PlainMP` (default 1), logged with the other Net values.
- `dll\coop_mode.ini`: `PlainMP=1` line with comment.

### 12.3 Protocol as built (differs from 4.1 in one point)
- Client to host, UDP 34500: `NET1? <link type> <last rtt ms or -1>`, every 2 s, only when the client runs plain MP with PlainMP=1.
- Host to client: `NET1 <Modern|Vanilla> <SendHz> <host link type>`. The client sets `g_net_modern` and `g_net_send_hz` from it, as the 0.9.5 NET line does, and logs `net mode (plain MP): host uses ...`.
- Change from 4.1: the client initiates. The host cannot send to a client's port 34500 because the client does not bind it. The host learns its peers from its TCP table instead.
- Host verdict = Modern only when: every TCP peer has sent NET1? (grace 10 s), each peer's link is 2 or more, each measured rtt is 250 ms or less, the host link is 2 or more, and `[Net] Mode=Modern`. Any silent peer or failed link sets a sticky fallback for the session.
- Client verdict expires after 10 s without a host reply (Vanilla).
- INFERENCE: the host accepts on TCP 34447. The table lookup is the same as the client side, which is FACT (coop_mode.cpp `game_session_host_ip`), but the host side is not live-verified.

### 12.4 Build and tests
- Build: `dll\build.bat` (32-bit MinGW g++, -Wall -Wextra -Werror), output `%TEMP%\coop_mode_build\coop_mode.dll`.
- SHA-256 of `%TEMP%\coop_mode_build\coop_mode.dll`: `3b97a83fe5183ba08fbebf480b798966ee968a81852f426da1e8a3ed78bfee8f` (445591 bytes, built 2026-10-13). An earlier interim build from the same session (before the CO-OP refused-manifest gate fix) is superseded and not a candidate.
- Headless tests (`python -I tests\test_*.py`), all PASS, run before the change and again after: address inventory 76 match; bridge ABI header copies identical; menu logic 36 change, 18 display, 10 walks; N2 protocol 5 tests OK; target probes 11 match; usage test PASS.
- Not added: the headless harness compiles only `coop_menu_logic.c` and the N2 protocol. The NET1 parsing and verdict code lives in `coop_mode.cpp`, which needs the game SDK headers. So the NET1 logic has no headless test. Testing it needs a refactor into a `.c` unit (follow-up).

### 12.5 Connection-type and replay findings (answers to the owner's questions)
- FACT: `0x7D0E50` is read natively by `NetComm_SetSelfAsReady` 0x460520, `SessionConfigBuilder` 0x461610 and `SessionTypeSetter` 0x463F70 (`functions/*.c`). The 0x461610 and 0x463F70 switches set the packet window `dword_7D0E98` (1->8, 2->12, 3->14, 4->32, default 6). No game-mode or CO-OP check appears in the lines read. The owner's belief "only vanilla MP and CO-OP use it" is therefore NOT confirmed: all MP sessions read the value natively. The mod hooks only the per-car interval (0x462FF0) and the budget (0x464E90), so the hook coverage does not include the packet window.
- INFERENCE: the packet window is unchanged by Modern, so it may still cap Modern throughput. Live measurement needed.
- Replay (OPEN): no network-state gate on the instant-replay key path was found. The ESC pause menu handles a REPLAY command (`development\mods\esc_pause_menu\esc_pause_menu.c:461`), but `STATUS.md:64` says REPLAY was removed from the visible menu (repair 2026-09-23). A broker source copy still injects DIK_R (0x13) for REPLAY. The DIK_R handler was not located in `f1c_ida_decompiled\functions` (`KeyboardInputActionMapper` 0x40BE70 has no case 19). MF-12 (2026-10-07) also found no replay policy. Not proven either way.
- Recorded in `f1c_ida_decompiled\findings.jsonl` (3 lines, ids `coop-conn-type-native-readers-20261013`, `coop-conn-type-window-unhooked-20261013`, `replay-key-network-gate-20261013`).

### 12.6 Pending live tests (owner runs the game; agent did not)
- P1 Plain MP, host and client both modded, host Modern: host `net mode (plain MP): Modern active`; client `host uses Modern`; no `COOP-E`; no `manifest request` lines.
- P2 Plain MP, one unmodded client: host logs a no-reply line for its IP and Vanilla for the session; the session plays on without a crash. This also tests the TCP-table assumption (section 12.3, INFERENCE). If no no-reply line appears for an unmodded peer, the assumption fails and Modern must not ship.
- P3 Bandwidth guard: a client with native connection type Modem (value 0) makes the host log a link refusal and Vanilla.
- P4 Host with `PlainMP=0`: plain MP Vanilla; spinner native; no NET1 traffic.
- P5 CO-OP regression: CO-OP host and client, full manifest, bots present, no NET1 lines.
- P6 Late join: an unmodded peer joins a Modern session; Vanilla after about 10 s.
- P7 Effect: fps and worst frame, Vanilla vs Modern, same PC, two laps (owner verdict).

### 12.7 Risks
- R-A (highest): the fallback depends on the host's TCP table showing unmodded peers. If it does not, an unmodded peer would run Modern. P2 must pass before any release.
- R-B: a peer in its grace window makes the verdict Vanilla (flicker). It returns to Modern once the peer answers.
- R-C: the client's `g_net_modern` is overwritten by the host reply and persists after the session, as the 0.9.5 NET line already does.
- R-D: all thresholds (grace 10 s, link 2, rtt 250 ms, request 2 s) are INFERENCE.
- R-E: the Lightloader Manager help text in spec 4.4 was NOT changed. It is outside this DLL change. Pending.
- Not done: no Lightloader version bump, no release, no production copy, no deploy.

### 12.8 Documentation gate
- Record: this section; `README.md` index line updated; label wording fixed in three docs (before-states kept).
- Facts vs inference: marked per item above (FACT / INFERENCE / OPEN).
- Pending validation: section 12.6.
- Workspace structure validator: 2 errors, both pre-existing and not from this change (`.claude` and `plans` top-level directories).
