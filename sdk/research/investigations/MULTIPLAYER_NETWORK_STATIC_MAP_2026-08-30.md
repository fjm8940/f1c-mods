# F1 Challenge multiplayer/network static map — 2026-08-30

## Scope and evidence discipline

**Objective:** map the legacy F1 Challenge multiplayer stack sufficiently to plan safe two-peer validation and SDK observation work. This is a static-analysis record of the shipped executable corpus; the running game was not clicked, attached, injected, configured, or otherwise changed.

**Evidence tiers:** statements under **Facts** are directly supported by the named raw Hex-Rays functions or their imports. **Inferences** identify interpretations that need runtime confirmation. Function labels remain corpus labels, not reviewed SDK callable contracts. All addresses are target-specific research references and are not authorization to call or patch them.

**Pre-existing work boundary:** `mods` routes as a legacy preserved archive and `mods\\sdk_sandbox` as the supported sandbox. `multiplayer_ai_host` routes as a project; `python tools/f1c_mod.py discover multiplayer_ai_host` reported 266 discoverable functions at `mods\\sdk_sandbox\\multiplayer_ai_host\\generated\\discovery.md`. Its source, generated evidence, and all sandbox material were left untouched; this map does not import its claims.

## System topology

### Facts

- The title uses a custom Winsock-based networking layer, not DirectPlay in the examined corpus. Raw code imports/uses `socket`, `bind`, `listen`, `accept`, `select`, `connect`, `sendto`, `getsockname`, `getpeername`, `setsockopt`, byte-order conversions, and Win32 mutexes. A corpus search for `DirectPlay` found no source match.
- `NetworkSocketInitializationAndListen` (`0x005804A0`) makes two independent 100-port bind scans: one begins at the configured base port and one begins at **base + 50**; it listens on the latter socket with a maximal backlog. `SocketBindPortScan` (`0x0057C7B0`) attempts offsets 0 through 99, so the actual bound ports can be base..base+99 and base+50..base+149 respectively. Its surrounding socket object is 3,064 bytes (`NetworkSocketObjectCreate`, `0x0057A420`; prior static search).
- `NetworkAccept` (`0x00580190`) polls the listening socket with zero-timeout `select`, accepts connections, allocates/reuses a connection-node object under mutex, records peer address, and calls `setsockopt(socket, 6, 1, ...)`. It then services each queued connection through `sub_57CB50`.
- `sub_57BF00` (`0x0057BF00`) implements a UDP textual discovery/control protocol. It replies using `sendto`, parses a remote IP/port, and creates a TCP client connection to that parsed remote endpoint. Before each connect it binds the **local source** socket beginning at configured base+50; on `WSAEADDRINUSE` it retries local offsets through +50 (base+50..base+100 inclusive). This 51-port client source retry is separate from the listener's two 100-port bind scans. The function also maintains a mutex-protected state field with values 1–5.
- The separate browser/master path creates a UDP datagram socket, scans a 100-port local bind range, and connects it to a master endpoint (`sub_561BF0`, `0x00561BF0`). The options table explicitly names `GameSpyQueryPort`; `sub_562AC0` (`0x00562AC0`) sends `\\heartbeat\\<id>\\gamename\\<name>` and optionally `\\statechanged\\<n>` by UDP. These facts show GameSpy/OpenSpy-compatible discovery framing, although service availability was not tested.
- `TickCountComparisonHandleInactivityTimeout` (`0x00561ED0`) compares `GetTickCount() - lastSend`: while a non-zero, non-future last-send value is at most 300,000 ms old, it sends only when its byte counter is non-zero and elapsed time is **strictly greater than 30,000 ms**. If that timestamp validity window fails it sends immediately. Every send records `GetTickCount`; the byte counter cycles after values above 10. The function does not itself prove end-to-end liveness recovery.
- LAN discovery emits the literal eight-byte `status` datagram across a port range (`sub_5630D0`, `0x005630D0`).

### Inferences

- Both transports can reach the generic decode path. `NetworkAccept` services the UDP socket with `sub_57CB50(..., 1)` and each accepted connection with `sub_57CB50(..., 0)`. In that function, the first path uses `recvfrom` and first offers bytes to the textual UDP command parser; the second uses stream `recv` and reassembles bytes in a circular buffer. The event sender's final transport selection is still hidden behind lower vtable calls, so static analysis does not prove that an individual gameplay opcode is TCP-only or UDP-only.
- `setsockopt(..., 6, 1, ...)` is likely `IPPROTO_TCP`/`TCP_NODELAY`, but the raw call contains numeric constants rather than symbolic imports.
- The master endpoint name and `GameSpyQueryPort` option make the browser GameSpy/OpenSpy-derived with high confidence. This does not establish that the legacy public service is still reachable.

