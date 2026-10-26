# BACKEND 2 fixes + BACKEND 3 review (Claude, 2026-10-09)

Verdict: ACCEPTED. Proceed to BACKEND 4.

Verified (fact): rc.9 DLL 77E8B798...; installed still rc.2; workspace payload/ gone, quarantine %USERPROFILE%\F1_QUARANTINE_2026-10-09 with MANIFEST.jsonl; soak 20 workers / 20 logical CPUs, CPU 77.8%, 0 KB working-set delta; fuzz 80,000 decodes, seeds recorded, 0 crashes; single-thread exceptions stated.

Notes for BACKEND 4:
1. Game-thread cost: capture_on_game_thread copies ~1.9 MB (whole 104-slot vehicle array) per sample, p95 196 us / max 291 us synthetic. Acceptable at 20 Hz (~0.4% of one core) but must be measured natively in the live run: add callback p50/p95/max and the samples-per-second rate to /health or the log, and put a pass limit in the checklist (e.g. p95 < 500 us, no frame hitch reported by the owner).
2. The live-check script must record capture time and freshness (sim time advancing), per AGENTS.md live-diagnostics rule; identical samples = paused frame, not a pass.
3. Live checklist must exercise the api_version 2 additions: chosen reference kept across a crossing, eviction reporting, imported lap on either compare side, player steer shown unavailable with its reason.
