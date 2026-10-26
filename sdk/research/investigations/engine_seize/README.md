# Heavy-impact engine-stop flag — `veh+0x4574`

**Date:** 2026-07-25
**Trust:** DERIVED — one trial, and with a **known confound in the negative control**
(see "Correction" below). Not poke-proven. Do not treat as settled.

## Claim (narrowed after correction)

`veh+0x4574` (byte) is set `0 -> 1` on a very heavy impact and cleared again once the
car settles. It is **impact-related for certain**. Whether it specifically means
"engine seized per `damage.ini Engine=10000.0`" — as opposed to a more generic
heavy-impact / engine-stopped state — is **not established**.

## CORRECTION (same session, after the initial write-up)

The first version of this document claimed `0x4574` was an engine-seize flag, on the
strength of Hakkinen (slot 2) reading 0 despite heavier part damage. **That control is
compromised.**

The flag turned out to be **transient**: Schumacher read 1 twice over 2.5 s while still
rolling at 51 km/h with `activeFlag=1`, then read **0** later once he had come to rest
with `activeFlag=0`. But Hakkinen had *already* come to rest when the control table was
sampled. So his 0 is equally well explained by the flag having cleared as by it never
having set — the comparison does not distinguish the two.

What survives the correction:
- The 18 never-crashed control cars all read 0, so the flag is genuinely impact-related
  and is not ordinary AI state (unlike the rejected candidates below).
- The magnitude/threshold story from `damage.ini` remains plausible but **untested**.

What does not survive: any confident claim that this is *the engine seize bit*
specifically, or that a sub-threshold impact leaves it at 0.

## Why this was looked for

Prior corpus state held that no per-car engine health existed — `name_index.json` has
68 vehicle fields with only `EngineRPM` / `EngineMaxRPM` / `EngineAngVel`, and the
F1CUI community table (~97 fields) has the same. That conclusion was **wrong**, because
it was drawn from the memory-field lists alone without consulting the game's data files.

The owner pointed out `SeasonData/damage.ini`:

```ini
[PHYSICAL]
Engine=10000.0             // Impulse to seize engine (~150 km/h head-on)
FrontWingDetach=1000.0     // ~15 km/h
RearWingDetach=1600.0      // ~18 km/h
WheelBend=1500.0           // ~22 km/h
WheelDetach=3500.0         // ~68 km/h    (WheelRandom=0.65)
Part0..Part7Detach=800.0   // Part*Random=0.60
```

So engine seizure is a real, impulse-driven mechanic distinct from the AI random
failure roll. Note this file is a modified install ("Option C - Balanced Realism"),
so the thresholds are calibrated to this setup, not stock.

**Bonus result:** `Part0`..`Part7` is exactly eight slots, which explains the
previously-empirical `partIdx` range 6–13 as bodywork. The whole partIdx enum is just
the ini's part list in order: wheels 0–3, front wing 4, rear wing 5, `PartN` = 6+N.

## Method — snapshot / act / diff with a noise filter

1. Full 0x48C0-byte struct dumped for all 22 live cars immediately before impact
   (`snap_before.bin` + `.json` roster).
2. Owner drove head-on into M. Schumacher (slot 1) at **268 km/h** — comfortably past
   the 10000 threshold. Schumacher was clean beforehand (`carConditions=0`,
   `ReasonCode=0`, active).
3. Second full dump after (`snap_after.bin`).
4. Diff, then **subtract every offset that also changed in any of the 18 healthy,
   active control cars** over the same interval. This removes position/physics/timing
   churn, which is the bulk of the delta.

Raw byte diff for the target: 765 changed bytes. After the noise filter: 269 dword
offsets unique to the target. Restricting to boolean-shaped transitions (both values
<= 3) left **15 candidates**.

## Result

```
  +0x0874  1 -> 0        part record flag
  +0x08B8  1 -> 0        part record flag
  +0x08DC  2 -> 0
  +0x099C  1 -> 0        part record flag
  +0x09E0  1 -> 0        part record flag
  +0x0A04  2 -> 0
  +0x0D14  1 -> 0        frontWingFlag   (CONFIRMED, known)
  +0x0F34  1 -> 0        part record flag
  +0x0F78  1 -> 0        part record flag
  +0x3D6C  0 -> 2        carConditions   (FACT, known — 2 = wheel loss)
  +0x3D70  2 -> 0
  +0x42CC  0 -> 1        REJECTED — see control table
  +0x4304  0 -> 1        REJECTED
  +0x4320  0 -> 1        REJECTED
  +0x4574  0 -> 1        <== ENGINE SEIZED
```

