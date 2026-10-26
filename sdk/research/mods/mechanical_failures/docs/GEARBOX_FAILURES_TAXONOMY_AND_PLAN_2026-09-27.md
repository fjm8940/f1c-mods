# Gearbox failures — taxonomy and in-game experience plan (DRAFT, 2026-09-27)

**Status: DRAFT for owner review. Not release authorization, not a spec.**
This is design input for Gate D decisions (D2, D4, D5, D8, D10 of
[SHIP_PLAN_1_0_0_DRAFT_2026-09-26.md](SHIP_PLAN_1_0_0_DRAFT_2026-09-26.md)).
Owner request (2026-09-27): go through the gearbox failures, sort out all the
gearbox-related issues, and draft how the average player will experience
gearbox problems in-game.

Pipeline stage: orient/define. No `PRODUCT_STATUS` entry, no production
artifact. Current control path is the GUI only (hotkeys removed 2026-09-27).

**Owner decisions recorded 2026-09-27 (GP1–GP7): see §3.7.**

---

## 1. What is already built, sorted

Terminology used here (reconciling the owner's words):

- **gear lost** = the lost-gear(s) family.
- **gear jam / gear stuck** = the same box fault: the selector cannot change
  (our `stuck` mode). A jam and "stuck in 3rd" are not two mechanisms; one is
  the cause, the other is the symptom.
- **no drive** = terminal neutral; nothing engages.

All gearbox mechanics live in `dll\gear_failure.c` on the proven player
`SetGear` detour (`0x447E00`, prologue-guarded 5-byte jump). Everything below
is DRIVER-CONFIRMED 2026-07-30 unless noted (evidence tiers per
[FEATURE_INVENTORY_DRAFT_2026-09-26.md](FEATURE_INVENTORY_DRAFT_2026-09-26.md)).

| # | Fault family | Sub-behaviour | What the driver gets | Evidence |
|---|---|---|---|---|
| G1 | **Jammed selector** (`stuck`) | binary lock; optional explicit target gear, otherwise locks whatever is engaged at arm time. Only ever refuses a change, never causes one | shift refused, clunk sound on every refusal, HUD `GEARBOX FAILING/FAILURE/CRITICAL`, radio "stuck in Nth" (eids 87–93) once total | DRIVER-CONFIRMED (3867 calls, 3862 blocked, "locked perfectly at 1") |
| G2 | **Lost gear(s)** | mask of gears 1–7; two behaviours: **skip past** (walk to next working gear, fall back the other way, all-lost → neutral) or **drop to neutral** (recoverable by selecting another gear) | one gear (or several) will not engage; time cost: first grab spits to neutral, only a second attempt within 1.5 s forces the next working gear; radio "lost Nth" (eids 80–86) | DRIVER-CONFIRMED (broken 2 → sequence 1,3,1,3) |
| G3 | **No drive** (`neutral`) | terminal: every request becomes neutral via the game's own `arg==0` branch; optional auto-recovery timer (`neutral_recover_ms`, 0 = terminal) | engine free-revs, car coasts; HUD `NO DRIVE`; no dedicated radio clip exists in the 80–93 set | DRIVER-CONFIRMED (mode live) |
| G4 | **Combined** (`both`) | stuck + lost | both symptoms together | BUILT |
| — | Warning phase | `progressive` escalation: stuck 20% → 100% → 100%; lost 25% → 60% → 100%, 10–20 s per phase | the box fails intermittently once or twice, then goes solid | DRIVER-CONFIRMED (after "didn't seem to stick" feedback) |
| — | Terminal DNF | `dnf_enable` + `dnf_after_ms`: writes ReasonCode 2 (Gearbox) + FinishState 2 + restart gate, the game runs its own teardown | retirement flow with the game's own screens | BUILT, not driver-tested |
| — | Story surfaces | 14 player clips unblocked (`PWS_MECHPROB20–33`, cooldown `FLT_MAX` → 15 s); HUD via the game message strip; refusal sound `0x4F89A0`; announce only once the fault is real, ~2.5 s later, naming the gear at that moment | engine engineer voice, banner, audible clunk | DRIVER-CONFIRMED (3 clips heard; 11 of 14 not yet ear-checked) |

Controls available through `F1C_GEARFAIL_CTL` (test panel): mode, stuck
target, lost-gear mask, skip vs drop behavior, skip-by, neutral recovery
timer, progressive on/off + phase time, radio delay, DNF toggle + delay, HUD
toggle, commentary toggle, "Say it" clip isolation. All GUI-only now.

Deprecated/out of scope by evidence: the poll-based `mechanical_failures.py`
poked offsets downstream of the decision; every one was live-tested and does
nothing (some caused glitches). Do not rebuild on it.

Adjacent systems that are **not** gearbox code and are not implemented:
clutch (vanilla F1C failure category 3/7), differential/driveshaft, gearbox
oil/overheat, impact damage to the casing, ECU/sensor faults.

---

## 2. Real F1 gearbox failure modes (1999–2002 context) and mapping

The game era is sequential semi-automatic transversal gearboxes with
electro-hydraulic/pneumatic shift actuation. Real mechanisms, what the world
sees, and the in-game status:

| Real mechanism | Real-world symptom | In-game equivalent | Status |
|---|---|---|---|
| Stripped dog rings / worn dogs | "lost third gear", missed shifts, crunch on engagement | G2 lost gear (skip = jumps past; drop-to-neutral = refuses and needs a second try) | **BUILT** |
| Selector barrel/fork jam or breakage | stuck in one gear, cannot select; sometimes locked in a range | G1 stuck | **BUILT** |
| Shift actuator fault (hydraulic/pneumatic pressure loss, solenoid) | shifts stop, shifts go slow/reluctant, sudden false neutral | G3 no drive; **delayed/reluctant shift not built** | **PARTIAL** |
| Gear-position sensor / ECU fault | wrong gear detected, engine cuts, box "confused", jumps to neutral | not safely reproducible from the proven decision point | **OUT of scope** |
| Oil leak / overheat | degrading shifts, smoke, eventual seizure; common TV retirement story | gearbox oil has no game telemetry; faking it would be pure fiction (engine has its own oil-leak glitch) | **DEFER** |
| Input shaft / bearing seizure, cracked casing | total loss of drive | G3 no drive | **BUILT** |
| Differential / driveshaft failure | drive loss on one side or entirely; not a "box" fault | different system, no proven hook in scope | **OUT of scope** |
| Clutch failure | slip, no launch, revs flare | vanilla F1C failure category 3/7; separate product decision | **OUT of scope** |

Driver-history colour worth designing toward: teams telling drivers to
**nurse a sick box to the flag** (avoid a gear, short-shift, no kerbs) and the
drama of "gearbox problem" radio calls before a possible retirement. The
built mechanics already support exactly that story: adapt and lose time, or
limp, or stop.

### 2.1 Real-world modes that are cheap extensions on the proven filter

These need no new addresses; each is a variation of the same decision
intercept:

1. **Reluctant/delayed shift** — refuse the change for a short random window
   (say 150–600 ms), then let it through; reads as a slow, dragging shift.
   Feasibility needs one investigation: whether the game re-issues the same
   request while the driver holds the input (the detour sees ~one call per
   attempt, so this must be checked before promising it).
2. **False neutral as its own fault** — next selection drops to neutral even
   though the game asked for a gear (recoverable). Today this exists only as
   G2's drop-to-neutral behavior; promoting it to a stand-alone weighted
   fault is nearly free. A direction-gated variant (downshift-only) is also
   trivial (the detour sees the shift direction).
3. **Pop-out under load** — gear engages, then drops out under throttle.
   Needs a timed follow-up write or a "next request" trigger; the README's
   "never forge the end state" rule and the 2026-07-30 downstream-write
   crashes make this 1.x material, behind a fresh live investigation.
4. **"Notchy box" warning only** — intermittent refusals with no terminal
   end. This is G1 phase 1 (`20%`) already; it can be shipped as a
   stand-alone lightweight fault if the owner wants more variety.

---

## 3. In-game experience plan (draft)

### 3.1 Experience goals

- Rare, tense, recognisable: roughly one gearbox incident every handful of
  races; most are survivable with adaptation, a minority end the race.
- Never an advantage, never a last-lap sucker punch.
- Everything the player hears/sees maps to a real mechanism: engineer radio,
  HUD banner, audible refusal clunk, visible time loss.
- The player should *feel* the story real drivers tell: "something's wrong
  with the box, manage it."

### 3.2 Trigger model (consistent with ship-plan D4/D5)

1. **Scope:** race and qualifying sessions (SessionType 7 and 5; owner answer
   2026-09-27). Practice and warm-up stay clean.
2. **One roll at race start** (deterministic seed, logged): probability
   `Chance` (default 20%).
3. On fire, **pick a type by weight** (proposal, internal for 1.0):
   lost gear 45%, stuck 30%, false neutral 15%, no drive 10%.
4. **Onset window:** not before `EarliestLap` (default 3); terminal outcomes
   are postponed or downgraded if the car is within the final 2 laps.
5. **At most one player failure per race** (D10 max-count rule).
6. **Escalation unchanged** from the live-proven tables (stuck 20/100/100,
   lost 25/60/100, 10–20 s per phase; stuck may only ever refuse; lost gear
   costs time via the neutral-first grab).
7. **Terminal handling:** no automatic retirement (owner answer 2026-09-27).
   No-drive leaves the car coasting under player control; the player parks it
   and retires when ready. The existing ReasonCode 2 DNF path stays in the dev
   build as an option but is off in 1.0.

### 3.3 What the average player actually experiences

1. Mid-race a shift clunks and refuses, or one gear will not engage. HUD
   says `GEARBOX FAILING`. The car is still fast; the driver starts adapting.
2. Over the next laps it worsens phase by phase. Once the fault is real, the
   engineer names it after ~2.5 s: "lost 3rd gear" / "stuck in 4th gear".
3. The player short-shifts around the lost gear, or lives with the jam and
   waits for a chance to pit; a no-drive car coasts to a stop.
4. Outcome split (by design): most faults are adapt-and-finish; jammed boxes
   may end in a slow crawl the player chooses to retire; a no-drive car coasts
   until the player parks it. No automatic retirement. Retirement commentary
   follows the existing engine-module wait-for-stop pattern when the player
   does retire.

### 3.4 Frequency defaults proposal (for owner tuning)

| Setting | Proposed default | Rationale |
|---|---|---|
| `Chance` per race | 0.20 | one gearbox story every ~5 races |
| Type weights | 45/30/15/10 | lost gear is the most common real-world case; no drive is rare |
| `EarliestLap` | 3 | no instant race-ruining start |
| Terminal last-lap guard | 2 laps | no unfair last-lap heartbreak |
| Max per race | 1 | D10 |

With those numbers the average player meets a gearbox fault in roughly one
race out of five, and loses a race to it roughly once per one to two seasons
of full-length races. All values are INI-editable; owners and mod lists can
turn it off or up.

### 3.5 Proposed INI surface (gearbox section)

```ini
[Gearbox]
Enabled=1            ; 0 = gearbox faults off
Chance=0.20          ; per-race/qualifying probability, 0.0 .. 1.0
EarliestLap=3
TerminalLastLapGuard=2
HUD=1
Radio=1
; terminal no-drive never auto-retires (owner answer 2026-09-27)
```

Type weights stay internal constants for 1.0; the other D10 fields (overall
chance, family weights, max failures) sit at the product level.

### 3.6 Deliberately not in the 1.0 gearbox scope (proposal)

- delayed/reluctant shifts and pop-out (investigation first, §2.1)
- gearbox oil leak / overheat (no telemetry; would be fiction)
- sensor/ECU confusion (unobservable, unpredictable)
- differential, driveshaft, clutch (separate systems, separate decisions)
- pit-lane gearbox repair (real F1 retires; arcade rule deferred)
- AI gearbox failures (vanilla AI already has its own system; staged parity
  is not achievable and must not be promised)

### 3.7 Owner decisions — recorded 2026-09-27

| # | Decision | Owner answer |
|---|---|---|
| GP1 | default `Chance` per race | **20%** |
| GP2 | session scope | **race + qualifying** (SessionType 7 and 5) |
| GP3 | terminal outcome | **never auto-retire** — the player parks and retires when ready |
| GP4 | false neutral as a stand-alone 1.0 fault | **yes** (in the weights, ~15%) |
| GP5 | delayed shift / pop-out in 1.0 | **investigate first; target 1.x** |
| GP6 | radio timing (announce only once the fault is real) | keep as built — drafted default, no objection recorded |
| GP7 | warning-only "notchy box" fault in the weights | out of 1.0 defaults — drafted default, no objection recorded |

### 3.8 Validation plan (feeds ship-plan P5/P6)

- **Unit:** seeded roll distribution vs weights; onset window; last-lap
  guard; max count; zero chance never fires; INI invalid = refuse + log.
- **Live forced-high:** each type triggered in a real race with dev INI;
  radio eids asserted against 80–93 (and never 201–217/340–383); HUD; refusal
  sound; lost-gear time cost; DNF path on the exact candidate hash.
- **Human (Matias):** does frequency feel right over a weekend; does a jammed
  box feel like a jammed box; is the radio line and timing right; is any
  outcome unfair.
- Finish the 11 not-yet-ear-checked radio clips first
  (`f1c_ida_decompiled\mods\extended_vanilla_features\data\progress.json`).

---

## 4. Implementation notes for the executor (later, under Gate D)

- Everything in §3.2's trigger model runs on top of the proven detour; no new
  game addresses are needed for the 1.0 scope.
- Delayed-shift feasibility is the one open technical question (§2.1 item 1);
  resolve it before promising the feature anywhere.
- "No drive" has no dedicated radio clip; options are reusing a lost/stuck
  line or silence — decide with the owner before spec.
- DNF uses the existing ReasonCode 2 path but has never been driven; that is
  a live checklist item, not a rebuild.
- Public build has no dev surface (no shared memory, no unload event) per D8;
  the dev panel stays as the test harness.
- All new behavior lands in `development\mods\mechanical_failures\` and the
  eventual `release_candidates\mechanical_failures-v1.0.0-rc.N\`; before any
  maintained doc edit, preserve its before-state.
