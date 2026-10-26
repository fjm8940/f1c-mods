# SPEC: Require the Multiplayer Overhaul mod (refuse or kick vanilla peers), 2026-10-13

Status: SPEC ONLY. No code, DLL, INI, or workspace file was changed for this record.
Product: `coop_mode` (display name "Multiplayer Overhaul"). Stage: investigate -> specify.
Lifecycle per `development\mods\MOD_STANDARD.md` section 1 (define, investigate, specify, implement, verify, owner test, release prep, deploy, observe). This document covers define, investigate and specify only.
Related: `docs\SPEC_MODERN_NET_PLAIN_MP_2026-10-13.md` (NET1 handshake, section 12), `docs\NETCODE_AUDIT_2026-10-11.md`, `docs\INVESTIGATION_NET_SYNC_2026-10-09.md`.

Evidence tags: **FACT** = read in a decompiled function, string table, or workspace source (address or file given). **INFERENCE** = reasoned from FACTs, not run in game. **OPEN** = not determined; needs a disassembly pass or a live test.

Path note: all addresses are the game's 32-bit image as used in `f1c_ida_decompiled\functions\`. Hex-Rays names are often wrong; addresses are authoritative.

---

## 1. Define

### 1.1 Objective (owner, 2026-10-13)
When the host runs the mod, a player who joins with a vanilla game (no mod) must be refused by the game and told they are missing the mod, for example a native join-refused or kick message "Multiplayer Overhaul required - get it at <site>". The vanilla client cannot run our code, so the whole mechanism must be host-side and use native netcode only.

### 1.2 Non-goals
- No change to the vanilla client, no client-side install requirement for vanilla peers.
- No change to the 34447 game TCP port or to the NET1 protocol on 34500 beyond what section 4 lists.
- No custom wire protocol that a vanilla client must parse.
- No release, no Lightloader version bump, no deploy. Implementation is a later stage (section 7).

### 1.3 Acceptance criteria (for the implementation stage, not met by this spec)
1. With `[Net] RequireMod=1` on a modded host, a vanilla peer that joins a plain MP or CO-OP session is removed by a native path within a bounded time (proposed 15 s) and sees a message that names the mod and the site.
2. A modded peer is never removed (0 false positives in the owner test).
3. The removal never happens during a race (only in lobby / grid / qualifying states).
4. `RequireMod=0` leaves current behaviour unchanged.

---

## 2. Investigation (FACT unless marked)

### 2.1 Native join refusal: the message table and its reason codes
- FACT: The refusal texts are a fixed table, read by `sub_462DA0` (0x462DA0, `functions\sub_462DA0_00462da0.c`):
  - code 1 -> "Incompatible game versions." (string 0x5C6274)
  - code 2 -> "The game is full." (0x5C6260)
  - code 3 -> aGameClosed (string address not resolved in this pass)
  - code 4 -> "Bad password." (0x5C6240)
  - code 5 -> "Join timed out." (0x5C6230)
  - default -> "No join failure." (0x5C621C)
- FACT: The client-side join state machine `MultiplayerSessionJoinHandler` (0x46DE10, `functions\MultiplayerSessionJoinHandler_0046de10.c`) turns a failure into `sub_462DA0(EnumToEnumTranslator(...))`, then `LookupDispatch("joinfailurepage")`. The displayed text is that fixed table entry. The free-text reason from the wire is copied into a local 125-byte struct (`v12`) and never displayed (INFERENCE from the decompile flow; live check P1 in section 6).
- FACT: `EnumToEnumTranslator` (0x462D30, `functions\EnumToEnumTranslator_00462d30.c`) maps the transport status (0x57A590) to the table index: transport 2 -> 1 (incompatible), 3 -> 2 (full), 4 -> 3 (closed), 5 -> 4 (bad password), 0 -> no failure.
- FACT: Transport status is written by `sub_57BF00` (0x57BF00, `functions\sub_57BF00_0057bf00.c`), the UDP control-message handler. It parses a text message and stores the number in the connection object at +1476 (0x5C4) and an optional text at +1480. Parsed codes 2, 3, 4, 5 are stored as given. Any other code is stored as 1. Read and clear happen in `sub_57A590` (0x57A590).
- FACT: The same handler builds the outgoing reply text `"%s%s %d ..."` and sends it with `sendto` (replies to a join probe). Code 2 appends `" (%s)"` from object+2328. Codes 3 or 4 are chosen by object+93 (`4 - (byte != 0)`). Which condition selects code 2 versus 3/4 is OPEN.
- FACT: The control tokens are stored as strings near 0x5E537C-0x5E53A0: `TCPDeny` (0x5E5390), `TCPInfo` (0x5E5398), `TCPReq` (0x5E53A0), `<<HPWsock>> ` (0x5E53AC). The pointer-table to token mapping (`off_5E5384`, `off_5E5388`, `off_5E538C`) is not resolved.
- FACT: `sub_57DC10` (0x57DC10, DuplicateConnectionCheckRegister, `shadow_source\waves\wave_004\packets\0057dc10.json`) checks incoming connections for duplicates. Its refusal text ("Duplicate connection", 0x5E62C8) is a debug string, not a join refusal.
- FACT (rejected): No native path lets the host write free text into the join-refusal page. The only free-text channel the vanilla client displays is chat (section 2.3).

Conclusion FACT: a vanilla joiner can be refused with a native reason, but the visible text is "Incompatible game versions." (code 2), "The game is full." (code 3 in wire terms) or "Bad password." (code 5). None says "Multiplayer Overhaul required". Using code 2 would tell the vanilla player the wrong thing (a version problem).

### 2.2 Native kick and boot path
- FACT: Opcode 0x20 is a boot/kick notice in the message dispatcher `NetCommSessionMessageDispatcher` (0x45D890, `functions\NetCommSessionMessageDispatcher_0045d890.c`, case 0x20u). Header top bits `0x80000000`, opcode = `(header >> 18) & 0x3F`.
  - FACT: On a host (session state `this[4081]` == 1 or 2) the case returns at once, so opcode 0x20 is host-to-client only.
  - FACT: On a client, when the target id `a2[2]` equals the local node (`SingleFunctionDispatch(this)`), it shows "You have been booted" (string 0x5C5D54), appends it to the message list, calls `DualVirtualDispatchWrapper(this[4078])`, and sleeps 1 s. For any other id it shows `"<name> was removed from the game"` (0x5C5D38, name from `NodeIDToPlayerNameLookup`).
