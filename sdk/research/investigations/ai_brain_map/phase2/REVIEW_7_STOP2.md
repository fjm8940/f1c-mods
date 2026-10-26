# Claude review 7 — phase 2 stop 2 (C2 fix, C4, C5), 2026-10-02

Verdict: **PASS.** C2 wording fixed. WRITE_READINESS hook positions match Claude's reading (0x4424 after stage 5,
0x4548 before stage 6, 0x4484 after the stage-4 reset and before stage 8, outputs post-hook). Coverage 32.8 % is an
honest number; the gap list is the right next step.

## C6 — build ABI v1.1 (new task, static only)

The 88 proposed fields are all `unnamed_xxxx`, but many already have names in prior art. Before merging:
1. For each proposed field, look up `python tools/f1c_mod.py inspect` / `name_index.json` / `findings.jsonl`
   (`evidence-query findings 0xHEX`). Use the existing name as `id` (snake_case) and copy its tier when the source
   has one (FACT / CONFIRMED / OBSERVED / DERIVED); otherwise keep GUESS. Cite the source in `source`.
   Known examples: 0x4068 = ReasonCode (finding: crash trials, value 6 accident) - NOT "pit-request state";
   0x3D70..0x3D7C = AI lifetime countdowns engine/brakes/gearbox/suspension (mechanical_failures handoff, findings);
   0x4060 position; 0x403C lap number; 0x409C best lap.
2. Fields that stay unnamed keep `unnamed_xxxx` and tier GUESS.
3. Extend `build_abi.py` (keep v1 output byte-identical) to also write `ai_brain_abi_v1_1.json` = v1 fields + the
   reviewed new fields, same schema, plus `"version": "1.1"`. Extend `test_abi.py` to test both files.
4. Recompute COVERAGE.md for v1.1.
Do not edit ai_brain_abi_v1.json. The viewer/recorder load v1.1 automatically when the file exists.

Then stop. C1 live runs remain (Matias names the situations).
