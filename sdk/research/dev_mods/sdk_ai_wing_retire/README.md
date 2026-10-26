# ai_wing_retire — investigation: does losing a wing retire an AI car?

**Status: INVESTIGATION ONLY — no working mod yet.** This folder is the
live-testing record behind the eventual "player picks an AI car and forces
it to retire" mod. It answers *what the game actually does*, not yet *how
to force it externally*. Read `mods/_README.md` first — this supersedes
nothing there, it extends `ai_pit_retire/`'s open question with a
confirmed root cause.

Trust tiers per `CLAUDE.md`: FACT (poke-proven) > CONFIRMED (2 sources) >
DERIVED (grounded, unproven) > GUESS. Everything below is FACT/CONFIRMED
unless marked otherwise — it was read live off a running game, repeatedly.

## The question that started this

Original goal: mod lets the player pick an AI car and damage it into
retiring. First problem hit: `RetiredFlag` (veh+0x4410) and
`FinishState` (veh+0x4064) are recalculated every frame by
`AIDriverStructInit_00424790` — an external poke of either does not
persist (see `skeletons_filled/AIDriverStructInit_00424790.md`). This
matches `ai_pit_retire/README.md`'s own finding: writes land
(`retired 21 AI cars`) but the desired in-game outcome was never
confirmed — "suspect game logic never reads it, or reads a different
field."

**This investigation's answer: the game reads it fine, but only once its
own decision logic (below) has actually run.** A raw poke of
`FinishState=2` bypasses that logic entirely, so nothing downstream ever
reacts to it — consistent with `ai_pit_retire`'s suspicion, now with a
mechanism attached.

## Confirmed facts, in order of how a retirement actually happens

