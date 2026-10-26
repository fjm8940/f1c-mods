Current revision: [0.3.4](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-27-simulated-save/RESULT.md): also saves the game-simulated classification when FINISH/NEXT is pressed before the end (friend-PC root cause).

Current revision: [0.3.3 diagnostic timeline](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-27-timeline/RESULT.md). Friend-PC failure under investigation; owner PC still runs the 2026-09-14 broker revision.

Current revision: [0.3.2 refusal diagnostics](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-27-diagnostic/RESULT.md). Same policy as 0.3.1; logs why a FINISH/NEXT simulation was not blocked.

Companion tool: [Grid Restore 0.1.0](../../mods/sdk_sandbox/grid_restore/README.md) re-applies or edits the saved qualifying_*.tsv order as the race grid before the start (candidate; native validation pending).

Current revision: [standalone 0.3.1 static runtime](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-27-static-runtime/RESULT.md). Same behaviour as 0.3.0; MinGW runtime statically linked so the DLL loads without MSYS2 (0.3.0 failed with error 126 on such PCs). Fixed addresses match all local F1C EXE variants. Native validation pending.

Current revision: [standalone 0.3.0 verification](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-15-standalone/RESULT.md). No runtime EXE identity/layout-fingerprint or broker/loader gate. Six-byte hook instruction integrity and completed-qualifying conditions remain. Archive failure no longer permits re-simulation. Process-lifetime pin replaces hot-unload. Built and tested, not installed; native validation pending. Earlier revision descriptions below are historical.

Current revision: [2026-09-14 executable compatibility candidate](../../mods/sdk_sandbox/qualifying_preservation/trials/2026-09-14-portability/RESULT.md). The current DLL uses reviewed layout/code checks instead of an exact EXE hash; both local game variants pass offline checks. Broker and full-weekend gameplay validation requirements remain. The September 7 record below is retained as historical evidence.

# Qualifying preservation â€” 2026-09-07

. Native candidate built; not installed or live-validated.
Canonical interface: `python tools/f1c_mod.py` from this repository root.

## Owner objective and scope

