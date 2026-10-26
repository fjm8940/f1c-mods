# Forcing a *genuine* AI retirement — state-copy investigation (2026-07-24)

> **SUPERSEDED 2026-07-24 — read `RETIRE_SOLVED_2026-07-24.md` FIRST.**
> Two claims below are DISPROVED: (a) `0x4354-0x43A0` is the AI racing-line
> waypoint array, NOT the driver-model child array; (b) the "missing
> garage teleport / driver-removal function" hunt is closed — driver removal is
> the byte `veh+0x105D`, the car is held still by `veh+0x1065`, and the garage
> placement is a `PhysX/Y/Z` (`0x1C/0x20/0x24`) write. The *dead-end lists* in
> this file are still valid and worth reading.


Goal: a script that makes an AI driver suffer a mechanical failure and **actually
retire** — car stopped, driver out, not "racing with the engine off." Prior
flag-write attempts (`mechanical_failure.py`, `retire_driver.py`) only set the
scoreboard flags and were visually inert; this session found out *why* and mapped
what real retirement actually is.

Method: diff a **genuinely-retired car** (an AI that DNF'd naturally in the live
race — Frentzen, slot 9) against a running car, then copy Frentzen's struct region
onto fresh running cars and observe in-game (human = oracle). All observations are
LIVE, direct-eyeball on a running race (lap 53, Monza, wet).

## Hard result: retirement is a FUNCTION, not a state

The scoreboard flags are **outputs**, not inputs. Confirmed dead lead:

```
veh+0x4064 FinishState   veh+0x4068 ReasonCode
veh+0x4410 RetiredFlag   veh+0x45F4 ActiveFlag
```
Writing these (any combination, incl. ActiveFlag=0) leaves the car **driving
normally with engine off / gear on / driver still in seat**. The game does not
read them back to drive a retirement. Every earlier mod that "worked" was reading
its own writes back via `standings.py` (which reads the same flags) — circular,
not verification. **Do not trust flag readback as proof; the human screen is the
only oracle.**

## Struct region map (LIVE-CONFIRMED by copy + eyeball)

Copying Frentzen's (slot 9, real DNF) bytes onto running cars, region by region:

| Region | Contents | Copy-safe? | Observed effect when copied onto a running car |
|---|---|---|---|
| `0x00–0x27` | identity ints + world XYZ position, embedded handles | NO | corrupts identity / teleports; always excluded |
| `0x28–0x3610` | **rendering / scene-node / model** | YES (pure data) | car **frame goes invisible**; a static leftover body stays at spawn; driver+physics keep driving normally |
| `0x3610–0x45F8` | stop/retire state **+ per-instance POINTERS** (self-refs, list links, AI-target ptrs) | **NO — CRASHES** | dangling pointers; game crashed on next collision (bad-ptr deref) |
| `0x461D+` | name / nationality / team / display-name strings (extend past 0x4700) | NO | identity splice — Coulthard→"H. FRENTHARD, Jordan #20" |

Full-region copy (`0x28–0x4600`, ~903–1084 differing fields, identity excluded)
DID produce **real retirement behavior**: car stopped (0 KMH, gear N), **collision
off — cars drive through it (ghost)**, driver moved into the **spectator/review
pool** (replay camera jumped to the copied car's location). This is everything the
flag-write faked and failed to do — but it also dragged the spectator-location
field and, in the upper half, live pointers.

### Why raw copy can't be the final answer
The upper half (`0x3610–0x45F8`) holds pointers valid only for the source object.
Copying them across cars = dangling refs = crash on collision. The game's own
retire routine fixes up these pointers/links; a byte-copy cannot. **=> the real
mechanism must be triggered via the game's function, not reconstructed by state.**

## ASCII/identity offsets (read-only scan of a clean car, slot 7 Raikkonen)
```
+0x461D "Kimi Raikkonen (2001)"   +0x4660 "Finnish"
+0x4678 "2001 Arrows" (team)      +0x46B8 "Arrows"
+0x46F8 "K. RAIKK..." (display, runs past 0x4700)
```
Exclude `0x461D`..end when copying anything, or you splice driver names.

## Bisection log (each round = one fresh car, human-observed)
- R1 lower half `0x28–0x3610` (542 flds) → slot 6 Ralf: **car frame invisible,
  driver keeps driving** ⇒ lower half = rendering, not the stop.
- R2 upper half `0x3610–0x45F8` (543 flds) → slot 8 Hill: **game CRASHED** on
  collision ⇒ upper half contains live pointers; raw copy unsafe. Bisection of the
  upper half by raw copy is a dead method.

## Next steps (recommended order)
1. **Data-breakpoint the real writer (route 2, preferred).** HW BP on a
   retire/velocity field of a running car; force a natural DNF (damage a car);
   catch the function that performs the pointer-safe teardown. Then a script
   *calls/triggers that function* — the actual clean primitive. Tools:
   frida / x64dbg (already attachable).
2. If continuing state-copy: **pointer-filter** the upper half — skip any field
   whose value is in `0x00400000–0x7FFFFFFF` (pointer range on this non-ASLR
   32-bit exe); copy only floats and 0/1 flags. Lets bisection proceed without
   the crash, but may miss the trigger if it *is* a link the game must set.
3. Cosmetic: stopped car stays visible on track (world-pos `0x1C–0x27` was
   excluded). Use `retire_driver.py`'s scene-node hide (`0x005EA2C0`+344/356,
   Y=−1000) or copy the render region (`0x28–0x3610`, safe) to remove the frame.

