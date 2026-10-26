# SPEC EVF-002A — yellow-flag clear on sector (Group A, T2)

```
CLIP / SDT index / EIDs:
  PWS_YELLOW04 / 142 / 133  ("Sector 1 cleared")
  PWS_YELLOW05 / 144 / 135  ("Sector 2 cleared")
  PWS_YELLOW06 / 146 / 137  ("Sector 3 cleared")

verdict:      NEEDS_TRACKER — T2 (emitter exists but fires on WRONG EDGE)
evidence:     PitSelectionStateMachine_0041b3c0.c:55/83/101, AISessionModeTransition_004193a0.c:22/28/34
offsets:      sector-flag state NOT YET IDENTIFIED — see uncertainty
trigger:      per-sector yellow-flag state: 1→0 edge detect + latch
uncertainty:  which memory location holds per-sector yellow-flag state
```

---

## 1. Existing emitter fires on flag RAISE, not clear

**Emitter:** `PitSelectionStateMachine_0041b3c0.c` lines 55/83/101 — calls `DriverDataTableLookupAndTimeConversion` with eids 133/135/137.

**Mechanism:** `AISessionModeTransition_004193a0.c` lines 22/28/34 calls `BooleanOROfTwoChecks(flt_5EA618, 133/135/137)` to SET the yellow-flag commentary state. `PitSelectionStateMachine` transforms this into actual commentary emission.

**Gates:**
- SessionType == 7 (race only) ✓ (currently 7)
- TrackConditionState flt_7CF3C8 != 0.0 ✓
- RaceActiveFlag dword_7D0308 != 0 ✓

**Cooldown:** all 5.0. NOT blocked. NOT FLT_MAX.

**Problem:** the emitter fires when the yellow flag is RAISED ("sector X yellow"). The human wants it to fire when cleared (1→0 transition).

## 2. What to build

Observer on per-frame tick (MainGameTickUpdate):
1. Identify the per-sector yellow-flag state memory location
2. Store `previous_sector_flags` from the last frame
3. On edge-detect: `(prev_sector_N == 1 AND current_sector_N == 0)` → fire eid for sector N
4. Latch: flag that the clear has been called this session, prevent re-fire if yellow raises again and clears again (cooldown=5.0 might handle this, but a latch is safer)

**Sector mapping (from AISessionModeTransition switch):**
- Sector 1 → eid 133 → PWS_YELLOW04
- Sector 2 → eid 135 → PWS_YELLOW05
- Sector 3 → eid 137 → PWS_YELLOW06

## 3. Uncertainty

1. **Per-sector yellow-flag state unknown**: `AISessionModeTransition` reads the sector-yellow state from a state machine. The actual flag location in memory (global or struct field) is NOT yet identified. The function reads `a2` parameter which determines sector — the flag data comes from a caller. Needs tracing.

2. **Yellow-flag clearing edge case**: if two sectors go yellow simultaneously and clear simultaneously, the latch per-sector prevents duplicate fires. But if the yellow flag is raised-clear-raised-clear within 5 seconds (cooldown), the second clear won't be heard. This is intentional (5s cooldown).

```
LIVE READ REQUEST — per-sector yellow flag state
  precondition: race session, trigger a yellow flag in sector 1 (crash/spin)
  commands:
    python tools/poke.py snapshot before_yellow.json
    (wait for yellow flag to appear)
    python tools/poke.py diff before_yellow.json
  distinguishes:
    Look for a field that changed from 0 to non-zero (or bit set) when
    the yellow flag appeared. Sector flags may be bitmasked in one DWORD
    (bit 0=s1, bit 1=s2, bit 2=s3) or three separate bytes.
```

## 4. Related

`PWS_AIINCID08` (Group D, eid 229, "cars all over in sector X") uses the same per-sector detection but counts incident cars rather than flag edges. Shares the same sector-state observer.
