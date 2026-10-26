# External tool review: morluto/rea (2026-10-09)

Source: https://github.com/morluto/rea (MIT, npm `rea-agents`, Node 22+). Shallow clone inspected in %TEMP%; nothing installed.

## What it is (verified by reading repo)
- One MCP server + CLI wrapping RE backends: Hopper, Ghidra (Windows support marked experimental), IDA (via mrexodia/ida-pro-mcp, GUI-attached or idalib headless).
- Also: JS/Electron, websites, HAR captures, .NET metadata, APK, firmware, Linux crash/ELF diagnostics, process capture (Linux/macOS only).
- Results returned with evidence + limitations envelope. IDA mode checks input SHA-256 and image base per request.
- `npx rea-agents setup` edits agent configs (with backups).

## Fit for F1 (inference)
- Native F1C work already covered: f1c_ida_decompiled exports + SDK front door (`f1c_mod.py inspect`, findings.jsonl). REA adds a generic layer over IDA, not new F1C knowledge.
- Possible borrows: (1) per-request binary hash + image-base check pattern for our IDA tooling; (2) evidence/limitations result envelope as model for SDK outputs; (3) headless idalib multi-binary querying (e.g. compare game versions) if ever needed.
- Not useful: Linux-only runtime/crash tools, APK, EVM, firmware, web.
- Risk: setup rewrites global agent config; adds Node stack. Would need owner approval.

## Status
No adoption decision. Recommendation: do not install; optionally borrow patterns.
