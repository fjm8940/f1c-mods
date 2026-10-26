# How does a spoken driver/team name get into commentary audio?

## Question

For lines that should mention a driver or team ("Michael Schumacher is on
pole", "Ferrari's strategy..."), how does the name make it into the audio?

- **H1** — one whole pre-recorded .mp2 per driver/team variant of a line (like
  the confirmed 17-per-track win clips picked by `PerDriverRaceCommentary_00419c50`).
- **H2** — a generic line clip plays back-to-back with a separate short
  name-only clip (concatenative audio), sequenced by a playback queue.

## What was checked

1. **`README.md` / `docs/HANDOFF_2026-07-28.md`** — no existing note on this
   specific question. `CLAUDE.md`'s "Event emission chain for PWS_QUAL32"
   section documents the emission call chain
   (`NamedEntryLookupInFormatList` → `DriverDataTableLookupAndTimeConversion`
   → `InsertDriverTableEntry(0x5EA618, buf)`), one call per event, no loop.

2. **SDT catalogs**, both banks fully listed with
   `tools/sdt_tool.py list`:
   - `Pws/PC_RE_MAIN.SDT` — 316 clips
   - `Tv/PC_TV_MAIN.SDT` — 258 clips

   No clip in either catalog is named after a driver or team (no
   `SCHUMACHER.mp2`, `FERRARI.mp2`, etc. — confirmed by grepping both listings
   for all 2001-season driver surnames and constructor names: zero hits).
   Every clip name is an event-code stem (`PWS_QUAL32`, `PWS_AIWIN02`,
   `TV_RACEINT14`, `TV_1999_DRIVERSCHAMP...` (truncated), etc.). The stored
   name field is hard-truncated to 15 ASCII chars at build time (verified by
   raw hex dump of clip headers 44/45/82-93 in `PC_TV_MAIN.SDT` — the header
   is a fixed 40-byte struct, `raw[8:40]`, and the 15-char strings are
   genuinely followed by `\x00` padding, not display truncation). No
   catalog of short per-name clips exists in either bank: the smallest
   clips present (`PWS_POSUPDATE01` 3134B, `PWS_PIT08` 3605B, `PWS_HIT09`
   3291B) are all event-coded, not name-coded, and are still full sentence
   fragments, not isolated surnames.

3. **Playback/queue trace downstream of `0x5EA618`.**
   - `InsertDriverTableEntry_00418560.c` (`tools/ask.py func InsertDriverTableEntry`):
     40 callers, all one-shot event emitters (`AIPositionChangeEvent`,
     `AIVehicleDamageEvent`, `PerDriverRaceCommentary`, etc.). Callees are
     `StructCopy`, a short computation, and `ScoreThresholdGateCheck` — it
     inserts exactly one table row per call, no iteration over multiple
     audio resources.
   - The file path builder, `sub_414600_00414600.c` (called from
     `NamedEntryLookupInFormatList` and `VehicleListBuilderFromGlobalArray`),
     uses format strings `"%stv%s_"` / `"%spws%s_"` and calls
     `DriverNameIDLookup_00415d90` once to resolve the driver/team-specific
     **event ID number**, which becomes part of a single filename fragment.
     It builds one complete file path per system (PWS, TV) per event — not a
     name-path plus a separate clip-path.
   - `DriverNameIDLookup_00415d90.c` (see `CLAUDE.md`'s mapping table) takes a
     driver/team name string and returns a **different event ID** depending on
     which driver/team matched (e.g. Michael Schumacher → 204/402/209/411
     depending on context flags, Ferrari → 409/420/426/437). This is the
     mechanism: the *event ID itself* is driver/team-specific, and that ID
     maps 1:1 to one SDT clip already containing the full spoken sentence.

4. **`config_schema.json` / `strings_index.json`** — grepped for any
   driver-surname or constructor-name-keyed audio resource token (all 2001
   driver surnames, all constructor names). Zero matches in
   `strings_index.json`. No such tokens exist.

5. **`findings.jsonl`** — queried via `python tools/ask.py findings <kw>` for
   "name clip", "concatenat", "InsertDriverTableEntry": no prior entries.
   This is a new finding.

## Verdict

**H1 CONFIRMED, tier DERIVED→CONFIRMED** (two independent static sources: the
absence of any name-keyed clip in either full SDT catalog, and the call chain
showing `DriverNameIDLookup` selects a whole different **event ID** — not a
name fragment — per driver/team).

There is no concatenative name-splicing system in this game. Driver/team
identity is baked into which whole pre-recorded clip gets selected
(`DriverNameIDLookup` / `sub_417640` pick one event ID out of a per-driver or
per-team table), exactly the same mechanism already confirmed for the
17 per-track win clips (`PerDriverRaceCommentary_00419c50`). The file path
builder (`sub_414600`) constructs one complete filename per event, not a
two-part name+line sequence, and `InsertDriverTableEntry` queues exactly one
resource per call with no loop.

This means: for any commentary line that needs to "say" a specific driver or
team, the corpus must contain (or the mod must synthesize/select) a full
per-variant clip — there is no shortcut of playing a generic line plus a
name snippet.

## Live verification (not required, but would upgrade to FACT)

Not performed — game state not required to reach this verdict since it's a
pure static/data-catalog question and both catalogs are exhaustively listed
on disk. If a next session wants FACT-tier certainty:

1. Run `python tools/commentary_inject.py <eid>` for two different eids that
   `DriverNameIDLookup` maps to the same base line but different drivers
   (e.g. 204 vs 205 vs 209 vs 210 for Michael Schumacher, from the table in
   `CLAUDE.md`), and listen for whether each plays a distinct pre-baked
   sentence (confirms H1) or the same generic sentence with a spliced name
   audibly stitched in (would indicate H2 was missed by static analysis).
2. Watch the SDT index actually opened for playback (via `race_monitor.py` or
   a file-handle/read watch on the `Pws`/`Tv` folders) during a name-bearing
   event, and confirm it opens exactly one `.mp2` per event, not two in quick
   succession.