## Full struct dissection (2026-07-24, all 22 live cars, paused on grid)
Tools: `dissect_car.py` (v1) / `dissect_car2.py` (v2, clean tags + ptr recursion).
Artifacts: `veh_field_map2.csv`, `veh_children.csv`, `veh_ptr_targets.txt`.

Per-car struct = full **0x48C0** bytes. Classification of the flat block:
- **3647 static** (identical across all cars) = config/shared/unused
- **693 float** dynamic (per-car telemetry; big block ~0x3600–0x4000)
- **57 int** dynamic, **52 string** (inline name/team/nationality at 0x4600–0x4744)
- **207 pointer-ish**; after removing float false-positives, the genuine heap
  child pointers (targets in real heap ranges 0x04Dxxxxx / 0x1Bxxxxxx / 0x1Exxxxxx,
  all per-car `dynamic`) cluster in the **0x066C–0x09xx** band:
  `0x06A8, 0x06EC, 0x06F4, 0x0780, 0x0834, 0x0878, 0x08D0, 0x08D4, 0x08E4,
  0x095C, 0x09A0` → OWNED sub-objects (physics / AI / telemetry). Plus
  `0x45F8 -> heap` in the retire region (likely the R2 crash culprit).
- Self/back-refs: `0x0000/0x000C/0x0014 -> 0x5EC6D8` (the vehicle array base).

**State NOT in the flat block:** (a) the ~dozen owned heap children above — the
flat copy wrote their pointer *values* but never their *targets*, so that state
(real physics/AI) persisted and, when a pointer was copied cross-car, dangled →
crash; (b) tail `0x4700–0x48C0` (F1CUI anchor pointers at 0x4714+, per memory).

Classifier TODO: swap the "readable" ptr test for "target in a real heap region"
(`enum_memory_regions`) to drop ~160 float false-positives. Then dissect each
heap child across cars + IDB-xref `VEH+off` to name each sub-object.

### Child object graph (dissect_children.py, VirtualQueryEx heap-filtered)
Artifact: `veh_children_real.csv`. Each live car owns, on the heap:
- **2 component-pointer lists**: `veh+0x06A8 -> heap` (30 ptrs, all dynamic),
  `veh+0x06EC -> heap` (29 ptrs) — sub-object/component containers.
- **~20 physics/float buffers** (27–29 floats each = 4x4 matrix + vectors, body/
  wheel physics/motion state): `veh+0x08D0,0x08D4,0x08E4, 0x09F8,0x09FC,0x0A0C,
  0x0B20,0x0B24,0x0B34, 0x0C48,0x0C4C,0x0C5C, 0x0BAC,0x0CD4,0x0D18,...` (targets
  in heap ranges 0x04D/0x19/0x1B/0x1E xxxxxx).
These are the per-car motion/AI state the flat block-copy never reached — i.e.
the "soul that kept driving." Retirement must zero/detach these, which is why
only the game's own routine (not a byte copy) does it correctly.
NOTE: rows tagged `mixed flt=0 ptr=0` with `0x3Axxxxxx` targets are float values
that coincidentally land in a committed region — ignore them; trust
0x04D/0x19/0x1B/0x1E targets with high flt/ptr counts.

## LIVE RETIREMENT CAPTURE (2026-07-24, runtime_delta.py, Hakkinen slot 2)
Method: same-session capture. Baseline `pre_hit` → crash Hakkinen (wheel off) →
`after_hit` (damaged) → live-poll caught the retire flip → `retired`. Diffs:
`pre_hit→after_hit` = damage delta; `after_hit→retired` = retire-decision delta.
Captures in `runtime_captures/`; `ai_retire` behavior_delta in `runtime_facts.json`.

