# LLM workflow and tooling review — 2026-09-30

Pipeline stage: orient/investigate (review only). No code, SDK artifact, IDB,
game install or production file was changed. Owner question: what would make
LLM-driven work faster, how good are the custom tools and SDK, and would more
static analysis / IDA scraping pay off.

## Measured facts (this session)

| Check | Result |
|---|---|
| Workspace validator | **FAILS, 11 errors**: unexpected top-level `.pytest_cache`, `f1 challenge testing versions`, `trials`; `F1 Challenge V10_clean` missing; 6 source-like `.py` files under `production\bug_report_site\v1.0.0/v1.0.1` and `production\lightloader\telemetry-service-v1-20260923`. Every cold start begins on a red gate. |
| `f1c_mod.py start --json` | 0.16 s, 2.7 KB. Good size. **Stale content**: first objective is "live-validate ESC pause menu", yet the checkpoint records ESC v1.0.2 published 2026-09-23. `AUTOMATION_CHECKPOINT.json` `updated_at` = 2026-08-24 while entries run to 2026-09-23. |
| `evidence-query offset 0x4620` | 0.35 s, tier-tagged, joins findings. Good. |
| `sdk-query field DriverName --json` | 0.18 s. Good. |
| `inspect 0x4C6E80` | **Fails** (0 matches). `004C6E80` and `sub_4C6E80` work. `source_explorer.resolve_address` requires exactly 8 hex digits, but docs and LLMs write `0x4C6E80`. |
| `search SetDriverVisibility` | **48.9 s**. Python reads ~64 MB / ~22k files per call. `rg -l` over the same corpora: 0.27 s. |
| `sdk-test` | 55 s, passes. |
| `findings.jsonl` (prior-art index) | 587 lines, 15 malformed. **Newest dated entry 2026-08-08.** All September SC/RF/pit/ESC knowledge lives only in prose. |
| Mod documentation | 1,276 `.md` under `development\mods` (530 in `trials`). Safety Car: 159 top-level `.md`, 160 trial dirs, `README.md` 70 KB with ~318 lines of dated changelog before the project description. `CURRENT_STATUS.md` is also a changelog, not a state. `tools/sdk/README.md` (20 KB) has the same prepended-news shape. |
| Decompiler exports | `functions/*.c` dated 2026-07-23; 0 files use `F1CVehicle`; access appears as `this[4649]` / `this + 4109` (dword-scaled, decimal), so byte offsets such as `0x4620` are not greppable. 1,536 of 7,143 still `sub_*`; e.g. `sub_4C6E80` is `ResetDriverGridState` (LEGACY) in the SDK and "native vehicle clear" in reviews. |
| Lab IDB | `analysis\sdk_lab_idb\F1_Challenge_Online_sdk_lab.i64` last written 2026-08-06. Reviewed names/types since then have not flowed back. |
| IDA | IDA Pro 9.1 with `idalib.dll` installed; Python `idapro` package not activated. Headless access is available but unused by the front door. |
| Git | Workspace and SDK repos: last commit 2026-08-18. SDK: 47 tracked files modified plus untracked work. 55 `__pycache__` files tracked in the workspace repo. |
| MCP | `f1c-memory` is registered for Codex only (`~\.codex\config.toml`); Claude Code sessions have no MCP servers configured, so AGENTS.md "use registered tools" silently falls back to CLI. |
| Text corruption | Several generated docs lose spaces before digits (`Exact357-byte`, `Published2026-09-22`, `SHA2561A70…`) or carry mojibake (`â€”`). Harms grep and hash/address reading. |

## Assessment (inference, not measured)

- **Front door / tool design: strong.** One entry point, JSON output, route,
  recommend, tier-tagged evidence joins, fail-closed rules, a real test gate.
  This is better LLM tooling than most projects have. Weak points are
  performance (`search`) and input tolerance (`inspect`).
- **SDK: strong on safety, weak on discoverability.** 58 headers with exact
  byte fingerprints, thread checks, broker ABI negotiation. But the state of
  each header (tested / natively validated / experimental) lives in dated prose
  paragraphs, not in machine-readable fields next to the API.
- **Main bottleneck is knowledge fragmentation, not missing analysis.** The
  structured stores stopped being fed ~2026-08-08; since then, knowledge went
  into hundreds of dated prose files. Each session must re-read large
  changelog READMEs to find current truth.
- **Second bottleneck: human-validation round trips.** 2026-09-23 alone shows
  SC 0.10.80 → 0.10.88, mostly "installed, native appearance pending owner".
  Capture/key/click/session-config capabilities exist but are not composed
  into a scenario runner the LLM can run and inspect itself.

## Recommendations, ranked by speed gain per effort

1. **Re-feed the structured stores.** Backfill `findings.jsonl` from
   September trial/review docs (FACT/DISPROVEN/OPEN, with source links); repair
   the 15 malformed lines; make "append a finding" part of the definition of done
   in `start --json`.
2. **Split state from history.** Per product: a short machine-readable
   `STATE.json`/`CURRENT_STATUS.md` (current installed/public hashes, open
   gates, next action) regenerated on each change; move dated news to
   `CHANGELOG.md`. Apply to SC, RF and `tools/sdk/README.md`. Additive; old text
   preserved.