### The control table is what makes this a finding rather than a guess

| Car | Situation | `0x42CC` | `0x4304` | `0x4320` | `0x4574` |
|---|---|---|---|---|---|
| **Schumacher (1)** | head-on @ 268 km/h | 1 | 1 | 1 | **1** |
| **Hakkinen (2)** | hard side hit: rear wing + wheel + 3 panels | 1 | 1 | 1 | **0** |
| Irvine (11) | retired, `ReasonCode=8` electronics | 0 | 0 | 0 | **0** |
| 18 healthy running cars | — | mostly 1 | mostly 1 | mostly 1 | **0** |

`0x42CC` / `0x4304` / `0x4320` are set on most *healthy* cars, so they are ordinary AI
state, not damage. Rejected.

**Hakkinen was intended as the decisive control** — he lost *more parts* than
Schumacher (rear wing, a wheel, three panels vs. front wing, two wheels, two panels)
yet read 0. The original reading was that his impulse cleared `WheelDetach` (3500) but
not `Engine` (10000), separating "seized" from "badly damaged".

**This inference is invalid** — see the Correction at the top. He had already stopped
when sampled, and the flag clears on settling, so his 0 proves nothing about whether it
set at the moment of his impact. The distinction that "had to be proven" is still
unproven.

### Behaviour while the flag was set (two samples, 2.5 s apart)

```
kmh 50.9 -> 50.9    rpm 741 -> 741    throttle 0    gear 0
x4574 = 1           activeFlag 1      retired 0     finishState 0     callToBox 0
```

Frozen. RPM sits at idle rather than revving, throttle and gear at zero, speed
unchanging. The car is still nominally racing — **a seize is not a retirement**, and
nothing else in the struct marks it. Without `0x4574` there is no way to tell a seized
car from one simply sitting still.

### Parts lost (hook-proven at source, `RepairServiceStateDispatcher_004ed250`)

```
588.128  Schumacher  partIdx 4   front wing   cc 0->1
588.128  Schumacher  partIdx 13  Part7        cc 1
588.128  Schumacher  partIdx 0   wheel0       cc ->2, ReasonCode ->6
588.128  Schumacher  partIdx 1   wheel1       cc 2
588.128  Schumacher  partIdx 12  Part6        cc 2
```

Textbook head-on: front wing, both front wheels, two panels — and **no rear wing**,
which independently matches the struct diff (`0xD14` armed, `0xD58` untouched).

## Open / unresolved

- **The meaning is not pinned down.** Impact-related: yes. Engine-seize-specific:
  unproven. The clean experiment is to **poll `0x4574` continuously** (not sample it
  after the fact) through two runs: (a) a >150 km/h head-on, (b) a heavy but
  sub-10000 impact that detaches wheels. If it sets in (a) and never in (b), the
  seize reading is confirmed. Sampling after the cars stop cannot answer this,
  which is precisely the mistake made the first time round.
- **It is transient.** Set on impact, cleared once the car settles — so anything
  consuming it must latch it as it happens, exactly like the wing flags. Post-hoc
  polling of a stopped car will always read 0 and look like nothing happened.
- **Single trial.** Reproduce before promoting to CONFIRMED.
- **Not poke-proven.** Writing `0x4574 = 1` on a healthy car and observing the engine
  die would raise this to FACT. Untested.
- **Adjacent to a suspect field.** `0x4570` is labelled `elapsed_time` in the corpus
  but reads 0.0 for every car in every session checked, so this whole region may be
  mislabelled. Treat the neighbourhood with suspicion.
- **Unidentified part flags.** `0x874 / 0x8B8 / 0x99C / 0x9E0 / 0xF34 / 0xF78` all went
  `1 -> 0` and pair at stride 0x44, matching the known wing-record shape (ptr at X,
  flag at X+0x40). They are almost certainly the wheel/bodywork equivalents of the wing
  records, but the mapping from offset to specific part is **not** established — the
  obvious arithmetic (`flag(i) = 0xD14 + (i-4)*0x44`) does not fit the observed set.
- `0x3D70`, `0x8DC`, `0x0A04` (`2 -> 0`) unexplained.

## Reproduce

`snap_before.bin`, `snap_after.bin`, roster JSONs and `detach_evidence.csv` are kept
next to this file. The diff is the node one-liner in the session log: dump both, XOR
per 0x48C0 stride, subtract offsets that moved in any healthy control car.