- FACT: The per-frame check `YouHaveBeenBootedHandler` (0x461D20, `functions\YouHaveBeenBootedHandler_00461d20.c`) also shows "You have been booted" once per session and sets the flag at `this+48`. It runs only when `SessionStateCheck` is in 1..3.
- FACT: `playerKicked.wav` (0x5D32F8) exists as a native sound, and the host UI has a native kick button `APP_Button_Multiplayer_Kick` (0x5CB734).
- FACT: The kick sender (the host code that builds opcode 0x20) is NOT in `functions\`. Searches for `0x20` builders found only `CollisionEventNetworkBroadcast` (opcode 32 = collision). The sender is OPEN. The host's own kick UI is probably in an APP_ handler not decompiled.
- OPEN: Whether the booted client actually leaves the session on its own (`DualVirtualDispatchWrapper` and the delegate in 0x461D20 are not traced to a disconnect). If it does not leave, the kick may only show a message. Must be tested live (P2).

### 2.3 Host chat (free text to every peer)
- FACT: Opcode 6 is a formatted player-text / chat message (case 6u). Layout: byte 0 = sender node id (used by `DriverDataCopyByID`), text at header+8, shown with `StringCopyWithFormatQuery` and `TextLineWrappingAndRendering` (0x45D890). It is shown when `!byte_7CE254 && byte_7D1044` is set. Text is formatted, so `%` in the text must be avoided.
- FACT: Strings `APP_Unique_Selbox_Multiplayer_ChatOutput_Host` (0x5CBCC0) and `..._Client` (0x5CBC90) show the chat list is a UI element, and `F1_TextInput_Multi_ChatInput` (0x5C86D0) is the chat input.
- FACT: The send side for opcode 6 is not decompiled in `functions\`. The header builder is `ProgressBarEventEncode` (0x4B33B0): header = `a5 + (a4<<8) + (a3<<18) + (...<<24)`, `a4` = extra byte count after the 8-byte prefix (`investigations\MULTIPLAYER_NETWORK_STATIC_MAP_2026-08-30.md`). Submission is `TextureCreationWrapper` (0x462250), which calls `NetworkSendDispatch` (0x57A6A0).
- FACT: The authority path relays frames from the host to every connected player (`TeamRadioNetworkMessageProcessing` 0x45F710, static map). So a host chat line reaches all peers, not only the target (INFERENCE that the relay applies to opcode 6 too; OPEN, see P3).
- FACT: Local-only text helper `0x45BD10` ("multiplayer message log add", documented in `dll\ADDRESS_INVENTORY.md`) shows text only on the local PC, not to peers.

### 2.4 Detection of a peer without the mod (existing code)
- FACT: `dll\coop_mode.cpp` (dev source, MOD_VERSION 0.9.7-dev1 per the spec record): peers = established TCP rows with local port 34447 (`game_session_peer_ips`, line ~1759). Host verdict once per second (`net1_host_evaluate`, ~1846). Silent peer after `NET1_GRACE_MS` = 10 s -> sticky Vanilla for the session (lines ~1863-1866). Client sends `NET1?` every 2 s to the host on UDP 34500 (lines ~1906-1915). CO-OP uses `COOP1?` on the same port (line ~2194 area).
- FACT: `mfb_peer_seen` (coop_mode.cpp ~1814, keyed by IP) is the other peer table the spec mentions.
- INFERENCE: A vanilla peer never sends `NET1?` or `COOP1?` to 34500, so after the grace it is distinguishable from a modded peer. Not live-verified: the host-side TCP table assumption (spec 12.3, risk R-A) is still the top open risk (test P2).

### 2.5 Refuse at join vs kick after grace (INFERENCE)
- A vanilla joiner's native join request (UDP `TCPReq`, `SendJoinRequest` 0x462C00 `functions\SendJoinRequest_00462c00.c`) is identical to a modded joiner's, and the mod sees nothing from it that says "modded". So at join time the host cannot tell them apart unless the modded client sends a probe first.
- Refuse-at-join is therefore only possible with a pre-join probe from the modded client (option B below). Without it, the only detection is after the TCP session exists (grace), so the removal is a kick, not a refusal.

### 2.6 Version and compatibility (FACT / INFERENCE)
- FACT: The native "Incompatible game versions." refusal exists (code 2 path above). The compared value is not identified in this pass.
- INFERENCE: The native version check compares the game build, not mods. Mixed mod / no-mod LAN peers on the same game build are not refused natively. A mod-level check must be ours.
- No hit in the findings index for "version mismatch" or "join request" (`evidence-query findings`, this pass).

### 2.7 Host option `[Net] RequireMod`
- Proposed key only. Not in the code yet. Default 0 until owner test.

---

## 3. Options (ranked)

### Option A (recommended first): post-join native kick + native chat warning, after grace, lobby only
1. Host: after the 10 s NET1 grace, a peer with no NET1/COOP1 reply is marked "no mod".
2. Host: send a chat line (opcode 6, host node id, text "Multiplayer Overhaul required - get it at <site>") to all peers. The vanilla player sees it in the chat list.
3. Host: send opcode 0x20 with the target node id, so the vanilla client shows "You have been booted". Other peers see "<name> was removed from the game".
4. Only in lobby / grid / qualifying states (session states where `SessionStateCheck` is 1..3 and no race is running). Never during a race.
- Pros: uses only native message types the vanilla client already handles; no new packet type; the text channel is ours.
- Cons: the kicked player sees the fixed "You have been booted" text, so the mod message must come from the chat line. The kick message is also broadcast to other players. Sender side of 0x20 and chat relay scope are OPEN (2.2, 2.3).

### Option B (later, if P1-P3 pass): pre-join admission refusal with native code 2
1. Modded client sends a probe to the host (UDP 34500, e.g. `NET1?`) before its join request. The host records the IP as "modded" for N s.
2. Host hooks `sub_57BF00` reply for a join request from an IP that did not probe: reply `TCPDeny` with code 2. The vanilla client then shows "Incompatible game versions." on its join-failure page.
- Pros: refused natively before any session is created; no kick.
- Cons: the text is wrong ("Incompatible game versions."), the owner's message cannot be shown on this page, a probe-first rule breaks any modded client that joins without the probe, the host hook depends on the reply-branch selector (OPEN, 2.1), and NAT / hairpin may drop the probe. Largest risk, highest reward.

### Option C (rejected): rewrite the outgoing refusal text
- The refusal text is a fixed table entry (2.1). Rewriting the reply text does not change what the vanilla client displays. Rejected.

### Option D (fallback only): chat warning without kick
- Only the chat line from Option A, step 2. Use if the kick path is not safe. Does not refuse anyone.

### Refuse vs kick and CO-OP vs plain MP
- Plain MP and CO-OP share the same silent-peer detection (NET1 on 34500 vs COOP1 on 34500). Use the same grace and the same kick path. CO-OP: a silent peer is a vanilla peer, because modded CO-OP clients send `COOP1?` (2.4).
- LAN with mixed versions: a different game build is refused natively already (2.6). Same build without the mod: Option A.

### Host option `[Net] RequireMod`
- `RequireMod=0` (default now): current behaviour (sticky Vanilla for the session, no kick).
- `RequireMod=1`: Option A after grace. Document in the Lightloader Manager help text (spec 4.4 of the Modern spec; not changed here).

---

## 4. Design details (for the implementation stage)

- New INI key `[Net] RequireMod` (default 0). Read next to `PlainMP` in `coop_mode.cpp`.
- Verdict per peer IP: `unknown -> modded | no_mod`. `no_mod` is set only after `NET1_GRACE_MS` with no reply. Re-check every peer every time the peer list changes.
- Removal action only in states where `SessionStateCheck` is 1..3 and the grid / qualifying page is open (state-specific gate to be defined in the implementation spec). Never during a race.
- Kick sequence: chat line first (opcode 6, text <= 100 characters, no `%`), then 0x20 with target id, then log `COOP-K01 removed no-mod peer <ip>` once per IP.
- Calls: `ProgressBarEventEncode` 0x4B33B0, `TextureCreationWrapper` 0x462250 and `NetworkSendDispatch` 0x57A6A0 are native and must run on the game thread (broker rule, MOD_STANDARD R2). Argument layout for 0x20 and opcode 6 must be confirmed (OPEN).
- Logging: one line per decision, no secrets.

---

## 5. Risks

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| K1 | False positive: a modded peer is removed because its NET1 reply was late or lost (UDP). | High | Grace 10 s plus two retries; kick only in lobby/grid; owner test P4. |
| K2 | The host-side TCP table does not show the vanilla peer (spec R-A). Without this, detection fails silently. | High | Test P2 must pass before any RequireMod release. |
| K3 | Kick during a race disconnects a human mid-race or corrupts results. | High | Lobby/grid gate only; abort condition A3. |
| K4 | 0x20 sender layout is wrong and the packet confuses the vanilla client or other peers. | Medium | Verify on two PCs with a dummy target before the owner test; abort condition A4. |
| K5 | The kicked player does not leave the session (2.2 OPEN): they sit in the lobby with a message. | Medium | Treat as a partial result; add a second native step only after live proof. |
| K6 | Chat text is not visible to the kicked player (chat panel hidden, `byte_7D1044` clear). | Medium | P3 checks the default chat flags; else use the message log path only on the host (not useful for the vanilla player). |
| K7 | Option B probe-first breaks modded clients that join without probing; NAT drops the probe. | High (if B is chosen) | Not for the first release. |
| K8 | Opcode 6 relay sends the warning to all peers, which shows an odd message to honest players. | Low | Word it for everyone ("A player without Multiplayer Overhaul was removed"). |
| K9 | The owner's `<site>` text is not decided. | Low | Owner supplies the text and URL before implementation; no placeholder ships. |
| K10 | Strings with `%` in opcode 6 text are formatted and garble output (2.3). | Low | Reject `%` in the warning text in code. |

---

## 6. Test plan (owner runs the game; the agent does not)

Setup: two PCs on the same LAN, both Windows, game build identical. PC-H (host, modded, Lightloader 2.3.x with the coop_mode DLL from the implementation stage). PC-C (client). PC-V (a third PC, or PC-C with the mod disabled in Lightloader, is a vanilla peer). One vanilla peer is required. Use a throwaway profile for PC-V. Logs go to `%TEMP%`.

| # | Test | Expected | Pass evidence |
|---|---|---|---|
| P1 | Vanilla join text | Vanilla joiner refused: which text is shown on a refused join (confirms 2.1 "fixed table" claim). | Screenshot of join-failure page. |
| P2 | TCP table assumption (K2) | Host log shows the vanilla peer IP in `game_session_peer_ips` and "gave no NET1 reply". | Host log lines with the IP. Must pass before anything else. |
| P3 | Chat visibility (K6, K8) | Chat line shows on PC-V and PC-C with the warning text. | Screenshot on PC-V. |
| P4 | Kick and false positive | PC-V: "You have been booted"; PC-C sees "<name> was removed". PC-C (modded) is not removed. | Both screenshots; host log. |
| P5 | Race gate (K3) | Start a race with PC-V present. No removal while the race runs. | Host log: no COOP-K01 during race. |
| P6 | RequireMod=0 regression | Same run with RequireMod=0: no chat warning, no kick, sticky Vanilla as now. | Host log. |
| P7 | CO-OP variant | Same as P4 in CO-OP mode with a vanilla joiner. COOP1 path. | Host log. |
| P8 | Leave behaviour (K5) | Does PC-V leave the session after 0x20, or only show the message? | Screenshot of PC-V 30 s after the kick. |

Pass order: P2, P1, P3, P4, P5, P6, P7, P8. Stop at the first failure (abort).

---

## 7. Abort conditions

- A1: P2 fails (the host does not see the vanilla peer in the TCP table). Do not ship RequireMod. Option A is dead; reconsider.
- A2: Any modded peer is removed in two runs (K1).
- A3: Any removal during a race (K3).
- A4: The host or a client crashes after a 0x20 or opcode 6 send (K4). Revert the send code; keep the detection log only.
- A5: The vanilla client shows a wrong state after the kick (for example, stuck in a non-leave state with a broken race grid) and cannot recover. Option A is not safe.
- A6: P1 shows that the refused join page displays free text from the wire (contrary to 2.1). Re-open Option B with the new text.

---

## 8. Pending validation and follow-up (documentation gate)

- Record: this file is the durable record. The index entry in `README.md` and the `findings.jsonl` entries for 2.1, 2.2, 2.3 and the OPEN items are NOT written yet, because this task was spec-only and forbade editing existing files. The owner should approve the `findings.jsonl` append (SDK rule, AGENTS "Game and SDK knowledge retrieval") and the `README.md` index line.
- Facts vs inference: tagged per item above.
- Unresolved (OPEN): kick sender layout (2.2), whether the kicked client leaves (2.2), opcode 6 relay scope (2.3), the reply-branch selector in `sub_57BF00` (2.1), the `aGameClosed` string address (2.1), and the TCP-table host-side assumption (2.4, P2).
- Wasted-effort check: none observed in the spec work except the first workspace validator call, which failed on a backslash path and was re-run. The validator reports 2 pre-existing errors (`.claude`, `plans` top-level directories), not related to this file.
- Not done: no implementation, no DLL build, no INI change, no release, no Lightloader change, no live test, no game run.

---

## 9. Implementation attempt, Option A (2026-10-13): BLOCKED at the send-path gate

Result: NOT IMPLEMENTED. Stopped before any call to an unproven native send path, per owner instruction.
- Before-state copy of this spec: `SPEC_REQUIRE_MOD_KICK_2026-10-13.md.BEFORE_20261013_optionA`.
- `dll\coop_mode.cpp`: NOT edited (no hash change). `dll\build.bat`, INI, Lightloader, production: NOT touched. No DLL built, no tests run, no game run.

Evidence gathered (FACT unless marked):
- Receiver side confirmed: `NetCommSessionMessageDispatcher` (0x45D890) `case 0x20u` (line 228) handles boot; `YouHaveBeenBootedHandler` (0x461D20) shows "You have been booted" once per session. Both are receive/display only.
- Sender NOT located. No `functions\` or `shadow_source\functions\` file contains the `0x80800000` header constant, the `Multiplayer_Kick` UI string, or a caller that builds opcode 0x20 with a boot target. Searches for "booted", "removed from the game" and the 0x5C5D38/0x5C5D54 strings return only receiver or unrelated files.
- CONFLICT (FACT): `functions\CollisionEventNetworkBroadcast_00462940.c` line 16 calls `ProgressBarEventEncode(4, 32, 4, 255, ...)` and `NetworkSendDispatch`. So the native sender of value 32 (0x20) that is visible in the corpus is the collision broadcast, while the receiver assigns 0x20 to boot. The opcode-to-argument mapping of `ProgressBarEventEncode` is not resolved, so the 0x20 meaning is ambiguous. Sending 0x20 from our code could produce a collision event on peers.
- coop_mode.cpp has no existing call to `NetworkSendDispatch`, `TextureCreationWrapper`, or `ProgressBarEventEncode`. There is no proven send precedent in the mod.
- Chat (opcode 6) send side is also unproven (spec 2.3); no mod code uses it.

Candidates for the next pass (OPEN, to be proven before any send): callers of `ProgressBarEventEncode` in `functions\ProgressBarEventEncode_004b33b0.c` (its callers list), and the host kick handler behind `APP_Button_Multiplayer_Kick` (0x5CB734), which is not in the corpus. Disassembly of the 0x20 argument layout (`exe-disassemble`) is the next gate.

Detection-only work (grace verdict, log line) was not started because the owner's rule was to stop as a whole when the send path is unproven.

Pending validation (unchanged): P1-P8 in section 6. Pending owner decision: whether to proceed with a disassembly pass to prove the 0x20 layout.

---

## 10. Round 2 (2026-10-13, coordinator correction): sender FACT, kick target NOT proven, still BLOCKED

- Coordinator sender claim VERIFIED by reading `f1c_ida_decompiled\functions\CollisionEventNetworkBroadcast_00462940.c`: the function (named wrongly) is the native boot sender. It is gated on netcomm state `*(this+16324)` == 1 or 2 (host), builds the message with `ProgressBarEventEncode(4, 32, 4, 255, ...)`, stores the target node id in `v8[2]`, then calls `NetworkSendDispatch`. Caller `0x46E1B0` (`DeferredTextureObjectLoader`, misnamed) reads the selected lobby name, calls `DriverDataCopyByID(dword_7CD25C, name, buf, &nodeId)`, then `0x462940(dword_7CD25C, nodeId)`. Caller `0x4613E0` (`ShortComputation_13e0`) boots every node in `netcomm+3869` (count `netcomm+3973`), so it is not usable for a single-peer kick.
- NOT PROVEN: the mapping from a peer TCP IP (`game_session_peer_ips`, or the accept path in `NetworkAccept_00580190`, which stores the socket and `getpeername` sockaddr only) to a netcomm node id. The node table holds node ids only. The `netcomm+0x3D0` entries (stride 140) hold `.veh` names, and `DriverDataCopyByID` matches a byte at entry+124. No IP field was found in either. Without this link, a kick could boot a modded player, so the kick is not implemented.
- Chat sender (class 4, subtype 6): still unproven. No decompiled caller found in this round.
- coop_mode.cpp: not edited. No build, no test, no INI change.

Next proof needed: a field in the native connection or node record that carries the peer address (trace `NetworkAccept_00580190` result storage and the node-creation path that writes `netcomm+3869`), then a 2-PC test that the IP maps to the vanilla node only.

---

## 11. Implementation (2026-10-13): Option A, node-id design, plain MP only, IMPLEMENTED, live test PENDING

Owner/coordinator design: no IP->node mapping. Modded clients put their own native netcomm node id in NET1?. The host boots only native node ids that no modded client claims.

Verified before coding (FACT, disassembly with `f1c_mod.py exe-disassemble`, `F1 Challenge Online.exe`):
- Boot receiver 0x45D890 case 0x20 (0x45DCE9): calls 0x462300 with ecx = `*netcomm`, compares the result with the target id `a2[2]`. So own id = `sub_462300(*N)`, N = netcomm object (`*(0x7CD25C)`).
- Boot sender 0x462940 (`CollisionEventNetworkBroadcast` name wrong), gated on state `*(this+16324)` in {1,2}; `v8[2] = node id`, sends class 4 subtype 32.
- Kick caller 0x46E1B0: `mov ecx, 0x7cd25c` (the ADDRESS of the global, not its value) then `call 0x462940` (0x46E212). So boot call = `ecx = 0x7CD25C`, arg = node id. Name -> id via 0x462460 (`mov ecx, 0x7cd25c`).
- Node list (0x4613E0): `netcomm+15476` (= +3869*4), count `netcomm+0x3E14` (= +3973*4).

Code (`dll\coop_mode.cpp`, MinGW build with -Werror, see 11.2):
- Client `net1_client_tick`: `NET1? <link> <rtt> <own node id>` (own id cached by the game thread; -1 if unknown). Old hosts ignore the 4th token.
- Host `net1_host_answer`: claim store `net1_claim_store` (table `g_net1_claims`, 32 slots, guarded by `g_manifest_lock`, claim window 6 s). A pre-1.0.0 peer (no 4th token) logs once: `sent NET1? without a node id`.
- Host tick `require_mod_tick` (game thread, from `netcomm_slot_sync_hook` after the native sync): runs only when `g_require_mod` (INI `[Net] RequireMod`, default 1), plain MP (`plain_mode_now()`), host state 1 or 2. Own id = `sub_462300(*N)`. Node list exactly as 0x4613E0. Each node except own: claimed -> state 1 (never booted); unclaimed for more than 10 s -> boot via 0x462940 (state 2, once per node), unless `session_type == 7` (race), where it logs `boot deferred` and waits.
- Chat: NOT sent (sender for class 4 subtype 6 not proven). Log only.
- Logs: `COOP-K01 booted native node N ...`, `node N claims the mod`, `race running ... deferred`, `sent NET1? without a node id`.

Scope and limits (not hidden):
- CO-OP is NOT covered: CO-OP clients send `COOP1?` without a node id, so enabling it there would boot every CO-OP client. Needs COOP1 node-id extension first.
- Plain MP clients with PlainMP=0 send no NET1? and would be booted. Clients must run 1.0.0 with PlainMP=1 (default). Owner decision needed.
- Pre-1.0.0 clients (0.9.x) are booted when the host runs RequireMod=1. Owner accepted this; all must update.
- MOD_VERSION left at 0.9.7-dev1. The 1.0.0 bump is a release decision, not made here.
- No Lightloader, production, deploy or game run.

11.2 Verification (this pass):
- Build: MinGW g++ (`C:\msys64\mingw32\bin\g++.exe`, flags from `dll\build.bat`), `-Wall -Wextra -Werror -std=c++17`, output `%TEMP%\mo_nodeid_build\multiplayer_overhaul.dll`, sha256 prefix `e3012276623d3683`. `build.bat` could not be run from this shell (cmd.exe did not find it); the same steps were run directly.
- Tests: `python -X utf8 -m pytest -q tests` in `development\mods\coop_mode` -> 5 passed (existing suite; no new test for the claim/tick logic).
- Source: `dll\coop_mode.cpp` sha256 prefix `28a7f8f0e670c36d`; before-state `dll\coop_mode.cpp.BEFORE_20261013_nodeid`; spec before-state `SPEC_REQUIRE_MOD_KICK_2026-10-13.md.BEFORE_20261013_optionA_r3`. Diff: 137 changed lines, one replaced line (the NET1? send).

11.3 Pending live test (owner, 2 PCs, one vanilla): host runs plain MP, one modded client, one vanilla client.
- Expect in host log: `booted native node <vanilla id>` after about 10 s in lobby. Modded client NOT booted, log `claims the mod`.
- Vanilla client sees "You have been booted" (native text only). No mod message, because chat is unproven.
- Race: start a race with the vanilla client present. Host log must show `race running ... deferred`, and no boot during the race.
- RequireMod=0: no boot, no claim logic running.
- Stop if a modded client is booted (abort A2).

---

## 12. Follow-up (2026-10-13): CO-OP enabled, pure decision header, headless test

Owner direction: CO-OP too; logic moved to a pure header; test the decision headlessly.

What changed (FACT, in code):
- `dll\coop_node_policy.h` (new, no Win32, C-compatible inline): `node_policy_step` (claims, grace, race defer, one boot per node, host own id skipped, non-host returns nothing, overflow above 32 nodes does nothing); `node_policy_parse_net1` ("NET1? <link> <rtt> <node>") and `node_policy_parse_coop1` ("COOP1? <node>"). Legacy two-token NET1? and bare "COOP1?" parse as unclaimed.
- `tests\test_node_policy.cpp` + `tests\test_node_policy.py` (headless; compiles with MinGW g++ `-Wall -Wextra -Werror` to a temp dir). Covers: claimed not booted; unclaimed booted only after grace, once; own id 3 skipped; race defers (logged once) then boots in lobby; client never boots; old-format requests unclaimed, new-format parsed; claim lapse gets fresh grace; overflow no action. Result: all checks passed.
- `dll\coop_mode.cpp`: CO-OP client sends `COOP1? <own node id>` at all four send sites (`coop_request` buffer). CO-OP host claims from COOP1? (`net1_claim_store`); a legacy COOP1? is logged once per peer IP as unclaimed. The tick (`require_mod_tick`) now runs for plain MP and CO-OP (`plain_mode_now() || coop_mode_is_selected()`) on hosts (state 1/2). The tick calls the pure policy, then performs the native boot (0x462940, ecx = 0x7CD25C) for BOOT events only. `net1_claim_live` removed (unused). `net1_claim_list` exports the live claims.
- Note: `coop_mode.cpp` had a change not made in this session when this pass started: the F-20 comment and `coop_host_peer_ok(from)` gate in the COOP1? host branch. It is kept as is; the claim parse sits above it.

Verification:
- Build: `cmd //c "%USERPROFILE%\Desktop\F1\development\mods\coop_mode\dll\build.bat"` (works with the full Windows path). Output `%TEMP%\multiplayer_overhaul_build\multiplayer_overhaul.dll`, sha256 prefix `c5011c0b86e5c173`. `-Werror` clean.
- Headless: `python -X utf8 tests\test_node_policy.py` -> all checks passed.
- Suite: `python -X utf8 -m pytest -q tests` -> 14 passed.
- Source hashes: `dll\coop_mode.cpp` `909586a23de5a037`; `dll\coop_node_policy.h` `5648759ce83eaa35`.
- Before-states: `dll\coop_mode.cpp.BEFORE_20261013_coopnode`, this spec `SPEC_REQUIRE_MOD_KICK_2026-10-13.md.BEFORE_20261013_optionA_r4`.

