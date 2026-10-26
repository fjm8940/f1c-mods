# Pit Stop All-Car Trace — 2026-07-29

Addendum to `docs/PIT_STOP_COMPREHENSIVE_ANALYSIS.md`. That doc was built from
the player's own stops only. This trace ran `tools/trace_pitstop_allcars.py`
(new, this session) against **every active car** for 15 minutes unattended,
catching one full AI stop (Nick Heidfeld) and one full player stop, both
successful. Raw log: `docs/pitstop_allcars_trace.txt`.

Method: full 0x48C0-byte struct diff, every 4-byte-aligned dword, but ONLY
while `pittingStatus` is 2 (stopped at the stall search) or 3 (stopped in
box) — the two states where the car is physically stationary. States 1/4
(driving) still carry full physics noise and were left to the existing
narrow filter (`trace_pitstop_full.py`'s -8..12 range), which is why this
run only shows stationary-state detail.

## 1. NEW subsystem found: stall search / ring-buffer countdown

Not in the existing doc at all. Fires only for the **AI** car (Heidfeld),
between entering the pit lane and the car settling into its stall — the
exact window `PitTimingUpdate` / `PitStallProximityCheck` / `PitStallRingBuffer`
were already named for in section 4 of the main doc, but with no concrete
offsets. Now has them:

| Offset | Behavior observed | Likely role |
|---|---|---|
| `+0x42E4` | Counts down 11→1, one step per ~0.2-0.25s | Ring-buffer search index, decrementing |
| `+0x42C8` | Counts down 10→-1 (right after 0x42E4 bottoms out) | Second countdown, candidate-stall index |
| `+0x42D0` | `5→0` once, at the same tick 0x42C8 hits -1 | Stall slot chosen |
| `+0x42E8` | `1→0` at the same tick | Search-complete flag |
| `+0x41B4` | `1→0` then `0→-1` then `-1→0` (toggles across the window) | Unclear — possibly a lock/busy flag on the ring buffer |
| `+0x42CC` | `0→1` then `1→0` then `0→1` (toggles in step with 0x41B4) | Paired with 0x41B4 |
| `+0x4544` | `0→1` once, after the countdowns finish | Stall-assignment-complete flag |
| `+0x4060` | Increments `9→10→11` across the window (position field, coincidental — this is the CONFIRMED `Position` offset, moving because the field is reused as a scratch counter here, or genuinely the session position changed mid-search) |

This did NOT appear in the player's own trace — plausible since the player's
own stall is presumably not searched via a ring buffer (`PitStopReadyCheck`
handles the player specially per the main doc's Section 5a: velocity check +
`PitStopDecisionTireCompound`, no mention of a stall search). Consistent with
the AI-only `PitStallProximityCheck` reading a `PitStallRingBuffer`.

**Caveat**: only one AI stop was captured. Not cross-checked against a
second AI car yet.

## 2. NEW: three-part count+flag cluster, fires on EVERY box-stop entry

Confirmed in BOTH stops (player and AI), same shape, at the exact tick
`pittingStatus` goes 2→3:

| Object A | Object B | Object C | Stride |
|---|---|---|---|
| `+0x12B4` (count, N→N+1) / `+0x12B8` (flag, 0→1) | `+0x15A0` / `+0x15A4` | `+0x188C` / `+0x1890` | 0x2EC between each pair |

Not in the main doc's offset table at all. Exactly 3 records, no 4th at the
predicted next stride (`0x1B78`) — checked, absent. Three is suggestive of
front wing / rear wing / one more inspectable part, or three wheel corners
excluding one, but nothing in the struct confirms which. The count field
increments by exactly 1 per visit (Heidfeld: 4→5, player: 5→6 — different
starting values because they'd each already made prior stops this session),
so it behaves like a **per-part inspection or service counter**, distinct
from the already-documented `+0x406C` (overall pit-visit counter).

Given it fires identically on two different successful stops with no
variance, this looks like normal per-stop bookkeeping, not anything
failure-related.

## 3. NEW correlation: DriverStatus (+0x4510) == 4 while stopped in box

`+0x4510` is already CONFIRMED as `DriverStatus` in `name_index.json`.
Live: it went `0→4` at the exact tick `pittingStatus` hit 3 (box), and back
to `0` at the `pittingStatus` 3→4 transition (service complete). A second,
independent, already-trusted field that flags "car is in the box" — usable
as a cross-check against `pittingStatus` in the DLL (two FACT-tier signals
agreeing raises confidence further, and disagreement would be worth chasing).

## 4. NEW correlation: AIControlFlag (+0x45F4) drops during the box stop

`+0x45F4` (CONFIRMED `AIControlFlag`) went `1→0` at box entry (both stops)
and `0→1` at the 3→4 exit transition. Reads as: the car is taken off active
AI/physics control while parked in the box, and handed back control the
instant service ends. Matches `PitStopProgress`'s auto-pilot-byte logic in
the main doc (Section 5d) at the vehicle-flag level rather than the
pit-object level.

## 5. Confirms callToBox clears exactly at box arrival

`+0x454C` (callToBox) `1→0` and `+0x4548` `0→1` both landed on the same tick
as `pittingStatus` 2→3 in the AI trace — the request is consumed the moment
the car reaches the box, matching the main doc's Section 2 code walkthrough
of `PitStateUpdateMachine` case 2.

## 6. Player-only: lollipop drop is a fixed 2.0s lead on the 3→4 transition

Player trace: `+0x3478` (lollipopFlag) went `1→0` at t=748.71s; `pittingStatus`
3→4 landed at t=750.71s — **exactly 2.00s later**. The main doc's own
single-stop trace showed the same shape (lollipop drop at t+22.18s, 3→4 at
t+24.14s = 1.96s later). Two independent stops now agree this is a fixed
lead time, not incidental — likely the same 2-second auto-pilot countdown
documented in Section 5a ("triggers 2-second countdown... `ShortComputation_d3c0`").

## 7. Timing comparison

| Phase | Main doc's example | AI (Heidfeld), this trace | Player, this trace |
|---|---|---|---|
| Lane → box (2→3) | 7.09s | 9.31s (96.84→106.15) | 6.66s (729.21→736.05, first status-2 tick recorded — real entry likely a touch earlier since state was already 2 at trace pickup) |
| Box service (3→4) | 9.96s | 9.02s (106.15→115.17) | 14.66s (736.05→750.71) — longer because this stop did tyres + a large refuel (98→104L) + a wing adjustment, and the game's own duration formula (main doc Section 5b) is `max(refuel, tire, wing, repair) + noise`, so a heavier stop taking longer is expected, not anomalous |
| Box → exit (3→4 to 4→0) | 5.6s (example) | 5.80s (115.17→120.97) | 9.79s (750.71→760.50) |

The AI stop lines up closely with the main doc's original single-stop
example; the player's longer service time is fully explained by the larger
service list, not a discrepancy.

## 8. Pit-failure hunt: still nothing found — and here's why that's expected

Both captured stops were **successful, ordinary services**. No offset
changed in either trace that looks like a rare/conditional failure flag —
everything found (sections 1-6 above) fires identically and predictably on
every stop. That is the expected result whether or not a failure mechanism
exists in code: per the main doc's Section 17, the 21 failure clips
(`PWS_2001_68-79`, `132-136`) are documented as rare/random events with
`blocked: false, cooldown: 1.0` and no known trigger. A failure, if the
mechanism exists at all, requires the RNG to actually roll it — something
neither of these two ordinary stops did. Catching it needs either many more
live stops sampled over a longer session, or finding the roll in static
analysis first (a call site consuming `flt_8AAE74`/`dword_8AAE70`, the same
noise table `PitStopOperationTimer` already uses for duration jitter, is the
most promising place to look statically — not yet done this session).

Static cross-check already done this session (see chat/`findings.jsonl`):
literal-eid grep across all 21 failure eids against every commentary
emission call site in the corpus — zero hits. `RepairServiceStateDispatcher`
→ `sub_41AFD0` was checked by hand and confirmed to be the PART-DETACH
commentary system (eids 106-111 / 221-223), a different system entirely,
not pit-service failures. `PitStopOperationTimer` was read in full (276
lines) — no failure branch, no probability roll beyond the already-documented
noise-jitter additions to duration.

**Bottom line: no trigger found, and the search is not yet exhaustive.**
Next step, if pursued: grep every caller of `flt_8AAE74`/`dword_8AAE70`
(the shared noise table) for a branch that does something OTHER than add
jitter to a duration — that is the one mechanism common to every
sub-service and the most likely place a rare "something went wrong" roll
would live.
