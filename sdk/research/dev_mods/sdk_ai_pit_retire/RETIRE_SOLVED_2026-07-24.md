# AI pit retirement — PARTIALLY solved (2026-07-24)

**STATUS: NOT a working retirement yet.** The forced recipe below leaves the car
**stuck at the pit box**, not in its garage. Read the "What is still broken"
section before building anything. An earlier revision of this file claimed
"SOLVED / no garage teleport" — that was wrong and is corrected here.

One byte is poke-proven (`0x105D`); one is a poke-DISPROVEN output (`0x1065`);
the garage placement is unsolved.

## The two bytes nobody had found — one control, one indicator

| offset | field | proof |
|--------|-------|-------|
| `veh+0x105D` | **DriverModelVisible** — 1 = driver drawn, 0 = hidden | poked 1→0 on M. SALO mid-race: helmet + arms vanished on screen, car kept driving. The game clears the same byte itself on a genuine retirement. |
| `veh+0x1065` | **ParkingBrakeIndicator** — read-only mirror; see the warning below | `VehiclePhysicsTimeStep_004c73f0:165`; observed 0→1 on a genuine retirement. **NOT a control** — poke-disproven, see below. |

Both live in the **render object embedded at `veh+0x6F0`** (proven live:
`*(veh+0x6F0)` == vehicle base exactly). `veh+0x1054` is the render *mode* (0..3),
NOT a pointer. Both bytes are bits `0x8000000` / `0x4000000` of one packed
replication word, which is why they always move together.

| render-object offset | absolute (veh+) | meaning |
|---|---|---|
| `+0x000` | `0x6F0` | back-pointer to vehicle base |
| `+0x004` | `0x6F4` | scene root node |
| `+0x964` (`[601]`) | `0x1054` | render mode 0..3 |
| `+0x96D` | `0x105D` | DriverModelVisible |
| `+0x975` | `0x1065` | ParkingBrakeIndicator (output, read-only) |
| `[547]`..`[552]` | `0x88C`..`0x8A0` | COCKPIT, SUSPENSION, HELMET, ARMS, BODY, WHEEL nodes |

Nodes assigned in `VehicleGraphicsSceneLoader_004dfce0:233-244`; `sub_4E0ED0`
(misnamed `AudioSoundEffectController` — it is the render-state update, not audio)
gates HELMET/ARMS/BODY on `+0x96D`. `PitCrewStateMachine_004bbfc0` clears
`0x105D` on its pit/retire branch and sets it on its exit branch.

### WARNING — `0x1065` is an OUTPUT, not an input (poke-disproven 2026-07-24)

Writing `0x1065 = 1` on a healthy moving car (E. TesterA, 89 m/s) succeeds and is
then **reverted to 0 within 0.5 s**. `VehiclePhysicsTimeStep_004c73f0:165`
recomputes it every frame from packed-word bit `0x4000000`:

```c
v27 = (*(_DWORD *)(a2 + 56) & 0x4000000) != 0;
*(_BYTE *)(this + 4197) = v27;                       // veh+0x1065
if ( v27 ) *(_DWORD *)(this + 13700) = 1065353216;   // 1.0f -> BrakeFactor
```

So it **mirrors** the braking state; it does not cause it. Writing it in a retire
recipe is a **no-op**. It is still a reliable retirement *indicator* (reads 1 on
every retired car, 0 on every running car — verified across the whole field).

Side-by-side control on the same car in the same run: `0x105D` **holds** when
poked, `0x1065` does not. That is why `0x105D` is FACT and `0x1065` is CONFIRMED
as an output only. An earlier revision of this file called `0x1065` "what holds a
retired car stationary" — wrong, and it was labelled FACT without ever having
been poked in isolation. What actually stops the car is `ActiveFlag(0x45F4)=0`,
which ends the physics step.

## SOLVED — what a genuine retirement writes

Captured as a full `0x48C0` before/after diff across a real AI terminal-damage
pit retirement (M. SALO lost his rear wing to a pit-lane collision):

| offset | field | before → after |
|--------|-------|----------------|
| `0x001C/0x20/0x24` | **PhysX/Y/Z** | (766.82, 7.75, 1979.96) → **(17.07, 0.14, 428.27)** |
| `0x105D` | DriverModelVisible | 1 → 0 |
| `0x1065` | ParkingBrakeIndicator | 0 → 1 (output — mirrors the brake state) |
| `0x3450` | pittingStatus | 1 → 0 |
| `0x4040` | PitState | 1 → 2 |
| `0x4064` | FinishState | 0 → 2 |
| `0x4068` | ReasonCode | 0 → **0 (never written)** |
| `0x4410` | RetiredFlag | 0 → 1 |
| `0x454C` | callToBox | 1 → 0 |
| `0x45F4` | ActiveFlag | 1 → 0 |

**There IS a garage teleport.** The car is moved ~1550 units in Z and ~750 in X.
Every naturally-retired car in the session sits at `PhysX ≈ 16.9–17.1` (the garage
line); cars retired by our forced writes sit at `PhysX ≈ 10.25` (the pit box).
Garage Z is per-team — Raikkonen 421.69 and Salo 428.27 (Sauber teammates) are
6.6 apart, while other teams are ~30 apart.

## What is still broken

Writing the nine flag fields at `pittingStatus == 3` produces:
- car stopped at the **pit box** (X = 10.25), not the garage
- driver model correctly removed (`0x105D=0`), DNF correctly classified
- the car is held still by `ActiveFlag=0` (physics step ends), NOT by the
  `0x1065` write, which is a no-op

