2026-09-23 P-key correction: 0x7CC4F4 is read by the native pause-toggle branch in sub_4FA7E0. The broker now consumes it while the ESC menu owns pause; the former audio-gate hold was removed. Offline gates pass; gameplay and sound remain pending. See development/mods/esc_pause_menu/verification/p-key-2026-09-23/REPORT.md.

2026-09-23: [Leave Circuit icon native results repair](UI_LEAVE_RESULTS_20260923.md). Original OSC restored; physical-X remap withdrawn; native-grid countdown trial installed, gameplay pending.

# Experimental game-update hook broker

## 2026-09-21 optional RF penalty hold

The broker_penalty_* headers add broker-owned native issue/service/consumption/lap hooks. Capability bit13 and Begin/Read/Seal/End are declared in f1c_broker_penalty_hold.h. Whole-field capture, current identities and one explicitly sealed constructor boundary preserve the event ledger. Original native lap timing and results still run. Partial optional installation stays pass-through; every installed hook detaches and native calls drain before freeing trampolines.

Review-only build: build/penalty_hold_20260921_r2/. Full SDK gate r4 passes; native acceptance pending. [Evidence, limits and restoration](../../../../development/mods/red_flag/trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md).


This sandbox DLL is the proposed ownership boundary for `0x0042C690`. Exactly
one broker patches the internal update entry. Mods register with the broker and
post bounded tasks; they never detour the function themselves.

## Contract

- Fixed ABI version and exact undecorated C exports.
- Sixteen client slots. Tokens contain a slot and monotonically changing
  generation so stale registrations fail closed.
- A global FIFO of 256 tasks, limited to 32 queued tasks per client.
- At most 32 tasks execute during one game-update call.
- Draining also stops after 2 ms of aggregate callback time. A single callback
  cannot be preempted. Ordinary callbacks above5ms are warned (once/sec/client).
  Ordinary callbacks over20ms for10 consecutive samples, or any over100ms,
  invoke recovery and quarantine/purge after return. <=20ms resets the streak.
  Native allowance remains250ms and cannot reset ordinary streaks.
- Callbacks execute outside the broker lock.
- Unregister invalidates the token and removes its queued callbacks before it
  waits for an already-running callback. A slot cannot be reused while retiring.
- Unregister from that client's own game-thread callback fails with
  `F1C_BROKER_WOULD_DEADLOCK`.
- Broker unload is refused while clients, queued tasks, callbacks, or API calls
  remain active.
- The broker accepts any 32-bit EXE whose fixed image layout, independent code
  fingerprint and exact bytes at both hook sites match the reviewed layout.
  EXE filename, timestamp, checksum, entry metadata, version and whole-file
  hash are not compatibility gates. Any layout, fingerprint or hook-byte
  mismatch installs nothing.

This does not make callbacks semantically safe. Each state-changing task still
needs an independently reviewed command contract. Native callback faults are
not contained in this prototype; production policy should prefer broker-owned
semantic commands over arbitrary mod callbacks.

## Packaged discovery candidate

`f1c_broker_discovery_experimental.*` is the Phase 2 sandbox candidate. It
enumerates modules already loaded in the current process and identifies the
broker by its undecorated query export, so clients do not guess an installation
path or DLL load directory. It negotiates the intersection of the client and
broker ABI ranges, requires an explicit capability subset, binds exports only
after negotiation, and refuses ambiguous duplicate broker-shaped modules.

A successful lookup pins the selected module. The client must stop all calls
and then call `f1c_broker_release` exactly once. Failed lookup leaves no callable
function pointers. The isolated compatibility matrix covers missing, old,
current, compatible-new, incompatible-new, missing-capability, and duplicate
brokers. Discovery takes the module reference before invoking the query export,
so enumeration cannot race an unpinned broker unload. Negotiated ABI 5 masks
ABI 6 snapshot/event capabilities and pointers, and the compatible-new fixture
returns a larger info-record size through the stable ABI 6 prefix. This remains
an unpromoted sandbox interface until live startup/unload integration proves the
packaging lifecycle.

## First semantic command

ABI 3 adds `F1CBrokerSetCallToBox(token, slot, enabled)`. The broker copies the
slot and boolean into its queue—no client pointer survives the API call. On the
game thread it requires active race phase 5, a live AI vehicle (`+0x32C8 == 1`),
and then invokes the reviewed `SetCallToBox` leaf at `0x0042AC70`. Invalid phase
or vehicle state is counted and rejected without calling the game function.