## UI, browser, host, and join state machines

### Facts

- `GameMode` (`0x0046D6D0`) routes global mode 5 into multiplayer: after `SessionStateCheck(...) == 3`, it opens `mpclientgrid`, sets up the network entrant, writes session configuration, optionally loads season/track scene data, and sets `dword_5CC9E8 = 1`.
- `sub_46FC30` (`0x0046FC30`) returns to `mphostgrid` or `mpclientgrid` under multiplayer mode depending on `StateCheckWrapper`; other modes route to `mainmenu` or championship post-race UI.
- Host creation is entered when byte `+0xC5` is set in `sub_463BC0` (`0x00463BC0`). It selects multiplayer mode, initiates the network subsystem, then opens either `selectcircuitprompt` or `multiplayercreate`.
- `sub_45D2D0` (`0x0045D2D0`) is the game-list/browser UI callback. Its observed status codes are: **0 done**, **1 getting game list**, **2 searching LAN**, and **3 querying games**. It updates the displayed message and sets a UI refresh flag.
- `NetCommParseCommandLine` (`0x00463160`) supports `password`, `dedicated`, `host`, `gamename`, `connect`, `name`, and `maxplayers` switches. `connect` parses an address plus optional port; absent port defaults to global `word_7D1004`. `maxplayers` is clamped to 2–104.
- `SendJoinRequest` (`0x00462C00`) selects the browser entry by index, formats its IP/port, clears a session field at `+888`, and passes the address and supplied password into the network request path.
- `MultiplayerPasswordValidation` (`0x0046DD30`) obtains UI element 34, reads entered text, tests session password metadata, conditionally opens `multiclientpassword`, and otherwise calls `SendJoinRequest`. A successful request sets a join countdown field to 4.0.
- `MultiplayerSessionJoinHandler` (`0x0046DE10`) handles the completed join: it checks session state 3, writes the received configuration, transitions every vehicle slot, sets active vehicle count to zero, rebuilds the active vehicle pointer array, sets multiplayer flag `dword_7C56A0 = 2`, and opens `clientteamselect`. A validation/timer failure routes to `joinfailurepage` and returns -1.
- `AutoJoinRetryLoop` (`0x004637A0`) is an explicit retry state machine for the parsed connect target. It repeatedly services UI/game work, polls session state, updates a remaining-time display from `flt_7D0F28`, retries a connect operation every ten seconds, and either performs the same client setup or displays auto-join failure.
- `SessionStateCheck` (`0x00461CD0`) normalizes state 4 to state 3 when the network delegate reports 3. The semantic labels for all state values are not yet reviewed.

### Inferences

- The `StateCheckWrapper` branch choosing `mphostgrid`, the send-only guards described below, and slot administration point to state values 1/2 as host/authority paths and state 3 as the client path. This role mapping needs a host/client trace before use in code.
- Password entry appears to be admission gating, not cryptographic protection: a UI string is passed to the join request path and no password hashing/encryption was observed in these functions. Do not make a security-strength claim without a packet capture and the lower transport constructors.

## Packet construction, receive dispatch, and message families

### Facts

