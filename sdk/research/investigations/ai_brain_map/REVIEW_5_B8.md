# Claude review 5 — B8 AI Brain ABI v1 registry (2026-10-02)

Verdict: **PASS after one fix.** test_abi OK (55 fields, 5 views). Groups, tiers and views are usable.

Fix (required):
- `position_x/y/z` point at 0x34D4/0x34D8/0x34DC (written by the path-follow/placement function). The car's live
  world position used by the whole brain (locator, steering law, watchdog, brainlog.py) is **0x34E0 (x), 0x34E4 (y),
  0x34E8 (z)**: sub_426430 L71-L98 + L170-L173, sub_426100 L16-L27, SteeringAngleController (mode-6 branch + LABEL_30 copy). Make 0x34E0/E4/E8 the
  `position_*` fields (tier DERIVED, source those lines; brainlog live use = OBSERVED is fine) and keep 0x34D4..
  as `placement_pose_*` in group `state` with tier GUESS. Add velocity 0x34F8/0x34FC/0x3500 (DERIVED, sub_426430 L72-L75 speed calc).
- Update test_abi.py to assert `position_x.hex == "0x34E0"`.

Accepted as is: tiers (OBSERVED only where doc 54 s2.1 or FACT findings exist), derived views, README GUI rule.
Not needed: rewriting B5 maps/glossary.

After the fix: done. Stop. Next phase (GUI) gets its own handoff from Claude.
