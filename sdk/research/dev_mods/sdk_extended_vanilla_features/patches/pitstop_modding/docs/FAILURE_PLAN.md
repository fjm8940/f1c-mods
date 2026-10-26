# Pit stop failure catalogue — variations, probabilities, durations

Design doc for `dll/pit_commentary.c`. Written 2026-07-29 against the v2 build
(box-pinned wheel-jam DNF landed and live-confirmed).

Everything below is constrained by two hard limits, so read these first:

1. **Audio inventory is the real ceiling.** The player-facing race-engineer clips
   that exist for pit failures are: 4 corner-specific pairs (`63/62 FL`,
   `65/64 FR`, `68/67 RL`, `70/69 RR` = "problem with X" / "X clear"), plus
   `61` generic wheel gun, `66` "wheel jammed", `56` refuel, `54` stall,
   `117` retire. That is **8 distinct narratives**, not 20. More *mechanical*
   variants are cheap; more *audible* variants are not. Anything new either
   reuses these or ships silent (HUD + timing only).
2. **Mechanism must already be proven.** Each row below is tagged with the
   mechanism it needs and whether that mechanism is live-proven today.

---

## 1. Proven mechanisms available

| Mechanism | Offset / call | Status |
|---|---|---|
| Box freeze (hold car in box) | `veh+0x3470` stopEndTime, re-armed ABSOLUTE each tick | FACT, in v2 |
| Refuel rate manipulation | `veh+0x3480` ramp start | in DLL, untested live |
| Per-actor crew animation freeze | crew array `veh+0x32D4`, freeze by channel pointer | FACT (`tools/freeze_crew_byptr.py`) |
| Terminal DNF + return to menu | `FinishState=2` then `dword_7CE258=1` | FACT, live 2026-07-29 |
| Commentary injection | queue head at `CONTROLLER+0x864` | FACT |
| HUD text | `0x007CD204` slot ring | FACT |
| WAV SFX | `PlaySoundA` | works, but **single channel** — see §4 |

Not available, do not plan around: engine kill / ignition (DISPROVEN today —
engine speed is recalculated per frame, key injection is overwritten by
DirectInput), unsafe-release penalties, pit fire.

---

## 2. Current probabilities are wrong — fix the roll shape first

`pick_player()` today rolls **independent** checks in sequence:

```c
if (roll_pct() < 15) → wheel gun
if (roll_pct() < 2)  → wheel jam
if (roll_pct() < 8)  → fuel
if (roll_pct() < 3)  → stall
for (c = 0..3) if (roll_pct() < 12) → slow corner   /* four separate rolls */
```

The per-corner loop alone is `1 - 0.88^4` ≈ **40%**, and it stacks with the rest.
Total failure rate per stop is roughly **55%**. That is not a rare event, it is
the default experience. Real 2001–02 pit stops went wrong in maybe 1 stop in 8,
and terminally in well under 1 in 100.

**Fix:** one roll, cumulative buckets, so the numbers below are the actual
per-stop odds and they sum to 100 by construction.

```c
int r = roll_pct();          /* 0..99 — single draw */
if      (r < 88) clean;
else if (r < 92) slow_corner;      /* 4%  */
else if (r < 945/10) ...           /* see table */
```

(Use per-mille — `rand_u32() % 1000` — since several buckets are below 1%.)

### Proposed per-stop probabilities (player)

| # | Failure | ‰ | % | Audio | Mechanism | Feasible now |
|---|---|---|---|---|---|---|
| 0 | Clean stop | 880 | 88.0 | — | — | yes |
| 1 | One slow corner | 40 | 4.0 | `WG_SLOW[c]` → `WG_OK[c]` | box hold | yes |
| 2 | Wheel gun trouble (recovers) | 25 | 2.5 | `61` + `WG_SLOW[c]` → `WG_OK[c]` | box hold + freeze that corner's crew | yes |
| 3 | Fuel rig problem | 20 | 2.0 | `56` | ramp manipulation | yes (untested) |
| 4 | Stall on release | 15 | 1.5 | `54` | box hold at exit | yes |
| 5 | Cross-threaded nut (long, recovers) | 10 | 1.0 | `66` + `WG_OK[c]` | box hold + corner freeze | yes |
| 6 | Jack failure | 6 | 0.6 | `61` (generic) | freeze JACK_FRONT/JACK_REAR actor | yes |
| 7 | **Wheel jammed — terminal DNF** | 4 | 0.4 | `66` → `117` | box pin + retire | **live in v2** |