- `ProgressBarEventEncode` (`0x004B33B0`) is a compact common message-header builder. In its raw formal names, it writes `header = a5 + (a4 << 8) + (a3 << 18) + ((dword_7C56A4 + 8*(a7 + 4*a2)) << 24)` (32-bit arithmetic), then writes `a6` at DWORD 1 and -1 at DWORD 107. Therefore `((header >> 18) & 0x3F) == (a3 & 0x3F)` and `((header >> 8) & 0x3FF) == (a4 & 0x3FF)`. `NetCommSessionMessageDispatcher` (`0x0045D890`) accepts headers with top bits `0x80000000` and dispatches `((header >> 18) & 0x3F)`.
- `TextureCreationWrapper` (`0x00462250`) submits exactly `((header >> 8) & 0x3FF) + 8` bytes to `NetworkSendDispatch`. Thus the builder's `a4` is a 10-bit extra-byte count after the eight-byte common prefix, rather than an unknown selector. A raw example is collision: `CollisionEventNetworkBroadcast` builds opcode 32 with `a4 = 4`, stores the collision value at DWORD 2, and submits 12 bytes.
- The dispatcher has direct, raw-evidenced cases: 0/1 (no-op); 2 (next-session configuration, only when state is 3); 3 (associate a remote node with a local slot and return configuration); 4 (copy server-supplied per-slot values and mark ready); 5 (next-track transition); 6 (formatted player text/chat); 0x20 (boot/kick notice); 0x23 (two-byte event/state input); 0x24 (content/config mismatch notification and response); 0x27 (string/list operation); 0x28 and 0x29 (set local state values 2/3); and 0x2A (terrain/position error correction with smoothing). Other packets fall through a separate ring-buffer filter.
- `TeamRadioNetworkMessageProcessing` (`0x0045F710`) pulls 432-byte frames from the network delegate, sets a sender/node ID on authority paths, runs collision/contact handling, and calls the dispatcher. On authority paths it also drains a buffered frame ring and relays frames to each connected player through `TextureCreationWrapper` (`0x00462250`) / `NetworkSendDispatch` (`0x0057A6A0`).
- `NetworkSendDispatch` obtains a sender object, attaches payload/target metadata, submits the requested byte length, and selects one of two lower vtable dispatch paths. Failure to obtain the sender returns 10.
- `NetworkReceivePacket` (`0x0057E940`) searches a mutex-protected received-event list by network address and returns the node field at `+0x14`. `NetworkPacketSend_ead0` (`0x0057EAD0`) accumulates sends per address in another mutex-protected list; it tracks most recent value, sum, and count.
- Confirmed outbound event constructors include: session/UI status opcode 2 (`SessionUIElementRenderer`, `0x0045F2B0`); server-ready opcode 3 (`0x00460470`); track-open opcode 38 (`0x00460E90`); track-close opcode 37 (`0x00461020`); and collision opcode 32 (`0x00462940`). The broader event-builder call graph includes pit, penalty, split/lap, DQ, engine-failure, respawn, position/telemetry, weather, audio, and input-command functions.
- The lower socket receive path uses a second, distinct frame wrapper. `sub_57CB50` (`0x0057CB50`) accepts/reassembles a record only when `received_bytes == word0 + word1 + 4` and total length is at most `0x604` (1540) bytes. TCP records are buffered until this condition holds; UDP records are evaluated directly. `BufferUnpackWithSizeCheckByteWordCopy` (`0x0057D660`) consumes the two leading 16-bit values and copies/reallocates the two following variable-sized spans before virtual dispatch. The raw code does not label either 16-bit value as a sequence number, acknowledgement, or checksum.
- The lower connection-control dispatcher (`0x00584230`) has a Ping Request branch (selector 11) that emits selector 12 with a four-byte timer value; selector 12 divides the current timer reading by two and passes it to a callback. This is direct evidence of ping/latency measurement, not of packet acknowledgement or retransmission.

### Inferences and limits

- The primary 6-bit opcode and submitted byte count are now statically resolved as above. A capture is still required to identify the top-byte session/channel field, field byte order on the wire, target/recipient semantics, lower wrapper contents, and the actual transport selected by each outgoing gameplay event.
- The broad call graph makes timing/rules/results synchronization plausible, but only collision and open/close behavior are directly tied to an immediate `NetworkSendDispatch` in this review. The lap, penalty, DQ, failure, position, weather, and input candidates must be confirmed by tracing their individual output paths and two-peer traffic.
- The `0x24` mismatch path checks several track/content-related values and displays a player-specific mismatch message. It is a compatibility check, not proof of complete anti-cheat or content integrity enforcement.

## Authority, replication, timing, rules, and results

### Facts

- `SynchronizeWithServer` (`0x00464290`) clears/reinitializes player slots on synchronization, starts collision-detection synchronization when session state is 3, resets timers, adjusts a transport byte depending on global state, and emits a state-changed heartbeat when configured.
- `SessionScoringUpdate` (`0x004643C0`), called by `GamePostMainLoop`, is the recurring orchestration point: it clears/synchronizes slots, updates client-grid and collision work, runs track-opening timing, sends server-ready, conditionally sends track-open, drains user-visible message queues, and processes input.
- `NetCommPlayerSlotSync` (`0x0045FC50`) reconciles up to 104 remote player slots. For present slots it creates/updates vehicle state, team/driver strings, vehicle scene/render state, active vehicle array, physics/player selection, and “entered game” text. For absent slots it displays “left the game,” removes associated connection/list entries on the authority paths, tears down the vehicle slot, clears player slots, reduces vehicle count, and rebuilds active pointers.
- `DriverDataCopyByID` (`0x00462460`) copies a 0x8C-byte remote driver/entrant record by node ID. `MultiplayerQualificationGridAssignment` (`0x00462E20`) maps server node IDs into qualifying positions and has a fallback placing a missing local node last.
- `SessionSynchronizer` (`0x0045F950`) applies server clock correction. A received time value is compared to `TimeOffsetGetter`; above 0.1 seconds it reports synchronization, above 0.5 seconds it invokes a larger correction, and otherwise applies an adjustment function.
- `ServerOpenTrackSignal` and `ServerCloseTrackSignal` build/send explicit network events, set the track-open flag at `+0x29`, and queue UI messages. `TrackOpeningProgressTimer` (`0x004612E0`) decrements the opening timer with a 0.01–0.5 frame-time clamp and marks the track open on expiry.
- `CollisionEventNetworkBroadcast` sends only when session field `+0x3FC4` is 1 or 2. It builds an event containing the collision argument, then uses the common send path.
- `MultiplayerVehiclePlacement` (`0x0042CF10`) copies per-vehicle state across multiplayer transition states. In state 7 it applies championship standings reorder or multiplayer grid assignment and then refreshes each active vehicle.