1. **Collision occurs.** Two fields move on every single collision
   regardless of what breaks: a small hit-counter cluster at
   `veh+0x33F4/0x33F8/0x33FC` (int32, undocumented in name_index/
   skeleton_facts — likely a computed/indexed array access, invisible to
   static analysis) and a probably-unrelated triad at `veh+0x12B4/0x188C/
   0x15A0` (the last of which is a documented, unrelated UI field —
   treat this triad's correlation as coincidental, not causal).

2. **Severity is classified via `ReasonCode` (veh+0x4068, CONFIRMED).**
   `mods/ai_pit_retire/README.md` documents the full enum:
   `1=Engine 2=Gearbox 3=Clutch 4=Suspension 5=Brakes 6=Accident
   7=Clutch 8=Electronics 9=Fuel`. Live-tested this session: **wing loss
   alone (front or rear, light or heavy, 7+ trials across 5 different
   drivers) never once set ReasonCode away from 0.** It only fired
   (`=6`, Accident) on the one hit that also cost a wheel. So
   `ReasonCode=6` tracks wheel/suspension-level accident damage, not
   wing loss — wings are cosmetically/aerodynamically modeled but
   apparently don't drive this classifier on their own.

3. **A pit order gets issued regardless of severity.** `callToBox`
   (veh+0x454C, CONFIRMED) + `pittingStatus` (veh+0x3450, CONFIRMED) both
   flip to 1 on every damaged car tested (Frentzen, Webber, Raikkonen,
   Salo, Sarrazin — 5/5), and stayed 0/0 on every clean control car
   checked simultaneously (M. Schumacher, R. Schumacher, Coulthard —
   3/3). This is a solid, reproducible "this AI car has been sent to pit"
   signal, independent of cause.

4. **The retire-vs-repair split is real but its trigger is still
   unlocated.** Empirically confirmed by direct observation (not derived
   from memory): **rear wing loss → AI always retires. Front wing loss →
   AI is typically repaired and continues.** Every struct field checked
   produces an *identical* signature for front and rear wing loss — the
   actual "which wing is gone" check was not found in any polled offset.
   Leading unconfirmed theory: it's a live query against the 3D
   scene/physics attachment state at decision time (e.g. "does the
   rear-wing node still exist"), not a persistent struct flag — which
   would explain why diffing memory before/after never finds it no
   matter how many trials run.
   - `pitState` (veh+0x4040) 1-vs-2 was hypothesized as this split, then
     **falsified**: a confirmed-retiring car (Raikkonen) read `pitState=2`,
     identical to a confirmed-repairable car (Webber). Most likely two
     sequential phases of the same pit-entry state machine ("called" vs
     "en route"), not an outcome split. Don't re-propose this.
   - `retirementPending` (veh+0x39FA, a byte documented in
     `PitStopDecisionTireCompound_0042a3d0`'s field table as "non-zero =
     retirement trigger") was watched live via a Frida write-monitor
     through two full, confirmed retirements — **zero writes fired**, and
     it read 0 even after both cars were fully parked. Likely ruled out,
     but the write-tracer used has a known one-shot-per-page limitation
     (see Tooling notes), so treat as probable, not certain.

5. **In-field terminal damage produces a distinct, delayed intermediate
   state**, different from a pit-lane retirement: `ActiveFlag: 1→0`,
   `Speed→~0`, `CurrentGear→0`, `pitState` resets to 0 (never having
   progressed toward the pit lane) — **but `RetiredFlag` and
   `FinishState` stay at 0** even though the car is fully stopped and
   deactivated. Retirement processing is asynchronous; it does not fire
   the instant the car stops. (Session ended on a game crash before this
   specific car's actual transition to `RetiredFlag=1` was caught —
   pick this back up as the next concrete experiment.)

6. **Confirmed, clean end-state once a retirement fully completes**
   (observed on two cars that reached the garage): `RetiredFlag=1`,
   `FinishState=2` (DNF), `ActiveFlag=0`, `CurrentGear=0`, `callToBox=0`
   (reset after the fact) — compared live against a still-racing control
   car showing `0/0/1/7/0` respectively. Notably `RetiredFlag` held at 1
   rather than resetting, which appears to contradict
   `skeletons_filled/AIDriverStructInit_00424790.md`'s note that it
   "recalcs every frame" — more likely that reset is itself gated on
   `ActiveFlag`/session state and simply doesn't fire once a car is
   genuinely deactivated. Worth a follow-up note in that skeleton fill if
   independently reconfirmed.

## Corpus correction made this session

`tirePuncture_RR/RL/FR/FL` (veh+0x363C/0x3734/0x382C/0x3924,
`name_index.json`) were tagged as if the game simulates punctures
natively. **Correction (authoritative, from the corpus owner): the game
does not simulate tire punctures. F1CUI fakes a puncture by externally
driving these tire wear-rate fields to minimum.** The offsets are still
correct (real wear-rate fields); only the "puncture" semantic was wrong.
Annotated with a `note` field on all four `name_index.json` entries,
trust tier left CONFIRMED. Live-tested this session: these fields moved
by ~0.0001 on a real severe collision with no external tool running —
consistent with ordinary physics jitter, not a real puncture-scale event.

## Open items (pick up here)

1. **Locate the actual front/rear-wing decision.** Static diffing has
   exhausted itself — every trial produces the same signature. Needs
   either (a) hooking whatever function calls
   `PitStopBlockingCondition_0042aee0` / sets `RetiredFlag`/`FinishState`
   for real (writer never identified) and inspecting what it reads at
   that exact moment, or (b) confirming the "queries scene/physics
   attachment state live" theory by finding the wing attachment-point
   object in the 3D/collision scene graph, if one exists.
2. **Confirm the in-field-terminal-damage → RetiredFlag transition
   directly** (item 5 above) — session ended on a crash before this was
   caught. Use `tools/watch_retire_transition.py` on a freshly
   immobilised car and let it run until the flip happens.
3. **Re-verify the `retirementPending` negative result** with a tracer
   that survives repeat writes (see Tooling notes below) rather than the
   one-shot tool used this session, since the current negative result
   is suggestive, not proof.

## Tooling in this folder

Run everything from the corpus root (`%USERPROFILE%\Desktop\f1c_ida_decompiled`)
with `%USERPROFILE%\AppData\Local\Python\bin\python3.exe`, game already running.

- **`tools/trace_ai_damage.py`** — poll a vehicle struct window, print any
  changed offset auto-tagged against `name_index.json`. Read-only,
  polling-based (has the timing/noise weaknesses described in
  `llm_feedback/AI_RETIRE_MOD_FINDINGS.md`).
  ```
  python mods\ai_wing_retire\tools\trace_ai_damage.py list
  python mods\ai_wing_retire\tools\trace_ai_damage.py 9
  python mods\ai_wing_retire\tools\trace_ai_damage.py 9 --lo 0x3000 --hi 0x4900 --hz 8
  ```
- **`tools/trace_write_source.py`** — instruction-level write tracer via
  Frida `MemoryAccessMonitor`; resolves the writer's instruction address
  for a small, fixed set of offsets on one slot. **Does not re-arm** —
  reliably catches only the *first* write to a given offset per process
  lifetime (`MemoryAccessMonitor` is one-shot per page). Safe, never
  crashed anything.
  ```
  python mods\ai_wing_retire\tools\trace_write_source.py 9 0x39fa 0x4068 0x454c
  ```
- **`tools/trace_broad_write.py`** — same idea over a wide offset range,
  fixed to re-arm on every hit so it survives repeat writes. **Do not
  run this on a frequently-written region** (e.g. anything touched every
  frame by physics) — an earlier version of this exact approach crashed
  the game once this session by racing the game's own writes on a hot
  page. Only verified safe on narrow, infrequently-written offsets after
  the fix; treat as unverified for hot ranges.
  ```
  python mods\ai_wing_retire\tools\trace_broad_write.py 9 --lo 0x39f0 --hi 0x3a00 --warmup 3.0
  ```
- **`tools/watch_retire_transition.py`** — simple continuous poller (not
  Frida) for one slot's retirement-relevant fields plus every
  undocumented offset found in the terminal-damage trial; prints
  timestamped changes until Ctrl+C. Safest option, use this first for
  open item #2.
  ```
  python mods\ai_wing_retire\tools\watch_retire_transition.py 4
  ```
- `snapshots/*.json` — raw before-state captures from this session's
  paused-snapshot trials (one file per car/attempt), kept for reference;
  each is a `{offset: uint32}` dump of a vehicle struct (0x0-0x48C0 or a
  band of it) taken while the game was paused, immediately before a
  specific hit. Regenerate one for a new car with:
  ```
  python -c "
  import sys, json; sys.path.insert(0,'mods/ai_wing_retire/tools')
  from trace_ai_damage import Game, VEH_BASE, VEH_STRIDE
  g = Game(); base = VEH_BASE + <SLOT>*VEH_STRIDE
  snap = {off: g.read_u32(base+off) for off in range(0x0, 0x48c0, 4) if g.read_u32(base+off) is not None}
  json.dump(snap, open('mods/ai_wing_retire/snapshots/<name>_before.json','w'))
  "
  ```

Full raw session transcript and the process-improvement critique (what
made this slow, what would make it faster) is in
`llm_feedback/AI_RETIRE_MOD_FINDINGS.md`.

## Session update — player-car wheel detach (which-wheel identity still unresolved)

Live-tested on the player car (slot 0), full-struct diff scan (0x0-0x48C0,
8Hz) across a crash that took off both wings + one wheel, ground-truth
confirmed visually by the corpus owner: **RF (right-front) wheel lost.**

**Confirmed generic damage signal (FACT):** `carConditions` (veh+0x3D6C,
already CONFIRMED in `f1c_sdk.h`) flips `0→2` a few ticks after the crash —
reproduces the same nonzero-on-damage behavior seen on AI cars.

**Candidate corner-array `0x36B4/0x37AC/0x38A4/0x3970` — ruled out as a
clean per-wheel index.** If this were a 4-wide FL/FR/RL/RR array, losing
one wheel should zero exactly one slot. Instead **three of the four**
(`0x37AC`, `0x38A4`, `0x3970`) hit exactly 0 simultaneously and later
recovered together through an identical decay curve (repair/respawn);
`0x36B4` never zeroed at all, just drifted on its own separate downward
trend. Three-of-four moving together for a single wheel loss is
inconsistent with per-corner indexing — more likely these track
body/wing contact-zone data (matches `CollisionSparkParticleEffects`'s
documented "8 body + 4 wheel zones", i.e. this range likely straddles
zone types rather than being wheel-only) or something generic to
being airborne/off-track, not wheel presence.

**Other crash-tick transitions seen** (`0x344C` 1→0, `0x40DC` 1→0,
`0x4544` 0→1, `0x4538`→99999 sentinel, `0x4458` 0xFFFFFFFF→0) — all
single scalars, not 4-wide, so none can encode "which of 4 wheels"
either; consistent with them firing identically regardless of which
part was actually lost (same "identical signature" pattern already
documented above for front-vs-rear wing).

**Conclusion at the time:** with RF confirmed as ground truth and still
zero fields isolating to it specifically, "which wheel/wing is missing"
looked like it genuinely isn't stored in a polled vehicle-struct dword.

## Follow-up — cross-check trial resolves it partially (CONFIRMED)

Second live trial, same player slot, same full-struct diff scan: this
crash took off FW+RW only (**no wheel** — confirmed by eye, corner
intact). Result, compared against the earlier wheel+wings crash:

- **`carConditions` (veh+0x3D6C, already CONFIRMED in name_index.json)
  is a severity level, not a boolean.** Wings-only crash: `0→1`.
  Wheel-loss crash: `0→2`. Two independent trials agree — **value `2`
  specifically marks wheel loss, `1` marks wing/panel-only damage.**
  Promoted with an evidence note on the name_index entry.
- **The five scalar flags from the wheel-loss trial
  (`0x344C` 1→0, `0x40DC` 1→0, `0x4544` 0→1, `0x4538`→99999 sentinel,
  `0x4458` 0xFFFFFFFF→0) did NOT fire on this wings-only crash at all.**
  Confirms they're wheel-loss-specific triggers, not generic
  crash/damage noise as suspected earlier — upgrade from "candidate" to
  CONFIRMED wheel-event markers (2 independent trials: present when a
  wheel is lost, absent when it isn't).

**Still unresolved at the time: which of the 4 wheels.** All the above
fields are scalars (fire once per event), not 4-wide, so none of them
can encode *which* corner. The corner-identity theory (open item #1:
live query against the 3D/physics attachment scene, not a struct flag)
still stood.

## Follow-up 2 — found a real 4-slot wheel-attachment array (paused snapshot diff)

Switched method: pause the game, snapshot the full player struct
(0x0-0x48C0) before and after a controlled crash, diff offline (no
physics-tick noise at all, unlike the earlier live 8Hz scans). This
surfaced a genuinely new candidate, distinct from the noisy
`0x36B4/37AC/38A4/3970` region above (that region is now ruled out —
it's just continuous suspension-travel telemetry, not identity; it
changes on every hit regardless of what broke).

**Structure:** 4 slots, stride `0xF8`, 3 fields each
(`0x35D4/36CC/37C4/38BC` = slot bases). The **third field of each
triple** (`0x35DC`, `0x36D4`, `0x37CC`, `0x38C4`) behaves like a clean
attachment flag: reads `1.0` when the corresponding wheel is attached,
drops to exactly `0` when it's lost — much cleaner signal than
anything found before (no noise, no gradual decay, no revert-on-repair
seen yet).

**Trial A** (FW + both wheels lost + engine destroyed/ignition off):
3 of 4 slots (`35DC`, `36D4`, `38C4`) hit exactly 0; `37CC` stayed
~0.98 (alive). Only 2 wheels were actually lost this trial, so the
extra zeroed slot is most likely explained by the simultaneous **engine
destruction**, not a third wheel — can't fully rule out until isolated.

**Trial B** (wings only — FW+RW lost, both wheels intact, engine
fine): **all 4 slots stayed ~0.99-1.0, none zeroed.** Confirms this
array is wheel-specific and doesn't react to wing loss at all — cleans
up Trial A's ambiguity (wings don't touch it, so the 3rd zero there
wasn't a wing effect either).

**Net:** high confidence this is the real per-wheel attachment array.

## Follow-up 3 — LR wheel isolated via damage.ini edit, partial index mapping resolved

Corpus owner edited `SeasonData/damage.ini` (`FrontWingDetach`/
`RearWingDetach` set to 1e8, `FrontWingRandom`/`RearWingRandom`=0,
`WheelRandom`=1.0 — wings effectively undetachable, wheel loss
guaranteed on impact past `WheelDetach=3500.0`; see
`VehicleDamageModel_004f24f0` for the loader that reads these keys into
the per-vehicle damage-constant struct read every frame by
`CollisionSparkParticleEffects_004ec1a0`). Confirmed visually: **left-
rear (LR) wheel lost, nothing else.**

Paused snapshot diff, only **2 of 4** slots zeroed this time
(`idx1`=`0x36D4`, `idx2`=`0x37CC`), not 1 — but cross-referencing
against Follow-up 2's trial (2 unspecified wheels + engine destroyed,
which zeroed `idx0`, `idx1`, `idx3`) resolves it: **`idx1` zeroed in
BOTH trials despite different wheels lost** → `idx1` is most likely a
shared/general "some wheel is gone" flag, not corner-specific. The
*other* zeroed index each trial is the actual corner:

- **`idx2` (veh+0x37CC) = LR**, confirmed this trial (only extra zeroed
  index, matches the one visually-confirmed lost wheel).
- `idx0` (veh+0x35DC) and `idx3` (veh+0x38C4) = 2 of {FL, FR, RR} from
  the earlier trial — not LR (idx2 stayed alive there) — but which two
  of those three still unresolved.

`carConditions` (veh+0x3D6C) confirmed `→2` for the 4th straight
wheel-loss trial now — treat as FACT-grade at this point.

**Still missing:** FL vs FR vs RR mapping for `idx0`/`idx3`, and
confirmation that `idx1` really is generic (vs. coincidence across 2
trials). One more clean single-wheel isolation (ideally FL or FR)
would settle it.

## Follow-up 4 — RF isolated, `idx1`-is-generic now 3-for-3, corner mapping still messy

Found the actual file the parser reads for wheel-only testing:
`SeasonData/Vehicles/damage.ini` (NOT `SeasonData/damage.ini` — that's
a different/unused copy, editing it had zero effect, wings kept
detaching regardless). Real vehicle-scoped file has
`FrontWingDetach=1500.0`, `RearWingDetach=1500.0`,
`WheelDetach=6500.0`, `WheelRandom=0.75` — edited wings to 1e8/0.0
random, `WheelRandom` to 1.0, restarted game to reload. Confirmed
visually: **RF (right-front) wheel lost, nothing else.**

Result: `idx0`=0.984 (unchanged), **`idx1`=0.0 (zeroed)**,
`idx2`=0.352 (partial, not a clean zero), `idx3`=0.980 (unchanged).
`carConditions` confirmed `→2` again (5th straight trial, treat as
settled FACT).

**`idx1` zeroed for LR (Follow-up 3) AND RF (this trial) — different
corners, same index zeroing both times.** This kills the idea that
`idx1` is corner-specific and strengthens it as a **generic "a wheel is
gone" flag** (now 3-for-3 across all wheel-loss trials: the 2-wheel one
in Follow-up 2, LR, and RF).

**Corner-specific signal is messier than Follow-up 3 suggested:** LR
gave a clean `idx2→0.0`; RF gives `idx2→0.352`, not a clean zero, and
no other slot fully zeros either. Don't trust `idx2=LR` as settled —
either (a) the paused snapshot caught this mid-transition rather than
at rest, or (b) `idx2`'s clean zero for LR was partly coincidental and
it isn't a clean per-corner flag at all. Needs a repeat LR trial and a
repeat RF trial (same corner twice each) to check whether the readings
are reproducible before trusting any corner-specific mapping from this
4-slot array. Don't re-derive the file-location confusion above —
always edit `SeasonData/Vehicles/damage.ini`.

## Follow-up 5 — "idx1 is generic" theory falsified; these are likely continuous load values, not discrete flags

FL trial. **Pre-crash baseline this time was NOT clean 1.0** on any
slot (`idx0`=0.977, `idx1`=0.970, `idx2`=0.804, `idx3`=0.325) — car was
evidently still settling/leaning when the "before" snapshot was taken,
unlike prior trials that happened to catch it flat.

Result: `idx0`=0.0 (**clean zero** — candidate FL), `idx1`=0.984
(recovered, did NOT zero), `idx2`=0.995 (recovered), `idx3`=0.995
(recovered). `carConditions` confirmed `→2` again (6th straight trial,
fully settled at this point).

**This breaks the `idx1`-is-generic theory (was 3-for-3, now 3-for-4).**
Combined with the pre-crash baseline sitting well below 1.0 on every
slot before any damage occurred, the simpler and more likely
explanation: **these 4 fields are continuous per-wheel load/contact
values that normally sit near 1.0 at rest but drift under body
roll/lean/suspension travel, and only reliably read exact `0.0` when
that specific wheel is physically gone.** The "idx1 always zeroes"
pattern across the LR and RF trials was most likely coincidence
(generic crash-moment lean happened to unload that corner both times),
not a true generic flag.

**Revised, more cautious read:** trust ONLY exact-zero readings taken
from a clean (flat, fully-settled, ideally stationary) baseline as
corner-loss signals; don't trust nonzero deltas as meaningful.  Current
tally of clean isolated zeros: `idx2`=LR (Follow-up 3), `idx0`=FL (this
trial) — RF never produced a clean solo zero (closest was
`idx2`→0.352, likely noise, RF mapping still open). Need: (1) a
repeat, clean-baseline RF trial to see if it reproduces a solo zero
somewhere, (2) an RR trial, (3) ideally snapshot only when the car is
fully stationary and flat (parked, not mid-spin/lean) to kill this
baseline-noise problem for good.

## Follow-up 6 — RR trial with a genuinely clean baseline: theory falsified, dead end closed

Did it properly this time: car fully stopped, game paused, baseline
snapshot confirmed all 4 slots read exactly `1.0` (raw int
`1065353216`, bit-exact) before the crash. Confirmed visually: **RR
lost, nothing else.**

Result: `idx0`→**0.0** (same index that gave a clean zero for the FL
trial!), `idx1`→0.9998 (unchanged), `idx2`→0.9919 (unchanged),
`idx3`→**0.0** (a second zeroed index, never seen zeroing in any prior
trial). `carConditions` confirmed `→2` again (7th straight trial).

**This falsifies the corner-mapping theory outright.** `idx0` cannot be
both "FL" (Follow-up 5) and "RR" (this trial) — and this was done with
a bit-exact clean baseline, so it isn't a timing/noise excuse this
time. Across all wheel-loss trials so far (2-unspecified-wheels+engine,
LR, FL, RR) the *set* of indices that hit exact 0.0 varies in ways that
don't fit any fixed 4-slot corner assignment.

**Conclusion, don't re-open this array as a lead:** `veh+0x35DC` /
`0x36D4` / `0x37CC` / `0x38C4` are NOT a stable per-corner
attached/detached flag array. Best working theory: these are
instantaneous suspension-load/contact-force readings that happen to
hit exact 0.0 depending on impact geometry and chassis-flex coupling
that specific frame (which wheels get momentarily unloaded/overloaded
in a crash varies with impact angle, not just which wheel physically
detached) — not identity data at all. Matches the wing-loss
investigation's original theory (open item #1): corner/part identity
is most likely resolved live against the 3D/physics attachment scene
graph, not any polled vehicle-struct dword. Stop looking for it in this
region; if corner-identity is ever needed for a mod, the collision/
scene-graph side is the only remaining lead.

**What DID come out of this whole investigation as solid, reusable
FACT-grade findings:** `carConditions` (veh+0x3D6C) as a
damage-severity level (1=wing/panel, 2=wheel — 7/7 trials agree); the
real damage-config file is `SeasonData/Vehicles/damage.ini` (not
`SeasonData/damage.ini`, which has no effect); and
`VehicleDamageModel_004f24f0` as the parser wiring `damage.ini` keys
into the per-vehicle collision-threshold struct read by
`CollisionSparkParticleEffects_004ec1a0`.
