# ASSIGNMENT EVF-002 — T1 batch (8 rows) + remaining T2 (10 rows)

Read `../DEEPSEEK_INSTRUCTIONS.md` first. It has been updated since EVF-001 —
two new rules apply to you now (§12a settled globals, §12b uncited numbers), plus
a hard rule against truncating prior-art searches.

---

## 0. CAPABILITY CHECK — answer this in your first reply, before any work

EVF-001 was written as if live verification were impossible. It is not. State
plainly, one line each:

1. **Can you execute shell commands?** (You cited `tools/ask.py offset 0x403C`
   and "19 readers" for `0x8ACF8C` — the reader count is exactly right, so either
   you ran it or you read the underlying store. Which was it?)
2. **Can you read files** under `%USERPROFILE%\Desktop\f1c_ida_decompiled`?
3. **Can you write files** there? (You wrote a spec + appended to
   `findings.jsonl`, so presumably yes.)

Do not guess or be agreeable about this. If you cannot run commands, say so —
the whole assignment changes shape, and a false yes wastes a review cycle.

---

## 1. THE GAME IS RUNNING RIGHT NOW

`F1 Challenge Online.exe`, in a **race** session, non-ASLR (absolute addresses
are live and valid as written). This changes what you are allowed to conclude:

**"Needs live observation" is no longer an acceptable place to stop** for
anything a single read can settle. On EVF-001 you listed five uncertainties; two
were closed in about a minute by two reads. Do that yourself.

### Read live memory — allowed, encouraged
```bash
python tools/poke.py read <Name>            # by name, e.g. SessionType
python tools/poke.py read 0x008acf8c:int32  # by address:type
python tools/poke.py veh 0x403C:int32       # player vehicle struct field
python tools/poke.py list <substring>       # find a named global
python tools/poke.py snapshot before.json   # capture all known globals
python tools/poke.py diff before.json       # what moved since
```

### Writing to live memory — NO
Read-only this batch. No `poke.py write`, no `patch.py apply`, no DLL injection.
A write can corrupt the user's live race. Also: **never** write `veh+0x10C0` or
`veh+0x33C8` — each kills the process instantly.

### Reads you cannot do yourself
Anything needing a *different* session state (a timed race, a wet race,
qualifying, a formation lap) or a human watching the screen. Don't fake it —
emit a request block and the human will run it:
```
LIVE READ REQUEST
  precondition: <exact session state needed, e.g. "timed race, 30 min">
  commands:     <the exact poke.py lines to run>
  distinguishes: <what value means what — state the branch BEFORE seeing it>
```
That last line is mandatory. Predict the outcomes first; a prediction made
after the fact is worthless.

---

## 2. Already settled — cite these, do not re-derive

| Thing | Value | Tier |
|---|---|---|
| `SessionType` `0x7C56A4` | **5 = qualifying, 7 = race** | FACT, live |
| `TotalRaceLaps` `0x8ACF8C` | **post-reduction, truncated** (53-lap @50% → 26) | FACT, live |
| `lapNumber` `veh+0x403C` | 0 on grid pre-start; per-car | CONFIRMED, LATCHED |
| `RaceActiveFlag` `0x7D0308` | 3 while on track in a race | FACT |
| `flt_7CF3C8` | commentary/track-condition gate, must be != 0.0 | CONFIRMED |

The enum in `docs/38-confirmed-offsets.md` is **wrong** about qualifying. Ignore
it. Do not confuse `0x7C56A4` (SessionType) with `0x7C56A8` (RacePhase) — both
can read 5, meaning different things.

---

## 3. The work — 18 rows, grouped by shared mechanism

Source of truth for each row's note: `docs/WORK_CATEGORIES.md` + `docs/TRIAGE_NOTES.txt`.
**Group them.** Rows sharing one observer are one spec, not N specs.

### Group A — yellow-flag clear (T2, 3 rows)
`PWS_YELLOW04` sdt 142 eid 133 · `PWS_YELLOW05` sdt 144 eid 135 · `PWS_YELLOW06` sdt 146 eid 137
Human note: currently fires when a sector yellow is *raised*; wanted on
**1→0 transition** (flag clearing), per sector. Find the per-sector flag state,
confirm its live values, specify the edge-detect + latch.

