# SPEC EVF-002C — AI tyre degradation (Group C, T2)

```
CLIP / SDT index / EIDs:
  PWS_AITYRES02 / 231 / 218 (AI severe blistering)
  PWS_AITYRES01 / 232 / 219 (AI light blistering)
  (secondary TV eids 472/473 exist but are not part of this spec)

verdict:      NEEDS_TRACKER — T2 (confirmed; NOT T1)
evidence:     below
offsets:      see section 2
trigger:      per-vehicle per-wheel tread threshold crossed on AI slot, edge-detect
uncertainty:  see section 5
```

---

## 1. Is this T1 (reuse existing mechanism)?

**No. The AI path does NOT reuse the player blister code to produce AI-specific commentary.**

The player blister emitter (`sub_438130` = `VehicleDynamicsForceAccumulatorIntegration`) runs for ALL cars during the per-wheel physics tick. It calls `AISessionPhaseCounter` (LIGHT) and `PitStateSlotActivation` (SEVERE), which push eids 94-105 — all player-focused clips (`PWS_TYRES01/02`, `RE_PD_*_BLIST/BAD_BL`). Neither function checks whether the car is player or AI; both emit the same player eids regardless.

Evidence:
- `sub_438130` has ONE caller: `VehiclePhysicsIntegrationStep` (per-frame physics, runs all active cars)
- `AISessionPhaseCounter_0041aa10.c` lines 25-46: switch on corner index, pushes eids 95/97/99/101/105. No player-vs-AI branch.
- `PitStateSlotActivation_0041ab30.c` lines 25-46: pushes eids 94/96/98/100/104. No player-vs-AI branch.
- Grep of `functions/*.c` for `\b218\b` and `\b219\b`: ZERO references to these eids as commentary event IDs. All 20 hits are struct offsets, loop counters, or array indices. No function emits eid 218 or 219.

The AI tyre eids (218/219) have no emitter in the decompiled codebase. They exist in the event table (`full_event_dump.json`: priority=40, cooldown=15.0, blocked=false, via_pws_lookup=true) and have valid SDT clips, but nothing calls them.

---

## 2. Trigger mechanism (what already exists that we can reuse)

The per-wheel tread physics IS shared between player and AI. The same `sub_438130` already computes:

```c
v27 = (*(double *)(this + 916) - 0.75) * 4.0;   // normalized tread above puncture
v29 = (*(double *)(this + 916) - v31 - 0.75) * 4.0;  // next-tick normalized tread
```

Where:
- `this + 916` (= `this[0x394]`): double, per-wheel tread, fresh=1.0, CONFIRMED
- `v31`: per-tick tread wear delta (computed from physics forces at lines 126-135)
- `0.75`: base threshold (global `dbl_59EA60`, DERIVED)
- `4.0`: scale factor (global `dbl_59E7D8`, DERIVED)

**Thresholds already proven for player (FACT, live CE disassembly):**
| Guard | Global | Value | Meaning |
|-------|--------|-------|---------|
| LIGHT (v27 ≤ 0.7) | `flt_59E6E8` (0x59E6E8) | 0.7f | tread ≤ 0.925 (= 0.75 + 0.7/4.0) |
| SEVERE (v27 > 0.4 && v29 ≤ 0.4) | `flt_59E8C8` (0x59E8C8) | 0.4f | tread crosses below 0.85 (= 0.75 + 0.4/4.0) |

**The AI should use the SAME thresholds** — tyre physics is tyre physics. The human triage note says "AI tire wear is a lot slower than player, and it is dictated in .tbc file" — meaning the DEGRADATION RATE differs, not the threshold for commentary.

