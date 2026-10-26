# Research record — how CMT 2026 F1C implements eighth gear

Date: 2026-09-21
Source inspected (read-only, outside workspace):
`%USERPROFILE%\Documents\CMT 2026 F1C (8th gear)\F1 Challenge 2026 CMT`
Owner observation: the car shifts into 8th with no issues in this installation.
Method: read-only inspection of exe strings/hashes, HDV gear blocks, gear-ratio
files, engine files, and 50 saved setups. No game or workspace file modified.

## 1. Custom per-car gear-ratio files

Every 2026 car HDV points at a private ratio file through the (stock) HDV field
`GearFile=`, placed before the gear settings exactly as the stock format
requires:

```text
GearFile=2026_gear_ratios_1.ini
FinalDriveSetting=1
ReverseSetting=0
ForwardGears=8
Gear1Setting=0
...
Gear8Setting=7
```

Verified: 33/33 vehicle HDVs in `CMTSeasonData\Vehicles` declare
`ForwardGears=8`; each team has its own `2026_gear_ratios_<N>.ini`
(1..11, plus an `L` league variant) with exactly eight entries:

```ini
[GEAR_RATIOS]
ratio=(18, 86)    //0 = 4,77778
...
ratio=(42, 65)    //6 = 1,54762
ratio=(46, 63)    //7 = 1,36995

[FINAL_DRIVE]
bevel=(50, 44)
ratio=(20, 67)    //0 = 3,350
```

The same stock tooth-pair format `(teethA, teethB)` as
`F1 Challenge V10\SeasonData\Vehicles\gear_ratios.ini` is used, but the file is
purpose-built for eight speeds. No stock catalogue index reuse, no adding
entries after the stock minimum, no index-stability constraint: the eighth
ratio is simply entry 7 of the car's own file. Gear 8 (`1.36995`) is one
normal spacing step above gear 7 (`1.54762`).

Observed inconsistency (not runtime-verified): the `L` league file contains
only seven ratios (0..6) while `2026_League.hdv` still sets
`Gear8Setting=7`, an index that does not exist in that file.

## 2. Patched executable

`F1C_CMT.exe` is a modified F1 Challenge 99-02 build:

| Property | Stock (`modified f1 challenge 99-02.exe`) | CMT |
|---|---|---|
| SHA-256 | `3D9592EEA277F4D3D986CEF8C188632ED9AE175ABCB0990CF03A0B3C54B88927` | `B3A7390AB9E9A23C348CD56912A33F91DB1840F9FD46BC9AA82249655F221BA0` |
| Size | 2,027,520 bytes | 2,764,800 bytes |
| Version | 1.0.2.6 | 1.0.2.6 |

The settings-name table around `Gear7Setting` was extended beyond the stock
seven entries:

- Stock: `ReverseSetting, FinalDriveSetting, Gear7Setting ... Gear1Setting, ForwardGears, GearFile`
- CMT: same, plus `Gear8Setting`, `Gear9Setting`, and a partial `Gear10S` remnant
  (raw bytes: `Gear8Setting\0Gear9Setting\0Gear10S\0\0\0\0`).

The stock executable contains no `Gear8Setting` or `Gear9Setting` literal in
either ANSI or UTF-16 form. The CMT build adds them at the gear-settings table,
so the setup/gear code whose name table was hard-limited to seven gears can
address more gears. This is the clearest implementation difference from a
config-only patched install.

## 3. Saved setups remain seven-gear

All 50 `CMTSave\*.svm` files (e.g. `2026CMTAustralia.svm`) contain
`Gear1Setting=0` .. `Gear7Setting=6` and no `Gear8Setting`. The car still shifts
to 8th in this installation, so in the CMT build the running gearbox takes gear
8 from the HDV default (`Gear8Setting=7` into the car's private 8-entry
GearFile) and the saved-setup key set does not need to grow.

## 4. Supporting configuration

- `CMTSeasonData\Vehicles\Ferrari\2026_SF-26\2026_engine.ini`:
  `RevLimitRange=(11250, 100.0, 11)`, `RevLimitSetting=5` — the 2026 engine is
  capped at 11,250 RPM, well inside a range where an 8th step of
  `1.54762 -> 1.36995` keeps RPM safe.
- No Lua, batch, or plugin code in the installation references gears
  (search of all `*.lua` and `*.bat` found no gear logic).
- `SHA256SUMS.txt` references `dinput8.dll` and Lightloader plug-ins
  (`f1c_game_update_broker.dll`, `redflag.dll`); the Lightloader bootstrap DLL
  is not present in the installation root, so these are not the gear mechanism.

## 5. Implication for F1C Eighth Gear (development\mods\eighth_gear)

Verified facts that differ from our config-only approach:

1. CMT gives each car a private `GearFile` with exactly eight ratios instead of
   reusing the shared stock catalogue. This removes every
   index-stability/interpolation problem our trials hit.
2. CMT ships a patched executable whose per-gear settings table was enlarged
   from seven to at least nine slots. We relied on the stock executable, which
   has no `Gear8Setting` name slot at all.
3. CMT's car setup still saves only seven gear keys, and 8th still works in
   their build; the missing key is not inherently fatal when the executable
   knows about gear 8.

## Evidence tier

- Verified now (read-only): file contents, hashes, string-table bytes, HDV and
  svm counts, engine values.
- Not verified: which exe code path consumes `Gear8Setting`; whether the stock
  executable could ever shift to 8 cleanly with only HDV/GearFile changes;
  runtime behavior of the `L` league file's out-of-range index.
- Owner-reported (not independently reproduced here): driving shifts to 8th
  with no issues in the CMT installation.
