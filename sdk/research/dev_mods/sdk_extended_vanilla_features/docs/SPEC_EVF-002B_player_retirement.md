# SPEC EVF-002B — player terminal damage / retirement (Group B, T1 + UNKNOWN)

```
CLIP / SDT index / EIDs:
  PWS_RETIRE02 / 118 / 117, 410  ("Player retired generic")
  PWS_RETIRE03 / 119 / NONE     ("Player retired generic 2")

verdict:      RETIRE02: T1 (emitter exists, FLT_MAX cooldown)
              RETIRE03: UNKNOWN (no eids — unreachable)

evidence:     sub_41B250_0041b250.c:23 (emitter for eid 117)
              full_event_dump.json (eid 117 = FLT_MAX, blocked=true)
              orphaned_clips.json (RETIRE03 has no eid)
offsets:      veh+0x4068=ReasonCode (CONFIRMED), veh+0x4044=damage float (unnamed)
trigger:      damage.ini engine seizure → ReasonCode → sub_41B250 emitter
uncertainty:  why RETIRE03 has zero eids; ReasonCode=1 vs ReasonCode=7 mapping
```

---

## 1. PWS_RETIRE02 (eid 117) — T1: emitter exists, FLT_MAX cooldown

**Emitter:** `sub_41B250_0041b250.c:23` — `DriverDataTableLookupAndTimeConversion(v4, 117, ...)`.

**Path:** called when `a1[16488]` (= veh+0x4068, CONFIRMED as ReasonCode in prior art) is NOT 1 and NOT 7. This is the default/catch-all retirement path.

**Full retirement eid mapping in sub_41B250:**
| ReasonCode (veh+0x4068) | Player eid | Clip |
|---------------------------|-----------|------|
| 1 (accident?) | 119 | PWS_RETIRE04 |
| 7 (engine?) | 118 | PWS_RETIRE01 |
| all others | **117** | **PWS_RETIRE02** |

**Gates (all currently satisfied):**
- `dword_7D0308` != 0 ✓
- `flt_7CF3C8` != 0.0 ✓
- `SessionType == 7` ✓
- `RacePhase != 8`
- `ChampionshipMode != 5` (dword_7D0410, currently 3)
- `player_veh[18580] == a1[18580]` — same session context (likely always true for player retirement)

**Blocker:** cooldown for eid 117 = FLT_MAX (3.4028234663852886e+38), `blocked=true` in full_event_dump.json. This IS part of the `player-retire-commentary` registered patch.

**Fix:** apply the `player-retire-commentary` patch from `patches/manifest.json` (changes FLT_MAX to 6.0, matching AIRetire02-05 cooldowns).

**Verdict:** WIRED (T1). Same pattern as PWS_HIT01-06 — emitter exists, FLT_MAX cooldown the only blocker.

## 2. PWS_RETIRE03 (sdt 119) — UNKNOWN: zero eids

**`progress.json`:** eids = []. No event ID maps to this clip.

**`orphaned_clips.json`:** RETIRE03 listed as orphaned (confirmed: no event's score resolves to sdt 119).

**`event_clip_map.json`:** no entry for RETIRE03.

**Implications:**
1. There is NO code path that can ever play this clip in vanilla.
2. The SDT file contains the audio, but the event table has no mapping to it.
3. To wire it: either allocate a new eid in an unused event-table slot (complex, needs table structure knowledge), OR rotate an existing firing retirement event's score to alternately point to sdt 119 (variant rotation).

**Feasibility:** allocating a new eid requires understanding the event-table layout (stride 0x14 per event, base at 0x5BF9A0) and finding unused slots. Variant rotation is simpler but loses the original clip.

**Verdict:** UNKNOWN. Needs a separate investigation: is adding a new event-table entry feasible, or should RETIRE03 be a variant of RETIRE02?

## 3. Uncertainty

1. **ReasonCode enum mapping**: which ReasonCode values map to which retirement type is not fully documented. The prior art finding says "ReasonCode (veh+0x4068) is never written on a pit retirement — it stays 0." For engine seizure (damage.ini), it's unclear which ReasonCode fires.

2. **`a1[16488]` identity**: 16488 = 0x4068 which is named ReasonCode in `name_index.json` (CONFIRMED). But the prior art also says ReasonCode stays 0 during pit retirement — meaning the values 1, 7, etc. are for different retirement causes (crash, engine failure, etc.).

3. **RETIRE03 wiring**: adding a new eid entry or rotating variant scores needs further investigation of the event-table structure. The `tools/poke.py` can read/write the table (base 0x5BF9A0), but modifying it in the exe requires a patch or DLL hook.

4. **Player-retire patch not yet applied**: the `player-retire-commentary` patch changes FLT_MAX to 6.0 for eids 117/118/119. It is REGISTERED but NOT APPLIED. It also gates on SessionType==7 (race only). If the user is in a race (SessionType=7), applying the patch should immediately enable these clips.
