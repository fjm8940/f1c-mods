# SPEC EVF-003 — PENALTY_STEWARD observer group

> **Status: EMITTER ALREADY EXISTS — see `sub_41A720`.** 24 clips in group.
>
> **The whole "build an observer" premise was wrong.** `sub_41A720` (0x41A720) is
> the game's own penalty commentary emitter and it is already fully wired for BOTH
> the player and AI. It takes the offending vehicle as parameter `a1`, strcmp's the
> infraction string, and calls the standard queue path. No observer needs building;
> the question for each clip is only whether its branch is REACHED.
>
> | Infraction string | player eid | AI eid |
> |---|---|---|
> | `Speeding In Pitlane` (0x5C271C) | 153 | 256 |
> | `Cut Track` (0x5C2710) | 151 | 253 |
> | `Jumped start` (0x5C2700) | 155 | 254 |
> | `Ignored Blue Flags` (0x5C26EC) | 154 (+ fires 123/124/125) | 252 |
>
> **RETRACTED 2026-07-28:** an earlier revision of this file claimed the 13 AI clips
> were dead because `sub_4BCCC0` is player-only. That measurement was right but the
> inference was wrong — `sub_41A720`'s other caller `StopGoPenaltyHandler` is reached
> from `sub_4BC9B0` (pit-box code, per-car, AI-reachable). AI reachability is
> UNTESTED, not disproven.
>
> Still genuinely blocked: `overtaking under yellow` (eids 146/156) — no such string
> exists in the binary. And DANGEROUS_DRIVING (4 clips) — no code path.
>
> **REVIEWED 2026-07-28 — the original "penalty type is unlocatable" blocker was wrong.**
> The type is not a hidden field: `sub_4BCCC0` passes it as a **string-literal
> argument** to three distinct handlers. Severity = which callee, infraction =
> arg2. No persistent offset, and no data breakpoint on `veh+0x4070`, is needed.

## Observer summary

Penalty detection lives in `sub_4BCCC0` (sole caller: `MainGameTickUpdate`). When
it decides an infraction has occurred it calls one of three handlers, passing the
infraction name as a string:

| Source line | Call | Severity | Infraction |
|---|---|---|---|
| L117 | `StopGoPenaltyHandler(this, aSpeedingInPitl, 10)` | stop/go | pit speeding |
| L184 | `StopGoPenaltyHandler(this, aCutTrack, 10)` | stop/go | cut track |
| L186 | `WarningHandler(this, aCutTrack)` | warning | cut track |
| L260 | `DisqualificationHandler(this, aWrongWay)` | DQ | wrong way |
| L379 | `StopGoPenaltyHandler(this, aIgnoredBlueFla, 10)` | stop/go | ignored blue flags |
| L122 | `sub_4BBA90(this, aSpeedingInPitl, <computed fine>)` | fine | pit speeding, **non-race branch** |

Handler addresses: `StopGoPenaltyHandler` 0x4BBD60, `WarningHandler` 0x4BBC60,
`DisqualificationHandler` 0x4BC0E0.

So the observer is an **event hook on those three functions**, not the per-tick
poll over unknown flags this spec originally described. The tick loop is still
needed, but only for the blue-flag timer and the investigation-delay framework.

## State observers needed