### Group B — player terminal damage / retirement (T2, 2 rows)
`PWS_RETIRE02` sdt 118 eid 117 · `PWS_RETIRE03` sdt 119 (**no eid — check `orphaned_clips.json`**)
Note points at `damage.ini` engine seizure. Prior art exists: search findings for
`ReasonCode`, `retire`, `engine_blowup`. Row 119 having no eid is the interesting
part — say what that implies for wiring it at all.

### Group C — AI tyre degradation (T2, 2 rows)
`PWS_AITYRES02` sdt 231 eid 218 · `PWS_AITYRES01` sdt 232 eid 219
Note says AI tyre wear is far slower than the player's and is set in `.tbc`.
**Relevant prior art you must read first:** the player-side equivalent
(`PWS_TYRES01/02`) is solved — trigger is in `sub_438130`, per-wheel tread at
`this[0x394]`, thresholds `0x59E6E8` (0.7) / `0x59E8C8` (0.4), and a patch named
`blister-tolerance` is already registered. Determine whether the AI path reuses
that same code (it is per-wheel physics, so it may) — if so this is T1, not T2.

### Group D — AI spin / off-track / incident (T2+T1, 3 rows)
`PWS_AIOFFTRACK0` sdt 237 eid 224 · `PWS_AIINCID01` sdt 248 eid 233 · `PWS_AIINCID08` sdt 243 eid 229
Notes: 237 = spin causing yellow, plus the known bug that a *pitting* AI slowing
below ~80 km/h raises a yellow; 248 = grid stall, note points at `StartStalls`
in `.rcd`; 243 = "cars all over the place", note says reuse red_flag detection.

### Group E — battle / holding position (T2, 1 row)
`PWS_OD_HOLDING` sdt 244 eid 230 — gap < 0.5 s sustained 5 s+. Needs the live
gap/interval field. Find it, verify live, state the sample rate you need.

### Group F — T1 "already exists, verify and wire" (8 rows)
These the human believes are already present or already have a mechanism. Your
job is **verify, then wire** — not redesign. For each, first answer: *does it
already fire, and if not, what exactly blocks it?*

| Clip | SDT | EID(s) | Human's pointer |
|---|---:|---|---|
| `PWS_PIT06` | 50 | 51, 356 | red_flag NUMPAD2 already detects pit-lane entry — copy it |
| `PWS_SLOWCAR01` | 136 | 132, 427 | reuse the faster-AI-car detection mechanism |
| `RE_PD_DQ` | 158 | 149, 479 | stop/go not served within ~3 laps; believed done already |
| `PWS_AIMECHPROB1` | 221 | 203-208, 449-453 | engine smoke; "played already I think" |
| `RE_OD_PUNCTURE` | 223 | 210, 458 | F1CUI mod already does AI punctures — review how |
| `RE_OD_STUCK_7` | 224 | 211, 462 | stuck in 7th; "also plays already" |
| `PWS_2001_221` | 298 | 280 | rain starting; F1CUI radio messages solved weather |

(That is 7 clips; the 8th T1 row is `PWS_AIINCID08`, handled in Group D above.)

Note `PWS_AIMECHPROB1` and `PWS_AIMECHPROB0` are **truncated names covering
several distinct lines each** (gear-loss variants). Check `sdt_index` against
`data/full_event_dump.json` — do not treat one row as one feature.

---

## 4. Deliverables

One spec file per group in `docs/`, named `SPEC_EVF-002<letter>_<slug>.md`.
Each uses the EVF-001 output contract: verdict / evidence / offsets / trigger /
**non-empty uncertainty**. Plus per group:

- every number cited to a store, a `.c` file:line, or a live read you ran
- explicit `LIVE READ REQUEST` blocks for anything needing another session state
- verdict per row: `WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN`
- **reclassification is welcome** — Group C may well be T1; say so with evidence
- one `findings.jsonl` entry per group, tier ≤ DERIVED, `needs_review: true`

**Do not** change `data/progress.json`. **Do not** hand-edit `PROGRESS.md` or
`WORK_CATEGORIES.md` (generated). **Do not** apply patches.

## 5. Order of work

Group C first (highest chance of collapsing to T1 via existing prior art), then
F, then A, then D, E, B. Stop and report after Group C if its answer changes the
shape of the rest.