Open (not proven, not hidden):
- CO-OP node list: the native list (netcomm+15476) is assumed to hold human players only. CO-OP AI are bot slots, so a bot must not be a native node. OPEN until the live test shows the host log lists no bot ids.
- CO-OP clients built before this pass send no node id, so they are booted. Owner accepted this: all clients must update.
- Chat still not sent (sender unproven).
- `MOD_VERSION` still 0.9.7-dev1 (release decision).

Pending live test (2 PCs, one vanilla, and CO-OP variant): host log shows `booted native node <vanilla>` after about 10 s in the lobby; modded clients log `claims the mod`, not booted; race defers; in CO-OP, the host log lists no AI/bot node ids. Stop (abort A2) if a modded client is booted.

12.1 (2026-10-13) CO-OP dry-run: `[Net] RequireModCoop` default 0. In CO-OP, a BOOT decision logs `COOP-K01 dry-run ... would boot node N` and sends nothing; `=1` enables the real boot. Plain MP keeps the real boot (RequireMod=1). Gate: `node_policy_should_boot` in `coop_node_policy.h`, tested in `test_node_policy.cpp` (case 8). Test: all checks passed. Build `build.bat` ok, DLL sha256 prefix `55d0d49f`, `coop_mode.cpp` `231fe67e`. Before-state: `SPEC...md.BEFORE_20261013_dryrun`, `coop_mode.cpp.BEFORE_20261013_coopnode` (prior pass).

