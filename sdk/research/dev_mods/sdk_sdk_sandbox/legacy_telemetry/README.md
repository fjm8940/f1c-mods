# legacy_telemetry — isolated analysis project for the vendor viewer

Purpose: hold the **isolated IDA database** and provenance for the legacy F1
Challenge telemetry viewer (`f1_telemetry.exe`) that the Web Telemetry
reconstruction assignment dismantles. This project is analysis-only; it is not
a mod, not a product, and contains no runtime code.

Boundary rules (from `DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`):

- The vendor tree `%USERPROFILE%\Documents\F1 Challenge V10\Telemetry\` is
  **read-only**. Nothing here writes to it.
- This IDB is **not** the game IDB. Viewer findings must always carry the
  viewer SHA256 and must never be written into game function exports,
  `functions/`, or game findings as if they were game code.
- The IDB was created with the canonical front door:
  `python tools/f1c_mod.py ida-new-project legacy_telemetry --exe "…\f1_telemetry.exe"`.

## Artifacts

| Artifact | Value |
|---|---|
| Target EXE | `%USERPROFILE%\Documents\F1 Challenge V10\Telemetry\f1_telemetry.exe` |
| Target SHA256 | `0AA34C51C30C3113BF6B6C2F1B59A8B3346AD1322F6AB519E60757BAF8573BEB` |
| IDB | `analysis/f1_telemetry.i64` |
| IDB SHA256 | `3E66F6DC306FFC6B0086D784FA5F81169A4D7D15C333544957D153A0387E7985` |
| Provenance | `analysis/f1_telemetry.provenance.json` |
| Survey (functions/strings/imports/edges) | `../web_telemetry/legacy_dossier/SURVEY_f1_telemetry_2026-10-09.json` |

## Findings so far

The viewer is a **Visual Basic 6 p-code application** (imports are
MSVBVM60.DLL-only, `VB5!` project header, 1,646 bytes of native glue in a
331,776-byte image). Application logic executes as VB p-code; native function
coverage and the p-code boundary are recorded in
`../web_telemetry/legacy_dossier/FUNCTION_COVERAGE_f1_telemetry_2026-10-09.md`.

Read-only queries, e.g.:

```powershell
python tools/f1c_mod.py ida-query decompile 0x004099B0 --idb mods/sdk_sandbox/legacy_telemetry/analysis/f1_telemetry.i64 --json
```

Never invoke `idat`/`tools/*.py` directly for this project.
