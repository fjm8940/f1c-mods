# Pit-stop commentary methodology — 2026-07-28 session

How PWS_QUAL28/29/30 and PWS_PIT06/09/10/12 were found, restored, and
debugged. Written up as a reusable playbook — the techniques below apply to
any future "find the trigger for a silent clip" work in this folder, not
just pit stops.

## 1. eid twins: low identity-mapped vs high team-bio table

`data/full_event_dump.json` often lists the SAME clip name under TWO eids —
a low one (identity-mapped range, e.g. 11-51) and a high one (293-336,
reached via `TeamNameDisplayIDLookup_0041b980`). **The high eid is a trap**:
it indexes an unrelated team-bio narration table that happens to have the
same entry count. Firing it plays the wrong clip ("Benetton team overview"
instead of the traffic-density line), even though the queue accepts it
cleanly and reports success.

Find both: grep `data/full_event_dump.json`'s `events` dict for all entries
sharing a `clip_name`, and compare `score`/`sdt_index` — a real twin pair
has identical `score` and `sdt_index`, only `eid` differs. Use the low one.

## 2. ALWAYS isolation-test a new eid before wiring a trigger

`tools/commentary_inject.py`'s `inject_event(eid)` fires one eid alone, no
trigger logic involved. Do this BEFORE writing any DLL condition, and get a
human to confirm out loud what played.

**Why this is not optional:** a clean compile + a queue-accept confirmation
proves the PIPELINE works, not that you named the right eid. This session
shipped a DLL with a one-column mix-up (used the SDT-index column instead of
the eid-twin column) — it compiled clean, the queue confirmed the eid
landed, and it still played the wrong clip ("Gear" instead of "Go") twice
in a row before anyone noticed. Isolation-testing the corrected eid caught
it in 3 calls, no DLL rebuild needed.

## 3. Full vehicle-struct diff tracer, not a curated field list

`tools/trace_pitstop_full.py` reads the ENTIRE player vehicle struct
(0x48C0 bytes) every poll and reports every 4-byte-aligned offset that
changed, filtered to small-int (enum/flag-shaped) values plus an explicit
float watchlist (fuel, tire wear). This is how `pittingStatus`'s full enum
(0=track, 1=driving, 2=entering pit lane, 3=stopped in box, 4=exiting) and
its companion offsets (`tireChoice`, service-complete flag cluster,
pit-visit counter) were found in one session, instead of guessing candidate
offsets one at a time.

The small-int filter matters: an unfiltered diff of a moving car is
dominated by physics noise (position, velocity, suspension) every tick.
Filtering to values in roughly [-8, 12] keeps flags/enums/counters and drops
continuous physics — add specific fields to the float watchlist by name if
you know what you're looking for (`FLOAT_FIELDS` dict in the script).

**Confirmation trick:** correlate what's on screen with what the trace
shows. Coulthard visibly still driving through the pit lane, De La Rosa
visibly out on track, both read against `pittingStatus` at the same
timestamps — that's what turned a raw offset diff into a proven trigger
condition, not just a plausible one.

## 4. The two "is it really playing" signals — and which one lies

- `CONTROLLER + 0xAE4` (`IS_PLAYING`): a simple "is *any* clip talking"
  boolean. **Produces false positives** — it can read true because of
  unrelated ambient/UI audio, so a DLL trusting it alone can log "success"
  while playing nothing, or nothing relevant.
- `CONTROLLER + 0x864` (`Q_HEAD`, the eid field of the queue-head struct):
  more specific, but **drains within about one 100ms poll** once the game
  actually starts playing the clip. Polling it on a 100ms cadence produces
  false "GAVE UP" negatives on clips that genuinely played.
- `CONTROLLER + 0xAF8 + eid*4` (`EVENT_TS(eid)`, per-event last-played
  timestamp float): **the reliable one**. It persists after the clip plays,
  so comparing it before/after a fire (not just checking a single instant)
  gives a real confirmation without a race window. This is the same array
  `tools/commentary_inject.py` reports as `old_ts`/`ts_after`, and what
  `mods/red_flag/redflag.c` zeroes to clear a cooldown.

## 5. Fire on the edge immediately, don't wait for silence, for time-critical lines