| # | What | Location | Tier | Reads |
|---|------|----------|------|-------|
| 1 | Infraction tracker (`sub_4BCCC0`'s `this`) | **veh slot0 + 0x4034 = 0x5F070C** | **FACT (live)** | Per frame, player only |
| 2 | Blue flag active byte | tracker+0xD4 = veh+0x4108 | DERIVED | Blue flag penalty arm |
| 3 | Blue flag timer (float seconds) | tracker+0xDC = veh+0x4110 | DERIVED | 30.0 threshold |
| 4 | Off-track byte | tracker+0xB4 = veh+0x40E8 | **FACT (live 0->1->0)** | Cut-track penalty arm |
| 5 | Penalty-type | **handler arg2 (`const char*`)** | DERIVED (decompiler-grounded, not yet read live) | Distinguishes every penalty type — see table above |
| 6 | PenaltiesEnabled | 0x7CEF84 | DERIVED | Global gate |
| 7 | FinishState | veh+0x4064 | FACT | Player still classified |
| 8 | SessionType | 0x7C56A4 | FACT | Race-only (7) |
| 9 | RaceActiveFlag | 0x7D0308 | FACT | On-track (3) |
| 10 | lapNumber | veh+0x403C | CONFIRMED | Restart detection |
| 11 | ReasonCode | veh+0x4068 | FACT | DQ/engine blowup |
| 12 | VehiclePtrArray | 0x7C54F0 | FACT | AI driver iteration |
| 13 | slot-live | veh+0x32C8 | FACT | Active AI slot |
| 14 | Driver name | veh+0x4620 | FACT | AI driver identification |

## Observer logic (pseudocode)

```
per tick:
  if SessionType != 7 or RaceActiveFlag != 3 or PenaltiesEnabled != 1:
    disarm all; return

  for each armed tracker:
    if cancel_condition met:
      disarm tracker; continue

    if fire_condition met:
      inject eid via commentary queue (wait idle, retry up to 12x)
      mark latched (per latch rule)
      disarm tracker

  if not latched for this penalty type:
    if arm_condition met:
      arm tracker with arming timestamp

  if lapNumber < previous_lapNumber:
    reset all latches and trackers
```

## Per-clip firing table

### Player penalties (buildable — 5 clips)

| Clip | EID | Arm (handler + arg2) | Priority | Latch |
|------|-----|----------------------|----------|-------|
| RE_PD_SG_SPEED | 153 | `PIT_SPEEDING` AND penalty=STOP_GO | 70 | once-per-race |
| PWS_MISC47 | 144 | `CUT_TRACK` | 70 | once-per-race |

| PWS_MISC03 | 147 | `PIT_SPEEDING` | 70 | once-per-race |
| PWS_PENALTY08 | 139 | `BLUE_FLAG` AND blue_flag_count >= 3 | 70 | once-per-race |
| PWS_PENALTY04 | 154 | `BLOCKING_BLUE` AND penalty=STOP_GO | 70 | once-per-race |


### Player penalties (unbuildable — missing infraction type)

The exe defines exactly FOUR infractions: `Cut Track` (0x5C2710), `Speeding In
Pitlane` (0x5C271C), `Ignored Blue Flags` (0x5C26EC), `Wrong Way` (0x5D25B0).
No "overtaking under yellow", no "jump start" string exists anywhere in the
binary (verified against strings_index.json 2026-07-28).

| Clip | EID | Blocker |
|------|-----|---------|
| RE_PD_INV_DD | 150 | No `DANGEROUS_DRIVING` detection |
| PWS_MISC04 | 146 | No `OVERTAKE_YELLOW` infraction exists |
| PWS_PENALTY03 | 156 | No `OVERTAKE_YELLOW` infraction exists |

Note: `Wrong Way` is a detected infraction with NO commentary clip mapped to it —
the reverse of the usual problem, and a possible bonus line if one fits.

### AI penalties (STATUS UNKNOWN — retracted "dead" verdict)

4 of these have a real, already-wired emitter branch in `sub_41A720`: eids
**252** (blue flags), **253** (cut track), **254** (jumped start), **256** (pit
speeding). Reachability by an AI car is untested. The remaining 9 have no
identified emitter branch.

| Clip | EID | Penalty type | Severity | Priority | Latch |
|------|-----|-------------|----------|----------|-------|
| AIPENALTY14 | 240 | CUT_TRACK | 25_SEC | 20 | repeatable |
| AIPENALTY07 | 241 | OVERTAKE_YELLOW | 25_SEC | 20 | repeatable |
| AIPENALTY05 | 242 | PIT_SPEEDING | 25_SEC | 20 | repeatable |
| AIPENALTY22 | 243 | FAILED_STOPGO | DQ | 70 | once-per-race |
| MISC48 | 245 | CUT_TRACK | INVESTIGATION | 20 | repeatable |
| MISC06 | 246 | JUMP_START | INVESTIGATION | 20 | repeatable |
| MISC08 | 247 | OVERTAKE_YELLOW | INVESTIGATION | 20 | repeatable |
| MISC07 | 248 | PIT_SPEEDING | INVESTIGATION | 20 | repeatable |
| AIPENALTY04 | 252 | BLOCKING_BLUE | STOP_GO | 20 | repeatable |
| AIPENALTY13 | 253 | CUT_TRACK | STOP_GO | 20 | repeatable |
| AIPENALTY02 | 254 | JUMP_START | STOP_GO | 20 | repeatable |
| AIPENALTY03 | 255 | OVERTAKE_YELLOW | STOP_GO | 20 | repeatable |
| AIPENALTY01 | 256 | PIT_SPEEDING | STOP_GO | 20 | repeatable |

### AI penalties (unbuildable — missing infraction type)

| Clip | EID | Blocker |
|------|-----|---------|
| RE_OD_25_DD | 249 | No `DANGEROUS_DRIVING` detection |
| RE_OD_INV_DD | 250 | No `DANGEROUS_DRIVING` detection |
| RE_OD_SG_DD | 251 | No `DANGEROUS_DRIVING` detection |

## Hook site

Hook the **three handlers**, not `sub_4BCCC0` itself. Each is `__thiscall`, so
`this` = ECX = the offending party's tracker, and arg2 = the infraction string:

| Hook | VA | Yields |
|---|---|---|
| `StopGoPenaltyHandler` | 0x4BBD60 | stop/go penalties, arg2 = infraction |
| `WarningHandler` | 0x4BBC60 | warnings (pre-penalty), arg2 = infraction |
| `DisqualificationHandler` | 0x4BC0E0 | DQ, arg2 = infraction |

Compare arg2 against the known literals (`aSpeedingInPitl`, `aCutTrack`,
`aIgnoredBlueFla`, `aWrongWay`) by pointer, not `strcmp` — they are fixed .rdata
addresses in a non-ASLR image.

**`veh+0x4070` (noOfPenaltiesToProcess) IS live and IS on this path.** An earlier
draft of this section said the opposite; that was wrong. It reads 0 normally, but
after driving the wrong way it read **2** (live, 2026-07-28). It gives penalty
COUNT, not type — so poll it as the cheap "a penalty happened" edge, and take the
type from the handler arg2 hook. Static extraction finding no writer meant only
that the extractor missed it.

The observer engine polls at `MainGameTickUpdate` (0x42C690), same tick site
as 6 of 10 existing spec categories.

### queue-injection path (proven, from gr21.c)
```
StructInitWithSentinelValues(buf, eid, ...)
DriverDataTableLookupAndTimeConversion(buf, eid, ...)
InsertDriverTableEntry(0x5EA618, buf)
```

## Open questions (ordered by impact)

1. ~~**Penalty-type persistent offset.**~~ **RESOLVED by review 2026-07-28** —
   the type is handler arg2, see "Observer summary". No offset hunt needed.

2. ~~**Does the game penalise AI?**~~ **ANSWERED: NO.** See "Live verification".
   The 13 AI clips are dead. Closed by user decision 2026-07-28 — not proceeding.

3. **Dangerous driving definition.** Three clips (249-251) and one player clip
   (150) need this. No code path exists. Must be defined by the modder:
   - Repeated cut-track? Multiple yellow-flag overtakes? Causing a collision?
   - The human's T5 note for RE_PD_25_DD says "what determines dangerous driving?"

4. **"Under investigation" framework.** 5 player clips and 4 AI clips are
   investigation lines. The human's T5 note proposes a 30-60s random delay
   between investigation and penalty. This is a new subsystem — not just a
   condition check.

5. **RE_PD_SG_SPEED (eid 153).** Human note says "EXISTS, double check" —
   this clip may already play in vanilla. Verify before building anything.

## Cooldown notes

Per `data/full_event_dump.json`:
- All penalty eids have cooldown 1.0 EXCEPT:
  - eid 154 (PENALTY04, stop/go blocking blue): 20.0
  - eid 259 (AIBLUE03, passed lapped car): 10.0 (different group)
  - eid 80 (MECHPROB20, gear loss): FLT_MAX BLOCKED (different group)
  - eid 113 (STALLED_T): FLT_MAX BLOCKED (different group)

No FLT_MAX blocks in the penalty group — all eids are open for injection.

## Live verification (2026-07-28, Frida on PID 31212, race in progress)

Conditions: SessionType 7, RaceActiveFlag 3, PenaltiesEnabled 1, 22 cars live,
slot0 = "Matias".

| Question | Result | How |
|---|---|---|
| What is `sub_4BCCC0`'s `this`? | **0x5F070C = veh slot0 + 0x4034** — the player's own vehicle, NOT global 0x5EA560 | Interceptor on 0x4BCCC0 |
| Does `sub_4BCCC0` run for AI? | No — 281 calls in 7 s, one distinct ECX, 22 cars live. **But this does NOT mean AI get no penalties**: `sub_41A720` is reached by other callers. Inference retracted. | same |
| Off-track byte real? | **Yes**, `this+0xB4` went 0->1->0 as the player ran wide | live poll |
| `veh+0x4070` dead? | **No** — read 2 after driving the wrong way (0 in all prior samples) | live poll |

Method note: Frida MCP scripts unload when `execute_in_session` returns, so hooks
must be installed AND awaited inside one call (<= ~7 s, or the call times out).
Catching a human-triggered instant that way is unreliable — poll a persistent
counter like `veh+0x4070` afterwards instead. Logged in `findings.jsonl`.