Total failure rate **12.0%**, terminal **0.4%** — roughly 1 bad stop every 8,
1 race-ending every 250. Tune from there.

**Corner weighting** (which of the 4 corners, once a corner failure is picked):
fronts are marginally more exposed than rears in practice — FL 28 / FR 28 /
RL 22 / RR 22. Keep it configurable; the difference is flavour, not physics.

**AI:** keep the AI table separate and looser than the player's (AI stops are
background colour, and `PWS_AIPIT*` / `PWS_AIRETIRE*` clips exist). Suggest
~6% per AI stop total, since with 21 AI cars pitting 1–2× a race that still
yields 2–3 visible AI problems per race.

---

## 3. Per-failure duration variation

Right now each failure has **one fixed constant** (`DELAY_WHEELGUN 15.0f`,
`DELAY_WHEELJAM 12.0f`, …), so every wheel gun failure costs exactly the same
time. That is the single biggest tell that it is scripted.

Replace each constant with a **triangular distribution** — min / mode / max,
sampled per stop. A triangular sample from two uniform draws is three lines and
needs no float library:

```c
/* triangular(min, mode, max) from one uniform u in [0,1) */
static float tri(float lo, float mode, float hi) {
    float u = (float)(rand_u32() % 10000) / 10000.0f;
    float c = (mode - lo) / (hi - lo);
    if (u < c) return lo + sqrtf(u * (hi - lo) * (mode - lo));
    return hi - sqrtf((1.0f - u) * (hi - lo) * (hi - mode));
}
```

| Failure | min | mode | max | Notes |
|---|---|---|---|---|
| Slow corner | 1.0 | 2.0 | 4.5 | barely noticeable → clearly slow |
| Wheel gun trouble | 4.0 | 7.0 | 14.0 | long tail is what makes it memorable |
| Cross-threaded nut | 10.0 | 15.0 | 25.0 | the "disaster but survivable" band |
| Fuel rig | 2.0 | 4.0 | 9.0 | per variant, scaled by which of A/B/C |
| Stall | 2.5 | 4.5 | 9.0 | restart attempts |
| Jack failure | 3.0 | 5.0 | 11.0 | |
| **Wheel jam → DNF** | 6.0 | 8.0 | 12.0 | `JAM_DNF_DELAY`, currently fixed 8.0 |

Second-order variation worth having, cheap to add:

- **Recovery is not guaranteed at first attempt.** For #2/#5, roll a second
  chance at the end of the window: 85% clear, 15% "still stuck" → extend by
  another `tri(3,5,9)` and re-fire `66`. This produces the occasional
  catastrophic stop without a separate failure type.
- **Escalation to terminal.** For #5 specifically, if it has already extended
  twice, 20% chance it converts to the terminal DNF path. Ties the failure tree
  together instead of terminal being an unrelated 0.4% coin flip.
- **Corner-dependent cost.** Rear corners take ~0.5 s longer to clear (harder
  access), fronts recover faster.

Put all of this in an INI next to the DLL so tuning does not need a rebuild —
the probability table and the min/mode/max triples are exactly the kind of
numbers that get adjusted 30 times and never once recompiled.

---

## 4. Wheel gun SFX — current behaviour is wrong, and why

Today (`pit_commentary.c`, tyre-crew block):

```c
if (tyre_start >= srv_start && tyre_start > 0) {
    if (!g_wg_sfx_played) { g_wg_sfx_at = sim_t() + 2.0f; g_wg_sfx_played = 1; }
    if (g_wg_sfx_at > 0 && sim_t() >= g_wg_sfx_at) { play_wav(WG_SOUNDS[roll_pct() % 3]); ... }
}
```

That is **one** WAV, **once** per stop, at a **fixed +2.0 s** after the tyre
animation start — unrelated to what any wheel gun is actually doing. Hence the
symptom: one gun noise at the start of the stop and silence thereafter.

### What the animation data says

From `docs/HANDOFF_ANIMATION_MAPPING.md` (FACT, actors self-identify by ASCII
name at `actor+100`):

- Corner actor groups: **6–11 front LEFT, 12–17 front RIGHT, 18–23 rear LEFT,
  24–29 rear RIGHT**; each group is `FMAN / GMAN / NMAN / WHEELGUN / WHEELOFF /
  WHEELON`.