### Concurrency note (Claude, 2026-10-13)
- A parallel telemetry session edited coop_mode.cpp (F-19/F-20 sender trust, coop_trust.h) during this work. Checked 13:58 file: both change sets present; build.bat OK, DLL sha256 prefix 81821131a1ce983 (differs from agent build 55d0d49f because of later edits); pytest 14 passed. Re-verify before packaging.


---

## 13. Kick chat notice (2026-10-13)

Owner request: before the host removes a plain-MP node without the mod, every player sees why.
Implemented in `dll\coop_mode.cpp` (1.0.0 work, DLL sha256 prefix `f203bf04`). Build and headless
tests pass. NOT run in game: the 2-PC live test (13.6) is pending.

### 13.1 Behavior (FACT, in code)
- Warn stage, real boot mode only. A node that is due for boot (no mod claim after the existing
  10 s grace, lobby, not in a race) gets a WARN event first. BOOT (native 0x462940) follows
  `NODE_WARN_DELAY_MS` = 3000 ms later. A claim during the 3 s cancels the boot. A race start defers
  it, as before. After a claim lapse the wait restarts, with no second WARN.
- Real boot mode means: plain MP with `[Net] RequireMod=1`, or CO-OP with `[Net] RequireModCoop=1`.
  Dry-run (CO-OP default, `RequireModCoop=0`) never warns (`node_policy_boot_mode`).
