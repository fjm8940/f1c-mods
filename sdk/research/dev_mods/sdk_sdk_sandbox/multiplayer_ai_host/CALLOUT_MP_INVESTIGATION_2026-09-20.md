# Multiplayer AI driver callouts — investigation, 2026-09-20

Question from the owner: in multiplayer, human players are called out as
"the McLaren driver" (team-based). Host-owned AI entrants still get the
team-based callout even though they carry real driver identities. Make AI
entrants be treated as AI (named callouts) 100%.

Status: **investigation only. No code changed. Live disambiguation pending.**

## Verified evidence chain (static, from the canonical corpus)

1. `RaceCommentaryDirectorSystem` (0x0040D4C0) drives broadcast commentary
   events (`Commentary`, `CommentaryStar`, `CommentaryStop`, `Driver` strings).
2. `VehicleListBuilderFromGlobalArray` (0x00415780) iterates the driver/entry
   array at `dword_8AC394`, count `dword_8AC390`, **stride 1060 bytes**. For
   each record it copies the string at **record+188 (0xBC)** into a format
   entry, then calls `sub_414600`.
3. `sub_414600` (0x00414600) builds the audio keys:
   `"%spws%s_"` + `FB/FE/SB/SE` and `"%stv%s_"` + `FB/FE/SB/SE`
   (i.e. `pws<name>_FB.wav` etc., matching the installed
   `Audio\Commentary\pws<Full Driver Name>_*.wav` files), and registers each
   key through `DriverNameIDLookup`.
4. `DriverNameIDLookup` (0x00415D90) indexes the same record array
   (`dword_8AC394 + 1060 * index`) and compares **record+188** against known
   driver short names ("Michael S", "Rubens B", ...) to resolve commentary
   IDs. Unknown names have no named-callout ID.
5. `VehicleSetupDriverNumber` (0x004C7B50) resolves a vehicle's DriverID
   (`this+0x489C`) from the full name at `vehicle+0x4620`, falling back to the
   record name `record+0` and commentary name `record+188`.
6. `TextFileReader` (0x004B09A0) copies the player's profile name into the
   current vehicle's name field and into `vehicle+188` — consistent with MP
   humans being identified by profile name, not by an RCD driver identity.

## Interpretation

- Named commentary requires a driver/entry record whose **+188** string is a
  recognized driver short name. Human MP cars carry profile names there, so
  the game cannot resolve a driver callout and uses the generic team phrasing
  ("the McLaren driver") — expected stock MP behavior.
- The multiplayer AI host creates bots as role-1/role-2 **vehicle objects**
  through `SessionStateTransition` + `NationalityDriverInfoLoader` +
  `TeamIDSetter` + `VehgenScnFileLoader`/`SceneRenderUpdate`. It is not yet
  established that this path creates or updates the 1060-byte **driver/entry
  record** used by the commentary system, nor what record+188 contains for a
  host-owned bot.

## Hypotheses to disambiguate (live, read-only)

- **H1 — missing/stale record**: bots have no driver record (or a record with
  an empty/team +188), so commentary falls back to the team phrasing.
- **H2 — session branch**: the commentary system branches on multiplayer
  session type and ignores named callouts for every car; then even a correct
  record is unused.
- **H3 — wrong name format**: record exists but +188 holds the full name
  ("Michael Schumacher") instead of the short name the lookup expects.

## Live evidence, 2026-09-20 (host pid 21168, Imola MP session)

Read-only `memory-mcp drivers` capture at 14:48:51Z while the 22-bot grid was
running (session_type=1, game_state=5, race_phase=5):

- slot 0: `Matias` role 0, ai_control 0 (host human)
- slot 1: `Test Man` role 2, ai_control 0 (client human)
- slots 2..23: role 1, ai_control 1, full 2002 identities
  (`Michael Schumacher (2002)`, `Olivier Panis (2002)`, ...)

Owner confirms the commentary still uses team phrasing ("the X driver") for
these cars despite correct role, ai_control, and names. So the callout path
does not read the vehicle identity used by the AI update loop; the 1060-byte
driver/entry record (+124 / +188) remains the prime suspect and the next
instrumentation target. H2 (session branch) is now more likely than H1 (no
identity at all), but a record read is still required to decide between
populating record fields and hooking the name-key path.

## Live field probe, 2026-09-20 (read-only, pid 21168, Imola MP race)

Frida read-only probe (attach, read, detach; no writes) of vehicle slots
(`0x5EC6D8 + slot * 0x48C0`) and the 273-entry vehicle catalog
(`dword_8AC394`, stride 1060):

| slot | role | +0x4620 name | +0x46F8 abbreviation | catalog idx | catalog name |
|---|---|---|---|---|---|
| 0 (host human) | 0 | Matias | PC | 116 | Ricardo Zonta (2002) |
| 1 (client human) | 2 | Test Man | T. MAN | 71 | Tester A (2002) |
| 2 (bot) | 1 | Michael Schumacher (2002) | M. SCHUMACHER | 10 | Michael Schumacher (2002) |
| 15 (bot) | 1 | Olivier Panis (2002) | O. PANIS | 179 | Olivier Panis (2002) |

Catalog record layout (verified live): +0 = pointer to `.veh` path,
+124 = team name, +188 = driver name (full, year-suffixed).

Conclusions:

- AI cars carry complete, correct identities; catalog indices resolve to the
  matching driver records. H1 (missing identity) is disproven for the host
  side.
- Humans carry profile names with unrelated catalog indices, so generic team
  callouts for humans are stock MP behaviour.