- `GMAN ≡ WHEELGUN` share an animation window — fronts `2.3 → 9.8`, and the
  **rears use `baseStart 1.0`**, not 2.3/2.5.
- `WHEELOFF` ends ~6.9 (old wheel away), `WHEELON` ends ~10.5 (new wheel on).

So a corner's gun is active roughly `[baseStart, baseEnd]` relative to the tyre
animation start, and physically fires **twice**: once to crack the nut off near
`baseStart`, once to tighten near `WHEELON`'s end.

### Proposed implementation

1. **Read the windows live, do not hardcode them.** At box entry, walk
   `veh+0x32D4` → crew → 34 actors (stride 120, name at `actor+100`), and for
   each corner take the `WHEELGUN` actor's `(baseStart, baseEnd)` channel pair.
   This survives different pit animation sets (52 `.ANM` per variant) and costs
   one read per stop. `tools/dump_crew_struct.py` already does exactly this walk
   read-only — port that logic into the DLL.
2. **Schedule two bursts per corner**, relative to `tyre_start`:
   - burst A at `baseStart + jitter(0, 0.3)` — nut off
   - burst B at `wheelOnEnd - 1.5 + jitter(0, 0.3)` — nut on
   With four corners that is 8 bursts per stop, staggered, which is what a real
   stop sounds like.
3. **On a failing corner, change the pattern rather than muting it**: repeat
   burst A every `tri(0.6, 0.9, 1.4)` s for the duration of the failure window
   (the gun that will not bite), and delay burst B until the corner clears. That
   makes the failure audible without a new clip.

### The blocker to solve first

`PlaySoundA` with `SND_ASYNC` plays **one sound at a time per process** — a
second call cuts the first off. Four corners overlapping will stomp on each
other, and the current code only ever plays one sound so this has never shown
up. Three options:

- **(a) Sequence it.** Enforce ≥250 ms spacing and drop bursts that collide.
  Zero new dependencies, but loses the overlapping-guns texture that is most of
  the point.
- **(b) `waveOutWrite` on N voices** (recommended). Open 4 wave-out handles, one
  per corner, submit buffers per burst. True polyphony, still winmm only, no new
  library. ~80 lines including WAV parsing.
- **(c) Route through the game's own audio.** Not investigated; the pit sound
  path is not mapped yet. Do not plan on it.

Recommend **(b)**, and it should land *before* the per-corner scheduling —
otherwise the scheduling work is untestable, since every burst would cut the
previous one.

---

## 5. Build order — STATUS as of 2026-07-29 (v8)

Items 1-5 are DONE and live. See `SESSION_2026-07-29_FAILURES_AND_AUDIO.md` for
what was actually built and the traps hit on the way.

| # | Item | Status |
|---|---|---|
| 1 | Single-roll probability table + INI | **DONE** — 55% -> 11.4% |
| 2 | `waveOutWrite` mixer | **DONE** — 8-voice shared pool |
| 3 | Per-corner gun scheduling from live crew data | **DONE** — one clip per corner |
| 4 | Triangular durations | **DONE** |
| 5 | Recovery re-roll + escalation | **DONE** |
| 6 | Jack failure | not started |

Corrections to this plan that came out of implementing it:

- **§4's "two bursts per corner" was wrong.** Each `wheelguns_N.wav` is already
  a complete BRRT-BRRT cycle, so one clip per corner is a corner's whole job.
  Two bursts plus a ~1 s failure stutter produced 25 clips in 20 s.
- **Option (b) for polyphony was the right call** and was needed before the
  scheduling could be judged at all.
- Measured gun windows differ from the plan's assumption: rears are
  **1.0/10.4**, not 1.0/9.8 — they run longer than the fronts, not just start
  earlier.

## 6. Original suggested build order (kept for reference)

1. **Single-roll probability table + INI** (§2) — fixes a 55% → 12% failure rate
   that currently distorts every test of everything else.
2. **`waveOutWrite` mixer** (§4b) — unblocks all SFX work.
3. **Per-corner gun scheduling from live crew data** (§4).
4. **Triangular durations** (§3) — kills the fixed-constant tell.
5. **Recovery re-roll + escalation** (§3) — depth, once the base is honest.
6. **Jack failure** (#6) — the only genuinely new failure type in the list, and
   it reuses the proven per-actor freeze.

Items 1 and 4 are pure constant/shape changes to existing code. Item 2 is the
only real engineering. Item 3 depends on 2. Nothing here needs new reverse
engineering — every offset it touches is already FACT-tier.