- WARN sends one native chat line, broadcast (target -1), text 77 bytes:
  `Multiplayer Overhaul required. Get Lightloader: mods.palloset.com/lightloader`
  Log: `COOP-K02 kick notice broadcast for node N: 77 text bytes, send result 0 (1 = dispatch failed)`.
- Once per node per session: the policy keeps a warned list of 64 nodes. A full list sends no further
  WARN, but the boot still follows the delay. A new session (host netcomm state not 1 or 2) clears
  the policy state, so the next session may warn again.
- INI `[Net] KickMessage=1` (default 1). `0` = no chat; the boot still happens after 3 s and logs
  `COOP-K02 kick notice off ([Net] KickMessage=0)`.
- Known effect (by design): the chat is a broadcast and each unclaimed node gets its own WARN.
  Two unclaimed nodes = two identical chat lines. Owner decision pending (13.6, P-K6).

### 13.2 Code (FACT)
- `dll\coop_node_policy.h`: `NODE_EV_WARN` (5), `NODE_WARN_DELAY_MS` (3000). `NodePolicyWatch` gains
  `warn_pending` and `warn_ms`. `NodePolicyState` gains `warn_stage`, `warned[64]`, `warned_count`.
  New `node_policy_boot_mode(st, dry_run)`. Default `warn_stage` 0 keeps the 1.0.0 immediate-BOOT
  behavior that the existing test cases check.
