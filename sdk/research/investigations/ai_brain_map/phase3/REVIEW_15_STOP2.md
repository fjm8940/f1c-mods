# Claude review 15 — phase 3 STOP 2 (D2 + D3) + registry v1.2 (2026-10-02)

Verdict: **PASS with one rejection. Registry v1.2 built** (`ai_brain_abi_v1_2.json`, script
`phase3/merge_abi_v1_2.py`). Tiers: 23 OBSERVED / 67 DERIVED / 50 GUESS (v1.1: 15 / 68 / 57).

## Checks

- verify_maps, verify_maps_phase2, verify_maps_phase3 (51 maps, 0 mismatches), test_abi (now also checks v1.2:
  same 140 fields, same offsets, version 1.2), test_extract_offsets: all exit 0.
- Spot check `phase3/maps/map_RandomVariationScaler.md` against `functions/RandomVariationScaler_004bea10.c`:
  quoted lines match. Two gaps for D5: the map's callee list says none, but the function calls RandLCG twice and
  `sub_5881F0` (CRT x87 helper taking st0/st1, returns in st0); its return looks dropped by Hex-Rays (same class as
  DROPPED_ST0_SCAN), so the `v8` used afterwards may be wrong in the pseudo-code. Check disassembly before modelling.

## D2 decisions

- Accepted 21/22 DeepSeek entries (7 GUESS -> OBSERVED, meaning notes appended as "v1.2 live note (Monza, D3.2)").
- **Rejected:** `unnamed_45f8` "per-car pointer scratch". Prior art (`investigations/ai_interaction_role/README.md`
  2026-08-18 "+0x45F8 is a pit-interaction link") proves it is the same-team peer vehicle pointer. Replaced.
- Claude additions from `phase3/PRIOR_ART_MAP_2026-10-02.md` (17 fields): .rcd sources for 0x44CC (PASSING),
  0x44C4 (RACEABILITY), 0x4500/0x4504 (CORNERINGADD/MULT), 0x44B8 (STARTSDRY/WET); 0x4580 wet factor -> OBSERVED
  (finding ai-brain-wet-factor-4580-live-2026-10-02); 0x453C 4-second AIAbility dynamics; 0x4508/0x450C traffic
  inputs; 0x4404 AIAbility deadline; 0x4448 phase-change random factor; mode table (mode 2 = one-tick reset);
  0x45FC / 0x457C yield throttle cap.
- Type fixes: 0x44B0 float -> int (R1: -1 logged as NaN), 0x4554 _DWORD -> float (D1: 1.0 logged as int bits),
  0x45F8 -> pointer _DWORD. Not yet live-read-tested with the new types (next watcher start).

## Changed files

- New: `ai_brain_abi_v1_2.json`, `phase3/merge_abi_v1_2.py`, this review.
- `test_abi.py`: v1.2 check added.
- `mods/sdk_sandbox/ai_mind_monitor/brain_core.py`: prefers v1.2 (the watcher already running on the wet race keeps
  v1.1 until restarted).
- `docs/54-ai-driving-brain-map.md`: mode-2 row corrected.
- R1 closed by the type fix (no recorder code change needed).

## D3 notes for later

- 51 maps accepted as triage; GUESS names unchanged. Battle-correlated steering fields (0x455C, 0x4570-0x4579)
  go to D6 together with prior-art sections IR 1920-3281.

## Live confirmation (same day)

Wet Imola 1 Hz overview: Massa 0x45F8 = Villeneuve (Sauber teammate), and the only non-null 0x45FC sample in
5 laps is Massa -> Villeneuve on the same lap (request -3.0, cap 0.75) = Overtaking same-team fallback. Confirms
the 0x45F8 rejection above. Finding ai-brain-same-team-peer-45f8-live-2026-10-02.