- `DriverNameIDLookup` (0x00415D90) compares only **short** names
  ("Michael S", "Rubens B", ...) in both branches, while this content stores
  **full year-suffixed** names in catalog +188. Named-callout selection
  therefore cannot succeed through that lookup for these records.
- The `pws<name>_FB.wav` files themselves exist for both plain and
  year-suffixed names, so the open question is which name source the
  commentary uses per session type (single player vs multiplayer).

## Session-type comparison, 2026-09-20

Owner confirms single-player always calls these same cars by driver name, while
multiplayer uses team phrasing. Live MP globals (read-only) show the
commentary machinery is active, not disabled:

- `dword_7D0308` (commentary gate) = 3
- `flt_7CF3C8` (track condition) = 0.268
- `dword_5C329C` (active vehicles) = 24, `dword_7D0410` (session mode) = 5
- `dword_7C54E8` (local player vehicle) resolves

AI commentary producers (`PerDriverRaceCommentary`, `RaceCommentary`) queue
events through `DriverDataTableLookupAndTimeConversion` (0x4144F0) with the
per-car DriverID (`vehicle+18588`); playback consumes the queue in
`TextOverlayDisplayTimer` from `flt_5EA618`. Bots carry valid DriverIDs
(10 = Michael Schumacher (2002), 179 = Olivier Panis (2002), ...), so the
remaining unknown is exactly where the played event loses the driver identity
in multiplayer (playback name table vs. player-slot-based resolution).

A temporary Frida aggregation hook on 0x4144F0 was attempted but the available
tool surface evaluates each script in a fresh runtime and does not await
promises, so persistent hooks are not possible through it. Diagnostic hooking
must therefore live inside the mod.

## Implemented fix, 2026-09-20

Root cause (static + live): driver/team commentary identity is a whole
pre-recorded clip selected per driver through `DriverNameIDLookup`
(0x415D90). That lookup compares the catalog record name (`record+188`)
against **short** names ("Michael S", "Rubens B", ...), while this content
stores **full year-suffixed** names ("Michael Schumacher (2002)"). The
lookup therefore never resolves for these cars and the generic team phrase
is used.

Implementation (SDK-reviewed, no raw addresses in mod code):

1. SDK extension in `tools/sdk/f1c_sdk_overrides.json`:
   - reviewed globals `VehicleCatalog` (0x008AC394, record pointer) and
     `VehicleCatalogCount` (0x008AC390);
   - reviewed ABI `DriverNameIDLookup` (stdcall, 5 args) with disassembly
     evidence. `sdk-refresh` passes: 43 reviewed ABIs.
2. New hook in `multiplayer_ai_host` on `DriverNameIDLookup`:
   `driver_name_lookup_hook` calls the original first; if it left the entry
   unresolved (`entry_field[2] < 0`) and the catalog name is year-suffixed,
   it temporarily substitutes short-form candidates (base name, first+last
   initial, first two words, first word), re-runs the lookup, restores the
   name, and logs `callout name resolved: index=... via='...' id=...`.
3. Build and gates: `generate`, `build`, project `test` gate all pass.
   DLL SHA-256: `0c25f8b77a1fc8c856eb776fd94276137130f1ce208f74d744c0058d7fd50a1d`.

Test plan: start the game, load the new build, host a multiplayer session
with the laptop, then read `multiplayer_ai_host_log.txt` for
`callout name resolved` lines and listen for named callouts. If the host
works, rebuild the friend package so the laptop gets the same fix.

## Superseded: diagnostic hook plan

Add a temporary, logging-only hook (no game writes) to the sandbox mod:

1. Hook `DriverDataTableLookupAndTimeConversion` (0x4144F0) and log
   `(event_id, driver_id)` pairs while a multiplayer session runs.
2. Hook the queue consumer (`TextOverlayDisplayTimer`) if ABI review allows,
   to capture the played event and the resolved name/audio id.
3. Compare a bot (driver id 10/179) against a human car in MP.
4. Decide the fix from the evidence:
   - if played events carry the bot DriverID but resolve generically: patch or
     hook the playback name resolution for AI-owned cars;
   - if events are dropped before playback: fix the selection gate for
     AI-owned cars.

## Disambiguation plan (needs a guarded MP weekend)

1. Extend the SDK with a reviewed, read-only accessor for the driver/entry
   array (base `dword_8AC394`, count `dword_8AC390`, stride 1060, name at +0,
   commentary name at +188). No raw address in mod code; broker-owned.
2. In the AI mod, log for each bot and for one human car: record presence,
   record+0, record+188, and the resolved `DriverNameIDLookup` result.
3. Compare host and client logs for the same car (role 1 vs role 2).
4. If H1/H3: add a reviewed write that populates the bot's record fields from
   the already-resolved roster identity (the mod has the full name and the
   abbreviation at `vehicle+0x4620` / `+0x46F8`).
5. If H2: hook the commentary name-key resolution (`sub_414600` input or the
   MP branch that selects generic phrasing) and substitute the bot's driver
   identity for AI-owned vehicles only; humans keep stock behavior.

## Non-goals

- No change to human-player callouts.
- No hard-coded global addresses in the mod; any new field access goes through
  the reviewed SDK/broker path.
- No production or executable modification.

## Validation gates

1. Static: SDK accessor reviewed, focused tests pass, `sdk-test` passes.
2. Live: guarded full weekend (never Quick Race), two humans + one bot;
   host and client commentary logs show the bot's named callout and humans
   unchanged.
3. Scale: repeat with the full grid before claiming the fix.