**Slot identification:** the slot index for the vehicle being processed is in the physics wheel struct. `*(this+120)` in sub_438130 gives the wheel corner (0=FL, 1=FR, 2=RL, 3=RR), but the VEHICLE slot is elsewhere. The vehicle identification needs verification (see uncertainty #2).

---

## 3. Predicate per clip

Both clips share one observer hook at `sub_438130` (or its caller `VehiclePhysicsIntegrationStep`).

| Clip | eid | Original code equivalent | Condition |
|------|-----|------------------------|-----------|
| PWS_AITYRES01 | 219 | `AISessionPhaseCounter` (LIGHT) | AI slot AND tread between 0.85 and 0.925 AND per-(vehicle×corner) latch false |
| PWS_AITYRES02 | 218 | `PitStateSlotActivation` (SEVERE) | AI slot AND tread crosses 0.85 downward AND per-(vehicle×corner) latch false |

**Latch:** the existing code already tracks per-corner flags at `dword_5EB978[657 + corner]` (set to 1 for LIGHT, 2 for SEVERE). These serve as one-shot gates — once flagged, re-firing is blocked for that corner. The patch for AI would need to:
- Hook after the existing trench detection in sub_438130
- When the car slot is AI and the trigger fires, queue eid 218 or 219 INSTEAD of the player eid

**Alternatively:** a T3-style per-frame observer on `MainGameTickUpdate` that reads AI vehicle tread values and tracks per-vehicle×per-corner latches. This avoids hooks inside the hot physics path but adds polling overhead for AI cars.

---

## 4. Cooldown analysis

| eid | Clip | Cooldown | Notes |
|-----|------|----------|-------|
| 218 | PWS_AITYRES02 (AI severe) | 15.0 | Reasonable; matches player blister cooldown |
| 219 | PWS_AITYRES01 (AI light) | 15.0 | Same |

Both cooldowns are 15.0 seconds — consistent with the player blister cooldowns (the restored PWS_TYRES01/02 also use 15.0). This is correct and needs no modification.

---

## 5. Uncertainty

1. **Vehicle slot → AI detection**: sub_438130 processes per-wheel physics objects. The slot index (0=player, 1..N=AI) is NOT at `this+120` (that's the corner). Where the vehicle slot is stored in the per-wheel physics object is unverified. The function reads `dword_5EC3BC[*(this+120)]` at line 142 which indexes by corner (0-3), not slot. **Needs live breakpoint or DBVM watch** on sub_438130 to capture `this+<slot_offset>` values for player vs AI wheels.

2. **.tbc tyre wear for AI**: the human notes AI tyre wear is slower. If AI tread degrades so slowly that they never cross 0.85 (SEVERE threshold) in a race, the AI blister clips fire rarely or never. This is a **balance issue**, not a bug — the emitter would still be correct even if seldom triggered. **No code fix needed for this.**

3. **AI blister variant selection (472/473)**: the secondary eids (472 for AITYRES02 TV variant, 473 for AITYRES01 TV variant) are present in the event table but their selection logic is unknown. Since PWS is the primary path (`via_pws_lookup=true`) and TV variant handling is already understood (event_clip_map shows the mapping), emitting eid 218/219 via PWS lookup should Also cover the TV path automatically. Unverified.

4. **Per-corner vs generic AI clip**: the player blister path has per-corner clips (eids 94-101) and a generic clip (104/105, fired when 3+ corners blistered). The AI clips (218/219) have no per-corner variants visible in the clip list. Confirmed: `progress.json` only lists two AI tyre clips (AITYRES01/02), and `full_event_dump.json` shows only two AI blister eids (218/219) with no corner-specific siblings. This simplifies the AI emitter — no corner counting needed.

5. **`dword_5EB978` struct layout**: the `this` pointer passed to AISessionPhaseCounter/PitStateSlotActivation is the global `dword_5EB978`. The flag at `this[657+corner]` indexes into this struct. If this struct is a flat per-corner array (not per-car), the logic breaks for multi-car races. The FINDING that the existing blister code works for the player proves the mechanism is correct for at least one car, but the struct stride and indexing need verification before extending to AI.

```
LIVE READ REQUEST
  precondition: race session, multiple AI cars on track, player has clean tyres
  commands:
    python tools/poke.py veh 0x32C8:int32                    # player active
    python tools/poke.py read 0x5EC3BC:int32                  # corner flag state
    python tools/poke.py read 0x5EC3BC:int32 --offset 4       # next DWORD
    python tools/poke.py read 0x5EC3BC:int32 --offset 8       # next DWORD
    python tools/poke.py read 0x5EC3BC:int32 --offset 12      # next DWORD
  distinguishes:
    If all four are 0: blister flags not set for any corner (player tyres fresh)
    If any is non-zero: blister flag tracking IS per-corner, not per-car — this
    confirms the limitation. Look at AI slot 1's wheels separately.
```

---

## 6. Implementability

**Best approach:** hook `sub_438130` after the existing blister detection (line 153), check if the car is AI, and queue eid 218 or 219 instead of letting the player eid through. This reuses the existing trench-compute, threshold comparison, and per-corner edge-detect — only the final eid dispatch changes.

**Fallback:** T3 per-frame observer on `MainGameTickUpdate`, reading AI tread values from each car's per-wheel data. Adds polling overhead but avoids physics-hot-path hooking.

**Reference:** player blister patch `blister-tolerance` in `patches/manifest.json` — proves the threshold constants and sub_438130 location are correct and patchable.