Preserve an actually completed qualifying classification when FINISH SESSION or NEXT SESSION is pressed, while retaining native simulation for unfinished qualifying. The requested direction is qualifying -> race (the owner's first sentence reverses the direction; surrounding explanation and observed captures establish the intended direction). This task creates a reusable save/compare script and an isolated native guard candidate, not a randomizer feature or public release. Default for ending early is unchanged native simulation. Optional owner question about preserving existing times during early finishing remains unanswered; no such requirement is implemented.

## Verified cause and bridge

Retained September 7 module snapshots show session type 5, phase 8, 24 entries, 95% strength, mode 3, lap limit 12. SessionClock is a DOUBLE: 4069.5001184; start 30; end 3630. The clock had expired by 439.500 seconds. All int32 active flags at V+45F4 were zero and all speeds were zero. Cars with 12 laps had finish state 1; many with 11 laps still had state 0. Thus requiring all finish states to be terminal would miss this exact bug. Owner reported no cars on track.

The subsequent race snapshot retains post-transition qualifying times at V+405C. Six actually improved: Manning, Coulthard, Raikkonen, Lasee, Bobbi, Sarrazin. All six previously had 11 laps. Eighteen positions changed. This is stronger evidence than just seeing a reordered list.

Raw chain: NEXT SESSION handler sub_46DC60 calls sub_46BF10 under qualifying+byte_7CEBC4. FINISH SESSION handler DispatchChainWithVTableCalls_0046DA20 calls the same wrapper. Wrapper calls SessionSimulationStep (legacy AIOvertake, 004BEB10), whose first initialization calls AIBehaviorInitialization (004BF0A0). That initializer may give another lap to eligible AI below the lap limit, even when bestLap already exists; it replaces bestLap if the generated time is better. The simulator's local clock begins zero in PerformanceSetupStructInitializer, so its first entry reaches initialization before its later local deadline handling. Runtime intervention at the exact branch was not traced; the code and before/after evidence strongly support this mechanism.

Do NOT clear byte_7CEBC4: it is also the qualifying-enabled flag used by the native transition to call FieldCopy. FieldCopy V+4034 copies V+4060 -> V+4058 and V+409C -> V+405C; later FieldCopy_c330 restores V+4058 -> V+4060. Disabling the flag can suppress the very grid save we need.

The native candidate instead hooks the simulation STEP at 004BEB10, not the whole wrapper. On qualifying type 5, championship mode 3, phase 5/8, finite expired clock, complete unique classification and valid active vehicle records, all inactive and stationary, it archives the order, initializes the caller-owned stop byte exactly like the original, and returns false. The wrapper continues its normal cleanup; the caller continues its grid/result copying. It does not alter best times, positions, finish flags, lap counters, RA, QA, qualifying-enabled flag or executable files. In all other states it forwards the original step through its trampoline. A failed archive forwards vanilla and logs failure.

## ABI and ownership evidence

Exact supported executable SHA-256: 1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003.

The candidate's simulator-entry/caller/exit/return JSONs were produced by variant-profile from that executable. The six stolen bytes are one complete position-independent instruction: `81 EC C4 01 00 00` (sub esp, 0x1C4). Caller pushes duration then stop pointer and puts local simulation object in ECX. Both function tails use ret 8 and EAX 0/1; caller tests AL. Reviewed ABI is uint8 thiscall(void* simulation, uint8_t* stop, float duration), game-thread forwarding only. This is not approval to call the simulator arbitrarily.

The DLL checks complete on-disk executable hash plus SDK PE metadata, negotiates lifecycle capability with the canonical broker, registers ownership, and posts hook installation/removal to the broker's game thread. It does not duplicate the broker tick/restart hook. Unexpected entry bytes refuse install. The callback requires the same owner thread and uses no hard-coded addresses: named SDK globals/accessors and generated hook bindings supply the reviewed target. Unload restores only owned bytes; failed/conflicted/pending teardown retains the DLL rather than unloading referenced callbacks. Retained failure instances require game exit; no clean-unload claim for those paths.

## Save script

Read-only immutable archive, independent of the candidate guard:

```powershell
python tools/f1c_mod.py qualifying-order capture my_qualifying --output PATH_TO_NEW_JSON
python tools/f1c_mod.py qualifying-order inspect SAVED_SNAPSHOT_TAG --output PATH_TO_NEW_JSON
python tools/f1c_mod.py qualifying-order compare BEFORE_JSON AFTER_JSON --output PATH_TO_NEW_COMPARISON
```

Use an elevated hidden shell when the running game is elevated. New snapshot tags/output paths are mandatory. No existing evidence is overwritten. The parser validates target hash, captured page hashes, vehicle pointers, names and position permutation; it reads all active entries, not the old 22-car utility limit. It preserves untimed entries and exact float values; compare uses driver identity, not pointer/slot ordering. Sequential snapshots are not atomic. Archive success does not lock the game.

## Native candidate and testing

[Candidate source and loading boundaries](../../mods/sdk_sandbox/qualifying_preservation/README.md).
Build uses `python tools/f1c_mod.py build qualifying_preservation --exe GAME_EXE`.
The build compiles/runs the pure policy regressions before producing the DLL. SDK regression tests cover parser integrity, immutable writes, missing/changed data, policy branches and the retained 24-car transition (18 changed positions and six changed times). Check the task closeout for actual gate outcomes.

Pending human/gameplay matrix: current exact target and canonical broker; full race weekend, never QuickRace. Test natural expiry with cars inactive through both NEXT and FINISH; exact pre/post time and order equality required. Test early session, expired clock with car still completing a lap, temporary empty track, no-time entries, repeated button use, next weekend, and race-session bypass. Verify archive-created and original-simulation-forwarding logs, no UI hang, correct grid placement and cooperative unload. Native hook has not been loaded in this task. Existing broken race order has not been repaired. The RA sweep keeps its original evidence and has a recorded grid confound.

## Corrections and preservation

Earlier ad-hoc JSON decoded 8AFEA0 as float32 and mislabelled 32A0 as aiActive. Correct schema reads float64 session clock, labels that field neutrally, and includes controller kind 32C8 separately. Old snapshots/exports remain untouched. The read-standings legacy utility silently treated failed reads as zero and capped at 22; it is not used by the new script.

Complete before-states of edited SDK operational documents, generated SDK core outputs, CLI, generator and career research index are under workspace_organization_audit/QUALIFYING_PRESERVATION_BEFORE_2026-09-07. Restoration is selective from those copies, preserving new evidence and any intervening edits; never reset the intentionally dirty SDK tree. No game files or memory changed in this development task. Existing user-generated settings and other SDK changes are preserved.

## Task closeout

Current follow-up: [live access and startup correction](../../../development/career_mod_platform/verification/qualifying-preservation-2026-09-07/LIVE_ACCESS_FIX.md). Initial installed startup failed because the broker API structure size was missing. Corrected build is staged for restart; live capture works. Updated checks: 16 native assertions and 13 focused Python tests. Guard activation and gameplay preservation remain pending. The paragraph below retains the initial development closeout.

Native build and 13 shared-policy assertions passed. All 12 focused archive regressions passed, including the real 24-car snapshots. Exact-target doctor passed. Full SDK gate did not pass: existing orchestrator test still uses the missing Desktop/F1/F1 Challenge V10 executable path. No live guard installation. DLL SHA-256: FC6B2CC7EF398324B0AFA328B39E21745005D319144703C7F60EFE7BF52404E8. See the indexed career verification/qualifying-preservation-2026-09-07 record for logs, changed files, restoration, pending human validation and wasted-effort check.