- `dll\coop_mode.cpp`: `g_kick_message` (INI `[Net] KickMessage`, logged as `config: Net KickMessage=`).
  `send_kick_chat(node)` is called on WARN from `require_mod_tick`. `require_mod_tick` calls
  `node_policy_boot_mode` each tick, and clears `g_node_policy` when the host state is not 1 or 2.
- Chat recipe (asm 0x470503-0x47055C of the native sender 0x470490; decompile of 0x462250):
  1. zeroed `uint32_t packet[108]` (432 bytes);
  2. `ProgressBarEventEncode` 0x004B33B0 thiscall (ecx = packet; stack: class 4, opcode 6, len 77,
     byte0 0, float from the double at 0x008AFEA0, flag 1);
  3. `memcpy(packet + 8, text, 77)`, `packet[85] = 0`;
  4. `TextureCreationWrapper` 0x00462250 thiscall (ecx = 0x007CD25C; stack: packet, mode 0,
     target -1 = broadcast, flag -1). Returns char 1 only when `NetworkSendDispatch` failed.
- No SEH: this file has no `__try`. The chat call is unguarded, like the boot call 0x462940.
  The guards are the startup probe check (13.3) and the host-state check in `require_mod_tick`.
  The native precondition `SessionStateCheckWrapper(0x7CD25C) != 0` (investigation 2.3) is not
  called; the host state 1 or 2 check is used instead.

### 13.3 Probes and inventory (FACT)
- `tools\gen_target_probes.py` CALLED gains `0x004B33B0` "net message encode" and `0x00462250`
  "net send dispatch". `dll\coop_target_probes.h` regenerated from the reviewed exe (SHA-256
  1A70E5ED, read-only); 15 probes now. The load-time check (`validate_supported_target`) refuses
  activation if these bytes differ, as for the other probes.
- `dll\ADDRESS_INVENTORY.md`: 3 rows added after 0x00462940 (0x004B33B0, 0x00462250, and a second
  row for 0x008AFEA0 as chat time, same address as the session time row). 80 addresses in source.
- `dll\coop_mode.ini`: `KickMessage=1` with a comment after `PlainMP`. `docs\USER_GUIDE.md` section 8:
  one `[Net] KickMessage` row.

### 13.4 Verification (this pass; not a game test)
- Build: `cmd //c "%USERPROFILE%\Desktop\F1\development\mods\coop_mode\dll\build.bat"`, `-Werror`
  clean. Output `%TEMP%\multiplayer_overhaul_build\multiplayer_overhaul.dll`, SHA-256
  `f203bf044553174e866a6edf540a68c598679c6e6474c58bae54d8be1382ad3c`. The notice string is present
  in the binary. Not copied to any install or production path. The build also writes
  `f1c_pid_injector.exe` to `%TEMP%`; it was not run.
