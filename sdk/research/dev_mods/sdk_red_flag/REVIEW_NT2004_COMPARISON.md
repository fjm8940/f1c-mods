# red_flag mod vs NT2004 native red flag — comparison review

**Date:** 2026-07-31 · **Reviewer:** corpus session
**Sources:** NT2004 decompile (`nt2004_ida_decompile/functions/*.c`, extraction) ·
`mods/red_flag/redflag.c` (this mod, read in full, 1,587 lines) ·
F1C live facts (`findings.jsonl` 2026-07-31).

Tier note: NT2004 semantic readings are DERIVED (its red trigger is a dead stub,
so the sequence was never runtime-exercised). Mod behaviour is FACT by reading
its own source. F1C live levers are FACT.

---

## 1. What NT2004's red flag actually is

The shipped binary has **red-flag machinery, no live trigger**. The only
automatic path to it is the rain gate `sub_460A50()`, which **is a stub returning
0** (`functions/sub_460A50_00460a50.c`). So the sequence below is what *would*
happen; it has never run in a retail race.

If fired, the chain is:

```
sub_4F9670 state 4/5 (caution laps elapsed) ─ rain? → sub_4F9920(8,-1)   [dead]
sub_4FB270 substate 1 (field catch) ─ leader gap > 900 units → mode 2   [live path]
      │
      ▼
steward state 8 "Red Flag Detected"  (sub_4F99C0 case 8)
      │  sub_42F950(&dword_75CBA8, 7)  → pace-lap substate 7 (red hold)
      │  sub_4F9F50 → every car mode 2 (red) via sub_4F8D30
      ▼
field held at pace (sub_4FB270 substate 7 accumulates hold timer;
                    per-car pace speed sub_4DC260 / +0x5178)
      │
      ▼
green flag (state 0) → substate 5 → cars mode 3 → sub_4FAFF0 all cars to race speed
```

Key structural facts:

- **Red is a substate of the caution phase.** `dword_75CBB0 == 7` (caution),
  `dword_75CBB4 == 7` (red hold). There is **no phase-8 setter call** anywhere —
  grep of all `sub_42F6C0(...)` call sites shows 4/5/7/variable, never 8. The
  "8 = red" phase value in the earlier map is not reached at runtime.
- **No re-grid, no lap adjustment, no fuel freeze.** The race clock
  (`dbl_7DA9D0`) keeps running; the field stays in pace-line order (which *is*
  running order, built by `sub_4FA900`); the "restart" is just green → race
  speed. NT2004 red = **hold the field, then resume**. It is *not* a full
  stop-and-restart.
- **Enforcement is penal + corrective:** `sub_4DF490` per-frame geometry check
  converts a pass under the hold into Stop/Go penalties and lap deductions.
- **Restart countdown exists** (`sub_4FA8E0`, 5-4-3-2-1 stages in substate 4)
  but belongs to the **caution/pace** restart, not the red hold.
- **AI controllers have a mode field** (0/1/2: green/caution/red) consumed by
  `sub_50C860` and broadcast through `sub_5A2930` to every wheel/controller —
  i.e. "red mode" is a real per-car AI input, not just a banner.

**Bottom line:** NT2004's red flag is a *field-hold-and-resume* primitive with
a dead trigger. It never re-forms a grid, never sends cars to pits, never
shortens the race.

## 2. What the mod does

Three hotkeys + a DLL state machine (phases 0/1/2/3/10), all in one worker
thread polling `GetAsyncKeyState`:

| Trigger | Behaviour |
|---|---|
| **NUMPAD1** | 30 s countdown → speed cap (`veh+0x453C = 0.38`) + RacePos lock (rewrite `veh+0x4060` to snapshot every tick) → `RESTART_FLAG=1` → re-grid active cars by running order (`QualPos = i+1`), DNF cars `QualPos=200`+`lap=0` → shorten race (`TOTAL_LAPS = saved − max_driven`) → restore fuel → re-retire + sink DNF cars |
| **NUMPAD2** | **Full red flag**: snapshot grid → pit-retire ALL AI via the game's own path (`0xD58=0` + `0x454C=1`, cars drive in, game retires them) → wait until the *player* reaches his own pit box (`pitting==2`) → un-retire exactly those cars → same restart as NUMPAD1. 5 min timeout. |
| **NUMPAD3** | Pit-retire every AI car, no restart (SHIFT reverts arms) |
| auto | 3-car pileup in 8 s → red flag. **DISABLED** (`AUTO_REDFLAG_ENABLED 0`) — restart transients drop `ActiveFlag=0` and look like a pileup |

The DNF machinery is the heart of the mod and the product of gr32→gr44:
4 s sustained-kill confirm (`kill_duration`), classify-then-revalidate
(`revalidate_dnf` undoes false positives), bounded restart hold
(`dnf_settled`), `QualPos=200`+`lap=0` written **at classification** (not at
restart), physics-off-first-then-teleport (`ActiveFlag=0` then scene+physics
`Y=−1000`), unhide/rehide across sequences.

