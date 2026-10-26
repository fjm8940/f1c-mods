# Claude independent format spot-check (2026-10-09)

Purpose: ground truth to check DS STOP 2 schemas against. Read-only, in place. Tier: OBSERVED on one file each (not yet tied to p-code).

- `Reference/reference_2002_hockenheim.lap` (487843 B): ASCII `CHECKSUM=<decimal>\r\n`, then uint32 LE 0x71 (113) = length of next block. Block is metadata text obfuscated: byte-5 per char, string reversed. Decodes to `VERSION=2003.14 TER=unknown VEH=2002 BAR TEAM= DATE=09-10-2026 12.43 TRACK=2002_Hockenheim DRIVER=Matthias Steppe`. Further reversed/shifted section follows with weather/aids/flags (`TRK_TEMP`, `RAIN`, `TRK_WET`, `AIDS=[...]`, `OFFTRACK=FAILED`, `UPLOAD_ALLOWED=NO`). Binary between blocks not decoded. Checksum algorithm unknown.
- `telemetry.spt` (1187709 B): bytes 0-7 unknown binary; float32 LE at offset 8 = 0.05 (candidate sample interval 20 Hz - unproven); then NUL-terminated strings: driver, car name, VEH path, track.
- `Tracks/*.dat`: the 136 DATs are per-track files (1999_*, 2002_* ...). Header uint16 2, then float32 triples (likely X/Y/Z track line points, ~-400..+250 m range) and 1.0 floats. Unproven.
- `2360498.svm`: plain INI setup (`[GENERAL]`, `Symmetric=`, `CGRightSetting=`...).

DS STOP 2 must confirm or correct each line from reader p-code.

## Live probe attempt (2026-10-09, read-only)

- Game PID 34576 race running (f1c-memory health ok, sim_time 9.07 s, 22 cars).
- web_telemetry rc.2 loaded and sampling, but HTTP failed: log `bind port 8080 failed: 10048`. Port 8080 held by `llama-server.exe` (PID 14736, local LLM). Web Telemetry unreachable while llama-server runs on 8080. Fix: move llama-server to another port, or make web_telemetry port configurable/fallback (candidate design item for STOP 3).
- RPM scale check NOT done: no read-only front-door recipe for engine RPM (runtime-read offers CurrentGear, ForwardGearCount, lapNumber only); field-viewer service not running. Open: add a reviewed `EngineRPM` runtime-read recipe (offset 0x39A0, x 9.549296585513721) then compare to rev limit.
- 11:27 UTC: f1c-memory `f1c_drivers` advancing (heartbeat 3942, sim 221 s) but exposes no engine RPM. `runtime-read` with a TEMP-only probe recipe (EngineAngVelProbe float32 @0x39A0) refused: `OpenProcess access denied` (needs game integrity level). No workspace recipe was added. Pending: owner runs the probe from an elevated shell.
- RESULT (elevated, 3 snapshots): 0x39A0 = rad/s for player and AI. Player idle neutral 418 rad/s = 3990 RPM; AI 1217-1935 rad/s = 11.6k-18.5k RPM across gears 1-7. Web Telemetry ~1700 "RPM" is rad/s. Finding `engine-rpm-live-scale-20261009` (OBSERVED) appended.