**Timed sequence (poll of slot 2):**
1. Crash → `ReasonCode(0x4068) 0→6` (Accident) IMMEDIATELY.
2. **+5.6 s later** a decision function flips `FinishState(0x4064) 0→2` AND
   `RetiredFlag(0x4410) 0→1` TOGETHER. `ActiveFlag(0x45F4)` stayed 0 the whole
   time. **CORRECTION: "ActiveFlag=0 is the kill switch" (old _README) is FALSE.**
   Retirement = FinishState+RetiredFlag flipped by a delayed routine, not ActiveFlag.
   Wheel-loss does NOT instantly retire — retire is a separate, delayed decision.

**Damage delta (pre_hit→after_hit), previously-unmapped damage model:**
- `0x054C–0x0564`, `0x00B0–0x00C0` blocks 0→values; per-wheel triplet
  `0x129C / 0x1588 / 0x1874` = (8.227, 2, 0.0315) ×3 (three surviving wheels).

**Retire-decision writes (after_hit→retired), structural (physics-settling excluded):**
- damage flags CLEARED: `0x00A0,0x00B0,0x00B8,0x00BC,0x00C0 → 0`.
- per-corner PARKING blocks written 0→`[2,float,1.0,1,...]`: `0x042C–0x0464`,
  `0x04AC–0x04C4`, `0x056C–0x0584` (stride ~0x20).
- heap children rewritten: array `0x4354–0x43A0` (~24 child ptrs, ~48–53 inner
  floats each), big objects `0x3674/0x376C/0x3864/0x395C/0x3AA8` (60+ inner),
  tail `0x462C` (64). This is the reposition/remove restructure a flat copy can't do.
- CAVEAT: window = 5.6 s, so physics-settling floats are mixed in; trust the
  0→structured-block writes and damage-clears, not the float-matrix churn.

**Decisive next step:** data-BP on `veh+0x4064` (FinishState) — now known to flip
exactly on retire — to catch the deciding function = the callable retire routine.

## SOLVED — retirement function chain (2026-07-24, live HW-BP + decompile)
Method: x32dbg (via x64dbg MCP) attached to live game; HW write-BP on
`veh[2]+0x4064` (FinishState=0x5F98BC) with condition `[5F98BC]==2` to skip resets
and halt ONLY on the retire write. Crashed Hakkinen → BP caught the writer in the
act. FACT tier (live-proven + decompile-confirmed).

