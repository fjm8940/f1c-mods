# Verification record — v0.2.0 correction

Date: 2026-08-25  
Result: **AUTOMATED PASS; CANDIDATE WITHDRAWN — DO NOT INSTALL**

## Owner correction

Matias clarified that the failed v0.1 drive used the Ferrari 2005, not the
Toyota 2002. The Ferrari's installed HDV already contains eight forward gears,
so expanding six-speed cars cannot correct the reported failure. This artifact
is preserved as a reproducible experiment, but it is not an approved fix.

## Corrected behavior

- Original seven-speed cars: retain the v0.1 conversion.
- Original six-speed cars: receive calculated seventh and eighth ratios.
- Added catalogue ratios: 0.950, 0.900, and 0.850.
- Existing catalogue indices remain unchanged because the additions follow the
  stock minimum ratio of 1.000.

## Exact incremental artifact

- Trial: `trials\2026-08-25\clean-v0.2.0-overlay-incremental`
- Tree SHA-256:
  `ed2993c43200cc3e9c66a1188b0700285d095b7e3365f2e2c166d5621e83a9d4`
- HDVs scanned: 135
- Six-speed HDVs converted: 84
- Existing v0.1 eight-speed HDVs left intact: 51
- Manifest changes: 85 (84 HDVs plus `gear_ratios.ini`)
- Focused tests: 5 passed

## Toyota 2002 proof

The generated Toyota 2002 HDV contains:

```text
ForwardGears=8
Gear5Setting=60
Gear6Setting=69
Gear7Setting=74
Gear8Setting=76
```

Index 74 is ratio 0.950 and index 76 is ratio 0.850.

## Installation decision

Do not install this candidate. Its premise was invalidated by the owner's car
identification. Runtime investigation of the Ferrari is the next gate.

## Wasted-effort check

- Repeated reading of large material: none observed.
- Unnecessarily broad builds/tests: none observed.
- Avoidable failed/retried commands: the first revised test run exposed a
  fixture ratio-format error and was corrected; no target files were touched.
- Polling/waiting/idle: none observed.
