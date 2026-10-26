# BACKEND 2 review (Claude, 2026-10-09)

Verdict: ACCEPTED WITH 4 CONTRACT FIXES (api_version 2) before BACKEND 3 builds on it.

Verified (fact): rc.8 DLL 5764AEEE...A434 in TEMP and payload/; sidecar C095A07E...; installed still rc.2; 14 before-state files present; 11 schemas + version.json; contract covers all endpoints.

Fixes:
1. RPM on the live frame: `/telemetry` exposes raw `r/mr` plus a factor, so the client must convert. That repeats the original mislabel risk. Add converted `rpm` / `max_rpm` fields (RPM) and keep the raw fields renamed or documented as `*_rad_s`; schema units must say which. Test: no field named or documented as RPM carries rad/s.
2. Reference lap: "previous completed lap" is not what the owner asked for. The frontend must be able to choose ANY completed or imported lap as the reference and keep it across lap crossings. Give every lap a stable identity (session generation + car slot + lap number + source live/import) in status/data/compare, report ring eviction explicitly (an evicted reference must say so, never silently switch), and let /trace/compare take an imported lap as either side.
3. Steering and other unproven channels: `/telemetry` `steer` and trace `st` must be marked unavailable for the player (vf/null + reason) until the live steering check passes; AI steering input stays as documented. Add a schema/contract test for it.
4. Lean workspace: payload/ puts a candidate DLL inside the workspace. Candidate builds belong in %TEMP% (AGENTS.md retention rule). Keep payload/ as the staging layout in %TEMP%; in the workspace keep only the manifest with hashes and the build/verify scripts. Move the current payload/web_telemetry.dll out via retention quarantine, not delete.

Accepted as client-side by design (document in the contract): freeze = client pin; corner entry/min/exit speeds, brake/throttle points and run statistics derived from samples; delta cursor interpolation per DM#14.