**Call chain that retires a car:**
```
SessionStateCheckWrapper_00461db0 -> sub_4BA460 -> sub_4B3400
  -> PitCrewStateMachine_004bbfc0        (executor; this = veh+0x4034, a2 = state obj)
       this[12]=2   => veh+0x4064 FinishState = 2
       this[13]=..  => veh+0x4068 ReasonCode
       AIStateReset_0042af40(veh)         (teardown)
            ConditionalGate(veh+0x0C)     <- STOPS THE AI DRIVING (the missing piece)
            veh+0x4410 RetiredFlag = 1
            veh+0x45F4 ActiveFlag  = 0
```
`ConditionalGate(veh+0xC)` inside AIStateReset is what every prior flag-poke mod
skipped — it performs the AI-driving/heap-children teardown. That is THE reason
writing FinishState/RetiredFlag/ActiveFlag by hand left the car driving ("soul kept
driving"): the flags are set by these functions AS A SIDE EFFECT of the teardown,
and the teardown (ConditionalGate) is the load-bearing call, not the flags.

**Callable primitive for a mod:** `AIStateReset_0042af40(veh_slot)` — thiscall,
ecx=veh, no stack args; sets RetiredFlag+ActiveFlag AND runs the teardown. Combine
with FinishState=2/ReasonCode (or call PitCrewStateMachine_004bbfc0). MUST run on
the game thread (calls StateChangeNotify/ConditionalGate) — detour hook, not
external poke. This is the clean retire(slot) we were after all along.

FACT offsets proven this session (live BP + decompile agree):
veh+0x4064 FinishState, +0x4068 ReasonCode, +0x4410 RetiredFlag, +0x45F4 ActiveFlag.

## BREAKTHROUGH — TWO retirement types; the pit path we kept missing (2026-07-24)
Captured a REAL natural pit-retirement (Heidfeld slot 15, stalled, Reason=8
Electronics, drove to pits, driver gone) via runtime_delta `pre_wing→wing_done`.
Diffed against our `AIStateReset` stop. They are DIFFERENT mechanisms:

| field | AIStateReset "stop" (accident-kill) | Heidfeld proper PIT-retire |
|---|---|---|
| callToBox 0x454C | 0 (untouched) | **0 → 1** |
| ActiveFlag 0x45F4 | forced 0 (car killed) | **UNCHANGED — stays active, keeps driving** |
| position | frozen | moved (drove to pit) |
| child array 0x4354–0x43A0 | not rewritten | **20 targets rewritten = driver-model removal** |

**We had it backwards.** Proper retirement does NOT kill the car. It sets
`callToBox(0x454C)=1`, KEEPS the car active, lets the AI DRIVE to the pits, then
completes the retire on arrival (FinishState=2, ReasonCode, RetiredFlag=1, and the
0x4354–0x43A0 child-array rewrite = driver disappears). `AIStateReset` kills
ActiveFlag → prevents the pit drive → that is why it only froze the car mid-track,
identical to the old block-copy. Both are the ACCIDENT/kill path, not retirement.
Confirms `ai_wing_retire` note: callToBox/pittingStatus flag a car pit-bound.
QualPos was NOT set to 198 by the natural retire (that is grid_reorder's manual
red-flag trick, not the live mechanism).

**Hypothesis to test:** on a healthy running AI, set `callToBox(0x454C)=1` (leave
ActiveFlag=1) → car drives to pit and retires, driver gone. Clean retire(slot)
primitive, opposite of AIStateReset. (Test pending — game restarted mid-test.)

## COMPLETE pit-retirement sequence (2026-07-24, live trace, Webber rear-wing)
Traced with `tools/trace_retire.py` while clipping Webber's (slot 20) rear wing;
end-state read live. FACT.

**Sequence:**
1. Rear wing lost → game sets `callToBox(0x454C)=1` + `pitting(0x3450)=1` TOGETHER;
   `ActiveFlag` STAYS 1. Car drives to pit under power.
2. On pit arrival → `FinishState(0x4064)=2`, `RetiredFlag(0x4410)=1`,
   `ActiveFlag(0x45F4)=0`, `PitState(0x4040)=2` (="retired in pit"); callToBox,
   pitting, ReasonCode RESET to 0; driver-model child-array (0x4354–0x43A0)
   rewritten → driver disappears.

**callToBox=1 confirmed = "go to pit" for ANY car** (tested on healthy TesterA +
Schumacher — both drove in). But healthy car REJOINS after service; only a car with
TERMINAL DAMAGE (rear wing gone) retires on arrival. => retire-vs-rejoin is decided
by damage severity, NOT by callToBox.

**Recipe to force AI retire-into-pits (driver gone, clean):**
- Authentic: set terminal wing/damage + callToBox=1 → game does the rest.
- Direct: set callToBox=1, then on pit arrival force FinishState=2, RetiredFlag=1,
  ActiveFlag=0, PitState=2 (hook pit-arrival, or poll pitting->PitState).
- Do NOT use AIStateReset / block-copy — those kill the car mid-track (accident
  path), which is the dead end this whole session kept re-hitting.

Open: the exact damage field that flags "terminal -> retire on pit". Next: trace
which field the wing-loss sets that TesterA's healthy callToBox lacked.

## Driver-model removal isolated (2026-07-24, same-car Webber A->B, clean)
Captured Webber (slot 20) natural wing-retire: `webber_A` (in pit lane, driver in)
-> `webber_B` (retired, driver gone). Same car, mostly stationary => low noise.

A->B changes:
- flags: FinishState 0->2, RetiredFlag 0->1, ActiveFlag 1->0, callToBox 1->0,
  pitting 2->0 (the part we already replicate).
- **DRIVER REMOVAL = child array `0x4354-0x43A0` (20 ptrs) targets REWRITTEN** —
  NOT zeroed; each target is a transform matrix rewritten to new world coords
  (parked/retired pose of driver + body parts). This is why forcing the flags on
  Raikkonen left the driver visibly sitting in the car: flags don't run this rewrite.
- also zeroed: per-wheel/damage blocks 0x864/0x8C0/0xAB4/0xB10/0xBDC/0xC38, and
  per-wheel visual triplets 0x12E4../0x15D0../0x188C.. ; 0x105C/0x1068 257->1.

CONCLUSION: driver removal is done by a FUNCTION (the pit-retire completion routine)
that rewrites the 0x4354-0x43A0 transform array — not replicable by flag-writes or
state-copy (per-instance world coords). To get it: HW write-BP on `veh+0x4354`
(or a child target) during a natural pit-retire -> catch the completion function ->
call it after forcing a car to the pit. That + the working callToBox pit path =
fully clean forced retirement (in pit, DNF, driver gone).

STATUS: force-pit-retire WORKS (car retires in pit, DNF, no rejoin). Driver-model
vanish = one more x64dbg BP-trace on the 0x4354 writer. Everything else solved.

## Trust tier
All region effects here are **FACT** (live direct-observation, this session).
Exact sub-field identities within each region are **DERIVED** (not yet
individually resolved — bisection incomplete, crashed at R2). No offsets promoted
to `name_index.json` yet; do that per-field once the writer BP resolves them.
