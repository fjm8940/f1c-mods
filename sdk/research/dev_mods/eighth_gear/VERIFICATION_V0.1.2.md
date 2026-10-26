# Verification record — v0.1.2 saved-setup correction

Date: 2026-08-25  
Result: **AUTOMATED PASS; INSTALLED FOR HUMAN VALIDATION**

The active `tempGarage.svm` and `2005RH Australia.svm` files contained gear
settings only through gear 7. The v0.1.2 overlay adds explicit
`Gear8Setting=73` (`1.000000`) to both, preventing F1C from synthesizing the
eighth setting through its seven-to-eight mismatch interpolation path.

- Files installed and verified: 2
- Focused tests: 7 passed
- Ratio catalogue changed: no
- Restoration map:
  `trials\2026-08-25\clean-v0.1.2-australia-svm-install\RESTORATION_MAP.json`
- Distribution status: ``

Human validation must confirm that the 7th-to-8th shift now lowers RPM and the
engine survives.

## Wasted-effort check

- Repeated large reads: none observed.
- Unnecessarily broad builds/tests: none observed.
- Avoidable failed/retried commands: none observed.
- Polling/waiting/idle: none observed.