- `python -X utf8 -m pytest -q tests`: 16 passed. This includes `test_coop_trust.py` and
  `test_node_claim_gating.py` (11 passed together, unchanged).
- `tests\test_node_policy.py` (C harness, `-Werror`): all checks passed. New cases 9-14: WARN before
  boot with the 3 s delay (boot exactly at 3000 ms, not at 2999); claim cancels; lapse waits a fresh
  grace with no second WARN; race defers and the lobby restarts the wait; dry-run never warns; the
  warned set is per session (two nodes = two WARN); a full warned list sends no WARN.
- `test_target_probes.py`: PASS, 15 probes match the reviewed exe. `test_address_inventory.py`: PASS,
  80 addresses. `test_usage.py`, `test_coop_bridge_abi.py`, `test_coop_menu_logic.py`: PASS.
- Not done: no game run, no exe launched, no deploy, no production edit, no Lightloader change.

### 13.5 Unproven (must be checked before the mod relies on it)
- Vanilla display. Receiver 0x45D890 case 6 shows the line only when `!byte_7CE254 && byte_7D1044`.
  The meaning of byte_7CE254 (race or pause?) and of byte_7D1044 (probably AllowChatInCar) is open.
  The line may not show in a race, or not at all.
- Sender name. Both native senders send `byte0 = 0`; the receiver looks the name up from byte0
  (`DriverDataCopyByID`). The name shown to vanilla peers may be the host's or a wrong one.
- Order. Whether the chat packet arrives before the boot packet. The WARN is 3 s before BOOT, so the
  send is queued first, but delivery is not measured.
- Directed chat (single peer) is NOT used. It is unproven (investigation 3, "INFERRED").
- CO-OP is covered only when `RequireModCoop=1`. CO-OP bots in the node list remain OPEN (spec 12).

### 13.6 Live test (owner, 2 PCs: host with the DLL, one vanilla client; RequireMod=1, KickMessage=1)
- P-K1: host log has `COOP-K02 kick notice broadcast for node N ... send result 0`, then
  `COOP-K01 booted native node N` about 3 s later.
- P-K2: the vanilla client sees the notice line (record the sender name shown), then
  "You have been booted".
- P-K3: a modded client gets no WARN and no boot; the host logs `claims the mod`.
- P-K4: `KickMessage=0`: no chat line; the host still boots after 3 s, with the `KickMessage=0` log line.
- P-K5: start a race during the 3 s window: no boot; host log `race running ... deferred`.
- P-K6: two vanilla clients: count the lines (expected two, one per node). Owner decides whether to
  collapse to one line per session.
- Abort: a modded client is booted (abort A2 of section 7), or the game crashes or stalls at the chat
  send. Stop, set `KickMessage=0`, and keep the log.

### 13.7 Follow-ups (not done here)
- `RequireMod` and `RequireModCoop` are read in `coop_mode.cpp` but are not in `coop_mode.ini`.
  Add them with comments in a later pass (owner decision on defaults for 1.0.0).
- Findings appended to `f1c_ida_decompiled\findings.jsonl`: `send-wrapper-0x462250-result-is-failure-flag-20261013`
  (FACT) and `kick-chat-notice-built-untested-20261013` (OPEN).
- `README.md` line for this spec still says "owner decision pending"; update after the live test.

---

## 14. Owner round 1 follow-up (2026-10-13): CO-OP kick ON by default, bot guard, return-value wording

Source: owner decision after two-PC round 1 (host log: CO-OP dry-run listed only human nodes 2 and 3; A8 PASS, the owner saw the chat notice). Evidence tags as before. Supersedes 13.2 where they differ.

### 14.1 Change (FACT, in code)
- `RequireModCoop` default 0 -> 1 (`dll\coop_mode.cpp`, `GetPrivateProfileIntA("Net","RequireModCoop",1,...)`; static default also 1). CO-OP now removes unclaimed peers for real unless the owner sets 0.
- Gate fix. Before this pass `require_mod_tick` returned at once when `RequireMod=0`, so `RequireModCoop=1` alone did nothing in CO-OP. Now: plain MP runs with `RequireMod=1`; CO-OP runs with `RequireMod=1` or `RequireModCoop=1`; CO-OP dry-run only when `RequireModCoop=0`.
- Real boot path, plain vs CO-OP with `RequireModCoop=1` (FACT, read in code): both call `node_policy_boot_mode(..., dry_run=0)` -> warn stage on -> WARN (chat, 13.1) -> BOOT 3 s later via `node_policy_should_boot(kind,0)` -> native 0x462940. The only difference is the entry gate above. The old "dry-run" log came from the default 0.
- INI: `dll\coop_mode.ini` `[Net]` now lists `RequireMod=1` and `RequireModCoop=1` with comments (the keys were read by the code but missing from the template). `docs\USER_GUIDE.md` has two rows. `README.md` index line updated. `coop_mode_README.txt` has no kick text, unchanged.
- Bot guard (new, FACT in code): `node_boot_block_reason` (`coop_mode.cpp`) and `node_policy_filter_eligible` (`coop_node_policy.h`). Before the policy step, every native list index that is not a human game peer candidate is removed from the list, so it gets no WARN, no BOOT, and no watch timer. A candidate must have all of: (a) an entry name of 1..31 bytes at netcomm+0x3D0+140*i (the native slot sync 0x45FC50 loads only entries with a name); (b) an entry slot byte (entry+64) that is not an owned mod AI slot (`slot_is_owned_ai`); (c) an entry .veh name that is not the file of an owned mod AI car (same comparison as `release_bots_for_joining_players`). Log once per node per session: `COOP-K00 node N not a boot candidate: <reason> (bot guard, spec 14)`.
- Boot log now names the entry and slot: `COOP-K01 booted native node N (entry '<car>.veh', slot S) ...`, so the live log can show what was booted.

