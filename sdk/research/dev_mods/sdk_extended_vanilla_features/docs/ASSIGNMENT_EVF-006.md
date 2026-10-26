# ASSIGNMENT EVF-006 — AI behaviour: incidents, pace, team orders (8 rows)

## Read first, in this order

1. `docs/ASSIGNMENT_PARALLEL_RULES.md` — **you are one of several workers running
   concurrently.** File ownership, live-game lock, findings append rule.
2. `../DEEPSEEK_INSTRUCTIONS.md` — §12z, §12a, §12b, no truncated prior-art searches.
3. `HANDOFF_2026-07-28.md` — **section 0 first.**
4. `../dll/README.md` — **mandatory**, six traps.
5. `WORK_CATEGORIES.md` + `TRIAGE_NOTES.txt` for each row's human note.

**You own:** `docs/SPEC_EVF-006_*.md` and nothing else.

---

## The framing question

Six of these eight rows come down to one question the corpus has never answered:

> **Does the AI have observable pace state at all** — a "pushing / holding /
> easing / pacing" mode the game itself sets — or is every pace line something we
> would have to infer from lap deltas?

Answer that once and six rows resolve together. Answer it wrong and you produce
six specs that all lie. So: **Group B first, and stop.**

---

## The rows

### Group A — AI off-track & spin detection (T5, 3 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_AIINCID06` | 240 | 227 | AI goes off track. Complication: AI routinely straight-lines corners, so naive off-track detection false-positives constantly |
| `PWS_AIINCID03` | 241 | 228 | same |
| `PWS_AIINCID07` | 245 | 231 | has spun **just ahead** — gap to player < 10 s at fire time. Human's spin proxy: under 5 km/h for 2 s+ |

Prior art you must read before deriving: `SPEC_EVF-002D` (AI spin / off-track /
incident) already exists and is audited. **Your job is to extend it, not to redo
it.** If EVF-002D already settles the spin predicate, say so and spend your effort
on the two things it does not cover:

1. the false-positive problem (AI cutting corners normally), and
2. `PWS_AIINCID07`'s **car-to-car gap** requirement.

The gap field is the known unbuildable in this project — `SPEC_EVF-002E` (battle /
holding position) hit the same wall. Check whether EVF-002E found it. If it did
not, `PWS_AIINCID07` goes on your unbuildable list with the search shown; **do not
invent an offset**.

Also **not yours**: the T3 incident clips (`PWS_AIINCID02/04/05`) belong to
EVF-003's `INCIDENT_STATE` observer. Do not spec them.

### Group B — AI pace shifts (T5, 3 rows) — **DO THIS FIRST**

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_MISC29` | 282 | 265 | "is easing off". What determines AI pace shifts? Anything in the AI driving-skill fields that actually changes? |
| `PWS_MISC26` | 285 | 268 | "is pacing himself" — human cannot think of a direct measure |
| `PWS_AIFASTLAP03` | 280 | 263 | all-time race lap record — where is that stored, and what does "all-time" mean here? |

The lead the human gives: **F1CUI's yellow-flag script forbids AI from
overtaking**, so *something* in the AI is switchable. Find what that script
touches; if the AI has a "do not overtake" mode, it very likely has neighbours.

Corpus leads to check, in order:
- `ask.py findings` for `AIAbility`, `pace`, `LivePaceFactor`, `AbilityBaseline`
- `strings_modules.json` for `ai_db.cpp` — the AI-database module's own functions
- `persistence_map.jsonl` for whatever offsets you find. Known trap: the
  safety-car mod's `veh+0x453C` is `RECALCULATED` — AIAbility adjusts it
  ±0.01/frame, so a poke cannot hold and a *read* of it is noisy per frame. If a
  pace field is recalculated every frame, your predicate needs a dwell window,
  not an instantaneous compare. State the window in seconds.

`PWS_AIFASTLAP03` is a different problem — a **persistence** problem ("all-time"
implies across sessions). Check whether the game stores a track record anywhere
(`.gdb`/`.plr` keys in `data_index.json`). If it does not, say the honest thing:
"all-time" can only mean "since this DLL loaded" unless we write our own file.
Note the T3 player-side twins `PWS_RACEFAST03` (sdt 70) and `PWS_2001_102`
(sdt 72) have the identical problem and belong to EVF-003 — **flag the shared
answer, do not spec their rows.**

### Group C — team orders (T5, 2 rows)

| Clip | SDT | EID | Human's note |
|---|---:|---|---|
| `PWS_2001_309` | 309 | 288 | "has let his team mate past" — is there anything suggesting AI team orders? |
| `PWS_2001_308` | 310 | 289 | same, from the perspective of the driver now being chased |

The human's own objection is the important part and you must engage with it: in
the top 6, a team-mate pass is usually just racing, and calling it team orders
"might just sound stupid". So the predicate needs to be **conservative enough that
a false positive is rare**, because a false positive under a once-per-race latch
eats the real event (`dll/README.md`).

Minimum viable predicate to evaluate: same constructor (team name at driver-struct
+124, 1060-byte stride), position swap, and *no* accompanying overtake-under-
braking signature — plus whatever pace state Group B turns up. If Group B finds
nothing, say plainly that Group C is not safely buildable and stop. That is an
acceptable outcome.

---

## Specific things not to get wrong

- These are **AI-side** rows: the subject is another vehicle slot, not the player.
  A slot is self-identifying via the ASCII driver name at `veh+0x4620`; a slot is
  live iff the int32 at `veh+0x32C8` != -1. **Never hardcode a grid list.**
- Naming the driver and team in the audio goes through `DriverNameIDLookup`
  (`0x415d90`) and `sub_417640`; see the tables in `../CLAUDE.md`. If your line
  must name a driver, say which eid variant carries the name.
- **NEVER write `veh+0x10C0` or `veh+0x33C8`** — each kills the process instantly.
  You are read-only anyway, but do not spec a write to them either.
- Do not propose a second DLL or tick-hook site. Rules §7.

---

## Deliverables

- `docs/SPEC_EVF-006A_ai_incident.md`, `docs/SPEC_EVF-006B_ai_pace.md`,
  `docs/SPEC_EVF-006C_team_orders.md`
- verdict per clip: `WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN`
- one findings line per group in `batch/evf_pending/EVF-006.jsonl`, tier <= DERIVED,
  `needs_review: true`
- non-empty uncertainty per group; unbuildable rows separate, with your search shown

## Order

**Group B first — specifically the single question "does the AI have observable
pace state" — then stop and report.** Groups A and C both depend on the answer.