### Inferences

- The recurring slot reconciliation, sender-ID labeling, and relay loop are strong evidence of host-authoritative session membership and event redistribution. They do not yet establish host authority for every physics/rule outcome.
- Vehicle replication is state/event-frame based rather than a fully identified deterministic-lockstep implementation. The 432-byte frames and terrain correction are evidence of substantial synchronized state, but field semantics, interpolation, prediction, send rate, and ownership split remain unproven.
- Multiplayer qualification-grid and track state are synchronized. Whether full race-result ordering, championship progression, penalties, and DNF adjudication are host-authoritative must be tested across a race finish; static callers alone do not settle it.

## Reliability, disconnects, rejoin, migration, and trust boundaries

### Facts

- Connection establishment retries TCP binds/connects across an offset range and sleeps 10 ms between port-collision retries (`0x0057BF00`). Auto-join has a separate configurable timeout/retry loop (`0x004637A0`).
- Receive/send queues and connection lists use `WaitForSingleObject`/`ReleaseMutex`; `NetworkAccept` services current connections and removes/cleans temporary node lists. This is concurrency control, not a reliability protocol proof.
- A client boot packet exists (dispatcher case 0x20): self-targeted handling displays “You have been booted,” triggers a network delegate operation, then returns UI flow; other targets display a named removal notice.
- Player departure is explicitly handled by `NetCommPlayerSlotSync`: absent entries unload the vehicle/slot, purge related connection list records on the 1/2 path, decrement active vehicle count, and rebuild active pointers.
- Static source searches for `disconnect`, `rejoin`, and `migration` found no matching source terms. No host-migration mechanism was identified in the reviewed functions.
- `NetworkReplayBufferInit` (`0x0045D260`) XORs six embedded bytes with 0x3E into globals before creating the game-list callback. This is obfuscation/key material for that object, not demonstrated encryption or authentication of multiplayer traffic.

### Inferences and security posture

- No packet sequence/ack/retransmit contract, authenticated identity proof, authoritative anti-cheat validation, or host migration was demonstrated. The reviewed framing/dispatcher code has no raw-labelled sequence or acknowledgement field; a raw source search found no `retrans` match and `sequence` matched only non-network physics/init text. Lower virtual payload objects remain unreviewed, so all of these properties remain **unknown**, not absent.
- The exposed text-based discovery/control parser and pre-TLS-era Winsock architecture are security-sensitive. Until a controlled capture shows validation and trust boundaries, do not expose a hosted session to an untrusted network and do not build a mod that assumes peer input is trustworthy.
- Rejoin may work as a fresh session-browser/join flow because slot allocation is reusable, but resuming the same race identity/state is unproven.

## Configuration and UI coupling

### Facts

`PlayerOptionsInitializer` (`0x004AA270`) creates a `Multiplayer Options` group with these direct defaults/ranges:

| Option | Static default / range | Coupling observed |
|---|---:|---|
| Network connection type | 4; 0–4 | connection profile selector |
| Multiplayer | 1; 0–1 | `0=LAN`, `1=Internet` text |
| Net data rate / maximum | 32; 6–32 | client setting / host-defined maximum |
| Net flush threshold | 1024; 0–1024 | automatic flush threshold |
| Net lazy flush | 0; 0–1 | explicit-flush toggle |
| Net join timeout | 5.0; 0.5–1000.0 | used by auto-join through `flt_7D0F28` (field mapping needs validation) |
| Allowed AI | -1 | session policy field |
| Force physics | 24; 0–31 | physics flags/policy field |
| Announce host / game-list auto-refresh | 1 / 1 | browser/master behavior candidates |
| Maximum MP players | 16; 2–104 | command line independently clamps to 2–104 |
| GameSpy query port | 34297; 1025–65535 | discovery/master UI option |
| Port start | 34397; 1025–65535 | socket base-port option |
| Game-list sort | 1; 0–3 | browser UI |
| Chat in car | 1 | dispatcher case 6/UI behavior candidate |
| Closed MP qualifying | 0 | qualifying policy candidate |
| Throttling | 1 | transport policy candidate |
| Collision threshold | 0.3125; 0–64.0 | latency/collision policy candidate |
| Dedicated target rate | 128; 32–1024 | dedicated-host rate candidate |

The option labels/defaults are facts; mapping every option to a specific transport field or runtime behavior remains an inference unless named above.

