# Race Data Collector in multiplayer — assessment, 2026-09-20

Owner request: ensure the collector works in multiplayer and collects data as
intended. Status: **assessment only. No code or installed artifact changed.**
No multiplayer run has been captured yet; the native acceptance matrix in
`../STATUS.md` is still entirely pending and does not distinguish single- from
multiplayer sessions.

## Why multiplayer is a distinct risk

1. **Session identity.** Capture identity currently derives from session type
   and championship normalization. Multiplayer sessions may present a
   different session-type value or lifecycle (lobby, formation, restart,
   disconnect), and restart/session transitions are already an open item.
2. **Host-owned AI entrants.** The multiplayer AI host creates role-1 AI on
   the host and role-2 network replicas on clients. The collector reads the
   shared vehicle/timing state; it is unproven whether:
   - host-side AI cars appear as normal entries with laps/timing;
   - client-side replicas appear once (not duplicated) and with the same
     identity;
   - classification and retirement of AI cars are exported.
3. **Identity sources.** The collector's per-driver identity mapping was
   validated for single-player RCD/VEH data. In MP, human cars are identified
   by profile/network names, and the commentary/entry record may differ
   (see the callout investigation in the AI mod). Driver identity fields in
   exports may be blank, team-based, or duplicated.
4. **Timing/classification scope.** Lap, pit, and results polling events must
   be compared against the native multiplayer results screen, including
   AI-added cars, before "as intended" can be claimed.

## Live observation, 2026-09-20

During a 22-bot multiplayer session at Imola with the collector loaded
(Lightloader log 17:42:30), no capture or export was produced. A read-only
`memory-mcp` snapshot of the same session reported
`session_type=1, game_state=5, race_phase=5`.

`src/plugin.cpp:177` gates capture on `session_type == 7`. If the multiplayer
race legitimately reports session type 1, that gate is the reason for the
missing capture; if the session was practice, the collector behaved as
designed. This must be confirmed with a race session before any source change.

## Required validation (next guarded MP session)

1. Run the installed collector (0.2.5) on the host and on one client through a
   full multiplayer weekend with the AI host mod active (two humans + one bot,
   then scale up).
2. Capture the JSON export from both machines plus a manual grid/lap record.
3. Check, per car: one entry per physical car, correct driver identity, lap
   counts and pit events, classification/retirement, and no cross-session
   contamination after restart/exit.
4. Repeat with the bot removed to separate MP-native issues from AI-mod
   issues.

## Likely follow-up work (not yet specified)

- A read-only MP capture aid (log of session type, car count, per-car role,
  timing pointers) to attribute any gap to the collector or the AI mod.
- Export-side identity fallback for MP names if the evidence shows blank
  identities for AI entrants.
- Explicit multiplayer section in `DATA_COVERAGE.md` once evidence exists.

## Limits of this assessment

- No live evidence was produced; all items above are risk statements, not
  findings.
- The collector remains unmodified and no reinstall is implied.
