# F1C Eighth Gear

Development-only configuration mod for F1 Challenge 99-02 / F1 Challenge V10.

## Objective

Give every six-speed and seven-speed F1 vehicle a usable eighth gear. Existing
six-speed cars receive calculated seventh and eighth ratios; existing
seven-speed cars receive a calculated eighth ratio. Each new ratio continues
the vehicle's existing top-gear spacing.

## Safety and scope

- The source game directory is read-only input.
- The builder writes a separate overlay and refuses to overwrite an existing
  output directory.
- Six-speed and seven-speed vehicles both become eight-speed vehicles.
- The generated overlay adds reviewed `0.95`, `0.90`, and `0.85` overdrive
  ratios after the stock catalogue, keeping every existing ratio index stable.
- Generated overlays are `` until gameplay validation and
  release gates are complete.
- No executable patch, injector, DLL hook, or production file is used.

The configuration path is supported by stock F1C: the vehicle parser recognizes
`ForwardGears` and `Gear8Setting`, while the stock gearbox contains a separate
ratio lookup path for gear numbers above seven.

## Build a trial overlay

From this directory:

```powershell
python tools\build_overlay.py `
  --game-root "%USERPROFILE%\Desktop\F1\F1 Challenge V10" `
  --output "trials\2026-08-25\v0.1.0-overlay"
```

The output contains only modified HDV files, a machine-readable manifest, and
a `` marker. It does not alter the game installation.

## Verification

```powershell
python -m unittest discover -s tests -v
```

Automated checks prove transformation and preservation behavior. Human testing
confirmed that the Ferrari 2005 can select eighth gear, but the v0.1 ratio
caused an engine-overspeed failure. The v0.1.1 trial therefore uses the lowest
stock catalogue ratio (`1.000`) for that car. Manual and automatic upshifts, AI
use, HUD display, top-speed behavior, garage setup behavior, and session restart
safety still require validation in a guarded full race weekend. QuickRace is
not an approved test path.

## Provenance

Generated HDV overlays are derived from the owner's local game data. They are
not represented as original first-party vehicle data and are not approved for
redistribution at this stage.
