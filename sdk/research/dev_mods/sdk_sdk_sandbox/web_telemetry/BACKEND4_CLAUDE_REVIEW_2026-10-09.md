# BACKEND 4 review (Claude, 2026-10-09)

Verdict: ACCEPTED - backend ready for owner live acceptance. Not final until L1-L15 pass live.

Verified (fact): payload %TEMP%\f1c-web-telemetry-4.2.0-rc.10\payload = DLL 1BA4C532...1B5B, dll.mod.json B5C6B1AD..., ini 26F66E10..., sidecar C095A07E...; installed still rc.2; live_check.py present; checklist L1-L15 + restoration present.

Checklist wording fixes (DS, during result recording):
- L1 step 5 says open :8080; if llama-server holds 8080 the server binds 8081 (L3). Use the port shown in the log / footer.
- Restoration "remove the added sidecar": move it to the backup/quarantine folder, never delete.
- %TEMP% may be cleaned: copy the payload to the dated backup/evidence folder before installing, or rebuild (builds are reproducible) and re-check the hashes.

## Install attempt stopped (Claude, 2026-10-09, game closed)

Observed (fact): plugins\lightloader\ no longer contains web_telemetry.dll (rc.2 BA87BC2E... was present earlier today), web_telemetry.dll.mod.json, web_telemetry.ini, nor f1c_game_update_broker.dll (only its .f1c-sdk-owned.json remains). Last web_telemetry log line is from the 18:5x-19:04 session. No copy of rc.2 found in the quarantine or the BACKEND4 install folder. Cause unknown (Lightloader Manager action? another agent?). rc.10 requires broker ABI 15, so installing web_telemetry alone would not work. Nothing installed or changed; waiting for the owner.
- Resolved (owner, 2026-10-09): the game update broker is built into Lightloader; a missing f1c_game_update_broker.dll in plugins\lightloader\ is normal. rc.2 had been moved to the Desktop by the owner; backed up and rc.10 installed (see INSTALL_RESTORATION.json).