ABI 4 adds `F1CBrokerPulseCallToBox(token, slot, enabled)`. A pulse snapshots
the exact prior dword, applies the reviewed setter before the original game
update, and restores the snapshot after that update returns. Restorations are
kept on the broker hook's stack and run in reverse order, so neither a client
pointer nor persistent mutable state crosses the API boundary. Runtime counters
separate applied, restored, and game-overwritten pulses.

ABI 5 adds a second owned observation hook at `RestartGate` (`0x004A0FD0`).
The broker derives a monotonically changing session generation from active
return-5 episodes and records exits on non-5 returns. Semantic tasks snapshot
their generation and fail if drained in another session.

`F1CBrokerRequestMiniRestart(token, expected_generation)` arms the game's own
one-byte mini-restart request only during active phase 5 and only when the
caller's generation matches. It does not call the restart routine directly.
The old generation is rejected synchronously after re-entry.

## Read-only observation foundation

ABI 6 adds capability discovery, immutable vehicle snapshots, and bounded event
polling. `F1CBrokerGetInfo` advertises the exact ABI range and capability bits;
clients must test capabilities rather than infer them from a DLL filename.

Vehicle handles contain session generation, per-slot object generation, and
slot. A handle therefore fails closed after a session replacement or an
observed slot identity change. The broker copies reviewed fields on the owned
game-update thread after the original update and transactional restorations
complete. Client threads receive only the immutable copy and never a game
pointer. Every snapshot includes a sequence and validity mask.

The first polling event set covers session start/end, race-phase transitions,
and vehicle creation/destruction. It uses a fixed 256-record ring. Polling
reports the precise number of records lost when a client cursor falls behind;
callbacks and unbounded allocations are deliberately excluded. Lap, pit,
damage, and race-control events require separately reviewed transition
semantics before promotion.

Per-client registration watermark (2026-08-14): each client's poll is floored
at the ring sequence in effect when it registered, so a fresh or re-injected
client never replays pre-registration events. Without this, esc_pause_menu
re-inject replayed a stale RETIRE command 828 ms after load (client starts
polling from cursor 0; the ring returns the oldest retained window). The
watermark lives in `F1CBrokerPollEvents` only — the pure ring in
`broker_core.h` is untouched, so the deterministic regressions still exercise
the exact poll semantics.

ABI 7 adds one coherent immutable session/environment snapshot. It copies only
instruction-supported globals on the same owned update thread: session type and
phase, vehicle count, race-active state and length, track condition, wetness,
ambient/track temperature, and spectated slot. It is generation-checked and
advertised independently from vehicle snapshots.

## Deterministic regressions

The ring, cursor/drop accounting, slot identity transitions, generation math,
and handle staleness decisions live in `broker_core.h`, a pure unit with no
game memory or process state. The broker consumes that unit directly, and
`broker_regression_test.exe` (built and run by `build_broker_regressions.bat`,
part of the canonical `sdk-test` gate) drives the same code so the regressions
exercise exactly what the broker runs:

- Event overflow: more than 256 emissions wrap the ring; a lagging cursor gets
  the precise lost count, a contiguous retained window, and never rolls back.
- Hostile readers: future cursors, `UINT64_MAX` cursor wrap, capacity-zero and
  NULL buffers, small buffers, and far-behind cursors all fail closed or stay
  consistent; the ring is bit-identical to a never-touched baseline afterwards.
- Within-session slot reuse: a vehicle destroyed and recreated at the same slot
  bumps the object generation, so the old handle is rejected with
  `F1C_BROKER_STALE_OBJECT` while the new handle resolves; session changes
  reject with `F1C_BROKER_STALE_SESSION` and generation wraps restart at 1.

## ABI 10: pause-menu capability (ESC -> in-race menu)

The broker's existing RestartGate observation site (ABI 5) is the vanilla ESC
abandon point: RestartGate_004a0fd0 scans the per-frame key queue
(word_7CB21C, count dword_7CB218) for DIK 1 and calls MainMenuTransition
(findings.jsonl "escape-scancode-1-abandons-race", FACT). ABI 10 adds
F1C_BROKER_CAP_PAUSE_MENU and turns that observation into a PRE-section
(pause_menu_before, runs BEFORE the original) that implements an in-race
pause menu modeled on NT2004's same-engine RestartGate twin sub_4BACD0.

Ownership split:

- Broker (game thread, pre-section): ESC edge detection, sim freeze via
  yte_7C56AC (single-writer session-reset byte, FACT), key-queue swallow
  (zero the count so the original scan and every other per-frame consumer see
  nothing), HUD-strip rendering (*0x7CD204, proven gr21.c primitive), the
  vanilla EXIT leaf (dword_7C569C = 6 + MainMenuTransition thiscall
  this=&dword_5EA43C, byte-identical to the vanilla branch), and DIK_R (0x13)
  injection for REPLAY. It also emits F1C_BROKER_EVENT_PAUSE_MENU with the
  executed command, resets menu state and unfreezes on session end, and passes
  ESC through to vanilla while the sim is in replay mode (dword_8AB6E4 == 2).