## 3. The comparison

| Aspect | NT2004 native | Mod | Verdict |
|---|---|---|---|
| Trigger | rain stub (dead) / leader-gap>900 | manual hotkeys; auto disabled | Mod is the only working trigger. NT2004's dead paths are not worth copying |
| Field control | per-car mode 0-3 + pace-line order + pace speed | speed cap `0x453C` + RacePos rewrite | **Mod's speed offset is the weak spot** (below) |
| No-overtaking | **physical** (pace-line + pace speed) + **penalty** (`sub_4DF490` → Stop/Go, lap deduction) | **bookkeeping** (rewrite RacePos every tick; the cap does the physical work) | NT2004's is principled; the mod's works but has no penalty teeth |
| Restart | green → resume, same order, race clock keeps running | full stop → re-grid by snapshot → race shortened → fuel frozen | Mod is a *real* red-flag restart (F1-style); NT2004 is a hold-and-resume |
| Cars during red | held on track | NUMPAD2: all cars to their pit boxes (garage) | Mod's NUMPAD2 is closer to a genuine F1 red-flag stop |
| Grid | unchanged (pace line = running order) | rebuilt: active by running order, DNF excluded (`QualPos=200`) | Mod only. NT2004 has no grid concept in red |
| Laps / fuel | untouched | laps shortened to "N remaining", fuel restored | Mod only — F1 rules model |
| DNF classification | game's own (slow, variable latency) | mod's own sweep (4 s confirm + revalidate + settle) | **Mod is strictly better** — NT2004 has nothing comparable |
| Countdown | `sub_4FA8E0` 5-4-3-2-1 (caution restart) | 30 s with HUD beeps | different scales; mod fine |
| Penalties for passing under red | yes (Stop/Go, lap deduction) | none — position snaps back silently | gap: mod could adopt the geometry check |
| Auto-trigger hygiene | scanner `sub_4F93E0` is gated on the pace-lap phase (runs under `sub_4F9040`) | auto-detect has **no phase gate** → mis-fires on restart transients, hence disabled | NT2004 confirms the fix the mod's README already proposes: gate on `GAME_STATE`/`RACE_PHASE` settled racing + persistent kill + manual cooldown |

## 4. Three things worth acting on

1. **The speed cap offset.** The mod writes `veh+0x453C = 0.38` once. The
   corpus's only claim about 0x453C ("AIAbility adjusts it ±0.01/frame") is
   DERIVED from a GUESS-tier fill and **unverified** — and the mod is
   driver-confirmed working, so empirically it holds well enough. But F1C's
   *proven* whole-field speed lever is **`sub_42ACD0(veh, N)` → veh+0x4538
   maxSpeed** (FACT, 2026-07-31: 21 cars held at 30 m/s for ~143 s, zero
   overrides, clean release; gate every call on `dword_7C56A8 == 5`). If the
   cap ever drifts, that is the replacement: call the game's own setter rather
   than poking the multiplier.

2. **No-overtaking without penalty teeth.** The mod's RacePos lock (0x4060,
   LATCHED) rewrites the *standing numbers* every tick; it does not stop a car
   physically passing and it does not punish the pass. NT2004's `sub_4DF490`
   geometry check (waypoint-scalar vs pace-target → Stop/Go / lap deduction)
   is portable and would give the red flag real enforcement. The full_safety_car
   live work showed the speed lever alone preserves order — so either the lock
   becomes a safety net (recommended) or a penalty layer lands.

3. **Auto-detect.** NT2004's incident scanner is gated on the pace-lap phase —
   exactly the "gate on settled racing" fix the mod's README says is needed to
   re-enable auto red flag. The NT2004 source is the reference for that gate.

## 5. What the mod gets right that NT2004 doesn't have at all

- **The DNF sweep.** NT2004's red flag has no concept of excluding cars that
  die during the window. The mod's 4 s kill-confirm + revalidate + bounded
  settle is genuinely superior and is the hardest-won knowledge in the whole
  mod.
- **Full restart semantics** (re-grid, laps remaining, fuel restore) — a real
  F1 red flag, not a hold-and-resume.
- **NUMPAD2's pit-retire path** uses the game's own retirement mechanism
  instead of forging end-states — the same principle the F1C corpus preaches
  ("arm the trigger, never forge the result").

## 6. Summary

NT2004's red flag is an **incomplete, dead-trigger field-hold** — machinery
with no race to run. The mod is a **complete, working, F1-style red flag** —
stop, re-grid, shorten, resume — with a DNF classification system NT2004
never had. NT2004 wins on exactly two fronts the mod should consider: a
phase-gated auto-trigger (the incident scanner) and penalty enforcement for
passes under the red. The mod's speed cap rests on an unverified offset where
a FACT-proven lever (`sub_42ACD0`/0x4538) exists.

---
*Changelog: 2026-07-31 — initial review.*