3. **Fix checkpoint freshness.** `start --json` should derive objectives from
   the newest entries or fail loudly when `updated_at` is older than the newest
   entry.
4. **Re-export the decompilation from a refreshed IDB** (the high-value
   "more static analysis"): apply the reviewed names from the SDK/overrides and
   the `F1CVehicle` type, then re-run Hex-Rays export into a *new* corpus
   directory (old `functions/` preserved). Typed field names make grep and LLM
   reading dramatically better. No new discovery is needed for this.
5. **Add an `ida-query` front-door subcommand via idalib** (read-only on a
   copied IDB): xrefs to address, struct-offset xrefs, fresh decompile, bytes,
   rename proposals written to a review file. Replaces bulk scraping with
   on-demand exact answers.
6. **Make `search` fast**: ripgrep prefilter to candidate files, then existing
   scoring. Expected ~49 s → <1 s.
7. **Accept `0x` + 1–8 hex digits in `inspect`/`resolve_address`.**
8. **Scenario runner**: compose guarded-weekend launch, session config, key
   injection, window capture, memory reads and log collection into one
   `scenario run NAME` producing a bundle (screenshots + JSON). The LLM can
   pre-check HUD text/state visually before asking the owner.
9. **Green the validator**: either allow-list the three root dirs and the
   production service `.py` files in `WORKSPACE_STRUCTURE.json`, or route them
   through `_human_review` (owner decision).
10. **Commit checkpoints** in both repos (additive, preservation-safe) and stop
    tracking `__pycache__` via `.gitignore` (files stay on disk). `git log`/`diff`
    is the cheapest "what changed" oracle an LLM has.
11. **Register `f1c-memory` for Claude Code** (`.mcp.json`) so both agents get
    the same live tools.
12. **Find and fix the space-stripping/mojibake source** in generated docs.

Not recommended: another broad batch IDA scrape / LLM prose fill. The 7,143
function corpus plus facts, strings, config schema and persistence map already
cover discovery; remaining value is in typing, naming propagation and
on-demand queries (items 4–5).

## Pending

All items are proposals for owner selection. Nothing was validated in game.
Wasted-effort check: one `du`/`git status` over the SDK tree timed out
(>120 s) and was re-run narrower; `search` cost 49 s once as a measurement.

## Addendum — reconciliation with the Codex roadmap (same day)

Read after this review: [LLM_OPTIMIZATION_ASSESSMENT_AND_ROADMAP_2026-09-30.md](LLM_OPTIMIZATION_ASSESSMENT_AND_ROADMAP_2026-09-30.md)
and its evidence record `LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md`.

**Correction to this review (side effect).** The `sdk-test` run above executed
`tests/run_all.bat`, which first runs five generators. At 2026-09-30 it
rewrote `f1c_sdk.json`, `f1c_sdk.h`, `f1c_vehicle_struct.h`,
`f1c_vehicle_struct_ida.h`, `f1c_trace_schema.h`, `f1c_reviewed_api.hpp` and
`analysis/recovery_index.json`. No pre-run copy exists. The outputs embed no
timestamps and inputs were unchanged, so byte-identical regeneration is likely
(inference, not verified). `recovery_index.json` matches git HEAD; the others
differ from HEAD (2026-08-18) as expected from uncommitted September work.
This confirms the Codex finding that "check" and "regenerate" are mixed.

**Agreement:** knowledge retrieval, not discovery, is the bottleneck; broad
IDA/prose scraping is low priority; state must be split from history; the
checkpoint is stale; the validator is red; `search` scans everything; reusable
lifecycle scenarios are the answer to repeated owner round trips.

**Codex found, this review missed:** `sdk-info` reports broker ABI 11 while
`f1c_broker_abi.h` declares ABI 15; `doctor` passes despite it; `inspect` does
not link raw symbols to current SDK helpers/tests (same `0x4C6E80` case);
unbounded `inspect` output; root-linked `research/game_mechanics` README and
AGENTS paths are absent; observation / guarded-command / expert-primitive
taxonomy; catalogue is 47 CONFIRMED vs 6,336 LEGACY functions.

**This review adds to Codex:** measured `search` 48.9 s vs `rg -l` 0.27 s
(single sample); `sdk-test` 55 s; `findings.jsonl` frozen since 2026-08-08 with
15 malformed lines; `inspect 0x4C6E80` parse failure; untyped, ungreppable
exports (`this[4649]`) from 2026-07-23 and lab IDB from 2026-08-06; idalib
present but unactivated (Codex: "did not test IDA availability"); no commits
since 2026-08-18; `f1c-memory` MCP registered for Codex only; space-stripping
and mojibake in generated docs.

**Position on static work:** consistent with the Codex stopping rule. The typed
re-export has a specific freshness/accuracy justification (offsets not
greppable; seven weeks of reviewed names absent from IDB and exports), so it
qualifies as targeted, not broad.

## Correction 2 (same day)

The "15 malformed lines" in `findings.jsonl` are intentional `//` header
comments skipped by `tools/ask.py load_jsonl`; the file is well-formed. The
stale-since-2026-08-08 finding stands. Execution now follows
[LLM_TOOLING_IMPLEMENTATION_PLAN_2026-09-30.md](LLM_TOOLING_IMPLEMENTATION_PLAN_2026-09-30.md).