- Clients: arm/disarm via F1CBrokerPauseMenuArm, queue one pending command
  via F1CBrokerPauseMenuCommand, and own long non-game-thread work. The
  RETIRE command only closes/unfreezes on the game thread; the client runs the
  retirement sequence itself (esc_pause_menu ports the proven
  
etire_player_to_pits() from pit_commentary.c: SessionType 7->5 flip
  re-asserted every 5 ms, restart flags pinned at 0, PLAYER_OUT wait, restore
  and 2.5 s tail -- findings "f1c-retire-to-pits-sessiontype-flip-was-a-race"
  and "f1c-sessiontype-flip-trips-sessionsynchronizer-regrid").

Menu semantics: ESC edge opens (race = CONTINUE / RETIRE TO PITS / EXIT RACE /
REPLAY; other in-car sessions = CONTINUE / RETURN TO PITS / EXIT / REPLAY),
Up/Down move the cursor with wrap, Enter confirms, ESC while open resumes
(CONTINUE). The pure state machine lives in roker_core.h (F1CMenuState,
broker_menu_tick, broker_menu_label) and is covered by
roker_regression_test.c, which shares the same header.

## ABI 13: exclusive restart guard

ABI 13 adds `F1C_BROKER_CAP_RESTART_GUARD` and
`F1CBrokerSetRestartGuard`. The capability reuses the broker-owned
`RestartGate_004A0FD0` detour; a client must not install a competing hook.
Exactly one registered client may own the guard. Registration and removal are
token-checked, and unregister clears ownership then waits for any in-flight
callback through the existing client lifetime counter.

When the game's mini-restart byte is zero, the guard is not called. When it is
set, the broker copies the callback under its lock, increments the owner's
in-flight count, calls without the lock, and then decrements the count. `ALLOW`
leaves vanilla state untouched. Any other result is fail-closed and clears the
mini-restart byte before the original gate executes. This gives Red Flag a
synchronous boundary for rebuilding its survivor-only roster before a later
player `Y` restart without broadening the broker into race-control policy.

## Variant preload compatibility � 2026-09-10

The ABI14 broker now permits preload after its existing global layout acceptance when either canonical metadata matches or a readable executable 528-byte code window at PRELOAD_ADDRESS matches the reviewed reference. Existing 6-byte detour patch validation remains mandatory. The advertised PRELOAD capability is still conditional on successful hook installation; mismatched/unreadable variant code leaves it absent and logs why. No ABI change.

Canonical and owner VM Online variants match all 528 bytes. Source helper broker_preload_layout.h; focused memory-protection and every-byte-mutation tests/evidence at development/mods/full_safety_car/trials/broker_preload_20260910 (workspace-relative). Build wrapper build_preload_variant.bat preserves earlier build artifacts. This window verifies the local preload entry/body, not every transitive game behavior. Full native SC/Red Flag lifecycle validation remains pending. Prior source/manifest/design are preserved under SC_BROKER_PRELOAD_BEFORE audit batch.

## VM timing candidate - 2026-09-10

An observed 5341us ordinary SC callback previously triggered immediate containment. Cause of elapsed time remains unknown; no scheduler diagnosis inferred. broker_callback_timing.h implements the bounded single-overrun grace described above. Per-client streak resets on registration; recovery and final quarantine use the same decision. Native allowance, 2ms drain budget, generation isolation and queue purge remain. Build uses build_vm_timing.bat with separate immutable candidate output. Canonical test_callback_recovery.py compiles actual dispatch and tests 5000/5001/5341/10000/10001us boundaries, consecutive/separated overruns, native interleaving, token/thread checks and unlocked recovery. Full SDK suite passed. VM test installation only; native validation pending. Product record: development/mods/full_safety_car/VM_DIAGNOSTIC_INSTALLED_2026-09-10.md (workspace-relative). Previous source/design/test/manifest preserved in SC_VM_DIAGNOSTIC_BEFORE audit batch.

## Current timing policy - 2026-09-10

Supersedes earlier VM grace description: small ordinary overruns warn only; sustained/severe thresholds above. See workspace development/mods/full_safety_car/IDLE_BUDGET_FIXED_2026-09-10.md. Actual dispatch tests and full SDK pass; normal-PC gameplay pending.
