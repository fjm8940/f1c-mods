# Readable JSON — 0.2.1, 2026-09-14

Owner reported one-line race output. Changed structural serialization to support indentation and publish all race summaries/checkpoints with two-space indentation and a trailing newline. Journal entries remain compact, one event per line, as required by JSONL. No schema or collected-value changes. New producer version0.2.1 identifies the build.

Canonical candidate: [build_20260914_223619_009](../trials/build_20260914_223619_009/), DLL SHA256 `0c4f2928698f9c794d876763083bc905692f7fc011c1e34cbeec56a9237bdc88`. Full component gate passed, including strict parsing of the actual indented Storage.publish output and equality against the compact fixture; inherited lifecycle/SC/RF tests and DLL guard pass. [Installed Narrative report test](../trials/build_20260914_223619_009/installed-control.log) passes. SDK surfaces unchanged; no repeat full SDK gate for this serialization-only change.

Game initially running; owner closed it normally. [Installation manifest](../trials/build_20260914_223619_009/installation_20260914_223718.json) records verified copies and old installed DLL/bridge before-states. DLL installed at Documents/F1 Challenge V10/plugins/lightloader/race_data_collector.dll. Enabled setting retained; next start loads the new binary. Bridge content unchanged by formatting.

Also indented the existing native Silverstone2002 completed JSON in game-root RaceData. [Existing-export manifest](../trials/build_20260914_223619_009/formatted-existing-manifest.json) records file, original backup, before/after hashes and parsed-value equality. Reformatted the same file rather than adding a duplicate import. Original preserved under workspace_organization_audit/RACE_JSON_FORMAT_BEFORE_20260914_223718. Legacy malformed Java exports and recovery journals were untouched. No race data was added to Season Narrative's database.

Restoration: preserve any later revisions, then copy the complete before-states named in the manifests back while the game is closed. No deletion required. Maintained-document before-states are under workspace_organization_audit/RACE_DATA_FORMAT_BEFORE_20260914_223640. Native gameplay acceptance remains pending; the existence of a native JSON export is not proof of complete field/lifecycle accuracy.

Documentation gate: README/status link this record and all evidence paths exist. Root validator retains ten known unrelated errors. Wasted effort: no material repeated large reads, broad SDK reruns, failed/retried commands or idle polling; component rebuild was necessary for the serializer change.
