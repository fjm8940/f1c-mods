# Telemetry.doc vs executable: extraction and comparison

Date: 2026-10-09. Source manual: `%USERPROFILE%\Documents\F1 Challenge V10\Options\Telemetry.doc`
(SHA256 `3A0B8B1FD84181DCF5A552A54915B6FBF0B40FB62495EC69C7AA285D5C4FD541`, 1,673,216 bytes, read-only).

## Extraction method

- A minimal own CFB (OLE2) reader was written under
  `%TEMP%\f1c-legacy-cfb\cfb_extract.py` (no third-party packages, no installs).
- The container is a valid CFB v3 file with streams `WordDocument` (1,652,748 B),
  `\x01CompObj`, `\x05SummaryInformation`, `\x05DocumentSummaryInformation`.
- The FIB identifies **Word 6.0/95** (`wIdent 0xA5DC`, `nFib 104`), so there is no
  Clx piece table; the text runs from `fcMin 0x300` to `fcMac 0x34E5` as ANSI.
- Extracted text: **12,773 characters**, UTF-8 SHA256
  `34BF078C403EE3676D9661C27197638C72024D2C46E151C2DD6843BC3BC750CB`
  (text file kept at `%TEMP%\f1c-legacy-cfb\Telemetry_doc_text.txt`).
- Machine summary: `TELEMETRY_DOC_EXTRACTION_2026-10-09.json` (this directory).
- Figures: a strict raster scan (JPEG/PNG/GIF/validated BMP) and a Word 6/95 PICF
  scan found **no embedded figures**. Captions remain in the text (for example
  "The image below shows a reference lap (in grey)..."). The two initial
  BMP-magic candidates were false positives; strict header validation rejected
  them. Figure extraction remains an open item.

## Documented intent vs executable evidence

| Documented (manual) | Executable / file evidence | Verdict |
|---|---|---|
| Offline use: "drive a few laps, then enter the Vehicle Setup page and click the Telemetry icon... import your telemetry" | Viewer reads `telemetry.spt` (debug.log: "reading from telemetry file"); no live-interface evidence found; imports via `(*.spt)` and `(*.lap)` dialogs | Consistent: offline import viewer |
| Default view "CHASSIS OVERVIEW" (track position, velocity, longitudinal/lateral G, RPM) | `overview.ini` = `*00_Overview` "Chassis Overview" with x/z map, lat/long dots, velocity and engine-rpm graphs | Consistent, exact title match |
| View names: Track View, Track and Velocity Trace, Driver Inputs, Weather, Damper Velocities (+Smoothed), Friction Circle, Chassis Accelerations XYZ, Front and Rear Ride Height (+Smoothed), Chassis Slip Angle, Suspension Travel, Tyre Temperatures (Inside/Middle/Outside, Averaged, front/rear), Tyre Wear, Wheel Spin, Engine and Gearbox Information, Incremental Time Difference | All present 1:1 as layout INIs whose own titles repeat the manual names (`track.ini` "Track View", `basic.ini` "Track and Velocity Trace", `chassis_inputs.ini` "Driver Inputs", `track_conditions.ini` "Weather", `dampers_all.ini`, `dampers_all_filtered.ini`, `chassis_gg.ini` "[Advanced] - Friction Circle", `chassis_gxyz.ini`, `chassis_rideheight.ini`, `chassis_rideheight_filtered.ini`, `chassis_slip.ini`, `susp_deflection_all.ini`, `tyre_temp_all/front/rear/tyre_temp.ini`, `tyre_wear.ini`, `wheels_slip.ini`, `chassis_gears.ini`, `time_diff.ini`) | Consistent; the layout files are the view catalogue |
| "Velocity (Speed) is measured in MPH" | `f1_telemetry.cfg` comment: "set this to 1 to show mph... (default=kph)"; config value `display_in_mph = 0` | Discrepancy: manual says MPH, shipped default is kph with an option |
| "Tyre wear is measured from 1.00 to 0.00" | Native wear model floor is 0.75 in the game's own code (`FuelConsumptionAndWear_004ed960`); viewer display may rescale | Nuance: viewer scale vs native model floor differ; do not assume the native floor is what the viewer draws |
| Damper velocities: "positive = rebounding (ride height increasing), negative = compressing" | Not previously established; no contradicting evidence | New semantic evidence for the modern workbench channel labels |
| Temperatures in degrees Centigrade; optimum per compound (Hard 114, Soft 112, Intermediate 109, Wet 107, Monsoon 105) | Native tyre/brake temperatures are Kelvin in memory (HUD subtracts 273.16) | Consistent conversion path; compound optima are manual-only knowledge |
| Rain/dampness 0-100%, 0% = dry | Not yet verified against executable channels | Open |
| Reference lap "colour coded grey, prefixed with *"; fastest lap colour coded; per-lap colour assignment; save/load laps; check lap validity; set reference | `Reference\reference_*.lap` / `Default_*.lap` stores, `*.lap` dialogs, `REFERENCE LAP` string, `set as reference lap` string | Consistent |
| Selection/cursors: drag to select a portion, RETURN zoom in, SPACE reset, BACKSPACE unselect, right button zoom out, button 3 maximise | Viewer behaviour is p-code; layout files only carry graphs | Consistent with the workbench's linked-cursor requirement; exact key handling unverified |
| Save lap writes to disk; "Imported telemetry is not stored permanently" | `telemetry.spt` is the import store (rewritten each run); laps saved as `.lap` files | Consistent |

## Follow-ups for the reconstruction

1. Figure extraction (Word 6/95 picture records) — not found by raster/PICF scan;
   investigate PICF variants or OLE object storage if figures are required.
2. MPH/kph discrepancy: confirm the actual axis unit in the running viewer
   (config default vs manual) before labelling the modern workbench axes.
3. Tyre-wear display scale (1.00-0.00 per manual vs 0.75 native floor) needs a
   live or file-level check of the `.spt` wear channel.
4. Damper sign convention from the manual should be recorded in the channel
   ledger as documented intent, tier DERIVED, until a live/file sample confirms.