### 14.2 Return values (FACT, decompile + disassembly; not run in game)
- `NetworkSendDispatch` 0x57A6A0: `return 10` when the send object (`vtable+88` of `*this`) is NULL; otherwise `return 0` after the send calls (decompile `functions\NetworkSendDispatch_0057a6a0.c`).
- `TextureCreationWrapper` 0x462250 (chat sender): exe disassembly `call 0x57a6a0; test eax,eax; jne 0x4622d5; mov bl,1`. Returned char = 1 only when `NetworkSendDispatch` returned 0. So 1 = no error, 0 = non-zero (for example 10). The earlier wording "1 = dispatch failed" was INVERTED. Section 13.2 ("Returns char 1 only when NetworkSendDispatch failed") is WRONG; corrected here.
- Boot sender 0x462940 (`CollisionEventNetworkBroadcast` name is wrong): disassembly `call 0x57a6a0; mov ebx,eax; neg ebx; sbb bl,bl; inc bl` -> returns 1 only when `NetworkSendDispatch` returned 0. So the "send=1" in the boot log means no error, same as above.
- INFERENCE: a 1 means the packet went to the native send routine without error. It does not by itself prove delivery. The owner's round 1 observation (notice seen on screen) is consistent with delivery.
- Findings appended to `f1c_ida_decompiled\findings.jsonl`: `send-wrapper-0x462250-result-meaning-corrected-20261013` (corrects the 13.7 entry `send-wrapper-0x462250-result-is-failure-flag-20261013`), `boot-sender-0x462940-result-meaning-20261013`, `netcomm-node-list-entry-pairing-20261013`.

### 14.3 Bot-safety evidence (FACT for the pairing, OPEN for bots in the list)
- Pairing (FACT): `DriverDataCopyByID` 0x462460 walks the entry table (netcomm+916+140*k, name at +60, slot byte at +124) and the node list (netcomm+15476+4*k) with one index k. `ShortComputation_13e0` 0x4613E0 walks the node list (count netcomm+15892). `ClearPlayerSlots` 0x460CB0 fills the node list from the registration array netcomm+117 (104 entries). So list index k and entry k describe one native player record. This is the same layout `release_bots_for_joining_players` already uses.
- Before this pass nothing in the code kept a mod AI bot out of the node list: the spec 12 assumption ("native list = human players only") was unproven. The guard above removes any entry that the mod's own AI bookkeeping (`g_ai_slots`, `g_ai_paths`) identifies as a bot, and any entry without a loaded name.
- Residual risk (OPEN): a bot whose slot or car is not in `g_ai_slots` / `g_ai_paths` at that tick (for example released just before, or never registered) would still be a candidate. A human sharing a car with a bot is excluded (safe direction, no boot). Live check: host log `COOP-K00` lines and the `booted native node ... entry '...' slot S` line must show human car names only.
- Empty-name entries are not booted (logged once). If a vanilla joiner stays unnamed in the lobby, the owner should see a `COOP-K00 ... no loaded player entry` line and no boot. Owner decision if this should instead count as human.

### 14.4 Verification (this pass; game may run; no runtime exe)
- Build: `cmd //c "%USERPROFILE%\Desktop\F1\development\mods\coop_mode\dll\build.bat"` exit 0, `-Werror`. Output `%TEMP%\multiplayer_overhaul_build\multiplayer_overhaul.dll`, SHA-256 `406bfd9dc96ba1501041ce2573cbcdd7139b33ed8a6ed9c56a6b74c1cd1d71e6` (re-check with sha256 before packaging). Not copied to any install, production or game path. The build also writes `f1c_pid_injector.exe` to `%TEMP%`; not run.
- Headless: `python -X utf8 tests\test_node_policy.py`: "node policy: all checks passed" (new section 15, bot guard: 5 checks; earlier sections unchanged).
- Suite: `python -X utf8 -m pytest -q tests` (from `development\mods\coop_mode`): 16 passed. Includes `test_target_probes.py` (15 probes), `test_address_inventory.py`, `test_coop_trust.py`, `test_node_claim_gating.py`, `test_node_policy.py`.
- Protected lines: every line containing `g_sync_host_ip`, `COOP-W30`, `coop_trust`, `coop_host_peer_ok` is identical to the before-state (diff check).
- Encoding: edited files UTF-8, no BOM; `coop_mode.cpp`, `coop_node_policy.h`, `coop_mode.ini`, `test_node_policy.cpp` are ASCII-only.
- Not done: no game run, no exe launched, no install, no deploy, no production or Lightloader change, no live test.

### 14.5 Before-states (kept)
- `dll\coop_mode.cpp.BEFORE_20261013_coopkick`, `dll\coop_node_policy.h.BEFORE_20261013_coopkick`, `dll\coop_mode.ini.BEFORE_20261013_coopkick`, `dll\coop_mode_README.txt.BEFORE_20261013_coopkick` (not edited), `README.md.BEFORE_20261013_coopkick`, `docs\USER_GUIDE.md.BEFORE_20261013_coopkick`, `docs\SPEC_REQUIRE_MOD_KICK_2026-10-13.md.BEFORE_20261013_coopkick`, `tests\test_node_policy.cpp.BEFORE_20261013_coopkick`, `tests\test_node_policy.py.BEFORE_20261013_coopkick` (not edited), `f1c_ida_decompiled\findings.jsonl.BEFORE_20261013_coopkick`.
- Source SHA-256 (prefix): `dll\coop_mode.cpp` 351a9e09d675c258; `dll\coop_node_policy.h` c7da731e98ce6fde; `dll\coop_mode.ini` 32e026e96a46bdff; `tests\test_node_policy.cpp` 5b96b85f018ff6cb.

### 14.6 Pending (owner)
- Live round 2, two PCs (host modded, one vanilla, one modded CO-OP client with 1.0.0 `COOP1? <node>`):
  - C1: CO-OP vanilla peer gets the notice (chat line visible? sender name?) and "You have been booted" about 3 s later; host log `COOP-K02 ... send result 1` and `COOP-K01 booted native node N (entry '...', slot S)`.
  - C2: modded CO-OP client not booted; log `node N claims the mod`.
  - C3: host log `COOP-K00` lines contain no AI/bot car names; any boot line shows a human car.
  - C4: `RequireMod=0` with `RequireModCoop=1` boots CO-OP vanilla peers (new gate); plain MP stays untouched.
  - C5: race start during the 3 s window defers the boot (unchanged).
- Clients built before 1.0.0 (no node id) are booted in CO-OP as well (owner accepted in 12).
- Chat sender name and display in vanilla clients still unproven (13.5).
- `MOD_VERSION` unchanged; release decision pending.
- `development\HANDOFF_JOINT_2026-10-13.md` and `docs\HANDOFF_CLAUDE_2026-10-13.md` still describe CO-OP as dry-run; not edited in this pass (owner to decide whether to update).