## Required two-peer validation (game is currently running)

No live action was taken in this task. A human-controlled second peer is required for the following guarded, read-only observation plan:

1. Start one host and one client through full race-weekend flow (never QuickRace); record selected LAN/Internet mode, base/query ports, host/client grid, and browser state transitions.
2. Capture loopback/LAN traffic during browse, join with/without password, player-ready, open/close track, qualifying grid, chat, collision, pit/penalty, DNF, finish/results, host kick, client exit, and host exit.
3. Correlate the captured headers/lengths with `0x004B33B0` and dispatcher cases; validate actual TCP/UDP division, sequence/ack/retry behavior, packet ownership, and the 432-byte frame cadence.
4. Repeat with a late join and reconnect attempt to decide fresh-join versus race-state rejoin. Test host loss only on disposable copies and a private network; specifically check whether any migration or deterministic failure path exists.
5. Record whether browser/master service responds. Do not infer Internet play from a successful LAN path.

## Limitations and follow-up

- Decompiled labels and most ABI records are `LEGACY`/`unreviewed`; code was not runtime-validated.
- Indirect vtable calls hide portions of lower transport and message serialization. A static map cannot prove packet direction, reliability, or trust decisions.
- No production mod, executable, game configuration, or SDK sandbox was modified. This report is the only artifact changed by the multiplayer subtask; the coordinated static-analysis task also updates the championship, OSC, input/FFB, and documentation-index records.
- No product-stage change is supported, so `governance\\PRODUCT_STATUS.json` was intentionally not updated.

## Evidence index

Primary raw corpus functions inspected through `python tools/f1c_mod.py inspect`:

- Session/UI: `0x0045D2D0`, `0x0045F2B0`, `0x00460470`, `0x00460520`, `0x00460CB0`, `0x00460E90`, `0x00461020`, `0x00461190`, `0x004612E0`, `0x00461CD0`, `0x00462C00`, `0x00463160`, `0x004637A0`, `0x00464290`, `0x004643C0`, `0x0046D6D0`, `0x0046DD30`, `0x0046DE10`, `0x0046FC30`.
- Replication/dispatch: `0x0042CF10`, `0x0045D890`, `0x0045F390`, `0x0045F710`, `0x0045F950`, `0x0045FC50`, `0x00462250`, `0x00462460`, `0x00462550`, `0x00462940`, `0x00462E20`, `0x00462848`, `0x004B33B0`.
- Transport/discovery: `0x00561BF0`, `0x00561ED0`, `0x00561F60`, `0x00562AC0`, `0x005630D0`, `0x0057A6A0`, `0x0057BF00`, `0x0057C7B0`, `0x0057C860`, `0x0057CB50`, `0x0057D660`, `0x0057DC10`, `0x0057E940`, `0x0057EAD0`, `0x0057F2B0`, `0x00580190`, `0x005804A0`, `0x00584130`, `0x00584230`, `0x00585270`, `0x00585380`, `0x00585400`, `0x00585870`, `0x00586800`, `0x00586A00`, `0x00587730`.
- Configuration: `0x004AA270` and its raw `Multiplayer Options` initialization at lines 1246–1267.

## Addendum — packet constructors, transport lifecycle, and trust boundary (raw-code pass)

### Verified facts

#### Packet header and submission boundary

- `0x004B33B0` (`ProgressBarEventEncode`) writes exactly three fields into the 432-byte event buffer: `dword[0] = a5 + ((a4 + ((a3 + ((dword_7C56A4 + 8*(a7 + 4*a2)) << 6)) << 10)) << 8)`, `dword[1] = a6`, and `dword[107] = -1`.  In 32-bit arithmetic, bits 8..17 carry `a4`, bits 18..23 carry `a3`, and the high-byte contribution is derived from global session type plus `a2/a7`; `a5` occupies the low 8 bits.  This is the raw-supported layout, not a claim of byte order on the wire.
- `0x00462250` (`TextureCreationWrapper`) submits exactly `((header >> 8) & 0x3ff) + 8` bytes to `0x0057A6A0` (`NetworkSendDispatch`).  Thus a constructor length `a4` describes payload bytes after the two-dword prefix.  The wrapper maps its `a3` values `1 -> 1`, `2 -> 2`, all other values -> 0 before passing them as a send flag; their full transport semantics remain vtable-defined.
- `0x0057A6A0` creates a lower message object, sets a target if supplied (or byte `+4 = 1` when absent), calls `sub_586800(1, 0, a5, a6, a7)`, copies the requested bytes, then chooses `VTableDispatchWithConditionalCall` when `a8` is true or `0x00585270` otherwise.  `0x00585270` either invokes another vtable dispatch or expands a broadcast to every enumerated peer; a targeted message iterates selected targets.  The raw code does **not** expose which physical socket (TCP versus UDP) a given game opcode ultimately uses.

