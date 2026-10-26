# Player subsystem failures — design (draft, 2026-09-27)

**Status: design draft for owner review. Not release authorization.**
Answers the owner's question: given the lifetime-seed finding (only the engine
has game data — see
[LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md](LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md)),
how should gearbox / engine / suspension / electronics be done for the player?

**Owner decision recorded 2026-09-27: all player failure probabilities use
OUR system.** No game countdown drives failure timing or chance. Native game
paths (engine life/stress, the gearbox SetGear decision, the suspension bend
call) are used only to make a fault behave like the game's own — never to
decide whether it happens.

## 1. What the game actually gives us, per subsystem

| Subsystem | Proven player lever | Game-provided data | What we must author |
|---|---|---|---|
| Engine | `EngineStressSimulation_0043ef20`: life `0x7C91DC`, stress `0x7C91D4` (power loss), banks (smoke), ignition `0x7C9081` — the game itself stages smoke → misfire → stop | `[ENGINE] LifetimeAvg/Var` seeds life at session load; stress feeds life drain | Only the trigger and the staging choice |
| Gearbox | SetGear detour `0x447E00` (proven): refuse / walk / neutral | **none** | The entire failure probability and schedule |
| Suspension | `SteeringGeometryUpdate_00442060` called on the game thread from a `0x4441F0` detour (proven): the game's own bend input | **none** (bend fields are driven state, not wear) | The entire failure probability and schedule |
| Electronics | **none proven** | **none** (AI failure category 8 exists as a name only) | Everything — needs RE before any promise |
| Brakes | none found for the player (AI counter `veh+0x3D74` only) | none | Out of current scope |
| Clutch / fuel | none found (vanilla AI categories 3/7/9) | none | Out of current scope |

## 2. Design principles

1. **Lever tiers.** Engine = native data path; gearbox/suspension = native call
   paths; anything else = no lever, no promise. Failures must act through a
   game decision the game itself makes (we have been burned by downstream
   writes).
2. **Arm → stages → outcome.** Every family: an onset symptom that costs time
   but is survivable, an escalation, and an outcome (recoverable, limp, or
   terminal). Never forge the end state.
3. **Presentation parity.** Every fault needs a visible/audible story: HUD
   banner, engineer radio (where a correct clip exists), refusal/impact sound.
   No invisible failures.
4. **One scheduler, family-agnostic.** Port the gearbox auto-trigger pattern
   (seeded per-session roll → family → type → severity, caps, guards, logs) to
   engine and suspension; eventually one merged plugin (ship-plan D3).
5. **Owner rules already set:** per-family chances (engine 15%, gearbox 20%,
   suspension 15%) with independent rolls; race + qualifying only; never an
   automatic retirement; false neutral in the weights.

## 3. Per-family plan

### 3.1 Engine — use the native clock

- Background: the vanilla life drain never fires in one race with stock
  `LifetimeAvg` (~9000 s vs ~5000 s race), so an engine fault is always
  something we arm. Two native-feeling shapes:
  - **Shortened life**: on a roll hit, write life (`0x7C91DC`) to a small
    value; the game then runs its own staged death (ReasonCode 1 → smoke →
    stress ramp → stop). Most authentic engine failure possible.
  - **Direct stage choice**: stage 1 (smoke), terminal ramp, instant blow,
    seizure — as the panel already does.
- Keep oil leak and cutout as glitch variants (already built: eid 130 warn,
  eid 119 retire commentary).
- **Chance is ours (decision 2026-09-27):** the engine rolls on our
  scheduler exactly like the other families. The native life/stress fields
  are actuation only; `LifetimeAvg` does not drive probability. A future
  authored per-car/team reliability modifier (our data, default off) may
  scale our chance if car variety is wanted later.
- **Auto mix (owner 2026-09-27):** smoke 35 / wear-to-death 35 / instant 15 /
  seizure 15; oil leak and misfire cutout stay manual-only glitches;
  instant blow and seizure are downgraded to smoke inside the final 2 laps.
  **UPDATE 2026-09-28:** oil leak joined the automatic mix (owner weights
  smoke 30 / wear 30 / instant 10 / seizure 10 / oil 20; both sources);
  misfire cutout moved to the new ELECTRONICS category (below).

### 3.2 Gearbox — already at the target architecture

- Types: lost gear(s) (skip / drop), stuck, false neutral, no-drive; weights
  45/30/15/10; onset lap guard; terminal last-lap guard; one fault per race.
- Next: delayed/reluctant shift feasibility check, then pop-out (1.x).
- Reliability: authored (no game data); may share the engine-derived scale.

### 3.3 Suspension — mechanism proven, scheduler missing

- Port the scheduler: types = FAILING / FAILURE / COLLAPSED, wheel masks
  (rear / front / all), radio eid 75, auto-reset on session exit / pit return
  (already in the sandbox port).
- Narrative: staged wear degradation, not impact damage (no impact model).
- Terminal (COLLAPSED) = the player parks it (owner rule: no auto-retire).
- Presentation decision for release: the sandbox build carries status via
  shared memory only; for the public build we need HUD and/or radio only.

### 3.4 Electronics — BUILT 2026-09-28 as a standalone category (owner-directed)

