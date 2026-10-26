# Read-only wetness adapter — 2026-09-08

`f1c_weather.h` reads a bounded eight-byte pair at 0x008AFEE8/0x008AFEEC through the caller reader. Evidence: canonical Red Flag v76 source ON_PATH_WETNESS/OFF_PATH_WETNESS and its existing AIW metric consumer. Exact target SHA256 1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003 only; caller establishes target and stable game callback. No broker ABI/hook changes or native writes.

This is existing-consumer code evidence, with synthetic verification; fresh native weather validation is pending. Wetness does not establish visibility, rain intensity, or passability. Invalid/null/unreadable/nonfinite/out-of-range pairs return valid=false. `tools/f1c_mod.py sdk-test --exe <supported exe>` now includes `test_weather.py`: exact address/length, range boundaries and failure cases. Canonical gate passes; initial x87 equality assertion corrected to numeric tolerance, initial evidence retained.

Consumer and lifecycle/calibration limits: [Safety Car weather record](../../../development/mods/full_safety_car/WEATHER_SC_2026-09-08.md). Preservation mapping is linked there. No Red Flag modification. Header and test are discoverable through this SDK README and AUTOMATION_CHECKPOINT.json.