#### Confirmed game event constructors (opcode is `(header >> 18) & 63`)

| Opcode | Constructor evidence | Raw-observable payload after 8-byte prefix |
|---:|---|---|
| 1 | `0x004A5C80` | 1 byte: pit-machine state `a2`; class argument 6. |
| 2 | `0x004EF2E0` | 6 or 18 bytes depending on two copied fields; source is a 9-word legacy-recovery-named state/event record helper; class 2.  Its physical force-feedback meaning is unproved by this static evidence.  Other call sites also construct variable-length opcode 2. |
| 4 | `0x0045D890`, `0x004FCEE0` | 420-byte join/ready response with slot at dword 2 and 104 copied dwords beginning at dword 3; a separate 12-byte form copies three dwords. |
| 6 | `0x00470490`, `0x004FCEE0` | chat/UI sender limits text to 159 bytes; dispatcher terminates at payload end before formatting/display.  The second source builds a one-byte form. |
| 7 | `0x004D1D80`, `0x00465460` | standings-style form is `12 + dword_5C329C` bytes: three floats at payload bytes 0..11 and one byte per player beginning at byte 12; another source emits 19 bytes. |
| 8 | `0x004BC390`, `0x004E0800` | 8-byte lap-split form: float at bytes 0..3 and two 16-bit values at bytes 4..7; another constructor emits 4 bytes. |
| 10 / 11 | `0x004D1730` | zero-payload control headers; opcode 10 precedes a packed sequence of 16-bit values written into the current buffer and header bits are updated with the count; a second opcode-11 header starts each subsequent group of 16.  The precise receiver/parser is not yet mapped. |
| 23 / 24 / 28 | `0x004BBD60`, `0x004BBF20`, `0x004BC0E0` | respectively one-byte stop/go value, no extra payload, and no extra payload; all originate in penalty/qualification flow. |
| 26 / 27 | `0x004BC060`, `0x004BEC55` | opcode 26 has one byte (vehicle-audio/failure event); opcode 27 has no payload. |
| 29 | `0x0040F600` | class 1, six constructor variants selected by local case 0..5; each calls with opcode 29 and a per-vehicle byte as low header field, then populates the event buffer with quantized pose/orientation values.  Full tail layout was not reconstructed in this pass. |
| 32 | `0x00462940` | 4-byte collision event; total wrapper submission is 12 bytes. |
| 33 / 34 | `0x004D1D80` | zero payload: a proximity/gap state transition. |
| 35 | `0x0045D890` | two bytes copied from session offsets `112468/112469`, targeted to the joiner after the opcode-4 response. |
| 36 | `0x004607A0` | 20-byte header established before checksum/content preparation; downstream `TextureCreationWrapper` is invoked with a separate local buffer, so the final transmitted layout cannot be asserted from this constructor alone. |
| 37 / 38 | `0x00461020`, `0x00460E90` | zero-payload close/open track signals. |
| 42 | `0x0045F950` | two-byte signed/quantized terrain-height correction, emitted by game state values 1 or 2 every 5 seconds and targeted using the per-slot value. |

This table is a constructor inventory, not a claim that every opcode is accepted by `0x0045D890`: that dispatcher only visibly branches on 0, 1, 2, 3, 4, 5, 6, 32, 35, 36, 39, 40, 41, and 42 before its default path.  In that same receiver, 39 is a bounded-string queue/search action which emits targeted 40/41 controls, 40 sets session field 222 to 2, 41 sets it to 3, and 42 applies a quantized terrain-height error/smoothing calculation across active slots.  Several opcode handlers may reside behind the post-dispatch chain or vtables.

#### TCP/UDP ownership, framing, and cleanup

