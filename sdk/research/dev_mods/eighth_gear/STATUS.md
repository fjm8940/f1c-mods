# Product status

- Product: F1C Eighth Gear
- Version: 0.1.2-dev saved-setup correction
- Pipeline stage: human validate
- Artifact maturity: development trial
- Canonical path: `development\mods\eighth_gear`
- Target: local `F1 Challenge V10` vehicle configuration
- Promotion status: not authorized
- Clean-test installation: v0.1 reached eighth gear in the Ferrari 2005 but
  caused extreme RPM. The HDV-only v0.1.1 ratio change did not correct it.
  v0.1.2 explicitly adds stock ratio `1.000` to the active Australia and
  temporary saved setups; the unrelated v0.2 candidate remains withdrawn.

## Acceptance criteria

1. Every six-speed and seven-speed HDV in the selected source gains
   `ForwardGears=8` and one valid `Gear8Setting`.
2. The selected eighth ratio continues the sixth-to-seventh ratio progression
   using an existing, lower ratio from the target's own catalogue.
3. Unsupported HDVs and the source installation remain byte-for-byte unchanged.
4. The output is reproducible, hashed, source-attributed, and labelled
   ``.
5. Focused automated tests and the workspace/SDK gates are recorded.
6. A guarded full-weekend human test confirms player, automatic gearbox, AI,
   HUD, garage, and restart behavior before any promotion.

## Current blockers

- Live guarded-weekend gameplay validation has not been performed.
- Owner correction: the failed test used the Ferrari 2005, whose installed HDV
  already declares `ForwardGears=8` and a valid `Gear8Setting=67`.
- The earlier Toyota 2002 diagnosis was wrong. The v0.2 six-speed expansion
  does not address the observed Ferrari failure and must not be installed as a
  fix for it.
- Saved setups omitted `Gear8Setting`, causing F1C to synthesize it through its
  mismatch interpolation path instead of using the corrected HDV default.
- The explicit saved-setup fix still requires human ratio validation.
- The v0.1.1 clean-test installation has not received human gameplay approval
  or promotion approval.

## Next gate

Repeat the guarded Australia/Ferrari test while monitoring the 7th-to-8th RPM
change and engine survival.