Confirmed on screen for F. ALONSO (slot 21): *"still there in the pitbox."*

Attempted fix that FAILED: writing `PhysX = 17.0` on the already-retired car did
**not** move it. With `ActiveFlag = 0` the physics step no longer runs, so the
position write is never propagated to the render transform. Any teleport must
either happen *before* `ActiveFlag` is cleared, or must also write the render
side (`RenderX/Y/Z` at `0x34F0/4/8`, which also changed in the genuine diff:
run `0x34B4 len=104` covers them), or must be done by the game's own code.

## The Fisichella anomaly — unexplained, worth chasing

G. FISICHELLA (slot 13) ended up correctly **in his garage** (X = 17.00). The
watcher log proves our script never fired: no `RETIRE APPLIED` line, and
`FinishState` was already 2. **The game retired him itself**, taking the proper
path including the garage teleport.

What was different about him: earlier in the session I had written the retire
end-state on him *mid-track* by mistake, then reverted it (FS 2→0, retired 1→0,
active 0→1, pitState 2→1, driver 0→1). He then drove normally again.
On his *next* pit entry the game retired him on its own.

A clean control run on F. ALONSO (never touched) proved this is NOT normal
behaviour: at `pittingStatus == 3` his `FinishState` was still 0, i.e. the game
was going to service him and send him back out. So **`callToBox=1` alone does not
retire a healthy car** — the wing-damage README was right about that.

Something the aborted mid-track retire left behind flipped Fisichella onto the
game's own retirement path. Finding *what* is the highest-value next step,
because that path does the garage placement for free.

Concrete next experiment: take a clean healthy car, write ONLY
`FinishState=2, RetiredFlag=1, ActiveFlag=0` mid-track, revert all three ~2 s
later, then send it to the pits with `callToBox=1` and watch whether the game
retires it on arrival. If it does, bisect which of the three fields is the
trigger, then find the reader in `functions/` and you have the real mechanism.

## pittingStatus enum (`0x3450`) — FACT

`0 = none, 1 = pit requested, 2 = in pit lane, 3 = stopped in box, 4 = exiting`

Gotchas:
- Re-issuing `callToBox=1` while status is 4 (exiting) is **silently rejected** —
  both fields reset to 0. Wait until the car is back on track.
- `callToBox` persists as the *order*; `pittingStatus` stays 0 until the car
  actually commits near pit entry (took ~100 s / a full lap for Alonso).
- The stop window (status 3) lasts ~7 s. A 0.25 s poll catches it comfortably.

## Reversibility (useful)

The full flag set is cleanly reversible. Writing
`FinishState=0, RetiredFlag=0, ActiveFlag=1, PitState=1, DriverModelVisible=1`
on a mid-track-frozen car put Fisichella straight back to racing at 60 m/s.
(`0x1065` needs no restoring — the physics step rewrites it every frame.)
Handy for experiments.

## `sub_4B3400` command dispatcher — partial map (static)

`this[0]`: `>>29` = category, `(>>18)&0x3F` = subcommand, `&0xFF` = slot.

| cat 4 sub | action |
|-----|--------|
| 7 | rebuild running-order linked list (prev/next at `veh+0x40D0/0x40D4`) |
| 8 | LapCompletion |
| 20/21 | session timer init |
| 23 | **push** a penalty onto a 4-entry queue (count `veh+0x4070`, entries `veh+0x4074`) |
| 24 | **pop** the front penalty |
| 26/27 | PitCrewStateMachine (26 = enter/retire, 27 = exit) |
| 28 | `FinishState = 3` (DSQ) |

`veh+0x4070` is `noOfPenaltiesToProcess` in `name_index.json` — independent
confirmation of the queue reading.

## Dead ends — do not repeat

- **`0x4354–0x43A0` is NOT the driver model.** It is the AI racing-line waypoint
  array: `AIClearStateFlags_0042ae00` = `memset(veh+0x4354, 0, 0x50)`;
  `AISteering_004270a0` indexes it as `veh[4*veh[0x44D0] + 0x4354]`. It churns
  every frame while driving — that is why a previous session's write-breakpoint
  on it produced nothing but noise.
- **Do not write ReasonCode** on a pit retirement; the game never does.
  `ReasonCode=6` came from an *accident* capture and does not generalise.
- **Do not call `AIStateReset_0042af40`** — accident/kill path, freezes the car
  mid-track with the driver still in it.
- Writing the end-state while the car is moving on track freezes it on the
  racing line. Reproduced.
- `0x33F4/0x33F8/0x33FC` are not damage severity — healthy cars show the same
  values as retired ones.

## Tooling

- `force_pit_retire.py` — the recipe as a script. `open_game()` enables
  SeDebugPrivilege and reports the real `WinError`; note the game runs
  **elevated**, so the script needs an Administrator shell. Includes
  instrumentation that distinguishes "the game retired it" from "we retired it".
- `driver_visibility_probe.py` — validates the render-object model live
  (`gfx[0] == veh base`) and pokes either visibility byte.

Cheat Engine notes: `readInteger(addr, "byte")` does **not** take a type — the
second argument is *signed*; use `readBytes(addr, 1, false)` for single bytes.
CE `createTimer` persists between MCP calls and is the right way to catch a
timing-tight transition; a `os.clock()` busy-wait inside an MCP call **stalls
that timer thread** and will make you miss the event.

**The human screen is the only oracle.** Every wrong conclusion in this file's
history came from verifying a write by reading the same field back.