- `0x0057FC80` creates `socket(AF_INET, SOCK_DGRAM, 0)` and `socket(AF_INET, SOCK_STREAM, 0)`, stores them at object dword offsets 591 and 590 respectively, initializes both, and returns failure code 6 unless both succeed.  It also calls `0x00585470` before returning, which is a lifecycle boundary whose internal shutdown semantics were not inspected here.
- `0x005804A0` calls `0x0057C7B0` on UDP at configured base port and TCP at base+50, then calls `listen()` on TCP only.  `0x0057C7B0` attempts offsets 0 through 99, returns immediately on `WSAEINVAL` (10022), and otherwise continues after failed binds until 100 attempts.  Therefore the two scans are independently `base..base+99` and `base+50..base+149`; overlap is intentional/possible across socket types.
- `0x00580190` accepts ready TCP listener connections, configures the accepted socket with `setsockopt(socket, 6, 1, &one, 4)`, creates/reuses a connection node, and then calls `0x0057CB50` on the UDP socket with mode 1 and every active TCP socket with mode 0.  The raw call parameters prove the option tuple but, without importing a platform constant table into the evidence set, this report does not label it `TCP_NODELAY` as fact.
- In UDP mode, `0x0057CB50` uses `recvfrom`, first gives the datagram to `0x0057BF00` (text discovery/connection protocol), then can decode it through the generic message path.  In TCP mode it uses `recv`, accumulates bytes in a 4096-byte circular buffer, reads two leading 16-bit words, and only releases a record when its accumulated byte count is exactly `word0 + word1 + 4`.  It rejects a received record over `0x604` (1540) bytes.  `0x0057D660` consumes the leading words as framing lengths; they are **not** identified as sequence, acknowledgement, checksum, or retry fields by raw code.
- TCP socket errors flow into error logging/handling in `0x0057CB50`; the function stops processing the current socket path on no data, peer close, or error.  `0x00580190` frees only its temporary per-poll list.  This pass did not prove an automatic reconnect, session rejoin, or host migration path.

#### Text discovery/connection protocol and master service

- `0x0057BF00` recognizes a textual UDP prefix, replies to compatible discovery/status probes with a string that includes the TCP listener port and sender endpoint data, and parses a request containing `" %d %d.%d.%d.%d:%d"` into a remote TCP endpoint.  It creates a TCP socket, locally binds with `0x0057C7B0(base + retry + 50)`, and calls `connect()` to that parsed endpoint.  It retries **only** `WSAEADDRINUSE` (10048), incrementing while `retry <= 50`; that creates at most 51 local candidates (`base+50` through `base+100`, inclusive).  This is separate from the listener's two 100-port scans.
- After a successful connect, `0x0057BF00` sends lower control constructions using `sub_586800` selectors 6 and 3, one containing a 16-byte address and another containing a 17-byte string derived from `ArithmeticComparison_5020`, then installs the connection node.  The strings/selector semantics are not fully decoded.
- `0x00561BF0` creates a separate UDP socket (`AF_INET`, `SOCK_DGRAM`, protocol 17), binds in a supplied base..base+99 range, and passes it to `0x00561FF0` with literal master hostname `aMasterOpenspyN` and port 27900.  `0x00562AC0` sends `\\heartbeat\\<id>\\gamename\\<name>` and optional `\\statechanged\\<n>` by UDP.  `0x005630D0` broadcasts literal `status` (8 bytes) across an inclusive port range.  This proves a plaintext UDP master/discovery design; it does not prove the current hostname's availability or a public service contract.

#### Authority, replication, results, and trust

- `0x0045F710` receives 432-byte frames from the session source.  For state values 1 or 2 it rewrites the low byte of the header to a slot found from the sender node, calls the collision solver, dispatches the event, and then relays each buffered frame to every other eligible player except the origin.  This is direct evidence of relay/authority behavior for state values 1 and 2; labels such as "host" and "dedicated host" remain inferences.
- `0x0045D890` accepts its game-event family only when `(header & 0xE0000000) == 0x80000000`.  Opcode 2 is acted on only when state is 3; opcode 3 builds the 104-entry opcode-4 response and a two-byte opcode-35 follow-up, targeted to the node stored in dword 107.  Opcode 4 installs those 104 values and calls `NetComm_SetSelfAsReady`.  These are concrete join/ready synchronization effects.
- Rule/result producers exist in ordinary simulation code: lap split (opcode 8), proximity/gap notices (7/33/34), stop/go (23), pit progress (24), DQ (28), vehicle failure/audio (26/27), and terrain correction (42).  Their direct constructors do not themselves prove that a remote peer cannot forge the same events.  The generic dispatcher performs header-family and certain state checks, but the inspected paths contain no cryptographic authentication, encryption, signatures, or authorization token validation.
- Lower selector 3 compares a locally configured string against a received/copied bounded string and sends selector 4 or error selector 5; this is evidence of a plaintext password-like gate, not evidence of secure password storage or resistance to replay/packet injection.  Discovery/status replies and heartbeat are likewise plaintext.  Treat all multiplayer inputs as untrusted until a live two-peer capture and full vtable tracing establish stronger validation.

#### Reliability / liveness stopping point

