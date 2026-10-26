# Claude review 12 — C9 (2026-10-02)

Verdict: **PASS.** verify_maps_phase2 0 mismatches (re-run). Map notes additive, tiers unchanged.

Interface fix by Claude: brain_core.tire_block() now writes `values = {"0x3A7C": float, ...}` (+ `ints` same keys),
the format c9_a_retest.load_static() reads. Synthetic check: 182 offsets parsed incl. 0x3C0C and 0x3D48.
(Before the fix the parser would have found no offsets in the old `floats` list format.)

Next: wait for a recorded race with cars_static.jsonl, then run c9_a_retest.py and report meas/pred. Stop until then.

Addendum (Claude, 2026-10-02, after the Monza run): c9_a_retest.load_static() treated a multi-line JSONL file that starts
with '{' as one JSON document and crashed (JSONDecodeError: Extra data). Fixed: one_doc = starts with '[' or (starts with
'{' and no newline). Before-copy: %TEMP%\c9_a_retest_BEFORE.py. Live check on watch_race4full: 19 drivers parsed, no
required scalars/tables missing.