An early build gated injection on `IS_PLAYING` being false — wait for the
audio channel to go quiet, then fire. For an ambience line (traffic
density) that's fine. For a procedure call like "Go" (the pit crew release
the car NOW), waiting behind an unrelated race-event line the engineer
happened to be saying meant it played after the moment had passed —
functionally useless. Fire time-critical clips immediately on the trigger
edge and let the game's own priority/queue handling deal with any
collision; don't build in an artificial wait.

## 6. Never call inject on every retry — fire once, then only poll to confirm

A subtle version of the same double-fire risk: if your retry loop calls the
injection function again on every failed confirmation attempt, and an early
attempt actually DID succeed but your confirmation check hadn't caught up
yet (see #4), you get a second, genuinely audible copy of the clip. Fire
exactly once per trigger edge; all subsequent loop iterations should only
be checking `EVENT_TS`, never re-injecting.

## 7. Safe DLL hot-swap: self-eject, not external FreeLibrary

**Do not** build an external "ejector" that calls `CreateRemoteThread` +
`kernel32!FreeLibrary` on a target DLL from outside. This session tried
exactly that (`tools/dll_eject.py`, since deleted) and it **crashed the
game** — freeing a module while its own worker thread (spawned in
`DllMain`, running an infinite poll loop) is still executing inside it is
inherently unsafe.

The working pattern: the DLL ejects **itself**.
1. `DllMain` creates a named, manual-reset event (`CreateEventA`) and stores
   its own `HINSTANCE`.
2. The worker thread checks `WaitForSingleObject(event, 0)` at the top of
   every poll iteration.
3. On signal, it breaks its loop and calls
   `FreeLibraryAndExitThread(hinst, 0)` — this drops the module's refcount
   and terminates the calling thread as one atomic step, so the very last
   instruction executing in the module IS the unload. Nothing is ever left
   running in freed code.
4. The host side (`tools/dll_unload_signal.py`) only has to `OpenEventA` +
   `SetEvent` by name — no remote thread, no race, no bitness handling
   needed (named kernel objects are visible across the 32/64-bit boundary,
   so 64-bit Python can signal a 32-bit game's DLL directly).

See `dll/pit_exit_commentary.c` (`UNLOAD_EVENT_NAME`, `g_unload_ev`,
`FreeLibraryAndExitThread`) for the reference implementation. **Any new DLL
in this folder should be built with this pattern from the start** — a DLL
built without it can only be removed by restarting the game.

## 8. WOW64 module enumeration gotchas (if you ever need this again)

The game is a 32-bit process; a 64-bit Python/tool enumerating its modules
hits two easy-to-miss traps:
- `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)` from a 64-bit caller
  against a 32-bit target silently returns almost nothing (just the WOW64
  stub modules) instead of erroring — looks like success, isn't.
- `EnumProcessModulesEx` (psapi.dll) works correctly cross-bitness, but
  `LIST_MODULES_32BIT` is `0x01`, not `0x02` (`0x02` is `LIST_MODULES_64BIT`
  — an easy transposition to get backwards, and it fails silently by
  returning the wrong module set rather than erroring). The returned handle
  array elements are sized for the CALLING process's pointer width (8 bytes
  on a 64-bit caller), not the target's (4 bytes) — reading it as 4-byte
  entries reads every other slot as garbage.
- CheatEngine's own `enum_modules` MCP tool also failed outright on this
  process (`CE_API_UNAVAILABLE`, reported likely anti-cheat interference).
  There is no shortcut here; `EnumProcessModulesEx` done correctly is what
  worked.

## 9. Frida MCP session gotcha: no shared state across `execute_in_session` calls

Each `execute_in_session` call appears to run as an isolated script — a
`globalThis` variable set in one call is `undefined` in the next, even
within the same session ID. A hook's `onEnter` callback firing later can't
push into an array declared in an earlier call and expect a subsequent call
to read it back.

**Workaround:** have the hook write directly to a log file on disk
(`new File(path, "a")`, `.write()`, `.close()`) instead of accumulating
results in a script-level variable, then read the file from outside Frida
entirely (plain file read) once you're done capturing.
