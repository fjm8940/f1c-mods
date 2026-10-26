# STOP 2 revised review (Claude, 2026-10-09)

Verdict: ACCEPTED WITH CORRECTIONS. Proceed to STOP 3 after items 1-3.

Verified (fact): `test_legacy_formats.py` and `test_vb6_pcode.py` pass; no viewer process running.

Corrections:
1. Wrong attribution. telemetry.spt is being written by the GAME (F1 Challenge Online.exe PID 34576, race running), not the viewer: no f1_telemetry.exe process; file mtime = now (14:37:45), size 1,970,176 -> 1,982,464 in 5 s (~2.4 KB/s, 4 KB buffered chunks); bytes 0-7 currently zero (header likely finalized at close). Correct the STOP2R record/finding. Consequences: (a) the game has a native telemetry recorder = writer code is in the GAME exe; find it via SDK front door (string "telemetry.spt", fopen/fwrite callers) - it gives the authoritative record layout and per-field source offsets; (b) record stride can be measured from growth rate x 0.05 s and confirmed from the writer; (c) decode only a closed file or a hashed TEMP snapshot.
2. .lap obfuscated block is decodable (see CLAUDE_FORMAT_SPOTCHECK): each byte minus 5, then reverse the string. Implement + test; then decode the remaining sections.
3. Use finding `engine-rpm-live-scale-20261009` (OBSERVED): 0x39A0 is rad/s for player and AI; RPM = x 9.549296585513721. Do not re-investigate; cite it in the STOP 3 channel ledger.

STOP 3 also must include: Web Telemetry port 8080 conflict (`bind port 8080 failed: 10048`, held by llama-server) - design configurable port / visible bind failure.