- `0x00584230` lower selectors 11 and 12 construct and consume a 4-byte timing value; selector 11 sends selector 12 and selector 12 stores half an elapsed time as a peer timing measure.  Selectors 1..15 also include node admission/removal, data relay (8), address/copy operations, and a 268-byte record cache (15).  Those operations show connection control and ping measurement, but no inspected branch sends an acknowledgement for arbitrary payload or retains a payload for retransmission.
- Raw-corpus searches for the literal stems `retrans` and `sequence` returned no source matches.  This is negative naming evidence only: lower virtual methods, unnamed fields, and binary library behavior remain unreviewed.  It does **not** establish that reliability, ordering, or duplicate suppression is absent.
- `0x00561ED0` calls `GetTickCount`; when the prior send timestamp is valid/nonfuture and no more than 300,000 ms old, it sends a heartbeat only when its byte counter is nonzero and elapsed time is strictly greater than 30,000 ms.  Otherwise it sends immediately.  It increments the counter and resets it after it becomes greater than 10.  This is heartbeat scheduling, not a proven peer-dead or reconnect policy.

### Coordinated-task scope correction

- This multiplayer report is the sole artifact edited by this subtask, but it is **not** the only file changed by the coordinated four-report task.  Any earlier wording that implied otherwise is superseded by this clarification.

### Inferences (kept separate)

- State values 1 and 2 are likely hosting/authoritative variants because they normalize sender identity and relay frames, while state 3 consumes the session configuration opcode.  The actual UI labels and exact distinction between 1 and 2 require a two-peer observation or additional state-initialization trace.
- The two leading TCP framing words are likely size fields because the receiver uses their sum plus four to decide record completeness.  The exact split of metadata versus payload is unresolved; they should not be called `sequence`/`ack` fields.
- The master hostname literal and heartbeat/status grammar closely resemble GameSpy/OpenSpy-era protocol families.  That association is contextual only and is not required for the raw behavior stated above.

### Open questions and required second-peer evidence

- Capture at least one host and one joiner while recording both TCP and UDP traffic to map each game opcode to physical transport, lower selector framing, any sequence/ack/retry behavior, and actual wire byte order.
- Exercise voluntary leave, TCP reset, UDP loss, timeout, host exit, duplicate join, wrong password, malformed discovery reply, and rejoin after process restart.  Record whether slot ownership, rules/results, and standings recover or are discarded.
- Trace the unreviewed vtables reached by `sub_586800`, `NetworkSendDispatch`, and the generic receive decoder; static work above establishes their call boundaries but not their full serialization/security policy.
- Validate option consumers by changing configuration only in a controlled copy and comparing generated session/setup packets; this static pass establishes option declaration and UI/command-line entrypoints, not an authoritative per-option wire mapping.

### Reproducible static verification commands

All were run from `%USERPROFILE%\\Desktop\\F1\\f1c_ida_decompiled` using the project-required tool only:

```powershell
python tools/f1c_mod.py inspect 0x004B33B0 --corpus raw --max-lines 200
python tools/f1c_mod.py inspect 0x00462250 --corpus raw --max-lines 180
python tools/f1c_mod.py inspect 0x0057A6A0 --corpus raw --max-lines 260
python tools/f1c_mod.py inspect 0x00585270 --corpus raw --max-lines 250
python tools/f1c_mod.py inspect 0x0057FC80 --corpus raw --max-lines 320
python tools/f1c_mod.py inspect 0x005804A0 --corpus raw --max-lines 260
python tools/f1c_mod.py inspect 0x0057C7B0 --corpus raw --max-lines 180
python tools/f1c_mod.py inspect 0x00580190 --corpus raw --max-lines 300
python tools/f1c_mod.py inspect 0x0057CB50 --corpus raw --max-lines 420
python tools/f1c_mod.py inspect 0x0057BF00 --corpus raw --max-lines 360
python tools/f1c_mod.py inspect 0x00584230 --corpus raw --max-lines 380
python tools/f1c_mod.py inspect 0x00585870 --corpus raw --max-lines 280
python tools/f1c_mod.py inspect 0x00561BF0 --corpus raw --max-lines 260
python tools/f1c_mod.py inspect 0x00562AC0 --corpus raw --max-lines 210
python tools/f1c_mod.py inspect 0x005630D0 --corpus raw --max-lines 180
python tools/f1c_mod.py inspect 0x00561ED0 --corpus raw --max-lines 180
python tools/f1c_mod.py inspect 0x0045D890 --corpus raw --max-lines 440
python tools/f1c_mod.py inspect 0x0045F710 --corpus raw --max-lines 280
python tools/f1c_mod.py inspect 0x0045F950 --corpus raw --max-lines 260
python tools/f1c_mod.py search "retrans" --corpus raw --case-sensitive --limit 100
python tools/f1c_mod.py search "sequence" --corpus raw --case-sensitive --limit 100
```

## Wasted-effort check

- Repeated large-material reading: none material; narrow `inspect` calls were used after an initial broad search.
- Unnecessarily broad builds/tests: none; no build, SDK test, or live action was in scope for static mapping.
- Failed/retried commands: two broad tool-output calls were truncated; subsequent queries used raw-corpus and per-function inspection.
- Polling/waiting/idle: none.
