# retire_commentary.dll — restored player retirement commentary

**Status: WORKING, in-game confirmed 2026-07-28.** `PWS_RETIRE02` (eid 117) is
audible. `PWS_RETIRE01` (118) and `PWS_RETIRE04` (119) are handled by the same
code but have not been observed live yet — they need `ReasonCode` 7 and 1
respectively, which have not occurred.

## Why a DLL at all

The game already ships an emitter for these three lines (`sub_41B250`), but it is
only reached from `sub_4F1FE0`'s `FinishState == 0` arm, behind conditions that
never come true for a human driver. Four designed live runs proved it is not a
missing situation:

| Ruled out | How |
|---|---|
| the `dword_7CE814` gate | reads **0 naturally** in-race — never the blocker |
| a timer race | measured **11.0 s** window vs a 10.0 s dwell |
| the dwell itself | 30 s stationary, every precondition met, silent |
| the clips being broken | direct injection of eid 117 was **audible** |

So the DLL does not repair the emitter. It watches for the retirement the game
performs anyway and injects the event the emitter would have sent.

## What it does

Polling thread, 100 ms. **No code hook, no trampoline, no byte patch.** The only
thing it writes is the commentary queue — the same path already proven in
`mods/grid_reorder/gr21.c`.

Predicate is **damaged AND stopped**:

1. arm when `ReasonCode` (`veh+0x4068`) != 0 while `FinishState` (`veh+0x4064`) == 0,
   and the car has already exceeded 10 m/s this race (`has_moved`)
2. require `Speed` (`veh+0x4440`, FACT, m/s) < 2.0 for 2.5 s continuously
3. re-check `ReasonCode` is *still* non-zero before speaking
4. inject only while the audio player (`CONTROLLER+0xAE4`) is idle, retry up to
   12×, and confirm `isPlaying` goes high before declaring success

eid chosen by `sub_41B250`'s own branch: `ReasonCode` 1 → 119, 7 → 118, else 117.

## Build & inject

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o retire_commentary_v5.dll retire_commentary.c -lkernel32
echo . | ../../red_flag/inject.exe retire_commentary_v5.dll
```

Log: `retire_log.txt` beside the DLL. A healthy run reads:

```
armed: ReasonCode 6 at 88.9 km/h - waiting to see if he stops
TERMINAL: ReasonCode 6, stopped 2.5s -> arming eid 117
  eid 117 PLAYING (attempt 1) — success
```

**Restart the game before injecting a new build** — the old DLL stays resident
otherwise (and holds a file lock on its own .dll, which is why builds are
versioned `_v4`, `_v5`).

## Requires

`player-retire-commentary` must be **applied** (`python tools/patch.py status`),
otherwise eids 117/118/119 keep their FLT_MAX cooldown and the DLL refuses to
inject by design, logging `still FLT_MAX-blocked`.

## The five iterations — read before writing a similar mod

Each failure was a distinct trap, all now encoded in the code comments and
`findings.jsonl`:

| ver | trigger | outcome |
|---|---|---|
| v1 | `FinishState -> 2` | injected but **never audible** — that transition *is* session teardown |
| v2 | `ReasonCode != 0` | still inaudible — AI wing-loss lines held the single queue slot |
| v3 | + idle-player wait, 12 retries | **played** — but announced a retirement at 88 km/h with a drivable car |
| v4 | + speed gate | fired **on the grid** with `ReasonCode 0`: latch survived a race restart, and that false positive **ate the real event** |
| v5 | + restart detect, re-check, `has_moved` | **correct** |

Generalisable rules that came out of it:

- Never inject on a transition that coincides with session teardown — find an
  earlier signal and *measure* the window.
- The queue is a **single slot** the game competes for. Wait for idle and retry;
  don't fire once and hope.
- Confirm by watching `isPlaying`, not by assuming the write took.
- Reset every latch on race restart. `SessionType`/`RaceActiveFlag` do **not**
  signal it — `lapNumber` going backwards does.
- Re-validate arming conditions at fire time; latched state goes stale.
- With a once-per-race latch, a false positive is **worse than silence** — it
  consumes the real event.

## Known issue

The line plays *after* the game's wing-loss chatter ("front wing knocked off,
rear wing knocked off… and that's the end"), because we politely wait for a gap —
in one run that took 12 retries (~4 s). Correct, but poor drama.

Untested lever: the queue entry's priority field at `qhead+0x1C` (currently
written from the event table; eid 117 = 55). Unlike the *score* field — which
`TableLookupByIndex_004144c0` provably re-reads by eid at play time — it is **not
established** whether priority is honoured from the queue entry or re-read from
the table. Verify before relying on it.