- **STATUS CHANGE 2026-09-28:** the owner explicitly requested a
  player-specific ELECTRONICS category; implemented as
  `electronics_failure.dll` (radio eid 76 PWS_MECHPROB14, periodic radio
  comms loss + misfire cutout + fuel<1 L starvation cutting; one 10% roll
  per session; no hook - poll + proven-field writes). The "do not build"
  reasoning below is kept as the historical caution that shaped it; the
  implementation obeys it: every effect reuses a proven lever or the
  game's own gate, no fiction systems.
- F1C has no player electronics system and we have **zero proven hooks**.
  Shipping an "electronics" family now would be pure fiction with no native
  behaviour to attach to.
- The effects that *read* as electronics already live inside other families:
  - ignition/rev cut → engine cutout glitch (built);
  - shift electronics → delayed shift (pending investigation) and false
    neutral (built).
- If a distinct electronics story is wanted later: investigation first (shift
  timing site, rev-limiter site), evidence before promise. Add to the
  investigation backlog, not the roadmap.

### 3.5 Brakes / clutch / fuel

- AI-only counters/vanilla categories; no player levers found. Keep out of
  1.0; candidates for a later investigation if the owner wants more families.

## 4. Shared scheduler architecture (target)

Each family rolls **independently** per race/qualifying session (owner
2026-09-27); type weights are ours:

```
engine 15%      -> smoke 30 / wear-to-death 30 / instant 10 / seizure 10 /
                   oil leak 20            [updated 2026-09-28]
electronics 10% -> comms loss + misfire cutout (+ fuel<1 L starvation)
                   [new 2026-09-28]
gearbox 20%     -> lost 45 / stuck 30 / false neutral 15 / no drive 10
suspension 15%  -> FAILING 45 / FAILURE 35 / COLLAPSED 10
                   wheels: rear 50 / all 30 / front 20
  -> wait for onset window (lap 3+)
  -> arm through the family's proven lever
  -> stages (family tables) -> outcome
caps: 1 auto fault per family per session; no instant death inside the last
      2 laps; never auto-retire
frequency: per-family chances above — OUR system; optional authored
           reliability modifier (our data, default off), no game countdown
```

- Reliability modifier (optional, ours): if car variety is wanted later, use
  our own per-car/team table (or an authored formula on engine.ini values) as
  a plain multiplier on our chance; default 1.0. Never a game countdown.
  **IMPLEMENTED 2026-09-28 (dev, default off):** the car-flavor layer reads
  the game-seeded per-car lifetime counters (`veh+0x3D70/74/78/7C`, floats)
  and scales each family's auto-roll chance by `clamp(fieldMean / playerCounter,
  0.50, 2.00)` — see `CAR_FLAVOR_WEIGHTING_2026-09-28.md`.
  **OWNER-DIRECTED CHANGE 2026-09-28 ("for now"):** the default auto source is
  now the **vanilla lifetime timer** — our ticker drives the player's own
  game-seeded counters at the AI rate (1.0/game-second from green) and the
  fault fires on expiry; the fixed-chance roll and flavor layer remain
  selectable. See `VANILLA_TIMER_MODE_2026-09-28.md`. The S2 record
  ("probability is ours, never a countdown") stands for release scoping until
  the owner decides otherwise; this is an explicit dev-mode exception.
- The dev implementation stays three DLLs; the release plugin merges them
  (D3) with one host proof, one INI, one scheduler.
  **D3 EXECUTED EARLY 2026-09-28 (owner directive):** the four family sources
  now link into one module `dll/mechanical_failures.dll` (four control
  blocks / four unload events retained; any Unload stops all families);
  standalone builds remain. One host is proven; one INI still future work -
  see `MERGED_DLL_2026-09-28.md`.

## 5. Next steps, in order

1. Gearbox live validation (owner, in progress) — finish before widening.
2. Port the auto-trigger pattern to **engine** (native clock: shortened-life +
   stage choices) and **suspension** (stages/wheels).
3. Delayed-shift feasibility test; decide 1.0 vs 1.x.
4. Electronics: only if the owner wants the family — one investigation spike
   (shift timing and rev-limit code sites) before any scope is promised.
5. Packaging gates per the ship plan (INI, Lightloader host, installer).

## 6. Open questions for the owner

| # | Question | Proposal |
|---|---|---|
| S1 | Family mix in 1.0 | **RESOLVED 2026-09-27: engine + gearbox + suspension; electronics folded — no standalone family** |
| S2 | Probability source | **RESOLVED 2026-09-27: our own system for all families.** No native countdowns drive chance; optional authored reliability modifier later, default off |
| S3 | Suspension presentation in the public build | HUD + radio (radio eid 75 already proven) |
| S4 | Engine failure shape preference | **RESOLVED 2026-09-27: four core auto types (smoke/wear/instant/seizure, 35/35/15/15); oil leak + cutout manual-only** |
| S5 | Electronics family | **RESOLVED 2026-09-27: folded into engine/gearbox effects; no standalone family until a lever is proven.** **OWNER CHANGE 2026-09-28: standalone player ELECTRONICS category built** (comms loss / misfire / fuel starvation, all on proven levers) — see `HUD_AUDIT_AND_ELECTRONICS_2026-09-28.md` |
